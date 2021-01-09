// Wengier: LFN, KEYBOARD and other support
#pragma once

#ifndef VDOS_H
#define VDOS_H

#include "config.h"
//#include <io.h>
//#include <windows.h>
#include "logging.h"

void E_Exit(const char * message,...);

extern char vDosVersion[];
extern char vDosBuild[];

extern bool do_debug;
extern bool bkey;
extern bool fflfn;
extern bool DOS_BreakFlag;

struct DOS_DateX {
	Bit16u year;
	Bit8u month;
	Bit8u day;
	Bit8u dayofweek;
	Bit8u hour;
	Bit8u minute;
	Bit8u second;
	Bit16u millisecond;
};

typedef struct {
	int	x, y;
} xyp;

void RunPC();
bool ConfGetBool(const char *name);
int ConfGetInt(const char *name);
char * ConfGetString(const char *name);
void ConfAddError(const char* desc, char* errLine);
void vDos_Init(void);

void INT2F_Cont(void);
void showAboutMsg(void);

extern HWND	vDosHwnd;

#define LENNAME 10																	// Max length of name
#define MAXNAMES 80																	// Max number of names
#define MAX_PATH_LEN 512															// Maximum filename size

#define txtMaxCols	240
#define txtMaxLins	60

#define DOS_FILES 255

extern char cfgname[MAXNAMES][LENNAME];
extern char cfgval[MAXNAMES][255];
extern char configfile[];
extern char autoexec[];
extern char tempdir[];
extern char vDosCaption[];

extern bool curauto;
extern bool winHidden;
extern bool autoHide,mouseHidden;
extern bool usesMouse;
extern bool uselfn;
extern bool speaker;
extern bool shortcut;
extern bool topwin;
extern bool showital;
extern bool showsout;
extern bool showsubp;
extern bool blinkCursor;
extern bool synctime;
extern bool winrun;
extern bool winKey;
extern int screen;
extern int keymode,keydelay,keyinter;
extern int wpVersion;
extern int codepage;
extern bool printTimeout;

extern Bit8u initialvMode;

#define idleTrigger 5																// When to sleep
extern int idleCount;
extern bool idleSkip;																// Don't sleep!

extern Bit32s CPU_Cycles;
#define CPU_CyclesLimit 32768
#define PIT_TICK_RATE 1193182


extern Bitu lastOpcode;

extern Bit8u ISR;																	// Interrupt Service Routine
extern bool keyb_req;

extern bool hPadding;																// Add extra pixels to the left and right?
extern Bitu hPadPixs;																// Horizontal padding pixels at the left=char width/2 rounded up (right remainder of char width)

extern bool WinProgNowait;															// Should Windows program return immediately (not called from command.com)

extern int spoolPRT;																// Printerport being spooled
#endif
