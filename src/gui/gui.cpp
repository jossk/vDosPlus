// Wengier: MULTI-MONITOR, KEYBOARD and other support
#include "stdafx.h"

//#include <stdlib.h>
//#include <stdio.h>
//#include "Windows.h"
//#include <Shellapi.h>

#include "video.h"
#include "events.h"
#include "vdos.h"
#include "mouse.h"
#include "vDosTTF.h"
#include "ttf.h"
#include "..\ints\int10.h"
#include "dos_inc.h"
//#include <time.h>
//#include <math.h>

void KEYBOARD_AddKey(Bit8u flags, Bit8u scancode, bool pressed);

#define WM_SC_SYSMENUABOUT		0x1110												// Pretty arbitrary, just needs to be < 0xF000
#define WM_SC_SYSMENUNOTES		(WM_SC_SYSMENUABOUT+1)
#define WM_SC_SYSMENUPCOPY		(WM_SC_SYSMENUABOUT+2)
#define WM_SC_SYSMENUPDUMP		(WM_SC_SYSMENUABOUT+3)
#define WM_SC_SYSMENUPASTE		(WM_SC_SYSMENUABOUT+4)
#define WM_SC_SYSMENUDECREASE	(WM_SC_SYSMENUABOUT+5)
#define WM_SC_SYSMENUINCREASE	(WM_SC_SYSMENUABOUT+6)
//#define WM_SC_SYSMENUMAX		(WM_SC_SYSMENUABOUT+7)

RGBQUAD altBGR0[16], altBGR1[16];

DWORD ttfSizeData, ttfSizeDatab, ttfSizeDatai, ttfSizeDatabi;
void * ttfFontData, * ttfFontDatab, * ttfFontDatai, * ttfFontDatabi;
static int prevPointSize = 0;
RECT prevPosition;
int winPerc = 75;
static int initialX = -1;
static int initialY = -1;
static int eurAscii = -1;															// ASCII value to use for the EUro symbol, standard none
static int clearbutt = -1;
static Bit8u bcount = 0;

static bool selectingText = false, firstmax = true;
static int selStartX, selPosX1, selPosX2, selStartY, selPosY1, selPosY2;

BOOL getr = false;
bool fontBoxed = false;																// ASCII 176-223 box/lines or extended ASCII
bool vgafixms, swapmod, evensize, mstate, chgicons = false, tobeframe = false;
int scroll, wheelmodv, wheelmodh, clickmodl, clickmodr, sstate, sysicons, pSize;
int wsVersion;																		// For now just 0 (no WordStar) or 1
int wsBackGround;																	// BackGround text color WordStar
int xyVersion;
int xyBackGround;																	// BackGround text color XyWrite
int curscr,padding,padcolor,smallclr,transwin;
Bit16u smap = NULL;
char fName[260], fbName[260], fiName[260], fbiName[260], icon[300];
extern Bit8u kbState[];
extern bool blinking, MousePosWP(int *x, int *y);

Render_ttf ttf;
Render_t render;
Bit16u curAttrChar[txtMaxLins*txtMaxCols];											// Current displayed textpage
Bit16u *newAttrChar;																// To be replaced by
RECT monrect;
char msg[255];

static RGB_Color ttf_fgColor = {0, 0, 0, 0}; 
static RGB_Color ttf_bgColor = {0, 0, 0, 0};
static RECT ttf_textRect = {0, 0, 0, 0};
static RECT ttf_textClip = {0, 0, 0, 0};

static WNDPROC fnDefaultWndProc;

static void TimedSetSize()
	{
	HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	if (vga.mode == M_TEXT && ttf.fullScrn)
		window.surface = SetVideoMode(monrect,info.rcMonitor.right-info.rcMonitor.left, info.rcMonitor.bottom-info.rcMonitor.top-(shortcut?1:0), false);
	else
		window.surface = SetVideoMode(monrect,window.width, window.height, window.framed);
	if (!window.surface)
		E_Exit("Failed to create surface for vDosPlus window");
	if (initialX >= 0)																// Position window (only once at startup)
		{
		RECT rect;
		GetWindowRect(vDosHwnd, &rect);												// Window has to be shown completely
		if (initialX+(firstmax?window.width:rect.right-rect.left) <= info.rcMonitor.right-info.rcMonitor.left && initialY+(firstmax?window.height:rect.bottom-rect.top) <= info.rcMonitor.bottom-info.rcMonitor.top)
			MoveWindow(vDosHwnd, info.rcMonitor.left+initialX, info.rcMonitor.top+initialY, rect.right-rect.left, rect.bottom-rect.top, true);
		initialX = -1;
		}
	window.active = true;
	}

void SetVideoSize()
	{
	if (vga.mode == M_TEXT)
		{
		window.width = (ttf.cols+hPadding)*ttf.charWidth+ttf.offX*2;
		window.height = ttf.lins*ttf.charHeight+ttf.offY*2;
		}
	else
		{
		window.width = render.cache.width*abs(window.scalex);
		window.height = render.cache.height*(window.scaley == 10 ? abs(window.scalex):abs(window.scaley));
		}
	if (!winHidden)
		TimedSetSize();
	}

bool StartVideoUpdate()
	{
	if (winHidden && GetTickCount() >= window.hideTill)
		{
		winHidden = false;
		TimedSetSize();
		}
	return window.active;
	}

HICON getIcon(char* icons, bool def)
	{
	if (!strlen(icons)||!strcmp(icons,"vDosPlus_ico"))
		{
		strcpy(icon,"vDosPlus_ico");
		return LoadIcon(GetModuleHandle(NULL), icon);
		}
	HICON IcoHwnd;
	char *p=strchr(icons, ','), *pe=0;
	if (p!=NULL) *p=0;
	if (strlen(rTrim(icons))&&(p==NULL||!strlen(lTrim(p+1))||!strcmp(lrTrim(p+1),"0")||*(lTrim(p+1))>'0'&&*(lTrim(p+1))<='9'&&strtol(lTrim(p+1),&pe,10)>0&&!*pe))
		IcoHwnd = ExtractIcon(NULL, rTrim(icons), p!=NULL&&strlen(lTrim(p+1))&&strcmp(lrTrim(p+1),"0")?strtol(lTrim(p+1),NULL,10):0);
	else
		{
		HINSTANCE module=strlen(icons)?LoadLibraryEx(rTrim(icons),NULL,LOAD_LIBRARY_AS_IMAGE_RESOURCE):NULL;
		IcoHwnd = LoadIcon(module, lTrim(p+1));
		if (module!=NULL) FreeLibrary(module);
		}
	if (p!=NULL) *p=',';
	//IcoHwnd = (HICON)LoadImage(NULL, ConfGetString("ICON"), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED | LR_LOADTRANSPARENT);
	if (IcoHwnd)
		{
		if (strlen(icons)>299)
			{
			strncpy(icon,icons,299);
			icon[299]=0;
			}
		else
			strcpy(icon,icons);
		}
	else if (def)
		strcpy(icon,"vDosPlus_ico");
	return IcoHwnd;
	}

int SetEuro(int n)
	{
	if (n==NULL||n==eurAscii) return eurAscii;
	if (smap != NULL && eurAscii != -1 && TTF_GlyphIsProvided(ttf.font, 0x20ac))
		cpMap[eurAscii] = smap;
	eurAscii = n;
	if (eurAscii != -1 && TTF_GlyphIsProvided(ttf.font, 0x20ac))
		{
		smap=cpMap[eurAscii];
		cpMap[eurAscii] = 0x20ac;
		}
	return eurAscii;
	}

int SetCodePage(int cp)
	{
	unsigned char cTest[256];														// ASCII format
	for (int i = 0; i < 256; i++)
		cTest[i] = i;
	Bit16u wcTest[512];
	if (MultiByteToWideChar(cp, MB_PRECOMPOSED|MB_ERR_INVALID_CHARS, (char*)cTest, 256, (LPWSTR)wcTest, 256) == 256)
		{
		TTF_Flush_Cache(ttf.font);													// Rendered glyph cache has to be cleared!
		int notMapped = 0;															// Number of characters not defined in font
		Bit16u unimap, orgMap[48] = {
			0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
			0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
			0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
		};
		for (int c = 128; c < 256; c++)
			{
			if (wcTest[c] != 0x20ac)												// To be consistent, no Euro substitution
				unimap = wcTest[c];
			if (!fontBoxed || c < 176 || c > 223)
				{
				if (!TTF_GlyphIsProvided(ttf.font, unimap))
					notMapped++;
				else
					cpMap[c] = unimap;
				}
			else
				cpMap[c] = orgMap[c-176];
			}
		if (eurAscii != -1 && TTF_GlyphIsProvided(ttf.font, 0x20ac))
			{
			smap=cpMap[eurAscii];
			cpMap[eurAscii] = 0x20ac;
			}
		return notMapped;
		}
	return -1;																		// Code page not set
	}

bool readTTF(const char *fName, bool bold, bool italics)							// Open and read alternative font
	{
	if (!strlen(fName))
		{
		if (bold && italics)
			{
			ttfFontDatabi = vDosTTFbi;
			ttfSizeDatabi = sizeof(vDosTTFbi);
			}
		else if (bold && !italics)
			{
			ttfFontDatab = vDosTTFbi;
			ttfSizeDatab = sizeof(vDosTTFbi);
			}
		else if (!bold && italics)
			{
			ttfFontDatai = vDosTTFbi;
			ttfSizeDatai = sizeof(vDosTTFbi);
			}
		else if (!bold && !italics)
			{
			ttfFontData = vDosTTFbi;
			ttfSizeData = sizeof(vDosTTFbi);
			}
		return true;
		}
	FILE * ttf_fh;
	char ttfPath[1024];

	strcpy(ttfPath, fName);															// First try to load it as-is (form current directory or full path)
	char *p = strrchr(ttfPath, '\\');												// Skip past paths
	p = strrchr(p ? p+1 : ttfPath, '.');											// Skip to last period
	if (!p)
		strcat(ttfPath, ".ttf");
	if (!(ttf_fh = fopen(ttfPath, "rb")))											// Try to load it from working directory
		{
		strcpy(strrchr(strcpy(ttfPath, _pgmptr), '\\')+1, fName);					// Try to load it from where vDosPlus was started
		if (!p)
			strcat(ttfPath, ".ttf");
		ttf_fh  = fopen(ttfPath, "rb");
		}
	if (ttf_fh)
		{
		if (!fseek(ttf_fh, 0, SEEK_END))
			{
			if (bold && italics)
				{
				if ((ttfSizeDatabi = ftell(ttf_fh)) != -1L)
					if (ttfFontDatabi = malloc((size_t)ttfSizeDatabi))
						if (!fseek(ttf_fh, 0, SEEK_SET))
							if (fread(ttfFontDatabi, 1, (size_t)ttfSizeDatabi, ttf_fh) == (size_t)ttfSizeDatabi)
								{
								fclose(ttf_fh);
								return true;
								}
				}
			else if (bold && !italics)
				{
				if ((ttfSizeDatab = ftell(ttf_fh)) != -1L)
					if (ttfFontDatab = malloc((size_t)ttfSizeDatab))
						if (!fseek(ttf_fh, 0, SEEK_SET))
							if (fread(ttfFontDatab, 1, (size_t)ttfSizeDatab, ttf_fh) == (size_t)ttfSizeDatab)
								{
								fclose(ttf_fh);
								return true;
								}
				}
			else if (!bold && italics)
				{
				if ((ttfSizeDatai = ftell(ttf_fh)) != -1L)
					if (ttfFontDatai = malloc((size_t)ttfSizeDatai))
						if (!fseek(ttf_fh, 0, SEEK_SET))
							if (fread(ttfFontDatai, 1, (size_t)ttfSizeDatai, ttf_fh) == (size_t)ttfSizeDatai)
								{
								fclose(ttf_fh);
								return true;
								}
				}
			else
				{
				if ((ttfSizeData = ftell(ttf_fh)) != -1L)
					if (ttfFontData = malloc((size_t)ttfSizeData))
						if (!fseek(ttf_fh, 0, SEEK_SET))
							if (fread(ttfFontData, 1, (size_t)ttfSizeData, ttf_fh) == (size_t)ttfSizeData)
								{
								fclose(ttf_fh);
								return true;
								}
				}
			}
		fclose(ttf_fh);
		}
	ConfAddError("Could not load TTF font file\n", (char *)fName);
	return false;
	}

void SelectFontByPoints(int ptsize)
	{
	if (ttf.font == 0)
		{
		ttf.font = TTF_New_Memory_Face((const unsigned char*)ttfFontData, ttfSizeData, ptsize, fName);
		if (ttf.font==NULL)
			{
			if (*fName)
				{
				strcpy(fName,"");
				readTTF(fName, false, false);
				ttf.font = TTF_New_Memory_Face((const unsigned char*)ttfFontData, ttfSizeData, ptsize, fName);
				}
			if (ttf.font==NULL) E_Exit("Failed to init default font");
			if ((*fbName || *fiName || *fbiName))
				{
				char cName[40];
				sprintf(cName, "%s%s%s", *fbName?"BOLDFONT\n":"", *fiName?"ITALFONT\n":"", *fbiName?"BOITFONT\n":"");
				if (cName[strlen(cName)-1]=='\n') cName[strlen(cName)-1]=0;
				ConfAddError("An valid FONT setting is needed for the following to take effect:\n", cName);
				strcpy(fbName,"");
				readTTF(fbName, true, false);
				strcpy(fiName,"");
				readTTF(fiName, false, true);
				strcpy(fbiName,"");
				readTTF(fbiName, true, true);
				}
			}
		if (ttfFontDatab != vDosTTFbi)
			{
			ttf.fontb = TTF_New_Memory_Face((const unsigned char*)ttfFontDatab, ttfSizeDatab, ptsize, fbName);
			if (ttf.fontb == NULL)
				{
				strcpy(fbName,"");
				readTTF(fbName, true, false);
				}
			}
		else
			ttf.fontb = NULL;
		if (ttfFontDatai != vDosTTFbi)
			{
			ttf.fonti = TTF_New_Memory_Face((const unsigned char*)ttfFontDatai, ttfSizeDatai, ptsize, fiName);
			if (ttf.fonti == NULL)
				{
				strcpy(fiName,"");
				readTTF(fiName, false, true);
				}
			}
		else
			ttf.fonti = NULL;
		if (ttfFontDatabi != vDosTTFbi)
			{
			ttf.fontbi = TTF_New_Memory_Face((const unsigned char*)ttfFontDatabi, ttfSizeDatabi, ptsize, fbiName);
			if (ttf.fontbi == NULL)
				{
				strcpy(fbiName,"");
				readTTF(fbiName, true, true);
				}
			}
		else
			ttf.fontbi = NULL;
		}
	else
		{
		TTF_SetCharSize(ttf.font, ptsize);
		if (ttfFontDatab != vDosTTFbi)
			TTF_SetCharSize(ttf.fontb, ptsize);
		if (ttfFontDatai != vDosTTFbi)
			TTF_SetCharSize(ttf.fonti, ptsize);
		if (ttfFontDatabi != vDosTTFbi)
			TTF_SetCharSize(ttf.fontbi, ptsize);
		}
	ttf.pointsize = ptsize;
	ttf.charWidth = TTF_FontWidth(ttf.font);
	ttf.charHeight = TTF_FontHeight(ttf.font);
	int width=ttf.charHeight>ttf.charWidth*3/2?ttf.charWidth:ttf.charWidth/2;
	int height=ttf.charWidth>ttf.charHeight*3/2?ttf.charHeight:ttf.charHeight/2;
	ttf.offX = width*((padding+width-1)/width);
	ttf.offY = height*((padding+height-1)/height);
	hPadPixs = hPadding?(ttf.charWidth+1)/2:0;
	if (ttf.fullScrn)
		{
		HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		ttf.offX = (info.rcMonitor.right-info.rcMonitor.left-ttf.charWidth*ttf.cols)/2-hPadPixs;
		ttf.offY = (info.rcMonitor.bottom-info.rcMonitor.top-(shortcut?1:0)-ttf.charHeight*ttf.lins)/2;
		}
	if (!codepage)
		{
		int cp = GetOEMCP();
		codepage = SetCodePage(cp) == -1 ? 437 : cp;
		}
	}


