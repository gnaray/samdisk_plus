#pragma once

#include "TrackData.h"
#include "Format.h"
#include "DeviceReadingPolicy.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

#include <cstdint>
#include <functional>
#include <vector>
#include <map>
#include <mutex>

class Disk
{
public:
    static const std::string TYPE_UNKNOWN;

    Disk() = default;
    virtual ~Disk() = default;

    virtual bool preload(const Range& range, int cyl_step);
    virtual void clear();

    virtual bool is_constant_disk() const;
    virtual void disk_is_read();

    // This method is for Disk implementators, avoid using it from common context. Instead use read(...).
    virtual TrackData& readNC(const CylHead& cylhead, bool uncached = false,
                              int with_head_seek_to = -1,
                              const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{});
    const TrackData& read(const CylHead& cylhead, bool uncached = false,
                          int with_head_seek_to = -1,
                          const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{});
    const Track& read_track(const CylHead& cylhead, bool uncached = false);
    const BitBuffer& read_bitstream(const CylHead& cylhead, bool uncached = false);
    const FluxData& read_flux(const CylHead& cylhead, bool uncached = false);

    // This method is for Disk implementators, avoid using it from common context. Instead use write(...).
    virtual TrackData& writeNC(TrackData&& trackdata);
    const TrackData& write(TrackData&& trackdata);
    const Track& write(const CylHead& cylhead, Track&& track);
    const BitBuffer& write(const CylHead& cylhead, BitBuffer&& bitbuf);
    const FluxData& write(const CylHead& cylhead, FluxData&& flux_revs, bool normalised = false);

    void each(const std::function<void(const CylHead& cylhead, const Track& track)>& func, bool cyls_first = false);

    void format(const RegularFormat& reg_fmt, const Data& data = Data(), bool cyls_first = false);
    void format(const Format& fmt, const Data& data = Data(), bool cyls_first = false);
    void flip_sides();
    void resize(int cyls, int heads);

    const Sector& get_sector(const Header& header);
    const Sector* find(const Header& header);

    Range range() const;
    int cyls() const;
    int heads() const;

    static int TransferTrack(Disk& src_disk, const CylHead& cylhead, Disk& dst_disk,
                      const int disk_round, ScanContext& context, DeviceReadingPolicy& deviceReadingPolicy);

    virtual Format& fmt();
    virtual const Format& fmt() const;
    virtual std::map<std::string, std::string>& metadata();
    virtual const std::map<std::string, std::string>& metadata() const;
    virtual std::string& strType();
    virtual const std::string& strType() const;
    virtual std::map<CylHead, TrackData>& GetTrackData();
    virtual const std::map<CylHead, TrackData>& GetTrackData() const;
    virtual std::mutex& GetTrackDataMutex();

protected:
    Format m_fmt{};
    std::map<std::string, std::string> m_metadata{};
    std::string m_strType = TYPE_UNKNOWN;
    std::map<CylHead, TrackData> m_trackdata{};
    std::mutex m_trackdata_mutex{};
};
