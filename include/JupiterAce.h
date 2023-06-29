// Jupiter Ace helper functions

#include "Disk.h"
#include "Sector.h"

int GetDeepThoughtDataOffset(const Data& data);
std::string GetDeepThoughtData(const Data& data);
bool IsDeepThoughtSector(const Sector& sector, int& data_offset);
const Sector* IsDeepThoughtDisk(Disk& disk);
bool IsValidDeepThoughtData(const Data& data);
