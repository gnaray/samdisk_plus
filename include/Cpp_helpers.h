#ifndef CPP_HELPERS_H
#define CPP_HELPERS_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <sstream>

template <typename ... Args>
std::string make_string(Args&& ... args)
{
    std::ostringstream ss;
    (void)std::initializer_list<bool> {(ss << args, false)...};
    return ss.str();
}

template <typename T, typename ... Args>
T make_error(Args&& ... args)
{
    return T(make_string(std::forward<Args>(args)...));
}



// Checking if value in type range (arithmetic in same arithmetic).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_same<TargetType, ValueType>::value && std::is_arithmetic<TargetType>::value> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType /*x*/)
{
    return false;
}

// Checking if value in type range: extending (signed integer in signed integer, unsigned integer in unsigned integer, unsigned integer in signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && ((std::is_signed<TargetType>::value && std::is_signed<ValueType>::value)
                              || (std::is_unsigned<TargetType>::value && std::is_unsigned<ValueType>::value)
                              || (std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value))
                          && (sizeof(TargetType) > sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType /*x*/)
{
    return false;
}

// Checking if value in type range: extending (signed integer in unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value
                          && (sizeof(TargetType) > sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x < static_cast<ValueType>(std::numeric_limits<TargetType>::min()); // Unsigned.min = 0.
}

// Checking if value in type range: keeping (signed integer in unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value
                          && (sizeof(TargetType) == sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x < static_cast<ValueType>(std::numeric_limits<TargetType>::min()); // Unsigned.min = 0.
}

// Checking if value in type range: keeping (unsigned integer in signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value
                          && (sizeof(TargetType) == sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > std::numeric_limits<TargetType>::max();
}

// Checking if value in type range: narrowing (unsigned integer in unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_unsigned<ValueType>::value
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > std::numeric_limits<TargetType>::max();
}

// Checking if value in type range: narrowing (signed integer in signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_signed<TargetType>::value && std::is_signed<ValueType>::value
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > std::numeric_limits<TargetType>::max() || x < std::numeric_limits<TargetType>::min();
}

// Checking if value in type range: narrowing (signed integer in unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > std::numeric_limits<TargetType>::max() || x < std::numeric_limits<TargetType>::min(); // Unsigned.min = 0.
}

// Checking if value in type range: narrowing (unsigned integer in signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > (std::numeric_limits<TargetType>::max());
}

// Checking if value in type range: (floating point in integer) except (double in long), (double in unsigned long).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_floating_point<ValueType>::value
                          && !(sizeof(ValueType) == 8 && sizeof(TargetType) == sizeof(long))
                          && !(sizeof(ValueType) == 4 && (sizeof(TargetType) == sizeof(long) || sizeof(TargetType) == sizeof(int)))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{
    return x > std::numeric_limits<TargetType>::max() || x < std::numeric_limits<TargetType>::min();
}

// Checking if value in type range: (double in long), (double in unsigned long).
// Required separately because of the long::max to double conversion warning.
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 8 && sizeof(TargetType) == sizeof(long)> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{   // Checking x >= max because long::max (9223372036854775807) becomes (9223372036854775808).
    // Checking x >= max because unsigned long::max (18446744073709551615) becomes (18446744073709551616).
    return x >= static_cast<ValueType>(std::numeric_limits<TargetType>::max()) || x < std::numeric_limits<TargetType>::min();
}

// Checking if value in type range: (float in int, long, unsigned int, unsigned long).
// Required separately because of the {int, long, unsigned int, unsigned long}::max to double conversion warning.
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 4 && (sizeof(TargetType) == sizeof(long) || sizeof(TargetType) == sizeof(int))> * = nullptr>
inline constexpr bool value_out_of_type_range(ValueType x)
{   // Checking x >= max because int::max (2147483647) becomes (2147483647).
    // Checking x >= max because unsigned int::max (4294967295) becomes (4294967296).
    // Checking x >= max because long::max (9223372036854775807) becomes (9223372036854775808).
    // Checking x >= max because unsigned long::max (18446744073709551615) becomes (18446744073709551616).
         //static_cast<ValueType>
    return x >= static_cast<ValueType>(std::numeric_limits<TargetType>::max()) || x < std::numeric_limits<TargetType>::min();
}



template<typename TargetType, typename ValueType>
inline void assert_value_in_type_range(ValueType x)
{
    if (value_out_of_type_range<TargetType>(x))
        throw make_error<std::runtime_error>("value ", x, " is out of type range");
}



