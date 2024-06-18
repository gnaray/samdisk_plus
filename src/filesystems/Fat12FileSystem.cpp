// Fat12FileSystem (including BIOS Parameter Block), for MS-DOS and compatible disks.

#ifdef _WIN32
#include "PlatformConfig.h" // For disabling localtime deprecation.
#endif
#include "filesystems/Fat12FileSystem.h"
#include "Util.h"
#include "DiskUtil.h"
#include "Disk.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <string>

const time_t Fat12FileSystem::DATE_MAX = std::time(nullptr);
const std::string Fat12FileSystem::FORMAT_STRING{"%Y-%m-%d %H:%M:%S"};
const std::string Fat12FileSystem::DOS_ILLEGAL_NAME_CHARACTERS{R"("*+,./:;<=>?[\]|)"};
const CylHead Fat12FileSystem::BOOT_SECTOR_CYLHEAD{0, 0};

bool operator!=(const BIOS_PARAMETER_BLOCK& lhs, const BIOS_PARAMETER_BLOCK& rhs)
{
    return !(lhs == rhs);
}

//////////////////////////////////////////////////////////////////////////////

/*static*/ const char* Fat12FileSystem::FileSystemName = "FAT12";

Fat12FileSystem::Fat12FileSystem(Disk& disk)
    : Fat12FileSystem(disk, Format{})
{
}

Fat12FileSystem::Fat12FileSystem(Disk& disk, const Format& format)
    : disk(disk), format(format)
{
}

/*static*/ std::shared_ptr<Fat12FileSystem> Fat12FileSystem::ConstructByApprovingDisk(Disk& disk_to_approve)
{
    auto fat12FileSystem = std::make_shared<Fat12FileSystem>(disk_to_approve);
    return fat12FileSystem->SetFormat() ? fat12FileSystem : std::shared_ptr<Fat12FileSystem>();
}

/*static*/ std::string Fat12FileSystem::Name()
{
    static std::string FileSystemName_string{FileSystemName};
    return FileSystemName_string;
}

bool Fat12FileSystem::IsBootSectorSigned(const Data& bootSectorData) const
{
    //https://learn.microsoft.com/en-us/azure/rtos/filex/chapter3
    // Clearly states: Signature 0x55AA: The signature field is a data pattern
    // used to identify the boot record. If this field is not present,
    // the boot record is not valid.
    //https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
    // The header.size must be >= 512 because the FAT alias BPB signature must be at
    // 510, no matter how large is the boot sector.
    return bootSectorData.size() >= 512 && util::le_value<2>(&bootSectorData[512 - 2]) == BIOS_PARAMETER_BLOCK_SIGNATURE_LE;
}

// TODO Tune for usage.
bool Fat12FileSystem::IsBootSectorBootable(const Data& bootSectorData) const
{
    const auto& bootSectorBPB = *reinterpret_cast<const BIOS_PARAMETER_BLOCK*>(bootSectorData.data());
    //https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/fatgen103.doc
    return (bootSectorBPB.abJump[0] == 0xeb && bootSectorBPB.abJump[2] == 0x90) || bootSectorBPB.abJump[0] == 0xe9;
}

bool Fat12FileSystem::SetFormat()
{
    const auto boot_sector = disk.find_ignoring_size(Header(0, 0, 1, 0));
    return boot_sector != nullptr && SetFormatByBootSector(*boot_sector);
}

bool Fat12FileSystem::SetFormatByBootSector(const Sector& bootSector)
{
    if (!bootSector.has_good_normaldata() || bootSector.header.sector != 1)
        return false;
    return SetFormatByBootSectorData(bootSector.data_best_copy());
}

bool Fat12FileSystem::SetFormatByBootSectorData(const Data& bootSectorData)
{
    const auto& bootSectorBPB = *reinterpret_cast<const BIOS_PARAMETER_BLOCK*>(bootSectorData.data());
    return SetFormatByBPB(bootSectorBPB)
        && util::le_value(bootSectorBPB.abBytesPerSec) == bootSectorData.size();
}

/*virtual*/ bool Fat12FileSystem::SetFormatByBPB(const BIOS_PARAMETER_BLOCK& bootSectorBPB)
{
    bpb = bootSectorBPB;
    if (format.IsNone())
        format = {RegularFormat::PC720};
    const int bpb_bytes_per_sec = util::le_value(bootSectorBPB.abBytesPerSec);
    const auto root_dir_ents = util::le_value(bootSectorBPB.abRootDirEnts);
    // Check for a sensible media byte amongst others.
    if (bootSectorBPB.bSecPerClust < 1 || util::le_value(bootSectorBPB.abResSectors) < 1
            || bootSectorBPB.bFATs < 1 || bootSectorBPB.bFATs > 2 || root_dir_ents == 0
            || (root_dir_ents & (bpb_bytes_per_sec / intsizeof(msdos_dir_entry) - 1)) != 0
            || (bootSectorBPB.bMedia < 0xf8 && bootSectorBPB.bMedia != 0xf0 && bootSectorBPB.bMedia != 0x0) // media = 0 is not standard but happens.
            || util::le_value(bootSectorBPB.abFATSecs) < 1)
        return false;

    format.base = 1;
    format.sectors = util::le_value(bootSectorBPB.abSecPerTrack);
    format.heads = util::le_value(bootSectorBPB.abHeads);
    auto total_sectors = util::le_value(bootSectorBPB.abSectors);
    format.cyls = (format.sectors > 0 && format.heads > 0) ? (total_sectors / (format.sectors * format.heads)) : 0;
    format.size = SizeToCode(bpb_bytes_per_sec);

    if (!format.TryValidate())
        return false;
    if (format.cyls == 0)
        Message(msgWarning, "%s BPB found but cyls is unavailable because sectors per track (%d) or heads (%d) is 0",
                GetName().c_str(), format.sectors, format.heads);
    else if (format.total_sectors() != total_sectors)
        Message(msgWarning, "%s BPB found but total sectors (%hu) does not perfectly matches the sector amount (%d) by cyls (%d), heads (%d), sectors (%d)",
                GetName().c_str(), total_sectors, format.total_sectors(), format.cyls, format.heads, format.sectors);
    format.datarate = (format.track_size() < 6000) ? DataRate::_250K : ((format.track_size() < 12000) ? DataRate::_500K : DataRate::_1M);
    format.gap3 = 0; // auto, based on sector count
    if (bootSectorBPB.bMedia == 0x0)
        MessageCPP(msgWarning, "BPB's Media byte is invalid (0) but accepting it since BPB seems to be valid anyway");
    return true;
}

