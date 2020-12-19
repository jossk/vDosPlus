// Wengier: LFN support
#include "stdafx.h"

//#include <string.h>
//#include <stdlib.h>
#include "vdos.h"
#include "mem.h"
#include "dos_inc.h"
#include "support.h"
//#include <map>

char sname[LFN_NAMELENGTH + 1], slname[LFN_NAMELENGTH + 1], storect[127];
static std::map<int, Bit8u> samap, ramap, sdmap;
struct finddata {
    Bit8u attr;
    Bit8u fres1[3];
    Bit32u ctime;
    Bit32u cdate;
    Bit32u atime;
    Bit32u adate;
    Bit32u mtime;
    Bit32u mdate;
    Bit32u hsize;
    Bit32u size;
    Bit8u fres2[8];
    char lname[260];
    char sname[14];
} fd;

void DOS_ParamBlock::Clear(void) {
    memset(&exec, 0, sizeof(exec));
    memset(&overlay, 0, sizeof(overlay));
}

void DOS_ParamBlock::LoadData(void) {
    exec.envseg = (Bit16u) sGet(sExec, envseg);
    exec.cmdtail = sGet(sExec, cmdtail);
    exec.fcb1 = sGet(sExec, fcb1);
    exec.fcb2 = sGet(sExec, fcb2);
    exec.initsssp = sGet(sExec, initsssp);
    exec.initcsip = sGet(sExec, initcsip);
    overlay.loadseg = (Bit16u) sGet(sOverlay, loadseg);
    overlay.relocation = (Bit16u) sGet(sOverlay, relocation);
}

void DOS_ParamBlock::SaveData(void) {
    sSave(sExec, envseg, exec.envseg);
    sSave(sExec, cmdtail, exec.cmdtail);
    sSave(sExec, fcb1, exec.fcb1);
    sSave(sExec, fcb2, exec.fcb2);
    sSave(sExec, initsssp, exec.initsssp);
    sSave(sExec, initcsip, exec.initcsip);
}

