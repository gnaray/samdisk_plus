// BIOS Parameter Block (part of FAT filesystem), for MS-DOS and compatible disks

#include "filesystems/Fat12FileSystem.h"
#include "Disk.h"
#include "MemFile.h"

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
    disk->format(fat12FileSystem->format, file.data());
    disk->strType() = fat12FileSystem->IsBootSectorSigned(bootSectorData) ? DISK_TYPE_BPB_DOS : DISK_TYPE_BPB;
    disk->GetFileSystem() = fat12FileSystem;
    disk->GetTypeDomesticFileSystemNames().emplace(Fat12FileSystem::Name());
    return true;
}
