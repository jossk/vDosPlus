// Wengier: KEYBOARD support
#include "stdafx.h"

#include "vdos.h"
#include "inout.h"


static Bit8u dummy_read(Bitu /*port*/) {
    return 0;
}

static void dummy_write(Bitu /*port*/, Bitu /*val*/) {
}

#define KEYBUFSIZE 32
static struct {
    Bit8u buffer[KEYBUFSIZE];
    Bitu used;
    Bitu pos;


    Bit8u p60data;
    bool p60changed;
    bool active;
    bool scanning;
    bool scheduled;
} keyb;

static Bit8u read_p64(Bitu port) {
    return 0x1d;                                                                    // Just to get rid of of all these reads
}


static void KEYBOARD_SetPort60(Bit8u val) {
    keyb.p60changed = true;
    keyb.p60data = val;
    //PIC_ActivateIRQ(1);
    keyb_req = true;

}


void KEYBOARD_NextFromBuf() {
    if (keyb.used && !keyb.p60changed) {
        Bit8u val = keyb.buffer[keyb.pos];
        if (++keyb.pos >= KEYBUFSIZE) keyb.pos -= KEYBUFSIZE;
        keyb.used--;
        KEYBOARD_SetPort60(val);
    }

}

static void KEYBOARD_AddBuffer(Bit8u data) {
    if (keyb.used >= KEYBUFSIZE) {
        //LOG(LOG_KEYBOARD, LOG_NORMAL)("Buffer full, dropping code");
        return;
    }
    Bitu start = keyb.pos + keyb.used;
    if (start >= KEYBUFSIZE) start -= KEYBUFSIZE;
    keyb.buffer[start] = data;
    keyb.used++;

    KEYBOARD_NextFromBuf();

}


static Bit8u read_p60(Bitu port) {
    /*if (!keyb.p60changed)
        printf("Stale Read\n");
    else
        printf("Read\n");*/
    keyb.p60changed = false;
    keyb_req = false;
    return keyb.p60data;
}

void KEYBOARD_AddKey(Bit8u flags, Bit8u scancode, bool pressed) {
    Bit8u ret = scancode, extend = flags & 1;
    if (extend) KEYBOARD_AddBuffer(0xe0);
    if (!pressed) ret += 128;
    KEYBOARD_AddBuffer(ret);
}

static int latchval = 0, lsb = 1, msb = 0, pcsp = 0, latchk = 0;
DWORD WINAPI

ThreadFunc(void *data) {
    if (latchk) {
        msb = 1;
        Beep(1193182 / latchk, 3000);
    }
    return 0;
}

static void write_p61(Bitu port, Bitu val) {
    if (speaker) {
        pcsp = val & 3;
        if (!pcsp) {
            if (msb) {
                Sleep(100);
                Beep(30000, 1);
                msb = 0;
            }
            latchval = 0;
        } else if (latchval) {
            latchk = latchval;
            HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
        }
    }
}

static void write_p42(Bitu port, Bitu val) {
    if (speaker) {
        if (lsb)
            latchval = val;
        else {
            latchval |= (val << 8);
            if (pcsp && latchval) {
                latchk = latchval;
                HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
                latchval = 0;
            }
        }
        lsb ^= 1;
    }
}

void KEYBOARD_Init() {
    IO_RegisterWriteHandler(0x60, dummy_write);
    IO_RegisterWriteHandler(0x61, write_p61);
    IO_RegisterWriteHandler(0x64, dummy_write);
    IO_RegisterWriteHandler(0x42, write_p42);
    IO_RegisterReadHandler(0x60, keymode ? read_p60 : dummy_read);
    IO_RegisterReadHandler(0x61, dummy_read);
    IO_RegisterReadHandler(0x64, read_p64);
}
