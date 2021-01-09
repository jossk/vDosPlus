// Wengier: LFN support
#include "stdafx.h"

#include "vdos.h"
#include "dos_system.h"
//#include "Shlwapi.h"
#include "dos_inc.h"
#include "support.h"
//#include <sys/stat.h>
//#include <time.h>
//#include <direct.h>
//#include <fcntl.h>
//#include <stdlib.h>
//#include <shellapi.h>

bool fini=true;
extern bool filter83;

bool WildFileCmp(const char * file, const char * wild) 
	{
	char file_name[9];
	char file_ext[4];
	char wild_name[9];
	char wild_ext[4];
	const char * find_ext;
	Bitu r;

	strcpy(wild_name, strcpy(file_name, "        "));
	strcpy(wild_ext, strcpy(file_ext, "   "));

	find_ext = strrchr(file, '.');
	if (find_ext)
		{
		Bitu size = (Bitu)(find_ext-file);
		if (size > 8)
			size = 8;
		memcpy(file_name, file, size);
		find_ext++;
		memcpy(file_ext, find_ext, (strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
		}
	else
		memcpy(file_name, file, (strlen(file) > 8) ? 8 : strlen(file));
	upcase(file_name);
	upcase(file_ext);
	find_ext = strrchr(wild, '.');
	if (find_ext)
		{
		Bitu size = (Bitu)(find_ext-wild);
		if (size > 8)
			size = 8;
		memcpy(wild_name, wild,size);
		find_ext++;
		memcpy(wild_ext, find_ext, (strlen(find_ext)>3) ? 3 : strlen(find_ext));
		}
	else
		memcpy(wild_name, wild, (strlen(wild) > 8) ? 8 : strlen(wild));
	upcase(wild_name);
	upcase(wild_ext);
	// Names are right do some checking
	r = 0;
	while (r <8)
		{
		if (wild_name[r] == '*')
			break;
		if (wild_name[r] != '?' && wild_name[r] != file_name[r])
			return false;
		r++;
		}
    r = 0;
	while (r < 3)
		{
		if (wild_ext[r] == '*')
			return true;
		if (wild_ext[r] != '?' && wild_ext[r] != file_ext[r])
			return false;
		r++;
		}
	return true;
	}

DOS_Drive::DOS_Drive(const char* startdir, Bit8u driveNo, bool use)
	{
	curdir[0] = 0;
	label[0] = driveNo+'A';
	strcpy(label+1, "_DRIVE");
	SetBaseDir(startdir);
	autouse=use;
	while (Bit8u c = *startdir++)
		VolSerial = c + (VolSerial << 6) + (VolSerial << 16) - VolSerial;
	Mem_Stosb(dWord2Ptr(dos.tables.mediaid)+driveNo*2, 0xF8);						// Set the correct media byte in the table (harddisk)
	}

void DOS_Drive::SetBaseDir(const char* startdir)
	{
	strcpy(basedir, startdir);
	if (basedir[strlen(basedir)-1] != '\\')
		strcat(basedir, "\\");

	remote = true;
	if (startdir[0] != '\\')														// Assume \\... is remote
		{
		char rootDir[] = " :\\";
		rootDir[0] = startdir[0];
		if (GetDriveType(rootDir)!=DRIVE_REMOTE)
			remote = false;
		}
	vpLog("%c: => %s) %s", *label, remote ? "(Remote" : " (Local", basedir);
	}

bool DOS_Drive::FileCreate(DOS_File * * file, char * name, Bit16u attr)
	{
	char win_name[MAX_PATH_LEN];

	int attribs = FILE_ATTRIBUTE_NORMAL;
	if (attr&7)																		// Read-only (1), Hidden (2), System (4) are the same in DOS and Windows
		attribs = FILE_ATTRIBUTE_READONLY;
	if (*name&&name[strlen(name)-1]==13) name[strlen(name)-1]=0;
	strcat(strcpy(win_name, basedir), name);
	HANDLE handle = CreateFile(win_name, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, attribs, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		{
		Bit32u errorno = GetLastError();
		DOS_SetError((Bit16u)errorno);
		vpLog("File creation failed: %s\nErrorno: %d", win_name, errorno);
		return false;
		}
	*file = new Disk_File(name, handle);
	(*file)->flags = OPEN_READWRITE;
	return true;
	}

bool DOS_Drive::FileOpen(DOS_File * * file, char * name, Bit32u flags)
	{
	if (*name&&name[strlen(name)-1]==13) name[strlen(name)-1]=0;
	int ohFlag, shhFlag;

	switch (flags&0xf)
		{
	case OPEN_READ:
		ohFlag = GENERIC_READ;
		break;
	case OPEN_WRITE:
		ohFlag = GENERIC_WRITE;
		break;
	case OPEN_READWRITE:
		ohFlag = GENERIC_READ|GENERIC_WRITE;
		break;
	default:
		DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		return false;
		}
	switch (flags&0x70)
		{
	case 0x10:
		shhFlag = 0;
		break;
	case 0x20:
		shhFlag = FILE_SHARE_READ;
		break;
	case 0x30:
		shhFlag = FILE_SHARE_WRITE;
		break;
	default:
		shhFlag = FILE_SHARE_READ|FILE_SHARE_WRITE;
		}
	char win_name[MAX_PATH_LEN];
	int len = strlen(name);
	if (!stricmp(name, "AUTOEXEC.BAT"))												// Redirect it to vDosPlus' autoexec.txt
		strcpy(curauto?win_name:strrchr(strcpy(win_name, _pgmptr), '\\')+1, autoexec);
	else if (!stricmp(name, "4DOS.HLP")
		|| (len > 8 && !strnicmp(name+len-9, "\\4DOS.HLP", 9)))						// Redirect it to that in vDosPlus' folder
		strcpy(strrchr(strcpy(win_name, _pgmptr), '\\'), "\\4DOS.HLP");
	else if (fini && (!stricmp(name, "4DOS.INI")
		|| (len > 8 && !strnicmp(name+len-9, "\\4DOS.INI", 9))))						// Redirect it to that in vDosPlus' folder
		{
		fini=false;
		strcpy(strrchr(strcpy(win_name, _pgmptr), '\\'), "\\4DOS.INI");
		}
	else
		strcat(strcpy(win_name, basedir), name);
	HANDLE handle = CreateFile(win_name, ohFlag, shhFlag, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (handle == INVALID_HANDLE_VALUE)
		{
		DOS_SetError((Bit16u)GetLastError());
		return false;
		}
	*file = new Disk_File(name, handle);
	(*file)->flags = flags;															// For the inheritance flag and maybe check for others.
	return true;
	}

bool DOS_Drive::SetFileAttr(char* name, Bit16u attr)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	if (!SetFileAttributes(win_name, attr))
		{
		DOS_SetError((Bit16u)GetLastError());
		return false;
		}
	return true;
	}

bool DOS_Drive::GetFileAttr(char* name, Bit16u* attr)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	Bit32u attribs = GetFileAttributes(win_name);
	if (attribs == INVALID_FILE_ATTRIBUTES)
		{
		DOS_SetError((Bit16u)GetLastError());
		return false;
		}
	*attr = attribs&0x3f;															// Only return lower 6 bits
	return true;
	}

bool DOS_Drive::GetFileAttrEx(char* name, LPVOID fad)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	if (!GetFileAttributesEx(win_name, GetFileExInfoStandard, fad))
		{
		DOS_SetError((Bit16u)GetLastError());
		return false;
		}
	return true;
	}

bool DOS_Drive::GetVolumeInfo(char* name, char* volume_label, DWORD *serial_number)
	{
	char win_name[MAX_PATH_LEN], Drive[] = "A:\\";

	strcat(strcpy(win_name, basedir), name);
	Drive[0]=*win_name;

	if (!GetVolumeInformation(Drive, volume_label, MAX_PATH, serial_number, NULL, NULL, NULL, 0))
		{
		DOS_SetError((Bit16u)GetLastError());
		return false;
		}
	return true;
	}

bool DOS_Drive::GetFreeDiskSpace(struct _diskfree_t* df)
	{
	char win_name[MAX_PATH_LEN];

	strcpy(win_name, basedir);
	return win_name[0]=='\\' ? NULL : _getdiskfree(toupper(win_name[0])-'A'+1, df) == 0;
	}

bool DOS_Drive::GetDiskFreeSpace32(char* name, DWORD* sectors_per_cluster, DWORD* bytes_per_sector, DWORD* free_clusters, DWORD* total_clusters)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	return GetDiskFreeSpace(win_name, sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters)?true:false;
	}

DWORD DOS_Drive::GetCompressedSize(char* name)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	DWORD size = GetCompressedFileSize(win_name, NULL);
	if (size != INVALID_FILE_SIZE)
		{
		if (size != 0 && size == GetFileSize(win_name, NULL))
			{
			DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
			if (GetDiskFreeSpace(win_name, &sectors_per_cluster, &bytes_per_sector, &free_clusters, &total_clusters))
				size = ((size - 1) | (sectors_per_cluster * bytes_per_sector - 1)) + 1;
			}
		return size;
		}
	else
		{
		DOS_SetError((Bit16u)GetLastError());
		return -1;
		}
	}

HANDLE DOS_Drive::CreateOpenFile(const char* name)
	{
	char win_name[MAX_PATH_LEN];

	strcat(strcpy(win_name, basedir), name);
	HANDLE handle=CreateFile(win_name, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle==INVALID_HANDLE_VALUE)
		DOS_SetError((Bit16u)GetLastError());
	return handle;
	}

bool DOS_Drive::FileExists(const char* name)
	{
	char win_name[MAX_PATH_LEN];

	Bit32u attribs = GetFileAttributes(strcat(strcpy(win_name, basedir), name));
	return (attribs != INVALID_FILE_ATTRIBUTES && !(attribs&FILE_ATTRIBUTE_DIRECTORY));
	}

