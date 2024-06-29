#pragma once

#include <ostream>
#include <sstream>

class RetryPolicy
{
public:
    RetryPolicy() = default;
    RetryPolicy(const int retryTimesOption)
        : RetryPolicy(retryTimesOption >= 0 ? retryTimesOption : -retryTimesOption,
            retryTimesOption >= 0 ? false : true)
    {
    }

    constexpr RetryPolicy(const int retryTimes_, const bool _sinceLastChange)
        : retryTimesInitial(retryTimes_), sinceLastChange(_sinceLastChange),
        retryTimes(sinceLastChange ? retryTimes_ - 1 : retryTimes_)
    {
    }

    constexpr bool GetSinceLastChange() const
    {
        return sinceLastChange;
    }

    void InvertSinceLastChange()
    {
        if (sinceLastChange)
        {
            sinceLastChange = false;
            retryTimes = --retryTimesInitial;
        }
        else
        {
            sinceLastChange = true;
            retryTimes = retryTimesInitial++;
        }
    }

    constexpr bool operator ==(const RetryPolicy& rhs) const
    {
        return retryTimes == rhs.retryTimes && sinceLastChange == rhs.sinceLastChange;
    }

    // Ordered by retryTimes, sinceLastChange incremented.
    constexpr bool operator <(const RetryPolicy& rhs) const
    {
        return retryTimes < rhs.retryTimes || (retryTimes == rhs.retryTimes && (sinceLastChange < rhs.sinceLastChange));
    }

    constexpr bool operator ==(const int retryTimes_) const
    {
        return this->retryTimes == retryTimes_;
    }

    // Ordered by retryTimes incremented.
    constexpr bool operator <(const int retryTimes_) const
    {
        return this->retryTimes < retryTimes_;
    }

    // prefix increment
    inline RetryPolicy& operator++()
    {
        retryTimes++; // actual increment takes place here
        return *this; // return new value by reference
    }

    // postfix increment
    inline RetryPolicy operator++(int)
    {
        RetryPolicy old = *this; // copy old value
        operator++();  // prefix increment
        return old;    // return old value
    }

    // prefix decrement
    inline RetryPolicy& operator--()
    {
        retryTimes--; // actual decrement takes place here
        return *this; // return new value by reference
    }

    // postfix decrement
    inline RetryPolicy operator--(int)
    {
        RetryPolicy old = *this; // copy old value
        operator--();  // prefix decrement
        return old;    // return old value
    }

    inline RetryPolicy& operator+=(const int retryTimes_) // compound assignment (does not need to be a member,
    {                                                   // but often is, to modify the private members)
        this->retryTimes += retryTimes_; /* addition of rhs to *this takes place here */
        return *this; // return the result by reference
    }

    // friends defined inside class body are inline and are hidden from non-ADL lookup
    friend RetryPolicy operator+(RetryPolicy lhs,       // passing lhs by value helps optimize chained a+b+c
                                 const int retryTimes_)  // otherwise, both parameters may be const references
    {
        lhs += retryTimes_; // reuse compound assignment
        return lhs; // return the result by value (uses move constructor)
    }

    inline RetryPolicy& operator-=(const int retryTimes_) // compound assignment (does not need to be a member,
    {                                                   // but often is, to modify the private members)
        this->retryTimes -= retryTimes_; /* addition of rhs to *this takes place here */
        return *this; // return the result by reference
    }

    // friends defined inside class body are inline and are hidden from non-ADL lookup
    friend RetryPolicy operator-(RetryPolicy lhs,       // passing lhs by value helps optimize chained a+b+c
                                 const int retryTimes_)  // otherwise, both parameters may be const references
    {
        lhs -= retryTimes_; // reuse compound assignment
        return lhs; // return the result by value (uses move constructor)
    }

    bool HasMoreRetry()
    {
        if (sinceLastChange && wasChange)
        {
            wasChange = false;
            retryTimes = retryTimesInitial;
        }
        return retryTimes > 0;
    }

    bool HasMoreRetryMinusMinus()
    {
        const auto result = HasMoreRetry();
        operator --();
        return result;
    }

    bool HasMoreMinusMinusRetry()
    {
        operator --();
        return HasMoreRetry();
    }

    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const RetryPolicy& retryPolicy, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << retryPolicy.ToString(onlyRelevantData);
        return ss.str();
    }

protected:
    int retryTimesInitial = 0;
    bool sinceLastChange = false;
public:
    int retryTimes = 0;
    bool wasChange = false;
};

inline std::ostream& operator<<(std::ostream& os, const RetryPolicy& r) { return os << r.ToString(); }

constexpr bool operator !=(const RetryPolicy& lhs, const RetryPolicy& rhs)
{
    return !(lhs == rhs);
}
constexpr bool operator >=(const RetryPolicy& lhs, const RetryPolicy& rhs)
{
    return !(lhs < rhs);
}
constexpr bool operator >(const RetryPolicy& lhs, const RetryPolicy& rhs)
{
    return rhs < lhs;
}
constexpr bool operator <=(const RetryPolicy& lhs, const RetryPolicy& rhs)
{
    return !(lhs > rhs);
}



constexpr bool operator !=(const RetryPolicy& lhs, const int retryTimes)
{
    return !(lhs == retryTimes);
}

constexpr bool operator >=(const RetryPolicy& lhs, const int retryTimes)
{
    return !(lhs < retryTimes);
}

constexpr bool operator <=(const RetryPolicy& lhs, const int retryTimes)
{
    return lhs < retryTimes || lhs == retryTimes;
}

constexpr bool operator >(const RetryPolicy& lhs, const int retryTimes)
{
    return !(lhs <= retryTimes);
}
