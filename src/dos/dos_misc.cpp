// Wengier: MISC fix
#include "stdafx.h"

#include "vdos.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"
#include "../src/ints/xms.h"
#include "video.h"
#include "ttf.h"
#include "../src/ints/int10.h"
#include "inout.h"
#include "devicePRT.h"
//#include <map>

bool notopt, comcmd;
extern RealPt rcb;
extern RGBQUAD altBGR1[];
extern RECT rect, prevPosition;
extern DWORD ttfSizeData, ttfSizeDatab, ttfSizeDatai, ttfSizeDatabi;
extern void *ttfFontData, *ttfFontDatab, *ttfFontDatai, *ttfFontDatabi;
extern unsigned char vDosTTFbi[];
extern char fName[], fbName[], fiName[], fbiName[], icon[];
extern int sysicons, pSize, smallclr, curscr, transwin, winPerc;
extern int padding, padcolor, wheelmodv, wheelmodh, clickmodl, clickmodr;
extern int xyVersion, wpVersion, wsVersion, xyBackGround, wsBackGround;
extern int SetEuro(int n);
extern int getMaxScale(int *maxw, int *maxh, bool difscr), getScreen(bool def);
extern bool getScale(char* scale, int* scalex, int* scaley, bool* warn);
extern bool vgafixms, swapmod, evensize, chgicons, tobeframe, fontBoxed, usedrvs, spaceLow, filter83;
extern bool readTTF(const char *fName, bool bold, bool italics);
extern bool getWP(char * wpStr);
extern bool getWheelmod(char* wheelmod);
extern bool getClickmod(char* clickmod);
extern bool getPadding(char *p);
extern bool setColors(const char *value, int n);
extern bool setWinInitial(const char *winDef, bool *fs);
extern bool accWin(int maxWidth, int maxHeight), setWin(int pad=-1, int pac=-1);
extern bool resetWin(bool full=false, int pad=-1, int pac=-1);
extern void maxWin(), minWin();
extern HICON getIcon(char *icons, bool def);
extern BOOL getr, CALLBACK EnumDispProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM lParam);

static Bitu INT2F_Handler(void)
	{
	char name[256];
	switch (reg_ax)
		{
	case 0x1000:																	// SHARE.EXE installation check
		reg_al = 0xff;																// Pretend it's installed.
		break;
	case 0x1600:																	// MS Windows - WINDOWS ENHANCED MODE INSTALLATION CHECK
		reg_ax = 0;																	// Just failing doesn't reset fWin95 flag 4DOS
		break;
	case 0x1613:																	// Get SYSTEM.DAT path
		strcpy(name,"C:\\WINDOWS\\SYSTEM.DAT");
		Mem_CopyTo(SegPhys(es)+reg_di,name,(Bitu)(strlen(name)+1));
		reg_ax=0;
		reg_cx=(Bit16u)strlen(name);
		break;
	case 0x1680:																	// Release current virtual machine time-slice
		idleCount = idleTrigger;
		break;
	case 0x1700:
		reg_al = 1;
		reg_ah = 1;
		break;
	case 0x1701:
		reg_ax=OpenClipboard(NULL)?1:0;
		break;
	case 0x1702:
		reg_ax=0;
		if (OpenClipboard(NULL))
			{
			reg_ax=EmptyClipboard()?1:0;
			CloseClipboard();
			}
		break;
	case 0x1703:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			char *text, *buffer;
			text = new char[reg_cx];
			Mem_StrnCopyFrom(text,SegPhys(es)+reg_bx,reg_cx);
			*(text+reg_cx-1)=0;
			EmptyClipboard();
			HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, strlen(text)+1);
			if (clipbuffer)
				{
				buffer = (char*)GlobalLock(clipbuffer);
				if (buffer)
					{
					strcpy(buffer, text);
					GlobalUnlock(clipbuffer);
					SetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT,clipbuffer);
					reg_ax++;
					}
				}
			delete[] text;
			CloseClipboard();
			}
		break;
	case 0x1704:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				reg_ax=(Bit16u)strlen((char *)text)+1;
				reg_dx=(Bit16u)((strlen((char *)text)+1)/65536);
				}
			CloseClipboard();
			}
		break;
	case 0x1705:
		reg_ax=0;
		if ((reg_dx==1||reg_dx==7)&&OpenClipboard(NULL))
			{
			if (HANDLE text = GetClipboardData(reg_dx==1?CF_TEXT:CF_OEMTEXT))
				{
				Mem_CopyTo(SegPhys(es)+reg_bx,text,(Bitu)(strlen((char *)text)+1));
				reg_ax++;
				}
			CloseClipboard();
			}
		break;
	case 0x1708:
		reg_ax=CloseClipboard()?1:0;
		break;
	case 0x1a00:																	// ANSI.SYS installation check
		reg_al = 0xff;
		break;
	case 0x4300:																	// XMS installed check
		if (TotXMSMB)
			reg_al = 0x80;
		break;
	case 0x4310:																	// XMS handler seg:offset
		if (TotXMSMB)
			{
			SegSet16(es, RealSeg(xms_callback));
			reg_bx = RealOff(xms_callback);
			}
		break;			
	case 0x4a01:																	// Query free HMA space
	case 0x4a02:																	// Allocate HMA space
		reg_bx = 0;																	// Number of bytes available in HMA
		SegSet16(es, 0xffff);														// ES:DI = ffff:ffff HMA not used
		reg_di = 0xffff;
		break;
	case 0x4a16:																	// Open bootlog
		break;
	case 0x4a17:																	// Write bootlog
		Mem_StrnCopyFrom(name,SegPhys(ds)+reg_dx,255);
		name[255]=0;
		vpLog("BOOTLOG: %s",name);
		break;
	case 0x4a33:																	// Check MS-DOS Version 7
		reg_ax = 0;
		break;
	case 0xb800:																	// Network - installation check
		reg_al = 1;																	// Installed
		reg_bx = 8;																	// Bit 3 - redirector
		break;
	case 0xb809:																	// Network - get version
		reg_ax = 0x0201;															// Major-minor version as returned by NTVDM-Windows XP
		break;
	case 0x150b:																	// CD-ROM v2.00+ - DRIVE CHECK
	case 0x1689:
	case 0x168f:																	// Windows95 - CLOSE-AWARENESS
	case 0x4680:																	// MS Windows v3.0 - INSTALLATION CHECK
	case 0xb706:																	// DOS 4.0+ APPEND - GET APPEND FUNCTION STATE
	case 0xd44d:																	// 4DOS - Retrieve information from previous shell
	case 0xd44e:																	// 4DOS - AWAITING USER INPUT
		break;																		// Dummy to prevent log as unhandled
	default:
		INT2F_Cont();
		}
	return CBRET_NONE;
	}

// Following are vDos specific extensions to 4DOS

#define HELP(helptext) if (strstr(args, "/?")) { Display(helptext); return; }

static void Display(const char * format, ...)
	{
	char buf[1024];
	va_list msg;
	
	va_start(msg, format);
	vsnprintf(buf, 1024, format, msg);
	va_end(msg);

	Bit16u size = (Bit16u)strlen(buf);
	DOS_WriteFile(STDOUT, (Bit8u *)buf, &size);
	}

static void CLI_CHCP(char *args) {
    DOS_SetError(DOSERR_NONE);
    HELP("Display or change the current DOS code page.\n\nCHCP [nnn]\n\n  nnn   Specifies a code page number.\n");
    if (!*args) {
        Display("Active code page: %d\n", codepage);
        return;
    }
    int newCP;
    char buff[256];
    if (sscanf(args, "%d%s", &newCP, buff) == 1)                                    // Only one parameter
    {
        if (newCP >= 37)                                                            // Minimum (IBM EBCDIC US-Canada)
        {
            int missing = SetCodePage(newCP);
            if (missing != -1) {
                codepage = newCP;
                Display("Active code page: %d\n", codepage);
                if (missing)
                    Display("Characters not defined in active TTF font: %d\nYou could try another font instead\n", missing);
                return;
            }
        }
    }
    Display("Invalid code page number\n");
    return;
}

static void CLI_VER(char* args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("Displays the vDosPlus and DOS version.\n");
	Display("vDosPlus %s (build %s), reported DOS version: %d.%02d\n", vDosVersion, vDosBuild, dos.version.major, dos.version.minor);
	}