template<typename T>
typename std::enable_if_t<std::is_floating_point<T>::value, bool>
approximately_equal(T x, T y)
{
    const auto absDiff = std::fabs(x - y);
    return absDiff <= std::numeric_limits<T>::epsilon() * std::max(std::fabs(x), std::fabs(y))
            || absDiff < std::numeric_limits<T>::min();
}

// https://en.cppreference.com/w/cpp/types/numeric_limits/epsilon
// QUESTION Will (+x, -x) return always false (except subnormal case) and is it the expected result?
template<typename T>
typename std::enable_if_t<!std::numeric_limits<T>::is_integer, bool>
constexpr almost_equal(T x, T y, int ulp)
{
    const auto absDiff = std::fabs(x - y);
    // the machine epsilon has to be scaled to the magnitude of the values used
    // and multiplied by the desired precision in ULPs (units in the last place)
    return absDiff <= std::numeric_limits<T>::epsilon() * std::fabs(x + y) * ulp
        // unless the result is subnormal
        || absDiff < std::numeric_limits<T>::min();
}



// Keeping cast for (arithmetic to same arithmetic).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_same<TargetType, ValueType>::value && std::is_arithmetic<TargetType>::value> * = nullptr>
inline constexpr TargetType lossless_static_cast(ValueType x)
{
    return x;
}

// Keeping cast for (signed integer to unsigned integer, unsigned integer to signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && ((std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value)
                              || (std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value))
                          && (sizeof(TargetType) == sizeof(ValueType))> * = nullptr>
inline constexpr TargetType lossless_static_cast(ValueType x)
{
    assert_value_in_type_range<TargetType>(x);
    return static_cast<TargetType>(x);
}

// Extending cast for (signed integer to signed integer, unsigned integer to unsigned integer, unsigned integer to signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && ((std::is_signed<TargetType>::value && std::is_signed<ValueType>::value)
                              || (std::is_unsigned<TargetType>::value && std::is_unsigned<ValueType>::value)
                              || (std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value))
                          && (sizeof(TargetType) > sizeof(ValueType))> * = nullptr>
inline constexpr TargetType lossless_static_cast(ValueType x)
{
    return static_cast<TargetType>(x);
}

// Everything else cast for (integer to integer), i.e. extending cast for (signed integer to unsigned integer) or narrowing cast.
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && ((std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value && (sizeof(TargetType) > sizeof(ValueType)))
                              || (sizeof(TargetType) < sizeof(ValueType)))> * = nullptr>
inline constexpr TargetType lossless_static_cast(ValueType x)
{
    assert_value_in_type_range<TargetType>(x);
    return static_cast<TargetType>(x);
}

// Cast for (integer to floating point).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_floating_point<TargetType>::value && std::is_integral<ValueType>::value> * = nullptr>
inline TargetType lossless_static_cast(ValueType x)
{
    const auto result = static_cast<TargetType>(x);
    if (x != static_cast<ValueType>(result))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}

// Cast for (floating point to integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_floating_point<ValueType>::value> * = nullptr>
inline TargetType lossless_static_cast(ValueType x)
{
    ValueType xIntegralPart;
    if (!approximately_equal(std::modf(x, &xIntegralPart), ValueType(0)))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    assert_value_in_type_range<TargetType>(xIntegralPart);
    return static_cast<TargetType>(xIntegralPart);
}

// Cast for (float to double).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_floating_point<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 4 && sizeof(TargetType) == 8> * = nullptr>
inline TargetType lossless_static_cast(ValueType x)
{
    return static_cast<TargetType>(x);
}

// Cast for (double to float).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_floating_point<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 8 && sizeof(TargetType) == 4> * = nullptr>
inline TargetType lossless_static_cast(ValueType x)
{
    const auto result = static_cast<TargetType>(x);
    if ((std::isinf(result) && !std::isinf(x)) || !approximately_equal(x, static_cast<ValueType>(result)))
        throw make_error<std::runtime_error>("Can not convert: value ", x, " loses precision");
    return result;
}



// Keeping cast for (integer to same integer).
template<typename T, typename U,
         std::enable_if_t<std::is_same<T, U>::value && std::is_integral<T>::value> * = nullptr>
inline constexpr T limited_static_cast(U x)
{
    return x;
}

// Keeping cast for (signed integer to unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value
                          && (sizeof(TargetType) == sizeof(ValueType))> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x < static_cast<ValueType>(std::numeric_limits<TargetType>::min()))
        return std::numeric_limits<TargetType>::min();
    return static_cast<TargetType>(x);
}

