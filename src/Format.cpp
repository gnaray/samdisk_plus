#include "Options.h"
#include "DiskUtil.h"
#include "Format.h"
#include "Sector.h"

static auto& opt_base = getOpt<int>("base");
static auto& opt_cylsfirst = getOpt<int>("cylsfirst");
static auto& opt_datarate = getOpt<DataRate>("datarate");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_fill = getOpt<int>("fill");
static auto& opt_gap3 = getOpt<int>("gap3");
static auto& opt_head0 = getOpt<int>("head0");
static auto& opt_head1 = getOpt<int>("head1");
static auto& opt_interleave = getOpt<int>("interleave");
static auto& opt_range = getOpt<Range>("range");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_size = getOpt<int>("size");
static auto& opt_skew = getOpt<int>("skew");
static auto& opt_step = getOpt<int>("step");

Format::Format(RegularFormat reg_fmt)
    : Format(GetFormat(reg_fmt))
{
}

int Format::sector_size() const
{
    return Sector::SizeCodeToLength(size);
}

int Format::track_size() const
{
    return sector_size() * sectors;
}

int Format::cyl_size() const
{
    return track_size() * heads;
}

int Format::side_size() const
{
    assert(cyls);
    return track_size() * cyls;
}

int Format::disk_size() const
{
    assert(heads);
    return side_size() * heads;
}

int Format::total_sectors() const
{
    assert(cyls && heads && sectors);
    return cyls * heads * sectors;
}

Range Format::range() const
{
    return Range(cyls, heads);
}

VectorX<int> Format::get_ids(const CylHead& cylhead) const
{
    return TrackSectorIds::GetIds(cylhead, sectors, interleave, skew, offset, base);
}


