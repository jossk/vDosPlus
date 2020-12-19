// Wengier: XMS 3.0 support
#pragma once

#ifndef __XMS_H__
#define __XMS_H__

Bit8u XMS_QueryFreeMemory(Bit32u& freeXMS);
Bit8u XMS_AllocateMemory(Bit32u size, Bit32u& handle);
Bit16u EMS_FreeKBs();

extern RealPt xms_callback;

#endif
