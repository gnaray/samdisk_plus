// Core disk class

#include "Options.h"
#include "Disk.h"
//#include "IBMPC.h"
#include "ThreadPool.h"

#include <algorithm>

static auto& opt_mt = getOpt<int>("mt");

//////////////////////////////////////////////////////////////////////////////

const std::string Disk::TYPE_UNKNOWN{"<unknown>"};
Disk::Disk(Format& fmt_)
    : fmt(fmt_)
{
    format(fmt);
}

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


bool Disk::preload(const Range& range_, int cyl_step)
{
    // No pre-loading if multi-threading disabled, or only a single core
    if (!opt_mt || ThreadPool::get_thread_count() <= 1)
        return false;

    ThreadPool pool;
    std::vector<std::future<void>> rets;

    range_.each([&](const CylHead cylhead) {
        rets.push_back(pool.enqueue([this, cylhead, cyl_step]() {
            read_track(cylhead * cyl_step);
            }));
        });

    for (auto& ret : rets)
        ret.get();

    return true;
}

void Disk::clear()
{
    GetTrackData().clear();
}


bool Disk::is_constant_disk() const
{
    return true;
}

void Disk::disk_is_read()
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

const Track& Disk::read_track(const CylHead& cylhead, bool uncached /* = false*/)
{
    return readNC(cylhead, uncached).track();
}

const BitBuffer& Disk::read_bitstream(const CylHead& cylhead, bool uncached /* = false*/)
{
    return readNC(cylhead, uncached).bitstream();
}

const FluxData& Disk::read_flux(const CylHead& cylhead, bool uncached /* = false*/)
{
    return readNC(cylhead, uncached).flux();
}


/*virtual*/ TrackData& Disk::writeNC(TrackData&& trackdata)
{
    // Invalidate stored format, since we can no longer guarantee a match
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

const Track& Disk::write(const CylHead& cylhead, Track&& track)
{
    return writeNC(TrackData(cylhead, std::move(track))).track();
}

const BitBuffer& Disk::write(const CylHead& cylhead, BitBuffer&& bitbuf)
{
    return writeNC(TrackData(cylhead, std::move(bitbuf))).bitstream();
}

const FluxData& Disk::write(const CylHead& cylhead, FluxData&& flux_revs, bool normalised /* = false*/)
{
    return writeNC(TrackData(cylhead, std::move(flux_revs), normalised)).flux();
}


void Disk::each(const std::function<void(const CylHead & cylhead, const Track & track)>& func, bool cyls_first /* = false*/)
{
    if (!GetTrackData().empty())
    {
        range().each([&](const CylHead& cylhead) {
            func(cylhead, read_track(cylhead));
            }, cyls_first);
    }
}

void Disk::format(const RegularFormat& reg_fmt, const Data& data /* = Data()*/, bool cyls_first /* = false*/)
{
    format(Format(reg_fmt), data, cyls_first);
}

void Disk::format(const Format& new_fmt, const Data& data /* = Data()*/, bool cyls_first /* = false*/)
{
    auto it = data.begin(), itEnd = data.end();

    new_fmt.range().each([&](const CylHead& cylhead) {
        Track track;
        track.format(cylhead, new_fmt);
        it = track.populate(it, itEnd);
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
    if (!new_cyls && !new_heads)
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
