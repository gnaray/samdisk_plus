#include "Sector.h"
#include "Cpp_helpers.h"
#include "Options.h"
#include "CRC16.h"
#include "DiskUtil.h"
#include "Util.h"
#include "PhysicalTrackMFM.h"

#include <algorithm>
#include <cstring>
#include <cmath>

static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_fill = getOpt<int>("fill");
static auto& opt_maxcopies = getOpt<int>("maxcopies");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_paranoia = getOpt<bool>("paranoia");
static auto& opt_stability_level = getOpt<int>("stability_level");

bool DataReadStats::IsStable() const
{
    return m_read_count >= opt_stability_level;
}

Sector::Sector(DataRate datarate_, Encoding encoding_, const Header& header_/* = Header()*/, int gap3_/* = 0*/)
    : header(header_), datarate(datarate_), encoding(encoding_), gap3(gap3_)
{
}

Sector Sector::CopyWithoutData(bool keepReadAttempts/* = true*/) const
{
    auto& thisWritable = *const_cast<Sector*>(this);
    auto dataTmp = std::move(thisWritable.m_data);
    auto dataReadStatsTmp = std::move(thisWritable.m_data_read_stats);
    thisWritable.m_data.clear();
    thisWritable.m_data_read_stats.clear();
    Sector sector(*this);
    thisWritable.m_data = std::move(dataTmp);
    thisWritable.m_data_read_stats = std::move(dataReadStatsTmp);

    sector.set_baddatacrc(false); // No data is copied thus the datacrc must be the default (good) also.
    if (!keepReadAttempts)
        sector.m_read_attempts = 0;

    return sector;
}

bool Sector::operator== (const Sector& sector) const
{
    // Headers must match
    if (sector.header != header)
        return false;

    // If neither has data it's a match
    if (sector.m_data.size() == 0 && m_data.size() == 0)
        return true;

    // Both sectors must have some data
    if (!sector.has_data() || !has_data())
        return false;

    assert(!HasUnknownSize());
    // This size and sector size are the same since headers are the same.
    // Both first sectors must have at least the natural size to compare
    if (sector.data_size() < sector.size() || data_size() < size())
        return false;

    // The natural data contents must match
    return std::equal(data_copy().begin(), data_copy().begin() + size(), sector.data_copy().begin());
}

int Sector::size() const
{
    assert(!HasUnknownSize());

    return header.sector_size();
}

int Sector::data_size() const
{
    return has_data() ? m_data[0].size() : 0;
}

const DataList& Sector::datas() const
{
    return m_data;
}

const Data& Sector::data_copy(int copy/*=0*/) const
{
    assert(has_data());

    copy = std::max(std::min(copy, m_data.size() - 1), 0);
    return m_data[copy];
}

Data& Sector::data_copy(int copy/*=0*/)
{
    assert(has_data());

    copy = std::max(std::min(copy, m_data.size() - 1), 0);
    return m_data[copy];
}

const Data& Sector::data_best_copy() const
{
    return data_copy(get_data_best_copy_index());
}

Data& Sector::data_best_copy()
{
    return data_copy(get_data_best_copy_index());
}

const DataReadStats& Sector::data_copy_read_stats(int instance/*=0*/) const
{
    assert(!m_data_read_stats.empty());

    return m_data_read_stats[instance];
}

DataReadStats& Sector::data_copy_read_stats(int instance/*=0*/)
{
    assert(!m_data_read_stats.empty());

    return m_data_read_stats[instance];
}

const DataReadStats& Sector::data_best_copy_read_stats() const
{
    return data_copy_read_stats(get_data_best_copy_index());
}

DataReadStats& Sector::data_best_copy_read_stats()
{
    return data_copy_read_stats(get_data_best_copy_index());
}

int Sector::get_data_best_copy_index() const
{
    assert(has_data());

    return static_cast<int>(std::max_element(m_data_read_stats.begin(), m_data_read_stats.end(),
                            [](const DataReadStats& a, const DataReadStats& b) { return a.ReadCount() < b.ReadCount(); })
            - m_data_read_stats.begin());
}

