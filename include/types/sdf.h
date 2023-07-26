#pragma once

#include "Disk.h"
#include "MemFile.h"

bool UnwrapSDF(std::shared_ptr<Disk>& src_disk, std::shared_ptr<Disk>& disk);

bool ReadSDF(MemFile& file, std::shared_ptr<Disk>& disk);
