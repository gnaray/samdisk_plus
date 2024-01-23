#include "PlatformConfig.h"
#include "Track.h"
#include "Options.h"
//#include "DiskUtil.h"
#include "IBMPCBase.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>

static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");

Track::Track(int num_sectors/*=0*/)
{
    m_sectors.reserve(lossless_static_cast<SectorsST>(num_sectors));
}

bool Track::empty() const
{
    return m_sectors.empty();
}

int Track::size() const
{
    return static_cast<int>(m_sectors.size());
}
/*
EncRate Track::encrate(EncRate preferred) const
{
std::map<EncRate, int> freq;

for (auto sector : m_sectors)
++freq[sector.encrate];

auto it = std::max_element(freq.begin(), freq.end(), [] (const std::pair<EncRate, int> &a, const std::pair<EncRate, int> &b) {
return a.second < b.second;
});

if (it == freq.end() || it->second == freq[preferred])
return preferred;

return it->first;
}
*/

const Sectors &Track::sectors() const
{
    return m_sectors;
}

Sectors& Track::sectors()
{
    return m_sectors;
}

const Sectors &Track::sectors_view_ordered_by_id() const
{
    m_sectors_view_ordered_by_id = m_sectors;
    std::sort(m_sectors_view_ordered_by_id.begin(), m_sectors_view_ordered_by_id.end(),
        [](const Sector& s1, const Sector& s2) {
            return s1.header.sector < s2.header.sector;
        }
    );
    return m_sectors_view_ordered_by_id;
}

const Sector& Track::operator [] (int index) const
{
    assert(index < static_cast<int>(m_sectors.size()));
    return m_sectors[lossless_static_cast<Sectors::size_type>(index)];
}

Sector& Track::operator [] (int index)
{
    assert(index < static_cast<int>(m_sectors.size()));
    return m_sectors[lossless_static_cast<Sectors::size_type>(index)];
}

int Track::index_of(const Sector& sector) const
{
    auto it = std::find_if(begin(), end(), [&](const Sector& s) {
        return &s == &sector;
        });

    return (it == end()) ? -1 : static_cast<int>(std::distance(begin(), it));
}

/* The data extent bits of a sector are the bits between offset of next sector
 * (if exists otherwise tracklen) and offset of this sector.
 */
int Track::data_extent_bits(const Sector& sector) const
{
    auto it = find(sector);
    assert(it != end());

    auto drive_speed = (sector.datarate == DataRate::_300K) ? RPM_TIME_360 : RPM_TIME_300;
    auto track_len = tracklen ? tracklen : GetTrackCapacity(drive_speed, sector.datarate, sector.encoding);

    // Approximate bit distance to next ID header.
    auto gap_bits = ((std::next(it) != end()) ? std::next(it)->offset : (track_len + begin()->offset)) - sector.offset;
    return gap_bits;
}

/* The data extent bytes of a sector are the bytes between offset of next sector
 * (if exists otherwise tracklen) and offset of this sector without the parts
 * before data bytes and without idam overhead of next sector, i.e. they are
 * the bytes from the first data byte until idam overhead of next sector,
 * i.e. data bytes + data crc + gap3 + sync.
 */
int Track::data_extent_bytes(const Sector& sector) const
{
    // We only support real data extent for MFM and FM sectors.
    if (sector.encoding != Encoding::MFM && sector.encoding != Encoding::FM)
        return sector.size();

    const auto encoding_shift = (sector.encoding == Encoding::FM) ? 5 : 4;
    const auto gap_bytes = data_extent_bits(sector) >> encoding_shift;
    const auto overhead_bytes = GetFmOrMfmSectorOverheadWithoutSyncAndDataCrc(sector.datarate, sector.encoding);
    const auto extent_bytes = (gap_bytes > overhead_bytes) ? gap_bytes - overhead_bytes : 0;
    return extent_bytes;
}