const Sector* Fat12FileSystem::GetBootSector()
{
    // Boot sector is the first logical sector (index=0).
    return GetLogicalSector(0, true);
}

Header Fat12FileSystem::LogicalSectorIndexToPhysical(int logicalSectorIndex) const
{
    if (format.sectors <= 0 || format.heads <= 0 || format.cyls <= 0)
        throw util::exception("selected disk region is empty");
    if (logicalSectorIndex < 0 || logicalSectorIndex > format.disk_size() / format.sector_size())
        throw util::exception("invalid logical sector index ", logicalSectorIndex);
    const auto sector_id = logicalSectorIndex % format.sectors + 1;
    logicalSectorIndex /= format.sectors;
    const auto head = logicalSectorIndex % format.heads;
    const auto cyl = logicalSectorIndex / format.heads;
    return Header(cyl, head, sector_id, format.size);
}

const Sector* Fat12FileSystem::GetLogicalSector(int sector_index, bool ignoreSize/*= false*/)
{
    const auto header = LogicalSectorIndexToPhysical(sector_index);
    // Finding sector this way because it might be missing.
    if (!ignoreSize)
        return disk.find(header);
    else
        return disk.find_ignoring_size(header);

}

time_t Fat12FileSystem::DateTime(const uint16_t date, const uint16_t time, const time_t& dateMax/* = {}*/) const
{
    tm result;
    result.tm_year = (date >> 9) + DATE_START_YEAR - 1900;
    result.tm_mon = ((date >> 5) & 0xf) - 1;
    result.tm_mday = date & 0x1f;
    result.tm_hour = time >> 11;
    result.tm_min = (time >> 5) & 0x3f;
    result.tm_sec = (time & 0x1f) * 2;
    result.tm_isdst = -1; // Using auto DST.
    if (result.tm_hour < 24 && result.tm_min < 60 && result.tm_sec < 60)
    {
        auto result_t = std::mktime(&result); // Determines tm_gmtoff as well.
        if (result_t != -1 && (dateMax == 0 || difftime(dateMax, result_t) >= 0))
            return result_t;
    }
    return -1;
}

std::string Fat12FileSystem::DateTimeString(const time_t& dateTime) const
{
    std::ostringstream os;
    if (dateTime != -1)
        os << std::put_time(std::localtime(&dateTime), FORMAT_STRING.c_str());
    else
        os << "INVALID  INVALID";
    return os.str();
}

std::string Fat12FileSystem::DateTimeString(const uint16_t date, const uint16_t time, const time_t& dateMax/* = {}*/) const
{
    const auto dateTime = DateTime(date, time, dateMax);
    return DateTimeString(dateTime);
}

int Fat12FileSystem::DetermineSectorsPerCluster() const
{
    auto fat1_sector_0 = util::le_value(bpb.abResSectors);
    auto fat_sectors = util::le_value(bpb.abFATSecs);
    // Starting with worst case: abRootDirEnts = 0x10 ...
    auto max_file_data_sectors = format.total_sectors() - fat1_sector_0 - bpb.bFATs * fat_sectors - 1;
    auto max_cluster_index_by_fat = fat_sectors * format.sector_size() / 3 * 2; // TODO Assuming FAT12
    // The min 1 limit is based on that "max cluster index by fat" * "sectors per cluster" must be >= "max file data sectors".
    auto min_1_sectors_per_cluster = static_cast<int>(std::ceil(static_cast<double>(max_file_data_sectors) / (max_cluster_index_by_fat - 2)));
    auto sectors_per_cluster = std::max(min_1_sectors_per_cluster, sectors_per_cluster_by_root_files);
    // Round up to next power of 2.
    auto sectors_per_cluster_power2 = 1 << static_cast<int>(std::ceil(log(sectors_per_cluster) / log(2)));
    if (sectors_per_cluster_power2 != 1 && sectors_per_cluster_power2 != 2)
        MessageCPP(msgWarning, "Found not normal ", sectors_per_cluster_power2, " sectors per cluster value");
    if (sectors_per_cluster_power2 > 255)
        throw util::exception("invalid sectors per cluster value ", sectors_per_cluster_power2);
    return sectors_per_cluster_power2;
}

bool Fat12FileSystem::IsEofFatIndex(int fat_index) const
{
    return fat_index >= 0xff8 && fat_index <= 0xfff;
}