Format Format::GetFormat(RegularFormat reg_fmt)
{
    Format fmt;

    switch (reg_fmt)
    {
    case RegularFormat::MGT:    // 800K
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 10;
        fmt.skew = 1;
        fmt.gap3 = 24;
        goto caseHandled;

    case RegularFormat::ProDos: // 720K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 9;
        fmt.interleave = 2;
        fmt.skew = 2;
        fmt.gap3 = 0x50;
        fmt.fill = 0xe5;
        goto caseHandled;

    case RegularFormat::PC320:   // 320K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 40;
        fmt.sectors = 8;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x50;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC360:   // 360K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 40;
        fmt.sectors = 9;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x50;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC640:   // 640K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 8;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x50;
        fmt.fill = 0xe5;
        goto caseHandled;

    case RegularFormat::PC720:   // 720K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 9;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x50;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC1200: // 1.2M
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 15;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x54;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC1232: // 1232K
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 77;
        fmt.sectors = 8;
        fmt.size = 3;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x54;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC1440: // 1.44M
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 18;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x65;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::PC2880: // 2.88M
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_1M;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 36;
        fmt.interleave = 1;
        fmt.skew = 1;
        fmt.gap3 = 0x53;
        fmt.fill = 0xf6;
        goto caseHandled;

    case RegularFormat::D80:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 9;
        fmt.skew = 5;
        fmt.fill = 0xe5;
        goto caseHandled;

    case RegularFormat::OPD:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 18;
        fmt.size = 1;
        fmt.fill = 0xe5;
        fmt.base = 0;
        fmt.offset = 17;
        fmt.interleave = 13;
        fmt.skew = 13;
        goto caseHandled;

    case RegularFormat::MBD820:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 82;
        fmt.sectors = 5;
        fmt.size = 3;
        fmt.skew = 1;
        fmt.gap3 = 44;
        goto caseHandled;

    case RegularFormat::MBD1804:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 82;
        fmt.sectors = 11;
        fmt.size = 3;
        fmt.skew = 1;
        goto caseHandled;

    case RegularFormat::TRDOS:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 80;
        fmt.heads = 2;
        fmt.sectors = 16;
        fmt.size = 1;
        fmt.interleave = 2;
        fmt.head1 = 0;
        goto caseHandled;

    case RegularFormat::QDOS:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 80;
        fmt.heads = 2;
        fmt.sectors = 9;
        fmt.size = 2;
        goto caseHandled;

    case RegularFormat::D2M:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 81;
        fmt.sectors = 10;
        fmt.size = 3;
        fmt.fill = 0xe5;
        fmt.gap3 = 0x64;
        fmt.head0 = 1;
        fmt.head1 = 0;
        goto caseHandled;

    case RegularFormat::D4M:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_1M;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 81;
        fmt.sectors = 20;
        fmt.size = 3;
        fmt.fill = 0xe5;
        fmt.gap3 = 0x64;
        fmt.head0 = 1;
        fmt.head1 = 0;
        goto caseHandled;

    case RegularFormat::D81:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 10;
        fmt.gap3 = 0x26;
        fmt.head0 = 1;
        fmt.head1 = 0;
        goto caseHandled;

    case RegularFormat::_2D:
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 40;
        fmt.sectors = 16;
        fmt.size = 1;
        goto caseHandled;

    case RegularFormat::AmigaDOS:
        fmt.fdc = FdcType::Amiga;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::Amiga;
        fmt.cyls = 80;
        fmt.sectors = 11;
        fmt.size = 2;
        fmt.base = 0;
        goto caseHandled;

    case RegularFormat::AmigaDOSHD:
        fmt.fdc = FdcType::Amiga;
        fmt.datarate = DataRate::_500K;
        fmt.encoding = Encoding::Amiga;
        fmt.sectors = 22;
        fmt.size = 2;
        fmt.base = 0;
        goto caseHandled;

    case RegularFormat::LIF:
        fmt.cyls = 77;
        fmt.heads = 2;
        fmt.fdc = FdcType::PC;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 16;
        fmt.size = 1;
        goto caseHandled;

    case RegularFormat::AtariST:
        fmt.fdc = FdcType::WD;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.cyls = 80;
        fmt.heads = 2;
        fmt.sectors = 9;
        fmt.size = 2;
        fmt.gap3 = 40;
        fmt.fill = 0x00;
        goto caseHandled;

    case RegularFormat::TO_640K_MFM:
        fmt.cyls = 80;
        fmt.heads = 2;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::MFM;
        fmt.sectors = 16;
        fmt.size = 1;
        fmt.interleave = 7;
        fmt.gap3 = 50;
        fmt.fill = 0xe5;
        fmt.cyls_first = true;
        goto caseHandled;

    case RegularFormat::TO_320K_MFM:
        fmt = GetFormat(RegularFormat::TO_640K_MFM);
        fmt.cyls = 80;
        fmt.heads = 1;
        fmt.size = 1;
        fmt.encoding = Encoding::MFM;
        goto caseHandled;

    case RegularFormat::TO_160K_MFM:
        fmt = GetFormat(RegularFormat::TO_320K_MFM);
        fmt.cyls = 40;
        fmt.heads = 1;
        fmt.size = 1;
        fmt.encoding = Encoding::MFM;
        goto caseHandled;

    case RegularFormat::TO_160K_FM:
        fmt = GetFormat(RegularFormat::TO_320K_MFM);
        fmt.cyls = 80;
        fmt.heads = 1;
        fmt.size = 0;
        fmt.encoding = Encoding::FM;
        goto caseHandled;

    case RegularFormat::TO_80K_FM:
        fmt = GetFormat(RegularFormat::TO_160K_FM);
        fmt.cyls = 40;
        fmt.heads = 1;
        fmt.size = 0;
        fmt.encoding = Encoding::FM;
        goto caseHandled;

    case RegularFormat::DO:
        fmt.fdc = FdcType::Apple;
        fmt.datarate = DataRate::_250K;
        fmt.encoding = Encoding::Apple;
        fmt.cyls = 35;
        fmt.heads = 1;
        fmt.sectors = 16;
        fmt.base = 0;
        fmt.size = 1;
        goto caseHandled;

    case RegularFormat::Unspecified:
        goto caseHandled;

    case RegularFormat::None:
        goto caseHandled;
    }
    throw util::exception("unsupported regular format (", reg_fmt, ")");

caseHandled:
    fmt.regular_format = reg_fmt;
    return fmt;
}


