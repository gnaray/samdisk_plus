#pragma once

#include "config.h"

#include "Disk.h"
#include "HDD.h"
#include "utils.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#ifndef _WIN32
#include <dirent.h>
#endif
#include <set>

#ifndef HIWORD
#define HIWORD(l)   static_cast<uint16_t>(static_cast<uint32_t>(l) >> 16)
#define LOWORD(l)   static_cast<uint16_t>(static_cast<uint32_t>(l))
#define HIBYTE(w)   static_cast<uint8_t>(static_cast<uint16_t>(w) >> 8)
#define LOBYTE(w)   static_cast<uint8_t>(static_cast<uint16_t>(w))
#endif


inline uint16_t tobe16(uint16_t u16)
{
    return (static_cast<uint16_t>((u16 & 0x00ff) << 8)) | (u16 >> 8);
}

inline uint16_t frombe16(uint16_t be16)
{
    return tobe16(be16);
}

inline uint32_t tobe32(uint32_t u32)
{
    return ((u32 & 0xff) << 24) | ((u32 & 0xff00) << 8) | ((u32 & 0xff0000) >> 8) | ((u32 & 0xff000000) >> 24);
}

inline uint32_t frombe32(uint32_t be)
{
    return tobe32(be);
}

template<size_t SIZE, typename T>
inline size_t array_size(T(&)[SIZE]) { return SIZE; }

#ifndef arraysize
template <typename T, size_t N>
char(&ArraySizeHelper(T(&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))
#define arrayintsizeof(array) (intsizeof(ArraySizeHelper(array)))
#endif

#define DAY ( \
  (__DATE__[4] == ' ' ? 0 : __DATE__[4]-'0') * 10 + \
  (__DATE__[5]-'0') \
)

#define MONTH ( \
  __DATE__[2] == 'n' ? (__DATE__[1] == 'a' ? 0 : 5) \
: __DATE__[2] == 'b' ? 1 \
: __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 2 : 3) \
: __DATE__[2] == 'y' ? 4 \
: __DATE__[2] == 'l' ? 6 \
: __DATE__[2] == 'g' ? 7 \
: __DATE__[2] == 'p' ? 8 \
: __DATE__[2] == 't' ? 9 \
: __DATE__[2] == 'v' ? 10 : 11 \
)

#define YEAR ( \
  (__DATE__[7]-'0') * 1000 + \
  (__DATE__[8]-'0') * 100 + \
  (__DATE__[9]-'0') * 10 + \
   __DATE__[10]-'0' \
)

#define YEAR_LAST_2_DIGITS ( \
  (__DATE__[9]-'0') * 10 + \
   __DATE__[10]-'0' \
)


#define USECS_PER_MINUTE    60000000

//const int MIN_TRACK_OVERHEAD = 32;        // 32 bytes of 0x4e at the start of a track
//const int MIN_SECTOR_OVERHEAD = 95;       // 22+12+3+1+6+22+8+3+1+1+16 (gapx, sync, idamsync, idam, id+idcrc, gap2, gap2sync, damsync, dam, ?, ?)

const int NORMAL_SIDES = 2;
const int NORMAL_TRACKS = 80;

const int DOS_SECTORS = 9;
const int DOS_TRACK_SIZE = DOS_SECTORS * SECTOR_SIZE;
const int DOS_DISK_SIZE = NORMAL_SIDES * NORMAL_TRACKS * DOS_TRACK_SIZE;

struct DFS_DIR
{
    uint8_t bLoadLow, bLoadHigh;        // Load address
    uint8_t bExecLow, bExecHigh;        // Execute address
    uint8_t bLengthLow, bLengthHigh;    // File length
    uint8_t bFlags;                     // b0-b1: file start sector b8-b9
                                        // b2-b3: file load address b16-b17
                                        // b4-b5: file length b16-b17
                                        // b6-b7: file execution address b16-b17
    uint8_t bStartSector;               // File start sector b0-b7
};

enum MsgType { msgStatus, msgInfo, msgFix, msgWarning, msgError, msgInfoAlways, msgFixAlways, msgWarningAlways };


const char* ValStr(int val, const char* pcszDec_, const char* pcszHex_, bool fForceDecimal_ = false);

const char* NumStr(int n);
const char* ByteStr(int b);
const char* WordStr(int w);

const char* CylStr(int cyl);
const char* HeadStr(int head);
const char* RecordStr(int record);
const char* SizeStr(int size);


std::string strCH(int cyl, int head);

// The sector is an index value.
std::string strCHS(int cyl, int head, int sector);

std::string strCHR(int cyl, int head, int record);

// The sector is an index value.
std::string strCHSR(int cyl, int head, int sector, int record);

[[deprecated("use strCH instead")]]
const char* CH(int cyl, int head);
[[deprecated("use strCH instead")]]
const char* CHS(int cyl, int head, int sector);
[[deprecated("use strCH instead")]]
const char* CHR(int cyl, int head, int record);
[[deprecated("use strCH instead")]]
const char* CHSR(int cyl, int head, int sector, int record);

extern std::set<std::string> seen_messages;

void MessageCore(MsgType type, const std::string& msg);

template <typename ...Args>
void Message(MsgType type, const char* pcsz_, Args&& ...args)
{
    MessageCore(type, util::fmt(pcsz_, std::forward<Args>(args)...));
}

