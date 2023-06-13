#pragma once

#include "Cpp_helpers.h"

#include <cassert>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <sstream>
#include <vector>

class posix_error : public std::system_error
{
public:
    posix_error(int error_code = 0, const char* message = nullptr)
        : std::system_error(error_code ? error_code : errno, std::generic_category(), message) {}
};


enum class ttycmd : uint8_t
{
    cleartoeol, clearline, statusbegin, statusend
};

#ifdef _WIN32

#include "Platform.h"

enum class colour : uint8_t
{
    black = 0,
    blue = FOREGROUND_BLUE,
    red = FOREGROUND_RED,
    magenta = red | blue,
    green = FOREGROUND_GREEN,
    cyan = green | blue,
    yellow = green | red,
    white = red | green | blue,

    bright = FOREGROUND_INTENSITY,

    BLUE = blue | bright,
    RED = red | bright,
    MAGENTA = magenta | bright,
    GREEN = green | bright,
    CYAN = cyan | bright,
    YELLOW = yellow | bright,
    WHITE = white | bright,

    grey = black | bright,
    none = white
};

#else

enum class colour : uint8_t
{
    blue = 34,
    red = 31,
    magenta = 35,
    green = 32,
    cyan = 36,
    yellow = 33,
    white = 0,

    bright = 0x80,

    BLUE = blue | bright,
    RED = red | bright,
    MAGENTA = magenta | bright,
    GREEN = green | bright,
    CYAN = cyan | bright,
    YELLOW = yellow | bright,
    WHITE = white | bright,

    grey = cyan,    // use cyan as bright black is not well supported for grey
    none = white
};

#endif



inline std::ostream& operator<<(std::ostream& os, uint8_t val)
{
    return os << lossless_static_cast<int>(val);
}

namespace util
{
inline std::ostream& operator<<(std::ostream& os, uint8_t val)
{
    return os << lossless_static_cast<int>(val);
}

template <typename ... Args>
std::string make_string(Args&& ... args)
{
    std::ostringstream ss;
    (void)std::initializer_list<bool> {(ss << args, false)...};
    return ss.str();
}

inline uint8_t reverse_byte_cpu(uint8_t byte)
{
    return lossless_static_cast<uint8_t>(((byte & 1) << 7) |
        ((byte & 2) << 5) |
        ((byte & 4) << 3) |
        ((byte & 8) << 1) |
        ((byte & 16) >> 1) |
        ((byte & 32) >> 3) |
        ((byte & 64) >> 5) |
        ((byte & 128) >> 7));
}

constexpr unsigned char reverse_byte_table[] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
        0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
        0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
        0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
        0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
        0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
        0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
        0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
        0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
        0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
        0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
        0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
        0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
        0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
        0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
        0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
        0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
        0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
        0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
        0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
        0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

constexpr uint8_t reverse_byte(uint8_t byte)
{
    return reverse_byte_table[byte];
}

inline void bit_reverse(uint8_t* pb, size_t len)
{
    while (len-- > 0)
    {
        *pb = reverse_byte(*pb);
        pb++;
    }
}

template <typename T> T byteswap(T x);

template<>
inline uint16_t byteswap<uint16_t>(uint16_t x)
{
    return (lossless_static_cast<uint16_t>((x & 0xff) << 8)) | (x >> 8);
}

template<>
inline uint32_t byteswap<uint32_t>(uint32_t x)
{
    return (lossless_static_cast<uint32_t>(byteswap<uint16_t>(x & 0xffff)) << 16) | byteswap<uint16_t>(x >> 16);
}

template<>
inline uint64_t byteswap<uint64_t>(uint64_t x)
{
    return (lossless_static_cast<uint64_t>(byteswap<uint32_t>(x & 0xffffffff)) << 32) | byteswap<uint32_t>(x >> 32);
}

// ToDo: detect compile endian
template <typename T>
T betoh(T x)
{
    return byteswap(x);
}

// ToDo: detect compile endian
template <typename T>
T htobe(T x)
{
    return byteswap(x);
}

template <typename T>
T htole(T x)
{
    return x;
}

template <typename T>
T letoh(T x)
{
    return x;
}

template <int N, std::enable_if_t<N == 2> * = nullptr>
uint16_t le_value(const uint8_t(&arr)[N])
{
    return (arr[1] << 8) | arr[0];
}

template <int N, std::enable_if_t<N == 3 || N == 4> * = nullptr>
auto le_value(const uint8_t(&arr)[N])
{
    int i = N - 1;
    uint32_t value = arr[i--];
    while (i >= 0)
        value = (value << 8) | arr[i--];
    return value;
}

template <int N, std::enable_if_t<N == 2> * = nullptr>
uint16_t be_value(const uint8_t(&arr)[N])
{
    return (arr[0] << 8) | arr[1];
}

template <int N, std::enable_if_t<N == 3 || N == 4> * = nullptr>
auto be_value(const uint8_t(&arr)[N])
{
    int i = 0;
    uint32_t value = arr[i++];
    while (i >= 0)
        value = (value << 8) | arr[i++];
    return value;
}

template <typename T, int N, std::enable_if_t<N == 2> * = nullptr>
void store_le_value(T value, uint8_t(&arr)[N])
{
    arr[0] = value & 255;
    arr[1] = value >> 8;
}

template <typename T, int N, std::enable_if_t<N == 3 || N == 4> * = nullptr>
void store_le_value(T value, uint8_t(&arr)[N])
{
    for (int i = 0; i < N; i++)
    {
        arr[i] = value & 255;
        value >>= 8;
    }
}

template <typename T, int N, std::enable_if_t<N == 2> * = nullptr>
void store_be_value(T value, uint8_t(&arr)[N])
{
    arr[0] = value >> 8;
    arr[1] = value & 255;
}

template <typename T, int N, std::enable_if_t<N == 3 || N == 4> * = nullptr>
void store_be_value(T value, uint8_t(&arr)[N])
{
    for (int i = N - 1; i >= 0; i--)
    {
        arr[i] = value & 255;
        value >>= 8;
    }
}


class exception : public std::runtime_error
{
public:
    template <typename ... Args>
    explicit exception(Args&& ... args)
        : std::runtime_error(make_string(std::forward<Args>(args)...)) {}
};

class diskspeedwrong_exception : public exception
{
public:
    template <typename ... Args>
    explicit diskspeedwrong_exception(Args&& ... args)
        : exception(make_string(std::forward<Args>(args)...)) {}
};

std::string fmt(const char* fmt, ...);
std::vector<std::string> split(const std::string& str, char delim = ' ', bool skip_empty = false);
std::string trim(const std::string& str);
std::string replace_extension(const std::string& s, const std::string& new_ext);
std::string prepend_extension(const std::string& s, const std::string& new_ext);
std::string resource_dir();
bool is_stdout_a_tty();
std::string lowercase(const std::string& str);

inline std::string to_string(int64_t v) { std::stringstream ss; ss << v; return ss.str(); }
inline std::string to_string(const std::string& s) { return s; }    // std:: lacks to_string to strings(!)
inline std::string format() { return ""; }                          // Needed for final empty entry

template <typename T, typename ...Args>
std::string format(T arg, Args&&... args)
{
    using namespace std; // pull in to_string for other types
    std::string s = to_string(arg) + format(std::forward<Args>(args)...);
    return s;
}


struct LogHelper
{
    LogHelper(std::ostream* screen_, std::ostream* file_ = nullptr)
        : screen(screen_), file(file_)
    {
    }

