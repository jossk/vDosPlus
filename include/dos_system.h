// Wengier: LFN support
#pragma once
#ifndef vDOS_DOS_SYSTEM_H
#define vDOS_DOS_SYSTEM_H

//#include <vector>
#include "vdos.h"
#include "support.h"
#include "mem.h"

#define DOS_NAMELENGTH 12
#define DOS_NAMELENGTH_ASCII (DOS_NAMELENGTH+1)
#define DOS_FCBNAME 15
#define DOS_PATHLENGTH 255
#define LFN_NAMELENGTH 255

enum {
    DOS_ATTR_READ_ONLY = 0x01,
    DOS_ATTR_HIDDEN = 0x02,
    DOS_ATTR_SYSTEM = 0x04,
    DOS_ATTR_VOLUME = 0x08,
    DOS_ATTR_DIRECTORY = 0x10,
    DOS_ATTR_ARCHIVE = 0x20,
    DOS_ATTR_DEVICE = 0x40
};

class DOS_DTA;

class DOS_File {
public:
    DOS_File() : flags(0) {
        name = 0;
        refCtr = 0;
        hdrive = 0xff;
    };

    DOS_File(const DOS_File &orig);

//	DOS_File & operator= (const DOS_File & orig);
    virtual    ~DOS_File() { if (name) delete[] name; };

    virtual bool Read(Bit8u *data, Bit16u *size) = 0;

    virtual bool Write(Bit8u *data, Bit16u *size) = 0;

    virtual bool Seek(Bit32u *pos, Bit32u type) = 0;

    virtual void Close() {};

    virtual bool LockFile(Bit8u mode, Bit32u pos, Bit32u size) { return false; };

    virtual Bit16u GetInformation(void) = 0;

    virtual void SetName(const char *_name) {
        if (name) delete[] name;
        name = new char[strlen(_name) + 1];
        strcpy(name, _name);
    }

//	virtual char*	GetName(void)				{ return name; };
    virtual bool IsName(const char *_name) {
        if (!name) return false;
        return stricmp(name, _name) == 0;
    };

    virtual void AddRef() { refCtr++; };

    virtual Bits RemoveRef() { return --refCtr; };

    virtual void UpdateDateTimeFromHost() {}

    virtual bool UpdateHostDateTime(Bit16u ntime, Bit16u ndate, int type) { return false; }

    void SetDrive(Bit8u drv) { hdrive = drv; }

    Bit8u GetDrive(void) { return hdrive; }

    Bit32u flags;
    Bit16u atime;
    Bit16u adate;
    Bit16u ctime;
    Bit16u cdate;
    Bit16u time;
    Bit16u date;
    Bit16u attr;
    Bits refCtr;
    char *name;
private:
    Bit8u hdrive;
};

class DOS_Device : public DOS_File {
public:
    DOS_Device(const DOS_Device &orig) : DOS_File(orig) {
        devnum = orig.devnum;
    }

    DOS_Device &operator=(const DOS_Device &orig) {
        DOS_File::operator=(orig);
        devnum = orig.devnum;
        return *this;
    }

    DOS_Device() : DOS_File(), devnum(0) {};

    virtual bool Read(Bit8u *data, Bit16u *size);

    virtual bool Write(Bit8u *data, Bit16u *size);

    virtual bool Seek(Bit32u *pos, Bit32u type);

    virtual void Close();

    virtual Bit16u GetInformation(void);

    void SetDeviceNumber(Bitu num) {
        devnum = num;
    }

    Bitu GetDeviceNumber(void) {
        return devnum;
    }

    Bit64u timeOutAt;
    bool ffWasLast;
private:
    Bitu devnum;
};

class Disk_File : public DOS_File {
public:
    Disk_File(const char *name, HANDLE handle);

    bool Read(Bit8u *data, Bit16u *size);

    bool Write(Bit8u *data, Bit16u *size);

    bool Seek(Bit32u *pos, Bit32u type);

    void Close();

    bool LockFile(Bit8u mode, Bit32u pos, Bit32u size);

    Bit16u GetInformation(void);

    void UpdateDateTimeFromHost();

    bool UpdateHostDateTime(Bit16u ntime, Bit16u ndate, int type);

private:
    HANDLE hFile;
};

class DOS_Drive {
public:
    DOS_Drive(const char *startdir, Bit8u driveNo, bool use = false);

    virtual ~DOS_Drive() {};

    void SetBaseDir(const char *startdir);

    bool FileCreate(DOS_File **file, char *name, Bit16u attributes);

    bool FileOpen(DOS_File **file, char *name, Bit32u flags);

    bool FileExists(const char *name);

    bool FileUnlink(char *_name, bool wildcard);

    bool FindFirst(char *_dir, DOS_DTA &dta, int handle);

    bool SetFileAttr(char *name, Bit16u attr);

    bool GetFileAttr(char *name, Bit16u *attr);

    bool GetFileAttrEx(char *name, LPVOID fad);

    bool GetVolumeInfo(char *name, char *volume_label, DWORD *serial_number);

    bool GetFreeDiskSpace(struct _diskfree_t *df);

    bool GetDiskFreeSpace32(char *name, DWORD *sectors_per_cluster, DWORD *bytes_per_sector, DWORD *free_clusters,
                            DWORD *total_clusters);

    DWORD GetCompressedSize(char *name);

    HANDLE CreateOpenFile(char const *const name);

    bool MakeDir(char *_dir);

    bool RemoveDir(char *_dir);

    bool Rename(char *oldname, char *newname);

    bool TestDir(const char *dir);

    char *GetWinDir(void) { return basedir; };

    char *GetLabel(void) { return label; };
    char basedir[MAX_PATH_LEN];
    Bit32u VolSerial;
    char curdir[DOS_PATHLENGTH];
    char label[14];
    bool remote;
    bool autouse;
};

enum {
    OPEN_READ = 0, OPEN_WRITE = 1, OPEN_READWRITE = 2, DOS_NOT_INHERIT = 128
};
enum {
    DOS_SEEK_SET = 0, DOS_SEEK_CUR = 1, DOS_SEEK_END = 2
};

bool DoFindNext(int handle);

void closeWinSeachByPSP(Bit16u psp);

/*
 A multiplex handler should read the registers to check what function is being called
 If the handler returns false dos will stop checking other handlers
*/

typedef bool (MultiplexHandler)(void);

// AddDevice stores the pointer to a created device
void DOS_AddDevice(DOS_Device *adddev);

bool WildFileCmp(const char *file, const char *wild);

#endif
