// StFat12FileSystem (including BIOS Parameter Block), for Atari ST disks.

#include "filesystems/StFat12FileSystem.h"
#include "Disk.h"
#include "HDD.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <string>

//https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
const std::string StFat12FileSystem::ST_YET_LEGAL_NAME_CHARACTERS{R"("+,;<=>[]|)"};

/*static*/ const char* StFat12FileSystem::FileSystemName = "STFAT12";

/*static*/ std::shared_ptr<StFat12FileSystem> StFat12FileSystem::ConstructByApprovingDisk(Disk& disk_to_approve)
{
    auto stFat12FileSystem = std::make_shared<StFat12FileSystem>(disk_to_approve);
    return stFat12FileSystem->SetFormat() ? stFat12FileSystem : std::shared_ptr<StFat12FileSystem>();
}

/*static*/ const std::string StFat12FileSystem::Name()
{
    return FileSystemName;
}

bool StFat12FileSystem::IsBootSectorBootable(const Data& bootSectorData) const
{
    // https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#BSST_OFS_000h
    // Although the Boot Signature is misinterpreted on this page, probably this
    // statement is correct: "If the logical sector size is larger than 512 bytes,
    // the remainder is not included in the checksum and is typically zero-filled."
    const auto be16_sum = [](auto a, auto b) -> uint16_t { return a + util::htobe(b); };
    const auto checksumming_size = std::min(bootSectorData.size(), SECTOR_SIZE);
    const auto checksum = std::accumulate(
        reinterpret_cast<const uint16_t*>(bootSectorData.data()),
        reinterpret_cast<const uint16_t*>(bootSectorData.data() + checksumming_size),
        uint16_t{ 0 }, be16_sum
    );

    // Accept a valid boot checksum.
    return checksum == ST_BOOT_CHECKSUM;
}

bool StFat12FileSystem::SetFormatByBPB(const BIOS_PARAMETER_BLOCK& bootSectorBPB) /*override*/
{
    // NOTE Here could limit FAT12 version to 3.0 which the ST uses.
    if (format.IsNone())
        format = {RegularFormat::AtariST};
    return Fat12FileSystem::SetFormatByBPB(bootSectorBPB);
}

bool StFat12FileSystem::IsShortNameCharValid(const uint8_t character, const int pos, bool allowLowerCase/* = false*/) const /*override*/
{
    allowLowerCase = false; // ST does not use lower case letters although it can handle those.
    if (Fat12FileSystem::IsShortNameCharValid(character, pos, allowLowerCase))
        return true;
    return ST_YET_LEGAL_NAME_CHARACTERS.find(static_cast<char>(character)) != std::string::npos;
}

std::string StFat12FileSystem::GetName() const /*override*/
{
    return StFat12FileSystem::Name();
};
