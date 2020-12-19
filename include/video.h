// Wengier: MULTI-MONITOR support
#pragma once

#ifndef _video_h
#define _video_h

//#include <windows.h>
#include "config.h"

typedef struct {
	Bit8u r;
	Bit8u g;
	Bit8u b;
	Bit8u unused;
} RGB_Color;

typedef struct {
	Bit32u	*colors;
	int		w, h;
	void	*pixels;
} vSurface;

typedef struct {
	bool		active;																// If this isn't set don't draw
	Bit32u		hideTill;
	bool		framed;
	int			scalex;
	int			scaley;
	Bit16u		width;
	Bit16u		height;
	vSurface	*surface;
} vWinSpecs;

// The main window
extern HWND vDosHwnd;

extern vWinSpecs window;
extern HBITMAP DIBSection;

// Function prototypes
void InitWindow(void);

void VideoQuit(void);

vSurface * SetVideoMode(RECT monrect, int width, int height, bool framed);
void HandleUserActions(void);
void SetVideoSize();

bool StartVideoUpdate(void);
void EndTextLines(void);
int SetCodePage(int cp);

vSurface * vCreateSurface(int width, int height, RGB_Color fg, RGB_Color bg);
void vUpdateRect(Bit32s x, Bit32s y, Bit32u w, Bit32u h);
void vUpdateWin(void);
void vBlitSurface(vSurface *src, RECT *srcrect,  RECT *dstrect);

LRESULT CALLBACK WinMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif

