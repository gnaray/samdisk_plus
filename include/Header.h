#pragma once

#include "Interval.h"
#include "DiskConstants.h"
#include "utils.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <map>
#include <set>

enum class DataRate : int { Unknown = 0, _250K = 250'000, _300K = 300'000, _500K = 500'000, _1M = 1'000'000 };
enum class Encoding { Unknown, MFM, FM, RX02, Amiga, GCR, Ace, MX, Agat, Apple, Victor, Vista };

std::string to_string(const DataRate& datarate);
std::string to_string(const Encoding& encoding);
std::string short_name(const Encoding& encoding);
DataRate datarate_from_string(std::string str);
Encoding encoding_from_string(std::string str);
constexpr bool are_interchangeably_equal_datarates(const DataRate& datarate1, const DataRate& datarate2)
{
    return ((datarate1 == DataRate::_250K || datarate1 == DataRate::_300K)
            && (datarate2 == DataRate::_250K || datarate2 == DataRate::_300K));
}

inline int bitcell_ns(DataRate datarate)
{
    switch (datarate)
    {
    case DataRate::Unknown: break;
    case DataRate::_250K:   return 2000;
    case DataRate::_300K:   return 1667;
    case DataRate::_500K:   return 1000;
    case DataRate::_1M:     return 500;
    }

    return 0;
}

constexpr int bits_per_second(DataRate datarate)
{
    return static_cast<int>(datarate);
}

constexpr int convert_offset_by_datarate(int offset, const DataRate& datarate_source, const DataRate& datarate_target)
{
    // The offset = reltime * datarate * constant, thus new offset = new datarate * old offset / old datarate.
    // Dividing and multiplying with constant in order to avoid overflow.
    return bits_per_second(datarate_target) / 10000 * offset / bits_per_second(datarate_source) * 10000;
}

inline bool are_offsets_tolerated_same(const int offset1, const int offset2, const int byte_tolerance_of_time, const int tracklen)
{
    if (tracklen == 0 || offset1 == 0 || offset2 == 0) // The offset is 0 exactly when tracklen is 0 but safer to check both.
        return true; // No offsets to compare, they are considered same.
    const auto offset_min = std::min(offset1, offset2);
    const auto offset_max = std::max(offset1, offset2);
    auto distance = std::min(offset_max - offset_min, tracklen + offset_min - offset_max);

    // Offsets must be close enough.
    return distance <= byte_tolerance_of_time * 16;
}



inline std::ostream& operator<<(std::ostream& os, const DataRate dr) { return os << to_string(dr); }
inline std::ostream& operator<<(std::ostream& os, const Encoding e) { return os << to_string(e); }

//////////////////////////////////////////////////////////////////////////////

struct CylHead
{
    CylHead() = default;
    CylHead(int cyl_, int head_) : cyl(cyl_), head(head_)
    {
        assert(cyl >= 0 && cyl < MAX_DISK_CYLS);
        assert(head >= 0 && head < MAX_DISK_HEADS);
    }

    operator int() const;

    std::string to_string() const
    {
#if 0   // ToDo
        if (opt_hex == 1) // TODO: former opt.hex is now opt_hex and not available, because Options.h should be included which includes Header.h, cyclic dependency.
            return util::format("cyl %02X head %u", cyl, head);
#endif

        return util::fmt("cyl %u head %u", cyl, head);
    }

    CylHead next_cyl()
    {
        CylHead cylhead(*this);
        ++cyl;
        assert(cyl < MAX_DISK_CYLS);
        return cylhead;
    }

    int cyl = -1, head = -1;
};

CylHead operator * (const CylHead& cylhead, int cyl_step);
inline std::ostream& operator<<(std::ostream& os, const CylHead& cylhead) { return os << cylhead.to_string(); }

//////////////////////////////////////////////////////////////////////////////

class Header
{
public:
    Header() = default;
    Header(int cyl, int head, int sector, int size);
    Header(const CylHead& cylhead, int sector, int size);

    operator CylHead() const;

    int sector_size() const;
    constexpr bool compare_chrn(const Header& rhs) const
    {
        return cyl == rhs.cyl &&
            head == rhs.head &&
            sector == rhs.sector &&
            size == rhs.size;
    }
    bool compare_crn(const Header& rhs) const;
    bool compare_chr(const Header& rhs) const;

    inline bool empty() const
    {
        return cyl == 0 && head == 0 && sector == 0 && size == 0;
    }

    int cyl = 0, head = 0, sector = 0, size = 0;
};

constexpr bool operator== (const Header& lhs, const Header& rhs)
{
    return lhs.compare_chrn(rhs); // NOTE This was compare_crn originally but TODO questioned if use compare_chrn?
}

constexpr bool operator <(const Header& lhs, const Header& rhs)
{
    return lhs.cyl < rhs.cyl || (lhs.cyl == rhs.cyl && (lhs.head < rhs.head
        || (lhs.head == rhs.head && (lhs.sector < rhs.sector
            || (lhs.sector == rhs.sector && lhs.size < rhs.size)))));
}

constexpr bool operator !=(const Header& lhs, const Header& rhs)
{
    return !(lhs == rhs);
}
constexpr bool operator >=(const Header& lhs, const Header& rhs)
{
    return !(lhs < rhs);
}
constexpr bool operator >(const Header& lhs, const Header& rhs)
{
    return rhs < lhs;
}
constexpr bool operator <=(const Header& lhs, const Header& rhs)
{
    return !(lhs > rhs);
}

//////////////////////////////////////////////////////////////////////////////

class Headers : public std::vector<Header>
{
public:
    Headers() = default;

    bool Contains(const Header& header) const;
    std::set<int> NotContainedIds(const Interval<int>& id_interval) const;
    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const Headers& headers, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << headers.ToString(onlyRelevantData);
        return ss.str();
    }
    std::string SectorIdsToString() const;
    bool HasIdSequence(const int first_id, const int length) const;
};
