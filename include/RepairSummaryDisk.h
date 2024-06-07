#ifndef REPAIRSUMMARYDISK_H
#define REPAIRSUMMARYDISK_H

#include "Disk.h"
#include "DeviceReadingPolicy.h"
#include "DiskUtil.h"

class RepairSummaryDisk : public Disk
{
public:
    RepairSummaryDisk() = delete;
    RepairSummaryDisk(Disk& ReadFromDisk, ScanContext& context);

    bool is_constant_disk() const override;

    TrackData& readNC(const CylHead& cylhead, bool uncached = false, int with_head_seek_to = -1,
                          const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{}) override;
    TrackData& writeNC(TrackData&& trackdata, const bool keepStoredFormat = false) override;
    void clear() override;
    void clearCache(const Range& range) override;
    bool isCached(const CylHead& cylhead) const override;

    void disk_is_read() override;

    // Inherited Disk class members are not used, instead their related methods
    // are forwarded to m_ReadFromDisk and m_WriteToDisk.
    Format& fmt() override;
    const Format& fmt() const override;
    std::map<std::string, std::string>& metadata() override;
    const std::map<std::string, std::string>& metadata() const override;
    std::string& strType() override;
    const std::string& strType() const override;
    std::shared_ptr<FileSystem>& GetFileSystem() override;
    const std::shared_ptr<FileSystem>& GetFileSystem() const override;
    std::set<std::string>& GetTypeDomesticFileSystemNames() override;
    const std::set<std::string>& GetTypeDomesticFileSystemNames() const override;
    std::string& GetPath() override;
    const std::string& GetPath() const override;
    std::map<CylHead, TrackData>& GetTrackData() override;
    const std::map<CylHead, TrackData>& GetTrackData() const override;
    std::mutex& GetTrackDataMutex() override;

protected:
    Disk& m_ReadFromDisk;
    Disk m_WriteToDisk{};
    ScanContext& m_Context;
};

#endif // REPAIRSUMMARYDISK_H