int Sector::read_attempts() const
{
    return m_read_attempts;
}

void Sector::set_read_attempts(int read_attempts)
{
    m_read_attempts = read_attempts;
}

void Sector::add_read_attempts(int read_attempts)
{
    m_read_attempts += read_attempts;
}

bool Sector::is_constant_disk() const
{
    return m_constant_disk;
}

void Sector::set_constant_disk(bool constant_disk)
{
    m_constant_disk = constant_disk;
}

void Sector::fix_readstats()
{
    // Fixing sector to have data read stats in case it does not have. Older
    // image files know nothing about readstats thus fixing is necessary.
    // This case is detectable by existing copies but no read stats.
    const auto data_copies = copies();
    if (data_copies > 0 && m_data_read_stats.size() == 0)
    {
        m_data_read_stats.resize(data_copies, DataReadStats(1));
        m_read_attempts = data_copies;
    }
}

int Sector::copies() const
{
    return m_data.size();
}

void Sector::add_read_stats(int instance, DataReadStats&& data_read_stats)
{
    m_data_read_stats[instance] += std::move(data_read_stats);
}

void Sector::set_read_stats(int instance, DataReadStats&& data_read_stats)
{
    m_data_read_stats[instance] = std::move(data_read_stats);
}

// The stable sector is good sector and in paranoia mode it is read at least stability level times.
bool Sector::has_stable_data(bool consider_checksummable_8K/* = false*/) const
{
    const bool result = has_good_data(consider_checksummable_8K && !opt_normal_disk, opt_normal_disk);
    // Backward compatibility: if no paranoia then good data is also stable data.
    if (!opt_paranoia || !result)
        return result;
    const auto read_stats = data_best_copy_read_stats();
    return read_stats.IsStable();
}

int Sector::GetGoodDataCopyStabilityScore(int instance) const
{
    // Backward compatibility: if no paranoia then good data has stability level 1.
    if (!opt_paranoia)
        return 1;
    const auto read_count = data_copy_read_stats(instance).ReadCount();
    return std::min(read_count, opt_stability_level);
}

bool Sector::HasNormalHeaderAndMisreadFromNeighborCyl(const CylHead& cylHead, const int trackSup) const
{
    return header.IsNormalAndOriginatedFromNeighborCyl(cylHead, trackSup);
}

/* Return values.
 * - Unchanged: The new data is ignored, it is not counted in read stats.
 * - Matched: The new data is not added because it exists but counted in read stats.
 * - Improved: The new data is replacing an old data, counted in read stats.
 * - NewData: The new data is added and all old data is discarded, counted in read stats.
 * - NewDataOverLimit: The new data could not be added due to copies limit.
 */
