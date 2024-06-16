// Core disk class

#include "Disk.h"
#include "DiskUtil.h"
#include "FileSystem.h"
#include "Options.h"
#include "SAMdisk.h"
#include "ThreadPool.h"
#include "Util.h"

#include <algorithm>

static auto& opt_minimal = getOpt<int>("minimal");
static auto& opt_mt = getOpt<int>("mt");
static auto& opt_repair = getOpt<int>("repair");
static auto& opt_skip_stable_sectors = getOpt<bool>("skip_stable_sectors");
static auto& opt_step = getOpt<int>("step");
static auto& opt_track_retries = getOpt<RetryPolicy>("track_retries");
static auto& opt_verbose = getOpt<int>("verbose");


//////////////////////////////////////////////////////////////////////////////

const std::string Disk::TYPE_UNKNOWN{"<unknown>"};

Range Disk::range() const
{
    return Range(cyls(), heads());
}

int Disk::cyls() const
{
    return GetTrackData().empty() ? 0 : (GetTrackData().rbegin()->first.cyl + 1);
}

int Disk::heads() const
{
    if (GetTrackData().empty())
        return 0;

    auto it = std::find_if(GetTrackData().begin(), GetTrackData().end(), [](const std::pair<const CylHead, const TrackData>& p) {
        return p.first.head != 0;
        });

    return (it != GetTrackData().end()) ? 2 : 1;
}


/*virtual*/ bool Disk::preload(const Range& range_, int cyl_step)
{
    // No pre-loading if multi-threading disabled, or only a single core
    if (!opt_mt || ThreadPool::get_thread_count() <= 1)
        return false;

    ThreadPool pool;
    VectorX<std::future<void>> rets;

    range_.each([&](const CylHead cylhead) {
        rets.push_back(pool.enqueue([this, cylhead, cyl_step]() {
            read_track(cylhead * cyl_step);
            }));
        });

    for (auto& ret : rets)
        ret.get();

    return true;
}

/*virtual*/ void Disk::clear()
{
    GetTrackData().clear();
}

/*virtual*/ void Disk::clearCache(const Range& /*range*/)
{
}

/*virtual*/ bool Disk::isCached(const CylHead& /*cylhead*/) const
{
    return true;
}


/*virtual*/ bool Disk::is_constant_disk() const
{
    return true;
}

/*virtual*/ void Disk::disk_is_read()
{
    range().each([&](const CylHead& cylhead) {
        read_track(cylhead); // Ignoring returned track because it is const.
        GetTrackData()[cylhead].fix_track_readstats();
    });
}


/*virtual*/ TrackData& Disk::readNC(const CylHead& cylhead, bool /*uncached*//* = false*/,
                                    int /*with_head_seek_to*//* = -1*/,
                                    const DeviceReadingPolicy& /*deviceReadingPolicy*//* = DeviceReadingPolicy{}*/)
{
    // Safe look-up requires mutex ownership, in case of call from preload()
    std::lock_guard<std::mutex> lock(GetTrackDataMutex());
    return GetTrackData()[cylhead];
}

const TrackData& Disk::read(const CylHead& cylhead, bool uncached/* = false*/,
                            int with_head_seek_to/* = -1*/,
                            const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/)
{
    return readNC(cylhead, uncached, with_head_seek_to, deviceReadingPolicy);
}

const Track& Disk::read_track(const CylHead& cylhead, bool uncached/* = false*/,
    const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/)
{
    return readNC(cylhead, uncached, -1, deviceReadingPolicy).track();
}

const BitBuffer& Disk::read_bitstream(const CylHead& cylhead, bool uncached /* = false*/)
{
    return readNC(cylhead, uncached).bitstream();
}

const FluxData& Disk::read_flux(const CylHead& cylhead, bool uncached /* = false*/)
{
    return readNC(cylhead, uncached).flux();
}


/*virtual*/ TrackData& Disk::writeNC(TrackData&& trackdata, const bool keepStoredFormat/* = false*/)
{
    if (!keepStoredFormat) // Invalidate stored format, since we can no longer guarantee a match
        fmt().sectors = 0;

    std::lock_guard<std::mutex> lock(GetTrackDataMutex());
    auto cylhead = trackdata.cylhead;
    GetTrackData()[cylhead] = std::move(trackdata);
    return GetTrackData()[cylhead];
}

