// Raw image files matched by file size alone

//#include "PlatformConfig.h"
#include "types/raw.h"
#include "DiskUtil.h"
#include "Options.h"
#include "Disk.h"
#include "MemFile.h"
#include "Util.h"

#include <memory>
#include <algorithm>

static auto& opt_base = getOpt<int>("base");
static auto& opt_range = getOpt<Range>("range");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_size = getOpt<int>("size");

bool ReadRAW(MemFile& file, std::shared_ptr<Disk>& disk)
{
    Format fmt;
    fmt.encoding = Encoding::MFM;

    // An empty format should not match an empty file!
    if (file.size() == 0)
        throw util::exception("image file is zero bytes");

    // Has the user customised any geometry parameters?
    bool customised = opt_range.cyls() || opt_range.heads() || opt_sectors > 0 || opt_size >= 0;

    // Attempt to match raw file size against a likely format.
    if (!Format::FromSize(file.size(), fmt) && !customised)
        return false;

    // Allow user overrides of the above format.
    auto orig_fmt = fmt;
    fmt.Override(true);

    // Ensure the intermediate geometry is complete.
    fmt.Validate();

    // If only cyls or heads is given, adjust the other one to match.
    if (fmt.cyls != orig_fmt.cyls && !opt_range.heads())
        fmt.heads = file.size() / (opt_range.cyls() * fmt.track_size());
    else if (fmt.heads != orig_fmt.heads && !opt_range.cyls())
        fmt.cyls = file.size() / (opt_range.heads() * fmt.track_size());

    // If only sector count or size are specified, adjust the other one to match.
    if (fmt.size != orig_fmt.size && opt_sectors < 0)
        fmt.sectors = file.size() / (fmt.cyls * fmt.heads * fmt.sector_size());
    else if (fmt.sectors != orig_fmt.sectors && opt_size < 0)
    {
        auto sector_size = file.size() / (fmt.cyls * fmt.heads * fmt.sectors);
        for (fmt.size = 0; sector_size > 128; sector_size /= 2)
            fmt.size++;
    }

    // Does the format now match the input file?
    if (fmt.disk_size() != file.size())
        throw util::exception("geometry doesn't match file size");

    // Ensure the final geometry is valid.
    fmt.Validate();

    // 720K images with a .cpm extension use the SAM Coupe Pro-Dos parameters
    if (file.size() == 737280 && IsFileExt(file.name(), "cpm"))
    {
        fmt = RegularFormat::ProDos;
        disk->strType() = "ProDos";
    }
    // Warn if the size-to-format conversion is being used unmodified.
    // This makes it more obvious when an unsupported format is matched by size.
    else if (!customised)
    {
        Message(msgWarning, "input format guessed from file size -- please check");
    }

    file.rewind();
    disk->format(fmt, file.data());
    disk->strType() = "RAW";

    return true;
}

/* Originally accepted the format when the ids of largest track are sequential
 * and ids of all other tracks are part of those ids before overriding.
 * The current solution does the same but after overriding.
 */
