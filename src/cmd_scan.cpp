// Scan command

#include "Options.h"
#include "SAMdisk.h"
#include "Image.h"
#include "IBMPCBase.h"
#include "DiskUtil.h"
#include "Util.h"

static auto& opt_detect_devfs = getOpt<std::string>("detect_devfs");
static auto& opt_nodiff = getOpt<int>("nodiff");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_offsets = getOpt<int>("offsets");
static auto& opt_readstats = getOpt<bool>("readstats");
static auto& opt_step = getOpt<int>("step");
static auto& opt_verbose = getOpt<int>("verbose");

void ScanTrack(const CylHead& cylhead, const Track& track, ScanContext& context, const UniqueSectors &ignored_sectors/* = UniqueSectors{}*/)
{
    // Reset the context if the cylinder is before the last (such as at a head change)
    if (cylhead.cyl < context.last_cylhead.cyl)
        context = ScanContext();

    context.last_cylhead = cylhead;

    // Process only non-blank tracks
    if (!track.empty())
    {
        // Have we warned about possible incompatible disks?
        // Example disk: Anaconda (TR-DOS)
        if (!context.warned && track.tracklen && track[0].encoding == Encoding::MFM)
        {
            auto& first_sector = *(track.begin());
            auto& last_sector = *(track.rbegin());

            // Calculate the offset needed to hide a 256-byte sector using the current encoding and
            // data rate.  If the first sector offset exceeds that then there could be a problem.
            auto min_offset_bits = DataBytePositionAsBitOffset(GetFmOrMfmSectorOverhead(first_sector.datarate, first_sector.encoding, Sector::SizeCodeToLength(1)), first_sector.encoding);

            // Calculate the gap between the end of final sector and the start of the first sector
            auto data_end_bits = last_sector.offset + DataBytePositionAsBitOffset(GetFmOrMfmSectorOverhead(last_sector.datarate, last_sector.encoding, last_sector.size()), last_sector.encoding);
            auto wrap_start_bits = track.tracklen + first_sector.offset;

            // If the gap before the first visible sector is suspiciously large, and the wrapping
            // gap is large enough to hide a 256-byte sector, warn the user
            if (first_sector.offset > min_offset_bits && (data_end_bits + min_offset_bits) < wrap_start_bits)
            {
                Message(msgWarning, "late track start on %s may indicate missing first sector", cylhead.ToString().c_str());
                context.warned = true;
            }
        }

        Sector typical = GetTypicalSector(cylhead, track, context.sector);
        bool custom_cyl = cylhead.cyl != typical.header.cyl;
        bool custom_head = cylhead.head != typical.header.head;

        // If the encoding, rate or size have changed, show the track settings
        if (typical.datarate != context.sector.datarate ||
            typical.encoding != context.sector.encoding ||
            (typical.header.cyl != context.sector.header.cyl && custom_cyl) ||
            (typical.header.head != context.sector.header.head && custom_head) ||
            typical.header.size != context.sector.header.size ||
            typical.gap3 != context.gap3 ||
            track.size() != context.sectors ||
            custom_cyl != context.custom_cyl ||
            custom_head != context.custom_head)
        {
            util::cout << util::fmt("%s %s, %2u sector%s, %4u bytes%s",
                to_string(typical.datarate).c_str(), to_string(typical.encoding).c_str(),
                track.size(), (track.size() == 1) ? "" : "s",
                typical.size(), (track.size() == 1) ? "" : "/sector");

            if (custom_cyl && typical.header.cyl != cylhead.cyl) util::cout << ", c=" << CylStr(typical.header.cyl);
            if (custom_head && typical.header.head != cylhead.head) util::cout << ", h=" << HeadStr(typical.header.head);
            if (typical.header.size != Sector::SizeCodeToRealSizeCode(typical.header.size)) util::cout << ", n=" << SizeStr(typical.header.size);
            if (typical.gap3 != 0) util::cout << ", gap3=" << ByteStr(typical.gap3);

            util::cout << ":\n";
        }

        context.sector = typical;
        context.sectors = track.size();
        context.gap3 = typical.gap3;
        context.custom_cyl = custom_cyl;
        context.custom_head = custom_head;
    }

    auto flags = 0;
    if (opt_offsets == 1) flags |= DUMP_OFFSETS;
    if (!opt_nodiff) flags |= DUMP_DIFF;
    if (opt_readstats) flags |= DUMP_READSTATS;
    DumpTrack(cylhead, track, context, flags, ignored_sectors);
}

bool ScanImage(const std::string& path, Range range)
{
    util::cout << '[' << path << "]\n";
    util::cout.screen->flush();

    auto disk = std::make_shared<Disk>();
    ReadImage(path, disk, true, opt_detect_devfs);
    const Format& fmt = disk->fmt();

    // Regular format and no range specified?
    if (!opt_verbose && range.empty() && !fmt.IsNone())
    {
        util::cout << util::fmt("%s %s, %2u cyls, %u heads, %2u sectors, %4u bytes/sector\n",
            to_string(fmt.datarate).c_str(), to_string(fmt.encoding).c_str(),
            disk->cyls(), disk->heads(), fmt.sectors, fmt.sector_size());

        std::stringstream ss;
        if (fmt.base != 1) { ss << util::fmt(" Base=%u", fmt.base); }
        if (fmt.offset) { ss << util::fmt(" Offset=%u", fmt.offset); }
        if (fmt.skew) { ss << util::fmt(" Skew=%u", fmt.skew); }
        if (fmt.interleave > 1) { ss << util::fmt(" Interleave=%u:1", fmt.interleave); }
        if (fmt.head0 != 0) { ss << util::fmt(" Head0=%u", fmt.head0); }
        if (fmt.head1 != 1) { ss << util::fmt(" Head1=%u", fmt.head1); }
        if (fmt.gap3) { ss << util::fmt(" Gap3=%u", fmt.gap3); }

        auto str = ss.str();
        if (!str.empty())
            util::cout << str << "\n";
    }
    else
    {
        ValidateRange(range, MAX_TRACKS, MAX_SIDES, opt_step, disk->cyls(), disk->heads());
        util::cout << range << ":\n";

        disk->preload(range, opt_step);

        ScanContext context;
        range.each([&](const CylHead cylhead) {
            if (cylhead.cyl == range.cyl_begin)
                context = ScanContext();

            auto track = disk->read_track(cylhead * opt_step);

            NormaliseTrack(cylhead, track);
            ScanTrack(cylhead, track, context);
            }, !opt_normal_disk); // A dedicated option would be better for cyls_first.
    }

    return true;
}
