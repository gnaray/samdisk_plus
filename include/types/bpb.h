#pragma once

// BIOS Parameter Block (part of FAT filesystem), for MS-DOS and compatible disks

#include "Disk.h"
#include "MemFile.h"

#include <memory>

extern const std::string STRECOVER_MISS_OR_BAD;

bool ReadBPB(MemFile& file, std::shared_ptr<Disk>& disk);
void ConvertStRecoverMissOrBadSectors(std::shared_ptr<Disk>& disk);