Sector::Merge Sector::add_original(Data&& new_data, bool bad_crc/*=false*/, uint8_t new_dam/*=IBM_DAM*/, int* affected_data_index/*=nullptr*/,
    DataReadStats* improved_data_read_stats/*=nullptr*/)
{
    Merge ret = Merge::NewData;

    // If the sector has a bad header CRC, it can't have any data
    if (has_badidcrc())
        return Merge::Unchanged;

#ifdef _DEBUG
    // If there's enough data, check the CRC
    if ((encoding == Encoding::MFM || encoding == Encoding::FM) &&
        !HasUnknownSize() && new_data.size() >= size() + 2)
    {
        CRC16 crc;
        if (encoding == Encoding::MFM) crc.init(CRC16::A1A1A1);
        crc.add(new_dam);
        auto bad_data_crc = crc.add(new_data.data(), size() + 2) != 0;
        if (bad_crc != bad_data_crc)
             util::cout << "Debug assert failed: New sector data has " << (bad_crc ? "bad" : "good")
                << " CRC and shortening it to expected sector size it has " << (bad_data_crc ? "bad" : "good") << " CRC\n";
    }
#endif

    // If the existing sector has good data, ignore supplied data if it's bad
    if (bad_crc && has_good_data())
        return Merge::Unchanged;

    // If the existing sector is bad, new good data will replace it all
    if (!bad_crc && has_baddatacrc())
    {
        remove_data();
        ret = Merge::NewData; // NewData instead of Improved because technically this is new data.
    }

    // 8K sectors always have a CRC error, but may include a secondary checksum
    if (is_8k_sector())
    {
        // Attempt to identify the 8K checksum method used by the new data
        // If it's recognised, replace any existing data with it
        if (!ChecksumMethods(new_data.data(), new_data.size()).empty())
        {
            remove_data();
            ret = Merge::NewData; // NewData instead of Improved because technically this is new data.
        }
        // Do we already have a copy?
        else if (copies() == 1)
        {
            // Can we identify the method used by the existing copy?
            if (!ChecksumMethods(m_data[0].data(), m_data[0].size()).empty())
            {
                // Keep the existing, ignoring the new data
                return Merge::Unchanged;
            }
        }
    }

    // DD 8K sectors are considered complete at 6K, everything else at natural size
    auto complete_size = is_8k_sector() ? 0x1800 : new_data.size();

    // Compare existing data with the new data, to avoid storing redundant copies.
    // The goal is keeping only 1 optimal sized data amongst those having matching content.
    // Optimal sized: complete size else smallest above complete size else biggest below complete size.
    const auto i_sup = m_data.size();
    for (auto i = 0; i < i_sup; i++)
    {
        const auto& data = m_data[i];
        const auto common_size = std::min({ data.size(), new_data.size(), complete_size });
        if (std::equal(data.begin(), data.begin() + common_size, new_data.begin()))
        {
            if (data.size() == new_data.size())
            {
                if (affected_data_index != nullptr)
                    *affected_data_index = i;
                return Merge::Matched; // was Unchanged;
            }
            if (new_data.size() < data.size())
            {
                if (new_data.size() < complete_size)
                {
                    if (affected_data_index != nullptr)
                        *affected_data_index = i;
                    return Merge::Matched; // was Unchanged;
                }
                // The new shorter complete copy replaces the existing data.
                if (improved_data_read_stats != nullptr)
                    *improved_data_read_stats = m_data_read_stats[i];
                erase_data(i--);
                ret = Merge::Improved;
                break; // Not continuing. See the goal above.
            }
            else
            {
                if (data.size() >= complete_size)
                {
                    if (affected_data_index != nullptr)
                        *affected_data_index = i;
                    return Merge::Matched; // was Unchanged;
                }
                // The new longer complete copy replaces the existing data.
                if (improved_data_read_stats != nullptr)
                    *improved_data_read_stats = m_data_read_stats[i];
                erase_data(i--);
                ret = Merge::Improved;
                break; // Not continuing. See the goal above.
            }
        }
    }

    // Will we now have multiple copies?
    if (has_data())
    {
        // Damage can cause us to see different DAM values for a sector.
        // Favour normal over deleted, and deleted over anything else.
        if (dam != new_dam &&
            (dam == IBM_DAM || (dam == IBM_DAM_DELETED && new_dam != IBM_DAM)))
        {
            return Merge::Unchanged;
        }

        // Multiple good copies mean a difference in the gap data after
        // a good sector, perhaps due to a splice. We just ignore it.
        // IMHO originally the goal was to avoid multiple good copies assuming a good CRC means good data.
        // However in paranoia mode a good CRC does not necessarily mean good data.
        if (!has_baddatacrc() && !opt_paranoia)
            return Merge::Unchanged;

        // Keep multiple copies the same size, whichever is shortest
        const auto new_size = std::min(new_data.size(), m_data[0].size());
        new_data.resize(new_size);

        // Resize any existing copies to match
        for (auto& d : m_data)
            d.resize(new_size);
        // TODO It can happen that copies are full and the new data will not be added.
        // Still the existing data might be resized to the shorter new data length. Misleading.
    }

    // If copies are full then discard the new data and copies above max and return unchanged.
    if (are_copies_full(opt_maxcopies))
    {
        limit_copies(opt_maxcopies);
        ret = Merge::NewDataOverLimit;
    }
    else
    {
        // Insert the new data copy.
        m_data.emplace_back(std::move(new_data));
    }

    // Update the data CRC state and DAM
    m_bad_data_crc = bad_crc;
    dam = new_dam;

    return ret;
}

