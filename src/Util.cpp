// Legacy utility functions

#ifdef _WIN32
#include "PlatformConfig.h" // For disabling strncat deprecation.
#else
#include "config.h"
#endif
#include "Util.h"
#include "Options.h"
#include "Image.h"
#include "SAMCoupe.h"

#include <cctype>
#include <cstring>
#include <set>
#include <string>
#include <sys/stat.h>

static auto& opt_hex = getOpt<int>("hex");
static auto& opt_time = getOpt<int>("time");
static auto& opt_verbose = getOpt<int>("verbose");

std::set<std::string> seen_messages;

static uint32_t adwUsed[2][3];

const char* ValStr(int val, const char* pcszDec_, const char* pcszHex_, bool fForceDecimal_/* = false*/)
{
    static char strs[8][32];
    static int idx;

    // Next slot
    idx = (idx + 1) % arrayintsizeof(strs);

    // Format in the current base using the supplied format string
    snprintf(strs[idx], sizeof(strs[idx]), (opt_hex == 0 || fForceDecimal_) ? pcszDec_ : pcszHex_, val);

    return strs[idx];
}

#define HEXCASE "X"

const char* NumStr(int n) { return ValStr(n, "%d", "%" HEXCASE); }
const char* ByteStr(int b) { return ValStr(b, "%u", "%02" HEXCASE); }
const char* WordStr(int w) { return ValStr(w, "%u", "%04" HEXCASE); }

const char* CylStr(int cyl) { return ValStr(cyl, "%d", "%02" HEXCASE, opt_hex == 2); }
const char* HeadStr(int head) { return ValStr(head, "%d", "%" HEXCASE, opt_hex == 2); }
const char* RecordStr(int record) { return ValStr(record, "%d", "%02" HEXCASE); }
const char* SizeStr(int size) { return ValStr(size, "%d", "%" HEXCASE); }

std::string strCH(int cyl, int head)
{
    std::ostringstream ss;
    ss << "cyl " << CylStr(cyl) << " head " << HeadStr(head);
    return ss.str();
}

// The sector is an index value.
std::string strCHS(int cyl, int head, int sector)
{
    std::ostringstream ss;
    ss << "cyl " << CylStr(cyl) << " head " << HeadStr(head) << " sector index " << sector;
    return ss.str();
}

std::string strCHR(int cyl, int head, int record)
{
    std::ostringstream ss;
    ss << "cyl " << CylStr(cyl) << " head " << HeadStr(head) << " sector id " << RecordStr(record);
    return ss.str();
}

// The sector is an index value.
std::string strCHSR(int cyl, int head, int sector, int record)
{
    std::ostringstream ss;
    ss << "cyl " << CylStr(cyl) << " head " << HeadStr(head) << " sector index " << sector << " sector id " << RecordStr(record);
    return ss.str();
}

const char* CH(int cyl, int head)
{
    static char sz[64];
    snprintf(sz, sizeof(sz), "%s", strCH(cyl, head).c_str());
    return sz;
}

// The sector is an index value.
const char* CHS(int cyl, int head, int sector)
{
    static char sz[64];
    snprintf(sz, sizeof(sz), "%s", strCHS(cyl, head, sector).c_str());
    return sz;
}

const char* CHR(int cyl, int head, int record)
{
    static char sz[64];
    snprintf(sz, sizeof(sz), "%s", strCHR(cyl, head, record).c_str());
    return sz;
}

// The sector is an index value.
const char* CHSR(int cyl, int head, int sector, int record)
{
    static char sz[128];
    snprintf(sz, sizeof(sz), "%s", strCHSR(cyl, head, sector, record).c_str());
    return sz;
}

#undef HEX