bool Track::data_overlap(const Sector& sector) const
{
    if (!sector.offset)
        return false;

    return data_extent_bytes(sector) < sector.size();
}

bool Track::is_mixed_encoding() const
{
    if (empty())
        return false;

    auto first_encoding = m_sectors[0].encoding;

    auto it = std::find_if(begin() + 1, end(), [&](const Sector& s) {
        return s.encoding != first_encoding;
        });

    return it != end();
}

bool Track::is_8k_sector() const
{
    return size() == 1 && m_sectors[0].is_8k_sector();
}

bool Track::is_repeated(const Sector& sector) const
{
    auto count = 0;

    for (const auto& s : m_sectors)
    {
        // Check for data rate, encoding, and CHRN match
        if (s.datarate == sector.datarate &&
            s.encoding == sector.encoding &&
            s.header == sector.header)
        {
            // Stop if we see more than one match.
            if (++count > 1)
                return true;
        }
    }

    return false;
}

bool Track::has_all_good_data() const
{
    if (empty())
        return true;

    auto it = std::find_if(begin(), end(), [&](const Sector& sector) {
        return !sector.has_good_data(true);
        });

    return it == end();
}

bool Track::has_any_good_data() const
{
    if (empty())
        return false;

    auto it = std::find_if(begin(), end(), [](const Sector& sector) {
        return sector.has_good_data();
        });

    return it != end();
}

const Sectors Track::good_idcrc_sectors() const
{
    Sectors good_idcrc_sectors;
    std::copy_if(begin(), end(), std::back_inserter(good_idcrc_sectors), [&](const Sector& sector) {
        return !sector.has_badidcrc();
    });

    return good_idcrc_sectors;
}

const Sectors Track::good_sectors() const
{
    Sectors good_sectors;
    std::copy_if(begin(), end(), std::back_inserter(good_sectors), [&](const Sector& sector) {
        if (sector.has_badidcrc())
            return false;
        // Checksummable 8k sector is considered in has_good_data method.
        return sector.has_good_data(!opt_normal_disk, opt_normal_disk);
    });

    return good_sectors;
}

const Sectors Track::stable_sectors() const
{
    Sectors stable_sectors;
    std::copy_if(begin(), end(), std::back_inserter(stable_sectors), [&](const Sector& sector) {
        if (sector.has_badidcrc())
            return false;
        // Checksummable 8k sector is considered in has_stable_data method.
        return sector.has_stable_data();
    });

    return stable_sectors;
}

bool Track::has_all_stable_data(const Sectors& stable_sectors) const
{
    if (empty())
        return true;

    auto it = std::find_if(begin(), end(), [&](const Sector& sector) {
        if (!sector.has_badidcrc() && stable_sectors.Contains(sector, tracklen))
            return false;
        // Checksummable 8k sector is considered in has_stable_data method.
        return !sector.has_stable_data();
    });

    return it == end();
}

int Track::normal_probable_size() const
{
    int amount_of_sector_id = 0;
    const auto sector_id_summer = [&](auto a, auto b) {
        if (b.has_badidcrc())
            return a;
        amount_of_sector_id++;
        return a + b.header.sector - 1; // Using sector indexing 0-based thus the -1.
    };
    const auto sum_of_sector_id = std::accumulate(begin(), end(), 0, sector_id_summer);
    if (amount_of_sector_id == 0)
        return 0;
    const auto average_sector_id = static_cast<double>(sum_of_sector_id) / amount_of_sector_id;
    const auto max_sector_id = round_AS<int>(average_sector_id * 2 + 1); // Back to sector indexing 1-based thus the +1.
    const auto sector_id_counter = [&](auto a, auto b) {
        if (b.has_badidcrc())
            return a;
        return a + ((b.header.sector >= 1 && b.header.sector <= max_sector_id) ? 1 : 0);
    };
    return std::accumulate(begin(), end(), 0, sector_id_counter);
}

