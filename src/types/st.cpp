// Fat12FileSystem (including BIOS Parameter Block), for Atari ST disks

#include "HDD.h"
#include "Util.h"
#include "Disk.h"
#include "MemFile.h"
#include "filesystems/StFat12FileSystem.h"
#include "types/raw.h"
#include "DiskUtil.h"

#include <memory>
#include <numeric>
#include <array>

constexpr const char* DISK_FILE_EXTENSION = "st";
constexpr const char* DISK_TYPE_ST = "ST";
constexpr const char* DISK_TYPE_ST_BPB = "ST (by BPB)";
constexpr int ST_MIN_SIDES = 1;
constexpr int ST_MAX_SIDES = 2;
constexpr int ST_MIN_TRACKS = 80;
constexpr int ST_MAX_TRACKS = 84;
constexpr int ST_DD_MIN_SECTORS = 8;
constexpr int ST_DD_MAX_SECTORS = 11;
constexpr int ST_HD_MIN_SECTORS = 18; // In ST's HD floppy world there were not special formatted disks.
constexpr int ST_HD_MAX_SECTORS = 21; // Originally ST could not operate HD floppy drive but later it could by a HW modification.

bool isUnexpectedSTFormat(const Format& format)
{
    return format.cyls < ST_MIN_TRACKS || format.cyls > ST_MAX_TRACKS
            || format.heads < ST_MIN_SIDES || format.heads > ST_MAX_SIDES
            || format.sectors < ST_DD_MIN_SECTORS
            || (format.sectors > ST_DD_MAX_SECTORS && format.sectors < ST_HD_MIN_SECTORS)
            || format.sectors > ST_HD_MAX_SECTORS;
}

bool ReadST(MemFile& file, std::shared_ptr<Disk>& disk)
{
    Data bootSectorData(SECTOR_SIZE); // Enough size because BPB fits and checksum is calculated at most for this size (512).
    if (!file.rewind() || !file.read(bootSectorData))
        return false;

    const auto stFat12FileSystem = std::make_shared<StFat12FileSystem>(*disk);
    const auto diskHasFileSystem = stFat12FileSystem->SetFormatByBootSectorData(bootSectorData);

    // Accept either a valid boot checksum or .st file extension.
    if (!stFat12FileSystem->IsBootSectorBootable(bootSectorData) && !IsFileExt(file.name(), DISK_FILE_EXTENSION))
        return false;

    Format format{stFat12FileSystem->format};
    if (!diskHasFileSystem || stFat12FileSystem->format.disk_size() != file.size()
            || isUnexpectedSTFormat(stFat12FileSystem->format))
    {
        // Scan geometry combinations known to be used by ST disks. These disks having no filesystem were DD floppies.
        format.size = SizeToCode(SECTOR_SIZE);
        for (int cyls = ST_MAX_TRACKS; cyls >= ST_MIN_TRACKS; --cyls)
        {
            for (int heads = ST_MAX_SIDES; heads >= ST_MIN_SIDES; --heads)
            {
                for (int sectors = ST_DD_MAX_SECTORS; sectors >= ST_DD_MIN_SECTORS; --sectors)
                {
                    format.cyls = cyls;
                    format.heads = heads;
                    format.sectors = sectors;
                    if (format.disk_size() == file.size())
                        goto foundFormat;
                }
            }
        }
        return false;
    }

foundFormat:
    file.rewind();
    disk->format(format, file.data());
    disk->strType() = diskHasFileSystem ? DISK_TYPE_ST_BPB : DISK_TYPE_ST;
    if (diskHasFileSystem)
    {
        disk->GetFileSystem() = stFat12FileSystem;
        // FileSystem format should not differ from image file format. If it differs, fixing it by image file format.
        if (disk->WarnIfFileSystemFormatDiffers())
        {
            Message(msgInfo, "Overriding %s filesystem format read from image file (%s) by latter's format",
                    stFat12FileSystem->GetName().c_str(), file.path().c_str());
            stFat12FileSystem->format = format;
        }
    }
    disk->GetTypeDomesticFileSystemNames().emplace(StFat12FileSystem::Name());
    return true;
}

bool WriteST(FILE* f_, std::shared_ptr<Disk>& disk)
{
    disk->GetTypeDomesticFileSystemNames().emplace(StFat12FileSystem::Name());

    const auto format = CheckBeforeWriteRAW(disk);
    disk->fmt() = format;

    auto bpb_modified = false;
    const auto stFat12FileSystem = std::make_shared<StFat12FileSystem>(*disk, format);
    // If disk has no boot sector or it has any filesystem but not StFat12 or StFat12 with different format then write new BPB.
    if (stFat12FileSystem->EnsureBootSector() || (fileSystemWrappers.FindAndSetApprover(*disk)
            && !disk->GetFileSystem()->IsSameNamedWithSameCylHeadSectorsSize(*stFat12FileSystem)))
    {
        stFat12FileSystem->ReadBpbFromDisk();
        if ((bpb_modified = stFat12FileSystem->ReconstructBpb()))
            stFat12FileSystem->WriteBpbToDisk();
        disk->strType() = DISK_TYPE_ST_BPB;
        disk->GetFileSystem() = stFat12FileSystem;
    }
    if (disk->strType() == Disk::TYPE_UNKNOWN)
        disk->strType() = DISK_TYPE_ST;

    const auto result = WriteRegularDisk(f_, *disk, format);
    if (result) {
        Message(msgInfo, "Wrote %u cyl%s, %u head%s, %2u sector%s, %4u bytes/sector = %u bytes%s",
            format.cyls, (format.cyls == 1) ? "" : "s",
            format.heads, (format.heads == 1) ? "" : "s",
            format.sectors, (format.sectors == 1) ? "" : "s",
            format.sector_size(), format.disk_size(),
            bpb_modified ? " and boot sector with reconstructed BPB" : "");
    }
    return result;
}