void DOS_InfoBlock::SetLocation(Bit16u segment) {
    seg = segment;
    pt = seg << 4;
    // Clear the initial Block
    Mem_rStosb(pt, 0xff, sizeof(sDIB));
    Mem_rStosb(pt, 0, 14);

    sSave(sDIB, regCXfrom5e, (Bit16u) 0);
    sSave(sDIB, countLRUcache, (Bit16u) 0);
    sSave(sDIB, countLRUopens, (Bit16u) 0);

    sSave(sDIB, protFCBs, (Bit16u) 0);
    sSave(sDIB, specialCodeSeg, (Bit16u) 0);
    sSave(sDIB, joindedDrives, (Bit8u) 0);
    sSave(sDIB, lastdrive, (Bit8u) 0x01);        // increase this if you add drives to cds-chain

    sSave(sDIB, diskInfoBuffer, SegOff2dWord(segment, offsetof(sDIB, diskBufferHeadPt)));
    sSave(sDIB, setverPtr, (Bit32u) 0);

    sSave(sDIB, a20FixOfs, (Bit16u) 0);
    sSave(sDIB, pspLastIfHMA, (Bit16u) 0);
    sSave(sDIB, blockDevices, (Bit8u) 0);

    sSave(sDIB, bootDrive, (Bit8u) 0);
    sSave(sDIB, useDwordMov, (Bit8u) 1);
    sSave(sDIB, extendedSize, (TotEXTMB + TotXMSMB > 63 ? 63 : (TotEXTMB + TotXMSMB)) * 1024);
    sSave(sDIB, magicWord, (Bit16u) 0x0001);        // dos5+

    sSave(sDIB, sharingCount, (Bit16u) 0);
    sSave(sDIB, sharingDelay, (Bit16u) 0);
    sSave(sDIB, ptrCONinput, (Bit16u) 0);        // no unread input available
    sSave(sDIB, maxSectorLength, (Bit16u) 0x200);

    sSave(sDIB, dirtyDiskBuffers, (Bit16u) 0);
    sSave(sDIB, lookaheadBufPt, (Bit32u) 0);
    sSave(sDIB, lookaheadBufNumber, (Bit16u) 0);
    sSave(sDIB, bufferLocation, (Bit8u) 0);        // buffer in base memory, no workspace
    sSave(sDIB, workspaceBuffer, (Bit32u) 0);

    sSave(sDIB, minMemForExec, (Bit16u) 0);
    sSave(sDIB, memAllocScanStart, (Bit16u) DOS_MEM_START);
    sSave(sDIB, startOfUMBChain, (Bit16u) 0xffff);
    sSave(sDIB, chainingUMB, (Bit8u) 0);

    sSave(sDIB, nulNextDriver, (Bit32u) 0xffffffff);
    sSave(sDIB, nulAttributes, (Bit16u) 0x8004);
    sSave(sDIB, nulStrategy, (Bit32u) 0x00000000);
    sSave(sDIB, nulString[0], (Bit8u) 0x4e);
    sSave(sDIB, nulString[1], (Bit8u) 0x55);
    sSave(sDIB, nulString[2], (Bit8u) 0x4c);
    sSave(sDIB, nulString[3], (Bit8u) 0x20);
    sSave(sDIB, nulString[4], (Bit8u) 0x20);
    sSave(sDIB, nulString[5], (Bit8u) 0x20);
    sSave(sDIB, nulString[6], (Bit8u) 0x20);
    sSave(sDIB, nulString[7], (Bit8u) 0x20);

    // Create a fake SFT, so programs think there are 100 file handles
    Bit16u sftOffset = offsetof(sDIB, firstFileTable) + 0xa2;
    sSave(sDIB, firstFileTable, SegOff2dWord(segment, sftOffset));
    Mem_Stosd(segment, sftOffset + 0x00, SegOff2dWord(segment + 0x26, 0));    // Next File Table
    Mem_Stosw(segment, sftOffset + 0x04, 100);                            // File Table supports 100 files
    Mem_Stosd(segment + 0x26, 0x00, 0xffffffff);                            // Last File Table
    Mem_Stosw(segment + 0x26, 0x04, 100);                                // File Table supports 100 files
}

void DOS_InfoBlock::SetFirstMCB(Bit16u _firstmcb) {
    sSave(sDIB, firstMCB, _firstmcb);    //c2woody
}

void DOS_InfoBlock::SetBuffers(Bit16u x, Bit16u y) {
    sSave(sDIB, buffers_x, x);
    sSave(sDIB, buffers_y, y);
}

void DOS_InfoBlock::SetCurDirStruct(Bit32u _curdirstruct) {
    sSave(sDIB, curDirStructure, _curdirstruct);
}

void DOS_InfoBlock::SetFCBTable(Bit32u _fcbtable) {
    sSave(sDIB, fcbTable, _fcbtable);
}

void DOS_InfoBlock::SetDeviceChainStart(Bit32u _devchain) {
    sSave(sDIB, nulNextDriver, _devchain);
}

void DOS_InfoBlock::SetDiskBufferHeadPt(Bit32u _dbheadpt) {
    sSave(sDIB, diskBufferHeadPt, _dbheadpt);
}

Bit16u DOS_InfoBlock::GetStartOfUMBChain(void) {
    Bit16u umb_start = (Bit16u) sGet(sDIB, startOfUMBChain);
    if (umb_start != 0xffff && umb_start != EndConvMem)
        E_Exit("Corrupt UMB chain: %x", umb_start);
    return umb_start;
}

void DOS_InfoBlock::SetStartOfUMBChain(Bit16u _umbstartseg) {
    sSave(sDIB, startOfUMBChain, _umbstartseg);
}

Bit8u DOS_InfoBlock::GetUMBChainState(void) {
    return (Bit8u) sGet(sDIB, chainingUMB);
}

void DOS_InfoBlock::SetUMBChainState(Bit8u _umbchaining) {
    sSave(sDIB, chainingUMB, _umbchaining);
}

