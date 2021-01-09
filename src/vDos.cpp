// Wengier: LFN, KEYBOARD and other support
#include "stdafx.h"
 
//#include <stdlib.h>
#include "vdos.h"
#include "video.h"
#include "cpu.h"
#include "callback.h"
#include "parport.h"
#include "support.h"
#include "bios.h"

#include "mouse.h"
#include "vga.h"
#include "paging.h"

char vDosVersion[] = "2015.11.01";
char vDosBuild[] = "2018.02.16";

// The whole load of startups for all the subfunctions
void GUI_StartUp();
void MEM_Init();
//void PAGING_Init();
void IO_Init();
void CALLBACK_Init();
void VGA_Init();
void DOS_Init();
void CPU_Init();
void KEYBOARD_Init();
void MOUSE_Init();
void SERIAL_Init(); 
void PIC_Init();
void TIMER_Init();
void BIOS_Init();
// void CMOS_Init();
void PARALLEL_Init();
// Dos Internal mostly
void XMS_Init();
void EMS_Init();
void SHELL_Init();
void INT10_Init();

static Bit32u mSecsLast = GetTickCount();
char vDosCaption[256] = "vDosPlus";
bool winHidden = true;
bool hPadding = true;																// Add extra pixels to the left and right?
Bitu hPadPixs = 0;																	// Horizontal padding pixels at the left=char width/2 rounded up (right remainder of char width)
bool autoHide, mouseHidden = false;
bool usesMouse;
bool uselfn;
bool filter83;
bool speaker;
bool shortcut;
bool topwin;
bool showital;
bool showsout;
bool showsubp;
bool blinkCursor;
bool synctime;
bool winrun;
bool winKey;																		// Ctrl+Key accepted as Win+Ctrl+Key
bool spaceLow;
int screen;
int keymode,keydelay,keyinter;
int wpVersion;																		// 1 - 99 (mostly 51..62, negative value will exclude some WP additions)
Bit8u initialvMode = 3;																// Initial videomode, 3 = color, 7 = Hercules for as far it works
int codepage = 0;																	// Current code page, defaults to Windows OEM
int spoolPRT = -1;																	// Printerport being spooled
bool printTimeout;																	// Should the spoolfile timeout?
bool WinProgNowait = true;
bool idleSkip = false;																// Don't try to sleep!
bool bkey = false;
bool fflfn = false;
bool DOS_BreakFlag = false;
bool curauto = true;
char autoexec[255],tempdir[256];

Bitu lastOpcode;

Bit32s CPU_Cycles =	0;

static Bit32u prevWinRefresh;
Bit8u ISR;
bool keyb_req = false;
static bool kbPending = false;
static bool intPending = false;

static Bit32u int8, int1c, int74;

DOS_DateX datex;

