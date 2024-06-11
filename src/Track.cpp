#include "PlatformConfig.h"
#include "Track.h"
#include "Options.h"
//#include "DiskUtil.h"
#include "IBMPCBase.h"
#include "Util.h"
#include "RingedInt.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>

static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");



std::shared_ptr<const VectorX<int>> RepeatedSectors::FindOffsetsById(const int sectorId) const
{
    const auto it = find(sectorId);
    if (it == end())
        return nullptr;
    return std::make_shared<const VectorX<int>>(it->second);
}

std::shared_ptr<int> RepeatedSectors::FindToleratedOffsetsById(const int sectorId, const int offset,
    const Encoding& encoding, const int byte_tolerance_of_time, const int trackLen) const
{
    const auto it = FindOffsetsById(sectorId);
    if (it == nullptr)
        return nullptr;
    const auto& offsets = *it;
    const auto iSup = offsets.size();
    for (auto i = 0; i < iSup; i++)
    {
        if (are_offsets_tolerated_same(offset, offsets[i], encoding, byte_tolerance_of_time, trackLen))
            return std::make_shared<int>(offsets[i]);
    }
    return nullptr;
}



Track::Track(int num_sectors/*=0*/)
{
    m_sectors.reserve(num_sectors);
}

Track Track::CopyWithoutSectorData() const
{
    auto& thisWritable = *const_cast<Track*>(this);
    auto sectorsTmp = std::move(thisWritable.m_sectors);
    auto sectorsViewOrdereByIdTmp = std::move(thisWritable.m_sectors_view_ordered_by_id);
    thisWritable.m_sectors.clear();
    thisWritable.m_sectors_view_ordered_by_id.clear();
    Track track(*this);
    thisWritable.m_sectors = std::move(sectorsTmp);
    thisWritable.m_sectors_view_ordered_by_id = std::move(sectorsViewOrdereByIdTmp);

    for (auto& sector : m_sectors)
        track.m_sectors.push_back(sector.CopyWithoutData(false)); // Resets read_attempts.
    for (auto& sectorView : m_sectors_view_ordered_by_id)
        track.m_sectors_view_ordered_by_id.push_back(sectorView.CopyWithoutData(false)); // Resets read_attempts.

    return track;
}

bool Track::empty() const
{
    return m_sectors.empty();
}

int Track::size() const
{
    return m_sectors.size();
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

const Sectors& Track::sectors() const
{
    return m_sectors;
}

Sectors& Track::sectors()
{
    return m_sectors;
}

const Sectors& Track::sectors_view_ordered_by_id() const
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
    assert(index < m_sectors.size());
    return m_sectors[index];
}

Sector& Track::operator [] (int index)
{
    assert(index < m_sectors.size());
    return m_sectors[index];
}

int Track::index_of(const Sector& sector) const
{
    auto it = std::find_if(begin(), end(), [&](const Sector& s) {
        return &s == &sector;
        });

    return (it == end()) ? -1 : static_cast<int>(std::distance(begin(), it));
}

/* The data extent bits of a sector are the bits between offset of next sector
 * (if exists otherwise tracklen + offset of first sector) and offset of this
 * sector.
 */
int Track::data_extent_bits(const Sector& sector) const
{
    auto it = find(sector);
    assert(it != end());

    auto revolution_time_ms = (sector.datarate == DataRate::_300K) ? RPM_TIME_360 : RPM_TIME_300; // time / rotation.
    auto track_len = tracklen ? tracklen : GetTrackCapacity(revolution_time_ms, sector.datarate, sector.encoding);

    // Approximate bit distance to next ID header.
    auto gap_bits = ((std::next(it) != end()) ? std::next(it)->offset : (track_len + begin()->offset)) - sector.offset;
    return gap_bits;
}

/* The data extent bytes of a sector are the bytes between offset of next sector
 * (if exists otherwise tracklen + offset of first sector) and offset of this
 * sector without the parts (id overhead without idam overhead, gap2, sync,
 * d overhead) before data bytes and without idam overhead of next
 * sector, i.e. they are the bytes from the first data byte until idam overhead
 * of next sector, i.e. data bytes + data crc + gap3 + sync.
 */
