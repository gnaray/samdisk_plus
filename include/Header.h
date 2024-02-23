#pragma once

#include "Interval.h"
#include "DiskConstants.h"
#include "VectorX.h"
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

/* bitcell_ns = ns/bitcell = tracktime / tracklen.bit = (tracklen.bit * mfmbit_us) / tracklen.bit = mfmbit_us.
 * Basically this is the same as GetFmOrMfmBitsTime(m_lastDataRate, 1) because bitcell is a rawbit.
 */
inline int bitcell_ns(DataRate datarate)
{   // The result is 1000000000 / datarate / 2 = 500000000 / datarate.
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

constexpr int DataBitPositionAsBitOffset(const int bitPosition, const Encoding& encoding)
{
    return bitPosition << (encoding == Encoding::FM ? 2 : 1);
}

constexpr int DataBytePositionAsBitOffset(const int bytePosition, const Encoding& encoding)
{
    return DataBitPositionAsBitOffset(bytePosition, encoding) * 8;
}

constexpr int BitOffsetAsDataBitPosition(const int offset, const Encoding& encoding)
{
    return offset >> (encoding == Encoding::FM ? 2 : 1);
}

constexpr int BitOffsetAsDataBytePosition(const int offset, const Encoding& encoding)
{
    return BitOffsetAsDataBitPosition(offset, encoding) / 8;
}

inline bool are_offsets_tolerated_same(const int offset1, const int offset2, const Encoding& encoding, const int byte_tolerance_of_time, const int tracklen)
{
    const auto offset_min = std::min(offset1, offset2);
    const auto offset_max = std::max(offset1, offset2);
    auto distance = offset_max - offset_min;
    if (tracklen > 0)
        distance = std::min(distance, tracklen + offset_min - offset_max);

    // Offsets must be close enough.
    return distance <= DataBytePositionAsBitOffset(byte_tolerance_of_time, encoding);
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

    CylHead next_cyl()
    {
        CylHead cylhead(*this);
        ++cyl;
        assert(cyl < MAX_DISK_CYLS);
        return cylhead;
    }

    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const CylHead& header, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << header.ToString(onlyRelevantData);
        return ss.str();
    }

    int cyl = -1, head = -1;
};

CylHead operator * (const CylHead& cylhead, int cyl_step);
inline std::ostream& operator<<(std::ostream& os, const CylHead& cylhead) { return os << cylhead.ToString(); }

//////////////////////////////////////////////////////////////////////////////

class Header
{
public:
    Header() = default;
    constexpr Header(int cyl, int head, int sector, int size)
        : cyl(cyl), head(head), sector(sector), size(size)
    {
    }

    constexpr Header(const CylHead& cylhead, int sector, int size)
        : cyl(cylhead.cyl), head(cylhead.head), sector(sector), size(size)
    {
    }

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

    std::string GetRecordAsString() const;

    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const Header& header, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << header.ToString(onlyRelevantData);
        return ss.str();
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

inline std::ostream& operator<<(std::ostream& os, const Header& header) { return os << header.ToString(); }

//////////////////////////////////////////////////////////////////////////////

class Headers : public VectorX<Header>
{
public:
    Headers() = default;

    bool Contains(const Header& header) const;
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

inline std::ostream& operator<<(std::ostream& os, const Headers& headers) { return os << headers.ToString(); }
