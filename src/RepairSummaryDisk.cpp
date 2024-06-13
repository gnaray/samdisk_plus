// This a virtual disk which acts like the origin disk it reads from but the
// tracks are repaired continuously and stored in an internal disk.
// On each read the track is transferred with repair* mode from origin disk
// to the internal disk, then the track of internal track is returned.
// *: when the track of internal disk does not exist, copy mode is used.

#include "RepairSummaryDisk.h"

RepairSummaryDisk::RepairSummaryDisk(Disk& readFromDisk, ScanContext& context)
    : m_ReadFromDisk(readFromDisk), m_Context(context)
{
}

bool RepairSummaryDisk::is_constant_disk() const /*override*/
{
    return m_ReadFromDisk.is_constant_disk();
}

TrackData& RepairSummaryDisk::readNC(const CylHead& cylhead, bool uncached/* = false*/,
    int /*with_head_seek_to*//* = -1*/,
    const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/) /*override*/
{
    const auto trackExists = m_WriteToDisk.track_exists(cylhead);
    if (!uncached)
    {
        if (trackExists)
        {
            auto& trackData = m_WriteToDisk.readNC(cylhead);
            auto deviceReadingPolicyLocal = deviceReadingPolicy;
            deviceReadingPolicyLocal.AddSkippableSectors(trackData.track().stable_sectors());
            if (!deviceReadingPolicyLocal.WantMoreSectors())
                return trackData;
            uncached = true;
        }
    }
    const auto transferMode = trackExists ? Disk::Repair : Disk::Copy;
    TransferTrack(m_ReadFromDisk, cylhead, m_WriteToDisk, m_Context, transferMode, uncached, deviceReadingPolicy);
    auto& trackData = m_WriteToDisk.readNC(cylhead);
    if (transferMode == Disk::Repair) // Src track is probably less than dst track so update src track.
        m_ReadFromDisk.Disk::writeNC(TrackData(trackData), true);
    return trackData;
}

TrackData& RepairSummaryDisk::writeNC(TrackData&& trackdata, const bool keepStoredFormat/* = false*/) /*override*/
{
    auto trackdataCopy = m_ReadFromDisk.write(std::move(trackdata));
    return m_WriteToDisk.writeNC(std::move(trackdataCopy), keepStoredFormat);
}

void RepairSummaryDisk::clear() /*override*/
{
    m_ReadFromDisk.clear();
    m_WriteToDisk.clear();
}

void RepairSummaryDisk::clearCache(const Range& range) /*override*/
{
    m_ReadFromDisk.clearCache(range);
    m_WriteToDisk.clearCache(range);
}

bool RepairSummaryDisk::isCached(const CylHead& cylhead) const /*override*/
{
    return m_ReadFromDisk.isCached(cylhead);
}

void RepairSummaryDisk::disk_is_read() /*override*/
{
    m_ReadFromDisk.disk_is_read();
    m_WriteToDisk.disk_is_read();
}

Format& RepairSummaryDisk::fmt() /*override*/
{
    return m_ReadFromDisk.fmt();
}

const Format& RepairSummaryDisk::fmt() const /*override*/
{
    return m_ReadFromDisk.fmt();
}

std::map<std::string, std::string>& RepairSummaryDisk::metadata() /*override*/
{
    return m_ReadFromDisk.metadata();
}

const std::map<std::string, std::string>& RepairSummaryDisk::metadata() const /*override*/
{
    return m_ReadFromDisk.metadata();
}

std::string& RepairSummaryDisk::strType() /*override*/
{
    return m_ReadFromDisk.strType();
}

const std::string& RepairSummaryDisk::strType() const /*override*/
{
    return m_ReadFromDisk.strType();
}

std::shared_ptr<FileSystem>& RepairSummaryDisk::GetFileSystem() /*override*/
{
    return m_ReadFromDisk.GetFileSystem();
}

const std::shared_ptr<FileSystem>& RepairSummaryDisk::GetFileSystem() const /*override*/
{
    return m_ReadFromDisk.GetFileSystem();
}

std::set<std::string>& RepairSummaryDisk::GetTypeDomesticFileSystemNames() /*override*/
{
    return m_ReadFromDisk.GetTypeDomesticFileSystemNames();
}

const std::set<std::string>& RepairSummaryDisk::GetTypeDomesticFileSystemNames() const /*override*/
{
    return m_ReadFromDisk.GetTypeDomesticFileSystemNames();
}

std::string& RepairSummaryDisk::GetPath() /*override*/
{
    return m_ReadFromDisk.GetPath();
}

const std::string& RepairSummaryDisk::GetPath() const /*override*/
{
    return m_ReadFromDisk.GetPath();
}

std::map<CylHead, TrackData>& RepairSummaryDisk::GetTrackData() /*override*/
{
    return m_WriteToDisk.GetTrackData();
}

const std::map<CylHead, TrackData>& RepairSummaryDisk::GetTrackData() const /*override*/
{
    return m_WriteToDisk.GetTrackData();
}

std::mutex& RepairSummaryDisk::GetTrackDataMutex() /*override*/
{
    return m_WriteToDisk.GetTrackDataMutex();
}