bool DOS_Drive::FileUnlink(char * name, bool wildcard)
	{
	char win_name[MAX_PATH_LEN];
	strcat(strcpy(win_name, basedir), name);
	if (DeleteFile(win_name))
		return true;
	else if (wildcard)
		{
		SHFILEOPSTRUCT op={0};
		op.wFunc = FO_DELETE;
		strcat(win_name, "\0");
		op.pFrom = win_name;
		op.pTo = NULL;
		op.fFlags = FOF_FILESONLY | FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NORECURSION;
		int err=SHFileOperation(&op);
		if (err) DOS_SetError(err);
		return !err;
		}
	DOS_SetError((Bit16u)GetLastError());
	return false;
	}

#define maxWinSearches 512															// Should be ennough!
static struct {																		// To keep track of open Window searches
	PhysPt	dta_pt;																	// DTA address
	Bit16u	psp;																	// PSP segment of program initiating search
	int 	handle;																	// LFN search handle
	HANDLE	sHandle;																// Search handle
	bool	root;																	// Search is at root DOS drive (no "." or "..")
	bool	wildcard;																// Search has wildcard
} WinSearches[maxWinSearches];

static int openWinSearches = 0;														// Number of active Windows serach handles
static char srch_dir[MAX_PATH_LEN];													// Windows search directory

static void closeWinSeach(DOS_DTA &dta, int handle)
	{
	PhysPt dta_Addr = dta.GetPt();
	for (int i = 0; i < openWinSearches; i++)
		{
		if (WinSearches[i].dta_pt == dta_Addr && WinSearches[i].handle == -1 || WinSearches[i].handle == handle && handle != -1)	// Match found
			{
			if (WinSearches[i].sHandle != INVALID_HANDLE_VALUE)						// If search started, close handle
				FindClose(WinSearches[i].sHandle);
			openWinSearches--;
			if (i < openWinSearches)												// Compact WinSearches down
				memmove(&WinSearches[i], &WinSearches[i+1], sizeof(WinSearches[1])*(openWinSearches-i));
			return;
			}
		}
	}

void closeWinSeachByPSP(Bit16u psp)
	{
	for (int i = 0; i < openWinSearches; i++)
		if (WinSearches[i].psp == psp)												// Match found
			{
			if (WinSearches[i].sHandle != INVALID_HANDLE_VALUE)						// If serach started, close handle
				FindClose(WinSearches[i].sHandle);
			openWinSearches--;
			if (i < openWinSearches)												// Compact WinSearches down
				memmove(&WinSearches[i], &WinSearches[i+1], sizeof(WinSearches[1])*(openWinSearches-i));
			return;
			}
	}

