// View command

#include "Options.h"
#include "SAMdisk.h"
#include "DiskUtil.h"
#include "Image.h"
#include "Track.h"
#include "Util.h"

#include <algorithm>
#include <memory>

static auto& opt_a1sync = getOpt<int>("a1sync");
static auto& opt_bytes_begin = getOpt<int>("bytes_begin");
static auto& opt_bytes_end = getOpt<int>("bytes_end");
static auto& opt_datacopy = getOpt<int>("datacopy");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_size = getOpt<int>("size");
static auto& opt_step = getOpt<int>("step");
static auto& opt_verbose = getOpt<int>("verbose");

void ViewTrack(const CylHead& cylhead, const Track& track)
{
    bool viewed = false;

    ScanContext context;
    ScanTrack(cylhead, track, context);
    if (!track.empty())
        util::cout << "\n";

    if (opt_verbose)
        return;

    const Sectors& sectors = opt_normal_disk ? track.sectors_view_ordered_by_id() : track.sectors();
    for (const auto& sector : sectors)
    {
        // If a specific sector/size is required, skip non-matching ones
        if ((opt_sectors != -1 && (sector.header.sector != opt_sectors)) ||
            (opt_size >= 0 && (sector.header.size != opt_size)))
            continue;

        if (!sector.has_data())
            util::cout << "Sector " << sector.header.sector << " (no data field)\n\n";
        else
        {
            // Determine the data copy and number of bytes to show
            auto copy = std::min(sector.copies() - 1, opt_datacopy);
            const Data& data = sector.data_copy(copy);
            auto data_size = data.size();

            auto show_begin = std::max(opt_bytes_begin, 0);
            auto show_end = (opt_bytes_end < 0) ? data_size :
                std::min(opt_bytes_end, data_size);

            if (data_size != sector.size())
                util::cout << "Sector " << sector.header.sector << " (" << sector.size() << " bytes, " << data_size << " stored):\n";
            else
                util::cout << "Sector " << sector.header.sector << " (" << data_size << " bytes):\n";

            if (show_end > show_begin)
            {
                if (sector.copies() == 1)
                    util::hex_dump(data.begin(), data.begin() + show_end, show_begin);
                else
                {
                    VectorX<colour> colours;
                    colours.reserve(sector.data_size());

                    for (auto& diff : DiffSectorCopies(sector))
                    {
                        colour c;
                        switch (diff.first)
                        {
                        default:
                        case '=': c = colour::none;     break;
                        case '-': c = colour::RED;      break;
                        case '+': c = colour::YELLOW;   break;
                        }

                        VectorX<colour> fill(diff.second, c);
                        colours.insert(colours.end(), fill.begin(), fill.end());
                    }

                    assert(colours.size() == sector.data_size());
                    util::hex_dump(data.begin(), data.begin() + show_end,
                        show_begin, colours.data());
                }
            }
            util::cout << "\n";
        }

        viewed = true;
    }

    // Single sector view but nothing matched?
    if (opt_sectors >= 0 && !viewed)
        util::cout << "Sector " << opt_sectors << " not found\n";

    if (!track.empty())
        util::cout << "\n";
}

void ViewTrack_MFM_FM(Encoding encoding, BitBuffer& bitbuf)
{
    auto max_size = bitbuf.track_bitsize() * 110 / 100;

    Data track_data;
    VectorX<colour> colours;
    track_data.reserve(max_size);
    colours.reserve(max_size);

    uint32_t dword = 0;
    int bits = 0, a1 = 0, am_dist = 0xffff, data_size = 0;
    uint8_t am = 0;
    uint16_t sync_mask = opt_a1sync ? 0xffdf : 0xffff;

    bitbuf.seek(0);
    while (!bitbuf.wrapped())
    {
        dword = (dword << 1) | bitbuf.read1();
        ++bits;

        bool found_am = false;
        if (encoding == Encoding::MFM && (dword & sync_mask) == 0x4489)
        {
            found_am = true;
        }
        else if (encoding == Encoding::FM)
        {
            switch (dword)
            {
            case 0xaa222888:    // F8/C7 DDAM
            case 0xaa22288a:    // F9/C7 Alt-DDAM
            case 0xaa2228a8:    // FA/C7 Alt-DAM
            case 0xaa2228aa:    // FB/C7 DAM
            case 0xaa2a2a88:    // FC/D7 IAM
            case 0xaa222a8a:    // FD/C7 RX02 DAM
            case 0xaa222aa8:    // FE/C7 IDAM
                found_am = true;
                break;
            }
        }

        if (found_am || (bits == (encoding == Encoding::MFM ? 16 : 32)))
        {
            // Decode data byte.
            uint8_t b = 0;
            if (encoding == Encoding::MFM)
            {
                for (int i = 7; i >= 0; --i)
                    b |= static_cast<uint8_t>(((dword >> (i * 2)) & 1) << i);
            }
            else
            {
                for (int i = 7; i >= 0; --i)
                    b |= static_cast<uint8_t>(((dword >> (i * 4 + 1)) & 1) << i);
            }
            track_data.push_back(b);
            ++am_dist;

            if (encoding == Encoding::MFM && found_am)
            {
                // A1 sync byte (bright yellow if aligned to bitstream, dark yellow if not).
                colours.push_back((bits == 16) ? colour::YELLOW : colour::yellow);
                ++a1;
            }
            else
            {
                if (am == IBM_IDAM && am_dist == 4)
                    data_size = Sector::SizeCodeToLength(b);

                if (a1 == 3) // MFM address mark.
                {
                    colours.push_back(colour::RED);
                    am = b;
                    am_dist = 0;
                }
                else if (encoding == Encoding::FM && found_am) // FM address mark.
                {
                    colours.push_back((bits == 32) ? colour::RED : colour::red);
                    am = b;
                    am_dist = 0;
                }
                else if (am == IBM_IDAM && am_dist >= 1 && am_dist <= 4) // Sector CHRN.
                {
                    colours.push_back((am_dist == 3) ? colour::GREEN : colour::green);
                }
                else if (am == IBM_DAM && am_dist >= 1 && am_dist <= data_size) // Sector data.
                {
                    colours.push_back(colour::white);
                }
                else if ((am == IBM_IDAM && am_dist > 4 && am_dist <= 6) ||
                    (am == IBM_DAM && am_dist > data_size && am_dist <= (data_size + 2))) // Block CRC.
                {
                    colours.push_back(colour::MAGENTA);
                }
                else // Everything else alias gaps.
                {
                    colours.push_back(colour::grey);
                }

                a1 = 0;
            }

            bits = 0;
        }
    }

    auto show_begin = std::max(opt_bytes_begin, 0);
    auto show_end = (opt_bytes_end < 0) ? track_data.size() :
        std::min(opt_bytes_end, track_data.size());
    if (show_end > show_begin)
    {
        util::cout << encoding << " Decode (" << bitbuf.track_bitsize() << " bits):\n";
        util::hex_dump(track_data.begin(), track_data.begin() + show_end,
            show_begin, colours.data());
    }
}

