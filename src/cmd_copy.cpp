// Copy command

#include "PlatformConfig.h"
#include "Options.h"
#include "DiskUtil.h"
#include "Image.h"
#include "MemFile.h"
#include "SAMdisk.h"
#include "SAMCoupe.h"
#include "Trinity.h"
#include "Util.h"
#include "utils.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static auto& opt_base = getOpt<int>("base");
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

bool OpenReadImage(const std::string& path, std::shared_ptr<Disk>& disk)
{
    disk.reset();
    disk = std::make_shared<Disk>();
    // Read the source image
    if (!ReadImage(path, disk))
        return false;

    // Limit to our maximum geometry, and default to copy everything present in the source
    ValidateRange(opt_range, MAX_TRACKS, MAX_SIDES, opt_step, disk->cyls(), disk->heads());

    if (opt_minimal)
        TrackUsedInit(*disk);
    return true;
}

bool ImageToImage(const std::string& src_path, const std::string& dst_path)
{
    std::shared_ptr<Disk> src_disk;
    auto dst_disk = std::make_shared<Disk>();
    ScanContext context;

    // Force Jupiter Ace reading mode if the output file extension is .dti
    // ToDo: add mechanism for target to hint how source should be read?
    if (opt_encoding != Encoding::Ace && IsFileExt(dst_path, "dti"))
    {
        opt_encoding = Encoding::Ace;
        Message(msgInfo, "assuming --encoding=Ace due to .dti output image");
    }

    if (!OpenReadImage(src_path, src_disk))
        return false;

    const bool skip_stable_sectors = opt_skip_stable_sectors && !src_disk->is_constant_disk() ? true : false;

    // tmp dst path in case of merge or repair mode.
    const std::string tmp_dst_path = util::prepend_extension(dst_path, "tmp.");
    bool result = false;
    // Do not retry disk when
    // 1) merging because it overwrites previous data, wasting of time.
    // 2) disk is constant because the constant disk image always provides the same data, wasting of time.
    const int disk_retries = !opt_merge && !src_disk->is_constant_disk() && opt_disk_retries >= 0 ? opt_disk_retries : 0;
    bool is_dst_disk_read = false;
    for (auto disk_round = 0; disk_round <= disk_retries; disk_round++)
    {
        // For merge or repair, read any existing target image in 0th round.
        // If switched to repair mode then read any existing target image in 1st round.
        if ((opt_merge || opt_repair) && !is_dst_disk_read)
        {
            // Read the image, ignore merge or repair if that fails
            if (!ReadImage(dst_path, dst_disk, false))
                throw util::exception("target image not found");
            is_dst_disk_read = true;
        }

        // If repair mode and normal disk then determine normal track size by calculating the average track size.
        int normal_track_size = 0;
        int normal_first_sector_id = opt_base > 0 ? opt_base : 1;
        if (opt_repair && opt_normal_disk) {
            // If sectors is specified then the normal track size is the sectors plus first sector id 0-based.
            if (opt_sectors > 0)
            {
                normal_track_size = lossless_static_cast<int>(opt_sectors) + normal_first_sector_id - 1;
            }
            else
            {
                int dst_track_amount = 0;
                int sum_dst_track_size = 0;
                opt_range.each([&](const CylHead& cylhead) {
                    Track dst_track = dst_disk->read_track(cylhead);
                    NormaliseTrack(cylhead, dst_track);
                    auto track_normal_probable_size = dst_track.normal_probable_size();
                    if (track_normal_probable_size > 0) {
                        sum_dst_track_size += track_normal_probable_size;
                        dst_track_amount++;
                    }
                });
                if (dst_track_amount > 0)
                    normal_track_size = round_AS<int>(lossless_static_cast<double>(sum_dst_track_size) / dst_track_amount);
            }
        }

        int repair_track_changed_amount_per_disk = 0;
        bool is_disk_retry = disk_round > 0; // First reading is not retry.
        if (opt_verbose)
            Message(msgInfo, "%seading disk in %uth round", (is_disk_retry ? "Rer" : "R"), disk_round);

        // Copy the range of tracks to the target image
        opt_range.each([&](const CylHead& cylhead) {
            // In minimal reading mode, skip unused tracks
            if (opt_minimal && !IsTrackUsed(cylhead.cyl, cylhead.head))
                return;

            int repair_track_changed_amount_per_track = 0;
            // Do not retry track when
            // 1) not repairing because it overwrites previous data, wasting of time.
            // 2) disk is constant because the constant disk image always provides the same data, wasting of time.
            const int track_retries = opt_repair && !src_disk->is_constant_disk() && opt_track_retries >= 0 ? opt_track_retries : 0;
            for (auto track_round = 0; track_round <= track_retries; track_round++)
            {
                bool is_track_retried = track_round > 0; // First reading is not retry.
                if (opt_verbose)
                    Message(msgInfo, "%seading track in %uth round", (is_track_retried ? "Rer" : "R"), disk_round);

                Track dst_track;
                Message(msgStatus, "Reading %s", CH(cylhead.cyl, cylhead.head));
                if (opt_repair) // Read dst track early so we can check if it has bad sectors.
                {
                    dst_track = dst_disk->read_track(cylhead);
                    NormaliseTrack(cylhead, dst_track);
                }

                DeviceReadingPolicy deviceReadingPolicy;
                // If repair mode and user specified skip_stable_sectors then skip checking those.
                if (opt_repair && skip_stable_sectors) // Repair mode => dst track can be checked.
                {
                    deviceReadingPolicy.SetSkippableSectors(dst_track.stable_sectors());
                    // If repair mode and user specified normal-disk then do not repair tracks
                    // which has track size amount of id sequence at least.
                    if (opt_normal_disk && normal_track_size > 0
                        && deviceReadingPolicy.SkippableSectors().HasIdSequence(normal_first_sector_id, normal_track_size - normal_first_sector_id + 1))
                        return;
                    if (opt_verbose && !deviceReadingPolicy.SkippableSectors().empty()) {
                        Message(msgInfo, "Ignoring already good sectors on %s: %s",
                            CH(cylhead.cyl, cylhead.head), deviceReadingPolicy.SkippableSectors().SectorIdsToString().c_str());
                    }
                }

                TrackData src_data;
                int src_disk_read_i;
                for (src_disk_read_i = 5; src_disk_read_i > 0; src_disk_read_i--) // Try 5 times, it could be commandline option argument.
                {
                    try
                    {
                        // https://docs.rs-online.com/41b6/0900766b8001b0a3.pdf, 7.2 Read error
                        // Seeking head forward then backward then forward etc. when track is retried.
                        const auto with_head_seek_to = is_track_retried ? std::max(0, std::min(cylhead.cyl + (track_round % 2 == 1 ? 1 : -1), src_disk->cyls() - 1)) : -1;
                        src_data = src_disk->read(cylhead * opt_step, !src_disk->is_constant_disk(), with_head_seek_to, deviceReadingPolicy);
                        break;
                    }
                    catch (util::diskspeedwrong_exception & e)
                    {
                        if (!opt_normal_disk)
                            throw;
                        util::cout << colour::RED << "Error: " << e.what() << colour::none << '\n';
                    }
                    Message(msgInfo, "If it happens too often then adjusting rpm and rpm-time-tolerance-permille should help.");
                    if (!OpenReadImage(src_path, src_disk))
                        throw util::exception("Reopening ", src_path, " failed");
                }
                if (src_disk_read_i == 0)
                    continue;
                auto src_track = src_data.track();

                if (src_data.has_bitstream())
                {
                    auto bitstream = src_data.bitstream();
                    if (NormaliseBitstream(bitstream))
                    {
                        src_data = TrackData(src_data.cylhead, std::move(bitstream));
                        src_track = src_data.track();
                    }
                }

                bool changed = NormaliseTrack(cylhead, src_track);

                if (opt_verbose)
                    ScanTrack(cylhead, src_track, context, deviceReadingPolicy.SkippableSectors());

                // Repair or copy?
                if (opt_repair)
                {
                    // Repair the target track using the source track.
                    auto repair_track_changed_amount = RepairTrack(cylhead, dst_track, src_track, deviceReadingPolicy.SkippableSectors());

                    dst_disk->write(cylhead, std::move(dst_track));
                    // If track retry is automatic and repairing then stop when repair could not improve the dst disk.
                    if (track_retries == DISK_RETRY_AUTO && repair_track_changed_amount == 0)
                        break;
                    repair_track_changed_amount_per_track += repair_track_changed_amount;
                }
                else
                {
                    // If the source track was modified it becomes the only track data.
                    if (changed)
                        dst_disk->write(cylhead, std::move(src_track));
                    else
                    {
                        // Preserve any source data.
                        src_data.cylhead = cylhead;
                        dst_disk->write(std::move(src_data));
                    }
                }
            }
            repair_track_changed_amount_per_disk += repair_track_changed_amount_per_track;
            if (opt_verbose && repair_track_changed_amount_per_track > 0)
                Message(msgInfo, "Destination disk's track %s was repaired %u times in %uth round",
                    CH(cylhead.cyl, cylhead.head), repair_track_changed_amount_per_track, disk_round);
        }, !opt_normal_disk);

        // Copy any metadata not already present in the target (emplace doesn't replace)
        for (const auto& m : src_disk->metadata)
            dst_disk->metadata.emplace(m);

        // Write the new/merged target image
        // When merge or repair mode is requested, a new tmp file is written and then renamed as final file.
        if (opt_merge || opt_repair) {
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

    if (!ReadImage(path, disk))
        return false;

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
    else if (!file.open(boot_path, !opt_nozip))
        /* already reported */;
    else if (file.size() != hdd->sector_size)
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
