// Copy command

#include "FileSystem.h"
#include "Interval.h"
#include "Options.h"
#include "DiskUtil.h"
#include "Image.h"
#include "MemFile.h"
#include "SAMdisk.h"
#include "SAMCoupe.h"
#include "RepairSummaryDisk.h"
#include "Trinity.h"
#include "Util.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static auto& opt_base = getOpt<int>("base");
static auto& opt_detect_devfs = getOpt<std::string>("detect_devfs");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_disk_retries = getOpt<int>("disk_retries");
static auto& opt_fix = getOpt<int>("fix");
static auto& opt_merge = getOpt<int>("merge");
static auto& opt_minimal = getOpt<int>("minimal");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_nozip = getOpt<int>("nozip");
static auto& opt_quick = getOpt<int>("quick");
static auto& opt_range = getOpt<Range>("range");
static auto& opt_repair = getOpt<int>("repair");
static auto& opt_resize = getOpt<int>("resize");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_skip_stable_sectors = getOpt<bool>("skip_stable_sectors");
static auto& opt_step = getOpt<int>("step");
static auto& opt_track_retries = getOpt<int>("track_retries");
static auto& opt_verbose = getOpt<int>("verbose");

// Priority of formats for srcDiskFormat (lowest first).
enum class FormatPriority { None, Src, NormalDstImage, SrcDevFS, DstImageFS, SrcImageFS};

void ReviewTransferPolicy(Disk& src_disk, Disk& dst_disk, Disk& srcFileSystemDeterminerDisk,
                          Format& transferDiskFormat, FormatPriority& transferDiskFormatPriority,
                          DeviceReadingPolicy& deviceReadingPolicy, Range& transferDiskRange)
{
    // The opt_range, opt_base and opt_sectors are highest priority.
    const bool fmtBaseOverriden = opt_base != -1;
    const bool fmtSectorsOverriden = opt_sectors != -1;
    const bool diskRangeCylsOverriden = opt_range.cyls() > 0;
    const bool diskRangeHeadsOverriden = opt_range.heads() > 0;

    if (transferDiskFormatPriority == FormatPriority::None)
    {
        transferDiskFormat = src_disk.GetFileSystem() ? src_disk.GetFileSystem()->GetFormat() : src_disk.fmt();
        transferDiskFormatPriority = src_disk.GetFileSystem() ? (src_disk.is_constant_disk() ? FormatPriority::SrcImageFS : FormatPriority::SrcDevFS) : FormatPriority::Src;
    }

    // Check the filesystems considering the priority of formats.
    if (!src_disk.GetFileSystem() && !src_disk.is_constant_disk() && transferDiskFormatPriority < FormatPriority::SrcDevFS
            && !opt_detect_devfs.empty() && fileSystemWrappers.FindAndSetApprover(srcFileSystemDeterminerDisk, opt_detect_devfs))
    {
        transferDiskFormat = srcFileSystemDeterminerDisk.GetFileSystem()->GetFormat();
        transferDiskFormatPriority = FormatPriority::SrcDevFS;
        util::cout << "YEEHAAWW!! We have src filesystem in cmd_copy, its format=" << transferDiskFormat << "\n";
    }
    if (!dst_disk.GetFileSystem() && dst_disk.is_constant_disk() && transferDiskFormatPriority < FormatPriority::DstImageFS
            && fileSystemWrappers.FindAndSetApprover(dst_disk))
    {
        transferDiskFormat = dst_disk.GetFileSystem()->GetFormat();
        transferDiskFormatPriority = FormatPriority::DstImageFS;
        util::cout << "WOW!! We have dst filesystem in cmd_copy, its format=" << transferDiskFormat << "\n";
    }
    if (opt_normal_disk)
    {
        auto normalTrackSize = 0;
        if (fmtSectorsOverriden)
            normalTrackSize = lossless_static_cast<int>(opt_sectors);
        // Format priority = NormalDstImage is allowed because dst disk can change in each disk round.
        else if (opt_repair && dst_disk.is_constant_disk() && transferDiskFormatPriority <= FormatPriority::NormalDstImage)
        {
            int dst_track_amount = 0;
            int sum_dst_track_size = 0;
            transferDiskRange.each([&](const CylHead& cylhead) {
                auto dst_track = dst_disk.read_track(cylhead);
                NormaliseTrack(cylhead, dst_track);
                const auto track_normal_probable_size = dst_track.normal_probable_size();
                if (track_normal_probable_size > 0) {
                    sum_dst_track_size += track_normal_probable_size;
                    dst_track_amount++;
                }
            });
            if (dst_track_amount > 0)
                normalTrackSize = round_AS<int>(lossless_static_cast<double>(sum_dst_track_size) / dst_track_amount);
        }
        if (normalTrackSize > 0)
        {
            transferDiskFormat.sectors = normalTrackSize - (transferDiskFormat.base - 1);
            if (transferDiskFormatPriority < FormatPriority::NormalDstImage)
                transferDiskFormatPriority = FormatPriority::NormalDstImage;
        }
        if (fmtBaseOverriden)
        {
            const auto baseDiff = opt_base - transferDiskFormat.base;
            if (baseDiff != 0)
            {
                transferDiskFormat.base = opt_base;
                transferDiskFormat.sectors -= baseDiff;
            }
        }
    }
    if (transferDiskFormatPriority >= FormatPriority::NormalDstImage)
    {
        deviceReadingPolicy.SetLookForPossibleSectors(false); // TODO What should turn this on if user wants?
        deviceReadingPolicy.SetWantedSectorHeaderIds(Interval<int>{transferDiskFormat.base, transferDiskFormat.sectors, BaseInterval::LeftAndSize});
        if (transferDiskFormatPriority >= FormatPriority::SrcDevFS)
        {
            if (!diskRangeCylsOverriden)
                transferDiskRange.cyl_end = transferDiskFormat.cyls;
            if (!diskRangeHeadsOverriden)
                transferDiskRange.head_end = transferDiskFormat.heads;
            // Limit to the src disk tracks and heads (it is better than src format in case of device).
            ValidateRange(transferDiskRange, src_disk.cyls(), src_disk.heads(), opt_step);
        }
        util::cout << "deviceReadingPolicy = {" << deviceReadingPolicy << "}\n";
    }
}