/* Implementation changed. Now it removes only the sectors and keeps other properties (e.g. tracklen, tracktime).
 * This way the behavour is more reasonable.
 */
void Track::clear()
{
    m_sectors.clear();
    m_sectors_view_ordered_by_id.clear();
}

void Track::add(Track&& track)
{
    // Ignore if no sectors to add
    if (!track.sectors().size())
        return;

    // Use longest track length and time
    tracklen = std::max(tracklen, track.tracklen);
    tracktime = std::max(tracktime, track.tracktime);

    add(std::move(track.sectors()));
}

void Track::add(Sectors&& sectors)
{
    // Ignore if no sectors to add.
    if (sectors.empty())
        return;

    // Merge supplied sectors into existing track.
    for (auto& sector : sectors)
    {
        assert(sector.offset != 0);
        add(std::move(sector));
    }
}

Track::AddResult Track::add(Sector&& sector)
{
    // Check the new datarate against any existing sector.
    if (!m_sectors.empty() && m_sectors[0].datarate != sector.datarate)
        throw util::exception("can't mix datarates on a track");

    // If there's no positional information, simply append.
    if (sector.offset == 0 || tracklen == 0) // The offset is 0 exactly when tracklen is 0 but safer to check both.
    {
        m_sectors.emplace_back(std::move(sector));
        return AddResult::Append;
    }
    else
    {
        // Find a sector close enough to the new offset to be the same one
        auto it = std::find_if(begin(), end(), [&](const Sector& s) {
            return sector.is_sector_tolerated_same(s, opt_byte_tolerance_of_time, tracklen);
            });

        // If that failed, we have a new sector with an offset
        if (it == end())
        {
            // Find the insertion point to keep the sectors in order
            it = std::find_if(begin(), end(), [&](const Sector& s) {
                return sector.offset < s.offset;
                });
            m_sectors.emplace(it, std::move(sector));
            return AddResult::Insert;
        }
        else
        {
            // Merge details with the existing sector
            auto ret = it->merge(std::move(sector));
            if (ret == Sector::Merge::Unchanged || ret == Sector::Merge::Matched) // Matched for backward compatibility.
                return AddResult::Unchanged;

            // Limit the number of data copies kept for each sector.
            if (data_overlap(*it) && !is_8k_sector())
                it->limit_copies(1);

            return AddResult::Merge;
        }
    }
}

void Track::insert(int index, Sector&& sector)
{
    assert(index <= static_cast<int>(m_sectors.size()));

    if (!m_sectors.empty() && m_sectors[0].datarate != sector.datarate)
        throw util::exception("can't mix datarates on a track");

    auto it = m_sectors.begin() + index;
    m_sectors.insert(it, std::move(sector));
}

Sector Track::remove(int index)
{
    assert(index < static_cast<int>(m_sectors.size()));

    auto it = m_sectors.begin() + index;
    auto sector = std::move(*it);
    m_sectors.erase(it);
    return sector;
}


const Sector& Track::get_sector(const Header& header) const
{
    auto it = find(header);
    if (it == end() || it->data_size() < header.sector_size())
        throw util::exception(CylHead(header.cyl, header.head), " sector ", header.sector, " not found");

    return *it;
}

DataRate Track::getDataRate() const
{
    assert(!empty());

    return (*this)[0].datarate;
}

Encoding Track::getEncoding() const
{
    assert(!empty());

    return (*this)[0].encoding;
}

int Track::getTimeOfOffset(const int offset) const
{
    assert(!empty());

    return GetFmOrMfmDataBitsTimeAsRounded(getDataRate(), getEncoding(), offset); // microsec
}

int Track::getOffsetOfTime(const int time) const
{
    assert(!empty());

    return GetFmOrMfmTimeDataBitsAsRounded(getDataRate(), getEncoding(), time); // mfmbits
}