static RGBQUAD *rgbColors = (RGBQUAD*)render.pal;

static int prev_sline = -1;
static bool hasFocus = true;														// Only used if not framed
static bool hasMinButton = false;													// ,,
static bool blinkstate = false;

static void showNotes(void)
	{
	if ((int)ShellExecute(NULL, "open", "sysnotes.txt", NULL, NULL, SW_SHOWNORMAL)<33)	// Open Notes file
		{
		char exePath[260];
		GetModuleFileName(NULL, exePath, sizeof(exePath)-5);
		strcpy(strrchr(exePath, '\\')+1, "sysnotes.txt");
		if ((int)ShellExecute(NULL, "open", exePath, NULL, NULL, SW_SHOWNORMAL)<33)
			{
			if (autoHide) while (ShowCursor(true)<=0);
			if (topwin)
				{
				RECT rect;
				GetWindowRect(vDosHwnd, &rect);
				SetWindowPos(vDosHwnd, HWND_NOTOPMOST, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
				}
			MessageBox(NULL, "Cannot find or open the notes file named \"sysnotes.txt\"", "vDosPlus warning", MB_OK|MB_ICONWARNING);
			if (autoHide&&mouseHidden) while (ShowCursor(false)>=0);
			}
		}
	}

static void copyScreen(void)
	{
	if (vga.mode != M_TEXT || !OpenClipboard(NULL))
		return;
	if (EmptyClipboard())
		{
		HGLOBAL hCbData = GlobalAlloc(NULL, 2*(ttf.cols+2)*ttf.lins-2);
		Bit16u* pChData = (Bit16u*)GlobalLock(hCbData);
		if (pChData)
			{
			int dataIdx = 0, lastNotSpace;
			Bit16u *curAC = curAttrChar;
			for (int line = 0; line < ttf.lins; line++)
				{
				lastNotSpace = dataIdx;
				for (int col = 1; col <= ttf.cols; col++)
					{
					Bit8u textChar =  *(curAC++)&255;
					pChData[dataIdx++] = cpMap[textChar];
					if (textChar != 32)
						lastNotSpace = dataIdx;
					}
				dataIdx = lastNotSpace;
				if (line != ttf.lins)								// Append line feed for all bur the last line
					{
					pChData[dataIdx++] = 0x0d;
					pChData[dataIdx++] = 0x0a;
					}
				}
			pChData[dataIdx] = 0;
			SetClipboardData(CF_UNICODETEXT, hCbData);
			GlobalUnlock(hCbData);
			}
		}
	CloseClipboard();
	}

static void dumpScreen(void)
	{
	FILE * fp;

	if (vga.mode == M_TEXT)
		if (fp = fopen("screen.txt", "wb"))
			{
			Bit16u textLine[txtMaxCols+1];
			Bit16u *curAC = curAttrChar;
			fprintf(fp, "\xff\xfe");												// It's a Unicode text file
			for (int lineno = 0; lineno < ttf.lins; lineno++)
				{
				int chars = 0;
				for (int xPos = 0; xPos < ttf.cols; xPos++)
					{
					Bit8u textChar =  *(curAC++)&255;
					textLine[xPos] = cpMap[textChar];
					if (textChar != 32)
						chars = xPos+1;
					}
				if (chars)															// If not blank line
					fwrite(textLine, 2, chars, fp);									// Write truncated
				if (lineno < ttf.lins-1)											// Append linefeed for all but the last line
					fwrite("\x0d\x00\x0a\x00", 1, 4, fp);
				}
			fclose(fp);
			ShellExecute(NULL, "open", "screen.txt", NULL, NULL, SW_SHOWNORMAL);	// Open text file
			}
	return;
	}

void getClipboard(void)
	{
	if (OpenClipboard(NULL))
		{	
		if (HANDLE cbText = GetClipboardData(CF_UNICODETEXT))
			{
			Bit16u *p = (Bit16u *)GlobalLock(cbText);
			BIOS_PasteClipboard(p);
			GlobalUnlock(cbText);
			}
		CloseClipboard();
		}
	}
	
void processWP(int* pstyle, Bit8u* pcolorBG, Bit8u* pcolorFG)
	{
	int style = *pstyle;
	Bit8u colorBG = *pcolorBG, colorFG = *pcolorFG;
	if (CurMode->mode == 7)												// Mono (Hercules)
		{
		style = (colorFG&7) == 1 ? TTF_STYLE_UNDERLINE : TTF_STYLE_NORMAL;
		if ((colorFG&0xa) == colorFG && colorBG == 0)
			colorFG = 8;
		else if (colorFG&7)
			colorFG |= 7;
		}
	else if (wpVersion > 0)												// If WP and not negative (color value to text attribute excluded)
		{
		if (showital && colorFG == 0xe && colorBG == 1)
			{
			style = TTF_STYLE_ITALIC;
			colorFG = 7;
			}
		else if ((colorFG == 1 || colorFG == 0xf) && colorBG == 7)
			{
			style = TTF_STYLE_UNDERLINE;
			colorBG = 1;
			colorFG = colorFG == 1 ? 7 : 0xf;
			}
		else if ((showsout && colorFG == 0 || colorFG == 1) && colorBG == 3)
			{
			style = colorFG == 1 ? TTF_STYLE_DOUBLEUNDERLINE : TTF_STYLE_STRIKETHROUGH;
			colorBG = 1;
			colorFG = 7;
			}
		else if (showsubp && (wpVersion < 6 && colorFG == 4 || colorFG == 5) && colorBG == 7)
			{
			style = colorFG ==4 ? TTF_STYLE_SUBSCRIPT : TTF_STYLE_SUPERSCRIPT;
			colorBG = 1;
			colorFG = 7;						
			}
		else
			style = TTF_STYLE_NORMAL;
		}
	else if (xyVersion)													// If XyWrite
		{
		if (showital && (colorFG == 10 || colorFG == 14) && colorBG != 12)
			{
			style = TTF_STYLE_ITALIC;
			if (colorBG == 3)
				{
				style |= TTF_STYLE_UNDERLINE;
				colorBG = xyBackGround;
				}
			colorFG = colorFG == 10 ? 7:15;
			}
		else if (colorFG == 3 || colorFG == 0xb)
			{
			style = TTF_STYLE_UNDERLINE;
			colorFG = colorFG == 3 ? 7:15;
			}
		else if (showsubp && colorBG == 6 && (colorFG == 0 || colorFG == 9 || colorFG == 15))
			{
			style |= TTF_STYLE_SUBSCRIPT;
			if (colorFG == 9) style |= TTF_STYLE_UNDERLINE;
			colorBG = xyBackGround;
			if (colorFG < 15) colorFG = 7;
			}
		else if (showsubp && showital && (colorBG == 5 || colorBG == 6) && colorFG == 2)
			{
			if (colorBG == 5)
				style |= TTF_STYLE_SUPERSCRIPT;
			else
				style |= TTF_STYLE_SUBSCRIPT;
			style |= TTF_STYLE_ITALIC;
			colorBG = xyBackGround;
			colorFG = 7;
			}
		else if (showsubp && colorBG == 5 && (colorFG == 1 || colorFG == 7 || colorFG == 15))
			{
			style |= TTF_STYLE_SUPERSCRIPT;
			if (colorFG == 1)
				{
				style |= TTF_STYLE_UNDERLINE;
				colorFG = 7;
				}
			colorBG = xyBackGround;
			}
		else if (showsubp && colorBG == 14 && colorFG == 12)
			{
			style |= TTF_STYLE_SUBSCRIPT;
			style |= TTF_STYLE_BOLD;
			style |= TTF_STYLE_UNDERLINE;
			colorBG = xyBackGround;
			colorFG = 15;
			}
		else if (showsubp && showital && colorBG == 12 && (colorFG == 12 || colorFG == 13 || colorFG == 14))
			{
			style |= TTF_STYLE_SUBSCRIPT;
			style |= TTF_STYLE_ITALIC;
			if (colorFG < 14) style |= TTF_STYLE_UNDERLINE;
			colorBG = xyBackGround;
			colorFG = colorFG == 13 ? 7:15;
			}
		else if (showsubp && showital && colorBG == 13 && (colorFG == 4 || colorFG == 5 || colorFG == 6))
			{
			style |= TTF_STYLE_SUPERSCRIPT;
			style |= TTF_STYLE_ITALIC;
			if (colorFG < 6) style |= TTF_STYLE_UNDERLINE;
			colorBG = xyBackGround;
			colorFG = colorFG == 5 ? 7:15;
			}
		else if (showsubp && colorBG == 15 && colorFG == 4)
			{
			style |= TTF_STYLE_SUPERSCRIPT;
			style |= TTF_STYLE_BOLD;
			style |= TTF_STYLE_UNDERLINE;
			colorBG = xyBackGround;
			colorFG = 15;
			}
		else if (!showsout || colorBG != 4)
			style = TTF_STYLE_NORMAL;
		if (showsout && colorBG == 4)
			{
			if (colorFG == 13)
				style |= TTF_STYLE_SUPERSCRIPT;
			else if (colorFG == 12)
				style |= TTF_STYLE_SUBSCRIPT;
			style |= TTF_STYLE_STRIKETHROUGH;
			colorBG = xyBackGround;
			if (colorFG != 15) colorFG = 7;
			}
		}
	else if (wsVersion)													// If WordStar
		{
		if (showsubp && (colorBG == 5 || colorBG == 13) && (colorFG == 6 || colorFG == 14))
			{
			style |= TTF_STYLE_SUPERSCRIPT;
			if (colorBG == 13) style |= TTF_STYLE_UNDERLINE;
			colorFG++;
			}
		else if (showsubp && showital && colorBG == 15 && (colorFG == 6 || colorFG == 14))
			{
			style |= TTF_STYLE_SUPERSCRIPT;
			style |= TTF_STYLE_ITALIC;
			colorFG++;
			}
		else if (showsubp && (colorBG == 5 || colorBG == 13) && (colorFG == 4 || colorFG == 12))
			{
			style |= TTF_STYLE_SUBSCRIPT;
			if (colorBG == 13) style |= TTF_STYLE_UNDERLINE;
			colorFG+=3;
			}
		else if (showsubp && showital && colorBG == 15 && (colorFG == 4 || colorFG == 12))
			{
			style |= TTF_STYLE_SUBSCRIPT;
			style |= TTF_STYLE_ITALIC;
			colorFG+=3;
			}
		else if (colorBG&8)												// If "blinking" set, modify attributes
			{
			if (colorBG&1)
				style |= TTF_STYLE_UNDERLINE;
			if (colorBG&2 && showital)
				style |= TTF_STYLE_ITALIC;
			if (colorBG&4 && showsout)
				style |= TTF_STYLE_STRIKETHROUGH;
			}
		if (style)
			colorBG = wsBackGround;										// Background color (text) is fixed at this
		}
	if (xyVersion||wsVersion||wpVersion>0)
		{
		if (colorFG == colorBG && colorBG == (xyVersion?xyBackGround:(wsVersion?wsBackGround:1)))
			{
			style = TTF_STYLE_SMALL;
			colorFG = smallclr;
			}
		else if (colorFG == 15)
			{
			if (ttf.fontbi||!(style&TTF_STYLE_ITALIC)) style |= TTF_STYLE_BOLD;
			if (ttf.fontbi&&style&TTF_STYLE_ITALIC||ttf.fontb&&!(style&TTF_STYLE_ITALIC)) colorFG = 7;
			}
		}
	*pstyle = style;
	*pcolorBG = colorBG;
	*pcolorFG = colorFG;
	}

// Fill a rectangle in video buffer with a color
void vBlitFill(Bit32u color, RECT *rect)
	{
	Bit32u *dstpix = (Bit32u *)((Bit8u *)window.surface->pixels+rect->top*window.surface->w*4+rect->left*4);
	int dstskip = window.surface->w-(rect->right-rect->left);

	for (int h = rect->bottom-rect->top; h; h--)
		{
		for (int w = rect->right-rect->left; w; --w)
			*dstpix++ =  color;
		dstpix += dstskip;
		}
	}

void EndTextLines(void)
	{
	if (selectingText)																// The easy way: don't update when selecting text
		return;

	if (autoHide&&mouseHidden)
		{
		mouseHidden=false;
		while (ShowCursor(true)<=0);
		}
	char asciiStr[txtMaxCols*2+1];													// Max+1 charaters in a line
	int xmin = ttf.cols;															// Keep track of changed area
	int ymin = ttf.lins;
	int xmax = -1;
	int ymax = -1;
	int style = 0;
	Bit16u *curAC = curAttrChar;													// Old/current buffer
	Bit16u *newAC = newAttrChar;													// New/changed buffer

	if ((GetFocus() == vDosHwnd) != hasFocus)
		{
		hasFocus = !hasFocus;
		bkey = false;
		memset(curAC, -1, ttf.cols*ttf.lins*2);										// Force redraw of all lines
		}
	if (ttf.cursor >= 0 && ttf.cursor < ttf.cols*ttf.lins)							// Hide/restore (previous) cursor-character if we had one
//		if (ttf.cursor != vga.draw.cursor.address>>1 || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15)
		if (ttf.cursor != vga.draw.cursor.address>>1)
			curAC[ttf.cursor] = newAC[ttf.cursor]^0xf0f0;							// Force redraw (differs)
	if (chgicons)
		{
		chgicons=false;
		memset(curAC, -1, ttf.cols*2);
		}
	if (ttf.offX || ttf.offY)
		{
		Bit8u color = padcolor;
		ttf_fgColor.b = padcolor==-1?0:rgbColors[color].rgbBlue;
		ttf_fgColor.g = padcolor==-1?0:rgbColors[color].rgbGreen;
		ttf_fgColor.r = padcolor==-1?0:rgbColors[color].rgbRed;
		ttf_bgColor.b = padcolor==-1?0:rgbColors[color].rgbBlue;
		ttf_bgColor.g = padcolor==-1?0:rgbColors[color].rgbGreen;
		ttf_bgColor.r = padcolor==-1?0:rgbColors[color].rgbRed;
		ttf_textClip.top = 0;
		ttf_textClip.bottom = ttf.charHeight;
		ttf_textClip.left = 0;
		ttf_textClip.right = window.width-2*ttf.offX;
		ttf_textRect.top = 0;
		ttf_textRect.left = ttf.offX;
		int adval=ttf.offY%ttf.charHeight>0?1:0;
		int ncols=(window.width-2*ttf.offX)/ttf.charWidth+(ttf.offX%ttf.charWidth>0?1:0);
		for (int i=0;i<ncols;i++)
			asciiStr[i] = 32;
		vSurface* textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, ncols, ttf_fgColor, ttf_bgColor, 0);
		for (int i=0;i<ttf.offY/ttf.charHeight+adval;i++)
			{
			ttf_textRect.top = i*ttf.charHeight;
			if (i==ttf.offY/ttf.charHeight)
				ttf_textClip.bottom = ttf.offY%ttf.charHeight;				
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		if (adval)
			{
			ttf_textClip.bottom = ((window.height-ttf.offY)/ttf.charHeight<window.height/ttf.charHeight?ttf.charHeight:window.height%ttf.charHeight)-ttf.offY%ttf.charHeight;
			ttf_textRect.top = ((window.height-ttf.offY)/ttf.charHeight+adval-1)*ttf.charHeight+ttf.offY%ttf.charHeight;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		ttf_textClip.bottom = ttf.charHeight;
		for (int i=(window.height-ttf.offY)/ttf.charHeight+adval;i<window.height/ttf.charHeight+adval;i++)
			{
			if (i==window.height/ttf.charHeight)
				ttf_textClip.bottom = window.height%ttf.charHeight;
			ttf_textRect.top = i*ttf.charHeight;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		ncols=ttf.offX/ttf.charWidth;
		ttf_textClip.top = 0;
		ttf_textClip.bottom = ttf.charHeight;
		ttf_textClip.right = ttf.charWidth*ncols+ttf.offX%ttf.charWidth;
		ttf_textRect.left = 0;
		ncols+=ttf.offX%ttf.charWidth>0?1:0;
		textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, ncols, ttf_fgColor, ttf_bgColor, 0);
		for (int i=0;i<window.height/ttf.charHeight+adval;i++)
			{
			if (i==window.height/ttf.charHeight)
				ttf_textClip.bottom = window.height%ttf.charHeight;
			ttf_textRect.top = i*ttf.charHeight;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		ncols=ttf.offX/ttf.charWidth;
		ttf_textClip.right = ttf.charWidth*ncols+ttf.offX%ttf.charWidth;
		ttf_textClip.bottom = ttf.charHeight;
		ttf_textRect.left = window.width-ttf.offX;
		for (int i=0;i<window.height/ttf.charHeight+adval;i++)
			{
			if (i==window.height/ttf.charHeight)
				ttf_textClip.bottom = window.height%ttf.charHeight;
			ttf_textRect.top = i*ttf.charHeight;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		if ((!window.framed||ttf.fullScrn)&&!clearbutt&&sysicons)
			{
			vUpdateRect(0, 0, window.width-ttf.charWidth*(sysicons>2?6:3), sysicons>2?ttf.charHeight:ttf.charHeight/2);
			vUpdateRect(0, ttf.charHeight/(sysicons>2?1:2), window.width, window.height);
			}
		else
			{
			if (sysicons&&ttf.offY<ttf.charHeight/(sysicons>2?1:2)) memset(curAC, -1, ttf.cols*2);
			vUpdateRect(0, 0, window.width, window.height);
			}
		}
	ttf_textClip.top = 0;
	ttf_textClip.bottom = ttf.charHeight;
	for (int y = 0; y < ttf.lins; y++)
		{
		rgbColors = altBGR1;
		if (!hasFocus && (!window.framed || ttf.fullScrn) && (y == 0))				// Dim topmost line
			rgbColors = altBGR0;

		ttf_textRect.top = ttf.offY+y*ttf.charHeight;
		for (int x = 0; x < ttf.cols; x++)
			{
			if (hasFocus && blinking && newAC[x]>>12&8 || newAC[x] != curAC[x])
				{
				xmin = min(x, xmin);
				ymin = min(y, ymin);
				ymax = y;
				ttf_textRect.left = ttf.offX+hPadPixs+x*ttf.charWidth;
				style = TTF_STYLE_NORMAL;

				Bit8u colorBG = newAC[x]>>12;
				Bit8u colorFG = (newAC[x]>>8)&15;
				processWP(&style, &colorBG, &colorFG);
				if (blinking && colorBG&8)
					{
					colorBG-=8;
					if ((bcount/8)%2 && hasFocus)
						colorFG=colorBG;
					}
				ttf_bgColor.b = rgbColors[colorBG].rgbBlue;
				ttf_bgColor.g = rgbColors[colorBG].rgbGreen;
 				ttf_bgColor.r = rgbColors[colorBG].rgbRed;
				ttf_fgColor.b = rgbColors[colorFG].rgbBlue;
				ttf_fgColor.g = rgbColors[colorFG].rgbGreen;
				ttf_fgColor.r = rgbColors[colorFG].rgbRed;

				int x1 = x;
				Bit8u ascii = newAC[x]&255;
				if (cpMap[ascii] > 0x2590 && cpMap[ascii] < 0x2594)					// Special: characters 176-178 = shaded block
					{
					ttf_bgColor.b = (ttf_bgColor.b*(179-ascii) + ttf_fgColor.b*(ascii-175))>>2;
					ttf_bgColor.g = (ttf_bgColor.g*(179-ascii) + ttf_fgColor.g*(ascii-175))>>2;
					ttf_bgColor.r = (ttf_bgColor.r*(179-ascii) + ttf_fgColor.r*(ascii-175))>>2;
					do																// As long char and foreground/background color equal
						{
						curAC[x] = newAC[x];
						asciiStr[x-x1] = 32;										// Shaded space
						x++;
						}
					while (x < ttf.cols && newAC[x] == newAC[x1] && newAC[x] != curAC[x]);
					}
				else
					{
					Bit8u color = newAC[x]>>8;
					do																// As long foreground/background color equal
						{
						curAC[x] = newAC[x];
						asciiStr[x-x1] = ascii;
						x++;
						ascii = newAC[x]&255;
						}
					while (x < ttf.cols && newAC[x] != curAC[x] && newAC[x]>>8 == color && (ascii < 176 || ascii > 178));
					}
				xmax = max(x-1, xmax);
				vSurface* textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, x-x1, ttf_fgColor, ttf_bgColor, style);
				ttf_textClip.right = (x-x1)*ttf.charWidth;
				vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
				if (hPadding && (x1 == 0 || (x && (x%ttf.cols) == 0)))				// Eventually add space to the left and right of the content
					{
					RECT rect;
					rect.top = ttf.offY+y*ttf.charHeight;
					rect.bottom = rect.top+ttf.charHeight;
					if (wpVersion > 0)												// WP draws its own cursor negative
						{
						int xWP, yWP;
						if (MousePosWP(&xWP, &yWP))
							{
							if (yWP == y)
								{
								if ((x1 == 0 && xWP == 0) || (x && (x%ttf.cols) == 0 && xWP == x-1))
									{
									colorBG = (7-(colorBG&7))|(colorBG&8);
									ttf_bgColor.b = rgbColors[colorBG].rgbBlue;
									ttf_bgColor.g = rgbColors[colorBG].rgbGreen;
 									ttf_bgColor.r = rgbColors[colorBG].rgbRed;
									}
								}
							}
						}
					if (x1 == 0)													// If left most character
						{
						rect.left = ttf.offX;
						rect.right = rect.left+hPadPixs;
						vBlitFill((255<<24)+(ttf_bgColor.r<<16)+(ttf_bgColor.g<<8)+ttf_bgColor.b, &rect);
						}
					if (x && x%ttf.cols == 0)										// If right most character
						{
						rect.left = ttf.offX+hPadPixs+ttf.cols*ttf.charWidth;
						rect.right = rect.left+ttf.charWidth-hPadPixs;
						vBlitFill((255<<24)+(ttf_bgColor.r<<16)+(ttf_bgColor.g<<8)+ttf_bgColor.b, &rect);
						}
					}
				x--;
				}
			}
		curAC += ttf.cols;
		newAC += ttf.cols;
		}
	bcount++;
	int newPos = vga.draw.cursor.address>>1;
	if (hasFocus && vga.draw.cursor.enabled && vga.draw.cursor.sline <= vga.draw.cursor.eline && vga.draw.cursor.sline < 16)	// Draw cursor?
		{
		if (newPos >= 0 && newPos < ttf.cols*ttf.lins)								// If on screen
			{
			int y = newPos/ttf.cols;
			int x = newPos%ttf.cols;
			vga.draw.cursor.count++;
			vga.draw.cursor.blinkon = (vga.draw.cursor.count & 4) ? true : false;
			if (ttf.cursor != newPos || vga.draw.cursor.sline != prev_sline || ((blinkstate != vga.draw.cursor.blinkon) && blinkCursor))		// If new position or shape changed, forse draw
				{
				if (blinkCursor && blinkstate == vga.draw.cursor.blinkon)
				{
					vga.draw.cursor.count = 4;
					vga.draw.cursor.blinkon = true;
				}
				prev_sline = vga.draw.cursor.sline;
				xmin = min(x, xmin);
				xmax = max(x, xmax);
				ymin = min(y, ymin);
				ymax = max(y, ymax);
				}
			blinkstate = vga.draw.cursor.blinkon;
			ttf.cursor = newPos;
			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax)					// If overdrawn previuosly (or new shape)
				{
				Bit8u colorBG = newAttrChar[newPos]>>12;
				Bit8u colorFG = (newAttrChar[newPos]>>8)&15;
				style = TTF_STYLE_NORMAL;
				processWP(&style, &colorBG, &colorFG);
				if (blinking && colorBG&8)
					{
					colorBG-=8;
					if ((bcount/8)%2 && hasFocus)
						colorFG=colorBG;
					}
				ttf_bgColor.b = rgbColors[colorBG].rgbBlue;
				ttf_bgColor.g = rgbColors[colorBG].rgbGreen;
				ttf_bgColor.r = rgbColors[colorBG].rgbRed;
				ttf_fgColor.b = rgbColors[colorFG].rgbBlue;
				ttf_fgColor.g = rgbColors[colorFG].rgbGreen;
				ttf_fgColor.r = rgbColors[colorFG].rgbRed;
				asciiStr[0] = newAttrChar[newPos]&255;
				// First redraw character
				vSurface* textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, 1, ttf_fgColor, ttf_bgColor, style);
				ttf_textClip.right = ttf_textClip.left+ttf.charWidth;
				ttf_textRect.left = ttf.offX+hPadPixs+x*ttf.charWidth;
				ttf_textRect.top = ttf.offY+y*ttf.charHeight;
				vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
				// Then reverse lower lines
				if ((vga.draw.cursor.blinkon || !blinkCursor) && hasFocus)
					{
					textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, 1, ttf_bgColor, ttf_fgColor, style);
					ttf_textClip.top = (ttf.charHeight*vga.draw.cursor.sline)>>4;
					ttf_textClip.bottom = ttf.charHeight;								// For now, cursor to bottom
					ttf_textRect.top = ttf.offY+y*ttf.charHeight + ttf_textClip.top;
					vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
					}
				}
			}
		}
	ttf.cursor = newPos;

	if (sysicons && (!hasFocus || hasMinButton || (ttf.offX || ttf.offY) && clearbutt >= 0) && (!window.framed || ttf.fullScrn) && xmax == ttf.cols-1 && ymin == 0)	// (Re)draw Minimize button?
		{
		clearbutt = (ttf.offX || ttf.offY) && !hasMinButton && hasFocus;
		if (sysicons>1)
			{
			if (sysicons>2)
				{
				ttf.charHeight*=2;
				ttf.charWidth*=2;
				}
			Bit8u color = clearbutt ? 0 : (newAttrChar[xmax]>>8)&15;
			RECT rect;
			rect.left = window.width-ttf.charWidth*3;
			rect.right = window.width;
			rect.top = 0;
			rect.bottom = ttf.charHeight/2;
			vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect);
			color = clearbutt ? 0 : newAttrChar[xmax]>>12;
			rect.left += ttf.charWidth/6;
			rect.right = window.width-ttf.charWidth*2-ttf.charWidth/6;
			rect.top = ttf.charHeight/4;
			if (rect.top>ttf.charHeight/2-4) rect.top--;
			rect.bottom = rect.top+2;
			vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect); // Minimize button
			rect.left += ttf.charWidth;
			rect.right += ttf.charWidth;
			int down=0;
			if (ttf.fullScrn)
				{
				rect.left += 4;
				rect.right += 4;
				if (rect.right > window.width-ttf.charWidth)
					{
					rect.left -= rect.right - (window.width-ttf.charWidth);
					rect.right = window.width-ttf.charWidth;
					}
				rect.bottom = ttf.charHeight/3;
				rect.top = ttf.charHeight/4-5;
				if (rect.top < 0)
					{
					down=rect.top==-1?1:2;
					rect.bottom -= rect.top+(down>1?1:0);
					rect.top = 0;
					}
				vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect);
				color = clearbutt ? 0 : (newAttrChar[xmax]>>8)&15;
				rect.left += 2;
				rect.right -= 2;
				rect.top += down>0?1:2;
				rect.bottom -= 2;
				vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect); // Restore button
				}
			color = clearbutt ? 0 : newAttrChar[xmax]>>12;
			rect.left -= ttf.fullScrn?6:0;
			rect.right -= (ttf.fullScrn?2:0);
			if (rect.left<window.width-2*ttf.charWidth)
				{
				rect.right -= rect.left - (window.width-2*ttf.charWidth);
				rect.left = window.width-2*ttf.charWidth;
				}
			rect.top = (ttf.fullScrn?ttf.charHeight/4:ttf.charHeight/6)-2+(down>1?1:0);
			rect.bottom = ttf.charHeight/3-(ttf.charHeight/3>ttf.charHeight/2-4?1:0)+3+down;
			if (rect.top < 0)
				{
				rect.bottom -= rect.top;
				rect.top = 0;
				}
			vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect);
			color = clearbutt ? 0 : (newAttrChar[xmax]>>8)&15;
			rect.left += 2;
			rect.right -= 2;
			rect.top += 2;
			rect.bottom -= 2;
			if (rect.bottom==ttf.charHeight/2&&rect.bottom>rect.top+1) rect.bottom--;
			vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect); // Maximize/restore button
			color = clearbutt ? 0 : newAttrChar[xmax]>>12;
			for (int i=ttf.charWidth/6;i<ttf.charWidth*5/6;i++)
				{
				rect.left = i+(window.width-ttf.charWidth)+(ttf.charWidth>ttf.charHeight*3/4?i-ttf.charWidth/6:0);
				rect.right = rect.left+2;
				rect.top = i+(ttf.charHeight/2-(ttf.charWidth>ttf.charHeight*3/4?(ttf.charWidth*2/3+1):ttf.charWidth)-1)/2;
				if (rect.top<0) rect.top=0;
				rect.bottom = rect.top+2;
				if (rect.right>window.width-ttf.charWidth/6 || rect.bottom>=ttf.charHeight/2) break;
				vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect);
				rect.left = 2*window.width-ttf.charWidth-rect.left-2;
				rect.right = rect.left+2;
				vBlitFill((255<<24)+(rgbColors[color].rgbRed<<16)+(rgbColors[color].rgbGreen<<8)+rgbColors[color].rgbBlue, &rect); // Close button
				if (rect.left==ttf.charWidth/6+window.width-ttf.charWidth) break;
				}
			if (sysicons>2)
				{
				ttf.charHeight/=2;
				ttf.charWidth/=2;
				}
			}
		else
			{
			clearbutt = (ttf.offX || ttf.offY) && !hasMinButton && hasFocus;
			Bit8u color = clearbutt ? 0 : newAttrChar[xmax]>>12;
			ttf_fgColor.b = rgbColors[color].rgbBlue;
			ttf_fgColor.g = rgbColors[color].rgbGreen;
			ttf_fgColor.r = rgbColors[color].rgbRed;
			color = clearbutt ? 0 : (newAttrChar[xmax]>>8)&15;
			ttf_bgColor.b = rgbColors[color].rgbBlue;
			ttf_bgColor.g = rgbColors[color].rgbGreen;
			ttf_bgColor.r = rgbColors[color].rgbRed;
			asciiStr[0] = 45;															// '-'
			asciiStr[1] = ttf.fullScrn?61:43;											// '=' or '+'
			vSurface* textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, 2, ttf_fgColor, ttf_bgColor, 0);
			ttf_textClip.top = ttf.charHeight>>2;
			ttf_textClip.bottom = (ttf.charHeight>>1) + (ttf.charHeight>>2);
			ttf_textClip.left = 0;
			ttf_textClip.right = ttf.charWidth*2;
			ttf_textRect.top = 0;
			ttf_textRect.left = window.width-ttf.charWidth*3;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			asciiStr[0] = 120;															// 'x'
			textSurface = TTF_RenderASCII(ttf.font, ttf.fontb, ttf.fonti, ttf.fontbi, asciiStr, 1, ttf_fgColor, ttf_bgColor, 0);
			if (ttf_textClip.bottom<ttf.charHeight-1)
				{
				ttf_textClip.top++;
				ttf_textClip.bottom++;
				}
			ttf_textClip.right = ttf.charWidth;
			ttf_textRect.left = window.width-ttf.charWidth;
			vBlitSurface(textSurface, &ttf_textClip, &ttf_textRect);
			}
		vUpdateRect(window.width-ttf.charWidth*(sysicons>2?6:3), 0, window.width*(sysicons>2?2:1), ttf.charHeight/(sysicons>2?1:2));
		if (clearbutt) clearbutt = -1;
		}
	if (xmin <= xmax)																// If any changes
		{
		xmin = ttf.offX+xmin*ttf.charWidth+(xmin ? hPadPixs : 0);
		xmax++;
		xmax = ttf.offX+hPadPixs+xmax*ttf.charWidth+(xmax%ttf.cols ? 0 : ttf.charWidth-hPadPixs);
		vUpdateRect(xmin, ttf.offY+ymin*ttf.charHeight, xmax-xmin, (ymax-ymin+1)*ttf.charHeight);
		}
	}