void RunPC(void)
	{
	while (1)
		{
		while (CPU_Cycles > 0)
			{
			if (BIOS_HostTimeSync(synctime) && !ISR)
			{
				intPending = true;														kbPending = false;
			} else if (keymode && !intPending && keyb_req && !ISR)
				kbPending=true;
				// New timer tick
			if (GETFLAG(IF))														// (hardware) Interrupts handled
//			if (GETFLAG(IF) && !cpu.pmode && !PAGING_Enabled())						// (hardware) Interrupts handled (extended check)
				if (intPending)
					{
					intPending = false;
					if ((Mem_aLodsb(4*8+3)|Mem_aLodsb(4*0x1c+3)) != 0xf0)			// And Int 8 or 1C replaced
						{
						ISR = 1;
						CPU_HW_Interrupt(8);										// Setup executing Int 8 (IRQ0)
						}
					}
				else if (kbPending && Mem_Lodsb(4 * 9 + 3) != 0xf0)
					{
						
						kbPending = false;
						keyb_req = false;
						ISR = 2;
						CPU_HW_Interrupt(9);
					}
				else if (mouse_event_type)
					CPU_HW_Interrupt(0x74);											// Setup executing Int 74 (Mouse)
			Bits ret = (*cpudecoder)();
			if (ret < 0)
				return;
			if (ret > 0 && (*CallBack_Handlers[ret])())
				return;
			}
		HandleUserActions();
		Bit32u mSecsNew = GetTickCount();
		if (!synctime)
			{
			datex.millisecond+=(Bit16u)(mSecsNew-mSecsLast);
			datex.second+=datex.millisecond/1000;
			datex.millisecond%=1000;
			datex.minute+=datex.second/60;
			datex.second%=60;
			datex.hour+=datex.minute/60;
			datex.minute%=60;
			datex.day+=datex.hour/24;
			datex.hour%=24;
			int m=datex.month,y=datex.year;
			if (datex.month==4||datex.month==6||datex.month==9||datex.month==11)
				datex.month+=datex.day/31;
			else if (datex.month==2)
				datex.month+=datex.day/(datex.year%4==0&&!(datex.year%100==0&&datex.year%400>0)?30:29);
			else
				datex.month+=datex.day/32;
			if (datex.month>m) datex.day=1;
			datex.year+=datex.month/13;
			if (datex.year>y) datex.month=1;
			dos.date.day = datex.day;
			dos.date.month = datex.month;
			dos.date.year = datex.year;
			}
		if (mSecsNew >= prevWinRefresh+40)											// 25 refreshes per second
			{
			prevWinRefresh = mSecsNew;
			VGA_VerticalTimer();
			}
		if (mSecsNew <= mSecsLast+55)												// To be real save???
			{
			LPT_CheckTimeOuts(mSecsNew);
			if (!idleSkip)
				Sleep(idleCount >= idleTrigger ? 2: 0);								// If idleTrigger or more repeated idle keyboard requests or int 28h called, sleep fixed (CPU usage drops down)
			}
		mSecsLast = mSecsNew;
		idleCount = 0;
		idleSkip = false;
		CPU_Cycles = CPU_CyclesLimit;
		}
	}

#define MAXSTRLEN 8192																// Max storage of all config strings

enum Vtype { V_BOOL, V_INT, V_STRING};

static int confEntries = 0;															// Entries so far
static char confStrings[MAXSTRLEN];
static unsigned int confStrOffset = 0;												// Offset to store new strings
static char errorMess[600];

static struct {
    char name[LENNAME + 1];
    Vtype type;
    bool set;
    union {
        bool _bool;
        int _int;
        char *_string;
    } value;
} ConfSetting[MAXNAMES];

char cfgname[MAXNAMES][LENNAME],cfgval[MAXNAMES][255],configfile[255]="CONFIG.TXT";

// No checking on add/get functions, we call them ourselves...
void ConfAddBool(const char *name, bool value) {
    strcpy(ConfSetting[confEntries].name, name);
    ConfSetting[confEntries].type = V_BOOL;
    ConfSetting[confEntries].value._bool = value;
    confEntries++;
}

void ConfAddInt(const char *name, int value) {
    strcpy(ConfSetting[confEntries].name, name);
    ConfSetting[confEntries].type = V_INT;
    ConfSetting[confEntries].value._int = value;
    confEntries++;
}

void ConfAddString(const char *name, char *value) {
    strcpy(ConfSetting[confEntries].name, name);
    ConfSetting[confEntries].type = V_STRING;
    ConfSetting[confEntries].value._string = value;
    confEntries++;
}

static int findEntry(const char *name)
	{
	for (int found = 0; found < confEntries; found++)
		if (!stricmp(name, ConfSetting[found].name))
			return found;
	return -1;
	}

bool ConfGetBool(const char *name) {
    int entry = findEntry(name);
    if (entry >= 0)
        return ConfSetting[entry].value._bool;
    return false;                                                                    // To satisfy compiler
}

int ConfGetInt(const char *name) {
    int entry = findEntry(name);
    if (entry >= 0)
        return ConfSetting[entry].value._int;
    return 0;                                                                        // To satisfy compiler
}

char *ConfGetString(const char *name) {
    int entry = findEntry(name);
    if (entry >= 0)
        return ConfSetting[entry].value._string;
    return "";                                                                        // To satisfy compiler
}
	
