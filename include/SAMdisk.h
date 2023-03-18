#pragma once

#include "config.h"

#if defined(_WIN32) && !defined(WINVER)
#define WINVER 0x0500
#define _WIN32_WINNT 0x0501
#define _WIN32_IE 0x0501
#define _RICHEDIT_VER 0x0100
#endif


// Handle, O_*, etc. moved to FileIO.h


#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS
// #define _ITERATOR_DEBUG_LEVEL 0  // ToDo: remove?

#pragma warning(default:4062)       // enumerator 'identifier' in a switch of enum 'enumeration' is not handled
// #pragma warning(default:4242)    // 'identifier' : conversion from 'type1' to 'type2', possible loss of data
// #pragma warning(default:4265)    // 'class': class has virtual functions, but destructor is not virtual
#endif // _MSC_VER

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <array>
#include <vector>
#include <map>
#include <set>
#include <memory>    // for unique_ptr
#include <algorithm> // for sort
#include <functional>
#include <numeric>
#include <bitset>
#include <limits>

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#include <fstream>
#include <sys/stat.h>
#include <cerrno>
#include <ctime>
#include <csignal>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <cassert>
#include <system_error>


#if !defined(HAVE_STRCASECMP) && defined(HAVE__STRCMPI)
#define strcasecmp  _stricmp
#define HAVE_STRCASECMP
#endif

#if !defined(HAVE_SNPRINTF) && defined(HAVE__SNPRINTF)
#define snprintf    _snprintf
#define HAVE_SNPRINTF
#endif

#ifndef HAVE_LSEEK64
#ifdef HAVE__LSEEKI64
#define lseek64 _lseeki64
#else
#define lseek64 lseek
#endif
#endif

#ifdef _WIN32
#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define STRICT
#include <windows.h>
#include <devguid.h>
#include <winioctl.h>
#include <shellapi.h>
#include "CrashDump.h"
#else
#endif // WIN32


// Networking moved to Trinity.h


#ifdef HAVE_SYS_TIMEB_H
#include <sys/timeb.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>   // for gettimeofday()
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "utils.h"
#include "win32_error.h"
#include "CRC16.h"
#include "Disk.h"
#include "DiskUtil.h"
#include "Header.h"
#include "MemFile.h"
#include "Image.h"
#include "HDD.h"
#include "Util.h"
#include "SAMCoupe.h"

// copy
bool ImageToImage(const std::string& src_path, const std::string& dst_path);
bool Image2Trinity(const std::string& path, const std::string& trinity_path);
bool Hdd2Hdd(const std::string& src_path, const std::string& dst_path);
bool Hdd2Boot(const std::string& hdd_path, const std::string& boot_path);
bool Boot2Hdd(const std::string& boot_path, const std::string& hdd_path);
bool Boot2Boot(const std::string& src_path, const std::string& dst_path);

// create
bool CreateImage(const std::string& path, Range range);
bool CreateHddImage(const std::string& path, int nSizeMB_);

// dir
bool Dir(Disk& disk);
bool DirImage(const std::string& path);
bool IsMgtDirSector(const Sector& sector);

// list
bool ListDrives(int nVerbose_);
bool ListRecords(const std::string& path);
void ListDrive(const std::string& path, const HDD& hdd, int verbose);

// scan
bool ScanImage(const std::string& path, Range range);
void ScanTrack(const CylHead& cylhead, const Track& track, ScanContext& context, const Headers& headers_of_ignored_sectors = Headers());

// format, verify
bool FormatHdd(const std::string& path);
bool FormatBoot(const std::string& path);
bool FormatRecord(const std::string& path);
bool FormatImage(const std::string& path, Range range);
bool UnformatImage(const std::string& path, Range range);

// rpm
bool DiskRpm(const std::string& path);

// info
bool HddInfo(const std::string& path, int nVerbose_);
bool ImageInfo(const std::string& path);

// view
bool ViewImage(const std::string& path, Range range);
bool ViewHdd(const std::string& path, Range range);
bool ViewBoot(const std::string& path, Range range);

// fdrawcmd.sys driver functions
bool CheckDriver();
bool ReportDriverVersion();

