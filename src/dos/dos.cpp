// Wengier: LFN support
#include "stdafx.h"

//#include <stdlib.h>
#include "vdos.h"
#include "bios.h"
#include "mem.h"
#include "callback.h"
#include "regs.h"
#include "parport.h"
#include "serialport.h"
#include "dos_inc.h"
#include "video.h"
//#include <time.h>

DOS_Block dos;
DOS_InfoBlock dos_infoblock;
extern DOS_DateX datex;
typedef struct {
    UINT16 size_of_structure;
    UINT16 structure_version;
    UINT32 sectors_per_cluster;
    UINT32 bytes_per_sector;
    UINT32 available_clusters_on_drive;
    UINT32 total_clusters_on_drive;
    UINT32 available_sectors_on_drive;
    UINT32 total_sectors_on_drive;
    UINT32 available_allocation_units;
    UINT32 total_allocation_units;
    UINT8 reserved[8];
} ext_space_info_t;

#define DOS_COPYBUFSIZE 0x10000
#define CROSS_LEN 512
Bit8u dos_copybuf[DOS_COPYBUFSIZE];
static Bit8u daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static Bit16u countryNo = 0;

void DOS_SetError(Bit16u code) {
    dos.errorcode = code;
}

void DOS_SetCountry(Bit16u countryNo) {
    switch (countryNo) {
        case 1:
            *dos.tables.country = 0;
            break;
        case 2:
        case 36:
        case 38:
        case 40:
        case 42:
        case 46:
        case 48:
        case 81:
        case 82:
        case 86:
        case 88:
        case 354:
        case 886:
            *dos.tables.country = 2;
            break;
        default:
            *dos.tables.country = 1;
            break;
    }
    switch (countryNo) {
        case 3:
        case 30:
        case 32:
        case 34:
        case 39:
        case 44:
        case 55:
        case 88:
        case 90:
        case 785:
        case 886:
        case 972:
            *(dos.tables.country + 11) = 0x2f;
            break;
        case 7:
        case 33:
        case 41:
        case 43:
        case 47:
        case 49:
        case 86:
        case 358:
            *(dos.tables.country + 11) = 0x2e;
            break;
        default:
            *(dos.tables.country + 11) = 0x2d;
            break;
    }
    switch (countryNo) {
        case 39:
        case 45:
        case 46:
        case 358:
            *(dos.tables.country + 13) = 0x2e;
            break;
        default:
            *(dos.tables.country + 13) = 0x3a;
            break;
    }
}

#define DOSNAMEBUF 256

// Something like FCBNameToStr, strips padding spaces right before '.', '\' and '/'
void DosPathStripSpaces(char *name) {
    int k = 0;
    for (int i = 0, j = 0; char c = name[i]; i++) {
        if (c == '.' || c == '/' || c == '\\')
            j = k;
        if (c != ' ')
            k = j + 1;                                                                // Last non-space position (+1)
        name[j++] = c;
    }
    name[k] = 0;
}

void MEM_StrCopy(PhysPt pt, char *data, Bitu size) {
    Mem_StrnCopyFrom(data, pt, size);
}

void MEM_BlockWrite(PhysPt pt, void const *const data, Bitu size) {
    Mem_CopyTo(pt, data, size);
}

bool DOS_BreakAgain = false, DOS_BreakINT23InProgress = false, usedrvs;
extern bool fini, force;

bool DOS_Break() {
    if (DOS_BreakINT23InProgress) {
        DOS_BreakAgain = true;
        throw int(0);
    }
    if (DOS_BreakFlag) {
        Bit16u n = 4;
        const char *nl = "^C\r\n";
        DOS_WriteFile(STDOUT, (Bit8u *) nl, &n);                    // print ^C <newline>
        DOS_BreakFlag = false;
        bool terminate = true, terminint23 = false;
        Bitu offv = Mem_Lodsw((0x23 * 4) + 0);
        Bitu segv = Mem_Lodsw((0x23 * 4) + 2);
        if (offv != 0 && segv != 0)                                // HACK: The shell currently does not assign INT 23h
        {
            Bitu save_sp = reg_sp;                                // NTS: DOS calls are allowed within INT 23h!
            reg_flags |= 1;                                        // set carry flag
            try                                                    // invoke INT 23h
            {
                DOS_BreakINT23InProgress = true;
                CALLBACK_RunRealInt(0x23);
                DOS_BreakINT23InProgress = false;
            }
            catch (int x) {
                if (x == 0) {
                    DOS_BreakINT23InProgress = false;
                    terminint23 = true;
                } else {
                    vpLog("Unexpected code in INT 23h termination exception\n");
                    abort();
                }
            }
            if (!terminint23)                                    // if the INT 23h handler did not already terminate itself.
            {
                if (reg_sp == save_sp ||
                    (reg_flags & 1) == 0)    // if it returned with IRET, or with RETF and CF=0, don't terminate
                    terminate = false;
                if (reg_sp != save_sp) reg_sp += 2;
            }
        }
        if (terminate) {
            //DOS_Terminate(dos.psp(),false,0);
            if (DOS_BreakAgain) {
                DOS_BreakAgain = false;
                DOS_Break();
            }
            return false;
        } else if (terminint23)
            return false;
    }

    return true;
}

bool do_debug = false;
Bit32u deb_address = 0;

