#pragma once

// StFat12FileSystem (including BIOS Parameter Block), for Atari ST disks.

#include "Fat12FileSystem.h"

class StFat12FileSystem : public Fat12FileSystem
{
public:
    using Fat12FileSystem::Fat12FileSystem;

    static std::shared_ptr<StFat12FileSystem> ConstructByApprovingDisk(Disk& disk_to_approve); // For wrapper.
    static const std::string Name(); // For wrapper.

    bool IsBootSectorBootable(const Data& bootSectorData) const;

    bool SetFormatByBPB(const BIOS_PARAMETER_BLOCK& bootSectorBPB) override;
    bool IsShortNameCharValid(const uint8_t character, const int pos, bool allowLowerCase = false) const override;
    std::string GetName() const override;

    static const char* FileSystemName;

private:
    static constexpr uint16_t ST_BOOT_CHECKSUM = 0x1234;
    static const std::string ST_YET_LEGAL_NAME_CHARACTERS;
};