bool ViewImage(const std::string& path, Range range)
{
    util::cout << "[" << path << "]\n";

    auto disk = std::make_shared<Disk>();
    ReadImage(path, disk, true);
    ValidateRange(range, MAX_TRACKS, MAX_SIDES, opt_step, disk->cyls(), disk->heads());

    range.each([&](const CylHead& cylhead) {
        auto track = disk->read_track(cylhead * opt_step);
        NormaliseTrack(cylhead, track);
        ViewTrack(cylhead, track);

        if (opt_verbose)
        {
            auto trackdata = disk->read(cylhead * opt_step);
            auto bitbuf = trackdata.preferred().bitstream();
            NormaliseBitstream(bitbuf);
            auto encoding = (opt_encoding == Encoding::Unknown) ?
                bitbuf.encoding : opt_encoding;

            switch (encoding)
            {
            case Encoding::MFM:
            case Encoding::Amiga:
            case Encoding::Agat:
            case Encoding::MX:
                ViewTrack_MFM_FM(Encoding::MFM, bitbuf);
                break;
            case Encoding::FM:
            case Encoding::RX02:
                ViewTrack_MFM_FM(Encoding::FM, bitbuf);
                break;
            default:
                throw util::exception("unsupported track view encoding");
            }
        }
        }, !opt_normal_disk);

    return true;
}

bool ViewHdd(const std::string& path, Range range)
{
    auto hdd = HDD::OpenDisk(path);
    if (!hdd)
        Error("open");

    if (!range.empty() && (range.cyls() != 1 || range.heads() != 1))
        throw util::exception("HDD view ranges are not supported");

    MEMORY mem(hdd->sector_size);

    auto cyl = range.cyl_begin;
    auto head = range.head_begin;
    auto sector = (opt_sectors < 0) ? 0 : opt_sectors;
    auto lba_sector = sector;

    if (!range.empty())
    {
        if (cyl >= hdd->cyls || head >= hdd->heads || sector > hdd->sectors || !sector)
        {
            util::cout << util::fmt("Invalid CHS address for drive (Cyl 0-%d, Head 0-%u, Sector 1-%u)\n",
                hdd->cyls - 1, hdd->heads - 1, hdd->sectors);
            return false;
        }

        // Convert CHS address to LBA
        lba_sector = (cyl * hdd->heads + head) * hdd->sectors + (sector - 1);
    }

    if (lba_sector >= hdd->total_sectors)
        util::cout << util::fmt("LBA value out of drive range (%u sectors).\n", hdd->total_sectors);
    else if (!hdd->Seek(lba_sector) || !hdd->Read(mem, 1))
        Error("read");
    else
    {
        if (!range.empty())
            util::cout << util::fmt("Cyl %s Head %s Sector %u (LBA %s):\n", CylStr(cyl), HeadStr(head), sector, lba_sector);
        else
            util::cout << util::fmt("LBA Sector %u (%u bytes):\n\n", lba_sector, mem.size);

        auto show_begin = std::max(opt_bytes_begin, 0);
        auto show_end = (opt_bytes_end < 0) ? mem.size :
            std::min(opt_bytes_end, mem.size);
        util::hex_dump(mem.pb, mem.pb + show_end, show_begin);
        return true;
    }

    return false;
}

bool ViewBoot(const std::string& path, Range range)
{
    // Strip ":0" from end of string
    std::string device = path.substr(0, path.find_last_of(":"));

    // Force boot sector
    opt_sectors = 0;

    return ViewHdd(device, range);
}