// Keeping cast for (unsigned integer to signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value
                          && (sizeof(TargetType) == sizeof(ValueType))> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x > static_cast<ValueType>(std::numeric_limits<TargetType>::max()))
        return std::numeric_limits<TargetType>::max();
    return static_cast<TargetType>(x);
}

// Extending cast for (signed integer to signed integer, unsigned integer to unsigned integer, unsigned integer to signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && ((std::is_signed<TargetType>::value && std::is_signed<ValueType>::value)
                              || (std::is_unsigned<TargetType>::value && std::is_unsigned<ValueType>::value)
                              || (std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value))
                          && (sizeof(TargetType) > sizeof(ValueType))> * = nullptr>
inline constexpr TargetType limited_static_cast(ValueType x)
{
    return static_cast<TargetType>(x);
}

// Extending cast for (signed integer to unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_unsigned<TargetType>::value && std::is_signed<ValueType>::value && (sizeof(TargetType) > sizeof(ValueType))> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x < static_cast<ValueType>(std::numeric_limits<TargetType>::min())) // Unsigned.min = 0.
        return std::numeric_limits<TargetType>::min();
    return static_cast<TargetType>(x);
}

// Narrowing cast for (signed integer to signed integer, signed integer to unsigned integer, unsigned integer to unsigned integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && !(std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value)
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x > static_cast<ValueType>(std::numeric_limits<TargetType>::max()))
        return std::numeric_limits<TargetType>::max();
    if (x < static_cast<ValueType>(std::numeric_limits<TargetType>::min()))
        return std::numeric_limits<TargetType>::min();
    return static_cast<TargetType>(x);
}

// Narrowing cast for (unsigned integer to signed integer).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_integral<TargetType>::value && std::is_integral<ValueType>::value
                          && std::is_signed<TargetType>::value && std::is_unsigned<ValueType>::value
                          && (sizeof(TargetType) < sizeof(ValueType))> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x > static_cast<ValueType>(std::numeric_limits<TargetType>::max()))
        return std::numeric_limits<TargetType>::max();
    return static_cast<TargetType>(x);
}

// Cast for (float to double).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_floating_point<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 4 && sizeof(TargetType) == 8> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    return static_cast<TargetType>(x);
}

// Cast for (double to float).
template<typename TargetType, typename ValueType,
         std::enable_if_t<std::is_floating_point<TargetType>::value && std::is_floating_point<ValueType>::value
                          && sizeof(ValueType) == 8 && sizeof(TargetType) == 4> * = nullptr>
inline TargetType limited_static_cast(ValueType x)
{
    if (x > static_cast<ValueType>(std::numeric_limits<TargetType>::max()))
        return std::numeric_limits<TargetType>::max();
    if (x < static_cast<ValueType>(std::numeric_limits<TargetType>::min()))
        return std::numeric_limits<TargetType>::min();
    return static_cast<TargetType>(x);
}



template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T round_AS(U x)
{
    return lossless_static_cast<T>(std::round(x));
}

template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T floor_AS(U x)
{
    return lossless_static_cast<T>(std::floor(x));
}

template<typename T,
         typename U,
         std::enable_if_t<std::is_integral<T>::value> * = nullptr,
         std::enable_if_t<std::is_floating_point<U>::value> * = nullptr>
inline T ceil_AS(U x)
{
    return lossless_static_cast<T>(std::ceil(x));
}



#define longsizeof(x) (static_cast<long>(sizeof(x)))
#define intsizeof(x) (static_cast<int>(sizeof(x)))
#define checkedlongsizeof(x) (lossless_static_cast<long>(sizeof(x)))
#define checkedintsizeof(x) (lossless_static_cast<int>(sizeof(x)))



constexpr const char* Module_divisor_is_0 = "Can not calculate modulo when divisor is 0";

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
inline int modulo2Power(int value, unsigned powerOf2)
{
    if (powerOf2 == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    return value & static_cast<int>(powerOf2 - 1);
}

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
// Slightly faster than modulo(int, int).
inline int modulo(int value, unsigned m)
{
    if (m == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    int mod = value % static_cast<int>(m);
    if (value < 0) {
        mod += m;
    }
    return mod;
}

// https://stackoverflow.com/questions/14997165/fastest-way-to-get-a-positive-modulo-in-c-c
inline int modulo_euclidean(int value, int m) // modulo_Euclidean2
{
    if (m == 0)
        throw make_error<std::runtime_error>(Module_divisor_is_0);
    if (m == -1)
        return 0; // This test needed to prevent UB of `INT_MIN % -1`.
    int mod = value % m;
    if (mod < 0)
        mod = (m < 0) ? mod - m : mod + m;
    return mod;
}

#endif // CPP_HELPERS_H