void MessageCore(MsgType type, const std::string& msg)
{
    if (type == msgError)
        throw util::exception(msg);

    if (type == msgInfo || type == msgFix || type == msgWarning)
    {
        if (seen_messages.find(msg) != seen_messages.end())
            return;

        seen_messages.insert(msg);
    }

    switch (type)
    {
    case msgStatus: break;
    case msgInfo:
    case msgInfoAlways:
        util::cout << "Info: "; break;
    case msgFix:
    case msgFixAlways:
        util::cout << colour::GREEN << "Fixed: "; break;
    case msgWarning:
    case msgWarningAlways:
        util::cout << colour::YELLOW << "Warning: "; break;
    case msgError:
        util::cout << colour::RED << "Error: "; break;
    }

    if (type == msgStatus)
        util::cout << ttycmd::statusbegin << "\r" << msg << ttycmd::statusend;
    else
        util::cout << msg << colour::none << '\n';
}

const char* LastError()
{
#ifdef _WIN32
    static char sz[256];
    uint32_t dwError = GetLastError();

    // Try for US English, but fall back on anything
    if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwError,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), sz, sizeof(sz), nullptr))
    {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), sz, sizeof(sz), nullptr);
    }

    return dwError ? sz : "";
#else
    return "";
#endif
}

bool Error(const char* pcsz_/*=nullptr*/)
{
#if 0//def _WIN32
    throw win32_error(::GetLastError(), pcsz_);
#else
    throw posix_error(errno, pcsz_);
#endif
}


int GetMemoryPageSize()
{
#ifdef _WIN32
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    return si.dwPageSize;
#elif defined(HAVE_SYSCONF)
    return static_cast<int>(sysconf(_SC_PAGESIZE));
#else
    return 0x1000;
#endif
}


bool IsBlockDevice(const std::string& path)
{
#ifndef _WIN32
    struct stat st;
    if (!stat(path.c_str(), &st) && S_ISBLK(st.st_mode))
        return true;
#endif
    (void)path;
    return false;
}
bool IsFloppyDevice(const std::string& path)
{
    // Must be in "X:" format
    if (!IsFloppy(path))
        return false;

#ifdef _WIN32
    // Check for the expected device path prefix
    char sz[MAX_PATH];
    return QueryDosDevice(path.c_str(), sz, arraysize(sz)) && !std::memcmp(sz, "\\Device\\Floppy", 14);
#else
    return true;
#endif
}

bool GetMountedDevice(const char* pcsz_, char* pszDevice_, int cbDevice_)
{
    bool f = false;

#ifdef _WIN32
    HKEY hkey;
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\MountedDevices", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        char szKey[64] = "\\DosDevices\\";
        strncat(szKey, pcsz_, arraysize(szKey) - 1);

        WCHAR wszBus[256];
        DWORD cbBus = sizeof(wszBus), dwType = 0;

        f = RegQueryValueEx(hkey, szKey, nullptr, &dwType, (BYTE*)wszBus, &cbBus) == ERROR_SUCCESS;
        RegCloseKey(hkey);

        if (f && dwType == REG_BINARY && wszBus[0] == L'\\')
        {
            int n = WideCharToMultiByte(CP_ACP, 0, wszBus, cbBus / 2, pszDevice_, cbDevice_ - 1, nullptr, nullptr);
            pszDevice_[n] = '\0';
            f = !!n;
        }
    }
#else
    (void)pcsz_; (void)pszDevice_; (void)cbDevice_;
#endif

    return f;
}


int64_t FileSize(const std::string& path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    if (GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &wfad))
        return (static_cast<int64_t>(wfad.nFileSizeHigh) << 32) | wfad.nFileSizeLow;
#else
    struct stat st = {};
    if (stat(path.c_str(), &st) == 0)
        return st.st_size;
#endif

    return -1;
}


bool IsFile(const std::string& path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    return GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &wfad) &&
        !(wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st = {};
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
#endif
}