const TrackData& Disk::write(TrackData&& trackdata)
{
    return writeNC(std::move(trackdata));
}

const Track& Disk::write(const CylHead& cylhead, Track&& track, const bool keepStoredFormat/* = false*/)
{
    return writeNC(TrackData(cylhead, std::move(track)), keepStoredFormat).track();
}

const BitBuffer& Disk::write(const CylHead& cylhead, BitBuffer&& bitbuf)
{
    return writeNC(TrackData(cylhead, std::move(bitbuf))).bitstream();
}

const FluxData& Disk::write(const CylHead& cylhead, FluxData&& flux_revs, bool normalised /* = false*/)
{
    return writeNC(TrackData(cylhead, std::move(flux_revs), normalised)).flux();
}


void Disk::each(const std::function<void(const CylHead& cylhead, const Track& track)>& func, bool cyls_first /* = false*/)
{
    if (!GetTrackData().empty())
    {
        range().each([&](const CylHead& cylhead) {
            func(cylhead, read_track(cylhead));
            }, cyls_first);
    }
}

bool Disk::track_exists(const CylHead& cylhead) const
{
    return GetTrackData().find(cylhead) != GetTrackData().end();
}

void Disk::format(const RegularFormat& reg_fmt, const Data& data /* = Data()*/, bool cyls_first /* = false*/, const bool signIncompleteData/* = false*/)
{
    format(Format(reg_fmt), data, cyls_first, signIncompleteData);
}

void Disk::format(const Format& new_fmt, const Data& data /* = Data()*/, bool cyls_first /* = false*/, const bool signIncompleteData/* = false*/)
{
    auto it = data.begin(), itEnd = data.end();

    new_fmt.range().each([&](const CylHead& cylhead) {
        Track track;
        track.format(cylhead, new_fmt);
        it = track.populate(it, itEnd, signIncompleteData);
        write(cylhead, std::move(track));
        }, cyls_first);

    // Assign format after formatting as it's cleared by formatting
    fmt() = new_fmt;
}

void Disk::flip_sides()
{
    decltype(m_trackdata) trackdata;

    for (auto pair : GetTrackData())
    {
        CylHead cylhead = pair.first;
        cylhead.head ^= 1;

        // Move tracks to the new head position
        trackdata[cylhead] = std::move(pair.second);
    }

    // Finally, swap the gutted container with the new one
    std::swap(trackdata, GetTrackData());
}

void Disk::resize(int new_cyls, int new_heads)
{
    if (new_cyls <= 0 && new_heads <= 0)
    {
        GetTrackData().clear();
        return;
    }

    // Remove tracks beyond the new extent
    for (auto it = GetTrackData().begin(); it != GetTrackData().end(); )
    {
        if (it->first.cyl >= new_cyls || it->first.head >= new_heads)
            it = GetTrackData().erase(it);
        else
            ++it;
    }

    // If the disk is too small, insert a blank track to extend it
    if (cyls() < new_cyls || heads() < new_heads)
        GetTrackData()[CylHead(new_cyls - 1, new_heads - 1)];
}

const Sector& Disk::get_sector(const Header& header)
{
    return read_track(header).get_sector(header);
}

const Sector* Disk::find(const Header& header)
{
    auto& track = read_track(header);
    auto it = track.find(header);
    if (it != track.end())
        return &*it;
    return nullptr;
}

const Sector* Disk::find_ignoring_size(const Header& header)
{
    auto& track = read_track(header);
    // TODO Although using next line instead of previous line would optimise
    // reading, later it would be returned again as cached track instead of
    // reading all sectors.
    // E.g. a filesystem detector would request only boot sector here and due to
    // a weak track only that is found (no retry since the requested is found)
    // but later when reading track for all sectors it would return the cached
    // track containing only the boot sector even if there would be chance for
    // other sectors.
    // This situation could be improved by storing the requested sectors in
    // Track class and when next request is part of stored request then the
    // cached track can be returned otherwise uncached reading is necessary.
    // auto& track = read_track(header, false, DeviceReadingPolicy(Interval<int>(header.sector, 0, BaseInterval::ConstructMode::StartAndLength), false));
    auto it = track.findIgnoringSize(header);
    if (it != track.end())
        return &*it;
    return nullptr;
}

