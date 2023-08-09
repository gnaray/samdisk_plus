#pragma once

class FileSystem; // Can not include FileSystem.h because it includes Disk.h.
struct ScanContext;// Can not include DiskUtil.h because it includes Disk.h.

#include "TrackData.h"
#include "Format.h"
#include "DeviceReadingPolicy.h"

#include <cstdint>
#include <functional>
#include <set>
#include <map>
#include <mutex>
#include <memory>

class Disk
{
public:
    static const std::string TYPE_UNKNOWN;

    enum TransferMode { Copy, Merge, Repair };

    Disk() = default;
    virtual ~Disk() = default;

    virtual bool preload(const Range& range, int cyl_step);
    virtual void clear();
    virtual void clearCache(const Range& range);

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

    bool track_exists(const CylHead& cylhead) const;
    void format(const RegularFormat& reg_fmt, const Data& data = Data(), bool cyls_first = false);
    void format(const Format& fmt, const Data& data = Data(), bool cyls_first = false);
    void flip_sides();
    void resize(int cyls, int heads);

    const Sector& get_sector(const Header& header);
    const Sector* find(const Header& header);
    const Sector* find_ignoring_size(const Header& header);

    Range range() const;
    int cyls() const;
    int heads() const;

    static int TransferTrack(Disk& src_disk, const CylHead& cylhead,
                             Disk& dst_disk, ScanContext& context,
                             TransferMode transferMode, bool uncached = false,
                             const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{});

    bool WarnIfFileSystemFormatDiffers() const;

    virtual Format& fmt();
    virtual const Format& fmt() const;
    virtual std::map<std::string, std::string>& metadata();
    virtual const std::map<std::string, std::string>& metadata() const;
    virtual std::string& strType();
    virtual const std::string& strType() const;
    virtual std::shared_ptr<FileSystem>& GetFileSystem();
    virtual const std::shared_ptr<FileSystem>& GetFileSystem() const;
    virtual std::set<std::string>& GetTypeDomesticFileSystemNames();
    virtual const std::set<std::string>& GetTypeDomesticFileSystemNames() const;
    virtual std::string& GetPath();
    virtual const std::string& GetPath() const;
    virtual std::map<CylHead, TrackData>& GetTrackData();
    virtual const std::map<CylHead, TrackData>& GetTrackData() const;
    virtual std::mutex& GetTrackDataMutex();

protected:
    Format m_fmt{};
    std::map<std::string, std::string> m_metadata{};
    std::string m_strType = TYPE_UNKNOWN;
    std::shared_ptr<FileSystem> m_fileSystem{};
    std::set<std::string> m_typeDomesticFileSystemNames{};
    std::string m_path{};

    std::map<CylHead, TrackData> m_trackdata{};
    std::mutex m_trackdata_mutex{};
};