RealPt DOS_InfoBlock::GetPointer(void) {
    return SegOff2dWord(seg, offsetof(sDIB, firstDPB));
}

Bit32u DOS_InfoBlock::GetDeviceChain(void) {
    return sGet(sDIB, nulNextDriver);
}


// program Segment prefix

Bit16u DOS_PSP::rootpsp = 0;

void DOS_PSP::MakeNew(Bit16u mem_size) {
    // get previous
//	DOS_PSP prevpsp(dos.psp());
    // Clear it first
    Mem_rStosb(pt, 0, sizeof(sPSP));
    // Set size
    sSave(sPSP, next_seg, seg + mem_size);
    // far call opcode
    sSave(sPSP, far_call, 0xea);
    // far call to interrupt 0x21 - faked for bill & ted
    // lets hope nobody really uses this address
    sSave(sPSP, cpm_entry, SegOff2dWord(0xDEAD, 0xFFFF));
    // Standard blocks,int 20  and int21 retf
    sSave(sPSP, exit[0], 0xcd);
    sSave(sPSP, exit[1], 0x20);
    sSave(sPSP, service[0], 0xcd);
    sSave(sPSP, service[1], 0x21);
    sSave(sPSP, service[2], 0xcb);
    // psp and psp-parent
    sSave(sPSP, psp_parent, dos.psp());
    sSave(sPSP, prev_psp, 0xffffffff);
    sSave(sPSP, dos_version, 0x0005);
    // terminate 22,break 23,crititcal error 24 address stored
    SaveVectors();

    // FCBs are filled with 0
    // ....
    /* Init file pointer and max_files */
    sSave(sPSP, file_table, SegOff2dWord(seg, offsetof(sPSP, files)));
    sSave(sPSP, max_files, 20);
    for (Bit16u ct = 0; ct < 20; ct++)
        SetFileHandle(ct, 0xff);

    // User Stack pointer
//	if (prevpsp.GetSegment()!=0) sSave(sPSP,stack,prevpsp.GetStack());

    if (rootpsp == 0)
        rootpsp = seg;
}

Bit8u DOS_PSP::GetFileHandle(Bit16u index) {
    if (index >= sGet(sPSP, max_files))
        return 0xff;
    return Mem_Lodsb(dWord2Ptr(sGet(sPSP, file_table)) + index);
}

void DOS_PSP::SetFileHandle(Bit16u index, Bit8u handle) {
    if (index < sGet(sPSP, max_files))
        Mem_Stosb(dWord2Ptr(sGet(sPSP, file_table)) + index, handle);
}

Bit16u DOS_PSP::FindFreeFileEntry(void) {
    PhysPt files = dWord2Ptr(sGet(sPSP, file_table));
    Bit16u max = sGet(sPSP, max_files);
    for (Bit16u i = 0; i < max; i++)
        if (Mem_Lodsb(files + i) == 0xff)
            return i;
    return 0xff;
}

Bit16u DOS_PSP::FindEntryByHandle(Bit8u handle) {
    PhysPt files = dWord2Ptr(sGet(sPSP, file_table));
    Bit16u max = sGet(sPSP, max_files);
    for (Bit16u i = 0; i < max; i++)
        if (Mem_Lodsb(files + i) == handle)
            return i;
    return 0xFF;
}

void DOS_PSP::CopyFileTable(DOS_PSP *srcpsp, bool createchildpsp) {
    // Copy file table from calling process
    for (Bit16u i = 0; i < 20; i++) {
        Bit8u handle = srcpsp->GetFileHandle(i);
        if (createchildpsp) {    // copy obeying not inherit flag.(but dont duplicate them)
            if ((handle < DOS_FILES) && Files[handle] && !(Files[handle]->flags & DOS_NOT_INHERIT)) {
                Files[handle]->AddRef();
                SetFileHandle(i, handle);
            } else
                SetFileHandle(i, 0xff);
        } else    // normal copy so don't mind the inheritance
            SetFileHandle(i, handle);
    }
}