int getMaxScale(int* maxw, int* maxh, bool difscr)
	{
	HMONITOR monitor = difscr ? MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST):MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	int maxWidth = info.rcMonitor.right-info.rcMonitor.left;
	int maxHeight = info.rcMonitor.bottom-info.rcMonitor.top;
	if (!ttf.fullScrn && window.framed)												// 3D borders
		{
		maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
		maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
		}
	*maxw = maxWidth;
	*maxh = maxHeight; 
	return min(maxWidth/640, maxHeight/480);										// Based on max resolution supported VGA modes
	}

bool resetWin(bool full=false, int pad=-1, int pac=-1)
	{
	if (full)
		{
		HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		int maxWidth = info.rcMonitor.right-info.rcMonitor.left;
		int maxHeight = info.rcMonitor.bottom-info.rcMonitor.top;
		int pPointSize = 0;
		RECT pPosition;
		if (GetWindowRect(vDosHwnd, &pPosition))
			pPointSize = ttf.pointsize;
		if (shortcut) maxHeight--;
		bool done=false;
		do
			{
			while (ttf.cols*ttf.charWidth <= maxWidth - padding*2 && ttf.lins*ttf.charHeight <= maxHeight - padding*2)	// Very lazy method indeed
				{
				SelectFontByPoints(ttf.pointsize+2);
				done=true;
				}
			if (ttf.pointsize<3) break;
			SelectFontByPoints(ttf.pointsize-2);
			} while (!done);
		if (pPointSize>0&&(pad>-1||pac>-1)&&(ttf.pointsize<12||ttf.offX<0||ttf.offY<0))
			{
			if (speaker) Beep(1750, 300);
			if (pad>-1 && pac>-1)
				{
				padding=pad;
				padcolor=pac;
				}
			else if (pad>-1)
				ttf.lins=pad;
			else if (pac>-1)
				ttf.cols=pac;
			ttf.pointsize=pPointSize;
			SelectFontByPoints(ttf.pointsize);
			MoveWindow(vDosHwnd, pPosition.left, pPosition.top, pPosition.right-pPosition.left, pPosition.bottom-pPosition.top, false);
			return false;
			}
			if (ttf.fullScrn && (pad>-1 || pac>-1))
				prevPointSize=0;
		}
	SetVideoSize();
	TimedSetSize();
	memset(curAttrChar, -1, ttf.cols*ttf.lins*2);
	return true;
	}