bool ImageToImage(const std::string& src_path, const std::string& dst_path)
{
    auto src_disk = std::make_shared<Disk>();
    auto dst_disk = std::make_shared<Disk>();
    ScanContext context;

    // Force Jupiter Ace reading mode if the output file extension is .dti
    // ToDo: add mechanism for target to hint how source should be read?
    if (opt_encoding != Encoding::Ace && IsFileExt(dst_path, "dti"))
    {
        opt_encoding = Encoding::Ace;
        Message(msgInfo, "assuming --encoding=Ace due to .dti output image");
    }

    // Read the source image
    ReadImage(src_path, src_disk); // No determining filesystem here, doing it in disk_round loop.

    auto transferDiskRange = opt_range;
    // Limit to our maximum geometry, and default to copy everything present in the source
    ValidateRange(transferDiskRange, MAX_TRACKS, MAX_SIDES, opt_step, src_disk->cyls(), src_disk->heads());

    if (opt_minimal)
        TrackUsedInit(*src_disk); // Valid only for MGT format.

    RepairSummaryDisk fileSystemDeterminerDisk{*src_disk, context};
    // For merge or repair, read any existing target image, error if that fails.
    if (opt_merge || opt_repair)
    {
        ReadImage(dst_path, dst_disk, "", false); // The dst disk should be already normalised.
        if (!dst_disk->is_constant_disk())
            throw util::exception("copying to device disk with merge or repair option is not supported");
        if (!dst_disk->GetFileSystem()) // Determining filesystem here separately so it does not affect device dst disk.
            fileSystemWrappers.FindAndSetApprover(*dst_disk);
        dst_disk->WarnIfFileSystemFormatDiffers();
    }

    // tmp dst path in case of merge or repair mode.
    const std::string tmp_dst_path = util::prepend_extension(dst_path, "tmp.");
    // Image file reader might have set the filesystem already.
    Format transferDiskFormat;
    FormatPriority transferDiskFormatPriority{FormatPriority::None};
    DeviceReadingPolicy deviceReadingPolicy;
    bool result = false;
    // Do not retry disk when
    // 1) merging because it overwrites previous data, wasting of time. Copy would be the same but it becomes repair after first round.
    // 2) disk is constant because the constant disk image always provides the same data, wasting of time.
    const int disk_retries = !opt_merge && !src_disk->is_constant_disk() && opt_disk_retries >= 0 ? opt_disk_retries : 0;
    for (auto disk_round = 0; disk_round <= disk_retries; disk_round++)
    {
        int repair_track_changed_amount_per_disk = 0;
        const auto transferUniteMode = opt_merge ? RepairSummaryDisk::Merge : (opt_repair ? RepairSummaryDisk::Repair : RepairSummaryDisk::Copy);
        if (!src_disk->is_constant_disk()) // Clear cached tracks of interest of not constant disk.
            src_disk->clearCache(transferDiskRange); // Required for determining stability of sectors in the requested range.
        ReviewTransferPolicy(*src_disk, *dst_disk, fileSystemDeterminerDisk, transferDiskFormat, transferDiskFormatPriority, deviceReadingPolicy, transferDiskRange);
        bool is_disk_retry = disk_round > 0; // First reading is not retry.
        if (opt_verbose)
            Message(msgInfo, "%seading disk in %uth round", (is_disk_retry ? "Rer" : "R"), disk_round);

        // Transfer the range of tracks to the target image (i.e. copy, merge or repair).
        transferDiskRange.each([&](const CylHead& cylhead)
        {
            repair_track_changed_amount_per_disk += Disk::TransferTrack(*src_disk, cylhead, *dst_disk, context, transferUniteMode, false, deviceReadingPolicy);
        }, !opt_normal_disk);

        // Copy any metadata not already present in the target (emplace doesn't replace)
        for (const auto& m : src_disk->metadata())
            dst_disk->metadata().emplace(m);

        // Write the new/merged target image
        // When merge or repair mode is requested, a new tmp file is written and then renamed as final file
        // which works well only if dst is a file (constant disk) but not device.
        if (dst_disk->is_constant_disk() && (opt_merge || opt_repair))
        {
            result = WriteImage(tmp_dst_path, dst_disk) && std::remove(dst_path.c_str()) == 0
                    && std::rename(tmp_dst_path.c_str(), dst_path.c_str()) == 0;
        }
        else
            result = WriteImage(dst_path, dst_disk);
        if (!result)
            break;

        if (opt_verbose && repair_track_changed_amount_per_disk > 0)
            Message(msgInfo, "Destination disk's tracks were repaired %u times in %uth round", repair_track_changed_amount_per_disk, disk_round);
        // Switching to repair mode from normal mode so disk retry will repair instead of overwrite.
        // If disk retry is automatic and repairing then stop when repair could not improve the dst disk.
        if (opt_repair && disk_retries == DISK_RETRY_AUTO && repair_track_changed_amount_per_disk == 0)
            break;
        if (!opt_merge && !opt_repair)
            opt_repair = 1;
    }
    return result;
}