bool IsDir(const std::string& path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA wfad;
    return GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &wfad) &&
        (wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st = {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

bool IsFloppy(const std::string& path)
{
    // Accept anything in the format X: as we'll check it more thoroughly later.
    // This gives better handling of USB floppy drives, or non-existent drives.
    return path.length() == 2 &&
        std::isalpha(static_cast<uint8_t>(path[0])) &&
        path[1] == ':';
}

bool IsHddImage(const std::string& path)
{
    auto size = FileSize(path);

    // Does the file exist?
    if (size >= 0)
    {
        // Accept existing files with a .hdf file extension
        if (IsFileExt(path, "hdf"))
            return true;

        // Accept existing files larger than max image size if they're an exact multiple of 512 bytes
        if (size > MAX_IMAGE_SIZE && !(size & (SECTOR_SIZE - 1)))
            return true;
    }
    else
    {
        // Require non-existent files have .hdf or .raw extensions
        if (IsFileExt(path, "hdf") || IsFileExt(path, "raw"))
            return true;
    }

    // Reject anything else
    return false;
}

bool IsBootSector(const std::string& path)
{
    auto uRecord = 0;
    return IsRecord(path, &uRecord) && uRecord == 0;
}

bool IsRecord(const std::string& path, int* pRecord)
{
    // Path must contain a colon
    auto it = path.rfind(':');
    if (it == std::string::npos)
        return false;

    // A record number must be present
    std::string strRecord = path.substr(it + 1);
    if (strRecord.empty() || !std::isdigit(static_cast<uint8_t>(strRecord[0])))
        return false;

    // Extract the record number
    size_t pos;
    const auto record = lossless_static_cast<int>(std::stoul(strRecord, &pos, 0));

    // Pass record number to caller if required
    if (pRecord)
        *pRecord = record;

    // Valid if nothing remaining in string
    return pos == strRecord.length();
}

bool IsTrinity(const std::string& path)
{
    std::string str = util::lowercase(path);
    return str.substr(0, 4) == "sam:" || str.substr(0, 8) == "trinity:";
}

bool IsBuiltIn(const std::string& path)
{
    if (path.length() < 2 || path[0] != '@')
        return false;

    char* end = nullptr;
    (void)std::strtol(path.c_str() + 1, &end, 0);
    return !*end;
}

bool IsVfd(const std::string& path)
{
    const std::string str = util::lowercase(path);
    return str.substr(0, 4) == "vfd:";
}

bool IsVfdpt(const std::string& path)
{
    const std::string str = util::lowercase(path);
    return str.substr(0, 6) == "vfdpt:";
}


bool IsConsoleWindow()
{
#ifdef _WIN32
    DWORD dwProcessId{};
    return GetConsoleProcessList(&dwProcessId, 1) > 1;
#else
    return true;
#endif
}

#ifndef HAVE_GETTIMEOFDAY
#ifdef HAVE_SYS_TIMEB_H
int gettimeofday(struct timeval* tv, struct timezone* /*tz*/)
{
    struct _timeb tb;
    _ftime(&tb);

    tv->tv_sec = static_cast<long>(tb.time);
    tv->tv_usec = tb.millitm * 1000;

    return 0;
}
#endif // HAVE_SYS_TIMEB_H
#endif // HAVE_GETTIMEOFDAY


bool CheckLibrary(const char* pcszLib_, const char* pcszFunc_)
{
#ifndef _WIN32
    (void)pcszLib_; (void)pcszFunc_;

    // Anything compiled in to non-Windows versions will/must be present
    return true;
#else
    // Convert name to the library filename
    if (!strcasecmp(pcszLib_, "zlib")) pcszLib_ = "zlibwapi.dll";
    else if (!strcasecmp(pcszLib_, "bzip2")) pcszLib_ = "bzip2.dll";
    else if (!strcasecmp(pcszLib_, "lzma")) pcszLib_ = "liblzma.dll";
    else if (!strcasecmp(pcszLib_, "capsimg")) pcszLib_ = "CAPSImg.dll";
    else if (!strcasecmp(pcszLib_, "winsock2")) pcszLib_ = "ws2_32.dll";
    else if (!strcasecmp(pcszLib_, "ftdi2")) pcszLib_ = "ftd2xx.dll";
    else if (!strcasecmp(pcszLib_, "winusb")) pcszLib_ = "winusb.dll";

    // Load the library and check for the named function
    // Leave the library loaded as it's about to be loaded anyway
    HINSTANCE hinst = LoadLibrary(pcszLib_);
    if (hinst)
        return GetProcAddress(hinst, pcszFunc_) != nullptr;

    return false;
#endif
}

///////////////////////////////////////////////////////////////////////////////

uint8_t* AllocMem(int len)
{
#ifdef _WIN32
    auto pb = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, len, MEM_COMMIT, PAGE_READWRITE));
#else
    static auto align = GetMemoryPageSize();

    auto size = len + align - 1 + longsizeof(void*);
    auto pv = calloc(1, static_cast<size_t>(size));
    void** ppv = reinterpret_cast<void**>((reinterpret_cast<uintptr_t>(pv) + static_cast<uintptr_t>(size - len)) & static_cast<uintptr_t>(~(align - 1)));
    ppv[-1] = pv;
    auto pb = reinterpret_cast<uint8_t*>(ppv);

#endif // WIN32

    if (!pb) throw std::bad_alloc();
    return pb;
}

