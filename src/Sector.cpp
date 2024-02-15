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

Sector::Sector(DataRate datarate_, Encoding encoding_, const Header& header_, int gap3_)
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
    if (sector.copies() == 0 || copies() == 0)
        return false;

    assert(header.size != SIZECODE_UNKNOWN);
    // This size and sector size are the same since headers are the same.
    // Both first sectors must have at least the natural size to compare
    if (sector.data_size() < sector.size() || data_size() < size())
        return false;

    // The natural data contents must match
    return std::equal(data_copy().begin(), data_copy().begin() + size(), sector.data_copy().begin());
}

int Sector::size() const
{
    assert(header.size != SIZECODE_UNKNOWN);

    return header.sector_size();
}

int Sector::data_size() const
{
    return copies() ? m_data[0].size() : 0;
}

const DataList& Sector::datas() const
{
    return m_data;
}

const Data& Sector::data_copy(int copy/*=0*/) const
{
    assert(m_data.size() != 0);
    copy = std::max(std::min(copy, m_data.size() - 1), 0);
    return m_data[copy];
}

Data& Sector::data_copy(int copy/*=0*/)
{
    assert(m_data.size() != 0);
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
    assert(m_data_read_stats.size() != 0);
    instance = std::max(std::min(instance, m_data_read_stats.size() - 1), 0);
    return m_data_read_stats[instance];
}

DataReadStats& Sector::data_copy_read_stats(int instance/*=0*/)
{
    assert(m_data_read_stats.size() != 0);
    instance = std::max(std::min(instance, m_data_read_stats.size() - 1), 0);
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
    if (!has_data())
        return -1;

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
bool Sector::has_stable_data() const
{
    const bool result = has_good_data(!opt_normal_disk, opt_normal_disk);
    // Backward compatibility: if no paranoia then good data is also stable data.
    if (!opt_paranoia || !result)
        return result;
    const auto read_count = data_best_copy_read_stats().ReadCount();
    return read_count >= opt_stability_level;
}

int Sector::GetGoodDataCopyStabilityScore(int instance) const
{
    // Backward compatibility: if no paranoia then good data has stability level 1.
    if (!opt_paranoia)
        return 1;
    const auto read_count = data_copy_read_stats(instance).ReadCount();
    return std::min(read_count, opt_stability_level);
}

/* Return values.
 * - Unchanged: The new data is ignored, it is not counted in read stats.
 * - Matched: The new data is not added because it exists but counted in read stats.
 * - Improved: The new data is replacing an old data, counted in read stats.
 * - NewData: The new data is added and all old data is discarded, counted in read stats.
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
            header.size != SIZECODE_UNKNOWN && new_data.size() >= size() + 2)
    {
        CRC16 crc;
        if (encoding == Encoding::MFM) crc.init(CRC16::A1A1A1);
        crc.add(new_dam);
        auto bad_data_crc = crc.add(new_data.data(), size() + 2) != 0;
        if (bad_crc != bad_data_crc)
             util::cout << std::boolalpha << "Debug assert failed: New sector data has " << bad_crc
                << " CRC and shortening it to expected sector size it has " << bad_data_crc << " CRC\n";
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
    if (copies() > 0)
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
        ret = Merge::Unchanged;
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
    if (ret == Merge::Unchanged)
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
    if (!has_baddatacrc() && sector.has_baddatacrc())
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
            if (ret == Merge::Unchanged || ret == Merge::Matched || (ret == Merge::Improved && add_ret == Merge::NewData))
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
    assert(parentSector.header.size != SIZECODE_UNKNOWN);
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
    if (copies() > 0 && !AcceptOrphanDataSectorSizeForMerging(orphanDataSector.data_size()))
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
    assert(header.size != SIZECODE_UNKNOWN);
    return data_size() > size();
}

bool Sector::has_shortdata() const
{
    assert(header.size != SIZECODE_UNKNOWN);
    return data_size() < size();
}

bool Sector::has_normaldata() const
{
    assert(header.size != SIZECODE_UNKNOWN);
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
    {   // Convert set of bad data to good data or create new good data.
        const auto fill_byte = lossless_static_cast<uint8_t>((opt_fill >= 0) ? opt_fill : 0);

        assert(header.size != SIZECODE_UNKNOWN);
        if (!has_data())
        {
            m_data.push_back(Data(size(), fill_byte));
            m_data_read_stats.emplace_back(DataReadStats()); // This is a newly created (not read) data.
        }
        else if (copies() > 1)
        {
            if (!opt_paranoia)
                resize_data(1);

            if (data_size() < size())
            {
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

bool Sector::is_sector_tolerated_same(const Sector& sector, const int byte_tolerance_of_time, const int tracklen) const
{
    // Sector must be close enough and have the same header.
    return are_offsets_tolerated_same(offset, sector.offset, byte_tolerance_of_time, tracklen)
            && header == sector.header;
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

// The sector and tracklen is from same track, this sector is from another.
bool Sector::has_same_record_properties(const Sector& other_sector, const int other_tracklen) const
{
    // Headers must match.
    if (other_sector.has_badidcrc() || has_badidcrc() || other_sector.header != header)
        return false;

    // Encodings must match.
    if (other_sector.encoding != encoding)
        return false;

    // Datarates must match interchangeably.
    if (other_sector.datarate != datarate && !are_interchangeably_equal_datarates(other_sector.datarate, datarate))
        return false;

    // Offsets must match interchangeably.
    auto offset_normalised = offset;
    if (other_sector.datarate != datarate && are_interchangeably_equal_datarates(other_sector.datarate, datarate))
        offset_normalised = convert_offset_by_datarate(offset, datarate, other_sector.datarate);
    return are_offsets_tolerated_same(offset_normalised, other_sector.offset, opt_byte_tolerance_of_time, other_tracklen);
}

void Sector::remove_gapdata(bool keep_crc/*=false*/)
{
    if (!has_gapdata())
        return;

    for (auto& data : m_data)
    {
        assert(header.size != SIZECODE_UNKNOWN);
        // If requested, attempt to preserve CRC bytes on bad sectors.
        if (keep_crc && has_baddatacrc() && data.size() >= (size() + 2))
            data.resize(size() + 2);
        else
            data.resize(size());
    }
}

// Looking for parent sector id by offset. It means this sector is orphan and its offset is data offset.
int Sector::FindParentSectorIdByOffset(const IdAndOffsetVector& sectorIdsAndOffsets) const
{
    for (const auto& idAndOffset : sectorIdsAndOffsets)
    {
        const auto cohereResult = DoSectorIdAndDataOffsetsCohere(idAndOffset.offset, offset, datarate, encoding);
        if (cohereResult == CohereResult::DataCoheres)
            return idAndOffset.id;
        if (cohereResult == CohereResult::DataTooEarly)
            break;
    }

    return -1;
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

const UniqueSectors UniqueSectors::StableSectors() const
{
    UniqueSectors stableSectors;
    std::copy_if(begin(), end(), std::inserter(stableSectors, stableSectors.end()),
                 [&](const Sector& sector) {
        if (sector.has_badidcrc())
            return false;
        // Checksummable 8k sector is considered in has_stable_data method.
        return sector.has_stable_data();
    });

    return stableSectors;
}

bool UniqueSectors::Contains(const Sector& other_sector, const int other_tracklen) const
{
    return std::any_of(cbegin(), cend(), [&](const Sector& sectorI) {
        return sectorI.has_same_record_properties(other_sector, other_tracklen);
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
    for (auto id = id_interval.Left(); id <= id_interval.Right() ; id++)
        if (contained_ids.find(id) == contained_ids.end())
            return true;
    return false;
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