bool DOS_Drive::FindFirst(char* _dir, DOS_DTA & dta, int handle)
	{
	closeWinSeach(dta, handle);														// Close Windows search handle if entry for this dta
	if (!TestDir(_dir))
		{
		if (uselfn) for (int i=0;i<(signed)strlen(_dir);i++) _dir[i]=tolower(_dir[i]);
		if (!uselfn||!TestDir(_dir))
			{
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
			}
		}
	strcat(strcpy(srch_dir, basedir), _dir);										// Should be on a per program base, but only used shortly so for now OK
	if (srch_dir[strlen(srch_dir)-1] != '\\')
		strcat(srch_dir, "\\");
	
	Bit8u sAttr, rAttr;
	char srch_pattern[LFN_NAMELENGTH+1];
	dta.GetSearchParams(handle, sAttr, rAttr, srch_pattern);
	if (!strcmp(srch_pattern, "        .   ")) {										// Special complete empty, what about abc. ? (abc.*?)
		strcat(srch_dir, "*.*");
	}
	else
		{
		int j = strlen(srch_dir), k=j;
 		for (int i = 0; i < (signed)strlen(srch_pattern); i++)								// Pattern had 8.3 format with embedded spaces, for now simply remove them ( "abc d   .a b"?)
			if (srch_pattern[i] != 0)
				srch_dir[j++] = srch_pattern[i];
		srch_dir[j]=0;
		if (uselfn&&!PathFileExistsA(srch_dir))
			{
			j=k;
			for (int i = 0; i < (signed)strlen(srch_pattern); i++)
				if (srch_pattern[i] != 0)
					srch_dir[j++] = tolower(srch_pattern[i]);
			if (!PathFileExistsA(srch_dir))
				{
				j=k;
				for (int i = 0; i < (signed)strlen(srch_pattern); i++)
					if (srch_pattern[i] != 0)
						srch_dir[j++] = srch_pattern[i];
				}
			}
		}
	// Windows "finds" LPT1-9/COM1-9 which are never returned in a (DOS)DIR
	if (((!strnicmp(srch_pattern, "LPT", 3) || !strnicmp(srch_pattern, "COM", 3)) && (srch_pattern[3] > '0' && srch_pattern[3] <= '9' && srch_pattern[4] ==' '))
		|| ((!strnicmp(srch_pattern, "PRN", 3) || !strnicmp(srch_pattern, "AUX", 3)) && srch_pattern[3] ==' '))
		{
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
		}
	DOS_SetError(DOSERR_NONE);
	char Drive[] = "C:\\", Label[MAX_PATH], VolLabel[14];
	DWORD fill;
	Drive[0]=*basedir;
	if (GetVolumeInformation(Drive[0]=='\\'?basedir:Drive,Label,sizeof(Label),NULL,&fill,&fill,NULL,0))
		{
		strncpy(VolLabel,Label,13);
		VolLabel[13]=0;
		}
	else {
		VolLabel[0] = dta.GetSearchDrive(handle)+'A';
		strcpy(VolLabel+1, "_DRIVE");
		}
	if (sAttr&DOS_ATTR_VOLUME && !strlen(VolLabel) && !(sAttr&DOS_ATTR_DIRECTORY && sAttr&DOS_ATTR_ARCHIVE))
		{
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;		
		}
	if ((sAttr&DOS_ATTR_VOLUME) && WildFileCmp(VolLabel, srch_pattern))
		{
		dta.SetResult(handle, VolLabel, VolLabel, 0, 0, 0, 0, 0, 0, 0, 0, DOS_ATTR_VOLUME);
		if (!(sAttr&DOS_ATTR_DIRECTORY && sAttr&DOS_ATTR_ARCHIVE))
			return true;
		}
	if (!(sAttr&(DOS_ATTR_VOLUME)))
	{
		// Remove
		int fLen = strlen(srch_dir);
		if (fLen > 11 && !stricmp("\\4HELP.EXE", srch_dir+fLen-10))					// Global check for "4help.exe" to start by 4DOS help function
			{
			dta.SetResult(handle, "4HELP.EXE", "4HELP.EXE", 0, 16384, 0, 0, 0, 0, 0, 0, DOS_ATTR_ARCHIVE);
			return true;
			}
		if ((fLen > 11 && !stricmp("\\COMMAND.*", srch_dir+fLen-10))
			|| (fLen > 13 && !stricmp("\\COMMAND.COM", srch_dir+fLen-12)))			// Global check for "COMMAND.COM"
			{
			dta.SetResult(handle, "COMMAND.COM", "COMMAND.COM", 0, 16384, 0, 0, 0, 0, 0, 0, DOS_ATTR_ARCHIVE);
			return true;
			}
		if ((fLen > 14 && !stricmp("\\AUTOEXEC.BAT", srch_dir+fLen-13)))
			{
			dta.SetResult(handle, "AUTOEXEC.BAT", "AUTOEXEC.BAT", 0, 16384, 0, 0, 0, 0, 0, 0, DOS_ATTR_ARCHIVE);
			return true;
			}
		}
	if (openWinSearches == maxWinSearches)											// We'll need a new Windows search handle
		E_Exit("Maximum number of Windows search handles exceeded");				// Shouldn't happen of course
	WinSearches[openWinSearches].dta_pt = dta.GetPt();
	WinSearches[openWinSearches].psp = dos.psp();
	WinSearches[openWinSearches].handle = handle;	
	WinSearches[openWinSearches].sHandle = INVALID_HANDLE_VALUE;
	WinSearches[openWinSearches].root = !strncmp(basedir, srch_dir, strrchr(srch_dir, '\\')-srch_dir);
	WinSearches[openWinSearches].wildcard = (strpbrk(srch_dir, "?*") != NULL);
	openWinSearches++;
	bool r=DoFindNext(handle);
	return r;
	}

bool isDosName(char* fName)															// Check for valid DOS filename, 8.3 and no forbidden characters
	{
	if (!strcmp(fName, ".") || !strcmp(fName, ".."))								// "." and ".." specials
		return true;
	if (strpbrk(fName, "+[] "))
		return false;
	char* pos = strchr(fName, '.');
	if (pos)																		// If extension
		{
		if (strlen(pos) > 4 || strchr(pos+1, '.') || pos - fName > 8)				// Max 3 chars,  max 1 "extension", max name = 8 chars
			return false;
		}
	else if (strlen(fName) > 8)														// Max name = 8 chars
		return false;
	return true;
	}