bool Fat12FileSystem::IsBadFatIndex(int fat_index) const
{
    return fat_index >= 0xff0 && fat_index <= 0xff7;
}

bool Fat12FileSystem::IsNextFatIndex(int fat_index) const
{
    return fat_index >= 2 && fat_index <= 0xfef;
}

bool Fat12FileSystem::IsUsedFatIndex(int fat_index) const
{
    return IsEofFatIndex(fat_index) || IsNextFatIndex(fat_index);
}

int Fat12FileSystem::ClusterIndexToLogicalSectorIndex(const int cluster) const
{
    const auto fat1_sector_0 = util::le_value(bpb.abResSectors);
    const auto fat_sectors = util::le_value(bpb.abFATSecs);
    const auto dir_sector_0 = fat1_sector_0 + bpb.bFATs * fat_sectors;
    const auto root_dir_ents = util::le_value(bpb.abRootDirEnts);
    const auto msdos_dir_entry_size = intsizeof(msdos_dir_entry);
    const auto dir_sectors = root_dir_ents * msdos_dir_entry_size / format.sector_size();
    const auto sectors_per_cluster = bpb.bSecPerClust;
    return (cluster - 2) * sectors_per_cluster + dir_sector_0 + dir_sectors;
}

// Calculates based on bpb.abFATSecs and bpb.abBytesPerSec.
int Fat12FileSystem::GetClusterSup() const
{
    const auto fat_sectors = util::le_value(bpb.abFATSecs);
    const int bpb_bytes_per_sec = util::le_value(bpb.abBytesPerSec);
    const auto fat_byte_length = fat_sectors * bpb_bytes_per_sec;
    const auto cluster_sup = fat_byte_length * 2 / 3;
    return cluster_sup;
}

// The fatInstance can be 0 (fat1) or 1 (fat2).
bool Fat12FileSystem::HasFatSectorNormalDataAt(const int fatInstance, const int offset) const
{
    assert(fatInstance >= 0 && fatInstance <= 1);
    const auto& fatSectorHasNormalData = fatInstance == 0 ? fat1SectorHasNormalData : fat2SectorHasNormalData;
    const auto fatSectorIndex = offset / format.sector_size();
    return fatSectorIndex < fatSectorHasNormalData.size() && fatSectorHasNormalData[fatSectorIndex];
}

// Fat1 and Fat2 must be already read.
// Return cluster's next cluster, or -1 if cluster is invalid.
int Fat12FileSystem::GetClusterNext(const int cluster, const int clusterSup) const
{
    const auto fat1_data = fat1.data();
    const auto fat2_data = fat2.data();
    if (clusterSup <= 0)
    {
        MessageCPP(msgWarning, "Number of clusters is unknown, dependent operations are not possible");
        return -1;
    }
    if (cluster >= clusterSup)
    {
        MessageCPP(msgWarning, "Found out of range FAT cluster index ", cluster, ", it must be < ", clusterSup);
        return -1;
    }
    auto cluster_fat_bytes = (cluster & ~0x1) * 3 / 2;
    int fat1_next_index;
    int fat2_next_index;
    const auto clusterLocationEven = (cluster & 1) == 0;
    const auto clusterFatBytesFirst = clusterLocationEven ? cluster_fat_bytes : cluster_fat_bytes + 1;
    const auto fat1NextIndexAvailable = HasFatSectorNormalDataAt(0, clusterFatBytesFirst) && HasFatSectorNormalDataAt(0, clusterFatBytesFirst + 1);
    const auto fat2NextIndexAvailable = HasFatSectorNormalDataAt(1, clusterFatBytesFirst) && HasFatSectorNormalDataAt(1, clusterFatBytesFirst + 1);
    if (clusterLocationEven)
    {
        fat1_next_index = ((fat1_data[clusterFatBytesFirst + 1] & 0xf) << 8) + fat1_data[clusterFatBytesFirst];
        fat2_next_index = ((fat2_data[clusterFatBytesFirst + 1] & 0xf) << 8) + fat2_data[clusterFatBytesFirst];
    }
    else {
        fat1_next_index = ((fat1_data[clusterFatBytesFirst] & 0xf0) >> 4) + (fat1_data[clusterFatBytesFirst + 1] << 4);
        fat2_next_index = ((fat2_data[clusterFatBytesFirst] & 0xf0) >> 4) + (fat2_data[clusterFatBytesFirst + 1] << 4);
    }
    // Favouring available FAT, then next index, then first FAT.
    auto fat_next_index = fat1_next_index;
    if (!fat1NextIndexAvailable)
    {
        if (!fat2NextIndexAvailable)
            return 0xfff; // Next index is not available, return EOF (although not precise).
        fat_next_index = fat2_next_index;
    }
    else if (fat2NextIndexAvailable)
    {
        if (!IsNextFatIndex(fat1_next_index) && IsNextFatIndex(fat2_next_index))
            fat_next_index = fat2_next_index;
        if (!IsUsedFatIndex(fat1_next_index) && IsUsedFatIndex(fat2_next_index))
            fat_next_index = fat2_next_index;
    }
    return fat_next_index;
}

