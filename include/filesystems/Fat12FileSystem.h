#pragma once

// Fat12FileSystem (including BIOS Parameter Block), for MS-DOS and compatible disks.

#include "FileSystem.h"
#include "Format.h"
#include "Disk.h"
#include <cstring>

//Since DOS 3.2?
constexpr uint16_t BIOS_PARAMETER_BLOCK_SIGNATURE_LE = 0xaa55; // Little endian on disk.

struct BIOS_PARAMETER_BLOCK
{
    uint8_t abJump[3];              // usually x86 jump (0xeb, ?, 0x90 or 0xe9, ?, ?)
    uint8_t bOemName[8];            // OEM string

    uint8_t abBytesPerSec[2];       // bytes per sector
    uint8_t bSecPerClust;           // sectors per cluster
    uint8_t abResSectors[2];        // number of reserved sectors
    uint8_t bFATs;                  // number of FATs
    uint8_t abRootDirEnts[2];       // number of root directory entries
    uint8_t abSectors[2];           // total number of sectors
    uint8_t bMedia;                 // media descriptor
    uint8_t abFATSecs[2];           // number of sectors per FAT
    // Since DOS 3.0:
    uint8_t abSecPerTrack[2];       // sectors per track
    uint8_t abHeads[2];             // number of heads
    uint8_t abHiddenSecs[4];        // number of hidden sectors (it was [2] until DOS 3.31)
    // Since DOS 3.2:
    uint8_t abLargeSecs[4];         // number of large sectors (it was [2] until DOS 3.31)
    // extended fields below
    //Since DOS 7.1: // Yes, this block is inserted here moving the DOS 3.4 block further.
    uint8_t abLargeSectorsPerFat[4];
    uint8_t abFlags[2];
    uint8_t abFsVersion[2];
    uint8_t abRootDirFirstCluster[4];
    uint8_t abFsInfoSector[2];
    uint8_t BackupBootSector[2];
    uint8_t abReserved[12];
    //Since DOS 3.4:
    uint8_t bPhysicalDriveNumber;
    uint8_t bFlags;
    uint8_t bExtendedBootSignature; // DOS 3.4 and DOS 7.1 short FAT32: 0x28 aka 4.0, DOS 4.0 and DOS 7.1 full FAT32: 0x29 aka 4.1
    uint8_t abVolumeSerialNumber[4];
    //Since DOS 4.0 and DOS 7.1 full FAT32:
    uint8_t abVolumeLabel[11];
    uint8_t abFileSystemType[8];

    // ...
    // uint8_t at510[2] : BIOS_PARAMETER_BLOCK_SIGNATURE_LE (0x55,0xAA) means valid boot sector.

    friend bool operator==(const BIOS_PARAMETER_BLOCK& lhs, const BIOS_PARAMETER_BLOCK& rhs)
    {
        return std::memcmp(&lhs, &rhs, sizeof(BIOS_PARAMETER_BLOCK)) == 0;
    }

};

bool operator!=(const BIOS_PARAMETER_BLOCK& lhs, const BIOS_PARAMETER_BLOCK& rhs);

//////////////////////////////////////////////////////////////////////////////

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

constexpr int DIR_ENTRY_ATTR_READ_ONLY = 0x01;
constexpr int DIR_ENTRY_ATTR_HIDDEN = 0x02;
constexpr int DIR_ENTRY_ATTR_SYSTEM = 0x04;
constexpr int DIR_ENTRY_ATTR_VOLUME_ID = 0x08;
constexpr int DIR_ENTRY_ATTR_DIRECTORY = 0x10;
constexpr int DIR_ENTRY_ATTR_ARCHIVE = 0x20;
constexpr int DIR_ENTRY_ATTR_LONG_NAME = DIR_ENTRY_ATTR_READ_ONLY | DIR_ENTRY_ATTR_HIDDEN | DIR_ENTRY_ATTR_SYSTEM | DIR_ENTRY_ATTR_VOLUME_ID;

//////////////////////////////////////////////////////////////////////////////