bool Image2Trinity(const std::string& path, const std::string&/*trinity_path*/) // ToDo: use trinity_path for record
{
    auto disk = std::make_shared<Disk>();
    const MGT_DIR* pdir = nullptr;
    std::string name;

    ReadImage(path, disk);

    // Scanning the first 10 directory sectors should be enough
    for (auto sector = 1; !pdir && sector <= MGT_SECTORS; ++sector)
    {
        auto s = disk->find(Header(0, 0, sector, 2));
        if (s == nullptr)
            break;

        // Each sector contains 2 MGT directory entries
        for (auto entry = 0; !pdir && entry < 2; ++entry)
        {
            pdir = reinterpret_cast<const MGT_DIR*>(s->data_copy().data() + 256 * entry);

            // Extract the first 4 characters of a potential MGT filename
            name = std::string(reinterpret_cast<const char*>(pdir->abName), 10);

            // Reject if not a code file, or no auto-execute, or not named AUTO*
            if ((pdir->bType & 0x3f) != 19 || pdir->bExecutePage == 0xff ||
                strcasecmp(name.substr(0, 4).c_str(), "auto"))
                pdir = nullptr;
        }
    }

    if (!pdir)
        throw util::exception("no suitable auto-executing code found");

    auto start_addr = TPeek(&pdir->bStartPage, 16384);
    auto exec_addr = TPeek(&pdir->bExecutePage);
    auto file_length = TPeek(&pdir->bLengthInPages);

    // Check the code wouldn't overwrite TrinLoad
    if (start_addr < 0x8000 && (start_addr + file_length) >= 0x6000)
        throw util::exception("code overlaps 6000-7fff range used by TrinLoad");

    auto uPos = 0;
    MEMORY mem(file_length);

    // The file start sector is taken from the directory entry.
    // Code files have a 9-byte metadata header, which must be skipped.
    auto trk = pdir->bStartTrack;
    auto sec = pdir->bStartSector;
    auto uOffset = MGT_FILE_HEADER_SIZE;

    // Loop until we've read the full file
    while (uPos < file_length)
    {
        // Bit 7 of the track number indicates head 1
        uint8_t cyl = (trk & 0x7f);
        uint8_t head = trk >> 7;

        auto s = disk->find(Header(cyl, head, sec, 2));
        if (s == nullptr || s->data_size() != SECTOR_SIZE)
            throw util::exception("end of file reading ", CylHead(cyl, head), " sector", sec);

        // Determine the size of data in this sector, which is at most 510 bytes.
        // The final 2 bytes contain the location of the next sector.
        auto uBlock = std::min(file_length - uPos, 510 - uOffset);
        auto& data = s->data_copy();

        memcpy(mem + uPos, data.data() + uOffset, lossless_static_cast<size_t>(uBlock));
        uPos += uBlock;
        uOffset = 0;

        // Follow the chain to the next sector
        trk = data[510];
        sec = data[511];
    }

    // Scan the local network for SAM machines running TrinLoad
    auto trinity = Trinity::Open();
    auto devices = trinity->devices();

    // Send the code file to the first device that responded
    Message(msgStatus, "Sending %s to %s...", name.c_str(), devices[0].c_str());
    trinity->send_file(mem, mem.size, start_addr, exec_addr);

    return true;
}