    std::ostream* screen;
    std::ostream* file;
    bool statusmsg = false;
    bool clearline = false;
};

extern LogHelper cout;
extern std::ofstream log;

LogHelper& operator<<(LogHelper& h, colour c);
LogHelper& operator<<(LogHelper& h, ttycmd cmd);

template <typename T>
LogHelper& operator<<(LogHelper& h, const T& t)
{
    if (h.clearline)
    {
        h.clearline = false;
        h << ttycmd::clearline;
    }

    *h.screen << t;
    if (h.file && !h.statusmsg) *h.file << t;
    return h;
}


template <typename ForwardIter>
void hex_dump(ForwardIter it, ForwardIter itEnd, int start_offset = 0, colour* pColours = nullptr, int per_line = 16)
{
    assert(per_line != 0);
    static const char hex[] = "0123456789ABCDEF";

    it += start_offset;
    if (pColours)
        pColours += start_offset;

    colour c = colour::none;
    auto base_offset = start_offset - (start_offset % per_line);
    start_offset %= per_line;

    while (it < itEnd)
    {
        std::string text(lossless_static_cast<std::string::size_type>(per_line), ' ');

        if (c != colour::none)
        {
            util::cout << colour::none;
            c = colour::none;
        }

        util::cout << hex[(base_offset >> 12) & 0xf] <<
            hex[(base_offset >> 8) & 0xf] <<
            hex[(base_offset >> 4) & 0xf] <<
            hex[base_offset & 0xf] << "  ";

        base_offset += per_line;

        for (int i = 0; i < per_line; i++)
        {
            if (start_offset-- <= 0 && it < itEnd)
            {
                if (pColours)
                {
                    auto new_colour = *pColours++;
                    if (new_colour != c)
                    {
                        util::cout << new_colour;
                        c = new_colour;
                    }
                }

                auto b = *it++;
                text[i] = std::isprint(b) ? b : '.';
                util::cout << hex[b >> 4] << hex[b & 0x0f] << ' ';
            }
            else
            {
                util::cout << "   ";
            }
        }

        util::cout << colour::none << " " << text << "\n";
    }
}

template <typename T>
T str_value(const std::string& str)
{
    static_assert(std::is_same<T, int>::value || std::is_same<T, long>::value, "int or long only");
    try
    {
        size_t idx_end = 0;
        auto hex = util::lowercase(str.substr(0, 2)) == "0x";
        T n{};
        if (std::is_same<T, int>::value)
            n = std::stoi(str, &idx_end, hex ? 16 : 10);
        else
            n = std::stol(str, &idx_end, hex ? 16 : 10);
        if (idx_end == str.size() && n >= 0)
            return n;
    }
    catch (...)
    {
    }

    throw util::exception(util::format("invalid value '", str, "'"));
}

inline void str_range(const std::string& str, int& range_begin, int& range_end)
{
    auto idx = str.rfind('-');
    if (idx == str.npos)
        idx = str.rfind(',');

    try
    {
        if (idx == str.npos)
        {
            range_begin = 0;
            range_end = str_value<int>(str);
            return;
        }

        range_begin = str_value<int>(str.substr(0, idx));
        auto value2 = str_value<int>(str.substr(idx + 1));

        if (str[idx] == '-')
            range_end = value2 + 1;
        else
            range_end = range_begin + value2;

        if (range_end > range_begin)
            return;
    }
    catch (...)
    {
    }

    throw util::exception(util::format("invalid range '", str, "'"));
}

class Version
{
public:
    constexpr uint8_t MajorValue() const
    {
        return (value >> 24) & 0xff;
    }
    constexpr uint8_t MinorValue() const
    {
        return (value >> 16) & 0xff;
    }
    constexpr uint8_t MaintenanceValue() const
    {
        return (value >> 8) & 0xff;
    }
    constexpr uint8_t BuildValue() const
    {
        return 0xff;
    }
    uint32_t value;
};

} // namespace util
