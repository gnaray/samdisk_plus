#include "Fat12Filesystem.h"

#include "Util.h"
#include "DiskUtil.h"

#include <algorithm>

Fat12Filesystem::Fat12Filesystem(const Format& fmt, Disk& disk)
    : fmt(fmt), disk(disk)
{
}

void Fat12Filesystem::PrepareBootSector()
{
    if (fmt.sectors <= 0 || fmt.heads <= 0 || fmt.cyls <= 0)
        throw util::exception("selected disk region is empty");
    /* Assumed that the disk will be written by WriteRegularDisk which writes
       only sectors in sector range (base, sectors) and in head and track range.
       Thus the boot sector requires the following check.
     */
    if (fmt.range().cyl_begin != 0 || fmt.range().head_begin != 0) // Boot sector location is outside of disk region.
        throw util::exception("missing boot sector location is unsuitable for ST output");
    if (fmt.base != 1) // Boot sector location is outside of disk region.
        throw util::exception("missing boot sector location is unsuitable for ST output, specifying base=1 might help");

    const Sector* boot_sector;
    // Boot sector is the first logical sector (index=0).
    if (!GetLogicalSector(0, boot_sector))
    {
        const CylHead cylHead{0, 0};
        auto& track_writable = const_cast<Track&>(disk.read_track(cylHead));
        if (fmt.datarate == DataRate::Unknown || fmt.encoding == Encoding::Unknown)
            throw util::exception("selected disk region has unknown datarate and encoding");
        track_writable.insert(0, Sector(fmt.datarate, fmt.encoding, Header(cylHead, 1, fmt.size)));
        boot_sector = &track_writable[0];
    }
    auto& boot_sector_writable = *const_cast<Sector*>(boot_sector);
    if (!boot_sector->has_data())
    {
        Data new_boot_sector_data(lossless_static_cast<DataST>(fmt.sector_size()), fmt.fill);
        std::copy(MISSING_SECTOR_SIGN.begin(), MISSING_SECTOR_SIGN.end(), new_boot_sector_data.begin()); // Signing sector with MISS.
        boot_sector_writable.add_with_readstats(std::move(new_boot_sector_data));
    }
    bpb = reinterpret_cast<BIOS_PARAMETER_BLOCK*>(boot_sector_writable.data_copy().data());
}

bool Fat12Filesystem::GetLogicalSector(int sector_index, const Sector*& found_sector)
{
    if (sector_index < 0 || sector_index > fmt.disk_size() / fmt.sector_size())
        throw util::exception("invalid logical sector index ", sector_index);
    const auto sector_id = sector_index % fmt.sectors + 1;
    sector_index /= fmt.sectors;
    const auto head = sector_index % fmt.heads;
    const auto cyl = sector_index / fmt.heads;
    // Finding sector this way because it might be missing.
    return disk.find(Header(cyl, head, sector_id, fmt.size), found_sector);
}

int Fat12Filesystem::DetermineSectorsPerCluster()
{
    auto fat1_sector_0 = util::le_value(bpb->abResSectors);
    auto fat_sectors = util::le_value(bpb->abFATSecs);
    // Starting with worst case: abRootDirEnts = 0x10 ...
    auto max_file_data_sectors = fmt.total_sectors() - fat1_sector_0 - bpb->bFATs * fat_sectors - 1;
    auto max_cluster_index_by_fat = fat_sectors * fmt.sector_size() / 3 * 2; // TODO Assuming FAT12
                                                                                // The min 1 limit is based on that "max cluster index by fat" * "sectors per cluster" must be >= "max file data sectors".
    auto min_1_sectors_per_cluster = static_cast<int>(std::ceil(static_cast<double>(max_file_data_sectors) / (max_cluster_index_by_fat - 2)));
    auto sectors_per_cluster = std::max(min_1_sectors_per_cluster, sectors_per_cluster_by_root_files);
    // Round up to next power of 2.
    auto sectors_per_cluster_power2 = 1 << static_cast<int>(std::ceil(log(sectors_per_cluster) / log(2)));
    if (sectors_per_cluster_power2 != 1 && sectors_per_cluster_power2 != 2)
        Message(msgWarning, "Found not normal %u sectors per cluster value", sectors_per_cluster_power2);
    return sectors_per_cluster_power2;
}

bool Fat12Filesystem::IsEofFatIndex(int fat_index)
{
    return fat_index >= 0xff8 && fat_index <= 0xfff;
}