bool Hdd2Hdd(const std::string& src_path, const std::string& dst_path)
{
    bool f = false;
    bool fCreated = false;

    auto src_hdd = HDD::OpenDisk(src_path);
    if (!src_hdd)
    {
        Error("open");
        return false;
    }

    auto dst_hdd = HDD::OpenDisk(dst_path);
    if (!dst_hdd)
    {
        dst_hdd = HDD::CreateDisk(dst_path, src_hdd->total_bytes, &src_hdd->sIdentify);
        fCreated = true;
    }

    if (!dst_hdd)
        Error("create");
    else if (src_hdd->total_sectors != dst_hdd->total_sectors && !opt_resize)
        throw util::exception("Source size (", src_hdd->total_sectors, " sectors) does not match target (", dst_hdd->total_sectors, " sectors)");
    else if (src_hdd->sector_size != dst_hdd->sector_size)
        throw util::exception("Source sector size (", src_hdd->sector_size, " bytes) does not match target (", dst_hdd->sector_size, " bytes)");
    else if ((fCreated || dst_hdd->SafetyCheck()) && dst_hdd->Lock())
    {
        BDOS_CAPS bdcSrc, bdcDst;

        if (opt_resize && IsBDOSDisk(*src_hdd, bdcSrc))
        {
            GetBDOSCaps(dst_hdd->total_sectors, bdcDst);

            auto uBase = std::min(bdcSrc.base_sectors, bdcDst.base_sectors);
            auto uData = std::min(src_hdd->total_sectors - bdcSrc.base_sectors, dst_hdd->total_sectors - bdcDst.base_sectors);
            auto uEnd = opt_quick ? uBase + uData : dst_hdd->total_sectors;

            // Copy base sectors, clear unused base sector area, copy record data sectors
            f = dst_hdd->Copy(src_hdd.get(), uBase, 0, 0, uEnd);
            f &= dst_hdd->Copy(nullptr, bdcDst.base_sectors - uBase, 0, uBase, uEnd);
            f &= dst_hdd->Copy(src_hdd.get(), uData, bdcSrc.base_sectors, bdcDst.base_sectors, uEnd);

            MEMORY mem(dst_hdd->sector_size);

            // If the record count isn't a multiple of 32, the final record list entry won't be full
            if (bdcDst.records % (dst_hdd->sector_size / BDOS_LABEL_SIZE) &&
                dst_hdd->Seek(bdcDst.base_sectors - 1) && dst_hdd->Read(mem, 1))
            {
                // Determine the number of used entries in the final record list sector
                auto used_bytes = (bdcDst.records * BDOS_LABEL_SIZE) & (dst_hdd->sector_size - 1);

                // Clear the unused space to remove any entries lost by resizing, and write it back
                memset(mem + used_bytes, 0, lossless_static_cast<size_t>(dst_hdd->sector_size - used_bytes));
                dst_hdd->Seek(bdcDst.base_sectors - 1);
                dst_hdd->Write(mem, 1);
            }

            // If this isn't a quick copy, clear the remaining destination space
            if (!opt_quick)
                f &= dst_hdd->Copy(nullptr, dst_hdd->total_sectors - (bdcDst.base_sectors + uData), 0x00, bdcDst.base_sectors + uData, uEnd);

            // If the source disk is bootable, consider updating the BDOS variables
            if (opt_fix != 0 && bdcSrc.bootable && dst_hdd->Seek(0) && dst_hdd->Read(mem, 1))
            {
                // Check for compatible DOS version
                if (UpdateBDOSBootSector(mem, *dst_hdd))
                {
                    dst_hdd->Seek(0);
                    dst_hdd->Write(mem, 1);
                }
            }
        }
        else
        {
            auto uCopy = std::min(src_hdd->total_sectors, dst_hdd->total_sectors);
            f = dst_hdd->Copy(src_hdd.get(), uCopy);
        }

        dst_hdd->Unlock();
    }

    return f;
}