static char* ConfSetValue(const char* name, char* value)
	{
	int entry = findEntry(name);
	if (entry == -1)
		return "No valid option\n";
	if (ConfSetting[entry].set)
		return "Option already set\n";
	ConfSetting[entry].set = true;
	if (!strlen(value) && !(strlen(name) == 4 && (!strnicmp(name, "LPT", 3) || !strnicmp(name, "COM", 3)) && (name[3] > '0' && name[3] <= '9')))
		return NULL;
	switch (ConfSetting[entry].type)
		{
	case V_BOOL:
		if (!stricmp(value, "on"))
			{
			ConfSetting[entry].value._bool = true;
			return NULL;
			}
		else if (!stricmp(value, "off"))
			{
			ConfSetting[entry].value._bool = false;
			return NULL;
			}
		break;
	case V_INT:
		{
		int testVal = atoi(value);
		char testStr[32];
		sprintf(testStr, "%d", testVal);
		if (!strcmp(value, testStr))
			{
			ConfSetting[entry].value._int = testVal;
			return NULL;
			}
		break;
		}
	case V_STRING:
		if (strlen(value) >= MAXSTRLEN-confStrOffset)
			return "vDosPlus ran out of space to store settings\n";
		strcpy(confStrings+confStrOffset, value);
		ConfSetting[entry].value._string = confStrings+confStrOffset;
		confStrOffset += strlen(value)+1;
		return NULL;
		}
	return "Invalid value for this option\n";
	}

static char * ParseConfigLine(char *line)
	{
	char *val = strchr(line, '=');
	if (!val)
		return "= assignment missing\n";
	*val = 0;
	char *name = lrTrim(line);
	if (!strlen(name))
		return "Option name missing\n";
	for (int i=0;i<MAXNAMES;i++)
		if (!stricmp(name,cfgname[i])&&strcmp(";\n",cfgval[i]))
			return NULL;
	val = lrTrim(val+1);
	//if (!strlen(val))
		//return "Option value missing\n";
	if (strlen(name) == 4 && (!strnicmp(name, "LPT", 3) || !strnicmp(name, "COM", 3)) && (name[3] > '0' && name[3] <= '9'))
		{
		int entry = findEntry(name);
		if (entry == -1)
			{
			ConfAddString(name, val);
			ConfSetValue(name, val);													// Have to use this, ConfAddString() uses static ref to val!
			return NULL;
			}
		else if (ConfSetting[entry].set)
			return "Option already set\n";
		}
	return ConfSetValue(name, val);
	}

void ConfAddError(const char* desc, char* errLine)
	{
	static bool addMess = true;
	if (addMess)
		{
		if (strlen(errLine) > 40)
			strcpy(errLine+37, "...");
		strcat(strcat(strcat(errorMess, "\n"), desc), errLine);
		if (!stricmp(desc,"No valid option\n"))
			{
			if (!stricmp(errLine,"AUTOHIDE"))
				strcat(errorMess, " (which is now AUHIDEMS)");
			else if (!stricmp(errLine,"TIMESYNC"))
				strcat(errorMess, " (which is now SYNCTIME)");
			}
		if (strlen(errorMess) > 500)												// Don't flood the MesageBox with error lines
			{
			strcat(errorMess, "\n...");
			addMess = false;
			}
		}
	}

void ParseConfigFile()
	{
	char * parseRes;
	char lineIn[1024];
	FILE * cFile;
	errorMess[0] = 0;
	if (!(cFile = fopen(configfile, "r")))
		{
		char exePath[260];
		GetModuleFileName(NULL, exePath, sizeof(exePath)-1);						// Then from where vDosPlus was started
		strcpy(strrchr(exePath, '\\')+1, configfile);
		if (!(cFile = fopen(exePath, "r")))
			return;
		}
	bool rem = false;
	while (fgets(lineIn, 1023, cFile))
		{
		char *line = lrTrim(lineIn);
		if (!rem && strlen(line) && !strnicmp(line, "rem", 3) && (line[3] == 32 || line[3] == 9) && !strnicmp(lTrim(line+4), "vdosplus:", 9))
			line = lTrim(lTrim(line+4)+9);
		if ((strlen(line)>3 && !strncmp(line, "#[", 2) || rem && strlen(line)>1) && line[strlen(line)-2]==']' && line[strlen(line)-1]=='#')
			rem = false;
		else if (strlen(line)>1 && !strncmp(line, "#[", 2))
			rem = true;
		else if (!rem && strlen(line) && line[0] != '#' && !(!strnicmp(line, "rem", 3) && (line[3] == 0 || line[3] == 32 || line[3] == 9)))	// Filter out rem ...
			if (parseRes = ParseConfigLine(line))
				ConfAddError(parseRes, line);
		}
	fclose(cFile);
	}