Format CheckBeforeWriteRAW(std::shared_ptr<Disk>& disk, const Format& format/* = Format(RegularFormat::None)*/)
{
    auto range = opt_range;
    // Ensure that disk contains range but modified range is junk because it is determined below.
    ValidateRange(range, disk->cyls(), disk->heads());

    const auto isFormatPreset = !format.IsNone();
    auto fmt = !isFormatPreset ? Format(RegularFormat::Unspecified) : format;
    if (!isFormatPreset || format.cyls < 1)
        fmt.cyls = 0;
    if (!isFormatPreset || format.heads < 1)
        fmt.heads = 0;
    if (!isFormatPreset || format.base < 0)
        fmt.base = 0xff;

    // Determine cyls, heads, datarate, encoding, size, sector range (base, sectors).
    auto typicalSectorSet = false;
    disk->each([&](const CylHead& cylhead, const Track& track) {
        // Skip empty tracks
        if (track.empty())
            return;

            // Track the used disk extent
        if (!isFormatPreset || format.cyls < 1)
            fmt.cyls = std::max(fmt.cyls, cylhead.cyl + 1);
        if (!isFormatPreset || format.heads < 1)
            fmt.heads = std::max(fmt.heads, cylhead.head + 1);

            // Keep track of the largest sector count
            if (!isFormatPreset || format.sectors < 1)
                if (track.size() > fmt.sectors)
                    fmt.sectors = static_cast<uint8_t>(track.size());

        // First not empty track?
        if (!typicalSectorSet)
        {
            typicalSectorSet = true;
            // Find a typical sector to use as a template
            ScanContext context;
            auto typical = GetTypicalSector(cylhead, track, context.sector);

            if (!isFormatPreset || format.datarate == DataRate::Unknown)
                fmt.datarate = typical.datarate;
            if (!isFormatPreset || format.encoding == Encoding::Unknown)
                fmt.encoding = typical.encoding;
            if (!isFormatPreset || format.size < 0)
                fmt.size = typical.header.size;
        }

        for (auto& s : track.sectors())
        {
            // Track the lowest sector number
            if (!isFormatPreset || format.base < 0)
                if (s.header.sector < fmt.base)
                    fmt.base = s.header.sector;
        }
    });

    if (fmt.datarate == DataRate::Unknown)
        throw util::exception("source disk is blank");

    const auto fmtBaseDetected = fmt.base;
    // Allow user overrides for flexibility
    fmt.Override(true);
    const auto fmtBaseOverriden = opt_base != -1;
    const auto fmtSectorsOverriden = opt_sectors != -1; // Override(true) accepts it.
    if (fmtBaseOverriden && !fmtSectorsOverriden) // Then sector_above remains the same.
        fmt.sectors -= (fmt.base - fmtBaseDetected);

    auto max_id = -1;
    const auto sector_above = fmt.base + fmt.sectors;
    if (fmt.sectors > 0)
    {
        disk->each([&](const CylHead& cylhead, const Track& track) {
            // Skip empty tracks
            if (track.empty())
                return;

            for (auto& s : track.sectors())
            {
                if ((fmtBaseOverriden && s.header.sector < fmt.base) // Ignore sectors below overriden sector range.
                        || (fmtSectorsOverriden && s.header.sector >= sector_above)) // Ignore sectors above sequential overriden sector range.
                    continue;
                if ((isFormatPreset && format.base >= 0 && s.header.sector < fmt.base)// Ignore sectors below specified input format.
                        || (isFormatPreset && format.sectors >= 1 && s.header.sector >= sector_above)) // Ignore sectors above sequential input format.
                    continue;
                // Track the highest sector number
                if (s.header.sector > max_id)
                {
                    max_id = s.header.sector;
                    if (max_id >= sector_above)
                        throw util::exception("non-sequential sector numbers (e.g. ",
                            s, " > top ", sector_above - 1, ") are unsuitable for raw output (overriding sector parameter might help)");
                }

                if (s.datarate != fmt.datarate)
                    throw util::exception("mixed data rates are unsuitable for raw output");
                else if (s.encoding != fmt.encoding)
                    throw util::exception("mixed data encodings are unsuitable for raw output");
                else if (s.header.size != fmt.size)
                    throw util::exception("mixed sector sizes are unsuitable for raw output, size of sector ("
                        , s, ") differs from track.format.size=", fmt.size,
                        " (bytesize=", fmt.sector_size(), ")");
            }
        });
    }

    if (max_id < fmt.base)
        throw util::exception("not found selected sectors");
    return fmt;
}

bool WriteRAW(FILE* f_, std::shared_ptr<Disk>& disk)
{
    auto fmt = CheckBeforeWriteRAW(disk);
    // Write the image, as read using the supplied format
    WriteRegularDisk(f_, *disk, fmt);

    util::cout << util::fmt("Wrote %u cyl%s, %u head%s, %2u sector%s, %4u bytes/sector = %u bytes\n",
        fmt.cyls, (fmt.cyls == 1) ? "" : "s",
        fmt.heads, (fmt.heads == 1) ? "" : "s",
        fmt.sectors, (fmt.sectors == 1) ? "" : "s",
        fmt.sector_size(), fmt.disk_size());
    return true;
}