// https://www.win.tue.nl/~aeb/linux/fs/fat/fat-1.html
class Fat12FileSystem : public FileSystem
{
public:
    Fat12FileSystem(Disk& disk_);
    Fat12FileSystem(Disk& disk_, const Format& format_);
    Fat12FileSystem(const Fat12FileSystem&) = delete;
    Fat12FileSystem& operator= (const Fat12FileSystem&) = delete;

    static std::shared_ptr<Fat12FileSystem> ConstructByApprovingDisk(Disk& disk_to_approve); // For FileSystemWrapper.
    static std::string Name(); // For FileSystemWrapper.

    bool IsBootSectorSigned(const Data& bootSectorData) const;
    bool IsBootSectorBootable(const Data& bootSectorData) const;
    bool SetFormat();
    bool SetFormatByBootSector(const Sector& bootSector);
    bool SetFormatByBootSectorData(const Data& bootSectorData);
    virtual bool SetFormatByBPB(const BIOS_PARAMETER_BLOCK& bootSectorBPB);

    time_t DateTime(const uint16_t date, const uint16_t time, const time_t& dateMax = {}) const;
    std::string DateTimeString(const time_t& dateTime) const;
    std::string DateTimeString(const uint16_t date, const uint16_t time, const time_t& dateMax = {}) const;
    const Sector* GetBootSector();
    Header LogicalSectorIndexToPhysical(int logicalSectorIndex) const;
    const Sector* GetLogicalSector(int sector_index, bool ignoreSize = false);
    int DetermineSectorsPerCluster() const;
    bool IsEofFatIndex(int fat_index) const;
    bool IsBadFatIndex(int fat_index) const;
    bool IsNextFatIndex(int fat_index) const;
    bool IsUsedFatIndex(int fat_index) const;
    int ClusterIndexToLogicalSectorIndex(const int cluster) const;
    int GetClusterSup() const;
    bool HasFatSectorNormalDataAt(const int fatInstance, const int offset) const;
    int GetClusterNext(const int cluster, const int clusterSup) const;
    int GetFileClusterAmount(int start_cluster) const;
    virtual bool IsShortNameCharValid(const uint8_t character, const int pos, bool allowLowerCase = false) const;
    bool IsValidShortName(const std::string& dir_entry_name) const;
    int AnalyseDirEntries();
    int MaxFatSectorsBeforeAnalysingFat() const;
    void ReadFATSectors(const int sectorsPerFAT, const int sectorSize, VectorX<const Sector*>* cachedLogicalSectors = nullptr);
    // Examining sector distance of FAT copies and finding the best distance which equals to fat sectors.
    int AnalyseFatSectors();
    bool ReconstructBpb();
    bool EnsureBootSector();
    void ReadBpbFromDisk();
    void WriteBpbToDisk();

    std::string NameWithExt3(const msdos_dir_entry& dir_entry, bool accept_deleted = false, bool* p_is_name_valid = nullptr) const;

    std::string GetName() const override;
    bool Dir() override;
    Format GetFormat() const override;
    void SetFormat(const Format& format_) override;
    bool IsSameNamed(const FileSystem &fileSystem) const override;
    bool IsSameNamedWithSameCylHeadSectorsSize(const FileSystem& fileSystem) const override;

    Disk& disk;
    Format format{};
    BIOS_PARAMETER_BLOCK bpb{};
    Data fat1{};
    VectorX<bool> fat1SectorHasNormalData{};
    Data fat2{};
    VectorX<bool> fat2SectorHasNormalData{};
    int new_fat_sectors = 0;
    int new_root_dir_entries = 0;
    int sectors_per_cluster_by_root_files = 0;

    static const char* FileSystemName;

private:
    static constexpr uint8_t DIR_ENTRY_DELETED_FLAG = 0xe5;
    static constexpr auto DATE_START_YEAR = 1980;
    static const time_t DATE_MAX;
    static const std::string FORMAT_STRING;
    static const std::string DOS_ILLEGAL_NAME_CHARACTERS;
    static const CylHead BOOT_SECTOR_CYLHEAD;
};
