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
    enum ConstructMode { LeftAndRight, LeftAndLength, LeftAndSize };
};

template <class T>
class Interval : public BaseInterval
{
private:
    T _left = 0, _right = 0;
    bool _empty = true;

public:
    Interval()
        : _left(0), _right(0), _empty(true)
    {
    }

    Interval(const T left, const T other, const ConstructMode constructMode)
    {
        assert(constructMode == LeftAndRight || constructMode == LeftAndLength || constructMode == LeftAndSize);
        switch (constructMode)
        {
        case LeftAndRight:
            // Always set left as min and right as max to simplify assumptions.
            _left = std::min(left, other);
            _right = std::max(left, other);
            _empty = false;
            break;
        case LeftAndLength: // other is length.
            _left = left;
            _right = _left + other;
            _empty = other < 0;
            break;
        case LeftAndSize: // other is size.
            _left = left;
            _empty = other <= 0;
            _right = _left + other - 1;
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

        if (x > _right)
            return Greater;
        else if (x < _left)
            return Less;
        else if (x >= _left && x <= _right)
            return Within;

        // It's NaN or incomparable.
        throw std::domain_error("Cannot determine location of NaN");
    }

    constexpr T Length() const
    {
        if (_empty)
            throw std::domain_error("No length of empty interval");
        return _right - _left;
    }

    template <std::enable_if_t<std::is_integral<T>::value, bool> = true>
    constexpr T Size() const
    {
        return _empty ? 0 : _right - _left + 1;
    }

    /*== Accessors ==*/

    /**
     * Leftmost bound.
     */
    constexpr T Left() const
    {
        if (_empty)
            throw std::domain_error("No left side of empty interval");
        return _left;
    }

    /**
     * Rightmost bound.
     */
    constexpr T Right() const
    {
        if (_empty)
            throw std::domain_error("No right side of empty interval");
        return _right;
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
                ss << "[" << _left << ", " << _right << "]";
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

inline std::ostream& operator<<(std::ostream& os, const Interval<int>& interval) { return os << to_string(interval); }

#endif // INTERVAL_H
