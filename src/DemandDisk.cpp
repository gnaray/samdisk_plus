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
    m_trackdata[cylhead].cylhead = cylhead;
}

bool DemandDisk::supports_retries() const
{
    // We only support full track rescans rather than individual sector retries.
    return false;
}

const TrackData& DemandDisk::read(const CylHead& cylhead, bool uncached, int with_head_seek_to, const Headers& headers_of_good_sectors)
{
    if (uncached || !m_loaded[cylhead])
    {
        // Quick first read, plus sector-based conversion.
        auto trackdata = load(cylhead, true, with_head_seek_to, headers_of_good_sectors);
        auto& track = trackdata.track();

        // If the disk supports sector-level retries we won't duplicate them.
        auto retries = supports_retries() ? 0 : opt_retries;
        auto rescans = opt_rescans;

        // Consider rescans and error retries.
        while (rescans > 0 || retries > 0)
        {
            // If no more rescans are required, stop when there's nothing to fix.
            if (rescans <= 0 && track.has_stable_data(headers_of_good_sectors))
                break;
            // Do not seek to 0th cyl at second, third, etc. loading.
            auto rescan_trackdata = load(cylhead, false, false, headers_of_good_sectors);
            auto& rescan_track = rescan_trackdata.track();

            // If the rescan found more sectors, use the new track data.
            // If the rescan found more good sectors, also use the new track data.
            if (rescan_track.size() > track.size() || rescan_track.good_sectors().size() > track.good_sectors().size())
                std::swap(trackdata, rescan_trackdata);

            // Flux reads include 5 revolutions, others just 1
            auto revs = trackdata.has_flux() ? REMAIN_READ_REVS : 1;
            rescans -= revs;
            retries -= revs;
        }

        std::lock_guard<std::mutex> lock(m_trackdata_mutex);
        m_trackdata[cylhead] = std::move(trackdata);
        m_loaded[cylhead] = true;
    }

    return Disk::read(cylhead);
}

void DemandDisk::save(TrackData&/*trackdata*/)
{
    throw util::exception("writing to this device is not currently supported");
}

const TrackData& DemandDisk::write(TrackData&& trackdata)
{
    save(trackdata);
    m_loaded[trackdata.cylhead] = true;
    return Disk::write(std::move(trackdata));
}

void DemandDisk::clear()
{
    Disk::clear();
    m_loaded.reset();
}