// Return number of clusters in cluster chain started by start_cluster.
int Fat12FileSystem::GetFileClusterAmount(int start_cluster) const
{
    // Find length of cluster chain starting at start_cluster.
    const auto fat_byte_length = new_fat_sectors * format.sector_size();
    const auto cluster_sup = fat_byte_length * 2 / 3;
    int cluster_amount = 0;
    int cluster_i = start_cluster;
    do
    {
        const auto fat_next_index = GetClusterNext(cluster_i, cluster_sup);
        if (fat_next_index < 0)
            break;
        if (!IsUsedFatIndex(fat_next_index)) // The cluster chain reached an unused cluster, it should not happen.
            break;
        cluster_amount++;
        if (IsEofFatIndex(fat_next_index))
            break;
        cluster_i = fat_next_index;
    } while (true);
    return cluster_amount;
}

/*virtual*/ bool Fat12FileSystem::IsShortNameCharValid(const uint8_t character, const int pos, bool allowLowerCase/* = false*/) const
{
    if (pos == 0 && character == ' ')
        return false;
    if (character < ' ')
        return false;
    if (!allowLowerCase && character >= 'a' && character <= 'z')
        return false;
    if (DOS_ILLEGAL_NAME_CHARACTERS.find(static_cast<char>(character)) != std::string::npos)
        return false;
    if (character == 127 || character == DIR_ENTRY_DELETED_FLAG)
        return false;
    return true;
}

bool Fat12FileSystem::IsValidShortName(const std::string& dir_entry_name) const
{
    // Reject filenames containing MSDOS illegal characters.
    const auto iSup = dir_entry_name.size();
    for (std::string::size_type i = 0; i < iSup; i++)
    {
        const auto ch = static_cast<uint8_t>(dir_entry_name[i]);
        if (!IsShortNameCharValid(ch, static_cast<int>(i)))
            return false;
    }
    return true;
}

int Fat12FileSystem::AnalyseDirEntries()
{
    const auto fat1_sector_0 = util::le_value(bpb.abResSectors);
    const auto fat_sectors = util::le_value(bpb.abFATSecs);
    const auto dir_sector_0 = fat1_sector_0 + bpb.bFATs * fat_sectors;
    const int normal_dir_entries_1 = 0x70;
    const int normal_dir_entries_2 = 0xe0;
    const auto msdos_dir_entry_size = intsizeof(msdos_dir_entry);
    const int normal_dir_sectors_1 = normal_dir_entries_1 * msdos_dir_entry_size / format.sector_size();
    const int normal_dir_sectors_2 = normal_dir_entries_2 * msdos_dir_entry_size / format.sector_size();
    const int normal_dir_entries = format.sectors <= 11 ? normal_dir_entries_1 : normal_dir_entries_2;
    const int max_dir_entries = 0x200;
    const auto max_dir_sectors = max_dir_entries * msdos_dir_entry_size / format.sector_size();
    int sum_sectors_per_cluster = 0;
    int sectors_per_cluster_participants = 0;
    bool looking_for_0 = true;
    bool found_not_0_after_0 = false;
//    int missing_last_sector_count = 0;
    int first_0_dir_sector = 0;
    int dir_sector_i;
    for (dir_sector_i = dir_sector_0; dir_sector_i < dir_sector_0 + max_dir_sectors; dir_sector_i++)
    {
        auto dir_sector = GetLogicalSector(dir_sector_i);
        if (dir_sector != nullptr && dir_sector->has_normaldata())
        {
            const auto dir_sector_min_size = std::min(dir_sector->data_size(), format.sector_size());
            const auto dir_sector_data = dir_sector->data_best_copy();
            for (int i = 0; i < dir_sector_min_size; i += msdos_dir_entry_size)
            {
                auto& dir_entry = *reinterpret_cast<const msdos_dir_entry*>(&dir_sector_data[i]);
                if (looking_for_0)
                {
                    if (dir_entry.name[0] == 0)
                    {
                        looking_for_0 = false;
                        first_0_dir_sector = dir_sector_i + (i > 0 ? 1 : 0);
                    }
                    // Entry must not be deleted, must not be label, must not be directory and must not be long file name part.
                    // At this point can not read directories because sector per cluster value is unknown.
                    else if (dir_entry.name[0] != DIR_ENTRY_DELETED_FLAG && ((dir_entry.attr & 0x18) == 0)
                        && dir_entry.name[0] > ' ') {
                        auto cluster_amount = GetFileClusterAmount(util::le_value(dir_entry.start));
                        if (cluster_amount > 1) {
                            sum_sectors_per_cluster += static_cast<int>(std::ceil(static_cast<double>(util::le_value(dir_entry.size)) / format.sector_size() / cluster_amount));
                            sectors_per_cluster_participants++;
                        }
                    }
                }
                else
                {
                    if (dir_entry.name[0] != 0)
                    {
                        found_not_0_after_0 = true;
                        break;
                    }
                }
            }
            if (found_not_0_after_0)
                break;
//            missing_last_sector_count = 0;
        }
//        else
//            missing_last_sector_count++;
    }
    if (sectors_per_cluster_participants > 0)
        sectors_per_cluster_by_root_files = static_cast<int>(std::round(static_cast<double>(sum_sectors_per_cluster) / sectors_per_cluster_participants));
    const auto max_found_dir_sectors = std::max(1, dir_sector_i - dir_sector_0);
//    const int min_found_dir_sectors = max_found_dir_sectors - missing_last_sector_count;
    const int min_found_dir_sectors = std::max(1, first_0_dir_sector - dir_sector_0);
    int found_dir_sectors = max_found_dir_sectors;
    // If normal dir sectors value is in [min, max] then prefer that one.
    if (min_found_dir_sectors <= normal_dir_sectors_1 && max_found_dir_sectors >= normal_dir_sectors_1)
        found_dir_sectors = normal_dir_sectors_1;
    else if (min_found_dir_sectors <= normal_dir_sectors_2 && max_found_dir_sectors >= normal_dir_sectors_2)
        found_dir_sectors = normal_dir_sectors_2;
    auto found_dir_entries = found_dir_sectors * format.sector_size() / msdos_dir_entry_size;
    if (found_dir_entries != normal_dir_entries_1 && found_dir_entries != normal_dir_entries_2) {
        MessageCPP(msgWarning, "Found not normal ", found_dir_entries, " directory entries value, it should be ", normal_dir_entries, " normally");
    }
    return found_dir_entries;
}