static void decreaseFontSize()
	{
	if (vga.mode != M_TEXT)
		{
		int maxWidth=0,maxHeight=0,maxScale=getMaxScale(&maxWidth, &maxHeight, false);
		bool reset=false;
		if (abs(window.scalex)>1)
			{
			reset=true;
			window.scalex-=window.scalex>0?1:-1;
			}
		if (window.scaley!=10&&abs(window.scaley)>1)
			{
			reset=true;
			window.scaley-=window.scaley>0?1:-1;
			}
		if (reset) resetWin();
		}
	else if (ttf.pointsize > 12)
		{
		int maxWidth=0, maxHeight=0;
		getMaxScale(&maxWidth, &maxHeight, false);
		SelectFontByPoints(ttf.pointsize-2);
		SetVideoSize();
		memset(curAttrChar, -1, ttf.cols*ttf.lins*2);								// Force redraw of complete window
		winPerc = (int)ceil(float(100*ttf.cols*ttf.charWidth/(maxWidth-ttf.offX*2)*ttf.lins*ttf.charHeight/(maxHeight-ttf.offY*2)));
		}
	}

static void increaseFontSize()
	{
	if (vga.mode != M_TEXT)
		{
		int maxWidth=0,maxHeight=0,maxScale=getMaxScale(&maxWidth, &maxHeight, false);
		bool reset=false;
		if (abs(window.scalex)<(window.scaley!=10?maxWidth/640:maxScale))
			{
			reset=true;
			window.scalex+=window.scalex>0?1:-1;
			}
		if (window.scaley!=10&&abs(window.scaley)<maxHeight/480)
			{
			reset=true;
			window.scaley+=window.scaley>0?1:-1;
			}
		if (reset) resetWin();
		}
	else																			// Increase fontsize
		{
		int currSize = ttf.pointsize;
		int maxWidth=0, maxHeight=0;
		getMaxScale(&maxWidth, &maxHeight, false);
		SelectFontByPoints(ttf.pointsize+2);
		if (ttf.cols*ttf.charWidth <= maxWidth - ttf.offX*2 && ttf.lins*ttf.charHeight <= maxHeight - ttf.offY*2)		// if it fits on screen
			{
			SetVideoSize();
			memset(curAttrChar, -1, ttf.cols*ttf.lins*2);							// Force redraw of complete window
			}
		else
			SelectFontByPoints(currSize);
		winPerc = (int)ceil(float(100*ttf.cols*ttf.charWidth/(maxWidth-ttf.offX*2)*ttf.lins*ttf.charHeight/(maxHeight-ttf.offY*2)));
		}
	}

bool accWin(int maxWidth, int maxHeight)
	{
	int curSize = 30;																// No clear idea what would be a good starting value
	int lastGood = -1;
	int trapLoop = 0;

	while (curSize != lastGood)
		{
		SelectFontByPoints(curSize);
		if ((ttf.cols+hPadding)*ttf.charWidth <= maxWidth - ttf.offX*2 && ttf.lins*ttf.charHeight <= maxHeight - ttf.offY*2)		// If it fits on screen
			{
			lastGood = curSize;
			float coveredPerc = float(100*ttf.cols*ttf.charWidth/(maxWidth-ttf.offX*2)*ttf.lins*ttf.charHeight/(maxHeight-ttf.offY*2));
			if (trapLoop++ > 4 && coveredPerc <= winPerc)							// We can get into a +/-/+/-... loop!
				break;
			curSize = (int)(curSize*sqrt((float)winPerc/coveredPerc));				// Rounding down is ok
			if (curSize < 12)														// Minimum size = 12
				curSize = 12;
			}
		else if (--curSize < 12)													// Silly, but OK, you never know..
			return false;
		}
	pSize = curSize;
	if (evensize)
		curSize &= ~1;																// Make it's even (a bit nicer)
	SelectFontByPoints(curSize);
	return true;
	}

