#pragma once

#include "Disk.h"

#include <bitset>

class DemandDisk : public Disk
{
public:
    constexpr static int FIRST_READ_REVS = 2;
    constexpr static int REMAIN_READ_REVS = 5;

    const TrackData& read(const CylHead& cylhead, bool uncached = false, int with_head_seek_to = -1, const Headers& headers_of_stable_sectors = Headers()) override;
    const TrackData& write(TrackData&& trackdata) override;
    void clear() override;
    void disk_is_read() override;

    void extend(const CylHead& cylhead);

protected:
    virtual bool supports_retries() const;
    virtual TrackData load(const CylHead& cylhead, bool first_read = false,
        int with_head_seek_to = -1, const Headers& headers_of_stable_sectors = Headers()) = 0;
    virtual void save(TrackData& trackdata);

    std::bitset<MAX_DISK_CYLS * MAX_DISK_HEADS> m_loaded{};
};