int Fat12FileSystem::MaxFatSectorsBeforeAnalysingFat() const
{
    const auto fat1_sector_0_index = util::le_value(bpb.abResSectors);
    // Starting with worst case: abFATSecs = 1, abRootDirEnts = 0x10 ...
    const auto max_file_data_sectors = format.total_sectors() - fat1_sector_0_index - bpb.bFATs * 1 - 1;
    // ... bSecPerClust = 1.
    const auto max_cluster_index_by_fat = max_file_data_sectors / 1 + 2;
    return static_cast<int>(std::ceil(static_cast<double>(max_cluster_index_by_fat)
        * 3 / 2 / format.sector_size())); // TODO Assuming FAT12
}

void Fat12FileSystem::ReadFATSectors(const int sectorsPerFAT, const int sectorSize, VectorX<const Sector*>* cachedLogicalSectors/* = nullptr*/)
{
    const auto fat_byte_length = sectorsPerFAT * sectorSize;
    // Store the FAT sectors continuously in fat1, fat2 so those can be processed by FAT12 3 bytes.
    fat1.resize(fat_byte_length);
    fat1SectorHasNormalData.resize(sectorsPerFAT);
    fat2.resize(fat_byte_length);
    fat2SectorHasNormalData.resize(sectorsPerFAT);
    const auto fat1_sector_0_index = util::le_value(bpb.abResSectors);
    for (int fat_sector_i = 0; fat_sector_i < sectorsPerFAT; fat_sector_i++)
    {
        const auto fat1SectorLogicalI = fat_sector_i + fat1_sector_0_index;
        const auto fat1_sector = cachedLogicalSectors !=nullptr && fat1SectorLogicalI < cachedLogicalSectors->size()
            ? cachedLogicalSectors->operator[](fat1SectorLogicalI) : GetLogicalSector(fat1SectorLogicalI);
        fat1SectorHasNormalData[fat_sector_i] = fat1_sector && fat1_sector->has_normaldata();
        if (fat1SectorHasNormalData[fat_sector_i])
        {
            const auto common_size = std::min(fat1_sector->data_size(), sectorSize);
            auto d_first = fat1.begin() + fat_sector_i * sectorSize;
            std::copy_n(fat1_sector->data_best_copy().begin(), common_size, d_first);
        }
        const auto fat2SectorLogicalI = fat1SectorLogicalI + sectorsPerFAT;
        const auto fat2_sector = cachedLogicalSectors != nullptr && fat2SectorLogicalI < cachedLogicalSectors->size()
            ? cachedLogicalSectors->operator[](fat2SectorLogicalI) : GetLogicalSector(fat2SectorLogicalI);
        fat2SectorHasNormalData[fat_sector_i] = fat2_sector && fat2_sector->has_normaldata();
        if (fat2SectorHasNormalData[fat_sector_i])
        {
            const auto common_size = std::min(fat2_sector->data_size(), sectorSize);
            auto d_first = fat2.begin() + fat_sector_i * sectorSize;
            std::copy_n(fat2_sector->data_best_copy().begin(), common_size, d_first);
        }
    }
}