bool setWin(int pad=-1, int pac=-1)
	{
	int pPointSize = ttf.pointsize;
	RECT pPosition;
	GetWindowRect(vDosHwnd, &pPosition);
	int maxWidth=0, maxHeight=0;
	getMaxScale(&maxWidth, &maxHeight, false);
	if (accWin(maxWidth, maxHeight))
		{
		SetVideoSize();
		RECT rect;
		GetWindowRect(vDosHwnd, &rect);
		resetWin();
		SetWindowPos(vDosHwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
		if (firstmax)
			{
			BringWindowToTop(GetForegroundWindow());
			mouse_event(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, 32768, 32768, 0, 0);
			}
		return true;
		}
	else
		{
		if (pad!=-2&&speaker) Beep(1750, 300);
		if (pad>-1&&pac>-1)
			{
			padding=pad;
			padcolor=pac;
			}
		else if (pad>-1&&pac==-1)
			ttf.lins=pad;
		else if (pad==-1&&pac>-1)
			ttf.cols=pac;
		ttf.pointsize = pPointSize;
		SelectFontByPoints(ttf.pointsize);
		MoveWindow(vDosHwnd, pPosition.left, pPosition.top, pPosition.right-pPosition.left, pPosition.bottom-pPosition.top, false);
		return false;
		}
	}

bool setWinInitial(const char *winDef, bool *fs)
	{																				// Format = <max perc>[,x-pos:y-pos]
	bool hpad = hPadding;
	hPadding = true;
	if (*winDef == '-')																// Padding left and right?
		{
		hPadding = false;
		winDef++;
		while (isspace(*winDef))
			++winDef;
		}
	if (!*winDef || !strcmp(winDef,","))											// Nothing set
		return true;
	else if (!strcmp(winDef,",:"))
		{
		*fs=false;
		return true;
		}
	int testVal1, testVal2, testVal3, l=0, perc=ttf.fullScrn?100:winPerc;
	char testStr[512];
	if (l=sscanf(winDef, "%d%s", &testVal1, testStr))								// Only <max perc>
		if (testVal1 > 0 && testVal1 <= 100 && (l==1 || !strcmp(testStr,",") || !strcmp(testStr,",:")))	// 1/100% are absolute minimum/maximum
			{
			if (*fs && vga.mode!=M_TEXT && testVal1!=perc) return NULL;
			if (!*fs || testVal1<100) winPerc = testVal1;
			if (*fs)
				{
				ttf.fullScrn=testVal1==100;
				if (!strcmp(testStr,",:")) *fs=false;
				}
			return true;
			}
	if (sscanf(winDef, ",%d:%d%s", &testVal1, &testVal2, testStr) == 2)				// Only x-and y-pos
		if (testVal1 >= 0 && testVal2 >= 0)
			{
			initialX = testVal1;
			initialY = testVal2;
			return true;
			}
	if (sscanf(winDef, "%d,%d:%d%s", &testVal1, &testVal2, &testVal3, testStr) == 3)	// All parameters
		if (testVal1 > 0 && testVal1 <= 100 && testVal2 >= 0 && testVal3 >= 0)		// X-and y-pos only tested for positive values
			{																		// Values too high are checked later and uActually dropped
			if (*fs && vga.mode!=M_TEXT && testVal1!=perc) return NULL;
			if (!*fs || testVal1<100) winPerc = testVal1;
			initialX = testVal2;
			initialY = testVal3;
			ttf.fullScrn=testVal1==100;
			return true;
			}
	hPadding = hpad;
	return false;
	}

void reaccWin()
	{
	int maxWidth=0,maxHeight=0,maxScale=getMaxScale(&maxWidth, &maxHeight, true);
	bool f=false;
	char perc[5];
	sprintf(perc,"%d",winPerc);
	if (setWinInitial(perc,&f))
		{
		accWin(maxWidth, maxHeight);
		resetWin();
		}
	}

void maxWin()
	{
	if (ttf.fullScrn || vga.mode != M_TEXT)
		{
		if (speaker) Beep(1750, 300);
		return;
		}
	ttf.fullScrn = true;
	HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	int maxWidth = info.rcMonitor.right-info.rcMonitor.left;
	int maxHeight = info.rcMonitor.bottom-info.rcMonitor.top;
	if (shortcut) maxHeight--;
	if (getr=GetWindowRect(vDosHwnd, &prevPosition))					// Save position and point size
		prevPointSize = ttf.pointsize;
	while ((ttf.cols+hPadding)*ttf.charWidth <= maxWidth - padding*2 && ttf.lins*ttf.charHeight <= maxHeight - padding*2)	// Very lazy method indeed
		SelectFontByPoints(ttf.pointsize+2);
	SelectFontByPoints(ttf.pointsize-2);
	if (ttf.offX<0||ttf.offY<0)
		{
		if (speaker) Beep(1750, 300);
		ttf.pointsize=prevPointSize;
		ttf.fullScrn=false;
		SelectFontByPoints(ttf.pointsize);
		return;
		}
	resetWin();
	}

void minWin()
	{
	if (!ttf.fullScrn || vga.mode != M_TEXT)
		{
		if (speaker) Beep(1750, 300);
		return;
		}
	ttf.fullScrn = false;
	HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	int maxWidth = info.rcMonitor.right-info.rcMonitor.left;
	int maxHeight = info.rcMonitor.bottom-info.rcMonitor.top;
	int width=ttf.charHeight>ttf.charWidth*3/2?ttf.charWidth:ttf.charWidth/2;
	int height=ttf.charWidth>ttf.charHeight*3/2?ttf.charHeight:ttf.charHeight/2;
	ttf.offX = width*((padding+width-1)/width);
	ttf.offY = height*((padding+height-1)/height);
	if (prevPointSize)												// Switching back to windowed mode
		{
		if (ttf.pointsize != prevPointSize)
			SelectFontByPoints(prevPointSize);
		}
	else if (window.framed)											// First switch to window mode with frame and maximized font size could give a too big window
		{
		maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
		maxHeight -= GetSystemMetrics(SM_CYCAPTION)+GetSystemMetrics(SM_CYBORDER)*2;
		if ((ttf.cols+hPadding)*ttf.charWidth > maxWidth || ttf.lins*ttf.charHeight > maxHeight)	// If it doesn't fits on screen
			SelectFontByPoints(ttf.pointsize-2);					// Should do the trick
		}
	hasMinButton = false;
	if (!tobeframe&&prevPointSize)
		{
		resetWin();
		MoveWindow(vDosHwnd, max(info.rcMonitor.left, prevPosition.left), max(info.rcMonitor.top, prevPosition.top), prevPosition.right-prevPosition.left, prevPosition.bottom-prevPosition.top, false);
		}
	else
		{
		setWin();
		if (getr)
			{
			RECT newrect;
			BOOL cur=GetWindowRect(vDosHwnd, &newrect);
			if (cur) MoveWindow(vDosHwnd, max(info.rcMonitor.left,prevPosition.left-((newrect.right-newrect.left)-(prevPosition.right-prevPosition.left))/2), max(info.rcMonitor.top,prevPosition.top-((newrect.bottom-newrect.top)-(prevPosition.bottom-prevPosition.top))/2), newrect.right-newrect.left, newrect.bottom-newrect.top, true);
			}
		}
	getr=false;
	tobeframe=false;
	}

BOOL CALLBACK EnumDispProc(HMONITOR hMon, HDC dcMon, RECT* pRcMon, LPARAM lParam)
	{
	xyp* xy = reinterpret_cast<xyp*>(lParam);
	curscr++;
	if (xy&&xy->x>-1&&xy->y>-1)
		{
		if (xy->x==pRcMon->left&&xy->y==pRcMon->top) screen=-curscr;
		}
	else if (screen==curscr)
		monrect=*pRcMon;
	return TRUE;
	}

bool getScale(char* scale, int* scalex, int* scaley, bool* warn)
	{
	int maxWidth=0,maxHeight=0,maxScale=getMaxScale(&maxWidth,&maxHeight,screen>0);
	if (maxScale<1)
		{
		sprintf(msg,"%s: scale factor is not supported for this screen resolution", configfile);
		E_Exit(msg);
		}
	*warn=false;
	if (!strlen(scale))
		{
		*scalex=-maxScale;
		*scaley=10;
		return true;
		}
	bool p=false;
	if (scale[0]=='-')
		{
		p=true;
		scale=lTrim(++scale);
		}
	if (scale[0]>='0'&&scale[0]<='9')
		{
		*scalex=scale[0]-48;
		if (*scalex>maxWidth/640)
			{
			*warn=true;
			if (!p)	return false;
			}
		scale=lTrim(++scale);
		if (scale[0]==',')
			{
			scale=lTrim(++scale);
			p=false;
			if (scale[0]=='-')
				{
				p=true;
				scale=lTrim(++scale);
				}
			if (scale[0]>='0'&&scale[0]<='9')
				{
				*scaley=scale[0]-48;
				if (*scaley>maxHeight/480)
					{
					*warn=true;
					if (!p)	return false;
					}
				scale=lTrim(++scale);
				if (scale[0]>='0'&&scale[0]<='9')
					return false;
				}
			else if (strlen(scale))
				return false;
			else
				{
				*scaley=10;
				if (*scalex>1&&*scalex>maxHeight/480)
					{
					*warn=true;
					if (!p)	return false;
					}
				}
			}
		else if (strlen(scale))
			return false;
		else
			{
			*scaley=10;
			if (*scalex>1&&*scalex>maxHeight/480)
				{
				*warn=true;
				if (!p)	return false;
				}
			}
		}
	else if (strlen(scale))
		return false;
	if (*scalex < 1)															// 0 = probably not set in config.txt
		*scalex = -max(1, min(9, *scaley == 10 ? maxScale : maxWidth/640));
	if (*scaley < 1)									// 0 = probably not set in config.txt
		*scaley = -max(1, min(9, maxHeight/480));								// Based on max resolution supported VGA modes
	return true;
	}

int getScreen(bool def)
	{
	HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	xyp xy={0};
	xy.x=def?0:info.rcMonitor.left;
	xy.y=def?0:info.rcMonitor.top;
	curscr=0;
	EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
	return abs(screen);
	}

void saveScreen(int scr)
	{
	if (scr==0) scr=abs(screen);
	screen=getScreen(false);
	if (scr!=screen)
		{
		xyp xy={0};
		xy.x=-1;
		xy.y=-1;
		curscr=0;
		EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
		int maxWidth=0,maxHeight=0,maxScale=getMaxScale(&maxWidth, &maxHeight, true);
		if (window.scalex<0) window.scalex=-(window.scaley==10?maxScale:maxWidth/640);
		if (window.scaley<0) window.scaley=-(maxHeight/480);
		bool f=false;
		char perc[5];
		sprintf(perc,"%d",ttf.fullScrn?100:winPerc);
		if (vga.mode!=M_TEXT||setWinInitial(perc,&f)&&accWin(maxWidth, maxHeight))
			{
			HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO info;
			info.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &info);
			//SetVideoSize();
			RECT rect, newrect;
			BOOL res=GetWindowRect(vDosHwnd, &rect);
			resetWin();
			BOOL cur=GetWindowRect(vDosHwnd, &newrect);
			if (res&&cur&&(vga.mode!=M_TEXT||!ttf.fullScrn))
				MoveWindow(vDosHwnd, max(info.rcMonitor.left, rect.left-((newrect.right-newrect.left)-(rect.right-rect.left))/2), max(info.rcMonitor.top, rect.top-((newrect.bottom-newrect.top)-(rect.bottom-rect.top))/2), newrect.right-newrect.left, newrect.bottom-newrect.top, true);
			}
		}
	}

static LRESULT CALLBACK SysMenuExtendWndProc(HWND hwnd, UINT uiMsg, WPARAM wparam, LPARAM lparam)
	{
	if (uiMsg  == WM_SYSCOMMAND)
		{
		switch (wparam)
			{
		//case WM_SC_SYSMENUMAX:
		case SC_MAXIMIZE:
			maxWin();
			return 0;
		case SC_MOVE:
		case 0xf012:
			CallWindowProc(fnDefaultWndProc, hwnd, uiMsg, wparam, lparam);
			saveScreen(0);
			return 0;
		case WM_SC_SYSMENUABOUT:
			showAboutMsg();
			return 0;
		case WM_SC_SYSMENUNOTES:
			showNotes();
			return 0;
		case WM_SC_SYSMENUPCOPY:
			copyScreen();
			return 0;
		case WM_SC_SYSMENUPDUMP:
			dumpScreen();
			return 0;
		case WM_SC_SYSMENUPASTE:
			getClipboard();
			return 0;
		case WM_SC_SYSMENUDECREASE:
			decreaseFontSize();
			return 0;
		case WM_SC_SYSMENUINCREASE:
			increaseFontSize();
			return 0;
			}
		}
	return CallWindowProc(fnDefaultWndProc, hwnd, uiMsg, wparam, lparam);
	}

bool getWP(char * wpStr)
	{
	xyVersion=xyBackGround=0;
	wsVersion=wsBackGround=0;
	wpVersion=0;
	if (!strnicmp(wpStr, "XY", 2))													// WP = XY (XyWrite)
		{
		wpStr += 2;
		xyVersion = 1;
		xyBackGround = 1;
		lTrim(wpStr);
		if (*wpStr == 0)
			return true;
		if (sscanf(wpStr, ", %d", &xyBackGround) == 1)
			if (xyBackGround >= 0 && xyBackGround <= 15)
				return true;
		xyBackGround = 0;
		xyVersion = 0;
		return false;
		}
	else if (!strnicmp(wpStr, "WS", 2))												// WP = WS (WordStar)
		{
		wpStr += 2;
		wsVersion = 1;
		lTrim(wpStr);
		if (*wpStr == 0)
			return true;
		if (sscanf(wpStr, ", %d", &wsBackGround) == 1)
			if (wsBackGround >= 0 && wsBackGround <= 15)
				return true;
		wsBackGround = 0;
		wsVersion = 0;
		return false;
		}
	if (sscanf(wpStr, "%d", &wpVersion) == 1)										// No checking on version number
		return true;
	return false;

	}

bool getWheelmod(char* wheelmod)
	{
	if (!strlen(wheelmod))
		{
		wheelmodv=1;
		wheelmodh=2;
		return true;
		}
	if (wheelmod[0]>='0'&&wheelmod[0]<='7')
		{
		wheelmodv=wheelmod[0]-48;
		wheelmodh=2;
		wheelmod=lTrim(++wheelmod);
		if (wheelmod[0]==',')
			{
			wheelmod=lTrim(++wheelmod);
			if (wheelmod[0]>='0'&&wheelmod[0]<='7')
				{
				wheelmodh=wheelmod[0]-48;
				wheelmod=lTrim(++wheelmod);
				if (wheelmod[0]>='0'&&wheelmod[0]<='9')
					return false;
				}
			else if (strlen(wheelmod))
				return false;
			}
		else if (strlen(wheelmod))
			return false;
		}
	else if (strlen(wheelmod))
		return false;
	return true;
	}

bool getClickmod(char* clickmod)
	{
	if (!strlen(clickmod))
		{
		clickmodl=1;
		clickmodr=4;
		return true;
		}
	bool p=false;
	if (clickmod[0]=='-')
		{
		p=true;
		clickmod=lTrim(++clickmod);
		}
	if (clickmod[0]>='0'&&clickmod[0]<='5')
		{
		clickmodl=clickmod[0]-48;
		if (p) clickmodl=-clickmodl;
		clickmodr=4;
		clickmod=lTrim(++clickmod);
		if (clickmod[0]==',')
			{
			clickmod=lTrim(++clickmod);
			p=false;
			if (clickmod[0]=='-')
				{
				p=true;
				clickmod=lTrim(++clickmod);
				}
			if (clickmod[0]>='0'&&clickmod[0]<='5')
				{
				clickmodr=clickmod[0]-48;
				if (p) clickmodr=-clickmodr;
				clickmod=lTrim(++clickmod);
				if (clickmod[0]>='0'&&clickmod[0]<='9')
					return false;
				}
			else if (strlen(clickmod))
				return false;
			}
		else if (strlen(clickmod))
			return false;
		}
	else if (strlen(clickmod))
		return false;
	return true;
	}

bool getPadding(char *p)
	{
	char pad[20], *paddings;
	if (strlen(p)>19)
		{
		strncpy(pad,p,19);
		pad[19]=0;
		}
	else
		strcpy(pad,p);
	paddings=pad;
	p=strchr(paddings, ',');
	if (p!=NULL)
		*p=0;
	if (!strlen(lrTrim(paddings))||!strcmp(lrTrim(paddings),"0")||!strcmp(lrTrim(paddings),"+0")||!strcmp(lrTrim(paddings),"-0")||!strcmp(lrTrim(paddings),"00"))
		padding=0;
	else if (!(padding=strtol(lrTrim(paddings), NULL, 10)))
		return false;
	if (p==NULL)
		padcolor=0;
	else
		{
		*p=',';
		paddings=lrTrim(p+1);
		if (!strlen(lrTrim(paddings))||!strcmp(lrTrim(paddings),"0")||!strcmp(lrTrim(paddings),"+0")||!strcmp(lrTrim(paddings),"-0")||!strcmp(lrTrim(paddings),"00"))
			padcolor=0;
		else if (!(padcolor=strtol(lrTrim(paddings), NULL, 10))||padcolor<-1||padcolor>15)
			return false;
		}
	return padding>=0;
	}

bool setColors(const char *colorArray, int n)
	{
	const char * nextRGB = colorArray;
	Bit8u * altPtr = (Bit8u *)altBGR1;
	int rgbVal[3];
	for (int colNo = 0; colNo < (n>-1?1:16); colNo++)
		{
		if (n>-1) altPtr+=4*n;
		if (sscanf(nextRGB, " ( %d , %d , %d)", &rgbVal[0], &rgbVal[1], &rgbVal[2]) == 3)	// Decimal: (red,green,blue)
			{
			for (int i = 0; i< 3; i++)
				{
				if (rgbVal[i] < 0 || rgbVal[i] >255)
					return false;
				altPtr[2-i] = rgbVal[i];
				}
			while (*nextRGB != ')')
				nextRGB++;
			nextRGB++;
			}
		else if (sscanf(nextRGB, " #%6x", &rgbVal[0]) == 1)							// Hexadecimal
			{
			if (rgbVal < 0)
				return false;
			for (int i = 0; i < 3; i++)
				{
				altPtr[i] = rgbVal[0]&255;
				rgbVal[0] >>= 8;
				}
			nextRGB = strchr(nextRGB, '#') + 7;
			}
		else
			return false;
		altPtr += 4;
		}
	for (int i = n>-1?n:0; i < (n>-1?n+1:16); i++)
		{
		altBGR0[i].rgbBlue = (altBGR1[i].rgbBlue*2 + 128)/4;
		altBGR0[i].rgbGreen = (altBGR1[i].rgbGreen*2 + 128)/4;
		altBGR0[i].rgbRed = (altBGR1[i].rgbRed*2 + 128)/4;
		}
	return true;
	}

