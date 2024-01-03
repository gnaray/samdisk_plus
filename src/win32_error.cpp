// Win32 error exception

#include "win32_error.h"
#include "Cpp_helpers.h"

#include <sstream>
#include <vector>

#ifndef _WIN32
uint32_t lastError = 0;
#endif

std::string GetWin32ErrorStr(uint32_t error_code = 0, bool english = false)
{
    if (!error_code)
        error_code = GetLastError_MP();

    if (!error_code)
        return "";


    std::ostringstream ss;
#ifdef _WIN32
    DWORD length = 0;
    LPWSTR pMessage = nullptr;

    // Try for English first?
    if (english)
    {
        length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), reinterpret_cast<LPWSTR>(&pMessage), 0, nullptr);
    }

    if (!length)
    {
        length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), reinterpret_cast<LPWSTR>(&pMessage), 0, nullptr);
    }

    if (length)
    {
        std::wstring wstr{ pMessage, length };
        wstr.erase(wstr.find_last_not_of(L"\r\n. ") + 1);
        LocalFree(pMessage);

        int wstr_length = static_cast<int>(wstr.length());
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr_length, nullptr, 0, nullptr, nullptr);

        std::vector<char> utf8_str(utf8_size);
        WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr_length, utf8_str.data(), utf8_size, nullptr, nullptr);

        ss << std::string(utf8_str.data(), utf8_str.size());
    }
    else
#else
    {
        ss << "Unknown Win32 error";
    }
#endif // _WIN32

    ss << " (" << error_code << ')';
    return ss.str();
}



std::string win32_category_impl::message(int error_code) const
{
   return GetWin32ErrorStr(lossless_static_cast<uint32_t>(error_code));
}



win32_error::win32_error(uint32_t error_code, const char* message)
        : std::system_error(lossless_static_cast<int>(error_code), win32_category(), message)
{
}