/*static*/ int Track::findMostPopularToleratedDiff(std::vector<int>& diffs)
{
    assert(diffs.size() > 0);

    std::sort(diffs.begin(), diffs.end());
    typedef std::pair<int, int> ParticipantsAndAverage;
    std::vector<ParticipantsAndAverage> participantsAndAveragedOffsetDiffs;
    const auto diffsEnd = diffs.end();
    for (auto it = diffs.begin(); it != diffsEnd; it++)
    {
        const auto it0 = it;
        auto s = *(it++);
        while (it != diffsEnd && *it < *it0 + Track::COMPARE_TOLERANCE_BITS)
            s += *(it++);
        participantsAndAveragedOffsetDiffs.push_back(std::make_pair(it - it0, s / (it - it0)));
        it--;
    }
    const auto it = std::max_element(participantsAndAveragedOffsetDiffs.begin(), participantsAndAveragedOffsetDiffs.end(),
                                     [] (const ParticipantsAndAverage &a, const ParticipantsAndAverage &b) {
        return a.first < b.first || (a.first == b.first && a.second > b.second); // Go for more participants and less averaged offset diff.
    });
    return it->second;
}

bool Track::findSyncOffsetComparedTo(const Track& referenceTrack, int& syncOffset) const
{
    if (referenceTrack.empty() || empty())
        return false;
    // Find the best sync (offset diff).
    std::vector<int> offsetDiffs;
    const auto trackEnd = end();
    for (auto& s : referenceTrack.sectors())
    {
        auto it = find(s.header);
        while (it != trackEnd)
        {
            offsetDiffs.push_back(it->offset - s.offset);
            it = findNext(s.header, it);
        }
    }
    if (offsetDiffs.empty())
        return false;
    syncOffset = findMostPopularToleratedDiff(offsetDiffs);
    return true;
}

void Track::syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle)
{
    assert(trackLenSingle > 0);
    assert(tracklen > 0);
    assert(syncOffset < tracklen);

    Track trackTempSingle;
    trackTempSingle.tracklen = trackLenSingle;
    trackTempSingle.tracktime = round_AS<int>(static_cast<double>(tracktime) * trackLenSingle / tracklen);
    if (!empty())
    {
        const auto tracklenCeilToMultiSingle = trackLenSingle * ceil_AS<int>(static_cast<double>(tracklen) / trackLenSingle);
        auto it = begin();
        while (it < end())
        {
            Sector sector(*it);
            sector.offset = (sector.offset - syncOffset + tracklenCeilToMultiSingle) % trackLenSingle; // Synced and demultid offset.
            const auto addResult = trackTempSingle.add(std::move(sector));
            if (addResult == Track::AddResult::Append || addResult == Track::AddResult::Insert)
            {
                it->revolution = it->offset / trackTempSingle.tracklen;
                it->offset = sector.offset;
                it++;
            }
            else
                it = sectors().erase(it);
        }
        std::sort(begin(), end(),
                  [](const Sector& s1, const Sector& s2) { return s1.offset < s2.offset; });
    }
    tracklen = trackTempSingle.tracklen;
    tracktime = trackTempSingle.tracktime;
}