bool DoFindNext(int handle)
	{
	Bit8u srch_attr, rattr;
	char srch_pattern[LFN_NAMELENGTH+1];  // +1 added by Joe Caverly

	DOS_DTA dta(dos.dta());
	dta.GetSearchParams(handle, srch_attr, rattr, srch_pattern);
	srch_attr ^= (DOS_ATTR_VOLUME);
	WIN32_FIND_DATA search_data;
	Bit8u find_attr;
	PhysPt dtaAddress = dta.GetPt();

	DOS_SetError(DOSERR_NO_MORE_FILES);												// DOS returns this on error
	int winSearchEntry = -1;														// Is a FindFirst executed?
	for (int i = 0; i < openWinSearches; i++)
		if (WinSearches[i].dta_pt == dtaAddress && handle == -1 || WinSearches[i].handle == handle && handle != -1)
			winSearchEntry = i;
	if (handle == -1 && winSearchEntry == -1 && dtaAddress > 200000)				// Workaround the WordPerfect 6 QuickFinder issue
		for (int i = 0; i < openWinSearches; i++)
			if (WinSearches[i].dta_pt == dtaAddress - 4)
				winSearchEntry = i;
	if (winSearchEntry == -1)
		return false;
	if (WinSearches[winSearchEntry].sHandle == INVALID_HANDLE_VALUE)				// Actually a FindFirst continuation
		{
		if ((WinSearches[winSearchEntry].sHandle = FindFirstFile(srch_dir, &search_data)) == INVALID_HANDLE_VALUE)
			{
			closeWinSeach(dta, handle);												// Invalidate this entry/dta search
			DOS_SetError((Bit16u)GetLastError());
			if (dos.errorcode == DOSERR_FILE_NOT_FOUND)   // Windows returns this if not found
				DOS_SetError(DOSERR_NO_MORE_FILES);    // DOS returns this instead
			return false;
			}
		}
	else if (!FindNextFile(WinSearches[winSearchEntry].sHandle, &search_data))
		{
		closeWinSeach(dta, handle);													// Close this entry/dta search
		return false;
		}
	do
		{
		if (search_data.cFileName[0] == '.' && WinSearches[winSearchEntry].root)	// Don't show . and .. folders in vDos root drives
			continue;
		bool sfn = false;
		char *orgname=search_data.cFileName, *shortname;
		shortname=search_data.cAlternateFileName[0]==0?search_data.cFileName:search_data.cAlternateFileName;
		char s[2];
		strcpy(s,"?");
		if ((!uselfn&&filter83||!isDosName(shortname))&&!isDosName(orgname))		// If it's not a DOS 8.3 name
			if (uselfn&&!filter83&&handle!=-1)
				shortname=s;
			else if (!WinSearches[winSearchEntry].wildcard)
				sfn = true;															// We allow it only if Find was called to test the existance of a specific SFN name (no wildcards)
			else
				continue;
//		if (!(search_data.dwFileAttributes & srch_attr))
//			continue;
		//if (search_data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			//find_attr = DOS_ATTR_DIRECTORY;
		//else
			//find_attr = DOS_ATTR_ARCHIVE;
		find_attr = search_data.dwFileAttributes&0xff;
		if (uselfn && fflfn && handle > -1 && rattr && (find_attr&rattr) != rattr)
			continue;
 		if (~srch_attr & find_attr & DOS_ATTR_DIRECTORY)
			continue;
 		if (~srch_attr & find_attr & (DOS_ATTR_HIDDEN | DOS_ATTR_SYSTEM) && !(srch_attr & find_attr & DOS_ATTR_DIRECTORY && strchr(srch_pattern, '*') == NULL && strchr(srch_pattern, '?') == NULL))
			continue;
		// file is okay, setup everything to be copied in DTA Block
		char find_name[DOS_NAMELENGTH_ASCII], *lfind_name=orgname;
		Bit16u find_date,find_time,find_adate,find_atime,find_cdate,find_ctime;
		Bit32u find_hsize, find_size;
		memset(find_name, 0, DOS_NAMELENGTH_ASCII);
		strcpy(find_name, sfn ? srch_pattern : shortname);				// If LFN of a SFN was found, we can't use that, use the search pattern (has to be the SFN)
		upcase(find_name);
		find_hsize = search_data.nFileSizeHigh;
		find_size = search_data.nFileSizeLow;
		FILETIME fTime;
		FileTimeToLocalFileTime(&search_data.ftLastWriteTime, &fTime);
		FileTimeToDosDateTime(&fTime, &find_date, &find_time);
		FileTimeToLocalFileTime(&search_data.ftLastAccessTime, &fTime);
		FileTimeToDosDateTime(&fTime, &find_adate, &find_atime);
		FileTimeToLocalFileTime(&search_data.ftCreationTime, &fTime);
		FileTimeToDosDateTime(&fTime, &find_cdate, &find_ctime);
		dta.SetResult(handle, find_name, lfind_name, find_hsize, find_size, find_date, find_time, find_adate, find_atime, find_cdate, find_ctime, find_attr);

		if (!WinSearches[winSearchEntry].wildcard)									// If search w/o wildcards
			closeWinSeach(dta, handle);												// Close this entry/dta search
		DOS_SetError(DOSERR_NONE);
		return true;
		}
	while (FindNextFile(WinSearches[winSearchEntry].sHandle, &search_data));
	closeWinSeach(dta, handle);														// Close this entry/dta search
	return false;
	}

