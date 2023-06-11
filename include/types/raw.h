#pragma once

#include "Disk.h"
#include "Format.h"

Format CheckBeforeWriteRAW(std::shared_ptr<Disk>& disk);