void GUI_StartUp()
	{
	STARTUPINFO si;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	GetStartupInfo(&si);
	char *title = ConfGetString("title");
	if (*title)
		strcpy(vDosCaption, title);
	else if (strstr(si.lpTitle," /cfg ")!=NULL || strstr(si.lpTitle," /set ")!=NULL);
	else if (char *p = strrchr(si.lpTitle, '\\'))	// Set title bar to shortcut name of vDosPlus
		{
		strncpy(vDosCaption, p+1,255);
		if (p = strrchr(vDosCaption, '.'))
			*p = 0;
		vDosCaption[255] = 0;
		}
	SetWindowText(vDosHwnd, vDosCaption);

	sstate = 0;
	if (si.dwFlags & STARTF_USESHOWWINDOW)
		sstate = si.wShowWindow == SW_MAXIMIZE ? 1 : (si.wShowWindow == SW_MINIMIZE || si.wShowWindow == SW_SHOWMINNOACTIVE ? 2 : 0);
	HICON IcoHwnd = getIcon(lTrim(ConfGetString("ICON")), true);
	if (IcoHwnd) 
		SetClassLongPtr(vDosHwnd, GCLP_HICON, (LONG)IcoHwnd); // Set external icon
	else
		{
		ConfAddError("The specified icon is invalid:\n", lTrim(ConfGetString("ICON")));
		SetClassLong(vDosHwnd, GCLP_HICON, (LONG)LoadIcon(GetModuleHandle(NULL), "vDosPlus_ico"));	// Set vDosPlus icon
		}
	if (strlen(lTrim(ConfGetString("font")))<260)
		strcpy(fName, lTrim(ConfGetString("font")));
	else
		{
		strncpy(fName, lTrim(ConfGetString("font")), 259);
		fName[259]=0;
		}
	if (*fName == '-')																// (Preceeding) - indicates ASCII 176-223 = lines
		{
		fontBoxed = true;
		strcpy(fbiName,lTrim(fName+1));
		strcpy(fName,fbiName);
		}
	strcpy(fbName, ConfGetString("boldfont"));
	strcpy(fiName, ConfGetString("italfont"));
	strcpy(fbiName, ConfGetString("boitfont"));
	char cName[40];
	if (readTTF(fName, false, false)&&strlen(fName))
		{
		if (!readTTF(fbName, true, false))
			{
			strcpy(fbName,"");
			readTTF(fbName, true, false);
			}
		if (!readTTF(fiName, false, true))
			{
			strcpy(fiName,"");
			readTTF(fiName, false, true);
			}
		if (!readTTF(fbiName, true, true))
			{
			strcpy(fbiName,"");
			readTTF(fbiName, true, true);
			}
		}
	else
		{
		if (strlen(fName))
			{
			strcpy(fName,"");
			readTTF(fName, false, false);
			}
		if ((*fbName || *fiName || *fbiName))
			{
			sprintf(cName, "%s%s%s", *fbName?"BOLDFONT\n":"", *fiName?"ITALFONT\n":"", *fbiName?"BOITFONT\n":"");
			if (cName[strlen(cName)-1]=='\n') cName[strlen(cName)-1]=0;
			ConfAddError("An valid FONT setting is needed for the following to take effect:\n", cName);
			}
		strcpy(fbName,"");
		readTTF(fbName, true, false);
		strcpy(fiName,"");
		readTTF(fiName, false, true);
		strcpy(fbiName,"");
		readTTF(fbiName, true, true);
		ttf.vDos = true;
		}
	ttf.lins = ConfGetInt("lins");
	if (ttf.lins<24||ttf.lins>txtMaxLins)
		{
		sprintf(cName, "LINS=%d", ttf.lins);
		ConfAddError(ttf.lins<24?"The specified number of lines can not be less than 24:\n":"The specified number of lines can not be greater than 60:\n", cName);
		}
	ttf.lins = max(24, min(txtMaxLins, ttf.lins));
	ttf.cols = ConfGetInt("cols");
	if (ttf.cols<60||ttf.cols>txtMaxCols)
		{
		sprintf(cName, "COLS=%d", ttf.cols);
		ConfAddError(ttf.cols<60?"The specified number of columns can not be less than 60:\n":"The specified number of columns can not be greater than 240:\n", cName);
		}
	ttf.cols = max(60, min(txtMaxCols, ttf.cols));
	for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++)								// Set the cols and lins in video mode 2,3,7
		{
		ModeList_VGA[i].twidth = ttf.cols;
		ModeList_VGA[i].theight = ttf.lins;
		}

	screen = ConfGetInt("screen");
	curscr=0;
	if (screen>0)
		{
		xyp xy={0};
		xy.x=-1;
		xy.y=-1;
		EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
		if (screen>curscr)
			{
			sprintf(cName, "SCREEN=%d", screen);
			ConfAddError("The specified screen number does not exist in current setup\n", cName);
			screen=0;
			}
		}
	else if (screen<0)
		{
		sprintf(cName, "SCREEN=%d", screen);
		ConfAddError("The screen number cannot be negative\n", cName);
		screen=0;
		}
	if (strlen(lTrim(ConfGetString("autoexec")))<255)
		strcpy(autoexec, lTrim(ConfGetString("autoexec")));
	else
		{
		strncpy(autoexec, lTrim(ConfGetString("autoexec")), 254);
		autoexec[254]=0;
		}
	if (!*autoexec)
		strcpy(autoexec,"AUTOEXEC.TXT");
	else if (stricmp(autoexec,"AUTOEXEC.TXT"))
  		vpLog("Using autoexec file %s",autoexec);
	if (screen>0)
		vpLog("Started on screen #%d.",screen);
	else
		vpLog("Started on the default screen.");
	usesMouse = ConfGetBool("mouse");
	autoHide = ConfGetBool("auhidems");
	vgafixms = ConfGetBool("vgafixms");
	showital = ConfGetBool("showital");
	showsubp = ConfGetBool("subpscr");
	showsout = ConfGetBool("strikout");
	blinkCursor = ConfGetBool("blinkc");
	smallclr = ConfGetInt("smallclr");
	keymode = ConfGetInt("keymode");
	keydelay = ConfGetInt("keydelay");
	keyinter = ConfGetInt("keyinter");
	sysicons = ConfGetInt("sysicons");
	synctime = ConfGetBool("synctime");
	shortcut = ConfGetBool("shortcut");
	speaker = ConfGetBool("speaker");
	topwin = ConfGetBool("topwin");
	winrun = ConfGetBool("winrun");
	window.framed = ConfGetBool("frame");
	transwin = ConfGetInt("transwin");
	eurAscii = ConfGetInt("euro");
	if (eurAscii != -1 && (eurAscii < 33 || eurAscii > 255))
		{
		sprintf(cName, "EURO=%d", eurAscii);
		ConfAddError("Euro ASCII value has to be between 33 and 255\n", cName);
		eurAscii = -1;
		}
	if (transwin < 0 || transwin > 90)
		{
		sprintf(cName, "TRANSWIN=%d", ConfGetInt("transwin"));
		ConfAddError("Transparent ratio has to be between 0 and 90\n", cName);
		transwin = 0;
		}
	if (smallclr < 0 || smallclr > 15)
		{
		sprintf(cName, "SMALLCLR=%d", smallclr);
		ConfAddError("ASCII color value of small text has to be between 0 and 15\n", cName);
		smallclr = 7;
		}
	if (sysicons < 0 || sysicons > 3)
		{
		sprintf(cName, "SYSICONS=%d", sysicons);
		ConfAddError("System icon mode has to be between 0 and 3\n", cName);
		sysicons = 2;
		}
	if (keymode < 0 || keymode > 2)
		{
		sprintf(cName, "KEYMODE=%d", keymode);
		ConfAddError("Keyboard mode has to be between 0 and 2\n", cName);
		keymode = 2;
		}
	if (keydelay < 0 || keyinter < 0)
		{
		if (keydelay<0 && keyinter<0)
			sprintf(cName, "KEYDELAY=%d\nKEYINTER=%d", keydelay, keyinter);
		else
			sprintf(cName, keydelay<0?"KEYDELAY=%d":"KEYINTER=%d", keydelay<0?keydelay:keyinter);
		ConfAddError("Keyboard delay values cannot be negative\n", cName);
		keydelay = 0;
		keyinter = 0;
		}
	if (!keydelay && keyinter || keydelay && !keyinter)
		ConfAddError("You must set both of the following in order for them to take effect:\n", "KEYDELAY\nKEYINTER");
	char *wpStr = ConfGetString("WP");
	if (*wpStr)
		if (!getWP(wpStr))
			ConfAddError("Invalid WP= parameters\n", wpStr);
	if (!xyVersion&&!wsVersion&&wpVersion<=0)
		{
		showital = false;
		showsubp = false;
		}
	swapmod=false;
	scroll=0;
	char *wheelmod = lTrim(ConfGetString("wheelmod"));
	if (strlen(wheelmod)&&!getWheelmod(wheelmod))
		{
		ConfAddError("Invalid WHEELMOD= parameters\n", lTrim(ConfGetString("wheelmod")));
		wheelmodv=1;
		wheelmodh=2;
		}
	mstate=false;
	char *clickmod = lTrim(ConfGetString("clickmod"));
	if (strlen(clickmod)&&!getClickmod(clickmod))
		{
		ConfAddError("Invalid CLICKMOD= parameters\n", lTrim(ConfGetString("clickmod")));
		clickmodl=1;
		clickmodr=4;
		}
	if (!getPadding(lTrim(ConfGetString("padding"))))
		{
		ConfAddError("Invalid PADDING= parameters\n", lTrim(ConfGetString("padding")));
		padding=0;
		padcolor=0;
		}
	int scalex=0,scaley=0;
	bool warn=false;
	if (!getScale(lTrim(ConfGetString("scale")),&scalex,&scaley,&warn))
		{
		ConfAddError(warn?"The scale factor setting is too high for screen resolution\n":"Invalid SCALE= parameters\n", lTrim(ConfGetString("scale")));
		window.scalex=-max(1,min(9,getMaxScale(&scalex, &scaley, true)));
		window.scaley=10;
		}
	else
		{
		if (warn) ConfAddError("The specified scale factor may not work well for screen resolution\n", lTrim(ConfGetString("scale")));
		window.scalex=scalex;
		window.scaley=scaley;
		}
	rgbColors = altBGR1;
	char * colors = ConfGetString("colors");
	if (!strnicmp(colors, "mono", 4))												// Mono, "Hercules" mode?
		{
		colors += 4;
		lTrim(colors);
		if (*colors == ',' || *colors == 0)
			{
			initialvMode = 7;
			if (*colors == ',')
				{
				colors += 1;
				lTrim(colors);
				}
			}
		}
	if (!setColors(colors,-1))
		{
		setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);	// Standard DOS colors
		if (*colors)
			{
			initialvMode = 3;
			ConfAddError("Invalid COLORS= parameters\n", colors);
			}
		}
	char * winDef = ConfGetString("window");
	bool f=false;
	if (!setWinInitial(winDef, &f))
		ConfAddError("Invalid WINDOW= parameters\n", winDef);
	int maxWidth = 0, maxHeight = 0;
	getMaxScale(&maxWidth, &maxHeight, screen>0);
	evensize = ConfGetBool("evensize");
	if (!accWin(maxWidth, maxHeight))
		E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);

	window.framed = true;
	HMENU hSysMenu = GetSystemMenu(vDosHwnd, FALSE);

	RemoveMenu(hSysMenu, SC_SIZE, MF_BYCOMMAND);									// Remove some useless items
	RemoveMenu(hSysMenu, SC_RESTORE, MF_BYCOMMAND);
	//ModifyMenu(hSysMenu, SC_MAXIMIZE, MF_BYCOMMAND, WM_SC_SYSMENUMAX, "Maximize (full-screen)\tAlt-Enter");
	
	AppendMenu(hSysMenu, MF_SEPARATOR, NULL, "");
    AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUPCOPY,		"Copy all text to Windows clipboard\t(Win+)Ctrl+A");
    AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUPDUMP,		"Copy screen text to and open file\t(Win+)Ctrl+C");
    AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUPASTE,		"Paste text from Windows clipboard\t(Win+)Ctrl+V");
	AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUDECREASE,	"Decrease font/window size\tWin+F11");
	AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUINCREASE,	"Increase font/window size\tWin+F12");
    AppendMenu(hSysMenu, MF_SEPARATOR, NULL, "");
    AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUNOTES,		"Show notes...\t(Win+)Ctrl+N");
    AppendMenu(hSysMenu, MF_STRING, WM_SC_SYSMENUABOUT,		"About...");

    fnDefaultWndProc = (WNDPROC)GetWindowLongPtr(vDosHwnd, GWLP_WNDPROC);
    SetWindowLongPtr(vDosHwnd, GWLP_WNDPROC, (LONG_PTR)&SysMenuExtendWndProc);
	RECT rect;
	GetWindowRect(vDosHwnd, &rect);
	if (topwin) SetWindowPos(vDosHwnd, HWND_TOPMOST, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
	return;
	}


static int winMovingX, winMovingY = 0, prescr = 0, incaps = 0, calock = 0, nulock = 0, sclock = 0, nucount = 0, cacount = 0, sccount = 0;
static bool winMoving = false, kbset = false, prevfocus = true, lfocus = true, rfocus = true, ftime = false;
extern bool keystate;
DWORD lasttime = 0;
userAction buAct;