void Sector::assign(Data&& data)
{
    m_data.clear();
    m_data.push_back(data);
    constexpr auto data_copies = 1;
    m_read_attempts = data_copies;
    m_data_read_stats.clear();
    m_data_read_stats.resize(data_copies, DataReadStats(1));
}

Sector::Merge Sector::add(Data&& new_data, bool new_bad_crc/*=false*/, uint8_t new_dam/*=IBM_DAM*/,
    int new_read_attempts/*=1*/, const DataReadStats& new_data_read_stats/*=DataReadStats(1)*/,
    bool readstats_counter_mode/*= true*/, bool update_this_read_attempts/*=true*/)
{
    auto affected_data_index = -1;
    DataReadStats improved_data_read_stats;
    const auto ret = add_original(std::move(new_data), new_bad_crc, new_dam, &affected_data_index, &improved_data_read_stats);
    process_merge_result(ret, new_read_attempts, new_data_read_stats, readstats_counter_mode,
        affected_data_index, improved_data_read_stats);
    if (update_this_read_attempts)
        m_read_attempts += new_read_attempts;
    return ret;
}

void Sector::process_merge_result(const Merge& ret, int new_read_attempts, const DataReadStats& new_data_read_stats,
    bool readstats_counter_mode, int affected_data_index, const DataReadStats& improved_data_read_stats)
{
    if (ret == Merge::Unchanged || ret == Merge::NewDataOverLimit)
        return;
    if (ret == Merge::NewData)
    {
        m_data_read_stats.emplace_back(new_data_read_stats);
        return;
    }
    if (ret == Merge::Matched || ret == Merge::Improved)
    {
        if (readstats_counter_mode) // counter combination, i.e. summing readstats
        {
            if (ret == Merge::Matched)
                m_data_read_stats[affected_data_index] += new_data_read_stats;
            else // Improved
                m_data_read_stats.emplace_back(new_data_read_stats + improved_data_read_stats);
        }
        else // % combination, to prefer higher read rate.
        {
            auto data_read_stats = ret == Merge::Matched ? m_data_read_stats[affected_data_index] : improved_data_read_stats;
            auto combined_read_attempts = read_attempts() + new_read_attempts;
            auto read_rate = lossless_static_cast<double>(data_read_stats.ReadCount()) / m_read_attempts;
            auto new_read_rate = lossless_static_cast<double>(new_data_read_stats.ReadCount()) / new_read_attempts;
            auto combined_read_rate = read_rate + new_read_rate - read_rate * new_read_rate;
            auto combined_read_count = round_AS<int>(combined_read_rate * combined_read_attempts);
            if (ret == Merge::Matched)
                m_data_read_stats[affected_data_index] = DataReadStats(combined_read_count);
            else // Improved
                m_data_read_stats.emplace_back(DataReadStats(combined_read_count));
        }
    }
    else
        throw util::exception("unimplemented Merge value (", static_cast<int>(ret), ")");
}

Sector::Merge Sector::merge(Sector&& sector)
{
    Merge ret = Merge::Unchanged;

    // If the new header CRC is bad there's nothing we can use
    if (sector.has_badidcrc()) // If badidcrc then the sector was not tried to read thus not counting readstats.
        return Merge::Unchanged;

    // Something is wrong if the new details don't match the existing one
    assert(sector.header == header);
    assert(sector.datarate == datarate);
    assert(sector.encoding == encoding);

    // If the existing header is bad, repair it
    if (has_badidcrc())
    {
        header = sector.header; // TODO Always equals. Else the assert above would have been activated.
        set_badidcrc(false);
        ret = Merge::Improved;
    }

    // We can't repair good data with bad
    if (has_data() && !has_baddatacrc() && sector.has_baddatacrc())
    {
        m_read_attempts += sector.m_read_attempts;
        return ret;
    }

    // Add the new data snapshots
    const auto i_sup = sector.copies();
    for (auto i = 0; i < i_sup; i++)
    {
        // Move the data into place, passing on the existing data CRC status and DAM
        const auto add_ret = add(std::move(sector.m_data[i]),
                sector.has_baddatacrc(), sector.dam, sector.m_read_attempts,
                sector.m_data_read_stats[i],
                !sector.is_constant_disk(), false);
        if (add_ret != Merge::Unchanged)
        {   // Set the most important return result.
            if (ret == Merge::Unchanged || ret == Merge::Matched
                || (ret == Merge::Improved && (add_ret == Merge::NewData || add_ret == Merge::NewDataOverLimit)))
                ret = add_ret;
        }
    }
    m_read_attempts += sector.m_read_attempts;

    return ret;
}

