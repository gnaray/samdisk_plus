#pragma once

#include "Disk.h"
#include "DeviceReadingPolicy.h"

#include <bitset>

class DemandDisk : public Disk
{
public:
    static constexpr int FIRST_READ_REVS = 2;
    static constexpr int REMAIN_READ_REVS = 5;

    TrackData& readNC(const CylHead& cylhead, bool uncached = false, int with_head_seek_to = -1, const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{}) override;
    TrackData& writeNC(TrackData&& trackdata, const bool keepStoredFormat = false) override;
    void clear() override;
    void clearCache(const Range& range) override;
    bool isCached(const CylHead& cylhead) const override;

    void disk_is_read() override;

    void extend(const CylHead& cylhead);

protected:
    virtual bool supports_retries() const;
    virtual bool supports_rescans() const;
    virtual TrackData load(const CylHead& cylhead, bool first_read = false,
        int with_head_seek_to = -1, const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{}) = 0;
    virtual void save(TrackData& trackdata);

    std::bitset<MAX_DISK_CYLS * MAX_DISK_HEADS> m_loaded{};
};