void FreeMem(void* pv_)
{
    if (pv_)
#ifdef _WIN32
        VirtualFree(pv_, 0, MEM_RELEASE);
#else
        free(reinterpret_cast<void**>(pv_)[-1]);
#endif // WIN32
}

///////////////////////////////////////////////////////////////////////////////

std::string FileExt(const std::string& path)
{
    auto idx = path.rfind('.');
    return (idx == std::string::npos) ? "" : path.substr(idx + 1);
}

bool IsFileExt(const std::string& path, const std::string& ext)
{
    return util::lowercase(FileExt(path)) == util::lowercase(ext);
}


int GetFileType(const char* pcsz_)
{
    // BDOS record?
    if (IsRecord(pcsz_))
        return ftRecord;

    // Order must match enum in Image.h -- this doesn't belong here
    static const char* output_types[] =
    { "", "", "raw", "dsk", "mgt", "sad", "trd", "ssd", "d2m", "d81", "d88", "imd", "mbd", "opd", "s24", "fdi", "cpm", "lif", "ds2", "qdos", "", nullptr };

    // Compare it to the output types we support
    for (int i = 1; output_types[i]; ++i)
        if (IsFileExt(pcsz_, output_types[i]))
            return i;

    return ftUnknown;
}

VectorX<std::string> FindFiles(const std::string& fileNamePart, const std::string& dirName)
{
    VectorX<std::string> result;
#ifdef _WIN32
    WIN32_FIND_DATA FindFileData;
    // The first parameter of FindFirstFile can contain wildcard...
    auto findFileHandle = FindFirstFile(dirName.c_str(), &FindFileData);
    if (findFileHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string fileName = FindFileData.cFileName;
#else
    auto findFileHandle = opendir(dirName.c_str());
    if (findFileHandle != nullptr)
    {
        struct dirent *diread;
        while ((diread = readdir(findFileHandle)) != nullptr)
        {
            const auto fileName = std::string(diread->d_name);
#endif
            if (fileName.find(fileNamePart) != std::string::npos)
                result.push_back(dirName + PATH_SEPARATOR_CHR + fileName);
#ifdef _WIN32
        } while (FindNextFile(findFileHandle, &FindFileData));
        CloseFindFile(findFileHandle);
    }
#else
        }
        closedir(findFileHandle);
        findFileHandle = nullptr;
    }
#endif
    return result;
}

std::string FindFirstFileOnly(const std::string& fileNamePart, const std::string& dirName)
{
    FindFileHandle dir_ptr;
    auto result = FindFirstFile(fileNamePart, dirName, dir_ptr);
    CloseFindFile(dir_ptr);
    return result;
}