// This can be orphan data or not orphan data sector.
bool Sector::AcceptOrphanDataSectorSizeForMerging(const int orphanDataPhysicalSize) const
{
    const auto thisDataPhysicalSize = header.sector == OrphanDataCapableTrack::ORPHAN_SECTOR_ID ? data_size() : SectorDataFromPhysicalTrack::PhysicalSizeOf(data_size());
    return thisDataPhysicalSize <= orphanDataPhysicalSize;
}

// Method for the case when sector size and parent sector became known and this orphan data sector can be converted.
void Sector::ConvertOrphanDataSectorLikeParentSector(const Sector& parentSector)
{
    assert(!parentSector.HasUnknownSize());
    const int sectorSize = parentSector.size();

    header = parentSector.header;
    offset = parentSector.offset;

    const auto iSup = copies();
    if (iSup == 0)
        return;

    const auto physicalDataSize = std::min(data_size(), SectorDataFromPhysicalTrack::PhysicalSizeOf(sectorSize)); // The data_size is the same for each data.
    auto data = std::move(m_data);
    const auto dataReadStats = std::move(m_data_read_stats);
    remove_data();

    for (int i = 0; i < iSup; i++)
    {
        auto& physicalData = data[i];
        /* TODO If the physical data size is less than physical sector size then the data ends at next AM or track end.
         * It means the data contains gap3 and sync thus its crc will be bad.
         * I am not sure which bytes the FDC reads latest but theoretically we could find the end of good data
         * by calculating crc for each data length and if it becomes 0 then probably we found the correct length,
         * and the last two bytes are the crc.
         */
        physicalData.resize(physicalDataSize);
        const SectorDataFromPhysicalTrack sectorData(encoding, 0, std::move(physicalData), true);
        // Passing read attempts = 0 does not change the read_attempts so it remains correct.
        add(sectorData.GetData(), sectorData.badCrc, sectorData.addressMark, 0, dataReadStats[i]);
    }
}

// Merge orphan data sector into this not orphan data sector.
void Sector::MergeOrphanDataSector(Sector&& orphanDataSector)
{
    if (datarate != orphanDataSector.datarate)
        throw util::exception("can't mix datarates when merging sectors");

    // Merge the orphan data sector if there is no previous data or the previous data is not longer (i.e. orphan data sector can be broken, merge it if there is no longer data).
    if (has_data() && !AcceptOrphanDataSectorSizeForMerging(orphanDataSector.data_size()))
    {
        if (opt_debug)
            util::cout << "MergeOrphanDataSector: not merging orphan data sector (offset=" << orphanDataSector.offset << ", id.sector=" << orphanDataSector.header.sector << "\n";
    }
    else
    {
        orphanDataSector.ConvertOrphanDataSectorLikeParentSector(*this);
        // The orphanDataSector is now a mergable sector and its data size is <= data size of this.
        merge(std::move(orphanDataSector));
    }
}

// Merge orphan data sector into this not orphan data sector.
void Sector::MergeOrphanDataSector(const Sector& orphanDataSector)
{
    MergeOrphanDataSector(Sector(orphanDataSector));
}