/*virtual*/ Format& Disk::fmt()
{
    return m_fmt;
}

/*virtual*/ const Format& Disk::fmt() const
{
    return m_fmt;
}

/*virtual*/ std::map<std::string, std::string>& Disk::metadata()
{
    return m_metadata;
}

/*virtual*/ const std::map<std::string, std::string>& Disk::metadata() const
{
    return m_metadata;
}

/*virtual*/ std::string& Disk::strType()
{
    return m_strType;
}

/*virtual*/ const std::string& Disk::strType() const
{
    return m_strType;
}

/*virtual*/ std::shared_ptr<FileSystem>& Disk::GetFileSystem()
{
    return m_fileSystem;
}

/*virtual*/ const std::shared_ptr<FileSystem>& Disk::GetFileSystem() const
{
    return m_fileSystem;
}

/*virtual*/ std::set<std::string>& Disk::GetTypeDomesticFileSystemNames()
{
    return m_typeDomesticFileSystemNames;
}

/*virtual*/ const std::set<std::string>& Disk::GetTypeDomesticFileSystemNames() const
{
    return m_typeDomesticFileSystemNames;
}

/*virtual*/ std::string& Disk::GetPath()
{
    return m_path;
}

/*virtual*/ const std::string& Disk::GetPath() const
{
    return m_path;
}

/*virtual*/ std::map<CylHead, TrackData>& Disk::GetTrackData()
{
    return m_trackdata;
}

/*virtual*/ const std::map<CylHead, TrackData>& Disk::GetTrackData() const
{
    return m_trackdata;
}

/*virtual*/ std::mutex& Disk::GetTrackDataMutex()
{
    return m_trackdata_mutex;
}

/*
 * Transfer means copy, merge or repair.
 * Copy: store src track in empty dst track.
 * Merge: store src track in loaded dst track.
 * Repair: repair loaded dst track by src track.
 */