std::string FindFirstFile(const std::string& fileNamePart, const std::string& dirName, FindFileHandle& findFileHandle)
{
    std::string result;
#ifdef _WIN32
    WIN32_FIND_DATA FindFileData;
    // The first parameter of FindFirstFile can contain wildcard...
    const auto winFileName = dirName + PATH_SEPARATOR_CHR + "*";
    findFileHandle = FindFirstFile(winFileName.c_str(), &FindFileData);
    if (findFileHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            const std::string fileName = FindFileData.cFileName;
#else
    findFileHandle = opendir(dirName.c_str());
    if (findFileHandle != nullptr)
    {
        struct dirent *diread;
        while ((diread = readdir(findFileHandle)) != nullptr)
        {
            const auto fileName = std::string(diread->d_name);
#endif
            if (fileName.find(fileNamePart) != std::string::npos)
            {
                result = dirName + PATH_SEPARATOR_CHR + fileName;
                break;
            }
#ifdef _WIN32
        } while (FindNextFile(findFileHandle, &FindFileData));
#else
        }
#endif
        if (result.empty())
            CloseFindFile(findFileHandle);
    }
    return result;
}

std::string FindNextFile(const std::string& fileNamePart, const std::string& dirName, FindFileHandle& findFileHandle)
{
    std::string result;
    if (findFileHandle != FindFileHandleVoid)
    {
#ifdef _WIN32
        WIN32_FIND_DATA FindFileData;
        while (FindNextFile(findFileHandle, &FindFileData))
        {
            const std::string fileName = FindFileData.cFileName;
#else
        struct dirent *diread;
        while ((diread = readdir(findFileHandle)) != nullptr)
        {
            const auto fileName = std::string(diread->d_name);
#endif
            if (fileName.find(fileNamePart) != std::string::npos)
            {
                result = dirName + PATH_SEPARATOR_CHR + fileName;
                break;
            }
        }
        if (result.empty())
            CloseFindFile(findFileHandle);
    }
    return result;
}

void CloseFindFile(FindFileHandle& findFileHandle)
{
    if (findFileHandle != FindFileHandleVoid)
    {
#ifdef _WIN32
        FindClose(findFileHandle);
#else
        closedir(findFileHandle);
#endif
        findFileHandle = FindFileHandleVoid;
    }
}
//#endif

void ReadBinaryFile(const std::string& filePath, Data& data)
{
    std::ifstream myfile;
    myfile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    myfile.open(filePath, std::ios::in | std::ios::binary);
    const auto begin = myfile.tellg();
    myfile.seekg(0, std::ios::end);
    const auto size = myfile.tellg() - begin;
    myfile.seekg(begin, std::ios::beg);
    data.resize(size);
    myfile.read(reinterpret_cast<char*>(data.data()), data.size());
    myfile.close();
}

void WriteBinaryFile(const std::string& filePath, const Data& data)
{
    std::ofstream myfile;
    myfile.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    myfile.open(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    myfile.write(reinterpret_cast<const char*>(data.data()), data.size());
    myfile.close();
}



void ByteSwap(void* pv, size_t len)
{
    assert((len & 1) == 0);
    uint8_t* pb = reinterpret_cast<uint8_t*>(pv);

    for (size_t i = 0; i < len; i += 2)
        std::swap(pb[i], pb[i + 1]);
}


void TrackUsedInit(Disk& disk)
{
    bool fDone = false;
    uint8_t abBAM[195] = {};

    // Tracks 0 and 4 cover the (empty) directory start and boot sector, and are always used
    memset(&adwUsed, 0, sizeof(adwUsed));
    adwUsed[0][0] |= 0x00000011;

    auto sector = disk.find(Header(0, 0, 1, 2));
    if (sector == nullptr || sector->data_size() < SECTOR_SIZE)
        throw util::exception("disk is not MGT format");

    MGT_DISK_INFO di;
    const Data& data1 = sector->data_copy();
    GetDiskInfo(data1.data(), di);

    for (uint8_t cyl = 0; !fDone && cyl < di.dir_tracks; ++cyl)
    {
        for (uint8_t sec = 1; !fDone && sec <= MGT_SECTORS; ++sec)
        {
            // Skip the boot sector on MasterDOS extended directories
            if (cyl == 4 && sec == 1)
                continue;

            CylHead cylhead(cyl, 0);
            if ((sector = disk.find(Header(cyl, 0, sec, 2))) == nullptr || sector->data_size() < SECTOR_SIZE)
                throw util::exception("cyl ", cyl, " head 0 sector ", sec, " not found");

            for (int entry = 0; !fDone && entry < 2; ++entry)
            {
                const Data& data = sector->data_copy();
                const MGT_DIR* pdi = reinterpret_cast<const MGT_DIR*>(data.data() + ((SECTOR_SIZE / 2) * entry));

                if (pdi->bType & 0x3f)
                {
                    // Mark the current track as used
                    adwUsed[0][cyl >> 5] |= (1 << (cyl & 0x1f));

                    // If the final entry of the non-final track is in use, ensure the next track is considered used
                    if (entry == 1 && sec == (MGT_SECTORS - 1) && cyl != (di.dir_tracks - 1))
                        adwUsed[0][(cyl + 1) >> 5] |= (1 << ((cyl + 1) & 0x1f));

                    // Merge the sector address map into the overall disk BAM
                    for (size_t i = 0; i < sizeof(abBAM); ++i)
                        abBAM[i] |= pdi->abSectorMap[i];
                }
                else
                    fDone = !pdi->abName[0];
            }
        }
    }

    // Convert the BAM used sectors to our used tracks
    for (size_t i = 0; i < (sizeof(abBAM) << 3); ++i)
    {
        // Sector in use?
        if (abBAM[i >> 3] & (1 << (i & 7)))
        {
            uint8_t cyl = static_cast<uint8_t>((i / MGT_SECTORS) + MGT_DIR_TRACKS);
            uint8_t head = cyl / NORMAL_TRACKS;

            cyl %= NORMAL_TRACKS;
            adwUsed[head][cyl >> 5] |= (1 << (cyl & 0x1f));
        }
    }
}

bool IsTrackUsed(int cyl_, int head_)
{
    return (adwUsed[head_ & 1][cyl_ >> 5] & (1 << (cyl_ & 0x1f))) != 0;
}


// Read the SAM 3-byte offset: page, low, high
int TPeek(const uint8_t* buf, int offset/*=0*/)
{
    auto addr = ((buf[0] & 0x1f) << 14) | ((buf[2] & 0x3f) << 8) | buf[1];

    // Clip to 512K and add the offset
    return (addr & ((1 << 19) - 1)) + offset;
}


// Calculate a suitable CHS geometry covering the supplied number of sectors
void CalculateGeometry(int64_t total_sectors, int& cyls, int& heads, int& sectors)
{
    Format fmt;
    if (Format::FromSize(total_sectors * SECTOR_SIZE, fmt))
    {
        cyls = fmt.cyls;
        heads = fmt.heads;
        sectors = fmt.sectors;
        return;
    }

    // If the sector count is exactly divisible by 16*63, use them for heads and sectors
    if ((total_sectors % (16 * 63)) == 0)
    {
        heads = 16;
        sectors = 63;
    }
    else
    {
        // Start the head count to give balanced figures for smaller drives
        heads = (total_sectors >= 65536) ? 8 : (total_sectors >= 32768) ? 4 : 2;
        sectors = 32;
    }

    // Loop until we're (ideally) within 1024 cylinders
    while ((total_sectors / heads / sectors) > 1023)
    {
        if (heads < 16)
            heads *= 2;
        else if (sectors != 63)
            sectors = 63;
        else
            break;
    }

    // Calculate the cylinder limit at or below the total size
    cyls = static_cast<int>(total_sectors / heads / sectors);

    // Update the supplied structure
    if (cyls > 16383)
        cyls = 16383;
}

void ValidateRange(Range& range, int max_cyls, int max_heads, int cyl_step/*=1*/, int def_cyls/*=0*/, int def_heads/*=0*/)
{
    // Default to the normal cyl/head limits
    if (def_cyls <= 0) def_cyls = max_cyls;
    if (def_heads <= 0) def_heads = max_heads;

    // Limit default according to step, rounding up
    if (cyl_step > 1)
    {
        def_cyls = (def_cyls + (cyl_step - 1)) / cyl_step;
        max_cyls = (max_cyls + (cyl_step - 1)) / cyl_step;
    }

    // Use default if unset
    if (range.cyls() <= 0)
    {
        range.cyl_begin = 0;
        range.cyl_end = def_cyls;
    }

    if (range.cyl_end > max_cyls)
        throw util::exception("end cylinder (", range.cyl_end - 1, ") out of range (0-", max_cyls - 1, ")");

    // If no head value is given, use the default range
    if (range.heads() <= 0)
    {
        range.head_begin = 0;
        range.head_end = def_heads;
    }

    if (range.head_end > max_heads)
        throw util::exception("end head (", range.head_end - 1, ") out of range (0-", max_heads - 1, ")");
}

int SizeToCode(int sector_size)
{
    for (auto i = 0; i < 8; ++i)
        if (sector_size == Sector::SizeCodeToLength(i))
            return i;
    // Should never hit this.
    throw util::exception("Unknown sector size (", sector_size, ")");
}

bool ReadSector(const HDD& hdd, int sector, MEMORY& pm_)
{
    return hdd.Seek(sector) && hdd.Read(pm_, 1);
}

bool CheckSig(const HDD& hdd, int sector, int offset, const char* sig, int len)
{
    MEMORY mem(hdd.sector_size);
    const auto uLen = len == 0 ? strlen(sig) : lossless_static_cast<size_t>(len);

    return ReadSector(hdd, sector, mem) &&
        !memcmp(mem + offset, reinterpret_cast<const uint8_t*>(sig), uLen);
}

bool DiskHasMBR(const HDD& hdd)
{
    return CheckSig(hdd, 0, 510, "\x55\xaa");
}

/* Verify if cylhead expected equals cylhead result.
 * Verification requires badCrc = false and optNormalDisk = true.
 * If the verification passed then the returned value is true.
 * Otherwise: if noReaction is true then the returned value is false
 * else (when noReaction is false) then an diskforeigncylhead_exception is thrown.
 */
bool VerifyCylHeadsMatch(const CylHead& cylHeadExpected, const Header& headerResult, bool badCrc/* = false*/, bool optNormalDisk/* = false*/, bool noReaction/* = false*/)
{
    if (optNormalDisk && !badCrc && (cylHeadExpected.cyl != headerResult.cyl || cylHeadExpected.head != headerResult.head))
    {
        if (noReaction)
            return false;
        MessageCPP(msgWarningAlways, "Suspicious: ", cylHeadExpected, " does not match sector's {", headerResult, "}");
    }
    return true;
}

std::chrono::system_clock::time_point StartStopper(const std::string& label/* = ""*/)
{
    if (opt_time && opt_verbose)
    {
        util::cout << "Stopper ";
        if (!label.empty())
            util::cout << '"' << label << "\" ";
        util::cout << "started\n";
    }
    return std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point StopStopper(const std::chrono::system_clock::time_point& startTime, const std::string& label/* = ""*/, bool isStartAlso/* = false*/)
{
    auto endTime = std::chrono::system_clock::now();
    if (opt_time && opt_verbose)
    {
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        util::cout << "Elapsed time since started ";
        if (!label.empty())
            util::cout << '"' << label << "\" ";
        util::cout << "stopper: " << elapsed_ms << "ms\n";
        if (isStartAlso)
            StartStopper(label);
    }
    return endTime;
}



MEMORY::~MEMORY()
{
    if (size > 0)
        FreeMem(pb);
}