bool Sector::MakeOffsetNot0(const bool warn/* = true*/)
{
    const auto result = offset == 0;
    if (result) // Avoid setting offset 0.
    {
        offset++;
        if (warn && !IsOrphan())
            MessageCPP(msgWarningAlways, "Offset of sector (", header, ") is changed from 0 to 1");
    }
    return result;
}


bool Sector::has_data() const
{
    return copies() != 0;
}

// consider_normal_disk inhibits consider_checksummable_8K because checksummable_8K is not normal.
bool Sector::has_good_data(bool consider_checksummable_8K/* = false*/, bool consider_normal_disk/* = false*/) const
{
    return consider_normal_disk ? has_good_normaldata()
            : ((consider_checksummable_8K && is_checksummable_8k_sector())
            || (has_data() && !has_baddatacrc() && !has_gapdata()));
}

bool Sector::has_gapdata() const
{
    assert(!HasUnknownSize());
    return data_size() > size();
}

bool Sector::has_shortdata() const
{
    assert(!HasUnknownSize());
    return data_size() < size();
}

bool Sector::has_normaldata() const
{
    assert(!HasUnknownSize());
    return data_size() == size();
}

bool Sector::has_good_normaldata() const
{
    return has_data() && !has_baddatacrc() && has_normaldata();
}

bool Sector::is_8k_sector() const
{
    // +3 and CPC disks treat this as a virtual complete sector
    return datarate == DataRate::_250K && encoding == Encoding::MFM &&
        header.size == 6 && has_data();
}

bool Sector::is_checksummable_8k_sector() const
{
    if (is_8k_sector() && has_data())
    {
        const Data& data = data_copy();
        if (!ChecksumMethods(data.data(), data.size()).empty())
            return true;
    }
    return false;
}

void Sector::set_badidcrc(bool bad/* = true*/)
{
    m_bad_id_crc = bad;

    if (bad)
        remove_data();
}

void Sector::set_baddatacrc(bool bad/* = true*/)
{
    m_bad_data_crc = bad;

    if (!bad)
    {   // Convert set of bad data to good data.
        if (has_data())
        {
            assert(!HasUnknownSize());
            if (copies() > 1)
                if (!opt_paranoia)
                    resize_data(1);

            if (data_size() < size())
            {
                const auto fill_byte = lossless_static_cast<uint8_t>((opt_fill >= 0) ? opt_fill : 0);
                const Data pad(size() - data_size(), fill_byte);
                for (auto& data : m_data)
                    data.insert(data.end(), pad.begin(), pad.end());
            }
        }
    }
}

void Sector::erase_data(int instance)
{
    m_data.erase(m_data.begin() + instance);
    m_data_read_stats.erase(m_data_read_stats.begin() + instance);
}

void Sector::resize_data(int count)
{
    m_data.resize(count);
    m_data_read_stats.resize(count);
}

void Sector::remove_data()
{
    m_data.clear();
    m_data_read_stats.clear();
    m_bad_data_crc = false;
    dam = IBM_DAM;
}

bool Sector::are_copies_full(int max_copies) const
{
    return copies() >= max_copies;
}

void Sector::limit_copies(int max_copies)
{
    if (copies() > max_copies)
    {
        m_data.resize(max_copies);
        m_data_read_stats.resize(max_copies);
    }
}

bool Sector::is_sector_tolerated_same(const Header& otherHeader, const int otherOffset, const int byte_tolerance_of_time, const int trackLen) const
{
    // Sector must be close enough and have the same header.
    return are_offsets_tolerated_same(offset, otherOffset, encoding, byte_tolerance_of_time, trackLen)
        && header == otherHeader;
}

bool Sector::is_sector_tolerated_same(const Sector& sector, const int byte_tolerance_of_time, const int trackLen) const
{
    // Sector must be close enough and have the same header.
    return is_sector_tolerated_same(sector.header, sector.offset, byte_tolerance_of_time, trackLen);
}

void Sector::normalise_datarate(const DataRate& datarate_target)
{
    if (datarate_target != datarate && are_interchangeably_equal_datarates(datarate, datarate_target))
    {
        // Convert this offset according to target data rate.
        offset = convert_offset_by_datarate(offset, datarate, datarate_target);
        // Convert this to target data rate.
        datarate = datarate_target;
    }
}

