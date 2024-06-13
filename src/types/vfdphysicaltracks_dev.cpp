// vfdphysicaltracks virtual device

#include "types/vfdphysicaltracks_dev.h"
#include "MemFile.h"
#include "PhysicalTrackMFM.h"

VfdPhysicalTracksDevDisk::VfdPhysicalTracksDevDisk(const std::string& path)
{
    try
    {
        SetMetadata(path);
    }
    catch (...)
    {
        throw util::exception("failed to initialise vfdphysicaltracks device");
    }
    m_path = path;
}

bool VfdPhysicalTracksDevDisk::preload(const Range&/*range*/, int /*cyl_step*/) /*override*/
{
    return false;
}

bool VfdPhysicalTracksDevDisk::is_constant_disk() const /*override*/
{
    return false;
}

bool VfdPhysicalTracksDevDisk::supports_retries() const /*override*/
{
    return true;
}

bool VfdPhysicalTracksDevDisk::supports_rescans() const /*override*/
{
    return true;
}

TrackData VfdPhysicalTracksDevDisk::load(const CylHead& cylhead, bool /*first_read*/,
    int /*with_head_seek_to*/, const DeviceReadingPolicy& /*deviceReadingPolicy*//* = DeviceReadingPolicy{}*/) /*override*/
{
    return LoadPhysicalTrack(cylhead);
}

TrackData VfdPhysicalTracksDevDisk::LoadPhysicalTrack(const CylHead& cylhead)
{
    const auto pattern = " Raw track (cyl %02d head %1d).floppy_raw_track";
    const auto fileNamePart = util::fmt(pattern, cylhead.cyl, cylhead.head);
    const auto physicalTrackFilePath = FindFirstFileOnly(fileNamePart, m_path);
    Data physicalTrackContent;

    if (!physicalTrackFilePath.empty())
    {
        try
        {
            ReadBinaryFile(physicalTrackFilePath, physicalTrackContent);
        }
        catch (std::exception & e)
        {
            util::cout << "File read error: " << e.what() << "\n";
        }
    }

    PhysicalTrackMFM physicalTrackMFM(physicalTrackContent, DataRate::_250K);
    auto physicalTrack = physicalTrackMFM;
    auto bitstream = physicalTrack.AsMFMBitstream();
    auto trackdata = TrackData(cylhead, std::move(bitstream));
    trackdata.fix_track_readstats();
    return trackdata;
}

void VfdPhysicalTracksDevDisk::SetMetadata(const std::string&/* path*/)
{
    static const VectorX<std::string> fdc_types{
        "Unknown", "Unknown1", "Normal", "Enhanced", "82077", "82077AA", "82078_44", "82078_64", "National" };
    static const VectorX<std::string> data_rates{
        "250K", "300K", "500K", "1M", "2M" };

    metadata()["fdc_type"] = fdc_types[2];
    metadata()["data_rates"] = data_rates[0];
}

bool ReadVfdphysicaltracks(const std::string& path, std::shared_ptr<Disk>& disk)
{
    if (!IsVfdpt(path))
        return false;
    const auto realPath = path.substr(6);
    if (!IsDir(realPath))
        throw util::exception("failed to open vfdphysicaltracks device");

    auto vfdphysicaltracks_dev_disk = std::make_shared<VfdPhysicalTracksDevDisk>(realPath);
    vfdphysicaltracks_dev_disk->extend(CylHead(83 - 1, 2 - 1));

    vfdphysicaltracks_dev_disk->strType() = "Vfdphysicaltrack";
    disk = vfdphysicaltracks_dev_disk;

    return true;
}

bool WriteVfdphysicaltracks(const std::string& path, std::shared_ptr<Disk>&/*disk*/)
{
    if (!IsFloppyDevice(path))
        return false;

    throw util::exception("vfdphysicaltracks writing not yet implemented");
}