// Examining sector distance of FAT copies and finding the best distance which equals to fat sectors.
int Fat12FileSystem::AnalyseFatSectors()
{
    if (bpb.bFATs != 2)
        throw util::exception("amount of FAT copies not being 2 is unsupported in FAT12");
    const auto fat1_sector_0_index = util::le_value(bpb.abResSectors);
    const auto max_fat_sectors = MaxFatSectorsBeforeAnalysingFat();
    VectorX<double> fat_sector_match(max_fat_sectors + 1);
    VectorX<int> fat_sector_participants(max_fat_sectors + 1);
    VectorX<const Sector*> logical_sectors(fat1_sector_0_index + max_fat_sectors * 2);

    // Cache the sectors so algorithm is bit faster.
    for (int fat_sector_i = fat1_sector_0_index; fat_sector_i < fat1_sector_0_index + max_fat_sectors * 2; fat_sector_i++)
        logical_sectors[fat_sector_i] = GetLogicalSector(fat_sector_i);

    // Collect distance matches.
    for (int fat_sector_dist = 1; fat_sector_dist <= max_fat_sectors; fat_sector_dist++)
    {
        for (int fat_sector_i = fat1_sector_0_index; fat_sector_i < fat1_sector_0_index + fat_sector_dist; fat_sector_i++)
        {
            auto fat1_sector = logical_sectors[fat_sector_i];
            auto fat2_sector = logical_sectors[fat_sector_i + fat_sector_dist];
            if (fat1_sector && fat2_sector && fat1_sector->has_normaldata() && fat2_sector->has_normaldata())
            {
                auto common_size = std::min({ fat1_sector->data_size(), fat2_sector->data_size(), format.sector_size() });
                int sum = 0;
                int equal = 0;
                int difference = 0;
                auto fat1_data = fat1_sector->data_best_copy();
                auto fat2_data = fat2_sector->data_best_copy();
                for (int i = 0; i < common_size; ) {
                    auto fat1_data_i = fat1_data[i];
                    if (fat1_data_i == fat2_data[i])
                        equal++;
                    sum += fat1_data[i++];
                    difference += std::abs(fat1_data_i - static_cast<int>(std::round(sum / i)));
                }
                // equal / common_size is in [0, 1], difference / common_size is in [0, 128)
                fat_sector_match[fat_sector_dist] += static_cast<double>(equal)
                    * difference / 128 / common_size / common_size;
                fat_sector_participants[fat_sector_dist]++;
            }
        }
    }
    // Find the best distance.
    int best_fat_sector_dist = 0;
    double best_match_percent = 0;
    for (int fat_sector_dist = 1; fat_sector_dist <= max_fat_sectors; fat_sector_dist++)
    {
        if (fat_sector_participants[fat_sector_dist] == 0)
            continue;
        auto match_percent = 100 * fat_sector_match[fat_sector_dist] / fat_sector_participants[fat_sector_dist]
            * std::sqrt(fat_sector_dist); // Weighting with sqrt(dist) preferring match of more sectors (so max percent is above 100).
        if (match_percent > best_match_percent)
        {
            best_fat_sector_dist = fat_sector_dist;
            best_match_percent = match_percent;
        }
    }
    if (best_fat_sector_dist != 3 && best_fat_sector_dist != 5)
        MessageCPP(msgWarning, "Found not normal ", best_fat_sector_dist, " sectors per FAT value");

    ReadFATSectors(best_fat_sector_dist, format.sector_size(), &logical_sectors);
    return best_fat_sector_dist;
}

bool Fat12FileSystem::ReconstructBpb()
{
    const auto bpb_previous = bpb;
    util::store_le_value(format.total_sectors(), bpb.abSectors);
    util::store_le_value(format.sectors, bpb.abSecPerTrack);
    util::store_le_value(format.heads, bpb.abHeads);
    util::store_le_value(Sector::SizeCodeToLength(format.size), bpb.abBytesPerSec); // It is 512 by standard.
    util::store_le_value(1, bpb.abResSectors); // It is 1 by standard (the boot sector itself).
    bpb.bFATs = 2; // It is 2 by standard.
    // On Atari ST the disks had 2 heads, at least 9 sectors and at least 80 tracks, thus Media is usually 0xF9.
    bpb.bMedia = 0xF8 | (format.cyls <= 42 ? 0x4 : 0) | (format.sectors <= 8 ? 0x2 : 0) | (format.heads == 2 ? 0x1 : 0);

    auto fat_sectors = util::le_value(bpb.abFATSecs);
    auto sectors_per_cluster = bpb.bSecPerClust;
    const auto fat_sectors_max = MaxFatSectorsBeforeAnalysingFat();
    if (fat_sectors >= 1 && fat_sectors <= fat_sectors_max && sectors_per_cluster >= 1 && sectors_per_cluster <= 4)
        util::cout << "The number of sectors per FAT and sectors per cluster values seem to be valid.\n";
    new_fat_sectors = AnalyseFatSectors();
    // Varies a lot, depends mainly on disk size and amount of clusters.
    util::store_le_value(new_fat_sectors, bpb.abFATSecs);
    new_root_dir_entries = AnalyseDirEntries();
    util::store_le_value(new_root_dir_entries, bpb.abRootDirEnts); // It is 0x70 or 0xe0 normally.
    // It is usually 2 but sometimes 1 when user wants less loss per cluster but it requires bigger FAT.
    bpb.bSecPerClust = lossless_static_cast<uint8_t>(DetermineSectorsPerCluster());

    return bpb != bpb_previous;
}

bool Fat12FileSystem::EnsureBootSector()
{
    /* Assumed that the disk will be written by WriteRegularDisk which writes
       only sectors in sector range (base, sectors) and in head and track range.
       Thus the boot sector requires the following check.
     */
    if (format.range().cyl_begin != 0 || format.range().head_begin != 0) // Boot sector location is outside of disk region.
        throw util::exception("missing boot sector location is unsuitable for writing BPB of ", GetName());
    if (format.base != 1) // Boot sector location is outside of disk region.
        throw util::exception("missing boot sector location is unsuitable for writing BPB of ", GetName(), ", specifying base=1 might help");

    auto boot_sector = GetBootSector();
    if (boot_sector && boot_sector->has_data())
        return false;
    const auto bootSectorHeader = Header(BOOT_SECTOR_CYLHEAD, 1, format.size);
    Track track00 = disk.read_track(BOOT_SECTOR_CYLHEAD);
    if (boot_sector == nullptr)
    {
        if (format.datarate == DataRate::Unknown || format.encoding == Encoding::Unknown)
            throw util::exception("selected disk region has unknown datarate and encoding");
        track00.insert(0, Sector(format.datarate, format.encoding, bootSectorHeader));
    }
    auto& bootSectorNew = *track00.findIgnoringSize(bootSectorHeader); // Iterator should not fail.
    if (!bootSectorNew.has_data())
    {
        Data new_boot_sector_data(format.sector_size(), format.fill);
        std::copy(MISSING_SECTOR_SIGN.begin(), MISSING_SECTOR_SIGN.end(), new_boot_sector_data.begin()); // Signing sector with MISS.
        bootSectorNew.add(std::move(new_boot_sector_data), true); // Flagging sector as bad so it can be repaired in the future.
    }
    disk.write(BOOT_SECTOR_CYLHEAD, std::move(track00));
    return true;
}

