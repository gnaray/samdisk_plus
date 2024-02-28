// Utility functions

#include "config.h"
#include "Options.h"
#include "utils.h"
#include "Util.h"

#include <algorithm>
#include <cstdarg>
#include <fstream>
#include <iostream>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <cctype>
#ifdef HAVE_IO_H
#include <io.h>
#endif

static auto& opt_tty = getOpt<int>("tty");

namespace util
{

std::ofstream log;
LogHelper cout(&std::cout);


std::string fmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    auto len = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    VectorX<char, size_t> bytes(len + 1); // +1 for \0

    va_start(args, fmt);
    std::vsnprintf(&bytes[0], bytes.size(), fmt, args);
    va_end(args);

    return std::string(bytes.data(), lossless_static_cast<std::string::size_type>(len));
}

VectorX<std::string> split(const std::string& str, char delim, bool skip_empty)
{
    VectorX<std::string> items;
    std::stringstream ss(str);
    std::string s;

    while (std::getline(ss, s, delim))
    {
        if (!skip_empty || !s.empty())
            items.emplace_back(std::move(s));
    }

    return items;
}

std::string trim(const std::string& str)
{
    std::string s(str.c_str());
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

std::string replace_extension(const std::string& s, const std::string& new_ext)
{
    std::string new_s = s;
    std::string::size_type i = new_s.rfind('.', new_s.length());

    if (i != std::string::npos) {
        new_s.replace(i + 1, new_s.length() - i - 1, new_ext);
    }
    return new_s;
}

std::string prepend_extension(const std::string& s, const std::string& prepender)
{
    std::string new_s = s;
    std::string::size_type i = new_s.rfind('.', new_s.length());

    if (i != std::string::npos) {
        const std::string replacer = prepender + new_s.substr(i + 1);
        new_s.replace(i + 1, new_s.length() - i - 1, replacer);
    }
    return new_s;
}

std::string resource_dir()
{
#if defined(_WIN32)
    char sz[MAX_PATH];
    if (GetModuleFileName(nullptr, sz, arraysize(sz)))
    {
        auto s = std::string(sz);
        s.erase(s.find_last_of('\\') + 1);
        return s;
    }
#elif defined(RESOURCE_DIR)
    return RESOURCE_DIR;
#endif

    return "";
}

bool is_stdout_a_tty()
{
#ifdef _WIN32
    static bool ret = _isatty(_fileno(stdout)) != 0;
#else
    static bool ret = isatty(fileno(stdout)) != 0;
#endif
    return ret || opt_tty;
}


std::string lowercase(const std::string& str)
{
    std::string ret = str;
    ret.reserve(str.length());
    std::transform(str.cbegin(), str.cend(), ret.begin(), [](char c) {
        return static_cast<uint8_t>(std::tolower(static_cast<int>(c)));
        });
    return ret;
}


bool caseInSensCompare(const std::string& str1, const std::string& str2)
{
    return (str1.size() == str2.size() && std::equal(str1.begin(), str1.end(), str2.begin(), [](const char & c1, const char & c2)
        {
            return (c1 == c2 || std::tolower(c1) == std::tolower(c2));
        }));
}


LogHelper& operator<<(LogHelper& h, colour c)
{
    // Colours are screen only
    if (util::is_stdout_a_tty())
    {
#ifdef _WIN32
        h.screen->flush();
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), static_cast<int>(c));
#else
        auto val = static_cast<int>(c);
        if (val & 0x80)
            *h.screen << "\x1b[" << (val & 0x7f) << ";1m";
        else
            *h.screen << "\x1b[0;" << (val & 0x7f) << 'm';
#endif
    }
    return h;
}

LogHelper& operator<<(LogHelper& h, ttycmd cmd)
{
    if (util::is_stdout_a_tty())
    {
        switch (cmd)
        {
        case ttycmd::statusbegin:
            h.statusmsg = true;
            break;

        case ttycmd::statusend:
            h.statusmsg = false;
            h.clearline = true;
            break;

        case ttycmd::clearline:
            h << "\r" << ttycmd::cleartoeol;
            h.clearline = false;
            break;

        case ttycmd::cleartoeol:
#ifdef _WIN32
            h.screen->flush();

            DWORD dwWritten;
            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

            if (GetConsoleScreenBufferInfo(hConsole, &csbi))
                FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X - csbi.dwCursorPosition.X, csbi.dwCursorPosition, &dwWritten);
#else
            * h.screen << "\x1b[0K";
#endif
            break;
        }
    }
    return h;
}

} // namespace util
