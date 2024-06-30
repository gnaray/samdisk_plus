#ifndef PLATFORM_H
#define PLATFORM_H

#include "PlatformConfig.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h> // _MEDIA_TYPE
#endif

#include <cstdint>

#ifndef _WIN32
// __WINDOWS_TYPES__ comes from WinTypes.h and only that xor this block can be included.
#ifndef __WINDOWS_TYPES__
// For fdrawcmd.h the following are required (which winioctl.h provides on Windows):
// CTL_CODE
// FILE_DEVICE_UNKNOWN
// DeviceIoControl()
// METHOD_*

// Defining things so this file can be included not only on Windows platform.
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
) // [used by fdrawcmd.h]

// Win32 definitions (win32.h): http://www.jbox.dk/sanos/source/include/win32.h.html
// Chromium's windows_types.h: https://chromium.googlesource.com/chromium/src/+/66.0.3359.158/base/win/windows_types.h
// ToDo: fix BlockDevice so it doesn't need these
typedef char *LPSTR;
typedef char *LPTSTR;
typedef const char *LPCTSTR;
typedef const char *LPCSTR;

typedef unsigned short *LPWSTR;

typedef void *HANDLE;

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD; // Originally long which is 32 bits except in Unix-like systems where 64 bits.
typedef unsigned int ULONG; // Originally long which is 32 bits except in Unix-like systems where 64 bits.
typedef int LONG; // Originally long which is 32 bits except in Unix-like systems where 64 bits.

typedef int BOOL;

// https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types
typedef DWORD *LPDWORD;
typedef void *PVOID, *LPVOID;
typedef struct _OVERLAPPED
{
    DWORD Internal;
    DWORD InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        PVOID  Pointer;
    } DUMMYUNIONNAME;
    HANDLE hEvent;
} OVERLAPPED, *LPOVERLAPPED;

typedef struct _SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
} SECURITY_ATTRIBUTES , *LPSECURITY_ATTRIBUTES;

#define INVALID_HANDLE_VALUE reinterpret_cast<HANDLE>(-1)
#define INVALID_FILE_SIZE    static_cast<DWORD>(0xFFFFFFFF)

// Specifying Device Types: https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/specifying-device-types
// WinIoCtl.h (WinIoCtl.h): https://github.com/tpn/winsdk-7/blob/master/v7.1A/Include/WinIoCtl.h
#define FILE_DEVICE_UNKNOWN             0x00000022 // [used by win32_error.cpp]

#include "win32_error.h"

// DeviceIoControl used by fdrawcmd.h.
inline BOOL DeviceIoControl(HANDLE /*hDevice*/, DWORD /*dwIoControlCode*/, LPVOID /*lpInBuffer*/, DWORD /*nInBufferSize*/,
    LPVOID /*lpOutBuffer*/, DWORD /*nOutBufferSize*/, LPDWORD lpBytesReturned, LPOVERLAPPED /*lpOverlapped*/)
{
    *lpBytesReturned = 0;
    SetLastError_MP(ERROR_FLOPPY_ID_MARK_NOT_FOUND);
    return false;
}

// https://www.codeproject.com/script/Content/ViewAssociatedFile.aspx?rzp=%2FKB%2Fasp%2Fuseraccesscheck%2Fuseraccesscheck_demo.zip&zep=ASPDev%2FMasks.txt&obid=1881&obtid=2&ovid=1
#define FILE_READ_DATA            ( 0x0001 )    // file & pipe [used by fdrawcmd.h]
#define FILE_LIST_DIRECTORY       ( 0x0001 )    // directory
#define FILE_WRITE_DATA           ( 0x0002 )    // file & pipe [used by fdrawcmd.h]
#define FILE_ADD_FILE             ( 0x0002 )    // directory

// http://www.jbox.dk/sanos/source/include/win32.h.html
#define GENERIC_READ                     0x80000000
#define GENERIC_WRITE                    0x40000000
#define GENERIC_EXECUTE                  0x20000000
#define GENERIC_ALL                      0x10000000

#define FILE_SHARE_READ                  0x00000001
#define FILE_SHARE_WRITE                 0x00000002
#define FILE_SHARE_DELETE                0x00000004

#define CREATE_NEW                       1
#define CREATE_ALWAYS                    2
#define OPEN_EXISTING                    3
#define OPEN_ALWAYS                      4
#define TRUNCATE_EXISTING                5

#define FILE_FLAG_WRITE_THROUGH          0x80000000
#define FILE_FLAG_NO_BUFFERING           0x20000000
#define FILE_FLAG_RANDOM_ACCESS          0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN        0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE        0x04000000
#define FILE_FLAG_OVERLAPPED             0x40000000

#define FILE_ATTRIBUTE_READONLY          0x00000001
#define FILE_ATTRIBUTE_HIDDEN            0x00000002
#define FILE_ATTRIBUTE_SYSTEM            0x00000004
#define FILE_ATTRIBUTE_DIRECTORY         0x00000010
#define FILE_ATTRIBUTE_ARCHIVE           0x00000020
#define FILE_ATTRIBUTE_DEVICE            0x00000040
#define FILE_ATTRIBUTE_NORMAL            0x00000080