static Bitu DOS_21Handler(void) {
    if (((reg_ah != 0x50) && (reg_ah != 0x51) && (reg_ah != 0x62) && (reg_ah != 0x64)) && (reg_ah < 0x6c))
        if (dos.psp() != 0)                                                            // No program loaded yet?
        {
            DOS_PSP psp(dos.psp());
            psp.SetStack(SegOff2dWord(SegValue(ss), reg_sp - 18));
            psp.FindFreeFileEntry();
        }
    if (((reg_ah >= 0x01 && reg_ah <= 0x0C) || (reg_ah != 0 && reg_ah != 0x4C && reg_ah != 0x31 && dos.breakcheck)) &&
        !DOS_Break())
        return CBRET_NONE;
    char name1[DOSNAMEBUF + 2 + DOS_NAMELENGTH_ASCII];
    char name2[DOSNAMEBUF + 2 + DOS_NAMELENGTH_ASCII];
    char *p;
    bool b;
    //if (reg_ah != 0x0b) vpLog("DOS: %04x", reg_ax);
    switch (reg_ah) {
        case 0x00:                                                                        // Terminate Program
            DOS_Terminate(Mem_Lodsw(SegPhys(ss) + reg_sp + 2), false, 0);
            if (DOS_BreakINT23InProgress) throw int(0);
            break;
        case 0x01:                                                                        // Read character from STDIN, with echo
        {
            Bit8u c;
            Bit16u n = 1;
            dos.echo = true;
            DOS_ReadFile(STDIN, &c, &n);
            if (c == 3) {
                if (dos.breakcheck) {
                    reg_al = 10;
                    break;
                } else {
                    DOS_BreakFlag = true;
                    if (!DOS_Break()) return CBRET_NONE;
                }
            }
            reg_al = c;
            dos.echo = false;
        }
            break;
        case 0x02:                                                                        // Write character to STDOUT
        {
            Bit8u c = reg_dl;
            Bit16u n = 1;
            DOS_WriteFile(STDOUT, &c, &n);
            // Not in the official specs, but happens nonetheless. (last written character)
            reg_al = c;                                                                    // reg_al=(c==9)?0x20:c; //Officially: tab to spaces
        }
            break;
        case 0x03:                                                                        // Read character from STDAUX
        {
            Bit16u port = Mem_aLodsw(BIOS_BASE_ADDRESS_COM1);
            if (port != 0 && serialPorts[0])
                serialPorts[0]->Getchar(&reg_al);
        }
            break;
        case 0x04:                                                                        // Write Character to STDAUX
        {
            Bit16u port = Mem_aLodsw(BIOS_BASE_ADDRESS_COM1);
            if (port != 0 && serialPorts[0])
                serialPorts[0]->Putchar(reg_dl);
        }
            break;
        case 0x05:                                                                        // Write Character to PRINTER
            parallelPorts[0]->Putchar(reg_dl);
            break;
        case 0x06:                                                                        // Direct Console Output/Input
            if (reg_dl == 0xff)                                                            // Input
            {
                if (!DOS_GetSTDINStatus()) {
                    reg_al = 0;
                    CALLBACK_SZF(true);
                    break;
                }
                Bit8u c;
                Bit16u n = 1;
                DOS_ReadFile(STDIN, &c, &n);
                reg_al = c;
                CALLBACK_SZF(false);
            } else                                                                        // Ouput
            {
                Bit8u c = reg_dl;
                Bit16u n = 1;
                DOS_WriteFile(STDOUT, &c, &n);
                reg_al = reg_dl;
            }
            break;
        case 0x07:                                                                        // Character Input, without echo
        case 0x08:                                                                        // Direct Character Input, without echo (checks for breaks officially :)
        {
            Bit8u c;
            Bit16u n = 1;
            DOS_ReadFile(STDIN, &c, &n);
            if (c == 3) {
                if (dos.breakcheck) {
                    reg_al = 10;
                    break;
                } else {
                    DOS_BreakFlag = true;
                    if (!DOS_Break()) return CBRET_NONE;
                }
            }
            reg_al = c;
        }
            break;
        case 0x09:                                                                        // Write string to STDOUT
        {
            Bit8u c;
            Bit16u n = 1;
            PhysPt buf = SegPhys(ds) + reg_dx;
            while ((c = Mem_Lodsb(buf++)) != '$')
                DOS_WriteFile(STDOUT, &c, &n);
        }
            break;
        case 0x0a:                                                                        // Buffered Input
        {
            PhysPt data = SegPhys(ds) + reg_dx;
            Bit8u free = Mem_Lodsb(data);
            if (!free)
                break;
            Bit8u in = STDIN;
            Bit8u out = STDOUT;
            Bit8u read = 0;
            for (;;) {
                if (!dos.breakcheck && !DOS_Break()) return CBRET_NONE;
                Bit8u c;
                Bit16u n = 1;
                DOS_ReadFile(in, &c, &n);
                if (c == 10)                                                            // Line feed
                    continue;
                if (c == 8) {                                                                    // Backspace
                    if (read) {                                                                // Something to backspace.
                        n = 3;
                        DOS_WriteFile(out, (Bit8u *) "\b \b", &n);
                        --read;
                    }
                    continue;
                }
                if (c == 3) {                                                            // CTRL+C
                    if (dos.breakcheck)
                        break;
                    else {
                        DOS_BreakFlag = true;
                        if (!DOS_Break()) return CBRET_NONE;
                    }
                }
                if (read >= free)                                                        // Keyboard buffer full
                {
                    if (speaker) Beep(1750, 300);
                    continue;
                }
                DOS_WriteFile(out, &c, &n);
                Mem_Stosb(data + read + 2, c);
                if (c == 13)
                    break;
                read++;
            }
            Mem_Stosb(data + 1, read);
        }
            break;
        case 0x0b:                                                                        // Get STDIN Status
            reg_al = DOS_GetSTDINStatus() ? 0xff : 0x00;
            break;
        case 0x0c:                                                                        // Flush Buffer and read STDIN call
        {
            Bit8u handle = RealHandle(STDIN);
            Bit8u c;
            Bit16u n;
            while (DOS_GetSTDINStatus()) {
                n = 1;
                DOS_ReadFile(STDIN, &c, &n);
            }
            if (reg_al == 0x1 || reg_al == 0x6 || reg_al == 0x7 || reg_al == 0x8 || reg_al == 0xa) {
                Bit8u oldah = reg_ah;
                reg_ah = reg_al;
                DOS_21Handler();
                reg_ah = oldah;
            } else
                reg_al = 0;
        }
            break;
        case 0x0d:                                                                        // Disk Reset
            break;
        case 0x0e:                                                                        // Select Default Drive
            DOS_SetDefaultDrive(reg_dl);
            reg_al = DOS_DRIVES;
            break;
        case 0x0f:                                                                        // Open File using FCB
            reg_al = FCB_OpenFile(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x10:                                                                        // Close File using FCB
            reg_al = FCB_CloseFile(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x11:                                                                        // Find First Matching File using FCB
            reg_al = FCB_FindFirst(SegValue(ds), reg_dx) ? 0 : 0xFF;                    // No test for C:\COMMAND.COM!
            break;
        case 0x12:                                                                        // Find Next Matching File using FCB
            reg_al = FCB_FindNext(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x13:                                                                        // Delete File using FCB
            reg_al = FCB_DeleteFile(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x14:                                                                        // Sequential read from FCB
            reg_al = FCB_ReadFile(SegValue(ds), reg_dx, 0);
            break;
        case 0x15:                                                                        // Sequential write to FCB
            reg_al = FCB_WriteFile(SegValue(ds), reg_dx, 0);
            break;
        case 0x16:                                                                        // Create or truncate file using FCB
            reg_al = FCB_CreateFile(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x17:                                                                        // Rename file using FCB
            reg_al = FCB_RenameFile(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x19:                                                                        // Get current default drive
            reg_al = DOS_GetDefaultDrive();
            break;
        case 0x1a:                                                                        // Set Disk Transfer Area Address
        {
            dos.dta(RealMakeSeg(ds, reg_dx));
            DOS_DTA dta(dos.dta());
            break;
        }
        case 0x1b:                                                                        // Get allocation info for default drive
            if (!DOS_GetAllocationInfo(0, &reg_cx, &reg_al, &reg_dx))
                reg_al = 0xFF;
            break;
        case 0x1c:                                                                        // Get allocation info for specific drive
            if (!DOS_GetAllocationInfo(reg_dl, &reg_cx, &reg_al, &reg_dx))
                reg_al = 0xFF;
            break;
        case 0x21:                                                                        // Read random record from FCB
        {
            Bit16u toread = 1;
            reg_al = FCB_RandomRead(SegValue(ds), reg_dx, &toread, true);
        }
            break;
        case 0x22:                                                                        // Write random record to FCB
        {
            Bit16u towrite = 1;
            reg_al = FCB_RandomWrite(SegValue(ds), reg_dx, &towrite, true);
        }
            break;
        case 0x23:                                                                        // Get file size for FCB
            reg_al = FCB_GetFileSize(SegValue(ds), reg_dx) ? 0 : 0xFF;
            break;
        case 0x24:                                                                        // Set Random Record number for FCB
            FCB_SetRandomRecord(SegValue(ds), reg_dx);
            break;
        case 0x25:                                                                        // Set Interrupt Vector
            RealSetVec(reg_al, RealMakeSeg(ds, reg_dx));
            break;
        case 0x26:                                                                        // Create new PSP
            DOS_NewPSP(reg_dx, DOS_PSP(dos.psp()).GetSize());
            break;
        case 0x27:                                                                        // Random block read from FCB
            reg_al = FCB_RandomRead(SegValue(ds), reg_dx, &reg_cx, false);
            break;
        case 0x28:                                                                        // Random Block write to FCB
            reg_al = FCB_RandomWrite(SegValue(ds), reg_dx, &reg_cx, false);
            break;
        case 0x29:                                                                        // Parse filename into FCB
        {
            Bit8u difference;
            char string[1024];
            Mem_StrnCopyFrom(string, SegPhys(ds) + reg_si, 1023);                            // 1024 toasts the stack
            reg_al = FCB_Parsename(SegValue(es), reg_di, reg_al, string, &difference);
            reg_si += difference;
        }
            break;
        case 0x2a:                                                                        // Get System Date
            if (synctime) {
                _SYSTEMTIME systime;                                                        // Return the Windows localdate
                GetLocalTime(&systime);
                reg_al = (Bit8u) systime.wDayOfWeek;                                            // NB Sunday = 0, despite of MSDN documentation
                reg_cx = systime.wYear;
                reg_dx = (systime.wMonth << 8) + systime.wDay;
            } else {
                reg_al = (Bit8u) datex.dayofweek;                                            // NB Sunday = 0, despite of MSDN documentation
                reg_cx = datex.year;
                reg_dx = (datex.month << 8) + datex.day;
            }
            break;
        case 0x2b:                                                                        // Set System Date (we don't!)
            reg_al = 0xff;
            daysInMonth[2] = reg_cx & 3 ? 28
                                        : 29;                                        // Year is from 1980 to 2099, it is this simple
            if (reg_cx >= 1980 && reg_cx <= 2099)
                if (reg_dh > 0 && reg_dh <= 12)
                    if (reg_dl > 0 && reg_dl <= daysInMonth[reg_dh]) {
                        datex.year = reg_cx;
                        datex.month = reg_dh;
                        datex.day = reg_dl;
                        reg_al = 0;                                                        // Date is valid, fake set
                    }
            break;
        case 0x2c:                                                                        // Get System Time
            if (synctime) {
                _SYSTEMTIME systime;                                                    // Return the Windows localtime
                GetLocalTime(&systime);
                reg_cx = (systime.wHour << 8) + systime.wMinute;
                reg_dx = (systime.wSecond << 8) + systime.wMilliseconds / 10;
            } else {
                reg_cx = (datex.hour << 8) + datex.minute;
                reg_dx = (datex.second << 8) + datex.millisecond / 10;
            }
            break;
        case 0x2d:                                                                        // Set System Time (we don't!)
            if (reg_ch < 24 && reg_cl < 60 && reg_dh < 60 && reg_dl < 100) {
                datex.hour = reg_ch;
                datex.minute = reg_cl;
                datex.second = reg_dh;
                datex.millisecond = reg_dl * 10;
                reg_al = 0;                                                                // Time is valid, fake set
            } else
                reg_al = 0xff;
            break;
        case 0x2e:                                                                        // Set Verify flag
            dos.verify = (reg_al == 1);
            break;
        case 0x2f:                                                                        // Get Disk Transfer Area
            SegSet16(es, RealSeg(dos.dta()));
            reg_bx = RealOff(dos.dta());
            break;
        case 0x30:                                                                        // Get DOS Version
            if (reg_al == 0)
                reg_bh = 0xFF;                                                            // Fake Microsoft DOS
            else if (reg_al == 1)
                reg_bh = 0;                                                                // DOS is NOT in HMA
            reg_al = dos.version.major;
            reg_ah = dos.version.minor;
            reg_bl = 0;                                                                    // Serialnumber
            reg_cx = 0;
            break;
        case 0x31:                                                                        // Terminate and stay resident
            // Important: This service does not set the carry flag!
            DOS_ResizeMemory(dos.psp(), &reg_dx);
            DOS_Terminate(dos.psp(), true, reg_al);
            if (DOS_BreakINT23InProgress) throw int(0);
            break;
        case 0x1f:                                                                        // Get drive parameter block for default drive
        case 0x32:                                                                        // Get drive parameter block for specific drive
        {                                                                            // Officially a dpb should be returned as well. The disk detection part is implemented
            Bit8u drive = reg_dl;
            if (!drive || reg_ah == 0x1f)
                drive = DOS_GetDefaultDrive();
            else
                drive--;
            if (Drives[drive]) {
                reg_al = 0;
                SegSet16(ds, dos.tables.dpb);
                reg_bx = drive;                                                            // Faking only the first entry (that is the driveletter)
            } else
                reg_al = 0xff;
        }
            break;
        case 0x33:                                                                        // Extended Break Checking
            switch (reg_al) {
                case 0:                                                                        // Get the breakcheck flag
                    reg_dl = dos.breakcheck;
                    break;
                case 1:                                                                        // Set the breakcheck flag
                    dos.breakcheck = (reg_dl > 0);
                    break;
                case 2: {
                    bool old = dos.breakcheck;
                    dos.breakcheck = (reg_dl > 0);
                    reg_dl = old;
                }
                    break;
                case 3:                                                                        // Get cpsw
                case 4:                                                                        // Set cpsw, both not used really
                    break;
                case 5:
                    reg_dl = 3;                                                                // Boot drive = C:
                    break;
                case 6:                                                                        // Get true version number
                    reg_bx = (dos.version.minor << 8) + dos.version.major;
                    reg_dx = 0x1000;                                                        // Dos in ROM
                    break;
                default:
                    reg_al = 0xff;
            }
            break;
        case 0x34:                                                                        // Get address of INDos Flag
            SegSet16(es, DOS_SDA_SEG);
            reg_bx = DOS_SDA_OFS + 0x01;
            break;
        case 0x35:                                                                        // Get interrupt vector
            reg_bx = Mem_aLodsw(((Bit16u) reg_al) * 4);
            SegSet16(es, Mem_aLodsw(((Bit16u) reg_al) * 4 + 2));
            break;
        case 0x36:                                                                        // Get Free Disk Space
        {
            Bit16u bytes, clusters, free;
            Bit8u sectors;
            if (DOS_GetFreeDiskSpace(reg_dl, &bytes, &sectors, &clusters, &free)) {
                reg_ax = sectors;
                reg_bx = free;
                reg_cx = bytes;
                reg_dx = clusters;
            } else
                reg_ax = 0xffff;                                                        // invalid drive specified
        }
            break;
        case 0x37:                                                                        // Get/Set Switch char Get/Set Availdev thing
            switch (reg_al) {
                case 0:
                    reg_dl = 0x2f;                                                            // Always return '/' like dos 5.0+
                    break;
                case 1:
                    reg_al = 0;
                    break;
                case 2:
                    reg_al = 0;
                    reg_dl = 0x2f;
                    break;
                case 3:
                    reg_al = 0;
                    break;
            }
            break;
        case 0x38:                                                                        // Get/set Country Code
            if (reg_al ==
                0)                                                            // Get country specidic information
            {
                PhysPt dest = SegPhys(ds) + reg_dx;
                Mem_CopyTo(dest, dos.tables.country, 0x18);
                reg_al = countryNo < 0xff ? countryNo : 0xff;
                reg_bx = countryNo;
                CALLBACK_SCF(false);
            } else if (reg_dx == 0xffff)                                                    // Set country code
            {
                countryNo = reg_al == 0xff ? reg_bx : reg_al;
                DOS_SetCountry(countryNo);
                reg_ax = 0;
                CALLBACK_SCF(false);
            } else
                CALLBACK_SCF(true);
            break;
        case 0x39:                                                                        // MKDIR Create directory
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_MakeDir(name1)) {
                reg_ax = 0xffff;                                                        // AX destroyed
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3a:                                                                        // RMDIR Remove directory
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_RemoveDir(name1)) {
                reg_ax = 0xffff;                                                        // AX destroyed
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3b:                                                                        // CHDIR Set current directory
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_ChangeDir(name1)) {
                reg_ax = 0xff00;                                                        // AX destroyed
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3c:                                                                        // CREATE Create or truncate file
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_CreateFile(name1, reg_cx, &reg_ax))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3d:                                                                        // OPEN Open existing file
        {
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            force = true;
            if (DOS_OpenFile(name1, reg_al, &reg_ax))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            force = false;
            break;
        }
        case 0x3e:                                                                        // CLOSE Close file
            if (DOS_CloseFile(reg_bx))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3f:                                                                        // READ Read from file or device
        {
            Bit16u toread = reg_cx;
            dos.echo = true;
            if (DOS_ReadFile(reg_bx, dos_copybuf, &toread)) {
                Mem_CopyTo(SegPhys(ds) + reg_dx, dos_copybuf, toread);
                reg_ax = toread;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            dos.echo = false;
        }
            break;
        case 0x40:                                                                        // WRITE Write to file or device
        {
            Bit16u towrite = reg_cx;
            Mem_CopyFrom(SegPhys(ds) + reg_dx, dos_copybuf, towrite);
            if (DOS_WriteFile(reg_bx, dos_copybuf, &towrite)) {
                reg_ax = towrite;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x41:                                                                        // UNLINK Delete file
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_DeleteFile(name1, false))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x42:                                                                        // LSEEK Set current file position
        {
            Bit32u pos = (reg_cx << 16) + reg_dx;
            if (DOS_SeekFile(reg_bx, &pos, reg_al)) {
                reg_dx = (Bit16u) (pos >> 16);
                reg_ax = (Bit16u) (pos & 0xFFFF);
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x43:                                                                        // Get/Set file attributes
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            if (reg_al == 0)                                                            // Get
            {
                Bit16u attr_val = reg_cx;
                if (!stricmp(name1, "C:\\AUTOEXEC.BAT") ||
                    DOS_GetDefaultDrive() == 2 && !stricmp(name1, "\\AUTOEXEC.BAT") || !stricmp(name1, "4DOS.HLP") ||
                    fini && !stricmp(name1, "4DOS.INI"))    // 4DOS uses this to test for existance
                {
                    dos.errorcode = 0;
                    reg_cx = 1;                                                            // Read-only seems right
                    CALLBACK_SCF(false);
                } else if (DOS_GetFileAttr(name1, &attr_val)) {
                    reg_cx = attr_val;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
            } else if (reg_al == 1)                                                        // Set
            {
                if (DOS_SetFileAttr(name1, reg_cx)) {
                    reg_ax = 0x202;                                                        // AX destroyed
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
            } else if (reg_al == 2) {
                DWORD size = DOS_GetCompressedFileSize(name1);
                if (size >= 0) {
                    reg_ax = LOWORD(size);
                    reg_dx = HIWORD(size);
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
            } else if (reg_al == 0xff && reg_bp == 0x5053 && (reg_cl == 0x39 || reg_cl == 0x56)) {
                if (reg_cl == 0x39) {
                    if (DOS_MakeDir(name1)) {
                        reg_ax = 0xffff;                                                // AX destroyed
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                } else {
                    Mem_StrnCopyFrom(name2, SegPhys(es) + reg_di, DOSNAMEBUF);
                    rSpTrim(name2);
                    if (DOS_Rename(name1, name2))
                        CALLBACK_SCF(false);
                    else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                }
            } else {
                reg_ax = 1;
                CALLBACK_SCF(true);
            }
            break;
        case 0x44:                                                                        // IOCTL Functions
            if (DOS_IOCTL())
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x45:                                                                        // DUP Duplicate file handle
            if (DOS_DuplicateEntry(reg_bx, &reg_ax))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x46:                                                                        // DUP2, FORCEDUP Force duplicate file handle
            if (DOS_ForceDuplicateEntry(reg_bx, reg_cx)) {
                reg_ax = reg_cx;                                                        // Not all sources agree on it.
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x47:                                                                        // CWD Get current directory
            if (DOS_GetCurrentDir(reg_dl, name1, false)) {
                Mem_CopyTo(SegPhys(ds) + reg_si, name1, (Bitu) (strlen(name1) + 1));
                reg_ax = 0x0100;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x48:                                                                        // Allocate memory
        {
            Bit16u size = reg_bx;
            Bit16u seg;
            if (DOS_AllocateMemory(&seg, &size)) {
                reg_ax = seg;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                reg_bx = size;
                CALLBACK_SCF(true);
            }
            break;
        }
        case 0x49:                                                                        // Free memory
            if (SegValue(es) &&
                DOS_FreeMemory(SegValue(es)))                            // No idea why ES=0 with LINK.EXE 3.69
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x4a:                                                                        // Resize memory block
        {
            Bit16u size = reg_bx;
            if (DOS_ResizeMemory(SegValue(es), &size)) {
                reg_ax = SegValue(es);
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                reg_bx = size;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x4b:                                                                        // EXEC Load and/or execute program
            if (reg_al > 3) {
                reg_ax = 1;
                CALLBACK_SCF(true);
            } else {
                Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
                rSpTrim(name1);
                if (!DOS_Execute(name1, SegPhys(es) + reg_bx, reg_al)) {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                } else
                    CALLBACK_SCF(false);
                WinProgNowait = true;
            }
            break;
        case 0x4c:                                                                        // EXIT Terminate with return code
            DOS_Terminate(dos.psp(), false, reg_al);
            if (DOS_BreakINT23InProgress) throw int(0);
            break;
        case 0x4d:                                                                        // Get Return code
            reg_al = dos.return_code;                                                    // Officially read from SDA and clear when read
            reg_ah = dos.return_mode;
            break;
        case 0x4e:                                                                        // FINDFIRST Find first matching file
        {
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            DosPathStripSpaces(name1);
            // NB FiAd calls this with 8.3 format
            fflfn = false;
            if (DOS_FindFirst(name1, reg_cx, -1)) {
                reg_ax = 0;                                                                // Undocumented
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        }
        case 0x4f:                                                                        // FINDNEXT Find next matching file
            fflfn = false;
            if (DOS_FindNext(-1)) {
                reg_ax = 0;                                                                // Undocumented:Qbix Willy beamish
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x50:                                                                        // Set current PSP
            dos.psp(reg_bx);
            break;
        case 0x51:                                                                        // Get current PSP
            reg_bx = dos.psp();
            break;
        case 0x52:                                                                        // Get list of lists
        {
            RealPt addr = dos_infoblock.GetPointer();
            SegSet16(es, RealSeg(addr));
            reg_bx = RealOff(addr);
        }
            break;
        case 0x54:                                                                        // Get verify flag
            reg_al = dos.verify ? 1 : 0;
            break;
        case 0x55:                                                                        // Create Child PSP
            DOS_ChildPSP(reg_dx, reg_si);
            dos.psp(reg_dx);
            reg_al = 0xf0;                                                                // AL destroyed
            break;
        case 0x56:                                                                        // Rename file
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);
            Mem_StrnCopyFrom(name2, SegPhys(es) + reg_di, DOSNAMEBUF);
            rSpTrim(name2);
            if (DOS_Rename(name1, name2))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x57:                                                                        // Get/set File's date and time
            if (reg_al == 0 || reg_al == 4 || reg_al == 6)
                CALLBACK_SCF(DOS_GetFileDate(reg_bx, &reg_cx, &reg_dx, reg_al) ? false : true);
            else if (reg_al == 1 || reg_al == 5 || reg_al == 7)
                CALLBACK_SCF(DOS_SetFileDate(reg_bx, reg_cx, reg_dx, reg_al) ? false : true);
            break;
        case 0x58:                                                                        // Get/set memory allocation strategy
            switch (reg_al) {
                case 0:                                                                        // Get strategy
                    reg_ax = DOS_GetMemAllocStrategy();
                    break;
                case 1:                                                                        // Set strategy
                    if (DOS_SetMemAllocStrategy(reg_bl))                                    // State is BL, not BX?
                        CALLBACK_SCF(false);
                    else {
                        reg_ax = 1;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 2:                                                                        // Get UMB link status
                    reg_al = dos_infoblock.GetUMBChainState() & 1;
                    CALLBACK_SCF(false);
                    break;
                case 3:                                                                        // Set UMB link status
                    if (DOS_LinkUMBsToMemChain(reg_bl))                                        // State is BL, not BX?
                        CALLBACK_SCF(false);
                    else {
                        reg_ax = 1;
                        CALLBACK_SCF(true);
                    }
                    break;
                default:
                    reg_ax = 1;
                    CALLBACK_SCF(true);
            }
            break;
        case 0x59:                                                                        // Get extended error information
            reg_ax = dos.errorcode;
            if (dos.errorcode == DOSERR_FILE_NOT_FOUND || dos.errorcode == DOSERR_PATH_NOT_FOUND)
                reg_bh = 8;                                                                // Not found error class (Road Hog)
            else
                reg_bh = 0;                                                                // Unspecified error class
            reg_bl = 1;                                                                    // Retry retry retry
            reg_ch = 0;                                                                    // Unkown error locus
            break;
        case 0x5a:                                                                        // Create temporary file
        {
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            Bit16u handle;
            if (DOS_CreateTempFile(name1, &handle)) {
                reg_ax = handle;
                Mem_CopyTo(SegPhys(ds) + reg_dx, name1, (Bitu) (strlen(name1) + 1));
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x5b:                                                                        // Create new file
        {
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_dx, DOSNAMEBUF);
            rSpTrim(name1);

            Bit16u handle;
            if (DOS_OpenFile(name1, 0, &handle))                                        // ??? what about devices ???
            {
                DOS_CloseFile(handle);
                DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            } else if (DOS_CreateFile(name1, reg_cx, &handle)) {
                reg_ax = handle;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x5c:                                                                        // FLOCK File region locking
        {
            if (DOS_LockFile(reg_bx, reg_al, (reg_cx << 16) + reg_dx, (reg_si << 16) + reg_di)) {
                reg_ax = 0;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
        }
            break;
        case 0x5d:                                                                        // Network Functions
            if (reg_al == 0x06) {
                SegSet16(ds, DOS_SDA_SEG);
                reg_si = DOS_SDA_OFS;
//			reg_cx = 0x80;															// Swap if in dos
                reg_cx = 0x78c;                                                            // Found by Edward Mendelson, fixes problems with WP (others) shelling out
                reg_dx = 0x1a;                                                            // Swap always
                break;
            }
        case 0x5e:                                                                        // Network
            if (reg_al == 0)                                                            // Get machine name
            {
                DWORD size = DOSNAMEBUF;
                GetComputerName(name1, &size);
                if (size) {
                    strcat(name1, "               ");                                    // Simply add 15 spaces
                    if (reg_ip == 0x11e5 || reg_ip ==
                                            0x1225)                            // 4DOS expects it to be 0 terminated (not documented)
                    {
                        name1[16] = 0;
                        Mem_CopyTo(SegPhys(ds) + reg_dx, name1, 17);
                    } else
                        Mem_CopyTo(SegPhys(ds) + reg_dx, name1, 16);
                    reg_cx = 0x1ff;                                                        // 01h name valid, FFh NetBIOS number for machine name
                    CALLBACK_SCF(false);
                    break;
                }
            }
            CALLBACK_SCF(true);
            break;
        case 0x5f:                                                                        // Network redirection
            reg_ax = 1;                                                                    // Failing it
            CALLBACK_SCF(true);
            break;
        case 0x60:                                                                        // Canonicalize filename or path
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_si, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_Canonicalize(name1, name2)) {
                Mem_CopyTo(SegPhys(es) + reg_di, name2, (Bitu) (strlen(name2) + 1));
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x62:                                                                        // Get Current PSP Address
            reg_bx = dos.psp();
            break;
        case 0x63:                                                                        // DOUBLE BYTE CHARACTER SET
            if (reg_al == 0) {
                SegSet16(ds, RealSeg(dos.tables.dbcs));
                reg_si = RealOff(dos.tables.dbcs);
                reg_al = 0;
                CALLBACK_SCF(false);                                                    // Undocumented
            } else
                reg_al = 0xff;                                                            // Doesn't officially touch carry flag
            break;
        case 0x65:                                                                        // Get extented country information and a lot of other useless shit
        {
            if ((reg_al <= 7) && (reg_cx <
                                  5))                                            // Todo maybe fully support this for now we set it standard for USA
            {
                DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
                CALLBACK_SCF(true);
                break;
            }
            Bitu len = 0;                                                                // For 0x21 and 0x22
            PhysPt data = SegPhys(es) + reg_di;
            switch (reg_al) {
                case 0x01:
                    Mem_Stosb(data + 0x00, reg_al);
                    Mem_Stosw(data + 0x01, 0x26);
                    if (!countryNo) {
                        char buffer[128];
                        if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, buffer, 128)) {
                            countryNo = Bit16u(atoi(buffer));
                            DOS_SetCountry(countryNo);
                        } else
                            countryNo = 1;    // Defaults to 1 (US) if failed
                    }
                    Mem_Stosw(data + 0x03, countryNo);
                    if (reg_cx > 0x06)
                        Mem_Stosw(data + 0x05, dos.loaded_codepage);
                    if (reg_cx > 0x08) {
                        Bitu amount = (reg_cx >= 0x29) ? 0x22 : (reg_cx - 7);
                        Mem_CopyTo(data + 0x07, dos.tables.country, amount);
                        reg_cx = (reg_cx >= 0x29) ? 0x29 : reg_cx;
                    }
                    CALLBACK_SCF(false);
                    break;
                case 0x05:                                                                    // Get pointer to filename terminator table
                    Mem_Stosb(data + 0x00, reg_al);
                    Mem_Stosd(data + 0x01, dos.tables.filenamechar);
                    reg_cx = 5;
                    CALLBACK_SCF(false);
                    break;
                case 0x02:                                                                    // Get pointer to uppercase table
                    Mem_Stosb(data + 0x00, reg_al);
                    Mem_Stosd(data + 0x01, dos.tables.upcase);
                    reg_cx = 5;
                    CALLBACK_SCF(false);
                    break;
                case 0x06:                                                                    // Get pointer to collating sequence table
                    Mem_Stosb(data + 0x00, reg_al);
                    Mem_Stosd(data + 0x01, dos.tables.collatingseq);
                    reg_cx = 5;
                    CALLBACK_SCF(false);
                    break;
                case 0x03:                                                                    // Get pointer to lowercase table
                case 0x04:                                                                    // Get pointer to filename uppercase table
                case 0x07:                                                                    // Get pointer to double byte char set table
                    Mem_Stosb(data + 0x00, reg_al);
                    Mem_Stosd(data + 0x01, dos.tables.dbcs);                                // Used to be 0
                    reg_cx = 5;
                    CALLBACK_SCF(false);
                    break;
                case 0x20:                                                                    // Capitalize Character
                    reg_dl = (Bit8u) toupper(reg_dl);
                    CALLBACK_SCF(false);
                    break;
                case 0x21:                                                                    // Capitalize String (cx=length)
                case 0x22:                                                                    // Capatilize ASCIZ string
                    data = SegPhys(ds) + reg_dx;
                    len = (reg_al == 0x21) ? reg_cx : Mem_StrLen(data);
                    if (len) {
                        if (len + reg_dx >
                            DOS_COPYBUFSIZE - 1)                                // Is limited to 65535 / within DS
                            E_Exit("DOS: 0x65 Buffer overflow");
                        Mem_CopyFrom(data, dos_copybuf, len);
                        dos_copybuf[len] = 0;
                        for (Bitu count = 0; count <
                                             len; count++)                            // No upcase as String(0x21) might be multiple asciiz strings
                            dos_copybuf[count] = (Bit8u) toupper(
                                    *reinterpret_cast<unsigned char *>(dos_copybuf + count));
                        Mem_CopyTo(data, dos_copybuf, len);
                    }
                    CALLBACK_SCF(false);
                    break;
                case 0x23:
                    if (reg_dl == 'n' || reg_dl == 'N')
                        reg_ax = 0;
                    else if (reg_dl == 'y' || reg_dl == 'Y')
                        reg_ax = 1;
                    else
                        reg_ax = 2;
                    CALLBACK_SCF(false);
                    break;
                default:
                    vpLog("INT 21-65: Unhandled country information call %2X", reg_al);
            }
        }
            break;
        case 0x66:
            if (reg_al == 1)                                                            // Get global code page table
            {
                reg_bx = codepage;                                                        // Active code page
                reg_dx = dos.loaded_codepage;                                            // System code page
                CALLBACK_SCF(false);
            } else
                CALLBACK_SCF(true);
            break;
        case 0x67:                                                                        // Set handle count
        {
            if (reg_bx > 255)                                                            // Limit to max 255
            {
                reg_ax = DOSERR_TOO_MANY_OPEN_FILES;
                CALLBACK_SCF(true);
            } else {
                DOS_PSP psp(dos.psp());
                psp.SetNumFiles(reg_bx);
                CALLBACK_SCF(false);
            }
        }
            break;
        case 0x68:                                                                        // FFLUSH Commit file
        case 0x6a:
            if (DOS_FlushFile(reg_bl))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x69:                                                                        // Get/set disk serial number
        {
            DWORD serial_number = 0;
            char Drive[] = "A:\\", volume_label[MAX_PATH];
            Drive[0] = 'A' + (reg_bl > 0 ? reg_bl - 1 : DOS_GetDefaultDrive());
            switch (reg_al) {
                case 0x00:
                    if (DOS_GetVolumeInfo(Drive, volume_label, &serial_number)) {
                        if (!strlen(volume_label)) strcpy(volume_label, "NO_NAME");
                        PhysPt data = SegPhys(ds) + reg_dx;
                        for (int i = 0; i < 25; i++)
                            Mem_Stosb(data + i, 0);
                        Mem_Stosb(data + 2, serial_number % 256);
                        Mem_Stosb(data + 3, (Bit8u) ((serial_number % 65536) / 256));
                        Mem_Stosb(data + 4, (Bit8u) ((serial_number % 16777216) / 65536));
                        Mem_Stosb(data + 5, (Bit8u) (serial_number / 16777216));
                        for (unsigned int i = 0; i < 11 && i < strlen(volume_label); i++)
                            Mem_Stosb(data + i + 6, volume_label[i]);
                        Mem_Stosb(data + 0x11, 'F');
                        Mem_Stosb(data + 0x12, 'A');
                        Mem_Stosb(data + 0x13, 'T');
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x01:
                default:
                    reg_ax = 0x03;
                    CALLBACK_SCF(true);
            }
            break;
        }
        case 0x6c:                                                                        // Extended Open/Create
            Mem_StrnCopyFrom(name1, SegPhys(ds) + reg_si, DOSNAMEBUF);
            rSpTrim(name1);
            if (DOS_OpenFileExtended(name1, reg_bx, reg_cx, reg_dx, &reg_ax, &reg_cx))
                CALLBACK_SCF(false);
            else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x71:                    /* Unknown probably 4dos detection */
            //vpLog("DOS:LFN function call 71%2X\n",reg_al);
            if (!uselfn) {
                reg_ax = 0x7100;
                CALLBACK_SCF(true); //Check this! What needs this ? See default case
                break;
            } else {
                //FILE* logmsg = fopen("log.txt", "a+");
                //fprintf(logmsg, "call %2X\n", reg_al);
                //fclose(logmsg);
            }
            switch (reg_al) {
                case 0x0d:        /* LFN RESET */
                    CALLBACK_SCF(false);
                    break;
                case 0x39:        /* LFN MKDIR */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_MakeDir(name1)) {
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x3a:        /* LFN RMDIR */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_RemoveDir(name1)) {
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x3b:        /* LFN CHDIR */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_ChangeDir(name1)) {
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x41:        /* LFN UNLINK */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_DeleteFile(name1, reg_si == 1)) {
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x43:        /* LFN ATTR */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    switch (reg_bl) {
                        case 0x00:                /* Get attribute */
                        {
                            Bit16u attr_val = reg_cx;
                            if (DOS_GetFileAttr(name1, &attr_val)) {
                                reg_cx = attr_val;
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                            } else {
                                CALLBACK_SCF(true);
                                reg_ax = dos.errorcode;
                            }
                            break;
                        };
                        case 0x01:                /* Set attribute */
                            if (DOS_SetFileAttr(name1, reg_cx)) {
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                            } else {
                                CALLBACK_SCF(true);
                                reg_ax = dos.errorcode;
                            }
                            break;
                        case 0x02:                /* Get compressed file size */
                        {
                            DWORD size = DOS_GetCompressedFileSize(name1);
                            if (size >= 0) {
                                reg_ax = LOWORD(size);
                                reg_dx = HIWORD(size);
                                CALLBACK_SCF(false);
                            } else {
                                CALLBACK_SCF(true);
                                reg_ax = dos.errorcode;
                            }
                            break;
                        }
                        case 0x03:                /* Set file date/time */
                        case 0x05:
                        case 0x07: {
                            HANDLE hFile = DOS_CreateOpenFile(name1);
                            if (hFile != INVALID_HANDLE_VALUE) {
                                time_t clock = time(NULL), ttime;
                                struct tm *t = localtime(&clock);
                                FILETIME time;
                                t->tm_isdst = -1;
                                t->tm_sec = (((int) reg_cx) << 1) & 0x3e;
                                t->tm_min = (((int) reg_cx) >> 5) & 0x3f;
                                t->tm_hour = (((int) reg_cx) >> 11) & 0x1f;
                                t->tm_mday = (int) (reg_di) & 0x1f;
                                t->tm_mon = ((int) (reg_di >> 5) & 0x0f) - 1;
                                t->tm_year = ((int) (reg_di >> 9) & 0x7f) + 80;
                                ttime = mktime(t);
                                LONGLONG ll = Int32x32To64(ttime, 10000000) + 116444736000000000 +
                                              (reg_bl == 0x07 ? reg_si * 100000 : 0);
                                time.dwLowDateTime = (DWORD) ll;
                                time.dwHighDateTime = (DWORD)(ll >> 32);
                                if (!SetFileTime(hFile, reg_bl == 0x07 ? &time : NULL, reg_bl == 0x05 ? &time : NULL,
                                                 reg_bl == 0x03 ? &time : NULL)) {
                                    CloseHandle(hFile);
                                    CALLBACK_SCF(true);
                                    reg_ax = dos.errorcode;
                                    break;
                                }
                                CloseHandle(hFile);
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                            } else {
                                CALLBACK_SCF(true);
                                reg_ax = dos.errorcode;
                            }
                            break;
                        }
                        case 0x04:                /* Get file date/time */
                        case 0x06:
                        case 0x08:
                            WIN32_FILE_ATTRIBUTE_DATA fad;
                            if (DOS_GetFileAttrEx(name1, (LPVOID) & fad)) {
                                FILETIME *time;
                                struct tm *ltime;
                                time = reg_bl == 0x04 ? &fad.ftLastWriteTime : reg_bl == 0x06 ? &fad.ftLastAccessTime
                                                                                              : &fad.ftCreationTime;
                                ULARGE_INTEGER ull;
                                ull.LowPart = time->dwLowDateTime;
                                ull.HighPart = time->dwHighDateTime;
                                time_t ttime = ull.QuadPart / 10000000ULL - 11644473600ULL;
                                if ((ltime = localtime(&ttime)) != 0) {
                                    reg_cx = DOS_PackTime((Bit16u) ltime->tm_hour, (Bit16u) ltime->tm_min,
                                                          (Bit16u) ltime->tm_sec);
                                    reg_di = DOS_PackDate((Bit16u) (ltime->tm_year + 1900),
                                                          (Bit16u) (ltime->tm_mon + 1), (Bit16u) ltime->tm_mday);
                                }
                                if (reg_bl == 0x08) {
                                    ull.QuadPart /= 100000;
                                    reg_si = (UINT16)(ull.QuadPart % 200);
                                }
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                            } else {
                                CALLBACK_SCF(true);
                                reg_ax = dos.errorcode;
                            }
                            break;
                        default:
                            E_Exit("DOS:Illegal LFN Attr call %2X", reg_bl);
                    }
                    break;
                case 0x47:        /* LFN PWD */
                {
                    DOS_PSP psp(dos.psp());
                    psp.StoreCommandTail();
                    if (DOS_GetCurrentDir(reg_dl, name1, true)) {
                        MEM_BlockWrite(SegPhys(ds) + reg_si, name1, (Bitu) (strlen(name1) + 1));
                        psp.RestoreCommandTail();
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                }
                case 0x4e:        /* LFN FindFirst */
                {
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (!DOS_GetSFNPath(name1, name2, false)) {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                        break;
                    }
                    Bit16u entry;
                    Bit8u i, handle = DOS_FILES;
                    for (i = 1; i < DOS_FILES; i++) {
                        if (!Files[i]) {
                            handle = i;
                            break;
                        }
                    }
                    if (handle == DOS_FILES) {
                        reg_ax = DOSERR_TOO_MANY_OPEN_FILES;
                        CALLBACK_SCF(true);
                        break;
                    }
                    fflfn = true;
                    b = DOS_FindFirst(name2, reg_cx, handle);
                    int error = dos.errorcode;
                    Bit16u attribute = 0;
                    if (!b && DOS_GetFileAttr(name2, &attribute) && (attribute & DOS_ATTR_DIRECTORY)) {
                        strcat(name2, "\\*.*");
                        b = DOS_FindFirst(name2, reg_cx, handle);
                        error = dos.errorcode;
                    }
                    if (b) {
                        DOS_PSP psp(dos.psp());
                        entry = psp.FindFreeFileEntry();
                        if (entry == 0xff) {
                            reg_ax = DOSERR_TOO_MANY_OPEN_FILES;
                            CALLBACK_SCF(true);
                            break;
                        }
                        if (handle >= DOS_DEVICES || !Devices[handle]) {
                            int m = 0;
                            for (int i = 1; i < DOS_DEVICES; i++)
                                if (Devices[i]) m = i;
                            Files[handle] = new DOS_Device(*Devices[m]);
                        } else
                            Files[handle] = new DOS_Device(*Devices[handle]);
                        Files[handle]->AddRef();
                        psp.SetFileHandle(entry, handle);
                        reg_ax = handle;
                        DOS_DTA dta(dos.dta());
                        char finddata[CROSS_LEN];
                        int c = 0;
                        MEM_BlockWrite(SegPhys(es) + reg_di, finddata, dta.GetFindData((int) reg_si, finddata, &c));
                        reg_cx = c;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = error;
                        DOS_SetError(error);
                        CALLBACK_SCF(true);
                    };
                    break;
                }
                case 0x4f:        /* LFN FindNext */
                {
                    Bit8u handle = (Bit8u) reg_bx;
                    if (!handle || handle >= DOS_FILES || !Files[handle]) {
                        reg_ax = DOSERR_INVALID_HANDLE;
                        CALLBACK_SCF(true);
                        break;
                    }
                    fflfn = true;
                    if (DOS_FindNext(handle)) {
                        DOS_DTA dta(dos.dta());
                        char finddata[CROSS_LEN];
                        int c = 0;
                        MEM_BlockWrite(SegPhys(es) + reg_di, finddata, dta.GetFindData((int) reg_si, finddata, &c));
                        reg_cx = c;
                        CALLBACK_SCF(false);
                        reg_ax = 0x4f00 + handle;
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    };
                    break;
                }
                case 0x56:        /* LFN Rename */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    MEM_StrCopy(SegPhys(es) + reg_di, name2 + 1, DOSNAMEBUF);
                    *name2 = '\"';
                    p = name2 + strlen(name2);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_Rename(name1, name2)) {
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x60:        /* LFN GetName */
                    MEM_StrCopy(SegPhys(ds) + reg_si, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_Canonicalize(name1, name2)) {
                        strcpy(name1, "\"");
                        strcat(name1, name2);
                        strcat(name1, "\"");
                        switch (reg_cl) {
                            case 0:        // Canonoical path name
                                strcpy(name2, name1);
                                MEM_BlockWrite(SegPhys(es) + reg_di, name2, (Bitu) (strlen(name2) + 1));
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                                break;
                            case 1:        // SFN path name
                                if (DOS_GetSFNPath(name1, name2, false)) {
                                    MEM_BlockWrite(SegPhys(es) + reg_di, name2, (Bitu) (strlen(name2) + 1));
                                    reg_ax = 0;
                                    CALLBACK_SCF(false);
                                } else {
                                    reg_ax = 2;
                                    CALLBACK_SCF(true);
                                }
                                break;
                            case 2:        // LFN path name
                                if (DOS_GetSFNPath(name1, name2, true)) {
                                    MEM_BlockWrite(SegPhys(es) + reg_di, name2, (Bitu) (strlen(name2) + 1));
                                    reg_ax = 0;
                                    CALLBACK_SCF(false);
                                } else {
                                    reg_ax = 2;
                                    CALLBACK_SCF(true);
                                }
                                break;
                            default:
                                E_Exit("DOS:Illegal LFN GetName call %2X", reg_cl);
                        }
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0x6c:        /* LFN Create */
                    MEM_StrCopy(SegPhys(ds) + reg_si, name1 + 1, DOSNAMEBUF);
                    *name1 = '\"';
                    p = name1 + strlen(name1);
                    while (*p == ' ' || *p == 0) p--;
                    *(p + 1) = '\"';
                    *(p + 2) = 0;
                    if (DOS_OpenFileExtended(name1, reg_bx, reg_cx, reg_dx, &reg_ax, &reg_cx)) {
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0xa0:        /* LFN VolInfo */
                    MEM_StrCopy(SegPhys(ds) + reg_dx, name1, DOSNAMEBUF);
                    if (DOS_Canonicalize(name1, name2)) {
                        if (reg_cx > 3)
                            MEM_BlockWrite(SegPhys(es) + reg_di, "FAT", 4);
                        reg_ax = 0;
                        reg_bx = 0x4006;
                        reg_cx = 0xff;
                        reg_dx = 0x104;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0xa1:        /* LFN FileClose */
                {
                    Bit8u handle = (Bit8u) reg_bx;
                    if (!handle || handle >= DOS_FILES || !Files[handle]) {
                        reg_ax = DOSERR_INVALID_HANDLE;
                        CALLBACK_SCF(true);
                        break;
                    }
                    DOS_PSP psp(dos.psp());
                    Bit16u entry = psp.FindEntryByHandle(handle);
                    if (entry > 0 && entry != 0xff) psp.SetFileHandle(entry, 0xff);
                    if (entry > 0 && Files[handle]->RemoveRef() <= 0) {
                        delete Files[handle];
                        Files[handle] = 0;
                    }
                    reg_ax = 0;
                    CALLBACK_SCF(false);
                    break;
                }
                case 0xa6:        /* LFN GetFileInfoByHandle */
                {
                    char buf[64], st = 0;
                    DWORD serial_number = 0, cdate, ctime, adate, atime, mdate, mtime;
                    Bit8u entry = (Bit8u) reg_bx, handle;
                    WIN32_FILE_ATTRIBUTE_DATA fad;
                    if (entry >= DOS_FILES) {
                        reg_ax = DOSERR_INVALID_HANDLE;
                        CALLBACK_SCF(true);
                        break;
                    }
                    DOS_PSP psp(dos.psp());
                    for (int i = 0; i <= DOS_FILES; i++)
                        if (Files[i] && psp.FindEntryByHandle(i) == entry)
                            handle = i;
                    if (handle < DOS_FILES && Files[handle] && Files[handle]->name != NULL) {
                        char volume[] = "A:\\";
                        volume[0] += Files[handle]->GetDrive();
                        GetVolumeInformation(volume, NULL, 0, &serial_number, NULL, NULL, NULL, 0);
                        if (DOS_GetFileAttrEx(Files[handle]->name, (LPVOID) & fad, Files[handle]->GetDrive())) {
                            FILETIME *time;
                            time_t ttime;
                            struct tm *ltime;
                            ULARGE_INTEGER ull;
                            time = &fad.ftCreationTime;
                            ull.LowPart = time->dwLowDateTime;
                            ull.HighPart = time->dwHighDateTime;
                            ttime = ull.QuadPart / 10000000ULL - 11644473600ULL;
                            if ((ltime = localtime(&ttime)) != 0) {
                                ctime = DOS_PackTime((Bit16u) ltime->tm_hour, (Bit16u) ltime->tm_min,
                                                     (Bit16u) ltime->tm_sec);
                                cdate = DOS_PackDate((Bit16u) (ltime->tm_year + 1900), (Bit16u) (ltime->tm_mon + 1),
                                                     (Bit16u) ltime->tm_mday);
                            }
                            time = &fad.ftLastAccessTime;
                            ull.LowPart = time->dwLowDateTime;
                            ull.HighPart = time->dwHighDateTime;
                            ttime = ull.QuadPart / 10000000ULL - 11644473600ULL;
                            if ((ltime = localtime(&ttime)) != 0) {
                                atime = DOS_PackTime((Bit16u) ltime->tm_hour, (Bit16u) ltime->tm_min,
                                                     (Bit16u) ltime->tm_sec);
                                adate = DOS_PackDate((Bit16u) (ltime->tm_year + 1900), (Bit16u) (ltime->tm_mon + 1),
                                                     (Bit16u) ltime->tm_mday);
                            }
                            time = &fad.ftLastWriteTime;
                            ull.LowPart = time->dwLowDateTime;
                            ull.HighPart = time->dwHighDateTime;
                            ttime = ull.QuadPart / 10000000ULL - 11644473600ULL;
                            if ((ltime = localtime(&ttime)) != 0) {
                                mtime = DOS_PackTime((Bit16u) ltime->tm_hour, (Bit16u) ltime->tm_min,
                                                     (Bit16u) ltime->tm_sec);
                                mdate = DOS_PackDate((Bit16u) (ltime->tm_year + 1900), (Bit16u) (ltime->tm_mon + 1),
                                                     (Bit16u) ltime->tm_mday);
                            }
                            sprintf(buf, "%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s", &fad.dwFileAttributes,
                                    &ctime, &cdate, &atime, &adate, &mtime, &mdate, &serial_number, &st, &st, &st, &st,
                                    &handle);
                            buf[32] = (char) ((Bit32u) fad.nFileSizeHigh % 256);
                            buf[33] = (char) (((Bit32u) fad.nFileSizeHigh % 65536) / 256);
                            buf[34] = (char) (((Bit32u) fad.nFileSizeHigh % 16777216) / 65536);
                            buf[35] = (char) ((Bit32u) fad.nFileSizeHigh / 16777216);
                            buf[36] = (char) ((Bit32u) fad.nFileSizeLow % 256);
                            buf[37] = (char) (((Bit32u) fad.nFileSizeLow % 65536) / 256);
                            buf[38] = (char) (((Bit32u) fad.nFileSizeLow % 16777216) / 65536);
                            buf[39] = (char) ((Bit32u) fad.nFileSizeLow / 16777216);
                            buf[40] = 1;
                            for (int i = 41; i < 47; i++) buf[i] = 0;
                            buf[52] = 0;
                            MEM_BlockWrite(SegPhys(ds) + reg_dx, buf, 53);
                            reg_ax = 0;
                            CALLBACK_SCF(false);
                        } else {
                            reg_ax = dos.errorcode;
                            CALLBACK_SCF(true);
                        }
                    } else {
                        reg_ax = dos.errorcode;
                        CALLBACK_SCF(true);
                    }
                    break;
                }
                case 0xa7:        /* LFN TimeConv */
                    switch (reg_bl) {
                        case 0x00:
                            reg_cl = Mem_Lodsb(SegPhys(ds) + reg_si);    //not yet a proper implementation,
                            reg_ch = Mem_Lodsb(SegPhys(ds) + reg_si + 1);    //but MS-DOS 7 and 4DOS DIR should
                            reg_dl = Mem_Lodsb(SegPhys(ds) + reg_si + 4);    //show date/time correctly now
                            reg_dh = Mem_Lodsb(SegPhys(ds) + reg_si + 5);
                            reg_bh = 0;
                            reg_ax = 0;
                            CALLBACK_SCF(false);
                            break;
                        case 0x01:
                            Mem_Stosb(SegPhys(es) + reg_di, reg_cl);
                            Mem_Stosb(SegPhys(es) + reg_di + 1, reg_ch);
                            Mem_Stosb(SegPhys(es) + reg_di + 4, reg_dl);
                            Mem_Stosb(SegPhys(es) + reg_di + 5, reg_dh);
                            reg_ax = 0;
                            CALLBACK_SCF(false);
                            break;
                        default:
                            E_Exit("DOS:Illegal LFN TimeConv call %2X", reg_bl);
                    }
                    break;
                case 0xa8:        /* LFN GenSFN */
                    if (reg_dh == 0 || reg_dh == 1) {
                        MEM_StrCopy(SegPhys(ds) + reg_si, name1, DOSNAMEBUF);
                        int i, j = 0;
                        char c[13], *s = strrchr(name1, '.');
                        for (i = 0; i < 8; j++) {
                            if (name1[j] == 0 || s - name1 <= j) break;
                            if (name1[j] == '.') continue;
                            sprintf(c, "%s%c", c, toupper(name1[j]));
                            i++;
                        }
                        if (s != NULL) {
                            s++;
                            if (s != 0 && reg_dh == 1) strcat(c, ".");
                            for (i = 0; i < 3; i++) {
                                if (*(s + i) == 0) break;
                                sprintf(c, "%s%c", c, toupper(*(s + i)));
                            }
                        }
                        MEM_BlockWrite(SegPhys(es) + reg_di, c, strlen(c) + 1);
                        reg_ax = 0;
                        CALLBACK_SCF(false);
                    } else {
                        reg_ax = 1;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 0xa9:        /* LFN Server Create */
                    reg_ax = 0x7100; // not implemented yet
                    CALLBACK_SCF(true);
                case 0xaa:        /* LFN Subst */
                    if (reg_bh > -1 && reg_bh < 3 && (reg_bl < 1 || reg_bl > 26)) {
                        reg_ax = DOSERR_INVALID_DRIVE;
                        CALLBACK_SCF(true);
                        break;
                    }
                    switch (reg_bh) {
                        case 0: {
                            Bit8u driveNo = reg_bl - 1;
                            MEM_StrCopy(SegPhys(ds) + reg_dx, name1, DOSNAMEBUF);
                            char *name = name1;
                            name = lTrim(name1);
                            if (!strlen(name1)) {
                                reg_ax = DOSERR_PATH_NOT_FOUND;
                                CALLBACK_SCF(true);
                                break;
                            }
                            bool changeBootDir = false;                                                        // C: can be changed once
                            char winDir[MAX_PATH_LEN];                                                        // Initially C: is set to Windows work directory
                            GetCurrentDirectory(MAX_PATH_LEN,
                                                winDir);                                        // No subdir has to be selected
                            if (winDir[strlen(winDir) - 1] !=
                                '\\')                                            // No files to be opened on C:
                                strcat(winDir, "\\");
                            if (driveNo == 2 && !*Drives[driveNo]->curdir &&
                                !strcmp(winDir, Drives[driveNo]->basedir)) {
                                changeBootDir = true;
                                for (Bit8u handle = 4; handle <
                                                       DOS_FILES; handle++)                        // Start was 0, autoexec.txt is opened by 4DOS, so...
                                    if (Files[handle])
                                        if (Files[handle]->GetDrive() == 2)
                                            changeBootDir = false;
                            }
                            if (Drives[driveNo] && !changeBootDir) {
                                reg_ax = 135;
                                CALLBACK_SCF(true);
                                break;
                            }
                            winDir[0] = 0;                                                                    // For eventual error message to test if used
                            int len = GetFullPathName(name1, MAX_PATH_LEN - 1, winDir,
                                                      NULL);                    // Optional lpFilePath is of no use
                            if (len) {
                                int attr = GetFileAttributes(winDir);
                                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                                    if (changeBootDir)
                                        Drives[2]->SetBaseDir(winDir);
                                    else
                                        Drives[driveNo] = new DOS_Drive(winDir, driveNo);
                                    reg_ax = 0;
                                    CALLBACK_SCF(false);
                                    break;
                                }
                            }
                            reg_ax = DOSERR_PATH_NOT_FOUND;
                            CALLBACK_SCF(true);
                            break;
                        }
                        case 1: {
                            Bit8u driveNo = reg_bl - 1;
                            char winDir[MAX_PATH_LEN];
                            GetCurrentDirectory(MAX_PATH_LEN, winDir);
                            if (winDir[strlen(winDir) - 1] != '\\')
                                strcat(winDir, "\\");
                            if (driveNo == DOS_GetDefaultDrive() || !Drives[driveNo] ||
                                driveNo == 2 && !*Drives[driveNo]->curdir &&
                                !strcmp(winDir, Drives[driveNo]->basedir)) {
                                reg_ax = DOSERR_INVALID_DRIVE;
                                CALLBACK_SCF(true);
                            } else {
                                bool inuse = false;
                                for (Bit8u handle = 4; handle < DOS_FILES; handle++)
                                    if (Files[handle])
                                        if (Files[handle]->GetDrive() == driveNo) {
                                            inuse = true;
                                            break;
                                        }
                                if (inuse) {
                                    reg_ax = 148;
                                    CALLBACK_SCF(true);
                                } else {
                                    delete Drives[driveNo];
                                    Drives[driveNo] = driveNo == 2 ? new DOS_Drive(winDir, driveNo) : NULL;
                                    reg_ax = 0;
                                    CALLBACK_SCF(false);
                                }
                            }
                            break;
                        }
                        case 2: {
                            Bit8u driveNo = reg_bl - 1;
                            if (Drives[driveNo]) {
                                strcpy(name1, Drives[driveNo]->GetWinDir());
                                MEM_BlockWrite(SegPhys(ds) + reg_dx, name1, (Bitu) (strlen(name1) + 1));
                                reg_ax = 0;
                                CALLBACK_SCF(false);
                            } else {
                                reg_ax = DOSERR_INVALID_DRIVE;
                                CALLBACK_SCF(true);
                            }
                            break;
                        }
                        default:
                            E_Exit("DOS:Illegal LFN Subst call %2X", reg_bl);
                    }
                    break;
                default:
                    reg_ax = 0x7100;
                    CALLBACK_SCF(true); //Check this! What needs this ? See default case
            }
            break;
        case 0x73:
            if (reg_al == 3) {
                MEM_StrCopy(SegPhys(ds) + reg_dx, name1, reg_cx);
                DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
                if (DOS_GetDiskFreeSpace32(name1, &sectors_per_cluster, &bytes_per_sector, &free_clusters,
                                           &total_clusters)) {
                    ext_space_info_t *info = new ext_space_info_t;
                    info->size_of_structure = sizeof(ext_space_info_t);
                    info->structure_version = 0;
                    info->sectors_per_cluster = sectors_per_cluster;
                    info->bytes_per_sector = bytes_per_sector;
                    info->available_clusters_on_drive = free_clusters;
                    info->total_clusters_on_drive = total_clusters;
                    info->available_sectors_on_drive = sectors_per_cluster * free_clusters;
                    info->total_sectors_on_drive = sectors_per_cluster * total_clusters;
                    info->available_allocation_units = free_clusters;
                    info->total_allocation_units = total_clusters;
                    MEM_BlockWrite(SegPhys(es) + reg_di, info, sizeof(ext_space_info_t));
                    delete (info);
                    reg_ax = 0;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
                break;
            }
        default:
            Bit32u ipVal = Mem_Lodsw(SegPhys(ss) + reg_sp);
            if (ipVal != 0x9539 && ipVal != 0x849d &&
                ipVal != 0x83d1)                    // Crude method to exclude 4DOS unhandled calls in log
                vpLog("Int 21 unhandled call %4X", reg_ax);
            reg_al = 0;                                                                    // Default value
            break;
    }
    return CBRET_NONE;
}

static Bitu BIOS_1BHandler(void) {
    Mem_Stosb(BIOS_KEYBOARD_FLAGS, 0x00);
    DOS_BreakFlag = true;
    return CBRET_NONE;
}

static Bitu DOS_20Handler(void) {
    reg_ah = 0;
    DOS_21Handler();
    return CBRET_NONE;
}

static Bitu DOS_27Handler(void)                                                        // Terminate & stay resident
{
    Bit16u para = (reg_dx + 15) / 16;
    Bit16u psp = dos.psp();
    if (DOS_ResizeMemory(psp, &para)) {
        DOS_Terminate(psp, true, 0);
        if (DOS_BreakINT23InProgress) throw int(0);
    }
    return CBRET_NONE;
}

static Bitu DOS_2526Handler(void) {
    reg_ax = 0x0103;
    SETFLAGBIT(CF, true);
    return CBRET_NONE;
}

static Bitu DOS_28Handler(void)                                                        // DOS idle
{
    idleCount = idleTrigger;
    return CBRET_NONE;
}

void DOS_Init() {
    CALLBACK_Install(0x20, &DOS_20Handler, CB_IRET);                                // DOS Int 20
    CALLBACK_Install(0x21, &DOS_21Handler, CB_IRET_STI);                            // DOS Int 21
    CALLBACK_Install(0x25, &DOS_2526Handler, CB_RETF);                                // DOS Int 25
    CALLBACK_Install(0x26, &DOS_2526Handler, CB_RETF);                                // DOS Int 26
    CALLBACK_Install(0x27, &DOS_27Handler, CB_IRET);                                // DOS Int 27
    CALLBACK_Install(0x28, &DOS_28Handler, CB_IRET_STI);                            // DOS Int 28 - Idle
    CALLBACK_Install(0x29, NULL, CB_INT29);                                            // DOS Int 29 - CON output
    CALLBACK_Install(0x1B, &BIOS_1BHandler, CB_IRET);                                // DOS Int 1B

    DOS_SetupFiles();                                                                // Setup system File tables
    DOS_SetupDevices();                                                                // Setup dos devices
    DOS_SetupTables();
    DOS_SetupMemory(ConfGetBool("low"));                                            // Setup first MCB
    DOS_SetupMisc();                                                                // Some additional dos interrupts

    usedrvs = ConfGetBool("usedrvs");
    if (usedrvs) {
        DWORD drives = GetLogicalDrives();
        char Drive[4];
        strcpy(Drive, "A:\\");
        for (int i = 0; i < 26; i++)
            if (drives & (1 << i)) {
                Drive[0] = 'A' + i;
                Drives[i] = new DOS_Drive(Drive, i, true);
            }
    }
    int d = 2;
    char winDirCur[512];                                                            // Initially set C: to Windows work directory
    if (GetCurrentDirectory(512, winDirCur)) {
        if (usedrvs) {
            if (winDirCur[0] >= 'A' && winDirCur[0] <= 'Z')
                d = winDirCur[0] - 'A';
            TCHAR *lppath = TEXT(winDirCur), lspath[512] = TEXT("");
            if (GetShortPathName(lppath, lspath, 512))
                DOS_ChangeDir(lspath);
        } else
            Drives[2] = new DOS_Drive(winDirCur, 2);
    }
    DOS_SDA(DOS_SDA_SEG, DOS_SDA_OFS).SetDrive(d);                                    // Default drive C:
    DOS_SetDefaultDrive(d);

    const char *dosver = lTrim(ConfGetString("dosver")), *p = strchr(dosver, '.');
    char cName[30];
    if (atoi(dosver) < 2 || atoi(dosver) > 9 || p != NULL && (atoi(p + 1) < 0 || atoi(p + 1) > 99)) {
        sprintf(cName, "DOSVER=%s", dosver);
        ConfAddError("Specified DOS version must be between 2.00 and 9.99\n", cName);
        dos.version.major = 7;
        dos.version.minor = 10;
    } else {
        dos.version.major = strlen(dosver) == 0 ? 7 : (Bit8u) (atoi(dosver));
        dos.version.minor = strlen(dosver) == 0 ? 10 : (p == NULL ? 0 : (Bit8u) (atoi(p + 1)));
    }
    dos.loaded_codepage = GetOEMCP();
    uselfn = ConfGetBool("lfn");
}