static void CLI_USE(char *args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("USE vDosPlus-drive-letter: Windows-directory\n\n  USE c: c:\\dosprogram\n\nAssigns DOS drive C: to the Windows folder c:\\dosprogram\n\nDrive letters already assigned can be unassigned by the UNUSE command.\n");
	args = lTrim(args);
	if (!*args)																		// If no arguments show active assignments
		{
		Display("\n      Windows path\n");
		for (int d = 0; d < DOS_DRIVES; d++)
			if (Drives[d])
				Display("%c: => %s%s\n", d+'A', Drives[d]->basedir, Drives[d]->autouse?" (autoused)":"");
		return;
		}
	if (strlen(args) < 2 || !isalpha(*args) || args[1] != ':')						// First argument should be drive letter:
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		char c, *p = strchr(args,' ');
		if (p!=NULL)
			{
			c=*p;
			*p=0;
			}
		Display("Invalid drive specification - %s\n", args);
		if (p!=NULL) *p=c;
		return;
		}
	char driveLtr =  toupper(*args);
	Bit8u driveNo = driveLtr-'A';
	char* assingTo = lTrim(args+2);													// Second argument should be the Windows path
	if (!*assingTo)
		{
		if (Drives[driveNo])
			Display("%c: => %s%s\n", driveLtr, Drives[driveNo]->basedir, Drives[driveNo]->autouse?" (autoused)":"");
		else
			{
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			Display("No Windows path is specified for %c:\n", driveLtr);
			}
		return;
		}

	bool changeBootDir = false;														// C: can be changed once
	char winDir[MAX_PATH_LEN];														// Initially C: is set to Windows work directory
	GetCurrentDirectory(MAX_PATH_LEN, winDir);										// No subdir has to be selected
	if (winDir[strlen(winDir)-1] != '\\')											// No files to be opened on C:
		strcat(winDir, "\\");
	if (driveNo == 2 && !*Drives[driveNo]->curdir && !strcmp(winDir, Drives[driveNo]->basedir))
		{
		changeBootDir = true;
		for (Bit8u handle = 4; handle < DOS_FILES; handle++)						// Start was 0, autoexec.txt is opened by 4DOS, so...
			if (Files[handle])
				if (Files[handle]->GetDrive() == 2)
					changeBootDir = false;
		}

	if (Drives[driveNo] && !changeBootDir)
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		Display("Drive already assigned - %c:\n", driveLtr);
		return;
		}
	while (strlen(assingTo) > 1 && *assingTo == '"' && assingTo[strlen(assingTo)-1] == '"')		// Surrounding by "'s not needed/wanted
		{
		assingTo[strlen(assingTo)-1] = 0;											// Remove them, eventually '\\' is appended
		assingTo++;
		}

	winDir[0] = 0;																	// For eventual error message to test if used
	int len = GetFullPathName(assingTo, MAX_PATH_LEN-1, winDir, NULL);				// Optional lpFilePath is of no use
	if (len)
		{
		int attr = GetFileAttributes(winDir);
		if (attr != INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_DIRECTORY))
			{
			if (changeBootDir)
				Drives[2]->SetBaseDir(winDir);
			else
				Drives[driveNo] = new DOS_Drive(winDir, driveNo);
			Display("%c => %s\n", driveLtr, Drives[driveNo]->basedir);
			return;
			}
		}
	Display("Directory does not exist - %s\n", winDir[0] != 0 ? winDir : assingTo);
	return;
	}
	
static void CLI_UNUSE(char* args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("UNUSE drive-letter\n\n  UNUSE d:\n\nUnassigns DOS drive D:\n");
	args = lTrim(args);
	if (!*args)
		{
		Display("You must specify a drive to unassign. Currently assigned drives include:\n");
		for (int d = 0; d < DOS_DRIVES; d++)
			if (Drives[d])
				Display(" %c:", d+'A');
		Display("\nMore information about these drives can be viewed with the bare USE command.\n");
		return;
		}
	if (strlen(args) > 3 || !isalpha(*args) || args[1] != ':' || strlen(args) == 3 && args[2] != '\\')
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		char *p = strchr(args,' ');
		if (p!=NULL)
			Display("Invalid parameter - %s\n", args);
		else
			Display("Invalid drive specification - %s\n", args);
		return;
		}
	char driveLtr = toupper(*args), winDir[MAX_PATH_LEN];
	Bit8u driveNo = driveLtr-'A';
	GetCurrentDirectory(MAX_PATH_LEN, winDir);
	if (winDir[strlen(winDir)-1] != '\\')
		strcat(winDir, "\\");
	if (!Drives[driveNo] || driveNo == 2 && !*Drives[driveNo]->curdir && !strcmp(winDir, Drives[driveNo]->basedir))
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		Display("Drive is not currently assigned - %c:\n", 'A'+driveNo);
		return;
		}
	if (driveNo != 2 && driveNo == DOS_GetDefaultDrive())
		{
		DOS_SetError(DOSERR_INVALID_DRIVE);
		Display("You cannot unassign the current drive - %c:\n", 'A'+driveNo);
		return;
		}
	bool inuse = false;
	for (Bit8u handle = 4; handle < DOS_FILES; handle++)
		if (Files[handle])
			if (Files[handle]->GetDrive() == driveNo)
				{
				inuse = true;
				break;
				}
	if (inuse)
		{
		DOS_SetError(ERROR_DRIVE_LOCKED);
		Display("Drive is currently in use - %c:\n", 'A'+driveNo);
		}
	else
		{
		delete Drives[driveNo];
		Drives[driveNo] = driveNo == 2 ? new DOS_Drive(winDir, driveNo) : NULL;
		}
	return;
	}

static void CLI_LABEL(char *args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("LABEL [drive-letter:] label\n\n  LABEL c: newlabel\n\nAssigns a new label to DOS drive C:\n");
	args = lrTrim(args);
	Bit8u driveNo = DOS_GetDefaultDrive();
	bool cur = true;
	char *p = strchr(args, ' '), *assingTo = NULL;
	if (p == NULL)
		assingTo = args;
	else
		*p = 0;
	if (strlen(args) == 2 && isalpha(*args) && args[1] == ':')						// First argument should be drive letter:
		{
		if (Drives[toupper(*args)-'A'])
			{
			cur = false;
			driveNo = toupper(*args)-'A';
			assingTo = p == NULL ? args+2 : lTrim(p+1);
			}
		else
			{
			DOS_SetError(DOSERR_INVALID_DRIVE);
			Display("Invalid drive specification - %c:\n", toupper(args[0]));
			return;
			}
		}
	if (assingTo==NULL)
		{
		if (p!=NULL) *p = ' ';
		assingTo = args;
		}
	if (!*assingTo)
		{
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		Display("Missing new label for the %s drive - %c:\nUse \"\" as the new label if you want to delete the current label.\n", cur?"current":"specified", 'A'+driveNo);
		return;
		}
	if (strlen(assingTo)>1 && *assingTo == '\"' && assingTo[strlen(assingTo)-1] == '\"')
		{
		assingTo[strlen(assingTo)-1] = 0;
		assingTo = lrTrim(assingTo+1);
		}
	char Drive[4];
	strcpy(Drive, "A:\\");
	Drive[0] = toupper(*Drives[driveNo]->basedir);
	if (Drive[0]=='\\')
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		Display("Cannot LABEL a network drive - %c:\n", 'A'+driveNo);
		}
	else if (!Drives[Drive[0]-'A'] || !Drives[Drive[0]-'A']->TestDir(""))
		{
		DOS_SetError(DOSERR_DRIVE_NOT_READY);
		Display("Drive not ready - %c:\n", 'A'+driveNo);
		}
	else if (SetVolumeLabel(Drive, strlen(assingTo) ? assingTo : NULL))
		Display("Setting label - %c: => %s\n", driveNo+'A', strlen(assingTo) ? assingTo : "(unlabeled)");
	else
		{
		int error = GetLastError();
		DOS_SetError(error);
		Display((error == DOSERR_ACCESS_DENIED || error == ERROR_WRITE_PROTECT)? "Access denied - %c:\n" : (error == ERROR_LABEL_TOO_LONG ? "Invalid volume label specified for %c:\n" : "Failed to set a volume label for %c:\n"), 'A'+driveNo);
		}
	return;
	}