int Track::determineBestTrackLen(const int timedTrackLen) const
{
    assert(timedTrackLen > 0);

    if (empty())
        return 0;
    std::vector<int> offsetDiffs;
    auto sectorsOrderedByHeader = sectors();
    std::sort(sectorsOrderedByHeader.begin(), sectorsOrderedByHeader.end(),
              [](const Sector& s1, const Sector& s2) { return s1.header < s2.header || (s1.header == s2.header && s1.offset < s2.offset); });
    const auto sectorsOrderedByHeaderEnd = sectorsOrderedByHeader.end();
    for (auto it = sectorsOrderedByHeader.begin(); it != sectorsOrderedByHeaderEnd; it++)
    {
        const auto it0 = it;
        while (++it != sectorsOrderedByHeaderEnd && it->header == it0->header)
        {
            offsetDiffs.push_back(it->offset - it0->offset);
        }
        it = it0;
    }
    if (offsetDiffs.empty())
        return 0;
    const auto offsetDiffBest = Track::findMostPopularToleratedDiff(offsetDiffs); // This can be multiple tracklen. It must be reduced.
    const auto multi = round_AS<int>(static_cast<double>(offsetDiffBest) / timedTrackLen);
    if (multi == 0)
    {
        if (opt_debug)
            util::cout << "determineBestTrackLen found offsetDiffBest " << offsetDiffBest << " to be too low compared to timedTrackLen " << timedTrackLen << "\n";
        return 0;
    }
    const auto trackLenBest = round_AS<int>(static_cast<double>(offsetDiffBest) / multi);
    if (opt_debug)
    {
        if (std::abs(trackLenBest - timedTrackLen) > Track::COMPARE_TOLERANCE_BITS)
            util::cout << "determineBestTrackLen found trackLenBest " << trackLenBest << " to be outside of tolerated timedTrackLen " << timedTrackLen << "\n";
    }
    if (opt_debug)
        util::cout << "determineBestTrackTime found trackLenBest " << trackLenBest << "\n";
    return trackLenBest;
}

void Track::setTrackLenAndNormaliseTrackTimeAndSectorOffsets(const int trackLen)
{
    assert(tracklen > 0);
    if (tracklen == trackLen)
        return;
    const double rate = static_cast<double>(trackLen) / tracklen;
    for (auto& sector : m_sectors)
        sector.offset = round_AS<int>(sector.offset * rate);
    tracklen = trackLen;
    if (!empty())
        tracktime = getTimeOfOffset(tracklen);
}

int Track::findReasonableIdOffsetForDataFmOrMfm(const int dataOffset) const
{
    const auto offsetDiff = GetFmOrMfmIdamAndAmDistance(getDataRate(), getEncoding()) * 8 * 2;
    // We could check if the sector overlaps something existing but unimportant now.
    return (dataOffset - offsetDiff + tracklen) % tracklen;
}