void DOS_PSP::CloseFiles(void) {
    Bit16u max = sGet(sPSP, max_files);
    for (Bit16u i = 0; i < max; i++)
        DOS_CloseFile(i);
}

void DOS_PSP::SaveVectors(void) {
    sSave(sPSP, int_22, RealGetVec(0x22));                                    // Save interrupt 22, 23, 24
    sSave(sPSP, int_23, RealGetVec(0x23));
    sSave(sPSP, int_24, RealGetVec(0x24));
}

void DOS_PSP::RestoreVectors(void) {
    RealSetVec(0x22, sGet(sPSP, int_22));                                    // Restore interrupt 22, 23, 24
    RealSetVec(0x23, sGet(sPSP, int_23));
    RealSetVec(0x24, sGet(sPSP, int_24));
}

void DOS_PSP::SetCommandTail(RealPt src) {
    if (src)                                                                // Valid source
    {
        Mem_rMovsb(pt + offsetof(sPSP, cmdtail), dWord2Ptr(src),
                   Mem_Lodsb(dWord2Ptr(src)) + 2);    // Terminating zero should be already in place
//		Mem_Stosb(dWord2Ptr(src)+Mem_Lodsb(dWord2Ptr(src))+2, 0);			// It isn't always !
    } else                                                                    // Empty
        Mem_Stosw(pt + offsetof(sPSP, cmdtail), 0x0d00);
}

void DOS_PSP::StoreCommandTail() {
    int len = Mem_StrLen(pt + offsetof(sPSP, cmdtail.buffer));
    Mem_StrnCopyFrom(storect, pt + offsetof(sPSP, cmdtail.buffer), len > 127 ? 127 : len);
}

void DOS_PSP::RestoreCommandTail() {
    Mem_Stosb(pt + offsetof(sPSP, cmdtail.count), strlen(storect) > 0 ? (Bit8u) strlen(storect) - 1 : 0);
    Mem_CopyTo(pt + offsetof(sPSP, cmdtail.buffer), storect, strlen(storect));
}

void DOS_PSP::SetFCB1(RealPt src) {
    if (src)
        Mem_rMovsb(SegOff2Ptr(seg, offsetof(sPSP, fcb1)), dWord2Ptr(src), 16);
}

void DOS_PSP::SetFCB2(RealPt src) {
    if (src)
        Mem_rMovsb(SegOff2Ptr(seg, offsetof(sPSP, fcb2)), dWord2Ptr(src), 16);
}

void DOS_PSP::SetNumFiles(Bit16u fileNum) {
    if (sGet(sPSP, max_files) < 20
        || (sGet(sPSP, max_files) == 20 && sGet(sPSP, file_table) >> 12 != pt)
        || (sGet(sPSP, max_files) > 20 && sGet(sPSP, file_table) >> 12 == pt))
        E_Exit("PSP file table (JFT) is messed up");
    if (fileNum >
        20)                                                                // Setup a table to accomondate more than 20 file handles?
    {
        if (sGet(sPSP, max_files) > 20)                                                // Already done before
            return;
        Bit16u new_table = DOS_GetPrivatMemory(
                16);                                    // Allocate 16 para's, maximize to 255 file handles to prevent DOS memory leaking with subsequent calls
        Mem_rStosb(new_table << 4, 0xff, 256);                                        // Init all to unused
        Mem_rMovsb(new_table << 4, dWord2Ptr(sGet(sPSP, file_table)), 20);            // Copy 20 entries of PSP over
        sSave(sPSP, file_table, SegOff2dWord(new_table, 0));                        // Store new file table address
        sSave(sPSP, max_files, fileNum);                                            // And max open files
    } else if (sGet(sPSP, max_files) ==
               20)                                            // New file handles <= 20 and PSP in use, nothing to do
        return;
    else                                                                            // We should test if none of the 20+ entries is in use
    {                                                                            // Returning error 4, but leave this responsibility to the caller
        Mem_rMovsb(pt + 0x18, dWord2Ptr(sGet(sPSP, file_table)),
                   20);                    // Copy first 20 entries back to PSP
        DOS_FreePrivatMemory(sGet(sPSP, file_table) >> 16,
                             16);                        // Try to release allocated memroyblock of extended file table
        sSave(sPSP, file_table,
              (pt << 12) + 0x18);                                        // Restore file table address to PSP
        sSave(sPSP, max_files, 20);                                                    // Max entries is always 20!
    }
}