/*static*/ int Disk::TransferTrack(Disk& src_disk, const CylHead& cylhead,
                                   Disk& dst_disk, ScanContext& context,
                                   TransferMode transferMode, bool uncached/* = false*/,
                                   const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/)
{
    int trackFixesNumber = 0;

    // In minimal reading mode, skip unused tracks
    if (opt_minimal && !IsTrackUsed(cylhead.cyl, cylhead.head))
        return trackFixesNumber;

    const bool repairMode = transferMode == Repair;
    const bool skip_stable_sectors = opt_skip_stable_sectors;
    DeviceReadingPolicy deviceReadingPolicyLocal{deviceReadingPolicy.WantedSectorHeaderSectors(), deviceReadingPolicy.LookForPossibleSectors()};

    TrackData dst_data;
    if (repairMode) // Read dst track early so we can check if it has bad sectors.
        dst_data = dst_disk.read(cylhead);
    // Do not retry track when
    // 1) not repairing because it overwrites previous data, wasting of time.
    // 2) disk is constant because the constant disk image always provides the same data, wasting of time.
    auto trackRetries = repairMode && !src_disk.is_constant_disk() && opt_track_retries >= 0 ? opt_track_retries : 0;
    auto track_round = 0;
    do
    {
        const bool is_track_retried = track_round > 0; // First reading is not retry.

        MessageCPP(msgStatus, (!is_track_retried ? "R" : "Rer"), "eading disk", cylhead);
        Track dst_track;
        if (repairMode) // Read dst track early so we can check if it has bad sectors.
        {
            dst_track = dst_data.track();
            NormaliseTrack(cylhead, dst_track);

            // If repair mode and user specified skip_stable_sectors then skip processing those.
            if (skip_stable_sectors)
            {
                deviceReadingPolicyLocal.SetSkippableSectors(deviceReadingPolicy.SkippableSectors());
                deviceReadingPolicyLocal.AddSkippableSectors(dst_track.stable_sectors());
                // If repair mode and user specified skip_stable_sectors and no looking for possible sectors
                // then do not repair track already containing all wanted sector ids (not empty) (thus those are skippable).
                if (!deviceReadingPolicyLocal.WantMoreSectors())
                    break;
                if (opt_verbose && !deviceReadingPolicyLocal.SkippableSectors().empty())
                {
                    Message(msgInfoAlways, "Ignoring already good sectors on %s: %s",
                            strCH(cylhead.cyl, cylhead.head).c_str(), deviceReadingPolicyLocal.SkippableSectors().SectorHeaderSectorsToString().c_str());
                }
            }
        }

        // https://docs.rs-online.com/41b6/0900766b8001b0a3.pdf, 7.2 Read error
        // Seeking head forward then backward then forward etc. when track is retried.
        const auto with_head_seek_to = is_track_retried ? std::max(0, std::min(cylhead.cyl + (track_round % 2 == 1 ? 1 : -1), src_disk.cyls() - 1)) : -1;
        auto src_data = src_disk.read(cylhead * opt_step, uncached || is_track_retried, with_head_seek_to, deviceReadingPolicyLocal);

        // Special case, force overriding cylhead of sector headers with cylhead.
        src_data.ForceCylHeads(src_disk.cyls());

        auto src_track = src_data.track();

        if (src_data.has_bitstream())
        {
            auto bitstream = src_data.bitstream();
            if (NormaliseBitstream(bitstream))
            {
                src_data = TrackData(src_data.cylhead, std::move(bitstream));
                src_track = src_data.track();
            }
        }

        bool changed = NormaliseTrack(cylhead, src_track);
        if (src_track.tracklen == 0 && deviceReadingPolicyLocal.SkippableSectors().trackLen > 0)
        {   // Source track has no tracklen but skippable sectors do, then fprget it.
            auto skippableSectors = deviceReadingPolicyLocal.SkippableSectors();
            skippableSectors.trackLen = 0;
            deviceReadingPolicyLocal.SetSkippableSectors(skippableSectors);
        }
        if (opt_verbose)
            ScanTrack(cylhead, src_track, context, deviceReadingPolicyLocal.SkippableSectors());

        // Repair? So neither copy nor merge.
        if (repairMode)
        {
            // Repair the target track using the source track.
            auto repair_track_changed_amount = RepairTrack(cylhead, dst_track, src_track, deviceReadingPolicyLocal.SkippableSectors());

            dst_data = TrackData(cylhead, std::move(dst_track));
            // If track retry is automatic and repairing then stop when repair could not improve the dst disk.
            if (repair_track_changed_amount > 0)
                trackRetries.wasChange = true;
            if (opt_verbose && repair_track_changed_amount > 0)
                MessageCPP(msgInfoAlways, "Destination disk's ",
                    strCH(cylhead.cyl, cylhead.head), " was repaired ", repair_track_changed_amount, " times");
            trackFixesNumber += repair_track_changed_amount;
        }
        else
        {
            // If the source track was modified it becomes the only track data.
            if (changed)
                dst_data = TrackData(cylhead, std::move(src_track));
            else
            {
                // Preserve any source data.
                src_data.cylhead = cylhead;
                dst_data = src_data;
            }
        }
        track_round++;
    } while (trackRetries.HasMoreRetryMinusMinus());
    dst_disk.write(std::move(dst_data));
    return trackFixesNumber;
}

bool Disk::WarnIfFileSystemFormatDiffers() const
{
    const auto fileSystem = GetFileSystem();
    if (is_constant_disk() && fileSystem)
    {
        const auto fileSystemFormat = fileSystem->GetFormat();
        const auto imageFormat = fmt();
        if (!imageFormat.IsNone() && !fileSystemFormat.IsSameCylHeadSectorsSize(imageFormat))
        {
            Message(msgWarning, "%s filesystem format in image file (%s) differs from image file format",
                    fileSystem->GetName().c_str(), GetPath().c_str());
            const auto fileContainsFileSystem = fileSystemFormat.cyls <= imageFormat.cyls && fileSystemFormat.heads <= imageFormat.heads
                    && fileSystemFormat.sectors <= imageFormat.sectors && fileSystemFormat.base >= imageFormat.base;
            Message(fileContainsFileSystem ? msgInfo : msgWarning, "%s filesystem is %s the file boundaries in image file (%s)",
                    fileSystem->GetName().c_str(), fileContainsFileSystem ? "within" : "outside", GetPath().c_str());
            return true;
        }
    }
    return false;
}