bool DOS_Drive::MakeDir(char * dir)
	{
	char win_dir[MAX_PATH_LEN];
	if (CreateDirectory(strcat(strcpy(win_dir, basedir), dir), NULL))
		return true;
	Bit16u error=(Bit16u)GetLastError();
	DOS_SetError(error==ERROR_ALREADY_EXISTS?DOSERR_ACCESS_DENIED:error);
	return false;
	}

bool DOS_Drive::RemoveDir(char * dir)
	{
	char win_dir[MAX_PATH_LEN];
	if (RemoveDirectory(strcat(strcpy(win_dir, basedir), dir)))
		return true;
	Bit16u error=(Bit16u)GetLastError();
	DOS_SetError((error==ERROR_DIRECTORY||error==ERROR_DIR_NOT_EMPTY)?DOSERR_ACCESS_DENIED:error);
	return false;
	}

bool DOS_Drive::Rename(char* oldname, char* newname)
	{
	char winold[MAX_PATH_LEN];
	char winnew[MAX_PATH_LEN];
	if (MoveFile(strcat(strcpy(winold, basedir), oldname), strcat(strcpy(winnew, basedir), newname)))
		return true;
	Bit16u error = (Bit16u)GetLastError();
	if (error == ERROR_ALREADY_EXISTS)												// Not kwnown by DOS
		error = DOSERR_ACCESS_DENIED;
	DOS_SetError(error);
	return false;
	}

bool DOS_Drive::TestDir(const char* dir)
	{
	char win_dir[MAX_PATH_LEN];
	strcat(strcpy(win_dir, basedir), dir);
	if (win_dir[strlen(win_dir)-1] != '\\')											// Make sure PathFileExists() only considers paths
		strcat(win_dir, "\\");
	if (PathFileExistsA(win_dir))
		return true;
	DOS_SetError((Bit16u)GetLastError());
	return false;
	}

bool Disk_File::Read(Bit8u* data, Bit16u* size)
	{
	if ((flags&0xf) == OPEN_WRITE)													// Check if file opened in write-only mode
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	Bit32u bytesRead;
	for (int tries = 3; tries; tries--)												// Try three times
		{
		if (ReadFile(hFile,static_cast<LPVOID>(data), (Bit32u)*size, &bytesRead, NULL))
			{
			*size = (Bit16u)bytesRead;
			if (bytesRead)															// Only if something is indeed read, skip the Idle function
				idleSkip = true;
			return true;
			}
		Sleep(25);																	// If failed (should be region locked), wait 25 millisecs
		}
	DOS_SetError((Bit16u)GetLastError());
	*size = 0;																		// Is this OK ??
	return false;
	}

bool Disk_File::Write(Bit8u* data, Bit16u* size)
	{
	if ((flags&0xf) == OPEN_READ)													// Check if file opened in read-only mode
		{
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
		}
	if (*size == 0)
		if (SetEndOfFile(hFile))
			return true;
		else
			{
			DOS_SetError((Bit16u)GetLastError());
			return false;
			}
	Bit32u bytesWritten;
	for (int tries = 3; tries; tries--)												// Try three times
		{
		if (WriteFile(hFile, data, (Bit32u)*size, &bytesWritten, NULL))
			{
			*size = (Bit16u)bytesWritten;
			idleSkip = true;
			return true;
			}
		Sleep(25);																	// If failed (should be region locked? (not documented in MSDN)), wait 25 millisecs
		}
	DOS_SetError((Bit16u)GetLastError());
	*size = 0;																		// Is this OK ??
	return false;
	}