void DOS_DTA::SetupSearch(Bit8u _sdrive, int handle, Bit8u _sattr, Bit8u _rattr, char *pattern) {
    if (handle == -2 || handle > -1 && fflfn) {
        sdmap[handle] = _sdrive;
        samap[handle] = _sattr;
        ramap[handle] = _rattr;
        int i;
        for (i = 0; i < LFN_NAMELENGTH; i++) {
            if (pattern[i] == 0) break;
            (handle == -2 ? slname[i] : sname[i]) = pattern[i];
        }
        while (i <= LFN_NAMELENGTH) (handle == -2 ? slname[i++] : sname[i++]) = 0;
    } else {
        sSave(sDTA, sdrive, _sdrive);
        sSave(sDTA, sattr, _sattr);
        //for (int i=0;i<4;i++)
        //sSave(sDTA, fill[i], i);
        Mem_rStosb(pt + offsetof(sDTA, spname), 0, 11);
        char *find_ext;
        find_ext = strchr(pattern, '.');
        if (find_ext) {
            Bitu size = (Bitu) (find_ext - pattern);
            if (size > 8) size = 8;
            Mem_CopyTo(pt + offsetof(sDTA, spname), pattern, size);
            find_ext++;
            Mem_CopyTo(pt + offsetof(sDTA, spext), find_ext, (strlen(find_ext) > 3) ? 3 : (Bitu) strlen(find_ext));
        } else
            Mem_CopyTo(pt + offsetof(sDTA, spname), pattern, (strlen(pattern) > 8) ? 8 : (Bitu) strlen(pattern));
    }
}

void DOS_DTA::SetResult(int handle, const char *_name, const char *_lname, Bit32u _hsize, Bit32u _size, Bit16u _date,
                        Bit16u _time, Bit16u _adate, Bit16u _atime, Bit16u _cdate, Bit16u _ctime, Bit8u _attr) {
    if (handle == -2 || handle > -1 && fflfn) {
        fd.hsize = _hsize;
        fd.size = _size;
        fd.adate = _adate;
        fd.atime = _atime;
        fd.cdate = _cdate;
        fd.ctime = _ctime;
        fd.mdate = _date;
        fd.mtime = _time;
        fd.attr = _attr;
        strcpy(fd.lname, _lname);
        strcpy(fd.sname, _name);
        if (handle > -1 && fflfn && !strcmp(fd.lname, fd.sname)) fd.sname[0] = 0;
    } else {
        Mem_CopyTo(pt + offsetof(sDTA, name), (void *) _name, DOS_NAMELENGTH_ASCII);
        sSave(sDTA, size, _size);
        sSave(sDTA, date, _date);
        sSave(sDTA, time, _time);
        sSave(sDTA, attr, _attr);
    }
}

void
DOS_DTA::GetResult(int handle, char *_name, char *_lname, Bit32u &_size, Bit16u &_date, Bit16u &_time, Bit8u &_attr) {
    if (handle == -2 || handle > -1 && fflfn) {
        strcpy(_name, fd.sname);
        strcpy(_lname, fd.lname);
        _size = fd.size;
        _date = fd.mdate;
        _time = fd.mtime;
        _attr = fd.attr;
    } else {
        Mem_CopyFrom(pt + offsetof(sDTA, name), _name, DOS_NAMELENGTH_ASCII);
        _size = sGet(sDTA, size);
        _date = (Bit16u) sGet(sDTA, date);
        _time = (Bit16u) sGet(sDTA, time);
        _attr = (Bit8u) sGet(sDTA, attr);
    }
}