bool Fat12Filesystem::IsNextFatIndex(int fat_index)
{
    return fat_index >= 2 && fat_index <= 0xfef;
}

bool Fat12Filesystem::IsUsedFatIndex(int fat_index)
{
    return IsEofFatIndex(fat_index) || IsNextFatIndex(fat_index);
}

int Fat12Filesystem::GetFileClusterAmount(int start_cluster)
{
    // Find length of cluster chain starting at start_cluster.
    auto fat1_data = fat1.data();
    auto fat2_data = fat2.data();
    const auto fat_byte_length = new_fat_sectors * fmt.sector_size();
    const auto cluster_sup = fat_byte_length * 2 / 3;
    int cluster_amount = 0;
    int cluster_i = start_cluster;
    do
    {
        if (cluster_i >= cluster_sup)
        {
            Message(msgWarning, "Found out of range FAT cluster index %u, it must be < ", cluster_i, cluster_sup);
            break;
        }
        auto cluster_i_fat_bytes = (cluster_i & ~0x1) * 3 / 2;
        int fat1_next_index;
        int fat2_next_index;
        if ((cluster_i & 1) == 0)
        {
            fat1_next_index = ((fat1_data[cluster_i_fat_bytes + 1] & 0xf) << 8) + fat1_data[cluster_i_fat_bytes];
            fat2_next_index = ((fat2_data[cluster_i_fat_bytes + 1] & 0xf) << 8) + fat2_data[cluster_i_fat_bytes];
        }
        else {
            fat1_next_index = ((fat1_data[cluster_i_fat_bytes + 1] & 0xf0) >> 4) + (fat1_data[cluster_i_fat_bytes + 2] << 4);
            fat2_next_index = ((fat2_data[cluster_i_fat_bytes + 1] & 0xf0) >> 4) + (fat2_data[cluster_i_fat_bytes + 2] << 4);
        }
        // Preferring first FAT but NEXT index even more.
        int fat_next_index = fat1_next_index;
        if (!IsNextFatIndex(fat1_next_index) && IsNextFatIndex(fat2_next_index))
            fat_next_index = fat2_next_index;
        if (!IsUsedFatIndex(fat1_next_index) && IsUsedFatIndex(fat2_next_index))
            fat_next_index = fat2_next_index;
        if (!IsUsedFatIndex(fat_next_index))
            break;
        cluster_amount++;
        if (IsEofFatIndex(fat_next_index))
            break;
        cluster_i = fat_next_index;
    } while (true);
    return cluster_amount;
}