bool Disk_File::LockFile(Bit8u mode, Bit32u pos, Bit32u size)
	{
	if (mode > 1)
		{
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	BOOL bRet = false;
	//auto lockFunct = mode == 0 ? ::LockFile : ::UnlockFile;
	for (int tries = 3; tries; tries--)												// Try three times
		{
		if ((bRet = mode == 0 ? ::LockFile(hFile, pos, 0, size, 0) : UnlockFile(hFile, pos, 0, size, 0)))
			return true;
		Sleep(25);																	// If failed, wait 25 millisecs
		}
	DOS_SetError((Bit16u)GetLastError());
	return false;
	}

bool Disk_File::Seek(Bit32u* pos, Bit32u type)
	{
	if (type > 2)
		{
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
		}
	Bit32s dwPtr = SetFilePointer(hFile, *pos, NULL, type);
	if (dwPtr != INVALID_SET_FILE_POINTER)											// If succes
		{
		*pos = (Bit32u)dwPtr;
		return true;
		}
	DOS_SetError((Bit16u)GetLastError());
	return false;
	}

void Disk_File::Close()
	{
	if (refCtr == 1)																// Only close if one reference left
		CloseHandle(hFile);
	}

Bit16u Disk_File::GetInformation(void)
	{
	return 0;
	}

Disk_File::Disk_File(const char* _name, HANDLE handle)
	{
	hFile = handle;
	UpdateDateTimeFromHost();

	attr = DOS_ATTR_ARCHIVE;

	name = 0;
	SetName(_name);
	}

void Disk_File::UpdateDateTimeFromHost()
	{
    FILETIME ftWrite, ftAccess, ftCreate;
	SYSTEMTIME stUTC, stLocal;
	atime = adate = 1;
	ctime = cdate = 1;
	time = date = 1;
	if (GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite))
		{
	    FileTimeToSystemTime(&ftCreate, &stUTC);									// Convert the creation time to local time
		SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
		SystemTimeToFileTime(&stLocal, &ftCreate);
		FileTimeToDosDateTime(&ftCreate, &cdate, &ctime);
	    FileTimeToSystemTime(&ftAccess, &stUTC);									// Convert the access time to local time
		SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
		SystemTimeToFileTime(&stLocal, &ftAccess);
		FileTimeToDosDateTime(&ftAccess, &adate, &atime);
	    FileTimeToSystemTime(&ftWrite, &stUTC);										// Convert the last-write time to local time
		SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
		SystemTimeToFileTime(&stLocal, &ftWrite);
		FileTimeToDosDateTime(&ftWrite, &date, &time);
		}
	return;
	}