#define METHOD_BUFFERED                 0 // [used by fdrawcmd.h]
#define METHOD_IN_DIRECT                1 // [used by fdrawcmd.h]
#define METHOD_OUT_DIRECT               2 // [used by fdrawcmd.h]
#define METHOD_NEITHER                  3

#define FILE_ANY_ACCESS                 0
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe


// https://github.com/tpn/winsdk-7/blob/master/v7.1A/Include/WinIoCtl.h
#define FILE_DEVICE_MASS_STORAGE        0x0000002d
//
// IoControlCode values for storage devices
//
#define IOCTL_STORAGE_BASE FILE_DEVICE_MASS_STORAGE
#define IOCTL_STORAGE_GET_MEDIA_TYPES         CTL_CODE(IOCTL_STORAGE_BASE, 0x0300, METHOD_BUFFERED, FILE_ANY_ACCESS)



#define STATUS_INVALID_PARAMETER		static_cast<DWORD>(0xC000000DL)

inline HANDLE CreateFile(LPCSTR /*lpFileName*/, DWORD /*dwDesiredAccess*/,
        DWORD /*dwShareMode*/, LPSECURITY_ATTRIBUTES /*lpSecurityAttributes*/,
        DWORD /*dwCreationDisposition*/, DWORD /*dwFlagsAndAttributes*/,
                   HANDLE /*hTemplateFile*/) { return INVALID_HANDLE_VALUE; }
inline BOOL CloseHandle(HANDLE /*hObject*/) { return false; }

// Service Control Manager (WinSvc.h): https://github.com/tpn/winsdk-7/blob/master/v7.1A/Include/WinSvc.h
#define DECLARE_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name
DECLARE_HANDLE(SC_HANDLE);
typedef SC_HANDLE   *LPSC_HANDLE;

typedef struct _SERVICE_STATUS {
    DWORD   dwServiceType;
    DWORD   dwCurrentState;
    DWORD   dwControlsAccepted;
    DWORD   dwWin32ExitCode;
    DWORD   dwServiceSpecificExitCode;
    DWORD   dwCheckPoint;
    DWORD   dwWaitHint;
} SERVICE_STATUS, *LPSERVICE_STATUS;

#define SERVICE_STOPPED                        0x00000001
#define SERVICE_START_PENDING                  0x00000002
#define SERVICE_STOP_PENDING                   0x00000003
#define SERVICE_RUNNING                        0x00000004
#define SERVICE_CONTINUE_PENDING               0x00000005
#define SERVICE_PAUSE_PENDING                  0x00000006
#define SERVICE_PAUSED                         0x00000007

inline SC_HANDLE OpenSCManager(LPCSTR /*lpMachineName*/, LPCSTR /*lpDatabaseName*/, DWORD /*dwDesiredAccess*/)
{ return nullptr; }
inline SC_HANDLE OpenService(SC_HANDLE /*hSCManager*/, LPCSTR /*lpServiceName*/, DWORD /*dwDesiredAccess*/)
{ return nullptr; }
inline BOOL QueryServiceStatus(SC_HANDLE /*hService*/, LPSERVICE_STATUS /*lpServiceStatus*/) { return false; }
inline BOOL CloseServiceHandle(SC_HANDLE /*hSCObject*/) { return false; }

#endif // __WINDOWS_TYPES__

typedef __int64_t __int64;
typedef __int64 LONGLONG;

typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } DUMMYSTRUCTNAME;
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

// Media_type (winioctl.h): https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ne-winioctl-media_type
typedef enum _MEDIA_TYPE {
  Unknown = 0,
  F5_1Pt2_512,
  F3_1Pt44_512,
  F3_2Pt88_512,
  F3_20Pt8_512,
  F3_720_512,
  F5_360_512,
  F5_320_512,
  F5_320_1024,
  F5_180_512,
  F5_160_512,
  RemovableMedia,
  FixedMedia,
  F3_120M_512,
  F3_640_512,
  F5_640_512,
  F5_720_512,
  F3_1Pt2_512,
  F3_1Pt23_1024,
  F5_1Pt23_1024,
  F3_128Mb_512,
  F3_230Mb_512,
  F8_256_128,
  F3_200Mb_512,
  F3_240M_512,
  F3_32M_512
} MEDIA_TYPE, *PMEDIA_TYPE;

// DISK_GEOMETRY structure (ntdddisk.h): https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntdddisk/ns-ntdddisk-_disk_geometry
typedef struct _DISK_GEOMETRY {
  LARGE_INTEGER Cylinders;
  MEDIA_TYPE    MediaType;
  ULONG         TracksPerCylinder;
  ULONG         SectorsPerTrack;
  ULONG         BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;

#endif // _WIN32

// NTSTATUS is available when building driver but its header file can not be included here.
#if !defined(_MSC_VER) || _MSC_VER <= 1900
typedef uint32_t NTSTATUS;
#else
typedef LONG NTSTATUS;
#endif
#define STATUS_BUFFER_TOO_SMALL static_cast<NTSTATUS>(0xC0000023L)

#endif // PLATFORM_H
