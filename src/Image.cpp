// High-level disk image/device reading and writing

#include "PlatformConfig.h" // For disabling fopen deprecation.
#include "Image.h"
#include "FileSystem.h"
#include "Options.h"
#include "SpectrumPlus3.h"
#include "Util.h"
#include "types.h"
//#include "BlockDevice.h"
#if 0
#include "types/record.h"
#include "types/sdf.h"
#endif

static auto& opt_cpm = getOpt<int>("cpm");
static auto& opt_fix = getOpt<int>("fix");
static auto& opt_flip = getOpt<int>("flip");
static auto& opt_head0 = getOpt<int>("head0");
static auto& opt_head1 = getOpt<int>("head1");
static auto& opt_nozip = getOpt<int>("nozip");
static auto& opt_range = getOpt<Range>("range");

void ReadImage(const std::string& path, std::shared_ptr<Disk>& disk, bool srcDisk, const std::string& determineDeviceFileSystem/* = ""*/, bool normalise/* = true*/)
{
    if (path.empty())
        throw util::exception("invalid empty path");
    disk->GetPath() = path;

    for (;;)
    {
        // Try devices first as the path may use a custom syntax
        for (auto p = aDeviceTypes; p->pszType; ++p)
            if (p->pfnRead && p->pfnRead(path, disk))
                goto diskRead;

        if (IsDir(path))
            throw util::exception("path is a directory");

        MemFile file;
        // Next try regular files (and archives)
        file.open(path, !opt_nozip);

        // Present the image to all types with read support
        for (auto p = aImageTypes; p->pszType; ++p)
            if (p->pfnRead && p->pfnRead(file, disk))
            {
                // Store the archive type the image was found in, if any
                if (file.compression() != Compress::None)
                    disk->metadata()["archive"] = to_string(file.compression());
                if (file.path().rfind(file.name()) + file.name().size() != file.path().size())
                    disk->metadata()["filename"] = file.name();
                goto diskRead;
            }

#if 0
        // Unwrap any sub-containers
        if (UnwrapSDF(olddisk, disk)) goto diskRead;   // MakeSDF image
        if (UnwrapCPM(olddisk, disk)) goto diskRead;   // BDOS CP/M record format
#endif

        throw util::exception("unrecognised disk image format");
    }
diskRead:
    // FileSystem might exist already by image file reader.
    if (!disk->GetFileSystem() && (!determineDeviceFileSystem.empty() || disk->is_constant_disk()))
        fileSystemWrappers.FindAndSetApprover(*disk, srcDisk, determineDeviceFileSystem.empty() ? DETECT_FS_AUTO : determineDeviceFileSystem);
    // FileSystem format should not differ from optional image file format. Safer to check.
    if (disk->WarnIfFileSystemFormatDiffers())
        Message(msgInfo, "Overriding disk format by %s filesystem format read from image file (%s)",
                disk->GetFileSystem()->GetName().c_str(), path.c_str());
    if (disk->GetFileSystem())
        disk->fmt() = disk->GetFileSystem()->GetFormat();

    disk->disk_is_read();

    if (normalise)
    {
        // ToDo: Make resize and flip optional?  replace fNormalise_ and fLoadFilter_?
#if 0 // breaks with sub-ranges
        if (!opt_range.empty())
            disk->resize(opt_range.cyls(), opt_range.heads());
#endif
        // Forcibly correct +3 boot loader problems?
        if (opt_fix == 1)
            FixPlus3BootLoader(disk);

        if (opt_flip)
            disk->flip_sides();
    }

#if 0
    auto cyls = disk->cyls(), heads = disk->heads();
    for (uint8_t head = 0; head < heads; ++head)
    {
        // Determine any forced head value for sectors on this track (-1 for none)
        int nHeadVal = head ? opt_head1 : opt_head0;

        // If nothing forced and we're using a regular format, use its head values
        if (nHeadVal == -1 && olddisk->format.sectors)
            nHeadVal = head ? olddisk->format.head1 : olddisk->format.head0;

        for (uint8_t cyl = 0; cyl < cyls; ++cyl)
        {
            PTRACK pt = disk->GetTrack(cyl, head);

            // Optionally normalise the track, to allow 'scan' to normalise on the fly
            if (fNormalise_)
                pt->Normalise(fLoadFilter_);

            // Forced head?
            if (nHeadVal != -1)
            {
                for (int i = 0; i < pt->sectors; ++i)
                    pt->sector[i].head = nHeadVal;
            }
        }
    }
#endif
}



bool WriteImage(const std::string& path, std::shared_ptr<Disk>& disk, const std::string& determineDeviceFileSystem/* = ""*/)
{
    disk->GetPath() = path;
#if 0
    // TODO: Wrap a CP/M image in a BDOS record container
    auto cpm_disk = std::make_shared<Disk>();
    if (opt_cpm && WrapCPM(disk, cpm_disk))
        return true;
#endif

    // Try devices first as the path may use a custom syntax
    for (auto p = aDeviceTypes; p->pszType; ++p)
    {
        if (p->pfnWrite && p->pfnWrite(path, disk))
            return true;
    }

    // Normal image file
    auto p = aImageTypes;

    // Find the type matching the output file extension
    for (; p->pszType; ++p)
    {
        // Matching extension with write
        if (IsFileExt(path, p->pszType))
            break;
    }

    if (!p->pszType)
        throw util::exception("unknown output file type");
    else if (!p->pfnWrite)
        throw util::exception(util::format(p->pszType, " is not supported for output"));

    util::unique_FILE_t file{fopen(path.c_str(), "wb")};
    if (!file)
        throw posix_error(errno, path.c_str());

    const auto fileSystemPrev = disk->GetFileSystem();

    try
    {
        // Write the image
        if (!p->pfnWrite(file.get(), disk))
            throw util::exception("output type is unsuitable for source content");
    }
    catch (...)
    {
        file.reset();
        std::remove(path.c_str());
        throw;
    }

    if (!determineDeviceFileSystem.empty() || disk->is_constant_disk())
    {
        const bool isFileSystemApproved = disk->GetFileSystem()
            || fileSystemWrappers.FindAndSetApprover(*disk, false,
                determineDeviceFileSystem.empty() ? DETECT_FS_AUTO : determineDeviceFileSystem);
        if (fileSystemPrev && (!isFileSystemApproved || !fileSystemPrev->IsSameNamed(*disk->GetFileSystem())))
            Message(msgWarning, "%s filesystem of disk at path (%s) has been modified",
                    fileSystemPrev->GetName().c_str(), path.c_str());
    }

    return true;
}
