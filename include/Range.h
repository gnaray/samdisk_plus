#pragma once

#include "Header.h"

#include <functional>

class Range
{
public:
    Range() = default;
    Range(int cyls, int heads);
    Range(int cyl_begin_, int cyl_end_, int head_begin, int head_end);

    bool empty() const;
    int cyls() const;
    int heads() const;
    bool contains(const CylHead& cylhead);
    void each(const std::function<void(const CylHead & cylhead)>& func, bool cyls_first = false) const;

    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const Range& r, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << r.ToString(onlyRelevantData);
        return ss.str();
    }

    int cyl_begin = 0, cyl_end = 0;
    int head_begin = 0, head_end = 0;
};

inline std::ostream& operator<<(std::ostream& os, const Range& r) { return os << r.ToString(); }