bool Hdd2Boot(const std::string& path, const std::string& boot_path)
{
    bool fRet = false;
    FILE* file = nullptr;

    auto hdd = HDD::OpenDisk(path.substr(0, path.rfind(':')));

    if (!hdd)
        Error("open");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!hdd->Seek(0) || !hdd->Read(mem, 1))
            Error("read");

        file = fopen(boot_path.c_str(), "wb");
        if (!file)
            Error("open");
        else if (!fwrite(mem, lossless_static_cast<size_t>(mem.size), 1, file))
            Error("write");
        else
            fRet = true;
    }

    if (file) fclose(file);
    return fRet;
}

bool Boot2Hdd(const std::string& boot_path, const std::string& hdd_path)
{
    bool fRet = false;
    MemFile file;

    auto hdd = HDD::OpenDisk(hdd_path.substr(0, hdd_path.rfind(':')));

    if (!hdd || !hdd->Seek(0))
        Error("open");
    file.open(boot_path, !opt_nozip);
    if (file.size() != hdd->sector_size)
        throw util::exception("boot sector must match sector size (", hdd->sector_size, " bytes)");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!file.read(mem, mem.size))
            Error("read");
        else if (hdd->SafetyCheck() && hdd->Lock())
        {
            // If we're writing a compatible AL+ boot sector, consider updating the BDOS variables
            if (opt_fix != 0)
                UpdateBDOSBootSector(mem, *hdd);

            if (!hdd->Write(mem, 1))
                Error("write");
            else
                fRet = true;

            hdd->Unlock();
        }
    }

    return fRet;
}

bool Boot2Boot(const std::string& src_path, const std::string& dst_path)
{
    bool fRet = false;

    auto hdd = HDD::OpenDisk(src_path.substr(src_path.rfind(':')));

    if (!hdd.get())
        Error("open");
    else
    {
        MEMORY mem(hdd->sector_size);

        if (!hdd->Seek(0) || !hdd->Read(mem, 1))
            Error("read");
        else
        {
            auto hdd2 = HDD::OpenDisk(dst_path.substr(dst_path.rfind(':')));

            if (!hdd2.get())
                Error("open2");
            else if (hdd2->sector_size != hdd->sector_size)
                throw util::exception("Source sector size (", hdd->sector_size, " bytes) does not match target (", hdd2->sector_size, " bytes)");
            else if (hdd2->SafetyCheck() && hdd2->Lock())
            {
                // If we're writing a compatible AL+ boot sector, consider updating the BDOS variables
                if (opt_fix != 0)
                    UpdateBDOSBootSector(mem, *hdd2);

                if (!hdd2->Seek(0) || !hdd2->Write(mem, 1))
                    Error("write");
                else
                    fRet = true;

                hdd2->Unlock();
            }
        }
    }

    return fRet;
}