int Track::data_extent_bytes(const Sector& sector) const
{
    // We only support real data extent for MFM and FM sectors.
    if (sector.encoding != Encoding::MFM && sector.encoding != Encoding::FM)
    {
        assert(!sector.HasUnknownSize());
        return sector.size();
    }

    const auto encoding_shift = (sector.encoding == Encoding::FM) ? 5 : 4;
    const auto gap_bytes = data_extent_bits(sector) >> encoding_shift;
    // The overhead_bytes are the sum of bytes of id overhead, gap2, sync, d overhead without data crc.
    const auto overhead_bytes = GetFmOrMfmSectorOverheadWithoutSyncAndDataCrc(sector.datarate, sector.encoding);
    const auto extent_bytes = (gap_bytes > overhead_bytes) ? gap_bytes - overhead_bytes : 0;
    return extent_bytes;
}

bool Track::data_overlap(const Sector& sector) const
{
    if (!sector.offset)
        return false;

    if (sector.HasUnknownSize())
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

const UniqueSectors Track::good_idcrc_sectors() const
{
    UniqueSectors good_idcrc_sectors;
    std::copy_if(begin(), end(), std::inserter(good_idcrc_sectors, good_idcrc_sectors.end()), [&](const Sector& sector) {
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

const UniqueSectors Track::stable_sectors() const
{
    UniqueSectors stable_sectors;
    std::copy_if(begin(), end(), std::inserter(stable_sectors, stable_sectors.end()), [&](const Sector& sector) {
        if (sector.has_badidcrc())
            return false;
        // Checksummable 8k sector is considered in has_stable_data method.
        return sector.has_stable_data(true);
    });

    return stable_sectors;
}

bool Track::has_all_stable_data(const UniqueSectors& stable_sectors) const
{
    if (empty())
        return true;

    auto it = std::find_if(begin(), end(), [&](const Sector& sector)
    { // Find not stable.
        if (sector.has_badidcrc())
            return true;
        if (stable_sectors.Contains(sector, tracklen))
            return false;
        // Checksummable 8k sector is considered in has_stable_data method.
        return !sector.has_stable_data(true);
    });

    return it == end(); // Not found not stable thus all sectors are stable.
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

void Track::add(Sectors&& sectors, const std::function<bool (const Sector &)>& sectorFilterPredicate/* = nullptr*/)
{
    // Ignore if no sectors to add.
    if (sectors.empty())
        return;

    // Merge supplied sectors into existing track.
    for (auto& sector : sectors)
    {
        if (!sectorFilterPredicate || sectorFilterPredicate(sector))
        {
            assert(sector.offset != 0);
            add(std::move(sector));
        }
    }
}

Track::AddResult Track::merge(Sector&& sector, const Sectors::iterator& it)
{
    if (getDataRate() != sector.datarate)
        throw util::exception("can't mix datarates on a track");
    auto result = AddResult::Merge;
    // Merge details with the existing sector
    const auto ret = it->merge(std::move(sector));
    if (ret == Sector::Merge::Unchanged || ret == Sector::Merge::Matched // Matched for backward compatibility.
        || ret == Sector::Merge::NewDataOverLimit)
        result = AddResult::Unchanged;
    else
    {
        // Limit the number of data copies kept for each sector.
        if (data_overlap(*it) && !is_8k_sector())
            it->limit_copies(1);
    }
    return result;
}

/* If dryrun is true then one of {Append, Insert, Merge} is returned and sector
 * is unchanged.
 * If dryrun is false then one of {Append, Insert, Merge, Unchanged} is
 * returned where the Unchanged value is a subcase of Merge value when the data
 * was ignored (unchanged) or matched. In case of returned Unchanged value the
 * sector is unchanged.
 * The returned affectedSectorIndex is the index of added sector (even if
 * dryrun is true).
 */
Track::AddResult Track::add(Sector&& sector, int* affectedSectorIndex/* = nullptr*/, bool dryrun/* = false*/)
{
    // Check the new datarate against any existing sector.
    if (!m_sectors.empty() && getDataRate() != sector.datarate)
        throw util::exception("can't mix datarates on a track");

    // If there's no positional information, simply append.
    if (sector.offset == 0)
    {
        if (affectedSectorIndex != nullptr)
            *affectedSectorIndex = m_sectors.size();
        if (!dryrun)
            m_sectors.emplace_back(std::move(sector));
        return AddResult::Append;
    }

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
        if (affectedSectorIndex != nullptr)
            *affectedSectorIndex = static_cast<int>(it - begin());
        if (!dryrun)
            m_sectors.emplace(it, std::move(sector));
        return AddResult::Insert;
    }

    auto result = AddResult::Merge;
    if (affectedSectorIndex != nullptr)
        *affectedSectorIndex = static_cast<int>(it - begin());
    if (!dryrun)
    {
        // Merge details with the existing sector
        auto ret = it->merge(std::move(sector));
        if (ret == Sector::Merge::Unchanged || ret == Sector::Merge::Matched) // Matched for backward compatibility.
            result = AddResult::Unchanged;
        else
        {
            // Limit the number of data copies kept for each sector.
            if (data_overlap(*it) && !is_8k_sector())
                it->limit_copies(1);
        }
    }
    return result;
}

void Track::insert(int index, Sector&& sector)
{
    assert(index <= m_sectors.size());

    if (!m_sectors.empty() && m_sectors[0].datarate != sector.datarate)
        throw util::exception("can't mix datarates on a track");

    auto it = m_sectors.begin() + index;
    m_sectors.insert(it, std::move(sector));
}

Sector Track::remove(int index)
{
    assert(index < m_sectors.size());

    auto it = m_sectors.begin() + index;
    auto sector = std::move(*it);
    m_sectors.erase(it);
    return sector;
}


// The track must not be orphan data track.
const Sector& Track::get_sector(const Header& header) const
{
    auto it = find(header);
    assert(it == end() || !it->HasUnknownSize());
    if (it == end() || it->data_size() < it->size())
        throw util::exception(CylHead(header.cyl, header.head), " sector ", header.sector, " not found");

    return *it;
}

DataRate Track::getDataRate() const
{
    assert(!empty());

    return operator[](0).datarate;
}

Encoding Track::getEncoding() const
{
    assert(!empty());

    return operator[](0).encoding;
}

int Track::getTimeOfOffset(const int offset) const
{
    assert(!empty());

    return GetFmOrMfmBitsTimeAsRounded(getDataRate(), offset); // microsec
}

int Track::getOffsetOfTime(const int time) const
{
    assert(!empty());

    return GetFmOrMfmTimeBitsAsRounded(getDataRate(), time); // mfmbits (rawbits)
}

void Track::setTrackLen(const int trackLen)
{
    tracklen = trackLen;
    if (!empty())
        tracktime = getTimeOfOffset(trackLen);
}

void Track::setTrackTime(const int trackTime)
{
    tracktime = trackTime;
    if (!empty())
        tracklen = getOffsetOfTime(trackTime);
}

/*static*/ int Track::findMostPopularToleratedDiff(VectorX<int> &diffs, const Encoding& encoding)
{
    assert(diffs.size() > 0);

    std::sort(diffs.begin(), diffs.end());
    typedef std::pair<int, int> ParticipantsAndAverage;
    VectorX<ParticipantsAndAverage> participantsAndAveragedOffsetDiffs;
    const auto diffsEnd = diffs.end();
    for (auto it = diffs.begin(); it != diffsEnd; )
    {
        const auto it0 = it;
        auto diffSum = *(it++);
        while (it != diffsEnd && *it < *it0 + DataBytePositionAsBitOffset(Track::COMPARE_TOLERANCE_BYTES, encoding))
            diffSum += *(it++);
        participantsAndAveragedOffsetDiffs.push_back(std::make_pair(it - it0, diffSum / (it - it0)));
    }
    const auto it = std::max_element(participantsAndAveragedOffsetDiffs.begin(), participantsAndAveragedOffsetDiffs.end(),
                                     [] (const ParticipantsAndAverage &a, const ParticipantsAndAverage &b) {
        return a.first < b.first || (a.first == b.first && a.second > b.second); // Go for more participants and less averaged offset diff.
    });
    return it->second;
}

// This track and otherTrack can be single or multi and must not be orphan data, they can have orphans.
std::map<int, int> Track::FindMatchingSectors(const Track& otherTrack, const RepeatedSectors& repeatedSectorIds) const
{
    std::map<int, int> result{};
    if (otherTrack.empty() || empty())
        return result;

    // Find the first pair of matching sectors.
    const auto trackEnd = otherTrack.end();
    const auto repeatedSectorIdsEnd = repeatedSectorIds.end();
    const auto iSup = size();
    for (auto i = 0; i < iSup; i++)
    {
        const auto& sector = operator[](i);
        if (sector.IsOrphan())
            continue;
        if (repeatedSectorIds.find(sector.header.sector) != repeatedSectorIdsEnd)
            continue;
        const auto it = otherTrack.find(sector.header);
        if (it != trackEnd)
            result.emplace(i, it - otherTrack.begin());
    }
    return result;
}

/* Guess track sector ids based on discovered gap3s and track sector scheme recognition.
* The track must not be orphan data track.
*/
bool Track::DiscoverTrackSectorScheme(const RepeatedSectors& repeatedSectorIds)
{
    idAndOffsetPairs.clear();
    if (empty())
        return false;
    assert(tracklen > 0);
    // Normal disk is required because otherwise we can not tell what sized
    // sectors fill in a hole.
    if (!opt_normal_disk)
    {
        MessageCPP(msgWarning, "DiscoverTrackSectorScheme is called but it does nothing in not normal disk mode (option is not set). It works only in normal disk mode with same sized sectors");
        return false;
    }
    IdAndOffsetPairs idAndOffsetPairsLocal;
    idAndOffsetPairsLocal.reserve(size()); // Size will be more if holes are found.

    const auto& encoding = getEncoding();
    const auto toleratedOffsetDistance = tolerated_offset_distance(encoding, opt_byte_tolerance_of_time);
    if (!DetermineOffsetDistanceMinMaxAverage(repeatedSectorIds))
        return false;
    const auto offsetDistanceAverage = round_AS<int>(idOffsetDistanceInfo.offsetDistanceAverage);
    const auto offsetDistanceMin = round_AS<int>(idOffsetDistanceInfo.offsetDistanceMin);
    const auto ignoredIdsEnd = idOffsetDistanceInfo.ignoredIds.end();
    if (opt_debug >= 2)
        util::cout << "DiscoverTrackSectorScheme: offsetDistanceAverage=" << offsetDistanceAverage << ", tracklen=" << tracklen << "\n";

    // Determine and add holes between sectors.
    auto firstNotOrphanSectorIndex = -1;
    auto firstNotOrphanSectorSize = 0;
    const auto iSup = size();
    for (auto i = 0; i < iSup; i++)
    {
        const auto& sector = operator[](i);

        if (sector.IsOrphan() || idOffsetDistanceInfo.ignoredIds.find(i) != ignoredIdsEnd)
            continue;
        assert(!sector.HasUnknownSize());
        if (firstNotOrphanSectorIndex < 0)
        {
            firstNotOrphanSectorIndex = i;
            firstNotOrphanSectorSize = sector.size();
        }
        else if (sector.size() != firstNotOrphanSectorSize) // Only same sized sectors are supported.
        {
            MessageCPP(msgWarningAlways, "Different sized sectors (", operator[](firstNotOrphanSectorIndex), ") and (",
                sector, ") are invalid in normal disk mode, sizes (",
                firstNotOrphanSectorSize, ", ", sector.size(), "), skipping discovering");
            return false;
        }
        idAndOffsetPairsLocal.push_back(IdAndOffset(sector.header.sector, sector.offset));
        if (opt_debug)
            util::cout << "DiscoverTrackSectorScheme: pushed sector (offset=" << sector.offset << ", id.sector=" << sector.header.sector << ")\n";
        const auto sectorNextPredictedOffset = sector.offset + offsetDistanceAverage;
        const auto sectorNextWrapped = i >= iSup - 1;
        const auto& sectorNext = sectorNextWrapped ? operator[](0) : operator[](i + 1);
        const auto holeSize = (sectorNextWrapped ? tracklen : 0) + sectorNext.offset - sectorNextPredictedOffset;
        if (holeSize > toleratedOffsetDistance) // Next sector is not close enough so there is hole.
        {
            const auto sectorsFittingHole = floor_AS<int>(static_cast<double>(
                holeSize + toleratedOffsetDistance) / offsetDistanceAverage);
            auto holeOffsetWrapped = sectorNextPredictedOffset;
            const auto remainingHoleOffsetDiff = std::max(0, holeSize - sectorsFittingHole * offsetDistanceMin); // The remaining size less than the average offset distance.
            for (auto j = 0; j < sectorsFittingHole; j++)
            {
                if (holeOffsetWrapped >= tracklen)
                    holeOffsetWrapped -= tracklen;
                const auto holeOffsetMaxUnwrapped = holeOffsetWrapped + remainingHoleOffsetDiff;
                const auto holeOffsetMaxWrapped = holeOffsetMaxUnwrapped >= tracklen
                    ? holeOffsetMaxUnwrapped - tracklen : holeOffsetMaxUnwrapped;
                idAndOffsetPairsLocal.push_back(IdAndOffset(-1, holeOffsetWrapped, holeOffsetMaxWrapped));
                if (opt_debug)
                    util::cout << "DiscoverTrackSectorScheme: pushed hole sector (offsetMin="
                        << holeOffsetWrapped << ", offsetMax=" << holeOffsetMaxWrapped << ")\n";
                holeOffsetWrapped += offsetDistanceMin;
            }
        }
    }

    if (!idAndOffsetPairsLocal.empty() && idAndOffsetPairsLocal.ReplaceMissingIdsByFindingTrackSectorIds())
    {
        idAndOffsetPairs = std::move(idAndOffsetPairsLocal);
        if (opt_debug)
        {
            const auto iSup = idAndOffsetPairs.size();
            for (int i = 0; i < iSup; i++)
            {
                util::cout << "DiscoverTrackSectorScheme: sectorIdsAndOffsets[" << i << "] has (id=" <<
                    idAndOffsetPairs[i].id << ", offsetMin=" <<
                    idAndOffsetPairs[i].offsetInterval.Start() << ", offsetMax=" <<
                    idAndOffsetPairs[i].offsetInterval.End() << ")\n";
            }
        }
        return true;
    }
    return false;
}

/*
 * Return empty result if different sized sectors are found or there are less
 * than 2 not orphan sectors.
 * The track must be single and not orphan data, it can have orphans.
 */
bool Track::DetermineOffsetDistanceMinMaxAverage(const RepeatedSectors& repeatedSectorIds)
{
    if (!opt_normal_disk)
        return false;
    if (!idOffsetDistanceInfo.IsEmpty())
        return true;
    if (size() < 2)
        return false;
    assert(tracklen > 0);

    const auto& encoding = getEncoding();
    auto predictedOverheadedSectorWithGap3WithDataBitsMin = 0;
    auto predictedOverheadedSectorWithGap3WithDataBitsMax = 0;

    VectorX<std::pair<int, double>> offsetDistances; // Sector index and offset distance to next per sector.
    // Determine the uniform offset distance by averaging neighbour sector distances and dropping the worst distance.
    auto firstNotOrphanSectorIndex = -1;
    auto firstNotOrphanSectorSize = 0;
    const auto repeatedSectorIdsEnd = repeatedSectorIds.end();
    const auto iSup = size();
    RingedInt iNext(0, iSup);
    for (auto i = 0; i < iSup; )
    {
        const auto& currentSector = operator[](i);
        if (currentSector.IsOrphan() || repeatedSectorIds.find(currentSector.header.sector) != repeatedSectorIdsEnd)
        {
            i++;
            continue;
        }
        assert(!currentSector.HasUnknownSize());
        if (firstNotOrphanSectorIndex < 0)
        {
            firstNotOrphanSectorIndex = i;
            firstNotOrphanSectorSize = currentSector.size();
            predictedOverheadedSectorWithGap3WithDataBitsMin = DataBytePositionAsBitOffset(
                GetFmOrMfmSectorOverheadWithGap3(getDataRate(), encoding, firstNotOrphanSectorSize, true), encoding);
            predictedOverheadedSectorWithGap3WithDataBitsMax = DataBytePositionAsBitOffset(
                GetFmOrMfmSectorOverheadWithGap3(getDataRate(), encoding, firstNotOrphanSectorSize), encoding);
        }
        else if (currentSector.size() != firstNotOrphanSectorSize) // Only same sized sectors are supported.
        {
            MessageCPP(msgWarningAlways, "Different sized sectors (", operator[](firstNotOrphanSectorIndex),
                ") and (", currentSector, ") are invalid in normal disk mode, sizes (",
                firstNotOrphanSectorSize, ", ", currentSector.size(),
                "), skipping determining id offset distance");
            return false;
        }

        iNext = i;
        while (operator[]((++iNext).Value()).IsOrphan()) ;
        if (iNext == i)
            break; // There is only 1 not orphan, can not calculate offset difference.
        const auto& nextSector = operator[](iNext.Value());
        if (repeatedSectorIds.find(nextSector.header.sector) != repeatedSectorIdsEnd)
        {
            i++;
            continue;
        }
        const double wrappedOffsetDiff = currentSector.OffsetDistanceFromThisTo(nextSector, tracklen);
        // The range between current and next sectors is examined.
        const int betweenSectors = round_AS<int>(ChooseCloserToInteger(
            wrappedOffsetDiff / predictedOverheadedSectorWithGap3WithDataBitsMin,
            wrappedOffsetDiff / predictedOverheadedSectorWithGap3WithDataBitsMax));
        if (betweenSectors == 0) // The two sectors are too close to each other, it is an error in normal disk mode.
        {
            MessageCPP(msgWarningAlways, "Too close sectors (", currentSector,
                ") (", nextSector, ") are invalid in normal disk mode, offsets (",
                currentSector.offset, ", ", nextSector.offset,
                "), omitting former sector from determining id offset distance");
            idOffsetDistanceInfo.ignoredIds.emplace(i);
        }
        else
            offsetDistances.emplace_back(i, wrappedOffsetDiff / betweenSectors);
        if (iNext <= i)
            break; // Wrapped, loop end.
        i = iNext.Value();
    }
    if (offsetDistances.empty())
        return false;
    if (offsetDistances.size() == 1)
    {
        idOffsetDistanceInfo.offsetDistanceAverage = offsetDistances[0].second;
        idOffsetDistanceInfo.offsetDistanceMin = idOffsetDistanceInfo.offsetDistanceAverage;
        idOffsetDistanceInfo.offsetDistanceMax = idOffsetDistanceInfo.offsetDistanceAverage;
        return true;
    }
    auto averageOffsetDistance = Average<std::pair<int, double>, double>(
        offsetDistances, [](const std::pair<int, double>& element) { return element.second; });
    do
    {
        std::sort(offsetDistances.begin(), offsetDistances.end(),
            [averageOffsetDistance](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            return std::abs(a.second - averageOffsetDistance) < std::abs(b.second - averageOffsetDistance);
        });
        const auto offsetDistanceMinMaxIt = std::minmax_element(
            offsetDistances.begin(), offsetDistances.end(),
            [averageOffsetDistance](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            return a.second < b.second;
        });
        const auto offsetDistanceMin = offsetDistanceMinMaxIt.first->second;
        const auto offsetDistanceMax = offsetDistanceMinMaxIt.second->second;
        // If the (max - average - (average - min)) / (max - min) is greater than 0.01 then count again without max element.
        const auto allowedOffsetDistanceMax = tolerated_offset_distance(encoding, opt_byte_tolerance_of_time);
        const auto variance = std::abs(offsetDistanceMin + offsetDistanceMax - 2 * averageOffsetDistance) / allowedOffsetDistanceMax;
        if (variance <= 0.1) // Experimental value.
            break;
        idOffsetDistanceInfo.notAverageFarFromNextIds.emplace(offsetDistances.back().first);
        const auto offsetDistanceSize = offsetDistances.size();
        averageOffsetDistance = (averageOffsetDistance * offsetDistanceSize - offsetDistances.back().second) / (offsetDistanceSize - 1);
        offsetDistances.erase(offsetDistances.end() - 1);
    } while (true);
    const auto offsetDistanceMinMaxIt = std::minmax_element(
        offsetDistances.begin(), offsetDistances.end(),
        [averageOffsetDistance](const std::pair<int, double>& a, const std::pair<int, double>& b) {
        return a.second < b.second;
    });
    idOffsetDistanceInfo.offsetDistanceMin = offsetDistanceMinMaxIt.first->second;
    idOffsetDistanceInfo.offsetDistanceMax = offsetDistanceMinMaxIt.second->second;
    idOffsetDistanceInfo.offsetDistanceAverage = averageOffsetDistance;
    return true;
}

// Track can have orphans.
void Track::CollectRepeatedSectorIdsInto(RepeatedSectors& repeatedSectorIds) const
{
    if (size() < 2)
        return;
    assert(tracklen > 0);

    const auto iSup = size();
    const auto iMax = iSup - 1;
    for (auto i = 0; i < iMax; i++)
    {
        const auto& sector = operator[](i);
        if (sector.IsOrphan())
            continue;
        if (repeatedSectorIds.find(sector.header.sector) != repeatedSectorIds.end())
            continue;
        for (auto j = i + 1; j < iSup; j++)
        {
            auto& otherSector = operator[](j);
            if (otherSector.IsOrphan())
                continue;
            if (sector.CompareHeader(otherSector))
            {
                repeatedSectorIds.emplace(std::make_pair(sector.header.sector, VectorX<int>{sector.offset, otherSector.offset}));
                MessageCPP(msgWarningAlways, "Repeated sectors (", sector,
                    ") at offsets (", sector.offset , ", ", otherSector.offset,
                    ") are problematic");
                break; // TODO More than 1 repeation is not supported currently.
            }
        }
    }
}

// Determine best track length. This method is for multi revolution track.
int Track::determineBestTrackLen(const int timedTrackLen) const
{
    assert(timedTrackLen > 0);

    if (empty())
        return 0;
    VectorX<int> offsetDiffs;
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
    const auto offsetDiffBest = Track::findMostPopularToleratedDiff(offsetDiffs, getEncoding()); // This can be multiple tracklen. It must be reduced.
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
        if (std::abs(trackLenBest - timedTrackLen) > DataBytePositionAsBitOffset(Track::COMPARE_TOLERANCE_BYTES, getEncoding()))
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
    assert(!empty());

    const auto offsetDiff = DataBytePositionAsBitOffset(GetFmOrMfmIdamAndDamDistance(getDataRate(), getEncoding()), getEncoding());
    // We could check if the sector overlaps something existing but unimportant now.
    return modulo(dataOffset - offsetDiff, static_cast<unsigned>(tracklen));
}

Track& Track::format(const CylHead& cylhead, const Format& fmt)
{
    assert(fmt.sectors != 0);

    m_sectors.clear();
    m_sectors.reserve(fmt.sectors);

    for (auto id : fmt.get_ids(cylhead))
    {
        Header header(cylhead.cyl, cylhead.head ? fmt.head1 : fmt.head0, id, fmt.size);
        Sector sector(fmt.datarate, fmt.encoding, header, fmt.gap3);
        Data data(fmt.sector_size(), fmt.fill);

        sector.add(std::move(data));
        add(std::move(sector));
    }

    return *this;
}

// The track must not be orphan data track.
Data::const_iterator Track::populate(Data::const_iterator it, Data::const_iterator itEnd)
{
    assert(std::distance(it, itEnd) >= 0);

    // Populate in sector number order, which requires sorting the track
    VectorX<Sector*> ptrs(m_sectors.size());
    std::transform(m_sectors.begin(), m_sectors.end(), ptrs.begin(), [](Sector& s) { return &s; });
    std::sort(ptrs.begin(), ptrs.end(), [](Sector* a, Sector* b) { return a->header.sector < b->header.sector; });

    for (auto sector : ptrs)
    {
        assert(sector->copies() == 1);
        assert(!sector->HasUnknownSize());
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

Sectors::const_iterator Track::find(const Header& header, const int offset) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return offset == s.offset && header == s.header;
        });
}

Sectors::const_iterator Track::findToleratedSame(const Header& header, const int offset, int tracklen) const
{
    return std::find_if(begin(), end(), [&](const Sector& s) {
        return s.is_sector_tolerated_same(header, offset, opt_byte_tolerance_of_time, tracklen);
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

Sectors::const_iterator Track::findSectorForDataFmOrMfm(const int dataOffset, const int sizeCode, bool findClosest/* = true*/) const
{
    if (empty())
        return end();
    assert(dataOffset > 0);

    const auto dataRate = getDataRate();
    const auto encoding = getEncoding();
    const auto itEnd = end();
    auto itFound = end();
    // Find a sector close enough to the data offset to be the same one.
    for (auto it = begin() ; it != itEnd; it++)
    {
        const auto cohereResult = DoSectorIdAndDataOffsetsCohere(it->offset, dataOffset, dataRate, encoding);
        if (cohereResult == CohereResult::DataTooEarly)
            break;
        else if (cohereResult == CohereResult::DataTooLate)
            continue;
        if (sizeCode == Header::SIZECODE_UNKNOWN || it->header.size == sizeCode)
        {
            if (!findClosest)
                return it;
            itFound = it;
        }
    }
    return itFound;
}