void showAboutMsg()
	{
	if (autoHide) while (ShowCursor(true)<=0);
	int aboutWidth=400, aboutHeight=250;
	Bit8u infoSize[11] = { 0x16, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x14, 0x10, 0x10, 0x10 };	// Pointsize of text  
	Bit8u infoAdv[11] = { 0x10, 0x1c, 0x14, 0x14, 0x12, 0x12, 0x12, 0x24, 0x18, 0x12, 0x12 };	// Advance verticalpoints
	char *infoText[11] =
	{
		"",
		"Copyright (C) 2014-2017, Wengier. All rights reserved.",
		"vDosPlus Home page: http://www.vdosplus.org/",
		" ",
		"Derived from vDos (www.vdos.info) by Jos Schaars",
		"Includes code from the projects: 4DOS (www.4dos.info),",
		"DOSBox (www.dosbox.com), FreeType (www.freetype.org)",
		"This version of vDosPlus is 100% free to use",
		"Support page made by Edward Mendelson:",
		"http://wpdos.org/dosbox-vdos-lfn.html",
		""
	};

	char version[] = "vDosPlus yyyy.mm.dd (build yyyy.mm.dd)"; 
	infoText[0] = version;
	strncpy(infoText[0] + 9, vDosVersion, 10);
	strncpy(infoText[0] + 27, vDosBuild, 10);

	HDC hDC = GetDC(vDosHwnd);
	RECT rect;
	GetClientRect(vDosHwnd, &rect);
	rect.top = (rect.bottom - aboutHeight) / 2;
	rect.bottom = (rect.bottom + aboutHeight) / 2;
	rect.left = (rect.right - aboutWidth) / 2;
	rect.right = (rect.right + aboutWidth) / 2;
	HBRUSH hBrush = CreateSolidBrush(RGB(224, 224, 224));
	SelectObject(hDC, hBrush);
	HPEN hPen = CreatePen(PS_SOLID, 4, RGB(176, 192, 192));
	SelectObject(hDC, hPen);
	Rectangle(hDC, rect.left, rect.top, rect.right, rect.bottom);
	DeleteObject(hPen);
	DeleteObject(hBrush);

	SetTextColor(hDC, RGB(0, 0, 0));
	SetBkColor(hDC, RGB(224, 224, 224));
	for (int i = 0; i < 11; i++)
		if (*infoText[i])
		{
			HFONT hFont = CreateFont(infoSize[i], 0, 0, 0, FW_NORMAL, false, false, false, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, "Arial");
			SelectObject(hDC, hFont);												// Speed is of no interest
			rect.top += infoAdv[i];
			DrawText(hDC, infoText[i], -1, &rect, DT_CENTER | DT_END_ELLIPSIS);
			DeleteObject(hFont);
		}
	ReleaseDC(vDosHwnd, hDC);

	Sleep(500);																		// Idle/wait for 0.5 second
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))								// Clear message queue
		;
	while (true)																	// Wait for key of mouse click
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
			if (msg.message != WM_MOUSEMOVE)
			{
				vUpdateWin();
				break;
			}
			else
				PeekMessage(&msg, NULL, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE);
		}
		Sleep(100);
	}
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) true;
	if (autoHide&&mouseHidden) while (ShowCursor(false)>=0);
	bkey=false;
	}

