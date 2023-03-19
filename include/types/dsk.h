#pragma once

#include "Disk.h"
#include "MemFile.h"
#include <stdio.h>

bool ReadDSK(MemFile& file, std::shared_ptr<Disk>& disk, int version);
bool WriteDSK(FILE* f_, std::shared_ptr<Disk>& disk, int version);
