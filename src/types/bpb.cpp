// BIOS Parameter Block (part of FAT filesystem), for MS-DOS and compatible disks

#include "filesystems/Fat12FileSystem.h"
#include "Disk.h"
#include "MemFile.h"
#include "types/bpb.h"

#include <algorithm>
#include <string>
#include <memory>

constexpr const char* DISK_TYPE_BPB = "BPB";
constexpr const char* DISK_TYPE_BPB_DOS = "BPB (DOS signed)";

bool ReadBPB(MemFile& file, std::shared_ptr<Disk>& disk)
{
    BIOS_PARAMETER_BLOCK bpb{};
    if (!file.rewind() || !file.read(&bpb, sizeof(bpb)))
        return false;

    const auto fat12FileSystem = std::make_shared<Fat12FileSystem>(*disk);
    // Reject disks larger than geometry suggests, but accept space-saver truncated images.
    if (!fat12FileSystem->SetFormatByBPB(bpb)
            || fat12FileSystem->format.disk_size() < file.size())
        return false;

    Data bootSectorData(fat12FileSystem->format.sector_size());
    if (!file.rewind() || !file.read(bootSectorData))
        return false;

    file.rewind();
    disk->format(fat12FileSystem->format, file.data(), false, true);
    ConvertStRecoverMissOrBadSectors(disk);
    disk->strType() = fat12FileSystem->IsBootSectorSigned(bootSectorData) ? DISK_TYPE_BPB_DOS : DISK_TYPE_BPB;
    disk->GetFileSystem() = fat12FileSystem;
    disk->GetTypeDomesticFileSystemNames().emplace(Fat12FileSystem::Name());
    return true;
}

const std::string STRECOVER_MISS_OR_BAD("======== SORRY, THIS SECTOR CANNOT BE READ FROM FLOPPY DISK BY ST RECOVER. ========");
const int STRECOVER_MISS_OR_BAD_SIZE = STRECOVER_MISS_OR_BAD.size();

void ConvertStRecoverMissOrBadSectors(std::shared_ptr<Disk>& disk)
{
    disk->fmt().range().each([&](const CylHead& cylhead) {
        const auto& track = disk->read_track(cylhead);
        VectorX<int> badSectorIndices;
        const auto iSup = track.size();
        for (auto i = 0; i < iSup; i++)
        {
            const auto& sector = track[i];
            if (sector.copies() > 0 && sector.data_size() >= STRECOVER_MISS_OR_BAD_SIZE)
            {
                const auto charData = reinterpret_cast<const char*>(sector.data_copy().data());
                const std::string strData(charData, STRECOVER_MISS_OR_BAD_SIZE);
                if (strData.compare(STRECOVER_MISS_OR_BAD) == 0)
                    badSectorIndices.push_back(i);
            }
        }
        if (!badSectorIndices.empty())
        {
            auto trackWritable = track;
            const auto iSup = badSectorIndices.size();
            for (auto i = 0; i < iSup; i++)
                trackWritable[badSectorIndices[i]].remove_data();
            disk->write(cylhead, std::move(trackWritable));
        }
    }, false);
}