void vDos_Init(void)
	{
	window.hideTill = GetTickCount()+2500;												// Auto hidden till first keyboard check, parachute at 2.5 secs

	ConfAddString("window", "");
	ConfAddString("xmem", "");
	ConfAddString("colors", "");
	ConfAddString("title", "");
	ConfAddString("icon", "vDosPlus_ico");
	ConfAddString("autoexec", "AUTOEXEC.TXT");
	ConfAddString("dosver", "7.10");
	ConfAddString("wheelmod", "1,2");
	ConfAddString("clickmod", "1,4");
	ConfAddString("padding", "0,0");
	ConfAddString("tempdir", "");
	ConfAddBool("spacelow", false);
	ConfAddBool("low", true);
	ConfAddBool("mouse", false);
	ConfAddBool("auhidems", false);
	ConfAddBool("vgafixms", false);
	ConfAddBool("winkey", true);
	ConfAddInt("keymode", 2);
	ConfAddInt("keydelay", 0);
	ConfAddInt("keyinter", 0);
	ConfAddInt("sysicons", 2);
	ConfAddBool("synctime", true);
	ConfAddBool("lfn", true);
	ConfAddBool("filter83", false);
	ConfAddBool("blinkc", true);
	ConfAddBool("speaker", true);
	ConfAddBool("shortcut", false);
	ConfAddBool("usedrvs", false);
	ConfAddBool("topwin", false);
	ConfAddBool("winrun", true);
	ConfAddInt("transwin", 0);
	ConfAddInt("screen", 0);
	ConfAddInt("lins", 25);
	ConfAddInt("cols", 80);
	ConfAddInt("smallclr", 7);
	ConfAddBool("showital", true);
	ConfAddBool("strikout", false);
	ConfAddBool("subpscr", true);
	ConfAddBool("evensize", true);
	ConfAddBool("frame", false);
	ConfAddBool("timeout", true);
	ConfAddBool("confwarn", true);
	ConfAddString("scale", "0");
	ConfAddString("font", "");
	ConfAddString("boldfont", "");
	ConfAddString("italfont", "");
	ConfAddString("boitfont", "");
	ConfAddString("wp", "");
	ConfAddInt("euro", -1);
	char *res, msg[300];
	for (int i=0;i<MAXNAMES;i++)
		{
		if (*cfgname[i])
			{
			if (strlen(cfgname[i]) == 4 && (!strnicmp(cfgname[i], "LPT", 3) || !strnicmp(cfgname[i], "COM", 3)) && (cfgname[i][3] > '0' && cfgname[i][3] <= '9'))
				{
				int entry = findEntry(cfgname[i]);
				if (entry == -1)
					{
					ConfAddString(cfgname[i], cfgval[i]);
					ConfSetValue(cfgname[i], cfgval[i]);
					}
				else if (ConfSetting[entry].set)
					{
					sprintf(msg,"Ignoring duplicate directive - %s=%s",cfgname[i],cfgval[i]);
					MessageBox(NULL, msg, "vDosPlus: Error in command line", MB_OK|MB_ICONWARNING);
					}
				}
			else if ((res=ConfSetValue(cfgname[i],cfgval[i]))!=NULL)
				{
				sprintf(msg,strcmp(res,"Option already set\n")?"Ignoring invalid directive - %s=%s":"Ignoring duplicate directive - %s=%s",cfgname[i],cfgval[i]);
				MessageBox(NULL, msg, "vDosPlus: Error in command line", MB_OK|MB_ICONWARNING);
				strcpy(cfgval[i],";\n");
				int entry = findEntry(cfgname[i]);
				if (entry > -1 && ConfSetting[entry].set)
					ConfSetting[entry].set = false;
				}
			}
		}
	ParseConfigFile();

	GUI_StartUp();
	IO_Init();
//	PAGING_Init();
	MEM_Init();
	CALLBACK_Init();
	PIC_Init();
	TIMER_Init();
//	CMOS_Init();
	VGA_Init();
	CPU_Init();
	KEYBOARD_Init();
	BIOS_Init();
	INT10_Init();
	MOUSE_Init();
	SERIAL_Init();
	PARALLEL_Init();
	if (strlen(lTrim(ConfGetString("tempdir")))<256)
		strcpy(tempdir,lTrim(ConfGetString("tempdir")));
	else
		{
		strncpy(tempdir,lTrim(ConfGetString("tempdir")),255);
		tempdir[255]=0;
		}
	printTimeout = ConfGetBool("timeout");
	winKey = ConfGetBool("winkey");
	spaceLow = ConfGetBool("spacelow");
	filter83 = ConfGetBool("filter83");
	DOS_Init();
	XMS_Init();
	EMS_Init();
	if (ConfGetBool("confwarn") && errorMess[0])
		{
		sprintf(msg,"vDosPlus: %s has unresolved items",configfile);
		MessageBox(NULL, errorMess+1, msg, MB_OK|MB_ICONWARNING);
		}
	SHELL_Init();																	// Start up main machine
	}
