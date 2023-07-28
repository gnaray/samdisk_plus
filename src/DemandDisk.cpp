// Demand-loaded disk tracks, for slow media

#include "Options.h"
#include "DemandDisk.h"

// Storage for class statics.
constexpr int DemandDisk::FIRST_READ_REVS;
constexpr int DemandDisk::REMAIN_READ_REVS;

static auto& opt_rescans = getOpt<int>("rescans");
static auto& opt_retries = getOpt<int>("retries");


void DemandDisk::extend(const CylHead& cylhead)
{
    // Access the track entry to pre-extend the disk ahead of loading it
    GetTrackData()[cylhead].cylhead = cylhead;
}

/*virtual*/ bool DemandDisk::supports_retries() const
{
    // We only support full track rescans rather than individual sector retries.
    return false;
}

/*virtual*/ bool DemandDisk::supports_rescans() const
{
    // We only support full track rescans rather than individual sector rescans.
    return false;
}

void DemandDisk::disk_is_read() /*override*/
{
    // The goal of calling this method is to fix readstats of whole disk in case of not demand disks.
    // Do nothing here because demand disk has not read any tracks at all when this trigger is fired.
    // Instead the fixing of readstats is done per track in read method.
}

TrackData& DemandDisk::readNC(const CylHead& cylhead, bool uncached,
                                  int with_head_seek_to, const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/) /*override*/
{
    if (uncached || !m_loaded[lossless_static_cast<size_t>(cylhead.operator int())])
    {
        // Quick first read, plus sector-based conversion.
        auto trackdata = load(cylhead, true, with_head_seek_to, deviceReadingPolicy);
        auto& track = trackdata.track();

        // If the disk supports sector-level retries we won't duplicate them.
        auto retries = supports_retries() ? 0 : opt_retries;
        // If the disk supports rescans we won't duplicate them.
        auto rescans = supports_rescans() ? 0 : opt_rescans;

        // Consider rescans and error retries.
        while (rescans > 0 || retries > 0)
        {
            // If no more rescans are required, stop when there's nothing to fix.
            if (rescans <= 0 && track.has_all_stable_data(deviceReadingPolicy.SkippableSectors()))
                break;
            // Do not seek at second, third, etc. loading.
            auto rescan_trackdata = load(cylhead, false, -1, deviceReadingPolicy);
            auto& rescan_track = rescan_trackdata.track();

            // If the rescan found more sectors, use the new track data.
            // Else in case of same size if the rescan found more good sectors, use the new track data.
            if (rescan_track.size() > track.size()
                    || (rescan_track.size() == track.size() && (rescan_track.good_sectors().size() > track.good_sectors().size()
                    || (rescan_track.good_sectors().size() == track.good_sectors().size() && rescan_track.stable_sectors().size() > track.stable_sectors().size()))))
                std::swap(trackdata, rescan_trackdata);

            // Flux reads include 5 revolutions, others just 1
            auto revs = trackdata.has_flux() ? REMAIN_READ_REVS : 1;
            rescans -= revs;
            retries -= revs;
        }

        trackdata.fix_track_readstats();
        std::lock_guard<std::mutex> lock(GetTrackDataMutex());
        GetTrackData()[cylhead] = std::move(trackdata);
        m_loaded[lossless_static_cast<size_t>(cylhead.operator int())] = true;
    }

    return Disk::readNC(cylhead);
}

/*virtual*/ void DemandDisk::save(TrackData& /*trackdata*/)
{
    throw util::exception("writing to this device is not currently supported");
}

/*virtual*/ TrackData& DemandDisk::writeNC(TrackData&& trackdata)
{
    save(trackdata);
    m_loaded[lossless_static_cast<size_t>(trackdata.cylhead.operator int())] = true;
    return Disk::writeNC(std::move(trackdata));
}

void DemandDisk::clear() /*override*/
{
    Disk::clear();
    m_loaded.reset();
}
