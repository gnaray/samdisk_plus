// Atari ST

#include "HDD.h"
#include "Util.h"
#include "Disk.h"
#include "MemFile.h"
#include "bpb.h"
#include "filesystems/Fat12FileSystem.h"
#include "types/raw.h"
#include "DiskUtil.h"

#include <memory>
#include <numeric>
#include <array>

constexpr uint16_t ST_BOOT_CHECKSUM = 0x1234;

bool ReadST(MemFile& file, std::shared_ptr<Disk>& disk)
{
    std::array<uint8_t, SECTOR_SIZE> boot;
    if (!file.rewind() || !file.read(boot))
        return false;

    auto& wboot = *reinterpret_cast<const std::array<uint16_t, boot.size() / 2>*>(boot.data());
    auto be16_sum = [](auto a, auto b) -> uint16_t { return a + util::htobe(b); };
    auto checksum = std::accumulate(
        wboot.begin(), wboot.end(), uint16_t{ 0 }, be16_sum);

    // Accept either a valid boot checksum or .st file extension.
    if (checksum != ST_BOOT_CHECKSUM && !IsFileExt(file.name(), "st"))
        return false;

    auto& bpb = *reinterpret_cast<const BIOS_PARAMETER_BLOCK*>(boot.data());
    auto total_sectors = util::le_value(bpb.abSectors);

    Format fmt{ RegularFormat::AtariST };
    fmt.sectors = util::le_value(bpb.abSecPerTrack);
    fmt.heads = util::le_value(bpb.abHeads);
    fmt.cyls = (fmt.sectors && fmt.heads) ? (total_sectors / (fmt.sectors * fmt.heads)) : 0;
    fmt.size = SizeToCode(util::le_value(bpb.abBytesPerSec));
    fmt.gap3 = 0;   // auto

    if (fmt.TryValidate() && file.size() == fmt.disk_size())
    {
        if (fmt.track_size() < 6000)
            fmt.datarate = DataRate::_250K;
        else
            fmt.datarate = DataRate::_500K;

        file.rewind();
        disk->format(fmt, file.data());
        disk->strType() = "ST (BPB)";
        return true;
    }

    // Scan geometry combinations known to be used by ST disks.
    for (int cyls = 84; cyls >= 80; --cyls)
    {
        for (int heads = 2; heads >= 1; --heads)
        {
            for (int sectors = 11; sectors >= 8; --sectors)
            {
                fmt.cyls = cyls;
                fmt.heads = heads;
                fmt.sectors = sectors;
                fmt.size = SizeToCode(SECTOR_SIZE);

                if (fmt.disk_size() == file.size())
                {
                    file.rewind();
                    disk->format(fmt, file.data());
                    disk->strType() = "ST";
                    return true;
                }
            }
        }
    }

    return false;
}

bool WriteST(FILE* f_, std::shared_ptr<Disk>& disk)
{
    const auto fmt = CheckBeforeWriteRAW(disk);

    Fat12Filesystem fat12_fs(fmt, *disk);

    fat12_fs.PrepareBootSector();
    const auto bpb_modified = fat12_fs.ReconstructBpb();
    disk->strType() = "ST (BPB)";

    const auto result = WriteRegularDisk(f_, *disk, fmt);
    if (result) {
        util::cout << util::fmt("Wrote %u cyl%s, %u head%s, %2u sector%s, %4u bytes/sector = %u bytes%s\n",
            fmt.cyls, (fmt.cyls == 1) ? "" : "s",
            fmt.heads, (fmt.heads == 1) ? "" : "s",
            fmt.sectors, (fmt.sectors == 1) ? "" : "s",
            fmt.sector_size(), fmt.disk_size(),
            bpb_modified ? " and boot sector with reconstructed BPB" : "");
    }
    return result;
}