// This sector is from a track, the otherSector and otherTrackLen is from another same track.
bool Sector::has_same_record_properties(const int thisTrackLen, const Sector& otherSector, const int otherTrackLen, const bool ignoreOffsets/* = false*/) const
{
    // Headers must match.
    if (otherSector.has_badidcrc() || has_badidcrc() || otherSector.header != header)
        return false;

    // Encodings must match.
    if (otherSector.encoding != encoding)
        return false;

    // Datarates must match interchangeably.
    if (otherSector.datarate != datarate && !are_interchangeably_equal_datarates(otherSector.datarate, datarate))
        return false;

    // If this and other tracklen is 0 then the offsets are matching.
    if (thisTrackLen == 0 && otherTrackLen == 0)
        return true;

    // If this xor other tracklen is 0 then the offsets are not matching.
    if (thisTrackLen == 0 || otherTrackLen == 0)
    {
        util::cout << "Comparing two sectors while exactly one has 0 tracklen is suspicious!\n";
        return false;
    }

    if (ignoreOffsets)
        return true;

    // Offsets must match interchangeably.
    auto offset_normalised = offset;
    if (otherSector.datarate != datarate && are_interchangeably_equal_datarates(otherSector.datarate, datarate))
        offset_normalised = convert_offset_by_datarate(offset, datarate, otherSector.datarate);
    // Offsets can be compared if normalising this offset from this tracklen to other tracklen.
    offset_normalised = round_AS<int>(static_cast<double>(offset_normalised) * otherTrackLen / thisTrackLen);
    return are_offsets_tolerated_same(offset_normalised, otherSector.offset, encoding, opt_byte_tolerance_of_time, otherTrackLen);
}

bool Sector::CompareHeader(const Sector& sector) const
{
    return header == sector.header;
}

bool Sector::CompareHeaderDatarateEncoding(const Sector& sector) const
{
    return datarate == sector.datarate && encoding == sector.encoding && CompareHeader(sector);
}

int Sector::NextSectorOffsetDistanceMin() const
{
    return DataBytePositionAsBitOffset(GetFmOrMfmSectorOverheadWithGap3(datarate, encoding, size(), true), encoding);
}

void Sector::remove_gapdata(bool keep_crc/*=false*/)
{
    if (!has_gapdata())
        return;

    for (auto& data : m_data)
    {
        assert(!HasUnknownSize());
        // If requested, attempt to preserve CRC bytes on bad sectors.
        if (keep_crc && has_baddatacrc() && data.size() >= (size() + 2))
            data.resize(size() + 2);
        else
            data.resize(size());
    }
}

// Look for sector id by offset. It means this sector is not orphan.
IdAndOffsetPairs::const_iterator Sector::FindSectorIdByOffset(const IdAndOffsetPairs& sectorIdAndOffsetPairs) const
{
    return sectorIdAndOffsetPairs.FindSectorIdByOffset(offset);
}

// Return offset interval suitable for parent sector. It means this sector is orphan and its offset is data offset.
Interval<int> Sector::GetOffsetIntervalSuitableForParent(const int trackLen) const
{
    assert(trackLen > 0);

    int min_distance, max_distance;
    GetSectorIdAndDataOffsetDistanceMinMax(datarate, encoding, min_distance, max_distance);
    const auto uTrackLen = static_cast<unsigned>(trackLen);
    const auto dataOffsetMin = modulo(offset - max_distance, uTrackLen);
    const auto dataOffsetMax = modulo(offset - min_distance, uTrackLen);
    const Interval<int> dataOffsetInterval(dataOffsetMin, dataOffsetMax, BaseInterval::ConstructMode::StartAndEnd);
    return dataOffsetInterval;
}