int Fat12Filesystem::AnalyseDirEntries()
{
    const auto fat1_sector_0 = util::le_value(bpb->abResSectors);
    const auto fat_sectors = util::le_value(bpb->abFATSecs);
    const auto dir_sector_0 = fat1_sector_0 + bpb->bFATs * fat_sectors;
    const int normal_dir_entries_1 = 0x70;
    const int normal_dir_entries_2 = 0xe0;
    const int normal_dir_sectors_1 = normal_dir_entries_1 * 32 / fmt.sector_size();
    const int normal_dir_sectors_2 = normal_dir_entries_2 * 32 / fmt.sector_size();
    const int normal_dir_entries = fmt.sectors <= 11 ? normal_dir_entries_1 : normal_dir_entries_2;
    const int max_dir_entries = 0x200;
    const auto max_dir_sectors = max_dir_entries * 32 / fmt.sector_size();
    int sum_sectors_per_cluster = 0;
    int sectors_per_cluster_participants = 0;
    bool looking_for_0 = true;
    bool found_not_0_after_0 = false;
//    int missing_last_sector_count = 0;
    int first_0_dir_sector = 0;
    int dir_sector_i;
    const Sector* dir_sector;
    for (dir_sector_i = dir_sector_0; dir_sector_i < dir_sector_0 + max_dir_sectors; dir_sector_i++)
    {
        if (GetLogicalSector(dir_sector_i, dir_sector) && dir_sector->has_normaldata())
        {
            const auto dir_sector_min_size = std::min(dir_sector->data_size(), fmt.sector_size());
            const auto dir_sector_data = dir_sector->data_copy();
            for (int i = 0; i < dir_sector_min_size; i += 32)
            {
                auto& dir_entry = *reinterpret_cast<const msdos_dir_entry*>(&dir_sector_data[lossless_static_cast<DataST>(i)]);
                if (looking_for_0)
                {
                    if (dir_entry.name[0] == 0)
                    {
                        looking_for_0 = false;
                        first_0_dir_sector = dir_sector_i + (i > 0 ? 1 : 0);
                    }
                    // Entry must not be deleted, must not be label, must not be directory and must not be long file name part.
                    // At this point can not read directories because sector per cluster value is unknown.
                    else if (dir_entry.name[0] != 0xe5 && ((dir_entry.attr & 0x18) == 0)
                        && dir_entry.name[0] >= 33) {
                        auto cluster_amount = GetFileClusterAmount(util::le_value(dir_entry.start));
                        if (cluster_amount > 1) {
                            sum_sectors_per_cluster += static_cast<int>(std::ceil(static_cast<double>(util::le_value(dir_entry.size)) / fmt.sector_size() / cluster_amount));
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
    auto found_dir_entries = found_dir_sectors * fmt.sector_size() / 32;
    if (found_dir_entries != normal_dir_entries_1 && found_dir_entries != normal_dir_entries_2) {
        Message(msgWarning, "Found not normal %u directory entries value, it should be %u normally", found_dir_entries, normal_dir_entries);
    }
    return found_dir_entries;
}

// Examining sector distance of FAT copies and finding the best distance which equals to fat sectors.
int Fat12Filesystem::AnalyseFatSectors()
{
    if (bpb->bFATs != 2)
        throw util::exception("amount of fat not being 2 is unsuitable for ST output");
    const auto fat1_sector_0_index = util::le_value(bpb->abResSectors);
    // Starting with worst case: abFATSecs = 1, abRootDirEnts = 0x10 ...
    const auto max_file_data_sectors = fmt.total_sectors() - fat1_sector_0_index - bpb->bFATs * 1 - 1;
    // ... bSecPerClust = 1.
    const auto max_cluster_index_by_fat = max_file_data_sectors / 1 + 2;
    const auto max_fat_sectors = static_cast<int>(std::ceil(static_cast<double>(max_cluster_index_by_fat)
        * 3 / 2 / fmt.sector_size())); // TODO Assuming FAT12
    std::vector<double> fat_sector_match(lossless_static_cast<size_t>(max_fat_sectors + 1));
    std::vector<int> fat_sector_participants(lossless_static_cast<size_t>(max_fat_sectors + 1));
    std::vector<const Sector*> logical_sectors(lossless_static_cast<size_t>(fat1_sector_0_index + max_fat_sectors * 2));

    // Cache the sectors so algorithm is bit faster.
    const Sector* fat_sector;
    for (int fat_sector_i = fat1_sector_0_index; fat_sector_i < fat1_sector_0_index + max_fat_sectors * 2; fat_sector_i++)
        logical_sectors[lossless_static_cast<size_t>(fat_sector_i)] = GetLogicalSector(fat_sector_i, fat_sector) ? fat_sector : nullptr;

    // Collect distance matches.
    for (int fat_sector_dist = 1; fat_sector_dist <= max_fat_sectors; fat_sector_dist++)
    {
        for (int fat_sector_i = fat1_sector_0_index; fat_sector_i < fat1_sector_0_index + fat_sector_dist; fat_sector_i++)
        {
            auto fat1_sector = logical_sectors[lossless_static_cast<size_t>(fat_sector_i)];
            auto fat2_sector = logical_sectors[lossless_static_cast<size_t>(fat_sector_i + fat_sector_dist)];
            if (fat1_sector && fat2_sector && fat1_sector->has_normaldata() && fat2_sector->has_normaldata())
            {
                auto common_size = std::min({ fat1_sector->data_size(), fat2_sector->data_size(), fmt.sector_size() });
                int sum = 0;
                int equal = 0;
                int difference = 0;
                auto fat1_data = fat1_sector->data_copy();
                auto fat2_data = fat2_sector->data_copy();
                for (int i = 0; i < common_size; ) {
                    auto fat1_data_i = fat1_data[lossless_static_cast<DataST>(i)];
                    if (fat1_data_i == fat2_data[lossless_static_cast<DataST>(i)])
                        equal++;
                    sum += fat1_data[lossless_static_cast<DataST>(i++)];
                    difference += std::abs(fat1_data_i - static_cast<int>(std::round(sum / i)));
                }
                // equal / common_size is in [0,1], difference / common_size is in [0, 128)
                fat_sector_match[lossless_static_cast<DataST>(fat_sector_dist)] += static_cast<double>(equal)
                    * difference / 128 / common_size / common_size;
                fat_sector_participants[lossless_static_cast<DataST>(fat_sector_dist)]++;
            }
        }
    }
    // Find the best distance.
    int best_fat_sector_dist = 0;
    double best_match_percent = 0;
    for (int fat_sector_dist = 1; fat_sector_dist <= max_fat_sectors; fat_sector_dist++)
    {
        if (fat_sector_participants[lossless_static_cast<size_t>(fat_sector_dist)] == 0)
            continue;
        auto match_percent = 100 * fat_sector_match[lossless_static_cast<size_t>(fat_sector_dist)] / fat_sector_participants[lossless_static_cast<size_t>(fat_sector_dist)]
            * std::sqrt(fat_sector_dist); // Weighting with sqrt(dist) preferring match of more sectors (so max percent is above 100).
        if (match_percent > best_match_percent)
        {
            best_fat_sector_dist = fat_sector_dist;
            best_match_percent = match_percent;
        }
    }
    if (best_fat_sector_dist != 3 && best_fat_sector_dist != 5)
        Message(msgWarning, "Found not normal %u sectors per FAT value", best_fat_sector_dist);

    const auto fat_byte_length = best_fat_sector_dist * fmt.sector_size();
    // Store the FAT sectors continuously in fat1, fat2 so those can be processed by FAT12 3 bytes.
    fat1.resize(lossless_static_cast<DataST>(fat_byte_length));
    fat2.resize(lossless_static_cast<DataST>(fat_byte_length));
    for (int fat_sector_i = fat1_sector_0_index; fat_sector_i < fat1_sector_0_index + best_fat_sector_dist; fat_sector_i++)
    {
        auto fat1_sector = logical_sectors[lossless_static_cast<size_t>(fat_sector_i)];
        if (fat1_sector && fat1_sector->has_normaldata())
        {
            const auto common_size = std::min(fat1_sector->data_size(), fmt.sector_size());
            auto d_first = fat1.begin() + (fat_sector_i - fat1_sector_0_index) * fmt.sector_size();
            std::copy_n(fat1_sector->data_copy().begin(), common_size, d_first);
        }
        auto fat2_sector = logical_sectors[lossless_static_cast<size_t>(fat_sector_i + best_fat_sector_dist)];
        if (fat2_sector && fat2_sector->has_normaldata())
        {
            const auto common_size = std::min(fat2_sector->data_size(), fmt.sector_size());
            auto d_first = fat2.begin() + (fat_sector_i - fat1_sector_0_index) * fmt.sector_size();
            std::copy_n(fat2_sector->data_copy().begin(), common_size, d_first);
        }
    }
    return best_fat_sector_dist;
}

bool Fat12Filesystem::ReconstructBpb()
{
    const auto bpb_previous = *bpb;
    util::store_le_value(fmt.total_sectors(), bpb->abSectors);
    util::store_le_value(fmt.sectors, bpb->abSecPerTrack);
    util::store_le_value(fmt.heads, bpb->abHeads);
    util::store_le_value(Sector::SizeCodeToLength(fmt.size), bpb->abBytesPerSec); // It is 512 by standard.
    util::store_le_value(1, bpb->abResSectors); // It is 1 by standard (the boot sector itself).
    bpb->bFATs = 2; // It is 2 by standard.
                    // On Atari ST the disks had 2 heads, at least 9 sectors and at least 80 tracks, thus Media is usually 0xF9.
    bpb->bMedia = 0xF8 | (fmt.cyls <= 42 ? 0x4 : 0) | (fmt.sectors <= 8 ? 0x2 : 0) | (fmt.heads == 2 ? 0x1 : 0);

    auto fat_sectors = util::le_value(bpb->abFATSecs);
    auto sectors_per_cluster = bpb->bSecPerClust;
    if (fat_sectors < 1 || fat_sectors > 10 || sectors_per_cluster < 1 || sectors_per_cluster > 4) {
        new_fat_sectors = AnalyseFatSectors();
        // Varies a lot, depends mainly on disk size and amount of clusters.
        util::store_le_value(new_fat_sectors, bpb->abFATSecs);
        new_root_dir_entries = AnalyseDirEntries();
        util::store_le_value(new_root_dir_entries, bpb->abRootDirEnts); // It is 0x70 or 0xe0 normally.
                                                                        // It is usually 2 but sometimes 1 when user wants less loss per cluster but it requires bigger FAT.
        bpb->bSecPerClust = lossless_static_cast<uint8_t>(DetermineSectorsPerCluster());
    }
    return std::memcmp(&bpb_previous, bpb, sizeof(bpb_previous)) == 0;
}
