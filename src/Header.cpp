#include "Header.h"
#include "Sector.h"

#include <iterator>
#include <algorithm>
#include <map>

std::string to_string(const DataRate& datarate)
{
    switch (datarate)
    {
    case DataRate::_250K:   return "250Kbps";
    case DataRate::_300K:   return "300Kbps";
    case DataRate::_500K:   return "500Kbps";
    case DataRate::_1M:     return "1Mbps";
    case DataRate::Unknown: break;
    }
    return "Unknown";
}

std::string to_string(const Encoding& encoding)
{
    switch (encoding)
    {
    case Encoding::MFM:     return "MFM";
    case Encoding::FM:      return "FM";
    case Encoding::RX02:    return "RX02";
    case Encoding::Amiga:   return "Amiga";
    case Encoding::GCR:     return "GCR";
    case Encoding::Ace:     return "Ace";
    case Encoding::MX:      return "MX";
    case Encoding::Agat:    return "Agat";
    case Encoding::Apple:   return "Apple";
    case Encoding::Victor:  return "Victor";
    case Encoding::Vista:   return "Vista";
    case Encoding::Unknown: break;
    }
    return "Unknown";
}

std::string short_name(const Encoding& encoding)
{
    switch (encoding)
    {
    case Encoding::MFM:     return "mfm";
    case Encoding::FM:      return "fm";
    case Encoding::RX02:    return "rx";
    case Encoding::Amiga:   return "ami";
    case Encoding::GCR:     return "gcr";
    case Encoding::Ace:     return "ace";
    case Encoding::MX:      return "mx";
    case Encoding::Agat:    return "agat";
    case Encoding::Apple:   return "a2";
    case Encoding::Victor:  return "vic";
    case Encoding::Vista:   return "vis";
    case Encoding::Unknown: break;
    }
    return "unk";
}


DataRate datarate_from_string(std::string str)
{
    str = util::lowercase(str);
    auto len = str.size();

    if (str == std::string("250kbps").substr(0, len)) return DataRate::_250K;
    if (str == std::string("300kbps").substr(0, len)) return DataRate::_300K;
    if (str == std::string("500kbps").substr(0, len)) return DataRate::_500K;
    if (str == std::string("1mbps").substr(0, len)) return DataRate::_1M;
    return DataRate::Unknown;
}

Encoding encoding_from_string(std::string str)
{
    str = util::lowercase(str);

    if (str == "mfm") return Encoding::MFM;
    if (str == "fm") return Encoding::FM;
    if (str == "gcr") return Encoding::GCR;
    if (str == "amiga") return Encoding::Amiga;
    if (str == "ace") return Encoding::Ace;
    if (str == "mx") return Encoding::MX;
    if (str == "agat") return Encoding::Agat;
    if (str == "apple") return Encoding::Apple;
    if (str == "victor") return Encoding::Victor;
    if (str == "vista") return Encoding::Vista;
    return Encoding::Unknown;
}

//////////////////////////////////////////////////////////////////////////////

CylHead::operator int() const
{
    return (cyl * MAX_DISK_HEADS) + head;
}

CylHead operator * (const CylHead& cylhead, int cyl_step)
{
    return CylHead(cylhead.cyl * cyl_step, cylhead.head);
}

//////////////////////////////////////////////////////////////////////////////

Header::Header(int cyl_, int head_, int sector_, int size_)
    : cyl(cyl_), head(head_), sector(sector_), size(size_)
{
}

Header::Header(const CylHead& cylhead, int sector_, int size_)
    : cyl(cylhead.cyl), head(cylhead.head), sector(sector_), size(size_)
{
}

bool Header::operator== (const Header& rhs) const
{
    return compare_crn(rhs);    // ToDo: use compare_chrn?
}

bool Header::operator!= (const Header& rhs) const
{
    return !compare_crn(rhs);
}

Header::operator CylHead() const
{
    return CylHead(cyl, head);
}

bool Header::compare_chrn(const Header& rhs) const
{
    return cyl == rhs.cyl &&
        head == rhs.head &&
        sector == rhs.sector &&
        size == rhs.size;
}

bool Header::compare_crn(const Header& rhs) const
{
    // Compare without head match, like WD17xx
    return cyl == rhs.cyl &&
        sector == rhs.sector &&
        size == rhs.size;
}

int Header::sector_size() const
{
    return Sector::SizeCodeToLength(size);
}

//////////////////////////////////////////////////////////////////////////////

bool Headers::Contains(const Header& header) const
{
    return std::find(cbegin(), cend(), header) != cend();
}

std::string Headers::ToString(bool onlyRelevantData/* = true*/) const
{
    std::ostringstream ss;
    if (!onlyRelevantData || !empty())
    {
        bool writingStarted = false;
        std::for_each(cbegin(), cend(), [&](const Header& header)
        {
            if (writingStarted)
                ss << ' ';
            else
                writingStarted = true;
            ss << header;
        });
    }
    return ss.str();
}

std::string Headers::SectorIdsToString() const
{
    std::ostringstream ss;
    bool writingStarted = false;
    std::for_each(cbegin(), cend(), [&](const Header& header)
    {
        if (writingStarted)
            ss << ' ';
        else
            writingStarted = true;
        ss << header.sector;
    });
    return ss.str();
}

bool Headers::HasIdSequence(const int first_id, const int length) const
{
    assert(length >= 0);
    typedef std::vector<bool> BoolVector;
    typedef BoolVector::size_type BoolVectorST;
    const auto u_length = lossless_static_cast<BoolVectorST>(length);
    if (size() < u_length) // No chance for long enough sequence.
        return false;
    BoolVector sequenceFlags(u_length);
    // Check the sequence of first_id, first_id+1, ..., first_id + length - 1
    for_each(begin(), end(), [&](const Header& header)
    {
        const auto headerSector = header.sector;
        if (headerSector < first_id || headerSector >= first_id + length)
            return;
        sequenceFlags[lossless_static_cast<BoolVectorST>(headerSector - first_id)] = true; // Multiple ids are OK, the id is still found.
    });
    return std::all_of(sequenceFlags.cbegin(), sequenceFlags.cend(), [](bool marked) {return marked;});
}