void Fat12FileSystem::ReadBpbFromDisk()
{
    const auto boot_sector = GetBootSector();
    if (boot_sector == nullptr || !boot_sector->has_data())
        throw util::exception("missing boot sector");
    const auto& boot_sector_data = boot_sector->data_best_copy();
    const auto& bpbOnDisk = *reinterpret_cast<const BIOS_PARAMETER_BLOCK*>(boot_sector_data.data());
    bpb = bpbOnDisk;
}

void Fat12FileSystem::WriteBpbToDisk()
{
    const auto boot_sector = GetBootSector();
    if (boot_sector == nullptr || !boot_sector->has_data())
        throw util::exception("missing boot sector");
    const auto bootSectorHeader = Header(BOOT_SECTOR_CYLHEAD, 1, format.size);
    Track track00 = disk.read_track(BOOT_SECTOR_CYLHEAD);
    auto& bootSectorNew = track00.findIgnoringSize(bootSectorHeader)->data_best_copy(); // Iterator should not fail.
    auto& bpbOnDisk = *reinterpret_cast<BIOS_PARAMETER_BLOCK*>(bootSectorNew.data());
    bpbOnDisk = bpb;
    disk.write(BOOT_SECTOR_CYLHEAD, std::move(track00), true);
}

std::string Fat12FileSystem::NameWithExt3(const msdos_dir_entry& dir_entry, bool accept_deleted/* = false*/, bool* p_is_name_valid/* = nullptr*/) const
{
    std::string dirEntryName{dir_entry.name, dir_entry.name + sizeof(dir_entry.name)};
    const auto allowDeleted = accept_deleted && dir_entry.name[0] == DIR_ENTRY_DELETED_FLAG;
    bool is_name_valid_local;
    auto pIsNameValidLocal = p_is_name_valid != nullptr ? p_is_name_valid : &is_name_valid_local;
    *pIsNameValidLocal = IsValidShortName(dirEntryName.substr(allowDeleted ? 1 : 0));
    if (!*pIsNameValidLocal && p_is_name_valid == nullptr)
        return "**INVALID**"; // Intentionally 11 long (it should not be longer).
    if (allowDeleted)
        dirEntryName[0] = '?';
    constexpr auto ext_len = 3;
    const auto nameLen = dirEntryName.size() - ext_len;
    const auto ext3 = util::trim(dirEntryName.substr(nameLen));
    const auto name = util::trim(dirEntryName.substr(0, nameLen));
    return ext3.empty() ? name : name + '.' + ext3;
}

std::string Fat12FileSystem::GetName() const /*override*/
{
    return Fat12FileSystem::Name();
};

void coutTextWithValidationError(const std::string& text, const bool isTextValid, const colour lineColor = colour::none)
{
    if (!isTextValid)
        util::cout << '*' << colour::magenta;
    util::cout << util::fmt("%-12.12s", text.c_str());
    if (!isTextValid)
        util::cout << lineColor << '*';
    else
        util::cout << "  ";
}