int DOS_DTA::GetFindData(int fmt, char *fdstr, int *c) {
    if (fmt == 1)
        sprintf(fdstr, "%-1s%-3s%-2s%-2s%-4s%-2s%-2s%-4s%-2s%-2s%-4s%-4s%-4s%-8s%-260s%-14s", &fd.attr, &fd.fres1,
                &fd.ctime, &fd.cdate, &fd.ctime, &fd.atime, &fd.adate, &fd.atime, &fd.mtime, &fd.mdate, &fd.mtime,
                &fd.hsize, &fd.size, &fd.fres2, &fd.lname, &fd.sname);
    else
        sprintf(fdstr, "%-1s%-3s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-8s%-260s%-14s", &fd.attr, &fd.fres1, &fd.ctime,
                &fd.cdate, &fd.atime, &fd.adate, &fd.mtime, &fd.mdate, &fd.hsize, &fd.size, &fd.fres2, &fd.lname,
                &fd.sname);
    fdstr[28] = (char) fd.hsize % 256;
    fdstr[29] = (char) ((fd.hsize % 65536) / 256);
    fdstr[30] = (char) ((fd.hsize % 16777216) / 65536);
    fdstr[31] = (char) (fd.hsize / 16777216);
    fdstr[32] = (char) fd.size % 256;
    fdstr[33] = (char) ((fd.size % 65536) / 256);
    fdstr[34] = (char) ((fd.size % 16777216) / 65536);
    fdstr[35] = (char) (fd.size / 16777216);
    fdstr[44 + strlen(fd.lname)] = 0;
    fdstr[304 + strlen(fd.sname)] = 0;
    if (!strcmp(fd.sname, "?") && strlen(fd.lname))
        *c = 2;
    else
        *c = !strchr(fd.sname, '?') && strchr(fd.lname, '?') ? 1 : 0;
    return (sizeof(fd));
}

Bit8u DOS_DTA::GetSearchDrive(int handle) {
    return handle == -2 || handle > -1 && fflfn ? sdmap[handle] : (Bit8u) sGet(sDTA, sdrive);
}

void DOS_DTA::GetSearchParams(int handle, Bit8u &_sattr, Bit8u &_rattr, char *pattern) {
    if (handle == -2 || handle > -1 && fflfn) {
        memcpy(pattern, handle == -2 ? slname : sname, LFN_NAMELENGTH);
        pattern[LFN_NAMELENGTH] = 0;
        _sattr = samap[handle];
        _rattr = ramap[handle];
    } else {
        char temp[11];
        Mem_CopyFrom(pt + offsetof(sDTA, spname), temp, 11);
        for (int i = 0; i < 13; i++) pattern[i] = 0;
        memcpy(pattern, temp, 8);
        pattern[strlen(pattern)] = '.';
        memcpy(&pattern[strlen(pattern)], &temp[8], 3);
        _sattr = (Bit8u) sGet(sDTA, sattr);
        _rattr = 0;
    }
}

DOS_FCB::DOS_FCB(Bit16u seg, Bit16u off, bool allow_extended) {
    SetPt(seg, off);
    real_pt = pt;
    extended = false;
    if (allow_extended)
        if (sGet(sFCB, drive) == 0xff) {
            pt += 7;
            extended = true;
        }
}

bool DOS_FCB::Extended(void) {
    return extended;
}

void DOS_FCB::Create(bool _extended) {
    Mem_rStosb(real_pt, 0, _extended ? 36 + 7 : 36);
    pt = real_pt;
    if (_extended) {
        Mem_Stosb(real_pt, 0xff);
        pt += 7;
    }
    extended = _extended;
}

void DOS_FCB::SetName(Bit8u _drive, char *_fname, char *_ext) {
    sSave(sFCB, drive, _drive);
    Mem_CopyTo(pt + offsetof(sFCB, filename), _fname, 8);
    Mem_CopyTo(pt + offsetof(sFCB, ext), _ext, 3);
}