#if (_MSC_VER == 1500)
	#define LL2FILETIME( ll, pft )\
		(pft)->dwLowDateTime = (UINT)(ll); \
		(pft)->dwHighDateTime = (UINT)((ll) >> 32);
	#define FILETIME2LL( pft, ll) \
		ll = (((LONGLONG)((pft)->dwHighDateTime))<<32) + (pft)-> dwLowDateTime ;

	static const int MonthLengths[2][12] =
	{
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

	static inline BOOL IsLeapYear(int Year)
	{
		return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0);
	}

	static int TIME_DayLightCompareDate( const SYSTEMTIME *date,
		const SYSTEMTIME *compareDate )
	{
		int limit_day, dayinsecs;

		if (date->wMonth < compareDate->wMonth)
			return -1; // We are in a month before the date limit.

		if (date->wMonth > compareDate->wMonth)
			return 1; // We are in a month after the date limit.

		// if year is 0 then date is in day-of-week format, otherwise
		// it's absolute date.
		if (compareDate->wYear == 0)
		{
			WORD First;
			// compareDate->wDay is interpreted as number of the week in the month
			// 5 means: the last week in the month
			int weekofmonth = compareDate->wDay;
			  // calculate the day of the first DayOfWeek in the month
			First = ( 6 + compareDate->wDayOfWeek - date->wDayOfWeek + date->wDay
				   ) % 7 + 1;
			limit_day = First + 7 * (weekofmonth - 1);
			// check needed for the 5th weekday of the month
			if(limit_day > MonthLengths[date->wMonth==2 && IsLeapYear(date->wYear)]
					[date->wMonth - 1])
				limit_day -= 7;
		}
		else
		{
		   limit_day = compareDate->wDay;
		}

		// convert to seconds
		limit_day = ((limit_day * 24  + compareDate->wHour) * 60 +
				compareDate->wMinute ) * 60;
		dayinsecs = ((date->wDay * 24  + date->wHour) * 60 +
				date->wMinute ) * 60 + date->wSecond;
		// and compare
		return dayinsecs < limit_day ? -1 :
			   dayinsecs > limit_day ? 1 :
			   0;   // date is equal to the date limit.
	}

	static DWORD TIME_CompTimeZoneID ( const TIME_ZONE_INFORMATION *pTZinfo,
		FILETIME *lpFileTime, BOOL islocal )
	{
		int ret, year;
		BOOL beforeStandardDate, afterDaylightDate;
		DWORD retval = TIME_ZONE_ID_INVALID;
		LONGLONG llTime = 0; // initialized to prevent gcc complaining
		SYSTEMTIME SysTime;
		FILETIME ftTemp;

		if (pTZinfo->DaylightDate.wMonth != 0)
		{
			// if year is 0 then date is in day-of-week format, otherwise
			// it's absolute date.
			if (pTZinfo->StandardDate.wMonth == 0 ||
				(pTZinfo->StandardDate.wYear == 0 &&
				(pTZinfo->StandardDate.wDay<1 ||
				pTZinfo->StandardDate.wDay>5 ||
				pTZinfo->DaylightDate.wDay<1 ||
				pTZinfo->DaylightDate.wDay>5)))
			{
				SetLastError(ERROR_INVALID_PARAMETER);
				return TIME_ZONE_ID_INVALID;
			}

			if (!islocal) {
				FILETIME2LL( lpFileTime, llTime );
				llTime -= pTZinfo->Bias * (LONGLONG)600000000;
				LL2FILETIME( llTime, &ftTemp)
				lpFileTime = &ftTemp;
			}

			FileTimeToSystemTime(lpFileTime, &SysTime);
			year = SysTime.wYear;

			if (!islocal) {
				llTime -= pTZinfo->DaylightBias * (LONGLONG)600000000;
				LL2FILETIME( llTime, &ftTemp)
				FileTimeToSystemTime(lpFileTime, &SysTime);
			}

			// check for daylight savings
			if(year == SysTime.wYear) {
				ret = TIME_DayLightCompareDate( &SysTime, &pTZinfo->StandardDate);
				if (ret == -2)
					return TIME_ZONE_ID_INVALID;

				beforeStandardDate = ret < 0;
			} else
				beforeStandardDate = SysTime.wYear < year;

			if (!islocal) {
				llTime -= ( pTZinfo->StandardBias - pTZinfo->DaylightBias )
					* (LONGLONG)600000000;
				LL2FILETIME( llTime, &ftTemp)
				FileTimeToSystemTime(lpFileTime, &SysTime);
			}

			if(year == SysTime.wYear) {
				ret = TIME_DayLightCompareDate( &SysTime, &pTZinfo->DaylightDate);
				if (ret == -2)
					return TIME_ZONE_ID_INVALID;

				afterDaylightDate = ret >= 0;
			} else
				afterDaylightDate = SysTime.wYear > year;

			retval = TIME_ZONE_ID_STANDARD;
			if( pTZinfo->DaylightDate.wMonth <  pTZinfo->StandardDate.wMonth ) {
				// Northern hemisphere
				if( beforeStandardDate && afterDaylightDate )
					retval = TIME_ZONE_ID_DAYLIGHT;
			} else    // Down south
				if( beforeStandardDate || afterDaylightDate )
				retval = TIME_ZONE_ID_DAYLIGHT;
		} else
			// No transition date
			retval = TIME_ZONE_ID_UNKNOWN;

		return retval;
	}

	static BOOL TIME_GetTimezoneBias( const TIME_ZONE_INFORMATION *pTZinfo,
		FILETIME *lpFileTime, BOOL islocal, LONG *pBias )
	{
		LONG bias = pTZinfo->Bias;
		DWORD tzid = TIME_CompTimeZoneID( pTZinfo, lpFileTime, islocal);

		if( tzid == TIME_ZONE_ID_INVALID)
			return FALSE;
		if (tzid == TIME_ZONE_ID_DAYLIGHT)
			bias += pTZinfo->DaylightBias;
		else if (tzid == TIME_ZONE_ID_STANDARD)
			bias += pTZinfo->StandardBias;
		*pBias = bias;
		return TRUE;
	}

	BOOL WINAPI TzSpecificLocalTimeToSystemTime(
		const TIME_ZONE_INFORMATION *lpTimeZoneInformation,
		LPSYSTEMTIME lpLocalTime, LPSYSTEMTIME lpUniversalTime)
	{
		FILETIME ft;
		LONG lBias;
		LONGLONG t;
		TIME_ZONE_INFORMATION tzinfo;

		if (lpTimeZoneInformation != NULL)
		{
			tzinfo = *lpTimeZoneInformation;
		}
		else
		{
			if (GetTimeZoneInformation(&tzinfo) == TIME_ZONE_ID_INVALID)
				return FALSE;
		}

		if (!SystemTimeToFileTime(lpLocalTime, &ft))
			return FALSE;
		FILETIME2LL( &ft, t)
		if (!TIME_GetTimezoneBias(&tzinfo, &ft, TRUE, &lBias))
			return FALSE;
		// convert minutes to 100-nanoseconds-ticks
		t += (LONGLONG)lBias * 600000000;
		LL2FILETIME( t, &ft)
		return FileTimeToSystemTime(&ft, lpUniversalTime);
	}
#endif

bool Disk_File::UpdateHostDateTime(Bit16u ntime, Bit16u ndate, int type)
	{
	FILETIME ftTime;
	SYSTEMTIME stUTC, stLocal;
	DosDateTimeToFileTime(ndate, ntime, &ftTime);
	FileTimeToSystemTime(&ftTime, &stLocal);
	TzSpecificLocalTimeToSystemTime(NULL, &stLocal, &stUTC);
	SystemTimeToFileTime(&stUTC, &ftTime);
	return SetFileTime(hFile, type==7?&ftTime:NULL, type==5?&ftTime:NULL, type==1?&ftTime:NULL)?true:false;
	}
