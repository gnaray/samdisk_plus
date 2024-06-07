#ifndef INTERVAL_H
#define INTERVAL_H

#include "utils.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

/*
 * First interval is [left1, right1], where left1 <= right1.
 * Second interval is [left2, right2], where left2 <= right2.
 */
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
constexpr bool IsIntersectingLeftRight(const T left1, const T right1, const T left2, const T right2)
{
    return left1 <= right2 && left2 <= right1;
}

/*
 * First interval is [s1, e1], where s1 can be > e1.
 * Second interval is [s2, e2], where s2 can be > e2.
 */
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
constexpr bool IsIntersecting(const T s1, const T e1, const T s2, const T e2)
{
    const auto left1 = std::min(s1, e1);
    const auto right1 = std::max(s1, e1);
    const auto left2 = std::min(s2, e2);
    const auto right2 = std::max(s2, e2);
    return IsIntersectingLeftRight(left1, right1, left2, right2);
}

/*
* First interval is [s1, e1], where s1 can be > e1 because of ringed interval.
* Second interval is [s2, e2], where s2 can be > e2 because of ringed interval.
*/
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool IsRingedWithin(const T s1, const T e1, const T x)
{
    auto result = false;
    if (s1 <= e1) // [s1,e1] is not ringed.
        result = s1 <= x && x <= e1;
    else // [s1,e1] is ringed.
        result = s1 <= x || x <= e1;
    return result;
}

/*
 * First interval is [s1, e1], where s1 can be > e1 because of ringed interval.
 * Second interval is [s2, e2], where s2 can be > e2 because of ringed interval.
 */
template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
inline bool IsRingedIntersecting(const T s1, const T e1, const T s2, const T e2)
{
    auto result = false;
    if (s1 <= e1 && s2 <= e2) // [s1,e1], [s2,e2] are not ringed.
        result = s1 <= e2 && s2 <= e1;
    else if ((s1 > e1 && s2 <= e2) || (s1 <= e1 && s2 > e2)) // [s1,e1] is ringed, [s2,e2] is not ringed or inversed.
        result = s1 <= e2 || s2 <= e1;
    else if (s1 > e1 && s2 > e2) // [s1,e1], [s2,e2] are ringed.
        result = true; // Intersects at ringing.
    return result;
}


class BaseInterval
{
public:
    enum Location { Less = -1, Within = 0, Greater = 1 };
    enum ConstructMode { StartAndEnd, StartAndLength, StartAndSize };
};

template<typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
class Interval : public BaseInterval
{
private:
    T _start = 0;
    T _end = 0;
    bool _empty = true;

public:
    constexpr Interval() = default;

    Interval(const T start, const T other, const ConstructMode constructMode)
    {
        assert(constructMode == StartAndEnd || constructMode == StartAndLength || constructMode == StartAndSize);
        switch (constructMode)
        {
        case StartAndEnd:
            _start = start;
            _end = other;
            _empty = false;
            break;
        case StartAndLength: // other is length.
            _start = start;
            _end = _start + other;
            _empty = false;
            break;
        case StartAndSize: // other is size.
            _start = start;
            _end = _start + other + (other > 0 ? - 1 : 1);
            _empty = other == 0;
            break;
        }
    }

    /**
     * Where is x relative to the interval?
     */
    Location Where(const T x) const
    {
        // Empty interval is incomparable.
        if (_empty)
            throw std::domain_error("No location based on empty interval");

        const auto left = std::min(_start, _end);
        const auto right = std::max(_start, _end);
        if (x > right)
            return Greater;
        else if (x < left)
            return Less;
        else if (x >= left && x <= right)
            return Within;

        // It's NaN or incomparable.
        throw std::domain_error("Cannot determine location of NaN");
    }

    constexpr T Length() const
    {
        if (_empty)
            throw std::domain_error("No length of empty interval");
        return _end - _start;
    }

    constexpr T AbsLength() const
    {
        return std::abs(Length());
    }

    /*== Accessors ==*/

    /**
     * Start bound.
     */
    constexpr T Start() const
    {
        if (_empty)
            throw std::domain_error("No start of empty interval");
        return _start;
    }

    /**
     * End bound.
     */
    constexpr T End() const
    {
        if (_empty)
            throw std::domain_error("No end of empty interval");
        return _end;
    }

    constexpr bool IsEmpty() const
    {
        return _empty;
    }

    inline friend bool IsIntersectingF(const Interval<T>& lhs, const Interval<T>& rhs)
    {
        if (lhs._empty || rhs._empty)
            return false;
        return ::IsIntersecting(lhs._start, lhs._end, rhs._start, rhs._end);
    }

    inline bool IsIntersecting(const Interval<T>& rhs) const
    {
        return IsIntersectingF(*this, rhs);
    }

    inline friend bool IsRingedWithinF(const Interval<T>& lhs, const T x)
    {
        if (lhs._empty)
            return false;
        return ::IsRingedWithin(lhs._start, lhs._end, x);
    }

    inline bool IsRingedWithin(const T x) const
    {
        return IsRingedWithinF(*this, x);
    }

    inline friend bool IsRingedIntersectingF(const Interval<T>& lhs, const Interval<T>& rhs)
    {
        if (lhs._empty || rhs._empty)
            return false;
        return ::IsRingedIntersecting(lhs._start, lhs._end, rhs._start, rhs._end);
    }

    inline bool IsRingedIntersecting(const Interval<T>& rhs) const
    {
        return IsRingedIntersectingF(*this, rhs);
    }

    std::string ToString(bool onlyRelevantData = true) const
    {
        std::ostringstream ss;
        if (!onlyRelevantData || !IsEmpty())
        {
            if (IsEmpty())
                ss << "[EMPTY]";
            else
                ss << "[" << _start << ", " << _end << "]";
        }
        return ss.str();
    }

    friend std::string to_string(const Interval<T>& interval, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << interval.ToString(onlyRelevantData);
        return ss.str();
    }
};

template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
constexpr T Size(const Interval<T>& interval)
{
    const auto length = interval.Length();
    return length + (length >= 0 ? 1 : -1);
}

inline std::ostream& operator<<(std::ostream& os, const Interval<int>& interval) { return os << interval.ToString(); }

#endif // INTERVAL_H
