#ifndef INTERVAL_H
#define INTERVAL_H

#include "utils.h"

#include <cassert>
#include <cmath>
#include <stdexcept>

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

inline std::ostream& operator<<(std::ostream& os, const Interval<int>& interval) { return os << interval.ToString(); }

#endif // INTERVAL_H
