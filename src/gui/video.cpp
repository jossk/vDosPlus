// Wengier: MULTI-MONITOR support
#include "stdafx.h"

#include "video.h"
#include "events.h"
#include "logging.h"
#include "support.h"

vWinSpecs window;
vSurface WinSurface;
extern HWND vDosHwnd;
extern int transwin;
HBITMAP DIBSection;

static LPSTR Appname = "vDosPlus";

void InitWindow(void)
	{
	WNDCLASS wc;
	HINSTANCE hInst;

	hInst = GetModuleHandle(NULL);
	memset((void *)&wc, 0, sizeof(WNDCLASS));

	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);								// Register the application class
	wc.lpszClassName	= Appname;
	wc.hInstance		= hInst;
	wc.lpfnWndProc		= WinMessage;
	wc.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);						// Black background, mostly for filling if "full-screen"
	if (RegisterClass(&wc))
		{
		vDosHwnd = CreateWindow(Appname, Appname,
			(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX),
			CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, NULL, NULL, hInst, NULL);
		if (vDosHwnd)
			return;
		}
	E_Exit("Could not create vDosPlus window");
	}

vSurface *SetVideoMode(RECT monrect, int width, int height, bool framed)
	{
	BITMAPINFO binfo;
	DWORD style;
	const DWORD directstyle = (WS_POPUP);
	const DWORD windowstyle = (WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
	HDC hdc;
	RECT bounds;

	// Fill in part of the video surface
	WinSurface.w = width;
	WinSurface.h = height;

	style = GetWindowLong(vDosHwnd, GWL_STYLE)&(~(windowstyle|directstyle));
	if (framed)
		style |= windowstyle;
	else
		style |= directstyle;
	SetWindowLong(vDosHwnd, GWL_STYLE, style);
	if (transwin)
		{
		SetWindowLong(vDosHwnd, GWL_EXSTYLE, GetWindowLong(vDosHwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetLayeredWindowAttributes(vDosHwnd, 0, 255*(100-transwin)/100, LWA_ALPHA);
		}
	else
		SetWindowLong(vDosHwnd, GWL_EXSTYLE, GetWindowLong(vDosHwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);

	// Delete the old bitmap if needed
	if (DIBSection != NULL)
		DeleteObject(DIBSection);

	memset(&(binfo.bmiHeader), 0, sizeof(BITMAPINFOHEADER));
	binfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	binfo.bmiHeader.biWidth = width;
	binfo.bmiHeader.biHeight = -height;												// -ve for topdown bitmap
	binfo.bmiHeader.biPlanes = 1;
	binfo.bmiHeader.biSizeImage = height*width*4;
	binfo.bmiHeader.biBitCount = 32;

	// Create the offscreen bitmap buffer
	hdc = GetDC(vDosHwnd);
	DIBSection = CreateDIBSection(hdc, &binfo, DIB_RGB_COLORS, (void **)(&WinSurface.pixels), NULL, 0);
	ReleaseDC(vDosHwnd, hdc);
	if (DIBSection == NULL)
		return NULL;

	// Resize the window
	bounds.left = bounds.top = 0;
	bounds.right = width;
	bounds.bottom = height;
	AdjustWindowRectEx(&bounds, style, false, 0);
	width = bounds.right-bounds.left;
	height = bounds.bottom-bounds.top;
	HMONITOR monitor = screen>0?MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST):MonitorFromWindow(vDosHwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO info;
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	SetWindowPos(vDosHwnd, NULL,
		(info.rcMonitor.right+info.rcMonitor.left-width)/2, (info.rcMonitor.bottom+info.rcMonitor.top-height)/2,	// Always center
		width, height, SWP_NOCOPYBITS|SWP_SHOWWINDOW);
	screen=-screen;
	return &WinSurface;
	}
	
// Cache color translations
#define NUM_GRAYS	256
#define maxColCache 32
struct
	{
	Bit32u fg;
	Bit32u bg;
	RGBQUAD colors[NUM_GRAYS];
	} cachedCols[maxColCache];
int cachedColsIdx = 0;
int cachedColsTot = 0;
	
// Create an empty RGB surface of the appropriate depth
vSurface * vCreateSurface(int width, int height, RGB_Color fg, RGB_Color bg)
	{
	static vSurface surface;
	static int prevAllocSize = 0;

	int toAlloc = height*width;
	if (surface.w != width || surface.h != height)
		{
		surface.w = width;
		surface.h = height;
		if (prevAllocSize < toAlloc)
			{
			if (surface.pixels)
				free(surface.pixels);
			surface.pixels = malloc(toAlloc);
			if (surface.pixels == NULL)
				return NULL;
			prevAllocSize = toAlloc;
			}
		}
	memset(surface.pixels, 0, toAlloc);												// Init to blank

	int idx;
	for (idx = 0; idx < cachedColsTot; idx++)										// Fill the palette with NUM_GRAYS levels of shading from bg to fg
		if (cachedCols[idx].fg == *(Bit32u *)&fg && cachedCols[idx].bg == *(Bit32u *)&bg)
			{
			surface.colors = (Bit32u *)cachedCols[idx].colors;
			break;
			}
	if (idx == cachedColsTot)
		{
		if (cachedColsTot < maxColCache)
			cachedColsTot++;
		int rdiff = fg.r-bg.r;
		int gdiff = fg.g-bg.g;
		int bdiff = fg.b-bg.b;

		for (int i = 0; i < NUM_GRAYS; ++i)
			{
			cachedCols[cachedColsIdx].colors[i].rgbRed = bg.r+(i*rdiff)/(NUM_GRAYS-1);
			cachedCols[cachedColsIdx].colors[i].rgbGreen = bg.g+(i*gdiff)/(NUM_GRAYS-1);
			cachedCols[cachedColsIdx].colors[i].rgbBlue = bg.b+(i*bdiff)/(NUM_GRAYS-1);
			}
		surface.colors = (Bit32u *)cachedCols[cachedColsIdx].colors;
		cachedCols[cachedColsIdx].fg = *(Bit32u *)&fg;
		cachedCols[cachedColsIdx].bg = *(Bit32u *)&bg;
		cachedColsIdx = (++cachedColsIdx)%maxColCache;
		}
	return &surface;
	}

// Update a specific portion of the physical screen
void vUpdateRect(Bit32s x, Bit32s y, Bit32u w, Bit32u h)
	{
	HDC hdc, mdc;

	hdc = GetDC(vDosHwnd);
	mdc = CreateCompatibleDC(hdc);
	SelectObject(mdc, DIBSection);
	BitBlt(hdc, x, y, w, h, mdc, x, y, SRCCOPY);
	DeleteDC(mdc);
	ReleaseDC(vDosHwnd, hdc);
	}

// Update screen
void vUpdateWin(void)
	{
	vUpdateRect(0, 0, window.width, window.height);
	}

//Update video buffer
void vBlitSurface (vSurface *src, RECT *srcrect, RECT *dstrect)
	{
	Bit8u *srcpix = (Bit8u *)src->pixels+srcrect->top*src->w;
	Bit32u *dstpix = (Bit32u *)((Bit8u *)window.surface->pixels+dstrect->top*window.surface->w*4+dstrect->left*4);
	int dstskip = window.surface->w-(srcrect->right-srcrect->left);

	for (int h = srcrect->bottom-srcrect->top; h; h--)
		{
		for (int w = srcrect->right-srcrect->left; w; --w)
			*dstpix++ =  *(Bit32u*)(&src->colors[*srcpix++]);
		dstpix += dstskip;
		}
	}

// Clean up the video subsystem
void VideoQuit (void)
	{
	if (vDosHwnd)
		{
		if (DIBSection)
			{
			DeleteObject(DIBSection);
			DIBSection = NULL;
			}
		DestroyWindow(vDosHwnd);
		vDosHwnd = NULL;
		}
	return;
	}

