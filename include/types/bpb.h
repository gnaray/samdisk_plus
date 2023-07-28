#pragma once

// BIOS Parameter Block (part of FAT filesystem), for MS-DOS and compatible disks

#include "Disk.h"
#include "MemFile.h"

#include <memory>

bool ReadBPB(MemFile& file, std::shared_ptr<Disk>& disk);