static void CLI_SET(char *comLine)													// Syntax in 4DOS: set <name>=<%%winname%%>, preprocessor strips outer %'s
	{																				// Set <name>=<%winname%>, if <%winname%> set, it is replaced by <winvalue>
	DOS_SetError(DOSERR_NONE);
	if (strstr(comLine, "=%") && comLine[strlen(comLine)-1]=='%')					// Check for "=%...%", %%var%% is already substituted by %var%
		{
		char newComLine[124];
		if (ExpandEnvironmentStrings(comLine, newComLine, 123) <= 123)
			{
			if (!strcmp(comLine, newComLine))										// If no substitution by a Windows var
				*(strstr(newComLine, "=%")+1) = 0;									// Change it to "="/UNSET, else 4DOS will more or less set the variable
			LinPt cmdBuffAddr = SegPhys(ds)+reg_bx;
			Mem_Stosb(cmdBuffAddr+1, (Bit8u)strlen(newComLine));					// Store (new) length
			Mem_CopyTo(cmdBuffAddr+2, newComLine, strlen(newComLine)+1);			// And (new) command line
			}
		else
			{
			Display("Windows variable is too long\n");
			DOS_SetError(DOSERR_DATA_INVALID);
			}
		}
	}

static void showCfg(char* args, const char* cfg)
	{
	if (!*args) Display("%s=", cfg);
	if (!*args||!stricmp(args,cfg))
		{
		if (!stricmp(cfg,"AUHIDEMS"))
			Display("%s",autoHide?"ON":"OFF");
		else if (!stricmp(cfg,"AUTOEXEC"))
			Display("%s",autoexec);
		else if (!stricmp(cfg,"BLINKC"))
			Display("%s",blinkCursor?"ON":"OFF");
		else if (!stricmp(cfg,"BOITFONT"))
			Display("%s",fbiName);
		else if (!stricmp(cfg,"BOLDFONT"))
			Display("%s",fbName);
		else if (!stricmp(cfg,"CLICKMOD"))
			Display("%d,%d",clickmodl,clickmodr);
		else if (!stricmp(cfg,"COLS"))
			Display("%d",ttf.cols);
		else if (!stricmp(cfg,"DOSVER"))
			Display("%d.%d",dos.version.major,dos.version.minor);
		else if (!stricmp(cfg,"EURO"))
			Display("%d",SetEuro(NULL));
		else if (!stricmp(cfg,"EVENSIZE"))
			Display("%s",evensize?"ON":"OFF");
		else if (!stricmp(cfg,"FILTER83"))
			Display("%s",filter83?"ON":"OFF");
		else if (!stricmp(cfg,"FONT"))
			Display("%s%s",fontBoxed?"-":"",fName);
		else if (!stricmp(cfg,"FRAME"))
			Display("%s",window.framed?"ON":"OFF");
		else if (!stricmp(cfg,"ICON"))
			Display("%s",icon);
		else if (!stricmp(cfg,"ITALFONT"))
			Display("%s",fiName);
		else if (!stricmp(cfg,"KEYDELAY"))
			Display("%d",keydelay);
		else if (!stricmp(cfg,"KEYINTER"))
			Display("%d",keyinter);
		else if (!stricmp(cfg,"KEYMODE"))
			Display("%d",keymode);
		else if (!stricmp(cfg,"LFN"))
			Display("%s",uselfn?"ON":"OFF");
		else if (!stricmp(cfg,"LINS"))
			Display("%d",ttf.lins);
		else if (!stricmp(cfg,"MOUSE"))
			Display("%s",usesMouse?"ON":"OFF");
		else if (!stricmp(cfg,"PADDING"))
			Display("%d,%d",padding,padcolor);
		else if (!stricmp(cfg,"SCALE"))
			{
			if (window.scaley==10)
				Display("%d",abs(window.scalex));
			else
				Display("%d,%d",abs(window.scalex),abs(window.scaley));
			}
		else if (!stricmp(cfg,"SCREEN"))
			Display("%d",getScreen(false));
		else if (!stricmp(cfg,"SHORTCUT"))
			Display("%s",shortcut?"ON":"OFF");
		else if (!stricmp(cfg,"SHOWITAL"))
			Display("%s",showital?"ON":"OFF");
		else if (!stricmp(cfg,"SMALLCLR"))
			Display("%d",smallclr);
		else if (!stricmp(cfg,"SPACELOW"))
			Display("%s",spaceLow?"ON":"OFF");
		else if (!stricmp(cfg,"SPEAKER"))
			Display("%s",speaker?"ON":"OFF");
		else if (!stricmp(cfg,"STRIKOUT"))
			Display("%s",showsout?"ON":"OFF");
		else if (!stricmp(cfg,"SUBPSCR"))
			Display("%s",showsubp?"ON":"OFF");
		else if (!stricmp(cfg,"SYNCTIME"))
			Display("%s",synctime?"ON":"OFF");
		else if (!stricmp(cfg,"SYSICONS"))
			Display("%d",sysicons);
		else if (!stricmp(cfg,"TEMPDIR"))
			Display("%s",tempdir);
		else if (!stricmp(cfg,"TIMEOUT"))
			Display("%s",printTimeout?"ON":"OFF");
		else if (!stricmp(cfg,"TITLE"))
			Display("%s",vDosCaption);
		else if (!stricmp(cfg,"TOPWIN"))
			Display("%s",topwin?"ON":"OFF");
		else if (!stricmp(cfg,"TRANSWIN"))
			Display("%d",transwin);
		else if (!stricmp(cfg,"USEDRVS"))
			Display("%s",usedrvs?"ON":"OFF");
		else if (!stricmp(cfg,"VGAFIXMS"))
			Display("%s",vgafixms?"ON":"OFF");
		else if (!stricmp(cfg,"WHEELMOD"))
			Display("%d,%d",wheelmodv,wheelmodh);
		else if (!stricmp(cfg,"WINDOW"))
			{
			int perc=ttf.fullScrn?100:winPerc;
			HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO info;
			info.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &info);
			if (GetWindowRect(vDosHwnd, &rect))
				Display("%s%d,%d:%d", hPadding?"":"-", perc, rect.left-info.rcMonitor.left, rect.top-info.rcMonitor.top);
			else
				Display("%s%d", hPadding?"":"-",perc);
			}
		else if (!stricmp(cfg,"WINKEY"))
			Display("%s",winKey?"ON":"OFF");
		else if (!stricmp(cfg,"WINRUN"))
			Display("%s",winrun?"ON":"OFF");
		else if (!stricmp(cfg,"WP"))
			{
			if (wpVersion)
				Display("%d",wpVersion);
			else if (xyVersion||wsVersion)
				Display("%s,%d",xyVersion?"XY":"WS",xyVersion?xyBackGround:wsBackGround);
			}
		}
	else if (*args) return;
	if (!*args) Display("\n");
	notopt=false;
	}

static bool resetText(int pad=-1, int pac=-1)
	{
	bool ret=false;
	if (vga.mode==M_TEXT||pac==-2)
		{
		BOOL res=GetWindowRect(vDosHwnd, &rect);
		ret=(pad==-4||pad!=-3&&pad!=-4&&ttf.fullScrn)?resetWin():setWin(pad, pac);
		RECT newrect;
		BOOL cur=GetWindowRect(vDosHwnd, &newrect);
		if (res&&cur&&!ttf.fullScrn) MoveWindow(vDosHwnd, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2, newrect.right-newrect.left, newrect.bottom-newrect.top, true);
		}
	return ret;
	}