// Guess sector ids based on discovered gap3s and format scheme recognition.
IdAndOffsetVector Track::DiscoverTrackSectorScheme() const
{
    IdAndOffsetVector sectorIdsAndOffsets;
    if (empty())
        return sectorIdsAndOffsets;
    if (!opt_normal_disk) // There can be different sector sizes and can not tell if one big or more small sectors fill in a hole.
        util::cout << "DiscoverTrackSectorScheme is called but normal disk option is not set. This method supports normal disk so setting that option is recommended.\n";
    typedef std::vector<int>::size_type SectorIdsST;
    sectorIdsAndOffsets.reserve(static_cast<SectorIdsST>(size())); // Size will be more if holes are found at the track end.

    const auto optByteToleranceBits = opt_byte_tolerance_of_time * 8 * 2;
    const auto sectorSize = m_sectors[0].size();
    const auto encoding = getEncoding();
    const auto predictedOverheadedSectorWithoutSyncAndDataBits = GetFmOrMfmSectorOverheadWithoutSync(getDataRate(), encoding) * 8 * 2;
    const auto predictedOverheadedSectorWithGap3AndDataBits = (GetFmOrMfmSectorOverheadWithGap3(getDataRate(), encoding) + sectorSize) * 8 * 2;
    auto overheadedSectorWithGap3AndDataBits = 0;
    auto overheadedSectorWithGap3AndDataBitsParticipants = 0;
    auto overheadedSectorWithGap3AndDataBitsAbout = predictedOverheadedSectorWithGap3AndDataBits;
    const auto iSup = size();
    for (int i = 0; i < iSup; i++)
    {
        const auto& sector = m_sectors[static_cast<Sectors::size_type>(i)];
        if (sector.size() != sectorSize) // Only same sized sectors are supported.
            return sectorIdsAndOffsets;
        if (opt_debug)
            util::cout << "DiscoverTrackSectorScheme: processing sector, offset=" << sector.offset << ", ID=" << sector.header.sector << "\n";
        if (i < iSup - 1)
        {
            const auto offsetDiff = m_sectors[static_cast<Sectors::size_type>(i + 1)].offset - sector.offset;
            if (offsetDiff <= overheadedSectorWithGap3AndDataBitsAbout + optByteToleranceBits) // Next sector is close enough so there is no hole.
            {
                overheadedSectorWithGap3AndDataBits += offsetDiff;
                overheadedSectorWithGap3AndDataBitsParticipants++;
                overheadedSectorWithGap3AndDataBitsAbout = overheadedSectorWithGap3AndDataBits / overheadedSectorWithGap3AndDataBitsParticipants;
            }
        }
    }
    overheadedSectorWithGap3AndDataBits = overheadedSectorWithGap3AndDataBitsAbout;
    const auto gap3PlusSyncBits = overheadedSectorWithGap3AndDataBits - predictedOverheadedSectorWithoutSyncAndDataBits - sectorSize * 8 * 2;
    const auto sectorOverheadTolerance = 1 * 8 * 2;

    // Determine and add holes between track start and first sector.
    const auto overheadedSectorPlusGap3PlusSyncPlusIdamSyncOverheadBits =
            overheadedSectorWithGap3AndDataBitsAbout + gap3PlusSyncBits + GetIdamOverheadSyncOverhead(encoding) * 8 * 2; // gap3 should be gap4a but good enough now.
    auto remainingStartOffset = m_sectors[0].offset;
    while (remainingStartOffset - 0 >= overheadedSectorPlusGap3PlusSyncPlusIdamSyncOverheadBits) // Could subtract a minimal gap4a instead of 0.
    {
        remainingStartOffset -= overheadedSectorWithGap3AndDataBitsAbout;
        sectorIdsAndOffsets.push_back(IdAndOffset(-1, remainingStartOffset));
        if (opt_debug)
            util::cout << "DiscoverTrackSectorScheme: pushed hole sector, offset=" << remainingStartOffset << "\n";
    }

    // Determine and add holes between sectors.
    for (int i = 0; i < iSup; i++)
    {
        const auto& sector = m_sectors[static_cast<Sectors::size_type>(i)];
        sectorIdsAndOffsets.push_back(IdAndOffset(sector.header.sector, sector.offset));
        if (opt_debug)
            util::cout << "DiscoverTrackSectorScheme: pushed sector, offset=" << sector.offset << ", ID=" << sector.header.sector << "\n";
        if (i < iSup - 1)
        {
            const auto offsetDiff = m_sectors[static_cast<Sectors::size_type>(i + 1)].offset - sector.offset;
            if (offsetDiff > overheadedSectorWithGap3AndDataBitsAbout + optByteToleranceBits) // Next sector is not close enough so there is hole.
            {
                const auto remainingoffsetDiff = offsetDiff - overheadedSectorWithGap3AndDataBits; // Subtracting this sector from hole.
                const auto sectorsFittingHoleAbout = round_AS<int>(static_cast<double>(remainingoffsetDiff) / overheadedSectorWithGap3AndDataBits);
                const auto sectorsFittingHole = floor_AS<int>(static_cast<double>(remainingoffsetDiff + sectorOverheadTolerance * sectorsFittingHoleAbout) / overheadedSectorWithGap3AndDataBits);
                auto holeOffset = sector.offset + overheadedSectorWithGap3AndDataBits;
                for (int j = 0; j < sectorsFittingHole; j++)
                {
                    sectorIdsAndOffsets.push_back(IdAndOffset(-1, holeOffset));
                    if (opt_debug)
                        util::cout << "DiscoverTrackSectorScheme: pushed hole sector, offset=" << holeOffset << "\n";
                    holeOffset += overheadedSectorWithGap3AndDataBits;
                }
            }
        }
    }

    // Determine and add holes between last sector and track end.
    const auto overheadedSectorFromOffsetToDataCrcEndBits = overheadedSectorWithGap3AndDataBitsAbout - gap3PlusSyncBits - GetIdamOverheadSyncOverhead(encoding) * 8 * 2;
    auto remainingEndOffset = m_sectors[static_cast<Sectors::size_type>(iSup - 1)].offset + overheadedSectorWithGap3AndDataBits;
    while (tracklen - remainingEndOffset >= overheadedSectorFromOffsetToDataCrcEndBits)
    {
        sectorIdsAndOffsets.push_back(IdAndOffset(-1, remainingEndOffset));
        if (opt_debug)
            util::cout << "DiscoverTrackSectorScheme: pushed hole sector, offset=" << remainingEndOffset << "\n";
        remainingEndOffset += overheadedSectorWithGap3AndDataBitsAbout;
    }
    if (sectorIdsAndOffsets.ReplaceMissingIdsByFindingTrackSectorIds())
    {
        if (opt_debug)
        {
            const auto iSup = static_cast<int>(sectorIdsAndOffsets.size());
            for (int i = 0; i < iSup; i++)
            {
                    util::cout << "DiscoverTrackSectorScheme: sectorIdsAndOffsets[" << i << "] has id=" <<
                                  sectorIdsAndOffsets[static_cast<IdAndOffsetVectorST>(i)].id << ", offset=" <<
                                  sectorIdsAndOffsets[static_cast<IdAndOffsetVectorST>(i)].offset << "\n";
            }
        }
        return sectorIdsAndOffsets;
    }
    return IdAndOffsetVector();
}