bool Format::FromSize(int64_t size, Format& fmt)
{
    switch (size)
    {
    case 143360:    // Apple ][
        fmt = RegularFormat::DO;
        break;

    case 163840:    // 5.25" SSSD (160K)
        fmt = RegularFormat::PC320;
        fmt.heads = 1;
        break;

    case 184320:    // 5.25" SSSD (180K)
        fmt = RegularFormat::PC360;
        fmt.heads = 1;
        break;

    case 327680:    // 5.25" DSDD (320K)
        fmt = RegularFormat::PC320;
        break;

    case 368640:    // 5.25" DSDD (360K)
        fmt = RegularFormat::PC360;
        break;

    case 655360:    // 3.5"  DSDD (640K)
        fmt = RegularFormat::PC640;
        break;

    case 737280:    // 3.5"  DSDD (720K)
        fmt = RegularFormat::PC720;
        break;

    case 819200:    // MGT (800K), for legacy matching.
        fmt = RegularFormat::MGT;
        break;

    case 1228800:   // 5.25" DSHD (1200K)
        fmt = RegularFormat::PC1200;
        break;

    case 1261568:   // 5.25" DSHD (1232K)
        fmt = RegularFormat::PC1232;
        break;

    case 1474560:   // 3.5"  DSHD (1440K)
        fmt = RegularFormat::PC1440;
        break;

    case 1638400:   // 3.5"  DSHD (1600K)
        fmt = RegularFormat::PC1440;
        fmt.sectors = 20;
        fmt.gap3 = 0;
        break;

    case 1720320:   // 3.5"  DSHD (1680K)
        fmt = RegularFormat::PC1440;
        fmt.sectors = 21;
        fmt.gap3 = 0;
        break;

    case 1763328:   // 3.5"  DSHD (1722K)
        fmt = RegularFormat::PC1440;
        fmt.cyls = 82;
        fmt.sectors = 21;
        fmt.gap3 = 0;
        break;

    case 1784832:   // 3.5"  DSHD (1743K)
        fmt = RegularFormat::PC1440;
        fmt.cyls = 83;
        fmt.sectors = 21;
        fmt.gap3 = 0;
        break;

    case 1802240:   // 3.5"  DSHD (1760K)
        fmt = RegularFormat::PC1440;
        fmt.sectors = 22;
        fmt.gap3 = 0;
        break;

    case 1884160:   // 3.5"  DSHD (1840K)
        fmt = RegularFormat::PC1440;
        fmt.sectors = 23;
        fmt.gap3 = 0;
        break;

    case 1966080:   // 3.5"  DSHD (1920K)
        fmt = RegularFormat::PC1440;
        fmt.sectors = 24;
        fmt.gap3 = 0;
        break;

    case 2949120:   // 3.5"  DSED (2880K)
        fmt = RegularFormat::PC2880;
        break;

    default:
        return false;
    }

    return true;
}


void Format::Validate() const
{
    if (!TryValidate(cyls, heads, sectors, sector_size()))
        throw util::exception("bad geometry");
}

/*static*/ void Format::Validate(int cyls_, int heads_, int sectors_/* = 1*/, int sector_size/* = 512*/, int max_sector_size/* = 0*/)
{
    if (!TryValidate(cyls_, heads_, sectors_, sector_size, max_sector_size))
        throw util::exception("bad geometry");
}

bool Format::TryValidate() const
{
    return TryValidate(cyls, heads, sectors, sector_size());
}

/*static*/ bool Format::TryValidate(int cyls_, int heads_, int sectors_/* = 1*/, int sector_size/* = 512*/, int max_sector_size/* = 0*/)
{
    return cyls_ > 0 && cyls_ <= MAX_TRACKS &&
        heads_ > 0 && heads_ <= MAX_SIDES &&
        sectors_ > 0 && sectors_ <= MAX_SECTORS &&
        (max_sector_size == 0 || sector_size <= max_sector_size);
}

void Format::Override(bool full_control/*=false*/)
{
    if (full_control)
    {
        if (opt_range.cyls() > 0) cyls = opt_range.cyls();
        if (opt_range.heads() > 0) heads = opt_range.heads();
        if (opt_sectors != -1) sectors = lossless_static_cast<int>(opt_sectors);
        if (opt_size >= 0 && opt_size <= 7) size = opt_size;

        if (datarate == DataRate::Unknown) datarate = DataRate::_250K;
        if (encoding == Encoding::Unknown) encoding = Encoding::MFM;
    }

    // Merge any overrides from the command-line
    if (opt_fill >= 0) fill = static_cast<uint8_t>(opt_fill);
    if (opt_gap3 >= 0) gap3 = opt_gap3;
    if (opt_base != -1) base = opt_base;
    if (opt_interleave >= 0) interleave = opt_interleave;
    if (opt_skew >= 0) skew = opt_skew;
    if (opt_head0 != -1) head0 = opt_head0;
    if (opt_head1 != -1) head1 = opt_head1;
    if (opt_cylsfirst != -1) cyls_first = (opt_cylsfirst != 0);
    if (opt_datarate != DataRate::Unknown) datarate = opt_datarate;
    if (opt_encoding != Encoding::Unknown) encoding = opt_encoding;
}