void toggleLock()
	{
	if (!kbset)
		{
		kbset=true;
		GetKeyboardState(kbState);
		if (kbState[VK_SCROLL] & 1)
			Mem_aStosb(BIOS_KEYBOARD_FLAGS1,Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x10);
		if (kbState[VK_NUMLOCK] & 1)
			Mem_aStosb(BIOS_KEYBOARD_FLAGS1,Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x20);
		if (kbState[VK_CAPITAL] & 1)
			Mem_aStosb(BIOS_KEYBOARD_FLAGS1,Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x40);
		}
	if (!keystate||!hasFocus)
		{
		prevfocus = hasFocus;
		return;
		}
	if (keystate&&hasFocus&&!prevfocus)
		{
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,kbState[VK_SCROLL] & 1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x10 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x10);
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,kbState[VK_NUMLOCK] & 1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x20 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x20);
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,kbState[VK_CAPITAL] & 1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x40 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x40);
		prevfocus = hasFocus;
		return;
		}
	prevfocus = hasFocus;
	if (sccount>10)
		{
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,sclock==1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x10 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x10);
		sclock=sccount=0;
		}
	else if (!(Mem_aLodsb(BIOS_KEYBOARD_FLAGS1) & 0x10) != !(kbState[VK_SCROLL] & 1) && !(kbState[VK_SCROLL] & 0x80))
		{
		if (sclock!=kbState[VK_SCROLL]+1)
			{
			sclock=kbState[VK_SCROLL]+1;
			sccount++;
			}
		else
			sclock=sccount=0;
		keybd_event( VK_SCROLL, 0x46, KEYEVENTF_EXTENDEDKEY | 0, 0);
		keybd_event( VK_SCROLL, 0x46, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
	if (nucount>10)
		{
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,nulock==1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x20 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x20);
		nulock=nucount=0;
		}
	else if (!(Mem_aLodsb(BIOS_KEYBOARD_FLAGS1) & 0x20) != !(kbState[VK_NUMLOCK] & 1) && !(kbState[VK_NUMLOCK] & 0x80))
		{
		if (nulock!=kbState[VK_NUMLOCK]+1)
			{
			nulock=kbState[VK_NUMLOCK]+1;
			nucount++;
			}
		else
			nulock=nucount=0;
		keybd_event( VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
		keybd_event( VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
	if (incaps>0)
		;
	else if (cacount>10)
		{
		Mem_aStosb(BIOS_KEYBOARD_FLAGS1,calock==1 ? Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)|0x40 : Mem_aLodsb(BIOS_KEYBOARD_FLAGS1)&~0x40);
		calock=cacount=0;
		}
	else if (!(Mem_aLodsb(BIOS_KEYBOARD_FLAGS1) & 0x40) != !(kbState[VK_CAPITAL] & 1) && !(kbState[VK_CAPITAL] & 0x80))
		{
		if (kbState[VK_CAPITAL] & 1) incaps=1;
		if (calock!=((Mem_aLodsb(BIOS_KEYBOARD_FLAGS1) & 0x42) == 0x42 ? 3 : kbState[VK_CAPITAL]+1))
			{
			calock=(Mem_aLodsb(BIOS_KEYBOARD_FLAGS1) & 0x42) == 0x42 ? 3 : kbState[VK_CAPITAL]+1;
			cacount++;
			}
		else
			calock=cacount=0;
		keybd_event( VK_CAPITAL, 0x3A, KEYEVENTF_EXTENDEDKEY | 0, 0);
		keybd_event( VK_CAPITAL, 0x3A, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
		}
	}

void firstWin()
	{
	firstmax = false;
	if (!ConfGetBool("frame"))
		window.framed=false;
	if (winPerc == 100 || sstate == 1)
		{
		maxWin();
		if (!window.framed)
			tobeframe=true;
		}
	else if (!ttf.fullScrn)
		{
		RECT rect;
		BOOL res=GetWindowRect(vDosHwnd, &rect);
		resetWin();
		HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		if (res && initialY != -1)
			{
			rect.top+=info.rcMonitor.top;
			rect.left+=info.rcMonitor.left;
			}
		int maxWidth=0, maxHeight=0;
		getMaxScale(&maxWidth, &maxHeight, false);
		if (!res || initialY == -1 || rect.left-info.rcMonitor.left+window.width > maxWidth || rect.top-info.rcMonitor.top+window.height > maxHeight)
			{
			rect.top=info.rcMonitor.top+(maxHeight-window.height)/2;
			rect.left=info.rcMonitor.left+(maxWidth-window.width)/2;
			}
		SetWindowPos(vDosHwnd, NULL, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
		}
	if (sstate == 2)
		ShowWindow(vDosHwnd, SW_MINIMIZE);
	}

void HandleUserActions()
	{
	toggleLock();
	userAction uAct;
	bool enter=bkey&&keydelay>0&&keyinter>0, p=false;
	while ((p=vPollEvent(&uAct))||enter)
		{
		enter=false;
		if (!newAttrChar)															// Don't process uActs as long not initialized
			continue;
		if (bkey&&keydelay>0&&keyinter>0)
			{
			if (p&&uAct.type==WM_KEYDOWN)
				{
				if (uAct.scancode!=buAct.scancode)
					bkey=false;
				else
					continue;
				}
			if (uAct.type!=WM_KEYUP&&GetTickCount()>=lasttime+keyinter+(ftime?keydelay:0))
				{
				uAct=buAct;
				ftime=false;
				}
			else if (uAct.type==WM_CLOSE||uAct.type==WM_KEYDOWN&&!(p&&uAct.scancode!=buAct.scancode))
				continue;
			}
		if (!p&&!bkey) continue;
		if (firstmax) firstWin();
		switch (uAct.type)
			{
		case WM_MOUSEWHEEL:
			if (!mouseHidden&&autoHide) while (ShowCursor(true)<=0);
			if (uAct.x==8&&(vga.mode!=M_TEXT||!ttf.fullScrn))
				{
				if (GetWindowRect(vDosHwnd, &prevPosition))						// Save position and point size
					prevPointSize = ttf.pointsize;
				RECT rect;
				if (uAct.y<=32768)
					{
					increaseFontSize();
					if (prevPointSize&&GetWindowRect(vDosHwnd, &rect)&&(rect.right-rect.left-prevPosition.right+prevPosition.left||rect.bottom-rect.top-prevPosition.bottom+prevPosition.top))
						MoveWindow(vDosHwnd, prevPosition.left-(rect.right-rect.left-prevPosition.right+prevPosition.left)/2, prevPosition.top-(rect.bottom-rect.top-prevPosition.bottom+prevPosition.top)/2, rect.right-rect.left, rect.bottom-rect.top, false);
					}
				else
					{
					decreaseFontSize();
					if (prevPointSize&&GetWindowRect(vDosHwnd, &rect)&&(prevPosition.right-prevPosition.left-rect.right+rect.left||prevPosition.bottom-prevPosition.top-rect.bottom+rect.top))
						MoveWindow(vDosHwnd, prevPosition.left+(prevPosition.right-prevPosition.left-rect.right+rect.left)/2, prevPosition.top+(prevPosition.bottom-prevPosition.top-rect.bottom+rect.top)/2, rect.right-rect.left, rect.bottom-rect.top, false);
					}
				}
			else if ((swapmod?wheelmodh:wheelmodv)>0&&(uAct.x==0||uAct.x==4))
				{
				int mode=swapmod?wheelmodh:wheelmodv;
				INPUT ip = {0};
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = (mode==1||mode==4)?(uAct.y<=32768?72:80):(mode==2||mode==5)?(uAct.y<=32768?75:77):(mode==3||mode==6)?(uAct.y<=32768?73:81):(uAct.y<=32768?17:44);
				if (mode>3)
					{
					if (scroll) break;
					scroll=1;
					}
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				bkey = false;
				}
			break;
		case WM_MOUSEHWHEEL:
			if (!mouseHidden&&autoHide) while (ShowCursor(true)<=0);
			if ((swapmod?wheelmodv:wheelmodh)>0&&(uAct.x==0||uAct.x==4))
				{
				int mode=swapmod?wheelmodv:wheelmodh;
				INPUT ip = {0};
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = (mode==1||mode==4)?(uAct.y>32768?72:80):(mode==2||mode==5)?(uAct.y>32768?75:77):(mode==3||mode==6)?(uAct.y>32768?73:81):(uAct.y>32768?17:44);
				if (mode>3)
					{
					if (scroll) break;
					scroll=2;
					}
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				bkey = false;
				}
			break;
		case WM_MOUSEMOVE:
			if (!mouseHidden&&autoHide) while (ShowCursor(true)<=0);
			if (selectingText)
				{
				RECT selBox1 = {selPosX1*ttf.charWidth+ttf.offX%ttf.charWidth, selPosY1*ttf.charHeight+ttf.offY%ttf.charHeight, (selPosX2+1)*ttf.charWidth+ttf.offX%ttf.charWidth, (selPosY2+1)*ttf.charHeight+ttf.offY%ttf.charHeight};
				int newX = (uAct.x-hPadPixs-ttf.offX%ttf.charWidth)/ttf.charWidth;
				if (newX>65535) newX=0;
				selPosX1 = min(selStartX, newX);
				selPosX2 = max(selStartX, newX);
				selPosX1 = max(selPosX1, (int)(ttf.offX/ttf.charWidth+0.5));
				selPosX2 = min(selPosX2, ttf.offX/ttf.charWidth+ttf.cols-1);
				int newY = (uAct.y-ttf.offY%ttf.charHeight)/ttf.charHeight;
				selPosY1 = min(selStartY, newY);
				selPosY2 = max(selStartY, newY);
				selPosY1 = max(selPosY1, (int)(ttf.offY/ttf.charHeight+0.5));
				selPosY2 = min(selPosY2, ttf.offY/ttf.charHeight+ttf.lins-1);
				RECT selBox2 = {selPosX1*ttf.charWidth+ttf.offX%ttf.charWidth, selPosY1*ttf.charHeight+ttf.offY%ttf.charHeight, (selPosX2+1)*ttf.charWidth+ttf.offX%ttf.charWidth, (selPosY2+1)*ttf.charHeight+ttf.offY%ttf.charHeight};
				if (memcmp(&selBox1, &selBox2, sizeof(RECT)))						// Update selected block if needed
					{
					HDC hDC = GetDC(vDosHwnd);
					selBox1.left += hPadPixs;
					selBox1.right += hPadPixs;
					InvertRect(hDC, &selBox1);
					selBox2.left += hPadPixs;
					selBox2.right += hPadPixs;
					InvertRect(hDC, &selBox2);
					ReleaseDC(vDosHwnd, hDC);
					}
				break;
				}
			if (!hasFocus)
				break;
			if (winMoving)
				{
				if (uAct.x != winMovingX || uAct.y != winMovingY)
					{
					RECT rect;
					GetWindowRect(vDosHwnd, &rect);
					// Repaint true due to Windows XP
					MoveWindow(vDosHwnd, rect.left+uAct.x-winMovingX, rect.top+uAct.y-winMovingY, rect.right-rect.left, rect.bottom-rect.top, TRUE);
					}
				break;
				}
			if (vga.mode == M_TEXT)
				{
				if (!window.framed || ttf.fullScrn)
					if ((uAct.y < ttf.charHeight) != hasMinButton)
						{
						hasMinButton = !hasMinButton;
						if (hasMinButton) clearbutt = 0;
						if (sysicons)
							for (int i=1;i<7&&(sysicons>2||i<4);i++)
								curAttrChar[ttf.cols-i] = newAttrChar[ttf.cols-i]^0xffff;
						}
				if (usesMouse)
					{
					if (uAct.x < ttf.offX || uAct.x >= (window.width-(hPadding? ttf.charWidth : 0)) - ttf.offX || uAct.y < ttf.offY || uAct.y >= window.height - ttf.offY)
						break;
					uAct.x -= ttf.offX+hPadPixs;
					uAct.y -= ttf.offY;
					Mouse_Moved(uAct.x, uAct.y, ttf.charWidth, ttf.charHeight);
					}
				}
			else if (usesMouse)
				Mouse_Moved(uAct.x, vgafixms?(Bit16u)(uAct.y/2.24+0.5):uAct.y, abs(window.scalex), window.scaley == 10 ? abs(window.scalex):abs(window.scaley));
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			mstate=false;
			if (!mouseHidden&&autoHide) while (ShowCursor(true)<=0);
			swapmod=uAct.x&1||uAct.x&2;
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			mstate=true;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			{
			if (!mouseHidden&&autoHide) while (ShowCursor(true)<=0);
			if (selectingText)														// End selecting text
				{
				vUpdateWin();														// To restore the window
				ClipCursor(NULL);													// Free mouse
				if (OpenClipboard(NULL))
					{
					if (EmptyClipboard())
						{
						selPosX1-=ttf.offX/ttf.charWidth;
						selPosX2-=ttf.offX/ttf.charWidth;
						selPosY1-=ttf.offY/ttf.charHeight;
						selPosY2-=ttf.offY/ttf.charHeight;
						HGLOBAL hCbData = GlobalAlloc(NULL, 2*(selPosY2-selPosY1+1)*(selPosX2-selPosX1+3)-2);
						Bit16u* pChData = (Bit16u*)GlobalLock(hCbData);
						if (pChData)
							{
							int dataIdx = 0;
							for (int line = selPosY1; line <= selPosY2; line++)
								{
								Bit16u *curAC = curAttrChar + line*ttf.cols + selPosX1;
								int lastNotSpace = dataIdx;
								for (int col = selPosX1; col <= selPosX2; col++)
									{
									Bit8u textChar =  *(curAC++)&255;
									pChData[dataIdx++] = cpMap[textChar];
									if (textChar != 32)
										lastNotSpace = dataIdx;
									}
								dataIdx = lastNotSpace;
								if (line != selPosY2)								// Append line feed for all bur the last line 
									{
									pChData[dataIdx++] = 0x0d;
									pChData[dataIdx++] = 0x0a;
									}
								curAC += ttf.cols;
								}
							pChData[dataIdx] = 0;
							SetClipboardData(CF_UNICODETEXT, hCbData);
							GlobalUnlock(hCbData);
							}
						}
					CloseClipboard();
					}
				selectingText = false;
				return;
				}
			// [Win][Ctrl+Left mouse button starts block select for clipboard copy
			if (vga.mode == M_TEXT && uAct.type == WM_LBUTTONDOWN && (uAct.flags1&4) && (uAct.flags&2 || !winKey && !(uAct.flags1&1) && !(uAct.flags1&2)) && uAct.x >= (int)hPadPixs + ttf.offX && uAct.x < ttf.cols*ttf.charWidth + (int)hPadPixs + ttf.offX && uAct.y >= ttf.offY && uAct.y < ttf.lins*ttf.charHeight + ttf.offY)
				{
				selStartX = selPosX1 = selPosX2 = (uAct.x-hPadPixs-ttf.offX%ttf.charWidth)/ttf.charWidth;
				selStartY = selPosY1 = selPosY2 = (uAct.y-ttf.offY%ttf.charHeight)/ttf.charHeight;
				RECT selBox = {selPosX1*ttf.charWidth+ttf.offX%ttf.charWidth, selPosY1*ttf.charHeight+ttf.offY%ttf.charHeight, selPosX2*ttf.charWidth+ttf.charWidth+ttf.offX%ttf.charWidth, selPosY2*ttf.charHeight+ttf.charHeight+ttf.offY%ttf.charHeight};
				selBox.left += hPadPixs;
				selBox.right += hPadPixs;
				HDC hDC = GetDC(vDosHwnd);
				InvertRect(hDC, &selBox);
				ReleaseDC(vDosHwnd, hDC);

				RECT rcClip;														// Restrict mouse to window
				GetClientRect(vDosHwnd, &rcClip);									// NB we need the client area
				POINT pt;															// So some translation for ClipCursor
				pt.x = pt.y = 0;
				ClientToScreen(vDosHwnd, &pt);
				rcClip.left += pt.x+hPadPixs;
				rcClip.top += pt.y;
				rcClip.right += pt.x+hPadPixs-(hPadding?ttf.charWidth:0);
				rcClip.bottom += pt.y;
				ClipCursor(&rcClip);
				selectingText = true;
				break;
				}
			else if (uAct.type == WM_RBUTTONDOWN && (uAct.flags1&4) && (uAct.flags&2 || !winKey && !(uAct.flags1&1) && !(uAct.flags1&2)))
				getClipboard();
			if (winMoving)
				{
				if (mouseHidden&&autoHide) while (ShowCursor(false)>=0);
				winMoving = false;
				saveScreen(prescr);
				}
			int height = window.framed && !ttf.fullScrn || !sysicons || vga.mode != M_TEXT ? -1 : ttf.charHeight/(sysicons>2?1:2);
			if (uAct.type == WM_LBUTTONDOWN)										//Minimize?
				{
				if ((!window.framed || ttf.fullScrn) && uAct.y < ttf.charHeight || uAct.flags1&4 && !(uAct.flags&2) && (winKey || uAct.flags1&1 || uAct.flags1&2))
					{
					if (uAct.y <= height && uAct.x >= window.width-ttf.charWidth*(sysicons>2?2:1))
						{
						::SendMessage(vDosHwnd, WM_CLOSE, NULL, NULL);
						mstate=false;
						break;
						}
					else if (uAct.y <= height && uAct.x >= window.width-ttf.charWidth*(sysicons>2?4:2))
						{
						ttf.fullScrn?minWin():maxWin();
						mstate=false;
						break;
						}
					else if (uAct.y <= height && uAct.x >= window.width-ttf.charWidth*(sysicons>2?6:3))
						{
						ShowWindow(vDosHwnd, SW_MINIMIZE);
						mstate=false;
						break;
						}
					if (mouseHidden&&autoHide) while (ShowCursor(true)<=0);
					if (vga.mode != M_TEXT || !ttf.fullScrn)
						{
						prescr=abs(screen);
						winMoving = true;
						winMovingX = uAct.x;
						winMovingY = uAct.y;
						}
					}
				}
			else if (uAct.type == WM_RBUTTONDOWN && (vga.mode != M_TEXT || !ttf.fullScrn) && uAct.flags1&4 && !(uAct.flags&2) && (winKey || uAct.flags1&1 || uAct.flags1&2))
				{
				HMONITOR monitor = MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
				MONITORINFO info;
				info.cbSize = sizeof(MONITORINFO);
				GetMonitorInfo(monitor, &info);
				RECT rect;
				BOOL cur=GetWindowRect(vDosHwnd, &rect);
				if (cur && rect.right-rect.left <= info.rcMonitor.right-info.rcMonitor.left && rect.bottom-rect.top <= info.rcMonitor.bottom-info.rcMonitor.top)
					MoveWindow(vDosHwnd, info.rcMonitor.left+(info.rcMonitor.right-info.rcMonitor.left-rect.right+rect.left)/2, info.rcMonitor.top+(info.rcMonitor.bottom-info.rcMonitor.top-rect.bottom+rect.top)/2, rect.right-rect.left, rect.bottom-rect.top, true);
				}
			if (usesMouse&&!(vga.mode == M_TEXT&&(uAct.x < ttf.offX || uAct.x >= window.width - ttf.offX || uAct.y < ttf.offY || uAct.y >= window.height - ttf.offY)))
				{
				switch (uAct.type)
					{
				case WM_LBUTTONDOWN:
					Mouse_ButtonPressed(0);
					break;
				case WM_RBUTTONDOWN:
					Mouse_ButtonPressed(1);
					break;
				case WM_LBUTTONUP:
					Mouse_ButtonReleased(0);
					break;
				case WM_RBUTTONUP:
					Mouse_ButtonReleased(1);
					break;
					}
				}
			if (uAct.type==WM_LBUTTONDOWN&&clickmodl) lfocus=hasFocus;
			if (uAct.type==WM_RBUTTONDOWN&&clickmodr) rfocus=hasFocus && !(uAct.y <= height && uAct.x >= window.width-ttf.charWidth*(sysicons>2?6:3));
			if (mstate&&!(uAct.flags1&4)&&((!usesMouse||clickmodl<0)&&uAct.type==WM_LBUTTONUP&&clickmodl&&lfocus||(!usesMouse||clickmodr<0)&&uAct.type==WM_RBUTTONUP&&clickmodr&&rfocus))
				{
				int mode=uAct.type==WM_LBUTTONUP?abs(clickmodl):abs(clickmodr);
				INPUT ip = {0};
				ip.type = INPUT_KEYBOARD;
				ip.ki.wScan = mode==1?28:mode==2?57:mode==3?15:mode==4?1:14;
				ip.ki.time = 0;
				ip.ki.dwExtraInfo = 0;
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
				SendInput(1, &ip, sizeof(INPUT));
				bkey = false;
				}
			if (uAct.type==WM_LBUTTONUP||uAct.type==WM_RBUTTONUP) mstate=false;
			break;
			}
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
			bkey = false;
			if (autoHide) while (ShowCursor(true)<=0);
			if (topwin)
				{
				RECT rect;
				GetWindowRect(vDosHwnd, &rect);
				SetWindowPos(vDosHwnd, uAct.type==WM_SETFOCUS?HWND_TOPMOST:HWND_NOTOPMOST, rect.left, rect.top, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOCOPYBITS);
				if (uAct.type==WM_KILLFOCUS) BringWindowToTop(GetForegroundWindow());
				}
			if (uAct.type==WM_SETFOCUS&&autoHide&&mouseHidden) while (ShowCursor(false)>=0);
			break;
		case WM_CLOSE:
			{
			for (Bit8u handle = 0; handle < DOS_FILES; handle++)
				if (Files[handle] && (Files[handle]->GetInformation()&0x8000) == 0)
					if (MessageBox(vDosHwnd, "It is unsafe to exit right now because one or more files are open.\n\nAre you sure to exit vDosPlus anyway now?", "Exit vDosPlus warning", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING)==IDYES) {
						for (Bit8u handle = 0; handle < DOS_FILES; handle++)
							if (Files[handle] && (Files[handle]->GetInformation()&0x8000) == 0)
								{
								Files[handle]->Close();
								if (Files[handle]->RemoveRef() <= 0)
									{
									delete Files[handle];
									Files[handle] = 0;
									}
								}
						break;
					} else
						return;
			VideoQuit();
			exit(0);
			break;
			}
		case WM_KEYDOWN:
			if (!bkey&&uAct.scancode!=29&&uAct.scancode!=42&&uAct.scancode!=54&&uAct.scancode!=56&&!(uAct.flags&2)) {
				bkey=true;
				buAct=uAct;
				ftime=true;
			}
			lasttime=GetTickCount();
		case WM_KEYUP:
			if (incaps==1&&uAct.type==WM_KEYUP&&uAct.scancode==58)
				{
				if (kbState[VK_CAPITAL] & 1)
					{
					incaps=2;
					keybd_event( VK_LSHIFT, 0x2A, KEYEVENTF_EXTENDEDKEY | 0, 0);
					keybd_event( VK_LSHIFT, 0x2A, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
					}
				else
					incaps=0;
				}
			else if (incaps==2&&uAct.type==WM_KEYUP&&uAct.scancode==42)
				incaps=0;
			if (autoHide&&!(uAct.flags1&4)) while (ShowCursor(false)>=0);
			if (uAct.type==WM_KEYUP)
			{
				bkey=false;
				ftime=false;
			}
			// [Left or Right Alt][Enter] switches from window to "fullscreen"
		    if (uAct.type == WM_KEYDOWN && uAct.scancode == 28 && uAct.flags1&0x08)
				{
				ttf.fullScrn?minWin():maxWin();
				break;
				}
			if (uAct.type == WM_KEYDOWN)
				{
				if (uAct.flags&2 || !winKey && !(uAct.flags1&1) && !(uAct.flags1&2))	// Win key down
					{
					// [Win][Ctrl]+C dumps screen to file screen.txt and opens it with the application (default notepad) assigned to .txt files
					if ((uAct.flags1&4) && uAct.scancode == 46)
						{
						dumpScreen();
						break;
						}
					// [Win][Ctrl]+V pastes clipboard into keyboard buffer
					if ((uAct.flags1&4) && uAct.scancode == 47)
						{
						getClipboard();
						break;
						}
					// [Win][Ctrl]+N opens user notes file
					if ((uAct.flags1&4) && uAct.scancode == 49)
						{
						showNotes();
						break;
						}
					// [Win][Ctrl]+A copies screen text into clipboard
					if ((uAct.flags1&4) && uAct.scancode == 30)
						{
						copyScreen();
						break;
						}
					// [Win][Ctrl]+S starts spooled print job
					if ((uAct.flags1&4) && uAct.scancode == 31)
						{
						if (spoolPRT != -1)
							{
							spoolPRT = -1;
							SetWindowText(vDosHwnd, vDosCaption);				// Restore window title
							}
						break;
						}
	 				// Window/font size modifiers
					}
				if ((vga.mode!=M_TEXT||!ttf.fullScrn) && (uAct.flags&2))
					{
					if (uAct.scancode == 87)									// F11
						{
						decreaseFontSize();
						break;
						}
					if (uAct.scancode == 88)									// F12
						{
						increaseFontSize();
						break;
						}
					}
				}
			if ((uAct.flags1&4 && uAct.scancode == 46 || (uAct.flags1&4 || uAct.flags&1) && uAct.scancode == 70) && uAct.type == WM_KEYDOWN && dos.breakcheck)
				DOS_BreakFlag = true;
			Mem_aStosb(BIOS_KEYBOARD_FLAGS1, uAct.flags1);						// Update keyboard flags
			Mem_aStosb(BIOS_KEYBOARD_FLAGS2, uAct.flags2);
			Mem_aStosb(BIOS_KEYBOARD_FLAGS3, uAct.flags3);
			Mem_aStosb(BIOS_KEYBOARD_LEDS, uAct.leds);
			bool bioskey = true;
			if (keymode && Mem_Lodsb(4 * 9 + 3) != 0xf0)
			{
				bioskey=false;
				if (keymode==2 && !(uAct.flags & 1)) {
					// if they are letters
					if ( (uAct.unicode >= 97 && uAct.unicode <= 122) && (uAct.flags1 == 65 || uAct.flags1 == 66 || uAct.flags1 == 97 || uAct.flags1 == 98) || (uAct.unicode >= 65 && uAct.unicode <= 90) && ( uAct.flags1 == 64 || uAct.flags1 == 96) )
							bioskey=true;
					// if they are keypad keys (/ - +) 
					else if (uAct.type == WM_KEYDOWN && ( (uAct.scancode == 53 || uAct.scancode == 74) || uAct.scancode == 78) )
							bioskey=true;
					// if they are keypad keys (789 456 1230)
					else if (uAct.type == WM_KEYDOWN && ( (uAct.scancode >= 71 && uAct.scancode <= 73) || (uAct.scancode >= 75 && uAct.scancode <= 77) || (uAct.scancode >= 79 && uAct.scancode <= 83)) && !((uAct.flags1&0x08) || (uAct.flags1&0x04) || (uAct.flags1&0x3)) )
							bioskey=true;
				}
			}
			int mode=(scroll==1&&swapmod||scroll==2&&!swapmod)?wheelmodh:wheelmodv;
			bool isscan = (mode==4&&(uAct.scancode==72||uAct.scancode==80))||(mode==5&&(uAct.scancode==75||uAct.scancode==77))||(mode==6&&(uAct.scancode==73||uAct.scancode==81))||(mode==7&&(uAct.scancode==17||uAct.scancode==44));
			if (scroll&&isscan)
				{
				uAct.flags1=4;
				uAct.flags2=1;
				uAct.flags=0;
				uAct.unicode=0;
				if (uAct.type==WM_KEYUP) scroll=0;
				}
			if (bioskey||scroll&&isscan)
				BIOS_AddKey(uAct.flags1, uAct.flags2, uAct.flags, uAct.scancode, uAct.unicode, uAct.type == WM_KEYDOWN);
			else
				KEYBOARD_AddKey(uAct.flags, uAct.scancode, uAct.type == WM_KEYDOWN);
			break;
			}
		}
	}


static FILE *LogFile;

void vpLog(char const *format, ...) {
    if (LogFile != NULL) {
        va_list args;
        va_start(args, format);
        vfprintf(LogFile, format, args);
        va_end(args);
        fprintf(LogFile, "\n");
        fflush(LogFile);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	LPSTR cmdline=lpCmdLine;
	while (*cmdline==' ') cmdline++;
	if (!stricmp(cmdline, "/log")||!strnicmp(cmdline, "/log ", 5))
		{
		LogFile = fopen("vDosPlus.log", "w");
		cmdline+=4;
		}
	while (*cmdline==' ') cmdline++;
	char *e,*p;
	vpLog("vDosPlus version: %s build: %s", vDosVersion, vDosBuild);
	if (!strnicmp(cmdline, "/cfg ", 5))
		{
		cmdline+=5;
		while (*cmdline==' ') cmdline++;
		if (*cmdline=='"')
			{
			cmdline++;
			p=strstr(cmdline,"\" ");
			if (p==NULL&&*(cmdline+strlen(cmdline)-1)=='\"')
				*(cmdline+strlen(cmdline)-1)=0;
			}
		else
			p=strchr(cmdline,' ');
		if (p!=NULL)
			*p=0;
		if (!strlen(lrTrim(cmdline))||strlen(lrTrim(cmdline))>255)
			strcpy(configfile,"nul");
		else
			strcpy(configfile,lrTrim(cmdline));
		if (stricmp(configfile,"CONFIG.TXT"))
			vpLog("Using configuration file %s",configfile);
		if (p!=NULL)
			{
			*p=' ';
			while (*p==' ') p++;
			cmdline=p;
			}
		else
			strcpy(cmdline,"");
		}
	while (*cmdline==' ') cmdline++;
	if (!strnicmp(cmdline, "/set ", 5))
		{
		cmdline+=5;
		char line[300], msg[330], *cfgline=line;
		bool rem = false;
		for (int i=0;i<MAXNAMES;i++)
			{
			p=strchr(cmdline,';');
			if (p!=NULL) *p=0;
			if (strlen(lrTrim(cmdline))>299)
				{
				strncpy(cfgline,lrTrim(cmdline),299);
				cfgline[299]=0;
				}
			else
				strcpy(cfgline,lrTrim(cmdline));
			if (!rem&&*cfgline&&!strnicmp(cfgline,"rem",3)&&(cfgline[3]==32||cfgline[3]==9)&&!strnicmp(lTrim(cfgline+4),"vdosplus:",9))
				cfgline=lTrim(lTrim(cfgline+4)+9);
			if ((strlen(cfgline)>3&&!strncmp(cfgline,"#[",2)||rem&&strlen(cfgline)>1)&&cfgline[strlen(cfgline)-2]==']'&&cfgline[strlen(cfgline)-1]=='#')
				rem = false;
			else if (strlen(cfgline)>1&&!strncmp(cfgline,"#[",2))
				rem = true;
			else if (!rem&&*cfgline&&cfgline[0]!='#'&&!(!strnicmp(cfgline,"rem",3)&&(cfgline[3]==0||cfgline[3]==32||cfgline[3]==9)))
				{
				e=strchr(cfgline,'=');
				if (e!=NULL)
					*e=0;
				if (e==NULL||!strlen(lrTrim(cfgline))||strlen(lrTrim(cfgline))>LENNAME||strlen(lrTrim(e+1))>255)
					{
					if (e!=NULL) *e='=';
					sprintf(msg, "Ignoring invalid directive - %s",cfgline);
					MessageBox(NULL, msg, "Error in command line", MB_OK|MB_ICONWARNING);
					}
				else
					{
					strcpy(cfgname[i],lrTrim(cfgline));
					strcpy(cfgval[i],lrTrim(e+1));
					}
				}
			if (p!=NULL)
				{
				*p=';';
				cmdline=p+1;
				}
			else
				break;
			}
		}
	try
		{
		if (*lpCmdLine&&*lpCmdLine!='/')
			SetEnvironmentVariable("VDOSPLUS",lpCmdLine);
		InitWindow();																// Init window
		TTF_Init();																	// Init TTF
		vDos_Init();
		}
	catch (char * error)
		{
		MessageBox(NULL, error, "vDosPlus - Exception", MB_OK|MB_ICONSTOP);
		}
	VideoQuit();
	return 0;
	}