template <typename ...Args>
void MessageCPP(MsgType type, Args&& ...args)
{
    MessageCore(type, make_string(std::forward<Args>(args)...));
}

const char* LastError();
bool Error(const char* pcsz_ = nullptr);

int GetMemoryPageSize();

bool IsBlockDevice(const std::string& path);
bool IsFloppyDevice(const std::string& path);
bool GetMountedDevice(const char* pcszDrive_, char* pszDevice_, int cbDevice_);
/*
bool IsDriveRemoveable (HANDLE hDrive_);
bool IsHardDiskDevice (const char *pcsz_);
*/
bool CheckLibrary(const char* pcszLib_, const char* pcszFunc_);

bool IsFile(const std::string& path);
bool IsDir(const std::string& path);
bool IsFloppy(const std::string& path);
bool IsHddImage(const std::string& path);
bool IsBootSector(const std::string& path);
bool IsRecord(const std::string& path, int* pRecord = nullptr);
bool IsTrinity(const std::string& path);
bool IsBuiltIn(const std::string& path);
bool IsVfd(const std::string& path);
bool IsVfdpt(const std::string& path);

bool IsConsoleWindow();

uint8_t* AllocMem(int len);
void FreeMem(void* pv_);

std::string FileExt(const std::string& path);
bool IsFileExt(const std::string& path, const std::string& ext);
int64_t FileSize(const std::string& path);
int GetFileType(const char* pcsz_);

#ifdef _WIN32
typedef HANDLE FindFileHandle;
const FindFileHandle FindFileHandleVoid = INVALID_HANDLE_VALUE;
#else
typedef DIR *FindFileHandle;
constexpr FindFileHandle FindFileHandleVoid = nullptr;
#endif

VectorX<std::string> FindFiles(const std::string& fileNamePart, const std::string& dirName);
std::string FindFirstFileOnly(const std::string& fileNamePart, const std::string& dirName);
std::string FindFirstFile(const std::string& fileNamePart, const std::string& dirName, FindFileHandle& findFileHandle);
std::string FindNextFile(const std::string& fileNamePart, const std::string& dirName, FindFileHandle& findFileHandle);
void CloseFindFile(FindFileHandle& findFileHandle);

void ReadBinaryFile(const std::string& filePath, Data& data);
void WriteBinaryFile(const std::string& filePath, const Data& data);

void ByteSwap(void* pv, size_t nSize_);
int TPeek(const uint8_t* buf, int offset = 0);
void TrackUsedInit(Disk& disk);
bool IsTrackUsed(int cyl_, int head_);

void CalculateGeometry(int64_t total_sectors, int& cyls, int& heads, int& sectors);
void ValidateRange(Range& range, int max_cyls, int max_heads, int cyl_step = 1, int def_cyls = -1, int def_heads = -1);

int SizeToCode(int sector_size);
bool ReadSector(const HDD& hdd, int sector, MEMORY& pm_);
bool CheckSig(const HDD& hdd, int sector, int offset, const char* sig, int len = 0);
bool DiskHasMBR(const HDD& hdd);

#ifndef S_ISDIR
#define _S_ISTYPE(mode,mask)    (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)           _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)           _S_ISTYPE((mode), _S_IFREG)
#endif

bool VerifyCylHeadsMatch(const CylHead& cylHeadExpected, const Header& headerResult, bool badCrc = false, bool optNormalDisk = false, bool noReaction = false);

std::chrono::system_clock::time_point StartStopper(const std::string& label = "");
std::chrono::system_clock::time_point StopStopper(const std::chrono::system_clock::time_point& timePoint, const std::string& label = "", bool isStartAlso = false);


class MEMORY
{
public:
    MEMORY() : size(0), pb(nullptr) {}
    explicit MEMORY(int uSize_) : size(uSize_), pb(AllocMem(size)) {}
    void copyFrom(const VectorX<uint8_t>& src, int copySize = 0, int thisOffset = 0)
    {
        if (pb == nullptr)
            throw util::exception("MEMORY is not allocated");
        if (copySize == 0)
            copySize = src.size();
        if (size < copySize + thisOffset)
            throw util::exception("MEMORY is smaller than the src copying from");
        auto p = pb + thisOffset;
        for (int i = 0; i < copySize; i++)
            *(p++) = src[i];
    }
    MEMORY(const MEMORY&) = delete;
    void operator= (const MEMORY& ref_) = delete;
    virtual ~MEMORY();

    operator uint8_t* () { return pb; }
    //  uint8_t& operator[] (size_t u_) { return pb[u_]; }
    inline void fill(int c = 0xee)
    {
        memset(pb, c, lossless_static_cast<size_t>(size));
    }

    void resize(int uSize)
    {
        if (size == uSize)
            return;
        if (size > 0)
        {
            FreeMem(pb);
            pb = nullptr;
            size = 0;
        }
        size = uSize;
        if (uSize > 0)
        {
            pb = AllocMem(uSize);
            // Invalidate the content so misbehaving FDCs can be identified.
            fill();
        }
    }

    int size = 0;
    uint8_t* pb = nullptr;
};
typedef MEMORY* PMEMORY;

#ifndef HAVE_GETTIMEOFDAY
int gettimeofday(struct timeval* tv, struct timezone* tz);
#endif
