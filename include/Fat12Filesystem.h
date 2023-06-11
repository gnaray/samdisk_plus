#pragma once

#include "PlatformConfig.h"
#include "types/bpb.h"
#include "Format.h"

struct msdos_dir_entry
{
    uint8_t  name[11];       /* name and extension */
    uint8_t  attr;           /* attribute bits */
    uint8_t  lcase;          /* Case for base and extension */
    uint8_t  ctime_cs;       /* Creation time, centiseconds (0-199) */
    uint8_t  ctime[2];          /* Creation time */
    uint8_t  cdate[2];          /* Creation date */
    uint8_t  adate[2];          /* Last access date */
    uint8_t  starthi[2];        /* High 16 bits of cluster in FAT32 */
    uint8_t  time[2], date[2], start[2];/* time, date and first cluster */
    uint8_t  size[4];           /* file size (in bytes) */
};

// https://www.win.tue.nl/~aeb/linux/fs/fat/fat-1.html
class Fat12Filesystem
{
public:
    Fat12Filesystem(const Format& fmt, Disk& disk);

    bool GetLogicalSector(int sector_index, const Sector*& found_sector);
    bool PrepareBootSector();
    int DetermineSectorsPerCluster();
    bool IsEofFatIndex(int fat_index);
    bool IsNextFatIndex(int fat_index);
    bool IsUsedFatIndex(int fat_index);
    int GetFileClusterAmount(int start_cluster);
    int AnalyseDirEntries();
    // Examining sector distance of FAT copies and finding the best distance which equals to fat sectors.
    int AnalyseFatSectors();
    void ReconstructBpb();

    const Format& fmt;
    Disk& disk;
    const Sector* boot_sector;
    Data new_boot_sector_data;
    BIOS_PARAMETER_BLOCK& bpb;
    Data fat1;
    Data fat2;
    int new_fat_sectors = 0;
    int new_root_dir_entries = 0;
    int sectors_per_cluster_by_root_files = 0;
};