Track& Track::format(const CylHead& cylhead, const Format& fmt)
{
    assert(fmt.sectors != 0);

    m_sectors.clear();
    m_sectors.reserve(lossless_static_cast<Sectors::size_type>(fmt.sectors));

    for (auto id : fmt.get_ids(cylhead))
    {
        Header header(cylhead.cyl, cylhead.head ? fmt.head1 : fmt.head0, id, fmt.size);
        Sector sector(fmt.datarate, fmt.encoding, header, fmt.gap3);
        Data data(lossless_static_cast<DataST>(fmt.sector_size()), fmt.fill);

        sector.add(std::move(data));
        add(std::move(sector));
    }

    return *this;
}

Data::const_iterator Track::populate(Data::const_iterator it, Data::const_iterator itEnd)
{
    assert(std::distance(it, itEnd) >= 0);

    // Populate in sector number order, which requires sorting the track
    std::vector<Sector*> ptrs(m_sectors.size());
    std::transform(m_sectors.begin(), m_sectors.end(), ptrs.begin(), [](Sector& s) { return &s; });
    std::sort(ptrs.begin(), ptrs.end(), [](Sector* a, Sector* b) { return a->header.sector < b->header.sector; });

    for (auto sector : ptrs)
    {
        assert(sector->copies() == 1);
        auto bytes = std::min(sector->size(), static_cast<int>(std::distance(it, itEnd)));
        std::copy_n(it, bytes, sector->data_copy(0).begin());
        it += bytes;
    }

    return it;
}

Sectors::const_iterator Track::find(const Sector& sector) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return &s == &sector;
        });
}

Sectors::const_iterator Track::find(const Header& header) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return header == s.header;
        });
}

Sectors::const_iterator Track::findNext(const Header& header, const Sectors::const_iterator& itPrev) const
{
    if (itPrev == end())
        return end();
    return std::find_if(std::next(itPrev), end(), [&](const Sector& s) {
        return header == s.header;
        });
}

Sectors::const_iterator Track::findFirstFromOffset(const int offset) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return offset <= s.offset;
        });
}

Sectors::const_iterator Track::findIgnoringSize(const Header& header) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return header.compare_chr(s.header);
    });
}

Sectors::const_iterator Track::find(const Header& header, const DataRate datarate, const Encoding encoding) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return header == s.header && datarate == s.datarate && encoding == s.encoding;
        });
}