void DOS_FCB::SetResult(Bit32u _size, Bit16u _date, Bit16u _time, Bit8u _attr) {
    Mem_Stosd(pt + 0x1d, _size);
    Mem_Stosw(pt + 0x19, _date);
    Mem_Stosw(pt + 0x17, _time);
    Mem_Stosb(pt + 0x0c, _attr);
}

void DOS_FCB::SetSizeDateTime(Bit32u _size, Bit16u _date, Bit16u _time) {
    sSave(sFCB, filesize, _size);
    sSave(sFCB, date, _date);
    sSave(sFCB, time, _time);
}

void DOS_FCB::GetSizeDateTime(Bit32u &_size, Bit16u &_date, Bit16u &_time) {
    _size = sGet(sFCB, filesize);
    _date = (Bit16u) sGet(sFCB, date);
    _time = (Bit16u) sGet(sFCB, time);
}

void DOS_FCB::GetRecord(Bit16u &_cur_block, Bit8u &_cur_rec) {
    _cur_block = (Bit16u) sGet(sFCB, cur_block);
    _cur_rec = (Bit8u) sGet(sFCB, cur_rec);
}

void DOS_FCB::SetRecord(Bit16u _cur_block, Bit8u _cur_rec) {
    sSave(sFCB, cur_block, _cur_block);
    sSave(sFCB, cur_rec, _cur_rec);
}

void DOS_FCB::GetSeqData(Bit8u &_fhandle, Bit16u &_rec_size) {
    _fhandle = (Bit8u) sGet(sFCB, file_handle);
    _rec_size = (Bit16u) sGet(sFCB, rec_size);
}

void DOS_FCB::GetRandom(Bit32u &_random) {
    _random = sGet(sFCB, rndm);
}

void DOS_FCB::SetRandom(Bit32u _random) {
    sSave(sFCB, rndm, _random);
}

void DOS_FCB::FileOpen(Bit8u _fhandle) {
    sSave(sFCB, drive, GetDrive() + 1);
    sSave(sFCB, file_handle, _fhandle);
    sSave(sFCB, cur_block, 0);
    sSave(sFCB, rec_size, 128);
//	sSave(sFCB,rndm,0); // breaks Jewels of darkness. 
    Bit8u temp = RealHandle(_fhandle);
    Bit32u size = 0;
    Files[temp]->Seek(&size, DOS_SEEK_END);
    sSave(sFCB, filesize, size);
    size = 0;
    Files[temp]->Seek(&size, DOS_SEEK_SET);
    sSave(sFCB, time, Files[temp]->time);
    sSave(sFCB, date, Files[temp]->date);
}

bool DOS_FCB::Valid() {
    // Very simple check for Oubliette
    if (sGet(sFCB, filename[0]) == 0 && sGet(sFCB, file_handle) == 0)
        return false;
    return true;
}

void DOS_FCB::FileClose(Bit8u &_fhandle) {
    _fhandle = (Bit8u) sGet(sFCB, file_handle);
    sSave(sFCB, file_handle, 0xff);
}

Bit8u DOS_FCB::GetDrive(void) {
    Bit8u drive = (Bit8u) sGet(sFCB, drive);
    if (!drive)
        return DOS_GetDefaultDrive();
    else
        return drive - 1;
}

void DOS_FCB::GetName(char *fillname) {
    fillname[0] = GetDrive() + 'A';
    fillname[1] = ':';
    Mem_CopyFrom(pt + offsetof(sFCB, filename), &fillname[2], 8);
    fillname[10] = '.';
    Mem_CopyFrom(pt + offsetof(sFCB, ext), &fillname[11], 3);
    fillname[14] = 0;
}

void DOS_FCB::GetAttr(Bit8u &attr) {
    if (extended)
        attr = Mem_Lodsb(pt - 1);
}

void DOS_FCB::SetAttr(Bit8u attr) {
    if (extended)
        Mem_Stosb(pt - 1, attr);
}

void DOS_SDA::Init() {
    // Clear
    Mem_rStosb(pt, 0, sizeof(sSDA));
    sSave(sSDA, drive_crit_error, 0xff);
}
