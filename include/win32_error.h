#pragma once

#ifdef _WIN32
#include "Platform.h"
#else
// Errors (Error.h): https://github.com/tpn/winsdk-7/blob/master/v7.1A/Include/Error.h
#define ERROR_FILE_NOT_FOUND             2L
#define ERROR_NO_MORE_FILES              18L
#define ERROR_INVALID_HANDLE        6
#define ERROR_CRC           23
#define ERROR_SECTOR_NOT_FOUND      27

// System Error Codes (1000-1299): https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--1000-1299-
#define ERROR_FLOPPY_ID_MARK_NOT_FOUND 0x462
#endif

#include <string>
#include <system_error>



inline uint32_t GetLastError_MP() // MP: MultiPlatform.
{
#ifdef _WIN32
    return GetLastError();
#else
    return ERROR_INVALID_HANDLE;
#endif
}



class win32_category_impl : public std::error_category
{
public:
    const char* name() const noexcept override { return "win32"; }
    std::string message(int error_code) const override;
};

inline const std::error_category& win32_category()
{
    static win32_category_impl category;
    return category;
}

class win32_error : public std::system_error
{
public:
    win32_error(uint32_t error_code, const char* message);
};
