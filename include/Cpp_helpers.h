#ifndef CPP_HELPERS_H
#define CPP_HELPERS_H

#include <climits>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <algorithm>

template <typename ... Args>
std::string make_string2(Args&& ... args)
{
    std::ostringstream ss;
    (void)std::initializer_list<bool> {(ss << args, false)...};
    return ss.str();
}

template <typename T, typename ... Args>
T make_error(Args&& ... args)
{
    return T(make_string2(std::forward<Args>(args)...));
}

template<class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
constexpr approximately_equal(T x, T y)
{
    const auto absDiff = std::fabs(x - y);
    return absDiff <= std::max(std::fabs(x), std::fabs(y)) * std::numeric_limits<double>::epsilon()
            || absDiff < std::numeric_limits<T>::min();
}

// https://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
template<class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
    almost_equal(T x, T y, int ulp)
{
    const auto absDiff = std::fabs(x - y);
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return absDiff <= std::numeric_limits<T>::epsilon() * std::fabs(x + y) * ulp
        // unless the result is subnormal
        || absDiff < std::numeric_limits<T>::min();
}

template<typename T, typename U>
T lossless_static_cast(U opt_sectors);

template<>
constexpr int lossless_static_cast(unsigned char x)
{
    return static_cast<int>(x);
}

template<>
constexpr int lossless_static_cast(long x)
{
    if (x > std::numeric_limits<int>::max() || x < std::numeric_limits<int>::min())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<int>(x);
}

template<>
constexpr double lossless_static_cast(long x)
{
    const auto result = static_cast<double>(x);
    if (x != static_cast<long>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
constexpr double lossless_static_cast(unsigned long x)
{
    const auto result = static_cast<double>(x);
    if (x != static_cast<unsigned long>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
constexpr int lossless_static_cast(size_t x)
{
    if (x > std::numeric_limits<int>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<int>(x);
}

template<>
constexpr uint8_t lossless_static_cast(int x)
{
    if (x < 0 || x > std::numeric_limits<uint8_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint8_t>(x);
}

template<>
constexpr uint16_t lossless_static_cast(int x)
{
    if (x < 0 || x > std::numeric_limits<uint16_t>::max())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<uint16_t>(x);
}

template<>
constexpr size_t lossless_static_cast(int x)
{
    if (x < 0)
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    return static_cast<size_t>(x);
}

template<>
constexpr double lossless_static_cast(int x)
{
    return static_cast<double>(x);
}

template<>
constexpr int lossless_static_cast(double x)
{
    if (x > std::numeric_limits<int>::max() || x < std::numeric_limits<int>::min())
        throw make_error<std::runtime_error>("Can not convert: value ", x, " is out of range");
    const auto result = static_cast<int>(x);
    const auto xCheck = static_cast<double>(result);
    if (!approximately_equal(x, xCheck))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

template<>
constexpr uint32_t lossless_static_cast(uint16_t x)
{
    return static_cast<uint16_t>(x);
}

template<>
constexpr uint64_t lossless_static_cast(uint32_t x)
{
    return static_cast<uint64_t>(x);
}

template<typename T, typename U>
T limited_static_cast(U opt_sectors);

template<>
constexpr int limited_static_cast(size_t x)
{
    if (x > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(x);
}
#endif // CPP_HELPERS_H