// Check if parent sector is suitable by offset. It means this sector is orphan and its offset is data offset.
bool Sector::IsOffsetSuitableAsParent(const int parentOffset, const int trackLen) const
{
    assert(trackLen > 0);

    return GetOffsetIntervalSuitableForParent(trackLen).Where(parentOffset) == BaseInterval::Location::Within;
}

// Look for parent sector id by offset. It means this sector is orphan and its offset is data offset.
int Sector::FindParentSectorIdByOffset(const IdAndOffsetPairs& sectorIdAndOffsetPairs, const int trackLen) const
{
    assert(trackLen > 0);

    auto result = -1;
    const auto parentOffsetInterval = GetOffsetIntervalSuitableForParent(trackLen);
    for (const auto& idAndOffset : sectorIdAndOffsetPairs)
    {
        if (parentOffsetInterval.IsRingedIntersecting(idAndOffset.offsetInterval))
        {
            if (result < 0)
                result = idAndOffset.id;
            else
            {
                MessageCPP(msgWarningAlways, "Ambiguous results (", result, ", ", idAndOffset.id,
                    ") when finding parent sector id for offset (", offset, ")");
                result = -1;
                break;
            }
        }
    }

    return result;
}

std::string Sector::ToString(bool onlyRelevantData/* = true*/) const
{
    std::ostringstream ss;
    ss << header.ToString(onlyRelevantData);
    return ss.str();
}

//////////////////////////////////////////////////////////////////////////////

std::string Sectors::ToString(bool onlyRelevantData/* = true*/) const
{
    std::ostringstream ss;
    if (!onlyRelevantData || !empty())
    {
        bool writingStarted = false;
        std::for_each(cbegin(), cend(), [&](const Sector& sector) {
            if (writingStarted)
                ss << ' ';
            else
                writingStarted = true;
            ss << sector.ToString(onlyRelevantData);
        });
    }
    return ss.str();
}

//////////////////////////////////////////////////////////////////////////////

bool UniqueSectors::Contains(const Sector& other_sector, const int other_tracklen, const bool ignoreOffsets/* = false*/) const
{
    return std::any_of(cbegin(), cend(), [&](const Sector& sectorI) {
        return sectorI.has_same_record_properties(trackLen, other_sector, other_tracklen, ignoreOffsets);
    });
}

bool UniqueSectors::AnyIdsNotContainedInThis(const Interval<int>& id_interval) const
{
    assert(!id_interval.IsEmpty());

    if (empty())
        return true;

    UniqueSectors not_contained_ids;
    std::set<int> contained_ids;
    std::for_each(begin(), end(), [&](const Sector& sector)
    {
        if (id_interval.Where(sector.header.sector) == BaseInterval::Within)
            contained_ids.emplace(sector.header.sector);
    });
    for (auto id = id_interval.Start(); id <= id_interval.End() ; id++)
        if (contained_ids.find(id) == contained_ids.end())
            return true;
    return false;
}

UniqueSectors::const_iterator UniqueSectors::FindToleratedSameSector(const Sector& sector,
    const int byte_tolerance_of_time, const int trackLen) const
{
    const auto itEnd = cend();
    for (auto it = cbegin(); it != itEnd; it++)
        if (sector.is_sector_tolerated_same(*it, byte_tolerance_of_time, trackLen))
            return it;
    return itEnd;
}

std::string UniqueSectors::SectorHeaderSectorsToString() const
{
    std::ostringstream ss;
    bool writingStarted = false;
    std::for_each(cbegin(), cend(), [&](const Sector& sector) {
        if (writingStarted)
            ss << ' ';
        else
            writingStarted = true;
        ss << sector.header.sector;
    });
    return ss.str();
}

std::string UniqueSectors::ToString(bool onlyRelevantData/* = true*/) const
{
    std::ostringstream ss;
    if (!onlyRelevantData || !empty())
    {
        bool writingStarted = false;
        std::for_each(cbegin(), cend(), [&](const Sector& sector) {
            if (writingStarted)
                ss << ' ';
            else
                writingStarted = true;
            ss << sector.ToString(onlyRelevantData);
        });
    }
    return ss.str();
}

//////////////////////////////////////////////////////////////////////////////