static void CLI_SETCFG(char * args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("SETCFG [option[=value]]\n\n  SETCFG BLINKC=OFF\n\nTurn off cursor blinking in vDosPlus\n\n  SETCFG WP=\n\nUse the default setting for the WP option\n\nNote: Most config options are supported by this command; type HELP SETCFG for more information. Use the SETPORT command to view or change COM and LPT port settings, and use the SETCOLOR command to view or change color settings.\n\nIf you do not specify any parameters for this command, then it will list the current settings; if only an option is given, then the current value for this option will be shown.\n");
	bool err=false;
	char *p;
	if (*args&&(p=strchr(args, '='))!=NULL)
		{
		*p=0;
		char key[LENNAME], value[255];
		if (strlen(lrTrim(args))&&strlen(lrTrim(args))<=LENNAME&&strlen(lrTrim(p+1))<=255)
			{
			strcpy(key, lrTrim(args));
			strcpy(value, lrTrim(p+1));
			sprintf(args, "%s=%s%c", key, value, 0);
			if (!stricmp(key,"LFN")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				uselfn=!!stricmp(value,"OFF");
			else if (!stricmp(key,"FILTER83")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				filter83=!stricmp(value,"ON");
			else if (!stricmp(key,"MOUSE")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				usesMouse=!stricmp(value,"ON");
				if (rcb!=NULL) RealSetVec(0x33, rcb+(usesMouse ? 0 : 5));
				}
			else if (!stricmp(key,"AUHIDEMS")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				autoHide=!stricmp(value,"ON");
				if (!autoHide)
					{
					mouseHidden=false;
					while (ShowCursor(true)<=0);
					}
				}
			else if (!stricmp(key,"VGAFIXMS")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				vgafixms=!stricmp(value,"ON");
			else if (!stricmp(key,"TOPWIN")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				topwin=!stricmp(value,"ON");
				GetWindowRect(vDosHwnd, &rect);
				SetWindowPos(vDosHwnd, topwin?HWND_TOPMOST:HWND_NOTOPMOST, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
				}
			else if (!stricmp(key,"BLINKC")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				blinkCursor=!!stricmp(value,"OFF");
			else if (!stricmp(key,"SPEAKER")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				speaker=!!stricmp(value,"OFF");
			else if (!stricmp(key,"SUBPSCR")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				bool sav=showsubp;
				showsubp=!!stricmp(value,"OFF");
				if (sav!=showsubp) resetText();
				}
			else if (!stricmp(key,"SHOWITAL")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				bool sav=showital;
				showital=!!stricmp(value,"OFF");
				if (sav!=showital) resetText();
				}
			else if (!stricmp(key,"STRIKOUT")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				bool sav=showsout;
				showsout=!stricmp(value,"ON");
				if (sav!=showsout) resetText();
				}
			else if (!stricmp(key,"SPACELOW")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				spaceLow=!stricmp(value,"ON");
			else if (!stricmp(key,"SYNCTIME")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				synctime=!!stricmp(value,"OFF");
			else if (!stricmp(key,"WINKEY")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				winKey=!!stricmp(value,"OFF");
			else if (!stricmp(key,"WINRUN")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				winrun=!!stricmp(value,"OFF");
			else if (!stricmp(key,"TIMEOUT")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				printTimeout=!!stricmp(value,"OFF");
			else if (!stricmp(key,"USEDRVS")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				char Drive[4], winDir[MAX_PATH_LEN];
				GetCurrentDirectory(MAX_PATH_LEN, winDir);
				if (winDir[strlen(winDir)-1] != '\\')
					strcat(winDir, "\\");
				usedrvs=!stricmp(value,"ON");
				if (usedrvs)
					{
					DWORD drives = GetLogicalDrives();
					strcpy(Drive,"A:\\");
					for (int i=0; i<26; i++)
						if ((!Drives[i] || i == 2 && !*Drives[i]->curdir && !strcmp(winDir, Drives[i]->basedir)) && drives & (1<<i))
							{
							Drive[0]='A'+i;
							Drives[i] = new DOS_Drive(Drive,i,true);
							if (i==2&&winDir[0]=='C')
								{
								TCHAR* lppath = TEXT(winDir), lspath[512] = TEXT("");
								if (GetShortPathName(lppath, lspath, 512))
									DOS_ChangeDir(lspath);
								}
							}
					}
				else
					{
					bool inuse;
					for (int i=0; i<26; i++)
						if (Drives[i] && Drives[i]->autouse && (i == 2 || i != DOS_GetDefaultDrive()))
							{
							inuse = false;
							for (Bit8u handle = 4; handle < DOS_FILES; handle++)
								if (Files[handle])
									if (Files[handle]->GetDrive() == i)
										{
										inuse = true;
										break;
										}
							if (inuse) continue;
							delete Drives[i];
							Drives[i] = i == 2 ? new DOS_Drive(winDir, i) : NULL;
							}
					}
				}
			else if (!stricmp(key,"SHORTCUT")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				if (ttf.fullScrn)
					{
					if (!shortcut&&!stricmp(value,"ON"))
						ttf.offY--;
					else if (shortcut&&stricmp(value,"ON"))
						ttf.offY++;
					shortcut=!stricmp(value,"ON");
					for (int i=0;i<3;i++)
						resetWin();
					}
				else
					shortcut=!stricmp(value,"ON");
				}
			else if (!stricmp(key,"FRAME")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				window.framed=!stricmp(value,"ON");
				resetText(-4,-2);
				if (ttf.fullScrn) tobeframe=true;
				}
			else if (!stricmp(key,"WINDOW"))
				{
				bool fs=ttf.fullScrn, f=true, h=hPadding, r=setWinInitial(value,&f);
				if (r)
					{
					int maxWidth=0, maxHeight=0;
					getMaxScale(&maxWidth, &maxHeight, false);
					if (fs&&ttf.fullScrn)
						{
						if (vga.mode==M_TEXT)
							{
							if (h!=hPadding)
								{
								minWin();
								resetText(-3);
								maxWin();
								}
							}
						else if (f)
							{
							SetVideoSize();
							BOOL res=GetWindowRect(vDosHwnd, &rect);
							resetWin();
							if (res)
								SetWindowPos(vDosHwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
							}
						}
					else if (!fs&&ttf.fullScrn)
						{
						ttf.fullScrn=false;
						if (h!=hPadding) resetText();
						maxWin();
						}
					else if (accWin(maxWidth, maxHeight))
						{
						BOOL res=GetWindowRect(vDosHwnd, &rect);
						SetVideoSize();
						if (f) res=GetWindowRect(vDosHwnd, &rect);
						resetWin();
						if (res)
							{
							if (f||vga.mode!=M_TEXT)
								SetWindowPos(vDosHwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
							else
								{
								HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
								MONITORINFO info;
								info.cbSize = sizeof(MONITORINFO);
								GetMonitorInfo(monitor, &info);
								RECT newrect;
								BOOL cur=GetWindowRect(vDosHwnd, &newrect);
								if (fs&&getr) rect=prevPosition;
								int left=rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2, top=rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2;
								if (cur&&left>=info.rcMonitor.left&&top>=info.rcMonitor.top&&left+newrect.right-newrect.left<=info.rcMonitor.right&&top+newrect.bottom-newrect.top<=info.rcMonitor.bottom)
									MoveWindow(vDosHwnd, left, top, newrect.right-newrect.left, newrect.bottom-newrect.top, true);
								}
							}
						}
					else if (speaker) Beep(1750, 300);
					}
				else if (r==NULL)
					{
					if (speaker) Beep(1750, 300);
					}
				else err=true;
				}
			else if (!stricmp(key,"SCREEN")&&(!strlen(value)||!stricmp(value,"0")||strtol(value,NULL,10)>0))
				{
				int arg=0, scr = getScreen(false), maxWidth=0, maxHeight=0;
				getMaxScale(&maxWidth, &maxHeight, false);
				screen=strtol(value,NULL,10)>0?strtol(value,NULL,10):getScreen(true);
				xyp xy={0};
				xy.x=-1;
				xy.y=-1;
				curscr=0;
				EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
				if (scr>0&&scr==screen)
					;
				else if (screen<=curscr)
					{
					char perc[5];
					sprintf(perc,"%d",ttf.fullScrn?100:winPerc);
					int maxScale=getMaxScale(&maxWidth, &maxHeight, true);
					if (window.scalex<0) window.scalex=-(window.scaley==10?maxScale:maxWidth/640);
					if (window.scaley<0) window.scaley=-(maxHeight/480);
					bool fs=false, f=true;
					setWinInitial(perc,&f);
					if (ttf.fullScrn&&vga.mode==M_TEXT)
						{
						fs=true;
						tobeframe=false;
						minWin();
						}
					if (vga.mode!=M_TEXT||accWin(maxWidth, maxHeight))
						{
						SetVideoSize();
						BOOL res=GetWindowRect(vDosHwnd, &rect);
						resetWin();
						if (res) SetWindowPos(vDosHwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
						if (fs) maxWin();
						if (!strtol(value,NULL,10)) screen=0;
						}
					else
						{
						screen=-scr;
						if (speaker) Beep(1750, 300);
						}
					}
				else
					{
					screen=-scr;
					if (speaker) Beep(1750, 300);
					}
				}
			else if (!stricmp(key,"EVENSIZE")&&(!strlen(value)||!stricmp(value,"ON")||!stricmp(value,"OFF")))
				{
				if ((evensize&&!stricmp(value,"OFF")||!evensize&&stricmp(value,"OFF")))
					{
					if (pSize%2&&vga.mode!=M_TEXT)
						{
						if (speaker) Beep(1750, 300);
						}
					else
						{
						evensize=!!stricmp(value,"OFF");
						if (pSize%2)
							{
							bool fs=ttf.fullScrn;
							if (fs) minWin();
							resetText(-3);
							if (fs) maxWin();
							}
						}
					}
				}
			else if (!stricmp(key,"KEYDELAY")&&(!strlen(value)||!stricmp(value,"0")||strtol(value,NULL,10)>0))
				keydelay=strtol(value,NULL,10)>0?strtol(value,NULL,10):0;
			else if (!stricmp(key,"KEYINTER")&&(!strlen(value)||!stricmp(value,"0")||strtol(value,NULL,10)>0))
				keyinter=strtol(value,NULL,10)>0?strtol(value,NULL,10):0;
			else if (!stricmp(key,"KEYMODE")&&(!strlen(value)||!stricmp(value,"0")||!stricmp(value,"1")||!stricmp(value,"2")))
				keymode=!stricmp(value,"")?2:strtol(value,NULL,10);
			else if (!stricmp(key,"AUTOEXEC"))
				strcpy(autoexec, strlen(value)?value:"AUTOEXEC.TXT");
			else if (!stricmp(key,"TITLE"))
				{
				strcpy(vDosCaption, strlen(value)?value:"vDosPlus");
				SetWindowText(vDosHwnd, vDosCaption);
				}
			else if (!stricmp(key,"PADDING"))
				{
				int pad=padding, pac=padcolor;
				if (vga.mode!=M_TEXT)
					{
					if ((padding||padcolor||strlen(value)&&stricmp(value,"0")&&stricmp(value,"0,0"))&&speaker) Beep(1750, 300);
					}
				else if (getPadding(value))
					{
					if (ttf.fullScrn)
						resetWin(true,pad,pac);
					else
						resetText(pad,pac);
					}
				else err=true;
				}
			else if (!stricmp(key,"EURO")&&(!strlen(value)||atoi(value)==-1||atoi(value)>=33&&atoi(value)<=255))
				{
				SetEuro(strlen(value)?atoi(value):-1);
				resetText(-4);
				}
			else if (!stricmp(key,"LINS")&&(!strlen(value)||atoi(value)>=24&&atoi(value)<=txtMaxLins))
				{
				if (vga.mode == M_TEXT)
					{
					bool setmode=true;
					BOOL res=false;
					int linsk=ttf.lins;
					ttf.lins=strlen(value)?atoi(value):25;
					if (linsk!=ttf.lins)
						{
						if (ttf.fullScrn)
							{
							if (!resetWin(true,linsk,-1))
								setmode=false;
							}
						else
							{
							res=GetWindowRect(vDosHwnd, &rect);
							if (!setWin(linsk,-1))
								setmode=false;
							}
						if (setmode)
							{
							for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++)
								ModeList_VGA[i].theight = ttf.lins;
							INT10_SetVideoMode(CurMode?CurMode->mode:3);
							RECT newrect;
							BOOL cur=GetWindowRect(vDosHwnd, &newrect);
							if (res&&cur&&!ttf.fullScrn)
								{
								HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
								MONITORINFO info;
								info.cbSize = sizeof(MONITORINFO);
								GetMonitorInfo(monitor, &info);
								MoveWindow(vDosHwnd, max(info.rcMonitor.left, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2), max(info.rcMonitor.top, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2), newrect.right-newrect.left, newrect.bottom-newrect.top, true);
								}
							}
						}
					}
				else if (ttf.lins!=(strlen(value)?atoi(value):25)&&speaker) Beep(1750, 300);
				}
			else if (!stricmp(key,"COLS")&&(!strlen(value)||atoi(value)>=60&&atoi(value)<=txtMaxCols))
				{
				if (vga.mode == M_TEXT)
					{
					bool setmode=true;
					BOOL res=false;
					int colsk=ttf.cols;
					ttf.cols=strlen(value)?atoi(value):60;
					if (colsk!=ttf.cols)
						{
						if (ttf.fullScrn)
							{
							if (!resetWin(true,-1,colsk))
								setmode=false;
							}
						else
							{
							res=GetWindowRect(vDosHwnd, &rect);
							if (!setWin(-1,colsk))
								setmode=false;
							}
						if (setmode)
							{
							for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++)
								ModeList_VGA[i].twidth = ttf.cols;
							INT10_SetVideoMode(CurMode?CurMode->mode:3);
							RECT newrect;
							BOOL cur=GetWindowRect(vDosHwnd, &newrect);
							if (res&&cur&&!ttf.fullScrn)
								{
								HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
								MONITORINFO info;
								info.cbSize = sizeof(MONITORINFO);
								GetMonitorInfo(monitor, &info);
								MoveWindow(vDosHwnd, max(info.rcMonitor.left, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2), max(info.rcMonitor.top, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2), newrect.right-newrect.left, newrect.bottom-newrect.top, true);
								}
							}
						}
					}
				else if (ttf.cols!=(strlen(value)?atoi(value):60)&&speaker) Beep(1750, 300);
				}
			else if (!stricmp(key,"SCALE"))
				{
				int scalex=0,scaley=0;
				bool warn=false;
				if (getScale(value,&scalex,&scaley,&warn))
					{
					if (window.scalex!=scalex||window.scaley!=scaley)
						{
						window.scalex=scalex;
						window.scaley=scaley;
						if (vga.mode != M_TEXT)
							{
							BOOL res=GetWindowRect(vDosHwnd, &rect);
							resetWin();
							RECT newrect;
							BOOL cur=GetWindowRect(vDosHwnd, &newrect);
							if (res&&cur)
								{
								HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
								MONITORINFO info;
								info.cbSize = sizeof(MONITORINFO);
								GetMonitorInfo(monitor, &info);
								MoveWindow(vDosHwnd, max(info.rcMonitor.left, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2), max(info.rcMonitor.top, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2), newrect.right-newrect.left, newrect.bottom-newrect.top, true);
								}
							}
						}
					}
				else if (speaker) Beep(1750, 300);
				}
			else if (!stricmp(key,"TRANSWIN")&&(!strlen(value)||!strcmp(value,"0")||atoi(value)>=1&&atoi(value)<=90))
				{
				transwin=!strlen(value)||!strcmp(value,"0")?0:atoi(value);
				if (transwin)
					{
					SetWindowLong(vDosHwnd, GWL_EXSTYLE, GetWindowLong(vDosHwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
					SetLayeredWindowAttributes(vDosHwnd, 0, 255*(100-transwin)/100, LWA_ALPHA);
					}
				else
					SetWindowLong(vDosHwnd, GWL_EXSTYLE, GetWindowLong(vDosHwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
				}
			else if (!stricmp(key,"SYSICONS")&&(!strlen(value)||!stricmp(value,"0")||!stricmp(value,"1")||!stricmp(value,"2")||!stricmp(value,"3")))
				{
				sysicons=!strlen(value)?2:strtol(value,NULL,10);
				chgicons=true;
				}
			else if (!stricmp(key,"DOSVER")&&!strlen(value)||atoi(value)>1&&atoi(value)<10)
				{
				char *dp = strchr(value,'.');
				if (dp!=NULL&&(atoi(dp+1)<0||atoi(dp+1)>99))
					err=true;
				else
					{
					dos.version.major = strlen(value)==0?7:(Bit8u)(atoi(value));
					dos.version.minor = strlen(value)==0?10:(dp==NULL?0:(Bit8u)(atoi(dp+1)));
					}
				}
			else if (!stricmp(key,"CLICKMOD")&&getClickmod(value))
				;
			else if (!stricmp(key,"WHEELMOD")&&getWheelmod(value))
				swapmod=false;
			else if (!stricmp(key,"SMALLCLR")&&(!strlen(value)||!stricmp(value,"0")||strtol(value,NULL,10)>0&&strtol(value,NULL,10)<16))
				{
				smallclr=!strlen(value)||!stricmp(value,"0")?0:strtol(value,NULL,10);
				resetText();
				}
			else if (!stricmp(key,"WP")&&(getWP(value)||!strlen(value)))
				resetText();
			else if (!stricmp(key,"TEMPDIR"))
				strcpy(tempdir,value);
			else if (!stricmp(key,"ICON"))
				{
				HICON IcoHwnd = getIcon(value,false);
				if (IcoHwnd)
					{
					bool fs=vga.mode==M_TEXT&&ttf.fullScrn;
					if (fs) minWin();
					if (window.framed)
						SetClassLongPtr(vDosHwnd, GCLP_HICON, (LONG_PTR)IcoHwnd);
					else
						{
						window.framed=true;
						BOOL res=GetWindowRect(vDosHwnd, &rect);
						resetWin();
						SetClassLongPtr(vDosHwnd, GCLP_HICON, (LONG_PTR)IcoHwnd);
						window.framed=false;
						resetWin();
						RECT newrect;
						BOOL cur=GetWindowRect(vDosHwnd, &newrect);
						if (res&cur) MoveWindow(vDosHwnd, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2, newrect.right-newrect.left, newrect.bottom-newrect.top, true);
						}
					if (fs) maxWin();
					}
				else if (speaker) Beep(1750, 300);
				}
			else if (!stricmp(key,"FONT"))
				{
				bool fb = false;
				char *v = value;
				if (*v == '-')
					{
					fb = true;
					v = lTrim(v+1);
					}
				void *fontData = ttfFontData;
				DWORD sizeData = ttfSizeData;
				TTF_Font *font;
				if (vga.mode == M_TEXT && readTTF(v,false,false) && (font=TTF_New_Memory_Face((const unsigned char*)ttfFontData, ttfSizeData, ttf.pointsize, NULL))!=NULL)
					{
					TTF_Font *savfont=ttf.font;
					ttf.font=font;
					bool fail=false, fs=ttf.fullScrn;
					if (fs) minWin();
					if (!resetText(-2)) fail=true;
					if (fs&&!fail) maxWin();
					if (fail)
						{
						ttfFontData=fontData;
						ttfSizeData=sizeData;
						ttf.font=savfont;
						resetText(-2);
						if (fs)	maxWin();
						if (speaker) Beep(1750, 300);
						}
					else
						{
						strcpy(fName,v);
						if (!strlen(fName)&&(strlen(fbName)||strlen(fiName)||strlen(fbiName)))
							{
							strcpy(fbName,"");
							readTTF(fbName,true,false);
							strcpy(fiName,"");
							readTTF(fiName,false,true);
							strcpy(fbiName,"");
							readTTF(fbiName,true,true);
							ttf.fontb = NULL;
							ttf.fonti = NULL;
							ttf.fontbi = NULL;
							}
						fontBoxed = fb;
						int missing = SetCodePage(codepage);
						if (missing>0)
							Display("Characters not defined in this TTF font: %d\nYou could try another font instead or use a different code page\n", missing);
						}
					}
				else
					{
					ttfFontData=fontData;
					ttfSizeData=sizeData;
					if ((strlen(fName) || strlen(value)) && speaker) Beep(1750, 300);
					}
				}
			else if (!stricmp(key,"BOLDFONT")||!stricmp(key,"ITALFONT")||!stricmp(key,"BOITFONT"))
				{
				void *fontData = !stricmp(key,"BOLDFONT")?ttfFontDatab:!stricmp(key,"ITALFONT")?ttfFontDatai:ttfFontDatabi;
				DWORD sizeData = !stricmp(key,"BOLDFONT")?ttfSizeDatab:!stricmp(key,"ITALFONT")?ttfSizeDatai:ttfSizeDatabi;
				if (vga.mode == M_TEXT && (strlen(fName) || !strlen(value)) && readTTF(value,!stricmp(key,"BOLDFONT")||!stricmp(key,"BOITFONT"),!stricmp(key,"ITALFONT")||!stricmp(key,"BOITFONT")))
					{
					void *newFontData = !stricmp(key,"BOLDFONT")?ttfFontDatab:!stricmp(key,"ITALFONT")?ttfFontDatai:ttfFontDatabi;
					DWORD newSizeData = !stricmp(key,"BOLDFONT")?ttfSizeDatab:!stricmp(key,"ITALFONT")?ttfSizeDatai:ttfSizeDatabi;
					TTF_Font *font;
					if (newFontData == vDosTTFbi||(font=TTF_New_Memory_Face((const unsigned char*)newFontData, newSizeData, ttf.pointsize, NULL))!=NULL)
						{
						strcpy(!stricmp(key,"BOLDFONT")?fbName:!stricmp(key,"ITALFONT")?fiName:fbiName,value);
						(!stricmp(key,"BOLDFONT")?ttf.fontb:!stricmp(key,"ITALFONT")?ttf.fonti:ttf.fontbi) = newFontData == vDosTTFbi?NULL:font;
						bool fs=ttf.fullScrn;
						if (fs) minWin();
						resetText(-3);
						if (fs) maxWin();
						}
					else
						{
						(!stricmp(key,"BOLDFONT")?ttfFontDatab:!stricmp(key,"ITALFONT")?ttfFontDatai:ttfFontDatabi)=fontData;
						(!stricmp(key,"BOLDFONT")?ttfSizeDatab:!stricmp(key,"ITALFONT")?ttfSizeDatai:ttfSizeDatabi)=sizeData;
						if (speaker) Beep(1750, 300);
						}
					}
				else if ((strlen(!stricmp(key,"BOLDFONT")?fbName:!stricmp(key,"ITALFONT")?fiName:fbiName) || strlen(value)) && speaker) Beep(1750, 300);
				}
			else if (!stricmp(key,"COLORS")||!stricmp(key,"CONFWARN")||!stricmp(key,"LOW")||!stricmp(key,"XMEM"))
				{
				Display("The following config options are not supported by this command:\n\n  COLORS\n  CONFWARN\n  LOW\n  XMEM\n\nUse the SETCOLOR command to view or change color settings.\n", lTrim(args));
				DOS_SetError(DOSERR_DATA_INVALID);
				}
			else if (strlen(key)==4&&(!strnicmp(key,"LPT",3)||!strnicmp(key,"COM",3))&&key[3]>'0'&&key[3]<='9')
				{
				Display("Use the SETPORT command to view or change the COM and LPT port settings.\n");
				DOS_SetError(DOSERR_DATA_INVALID);
				}
			else
				err=true;
			}
		else
			{
			*p='=';
			err=true;
			}
		if (err)
			{
			Display("Invalid configuration setting - %s\n", lTrim(args));
			DOS_SetError(DOSERR_DATA_INVALID);
			}
		}
	else
		{
		args=lrTrim(args);
		notopt=true;
		showCfg(args,"AUHIDEMS");
		showCfg(args,"AUTOEXEC");
		showCfg(args,"BLINKC");
		showCfg(args,"BOITFONT");
		showCfg(args,"BOLDFONT");
		showCfg(args,"CLICKMOD");
		showCfg(args,"COLS");
		showCfg(args,"DOSVER");
		showCfg(args,"EURO");
		showCfg(args,"EVENSIZE");
		showCfg(args,"FILTER83");
		showCfg(args,"FONT");
		showCfg(args,"FRAME");
		showCfg(args,"ICON");
		showCfg(args,"ITALFONT");
		showCfg(args,"KEYDELAY");
		showCfg(args,"KEYINTER");
		showCfg(args,"KEYMODE");
		showCfg(args,"LFN");
		showCfg(args,"LINS");
		showCfg(args,"MOUSE");
		showCfg(args,"PADDING");
		showCfg(args,"SCALE");
		showCfg(args,"SCREEN");
		showCfg(args,"SHORTCUT");
		showCfg(args,"SHOWITAL");
		showCfg(args,"SPACELOW");
		showCfg(args,"SMALLCLR");
		showCfg(args,"SPEAKER");
		showCfg(args,"STRIKOUT");
		showCfg(args,"SUBPSCR");
		showCfg(args,"SYNCTIME");
		showCfg(args,"SYSICONS");
		showCfg(args,"TEMPDIR");
		showCfg(args,"TIMEOUT");
		showCfg(args,"TITLE");
		showCfg(args,"TOPWIN");
		showCfg(args,"TRANSWIN");
		showCfg(args,"USEDRVS");
		showCfg(args,"VGAFIXMS");
		showCfg(args,"WHEELMOD");
		showCfg(args,"WINDOW");
		showCfg(args,"WINKEY");
		showCfg(args,"WINRUN");
		showCfg(args,"WP");
		if (!stricmp(args,"COLORS")||!stricmp(args,"CONFWARN")||!stricmp(args,"LOW")||!stricmp(args,"XMEM"))
			Display("The following config options are not supported by this command:\n\n  COLORS\n  CONFWARN\n  LOW\n  XMEM\n\nUse the SETCOLOR command to view or change color settings.");
		else if (strlen(args)==4&&(!strnicmp(args,"LPT",3)||!strnicmp(args,"COM",3))&&args[3]>'0'&&args[3]<='9')
			Display("Use the SETPORT command to view or change the COM and LPT port settings.");
		else if (notopt) Display("Not a config option - %s",args);
		if (*args) Display("\n");
		}
	}
	
static void CLI_SETCOLOR(char * args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("SETCOLOR [color# [value]]\n\n  SETCOLOR 1 (50,50,50)\n\nChange Color #1 to the specified color value\n\n  SETCOLOR 7 -\n\nReturn Color #7 to the default color value\n\n  SETCOLOR MONO\n\nDisplay current MONO mode status\n");
	if (*args)
		{
		args=lrTrim(args);
		char *p = strchr(args, ' ');
		if (p!=NULL)
			*p=0;
		int i=atoi(args);
		if (!stricmp(args,"MONO"))
			{
			if (p==NULL)
				Display("MONO mode status: %s (video mode %d)\n",CurMode->mode==7?"active":CurMode->mode==3?"inactive":"unavailable",CurMode->mode);
			else if (!strcmp(lrTrim(p+1),"+"))
				{
				if (CurMode->mode!=7) INT10_SetVideoMode(7);
				Display(CurMode->mode==7?"MONO mode status => active (video mode 7)\n":"Failed to change MONO mode\n");
				}
			else if (!strcmp(lrTrim(p+1),"-"))
				{
				if (CurMode->mode!=3) INT10_SetVideoMode(3);
				Display(CurMode->mode==3?"MONO mode status => inactive (video mode 3)\n":"Failed to change MONO mode\n");
				}
			else
				Display("Must be + or - for MONO: %s\n",lrTrim(p+1));
			}
		else if (!strcmp(args,"0")||!strcmp(args,"00")||!strcmp(args,"+0")||!strcmp(args,"-0")||i>0&&i<16)
			{
			if (p==NULL) Display("Color %d: (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue);
			}
		else
			{
			Display("Invalid color number - %s\n", lTrim(args));
			DOS_SetError(DOSERR_DATA_INVALID);
			return;
			}
		if (p!=NULL&&stricmp(args,"MONO"))
			{
			char value[128];
			if (strcmp(lrTrim(p+1),"-"))
				{
				strncpy(value,lrTrim(p+1),127);
				value[127]=0;
				}
			else
				strcpy(value,i==0?"#000000":i==1?"#0000aa":i==2?"#00aa00":i==3?"#00aaaa":i==4?"#aa0000":i==5?"#aa00aa":i==6?"#aa5500":i==7?"#aaaaaa":i==8?"#555555":i==9?"#5555ff":i==10?"#55ff55":i==11?"#55ffff":i==12?"#ff5555":i==13?"#ff55ff":i==14?"#ffff55":"#ffffff");
			if (setColors(value,i))
				{
				std::map<Bit8u,int> imap;
				for (Bit8u j = 0; j < 0x10; j++)
					{
					IO_ReadB(Mem_aLodsw(BIOS_VIDEO_PORT)+6);
					IO_WriteB(VGAREG_ACTL_ADDRESS, i+32);
					imap[j]=IO_ReadB(VGAREG_ACTL_READ_DATA);
					}
				IO_WriteB(VGAREG_DAC_WRITE_ADDRESS, imap[i]);
				IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].rgbRed*63/255);
				IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].rgbGreen*63/255);
				IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].rgbBlue*63/255);
				Display("Color %d => (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue);
				resetWin();
				}
			else
				Display("Invalid color value - %s\n",value);
			}
		}
	else
		{
		Display("MONO mode status: %s (video mode %d)\n",CurMode->mode==7?"active":CurMode->mode==3?"inactive":"unavailable",CurMode->mode);
		for (int i = 0; i < 16; i++)
			Display("Color %d: (%d,%d,%d) or #%02x%02x%02x\n",i,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue,altBGR1[i].rgbRed,altBGR1[i].rgbGreen,altBGR1[i].rgbBlue);
		}
	}

#include "parport.h"
#include "serialport.h"
extern CParallel* parallelPorts[3];
extern CSerial* serialPorts[4];

static void CLI_SETPORT(char * args)
	{
	DOS_SetError(DOSERR_NONE);
	HELP("SETPORT [port[=value]]\n\n  SETPORT LPT4=CLIP\n\nUse the LPT4 port to exchange data with the Windows clipboard\n\n  SETPORT COM5=\n\nUse the default setting for the COM5 port\n\nNote: If you do not specify any parameters for this command, then it will list the current settings; if only a port is given, then the current setting for this port will be shown.\n");
	device_PRT* dosdevice;
	Bit8u devnum;
	if (*args)
		{
		args=lrTrim(args);
		char *p, value[257];
		if (*args&&(p=strchr(args, '='))!=NULL)
			{
			*p=0;
			if (strlen(lrTrim(p+1))<257)
				strcpy(value, lrTrim(p+1));
			else
				{
				strncpy(value, lrTrim(p+1), 256);
				value[256]=0;
				}
			}
		else
			strcpy(value,"");
		args=rTrim(args);
		if (strlen(args)==4&&(!strnicmp(args,"LPT",3)||!strnicmp(args,"COM",3))&&args[3]>'0'&&args[3]<='9')
			{
			for (int i=0;i<3;i++)
				args[i]=toupper(args[i]);
			devnum = DOS_FindDevice(args);
			if (devnum != DOS_DEVICES)
				{
				dosdevice = (device_PRT *)(Devices[devnum]);
				if (p==NULL)
					Display("%s\n", strlen(dosdevice->value)?dosdevice->value:"<default setting>");
				else
					{
					if (spoolPRT==devnum)
						{
						spoolPRT=-1;
						SetWindowText(vDosHwnd, vDosCaption);
						}
					if (strcmp(value,dosdevice->value))
						{
						dosdevice->Close();
						delete Devices[devnum];
						Devices[devnum]=NULL;
						dosdevice = new device_PRT(args, value);
						DOS_AddDevice(dosdevice);
						int n=args[3]-49;
						if (!strnicmp(args,"LPT",3) && n < 3)
							parallelPorts[n] = new CParallel(n, dosdevice);
						else if (!strnicmp(args,"COM",3) && n < 4)
							serialPorts[n] = new CSerial(n, dosdevice);
						}
					}
				}
			else
				Display("<none>\n");
			}
		else
			{
			Display("Not a COM or LPT port - %s\n", args);
			DOS_SetError(DOSERR_DATA_INVALID);
			}
		return;
		}
	char pname[] = "COMx";
	for (Bitu i = 0; i < 9; i++)
		{
		pname[3] = '1' + i;
		devnum = DOS_FindDevice(pname);
		if (devnum != DOS_DEVICES)
			{
			dosdevice = (device_PRT *)(Devices[devnum]);
			Display("%s = %s\n", pname, strlen(dosdevice->value)?dosdevice->value:"<default setting>");
			}
		else
			Display("<none>\n");
		}
	strcpy(pname, "LPTx");
	for (Bitu i = 0; i < 9; i++)
		{
		pname[3] = '1' + i;
		devnum = DOS_FindDevice(pname);
		if (devnum != DOS_DEVICES)
			{
			dosdevice = (device_PRT *)(Devices[devnum]);
			Display("%s = %s\n", pname, strlen(dosdevice->value)?dosdevice->value:"<default setting>");
			}
		else
			Display("<none>\n");
		}
	}

static void CLI_MEM(char * args)
	{
	HELP("Displays free memory.\n\nA more detailed report is provided by the MEMORY command.\n");
	if (*args)
		{
		Display("Invalid parameter - %s\n", args);
		return;
		}
	Display("\n  Free memory:\n");
	Bit16u segment;																	// Show free conventional memory
	Bit16u total = 0xffff;
	DOS_AllocateMemory(&segment, &total);
	Display("%5dK Conventional\n", (total*16+277376)/1024);							// +277,376 bytes to account for 4DOS (is this always correct?)

	Bit16u largest, count;															// Show free upper memory
	if (DOS_GetFreeUMB(&total, &largest, &count))
		if (count == 1)
			Display("%5dK Upper\n", (total*16+112)/1024);							// Round numbers
		else
			Display("%5dK Upper in %d blocks, largest: %dK\n", (total*16+112)/1024, count, (largest*16+112)/1024);
	if (TotEXTMB != 0)																// Show free extended (actually total, don't like to go into the trouble of getting free)	
		Display("%5dK Extended\n", TotEXTMB*1024);
	Bit32u large32;
	if (TotXMSMB != 0 && !XMS_QueryFreeMemory(large32))								// Show free XMS
		Display("%5dK XMS\n", large32);
	largest = EMS_FreeKBs();														// Show free EMS
	if (largest)
		Display("%5dK EMS\n", largest);
	}

static void CLI_CMD(char *args)
	{
	HELP("CMD [WAIT][HIDE] windows command line\n\n  CMD /C DIR\n\nExecute the Windows DIR command\n\n  CMD WAITHIDE /C notepad\n\nStart Notepad without showing the console window and wait for it to exit\n");
	char winProg[256];
	char winCurDir[512];
	args = lrTrim(args);
	bool wait=!strcmp(args,"WAIT")||!strncmp(args,"WAIT ",5)||!strcmp(args,"WAITHIDE")||!strncmp(args,"WAITHIDE ",9);
	bool hide=!strcmp(args,"HIDE")||!strncmp(args,"HIDE ",5)||!strcmp(args,"WAITHIDE")||!strncmp(args,"WAITHIDE ",9);
	args = lTrim(args+(wait&&hide?8:(wait&&!hide||!wait&&hide?4:0)));

	ExpandEnvironmentStrings("%ComSPec%", winProg, sizeof(winProg)-1);

	strcpy(winCurDir, Drives[DOS_GetDefaultDrive()]->basedir);						// Windows directory of DOS drive
	strcat(winCurDir, Drives[DOS_GetDefaultDrive()]->curdir);						// Append DOS current directory

	STARTUPINFO si = {0};
	PROCESS_INFORMATION pi = {0};
	si.cb = sizeof(si);
	if (hide)
		{
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		}
	bkey = false;
	if (autoHide) while (ShowCursor(true)<=0);
	if (topwin)
		{
		GetWindowRect(vDosHwnd, &rect);
		SetWindowPos(vDosHwnd, HWND_NOTOPMOST, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE);
		}
	if (CreateProcess(winProg, args, NULL, NULL, FALSE, 0, NULL, winCurDir, &si, &pi) && wait)	// Start it
		{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		}
	DOS_SetError((Bit16u)GetLastError());
	}

static void CLI_ABOUT(char *args)
	{
	HELP("Displays a popup with information about vDosPlus.\n");
	showAboutMsg();
	}

static struct {
	const char * name;																// Command name
	void (*handler)(char * args);													// Handler for this command
} vDos_Cmds[]= {
	"SET",		CLI_SET,
	"SETCFG",   CLI_SETCFG,
	"SETCOLOR", CLI_SETCOLOR,
	"SETPORT",	CLI_SETPORT,
	"MEM",		CLI_MEM,
	"USE",		CLI_USE,
	"UNUSE",	CLI_UNUSE,
	"LABEL",	CLI_LABEL,
	"CHCP",		CLI_CHCP,
	"VER",		CLI_VER,
	"CMD",		CLI_CMD,
	"ABOUT",	CLI_ABOUT
}; 

void INT2F_Cont()
	{
	if (reg_dx == 0xffff)															// First magic value
		{
		if (reg_ax == 0xae00 && (reg_ch == 0 || reg_ch == 0xff))					// DOS 3.3+ internal - INSTALLABLE COMMAND - INSTALLATION CHECK
			{
			char command[128], cmdName[128];
			LinPt cmdBuffAddr = SegPhys(ds)+reg_bx;
			comcmd = reg_ch == 0;
			if (comcmd || Mem_Lodsb(cmdBuffAddr) == 124 || Mem_Lodsb(cmdBuffAddr) == 255)	// First byte = 124 (max length)!
				{
				Mem_StrnCopyFrom(command, cmdBuffAddr+(comcmd&&Mem_Lodsb(cmdBuffAddr)!=128?0:2), Mem_Lodsb(cmdBuffAddr+(comcmd&&Mem_Lodsb(cmdBuffAddr)!=128?-2:1)));
				if (comcmd) strcpy(command,lTrim(command));
				cmdBuffAddr = SegPhys(ds)+reg_si;
				Mem_StrnCopyFrom(cmdName, cmdBuffAddr+1, Mem_Lodsb(cmdBuffAddr));
				for (int i = 0; i < sizeof(vDos_Cmds)/sizeof(vDos_Cmds[0]); i++)
					if (!stricmp(cmdName, vDos_Cmds[i].name) && !(strlen(command)>strlen(cmdName) && command[strlen(cmdName)]=='.') && !(comcmd && !stricmp(cmdName, "VER")))
						reg_al = 0xff;												// Executed by us!
				}
			if (!reg_al) comcmd=false;
			return;
			}
		if (reg_ax == 0xae01 && (reg_ch == 0 || reg_ch == 0xff))					// DOS 3.3+ internal - INSTALLABLE COMMAND - EXECUTE
			{
			char command[128], cmdName[128];
			LinPt cmdBuffAddr = SegPhys(ds)+reg_bx;
			if (reg_ch) comcmd = false;
			if (comcmd || Mem_Lodsb(cmdBuffAddr) == 124 || Mem_Lodsb(cmdBuffAddr) == 255)	// First byte = 124 (max length)!
				{
				Mem_StrnCopyFrom(command, cmdBuffAddr+(comcmd&&Mem_Lodsb(cmdBuffAddr)!=128?0:2), Mem_Lodsb(cmdBuffAddr+(comcmd&&Mem_Lodsb(cmdBuffAddr)!=128?-2:1)));
				if (comcmd) strcpy(command,lTrim(command));
				cmdBuffAddr = SegPhys(ds)+reg_si;
				Mem_StrnCopyFrom(cmdName, cmdBuffAddr+1, Mem_Lodsb(cmdBuffAddr));
				if (!stricmp(cmdName, "SET"))										// This one needs special handling
					{
					CLI_SET(command);
					if (dos.errorcode)
						Mem_Stosb(cmdBuffAddr, 0);
					return;
					}
				for (int i = 1; i < sizeof(vDos_Cmds)/sizeof(vDos_Cmds[0]); i++)
					if (!stricmp(cmdName, vDos_Cmds[i].name) && !((reg_ch || comcmd) && !stricmp(cmdName, "VER")))
						{
						(*(vDos_Cmds[i].handler))(command+Mem_Lodsb(cmdBuffAddr));
						Mem_Stosb(cmdBuffAddr, 0);									// Set length command name to 0 to signal command is executed
						}
				return;
				}
			}
		}
	vpLog("Int 2F unhandled call %4X", reg_ax);
	}

static Bitu INT2A_Handler(void)
	{
	return CBRET_NONE;
	}

void DOS_SetupMisc(void)
	{
	CALLBACK_Install(0x2f, &INT2F_Handler, CB_IRET_STI);							// DOS Int 2f - Multiplex
	CALLBACK_Install(0x2a, &INT2A_Handler, CB_IRET);								// DOS Int 2a - Network
	}