bool Fat12FileSystem::Dir() /*override*/
{
    const auto fat1_sector_0 = util::le_value(bpb.abResSectors);
    const auto fat_sectors = util::le_value(bpb.abFATSecs);
    const auto dir_sector_0 = fat1_sector_0 + bpb.bFATs * fat_sectors;
    const auto root_dir_ents = util::le_value(bpb.abRootDirEnts);
    const auto msdos_dir_entry_size = intsizeof(msdos_dir_entry);
    const auto max_dir_sectors = root_dir_ents * msdos_dir_entry_size / format.sector_size();
    std::string volume_label;
    bool is_volume_label_valid = false;
    util::cout << "T  File Name        Clst     Size         Date     Time   Offset\n";
    for (auto dir_sector_i = dir_sector_0; dir_sector_i < dir_sector_0 + max_dir_sectors; dir_sector_i++)
    {
        auto dir_sector = GetLogicalSector(dir_sector_i);
        if (dir_sector == nullptr)
            util::cout << "Logical sector " << dir_sector_i << " is missing\n";
        else if (!dir_sector->has_normaldata())
            util::cout << "Logical sector " << dir_sector_i << " exists but its size is not normal\n";
        else
        {
            const auto dir_sector_min_size = std::min(dir_sector->data_size(), format.sector_size());
            const auto dir_sector_data = dir_sector->data_best_copy();
            for (int i = 0; i < dir_sector_min_size; i += msdos_dir_entry_size)
            {
                auto& dir_entry = *reinterpret_cast<const msdos_dir_entry*>(&dir_sector_data[i]);
                if (dir_entry.name[0] == 0)
                    goto noMoreDirEntries;
                if (dir_entry.attr == DIR_ENTRY_ATTR_LONG_NAME || dir_entry.name[0] <= ' ')
                    continue;
                if (dir_entry.attr & DIR_ENTRY_ATTR_VOLUME_ID)
                {   // TODO Can be a label deleted? Probably not.
                    volume_label = NameWithExt3(dir_entry, false, &is_volume_label_valid);
                    continue;
                }
                const auto dir_entry_deleted = dir_entry.name[0] == DIR_ENTRY_DELETED_FLAG;
                auto is_name_valid = false;
                const auto name = NameWithExt3(dir_entry, true, &is_name_valid);
                if (dir_entry_deleted)
                {
                    if (!is_name_valid || name[1] == 0) // Checking 1st character is redundant but safe.
                        continue;
                    // The entry is a deleted invalid entry.
                    const std::string dirEntry{ reinterpret_cast<const char*>(&dir_entry), reinterpret_cast<const char*>(&(&dir_entry)[1]) };
                    const auto pos = dirEntry.find_first_not_of(dirEntry[1], 2);
                    if (pos == std::string::npos || pos >= sizeof(dir_entry))
                        continue; // The entry contains a repeated character as in a sector filled with E5 or similar character.
                }
                const auto attr_readonly = (dir_entry.attr & DIR_ENTRY_ATTR_READ_ONLY) != 0;
                const auto attr_hidden = (dir_entry.attr & DIR_ENTRY_ATTR_HIDDEN) != 0;
                const auto attr_system = (dir_entry.attr & DIR_ENTRY_ATTR_SYSTEM) != 0;
                const auto dateTime = DateTime(util::le_value(dir_entry.date), util::le_value(dir_entry.time));
                const auto file_size = util::le_value(dir_entry.size);
                const auto fileSizeString = file_size > static_cast<uint32_t>(format.disk_size()) ? "INVALID" : util::fmt("%7u", file_size);
                auto row_colour = colour::none;
                // Show deleted entry in red, with first character of filename replaced by "?"
                if (dir_entry_deleted)
                    row_colour = colour::red;
                else if (attr_hidden)
                    row_colour = colour::cyan;
                const auto typeLetter = dir_entry.attr & DIR_ENTRY_ATTR_DIRECTORY ? 'D' : 'F';
                if (row_colour != colour::none)
                    util::cout << row_colour;
                util::cout << util::fmt("%c  ", typeLetter);
                coutTextWithValidationError(name, is_name_valid, row_colour);
                const auto dirEntryStart = util::le_value(dir_entry.start);
                util::cout << util::fmt("  %5hu  %s  ", dirEntryStart, fileSizeString.c_str());
                const auto dateTimeInFuture = difftime(DATE_MAX, dateTime) < 0;
                if (dateTimeInFuture)
                    util::cout << colour::yellow;
                util::cout << util::fmt("%20s", DateTimeString(dateTime).c_str());
                if (dateTimeInFuture)
                    util::cout << row_colour;
                util::cout << util::fmt("  %7u", ClusterIndexToLogicalSectorIndex(dirEntryStart) * format.sector_size());
                std::stringstream ss;
                bool writingStarted = false;
                if (attr_readonly) { if (writingStarted) ss << ", "; else writingStarted = true; ss << "Read-Only"; }
                if (attr_hidden) { if (writingStarted) ss << ", "; else writingStarted = true; ss << "Hidden"; }
                if (attr_system) { if (writingStarted) ss << ", "; else writingStarted = true; ss << "System"; }
                if (writingStarted)
                    util::cout << " (" << ss.str() << ")";

                if (row_colour != colour::none)
                    util::cout << colour::none;
                util::cout << "\n";
            }
        }
    }
noMoreDirEntries:
    if (!volume_label.empty())
    {
        util::cout << "Volume label: ";
        coutTextWithValidationError(volume_label, is_volume_label_valid);
        util::cout << '\n';
    }

    // Listing bad clusters
    ReadFATSectors(util::le_value(bpb.abFATSecs), util::le_value(bpb.abBytesPerSec));
    const auto clusterIndexSup = GetClusterSup();
    auto writingStarted = false;
    for (auto clusterIndex = 2; clusterIndex < clusterIndexSup; clusterIndex++)
    {
        const auto clusterNext = GetClusterNext(clusterIndex, clusterIndexSup);
        if (clusterNext < 0)
            break;
        if (IsUsedFatIndex(clusterNext) || !IsBadFatIndex(clusterNext))
            continue;
        if (!writingStarted)
        {
            util::cout << "Bad clusters:\n";
            writingStarted = true;
        }
        const auto logicalSectorIndexStart = ClusterIndexToLogicalSectorIndex(clusterIndex);
        const auto headerStart = LogicalSectorIndexToPhysical(logicalSectorIndexStart);
        util::cout << clusterIndex << " (" << headerStart;
        const auto sectors_per_cluster = bpb.bSecPerClust;
        if (sectors_per_cluster > 1)
        {
            const auto logicalSectorIndexLast = logicalSectorIndexStart + sectors_per_cluster - 1;
            const auto headerLast = LogicalSectorIndexToPhysical(logicalSectorIndexLast);
            util::cout << " - " << headerLast;
        }
        util::cout << ")\n";
    }

    return true;
}

Format Fat12FileSystem::GetFormat() const /*override*/
{
    return format;
}

void Fat12FileSystem::SetFormat(const Format& format) /*override*/
{
    this->format = format;
}

bool Fat12FileSystem::IsSameNamed(const FileSystem& fileSystem) const /*override*/
{
    return GetName() == fileSystem.GetName();
}

bool Fat12FileSystem::IsSameNamedWithSameCylHeadSectorsSize(const FileSystem& fileSystem) const /*override*/
{
    return IsSameNamed(fileSystem) && GetFormat().IsSameCylHeadSectorsSize(fileSystem.GetFormat());
}
