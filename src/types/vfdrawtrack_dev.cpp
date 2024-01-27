// vfdrawtrack virtual device

#include "types/vfdrawtrack_dev.h"
#include "MemFile.h"
#include "RawTrackMFM.h"

VfdRawTrackDevDisk::VfdRawTrackDevDisk(const std::string& path)
{
    try
    {
        SetMetadata(path);
    }
    catch (...)
    {
        throw util::exception("failed to initialise vfdrawtrack device");
    }
    m_path = path;
}

bool VfdRawTrackDevDisk::preload(const Range&/*range*/, int /*cyl_step*/) /*override*/
{
    return false;
}

bool VfdRawTrackDevDisk::is_constant_disk() const /*override*/
{
    return false;
}

bool VfdRawTrackDevDisk::supports_retries() const /*override*/
{
    return true;
}

bool VfdRawTrackDevDisk::supports_rescans() const /*override*/
{
    return true;
}

TrackData VfdRawTrackDevDisk::load(const CylHead& cylhead, bool /*first_read*/,
    int /*with_head_seek_to*/, const DeviceReadingPolicy& /*deviceReadingPolicy*//* = DeviceReadingPolicy{}*/) /*override*/
{
    return LoadRawTrack(cylhead);
}

TrackData VfdRawTrackDevDisk::LoadRawTrack(const CylHead& cylhead)
{
    MemFile file;
    RawTrackMFM rawTrackMFM;
    const auto pattern = " Raw track (track %02d, head %1d).floppy_raw_track";
    const auto fileNamePart = util::fmt(pattern, cylhead.cyl, cylhead.head);
    const auto rawTrackFilePath = FindFirstFile(fileNamePart, m_path);

    do
    {
        if (rawTrackFilePath.empty())
            break;
        try
        {
            file.open(rawTrackFilePath);
        }
        catch (...)
        {
            break;
        }
        VectorX<uint8_t> rawTrackContent(file.size());
        if (file.rewind() && file.read(rawTrackContent))
            rawTrackMFM = RawTrackMFM(file.data(), DataRate::_250K);
    } while (false);

    auto rawTrack = rawTrackMFM;
    auto bitstream = rawTrack.AsBitstream();
    auto trackdata = TrackData(cylhead, std::move(bitstream));
    trackdata.fix_track_readstats();
    return trackdata;
}

void VfdRawTrackDevDisk::SetMetadata(const std::string&/* path*/)
{
    static const VectorX<std::string> fdc_types{
        "Unknown", "Unknown1", "Normal", "Enhanced", "82077", "82077AA", "82078_44", "82078_64", "National" };
    static const VectorX<std::string> data_rates{
        "250K", "300K", "500K", "1M", "2M" };

    metadata()["fdc_type"] = fdc_types[2];
    metadata()["data_rates"] = data_rates[0];
}

bool ReadVfdrawtrack(const std::string& path, std::shared_ptr<Disk>& disk)
{
    if (!IsVfdrt(path))
        return false;
    const auto realPath = path.substr(6);
    if (!IsDir(realPath))
        throw util::exception("failed to open vfdrawtrack device");

    auto fdrawcmd_dev_disk = std::make_shared<VfdRawTrackDevDisk>(realPath);
    fdrawcmd_dev_disk->extend(CylHead(83 - 1, 2 - 1));

    fdrawcmd_dev_disk->strType() = "Vfdrawtrack";
    disk = fdrawcmd_dev_disk;

    return true;
}

bool WriteVfdrawtrack(const std::string& path, std::shared_ptr<Disk>&/*disk*/)
{
    if (!IsFloppyDevice(path))
        return false;

    throw util::exception("vfdrawtrack writing not yet implemented");
}
