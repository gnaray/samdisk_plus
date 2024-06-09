// Main entry point and command-line handler

#include "SAMdisk.h"
#include "FileSystem.h"
#include "Options.h"
#include "Util.h"
#include "types.h"
#include "BlockDevice.h"
#include "FluxDecoder.h"
#ifdef _WIN32
#include "CrashDump.h"
#endif

constexpr int STABILITY_LEVEL_DEFAULT = 3;

enum { cmdCopy, cmdScan, cmdFormat, cmdList, cmdView, cmdInfo, cmdDir, cmdRpm, cmdVerify, cmdUnformat, cmdVersion, cmdCreate, cmdEnd };

static const char* aszCommands[] =
{ "copy",  "scan",  "format",  "list",  "view",  "info",  "dir",  "rpm",  "verify",  "unformat",  "version",  "create",  nullptr };

// The options and its publishers. [BEGIN]

struct OPTIONS
{
    Range range{};
    int step = 1;

    int base = -1, size = -1, gap3 = -1, interleave = -1, skew = -1, fill = -1;
    int gaps = -1, gap2 = -1, gap4b = -1, idcrc = -1, gapmask = -1, maxsplice = -1;
    int cylsfirst = -1, head0 = -1, head1 = -1, steprate = -1, check8k = -1;
    int offsets = -1, fix = -1, mt = -1, plladjust = -1, hardsectors = -1;

    int command = 0, hex = 0, debug = 0, verbose = 0, log = 0, force = 0, quick = 0;
    int merge = 0, repair = 0, trim = 0, calibrate = 0, newdrive = 0, byteswap = 0;
    int noweak = 0, nosig = 0, nodata = 0, nocfa = 0, noidentify = 0, nospecial = 0;
    int nozip = 0, nodiff = 0, noformat = 0, nodups = 0, nowobble = 0, nottb = 0;
    int bdos = 0, atom = 0, hdf = 0, resize = 0, cpm = 0, minimal = 0, legacy = 0;
    int absoffsets = 0, datacopy = 0, align = 0, keepoverlap = 0, fmoverlap = 0;
    int flip = 0, multiformat = 0, rpm = 0, tty = 0, time = 0;
    int a1sync = 0;

    bool normal_disk = false;
    bool readstats = false, paranoia = false, skip_stable_sectors = false;
    bool fdraw_rescue_mode = false;
    bool unhide_first_sector_by_track_end_sector = false;
    std::string detect_devfs{}; // Detect device (floppy) filesystem thus use its format.

    RetryPolicy rescans = 0, retries = 5;
    int maxcopies = 3;
    int scale = 100, pllphase = DEFAULT_PLL_PHASE;
    int bytes_begin = 0, bytes_end = std::numeric_limits<int>::max();
    int track_retries = -1, disk_retries = -1;
    int stability_level = -1, byte_tolerance_of_time = Track::COMPARE_TOLERANCE_BYTES;

    Encoding encoding{ Encoding::Unknown };
    DataRate datarate{ DataRate::Unknown };
    PreferredData prefer{ PreferredData::Unknown };
    long sectors = -1;
    std::string label{}, boot{};

    char szSource[MAX_PATH], szTarget[MAX_PATH];
};

class Options
{
public:
    static struct OPTIONS opt;
};

struct OPTIONS Options::opt;

template<>
int& getOpt(const char* key)
{
    static const std::map<std::string, int&> s_mapStringToIntegerVariables =
    {
        {"a1sync", Options::opt.a1sync},
        {"absoffsets", Options::opt.absoffsets},
        {"align", Options::opt.align},
        {"atom", Options::opt.atom},
        {"base", Options::opt.base},
        {"bdos", Options::opt.bdos},
        {"byte_tolerance_of_time", Options::opt.byte_tolerance_of_time},
        {"bytes_begin", Options::opt.bytes_begin},
        {"bytes_end", Options::opt.bytes_end},
        {"byteswap", Options::opt.byteswap},
        {"calibrate", Options::opt.calibrate},
        {"check8k", Options::opt.check8k},
        {"command", Options::opt.command},
        {"cpm", Options::opt.cpm},
        {"cylsfirst", Options::opt.cylsfirst},
        {"datacopy", Options::opt.datacopy},
        {"debug", Options::opt.debug},
        {"disk_retries", Options::opt.disk_retries},
        {"fill", Options::opt.fill},
        {"fix", Options::opt.fix},
        {"flip", Options::opt.flip},
        {"fmoverlap", Options::opt.fmoverlap},
        {"force", Options::opt.force},
        {"gap2", Options::opt.gap2},
        {"gap3", Options::opt.gap3},
        {"gap4b", Options::opt.gap4b},
        {"gapmask", Options::opt.gapmask},
        {"gaps", Options::opt.gaps},
        {"hardsectors", Options::opt.hardsectors},
        {"hdf", Options::opt.hdf},
        {"head0", Options::opt.head0},
        {"head1", Options::opt.head1},
        {"hex", Options::opt.hex},
        {"idcrc", Options::opt.idcrc},
        {"interleave", Options::opt.interleave},
        {"keepoverlap", Options::opt.keepoverlap},
        {"legacy", Options::opt.legacy},
        {"log", Options::opt.log},
        {"maxcopies", Options::opt.maxcopies},
        {"maxsplice", Options::opt.maxsplice},
        {"merge", Options::opt.merge},
        {"minimal", Options::opt.minimal},
        {"mt", Options::opt.mt},
        {"multiformat", Options::opt.multiformat},
        {"newdrive", Options::opt.newdrive},
        {"nocfa", Options::opt.nocfa},
        {"nodata", Options::opt.nodata},
        {"nodiff", Options::opt.nodiff},
        {"nodups", Options::opt.nodups},
        {"noformat", Options::opt.noformat},
        {"noidentify", Options::opt.noidentify},
        {"nosig", Options::opt.nosig},
        {"nospecial", Options::opt.nospecial},
        {"nottb", Options::opt.nottb},
        {"noweak", Options::opt.noweak},
        {"nowobble", Options::opt.nowobble},
        {"nozip", Options::opt.nozip},
        {"offsets", Options::opt.offsets},
        {"plladjust", Options::opt.plladjust},
        {"pllphase", Options::opt.pllphase},
        {"quick", Options::opt.quick},
        {"repair", Options::opt.repair},
        {"resize", Options::opt.resize},
        {"rpm", Options::opt.rpm},
        {"scale", Options::opt.scale},
        {"size", Options::opt.size},
        {"skew", Options::opt.skew},
        {"stability_level", Options::opt.stability_level},
        {"step", Options::opt.step},
        {"steprate", Options::opt.steprate},
        {"time", Options::opt.time},
        {"track_retries", Options::opt.track_retries},
        {"trim", Options::opt.trim},
        {"tty", Options::opt.tty},
        {"verbose", Options::opt.verbose},
    };
    return s_mapStringToIntegerVariables.at(key);
}

template<>
bool& getOpt(const char* key)
{
    static const std::map<std::string, bool&> s_mapStringToBoolVariables =
    {
        {"unhide_first_sector_by_track_end_sector", Options::opt.unhide_first_sector_by_track_end_sector },
        {"normal_disk", Options::opt.normal_disk},
        {"paranoia", Options::opt.paranoia},
        {"readstats", Options::opt.readstats},
        {"skip_stable_sectors", Options::opt.skip_stable_sectors},
        {"fdraw_rescue_mode", Options::opt.fdraw_rescue_mode},
    };
    return s_mapStringToBoolVariables.at(key);
}

template<>
long& getOpt(const char* key)
{
    static const std::map<std::string, long&> s_mapStringToLongVariables =
    {
        {"sectors", Options::opt.sectors}
    };

    return s_mapStringToLongVariables.at(key);
}

template<>
Range& getOpt(const char* key)
{
    static const std::map<std::string, Range&> s_mapStringToRangeVariables =
    {
        {"range", Options::opt.range}
    };

    return s_mapStringToRangeVariables.at(key);
}

template<>
Encoding& getOpt(const char* key)
{
    static const std::map<std::string, Encoding&> s_mapStringToEncodingVariables =
    {
        {"encoding", Options::opt.encoding}
    };

    return s_mapStringToEncodingVariables.at(key);
}

template<>
DataRate& getOpt(const char* key)
{
    static const std::map<std::string, DataRate&> s_mapStringToDataRateVariables =
    {
        {"datarate", Options::opt.datarate}
    };

    return s_mapStringToDataRateVariables.at(key);
}

template<>
PreferredData& getOpt(const char* key)
{
    static const std::map<std::string, PreferredData&> s_mapStringToPreferredDataVariables =
    {
        {"prefer", Options::opt.prefer}
    };

    return s_mapStringToPreferredDataVariables.at(key);
}

template<>
std::string& getOpt(const char* key)
{
    static const std::map<std::string, std::string&> s_mapStringToStringVariables =
    {
        {"label", Options::opt.label},
        {"boot", Options::opt.boot},
        {"detect_devfs", Options::opt.detect_devfs}
    };

    return s_mapStringToStringVariables.at(key);
}

template<>
charArrayMAX_PATH& getOpt(const char* key)
{
    static const std::map<std::string, char(&)[MAX_PATH]> s_mapStringToCharArrayVariables =
    {
        {"szSource", Options::opt.szSource},
        {"szTarget", Options::opt.szTarget}
    };

    return s_mapStringToCharArrayVariables.at(key);
}

template<>
RetryPolicy& getOpt(const char* key)
{
    static const std::map<std::string, RetryPolicy&> s_mapStringToRetryPolicyVariables =
    {
        {"rescans", Options::opt.rescans},
        {"retries", Options::opt.retries},
    };

    return s_mapStringToRetryPolicyVariables.at(key);
}

// The options and its publishers. [END]

void Version()
{
    util::cout << colour::WHITE << "SAMdisk 4.0 ALPHA (" __DATE__ ")" <<
        colour::none << ", (c) 2002-" << YEAR_LAST_2_DIGITS << " Simon Owen\n";
}

int Usage()
{
    Version();
    Format fmtMGT = RegularFormat::MGT;

    util::cout << "\n"
        << " SAMDISK [copy|scan|format|list|view|info|dir|rpm] <args>\n"
        << "\n"
        << "  -c, --cyls=N        cylinder count (N) or range (A-B)\n"
        << "  -h, --head=N        single head select (0 or 1)\n"
        << "  -s, --sector[s]     sector count for format, or single sector select\n"
        << "  -r, --retries=N     retry count for bad sectors (default=" << Options::opt.retries << ")\n"
        << "  -R, --rescans=N     rescan count for full track reads (default=" << Options::opt.rescans << ")\n"
        << "  -d, --double-step   step floppy head twice between tracks\n"
        << "  -f, --force         suppress confirmation prompts (careful!)\n"
        << "\n"
        << "The following apply to regular disk formats only:\n"
        << "  -n, --no-format     skip formatting stage when writing\n"
        << "  -m, --minimal       read/write only used MGT tracks\n"
        << "  -g, --gap3=N        override gap3 inter-sector spacing (default=0; auto)\n"
        << "  -i, --interleave=N  override sector interleave (default=" << fmtMGT.interleave << ")\n"
        << "  -k, --skew=N        override inter-track skew (default=" << fmtMGT.skew << ")\n"
        << "  -z, --size=N        override sector size code (default=" <<
        fmtMGT.size << "; " << Sector::SizeCodeToLength(fmtMGT.size) << " bytes)\n"
        << "  -b, --base=N        override lowest sector number (default=" << fmtMGT.base << ")\n"
        << "  -0, --head[0|1]=N   override head 0 or 1 value\n"
        << "\n"
        << "See " << colour::CYAN << "https://simonowen.com/samdisk/" << colour::none << " for further details.\n";

    exit(1);
}

void ReportTypes()
{
    util::cout << "\nSupported image types:\n";

    std::string header = " R/W:";
    for (auto p = aImageTypes; p->pszType; ++p)
    {
        if (p->pfnRead && p->pfnWrite && *p->pszType)
        {
            util::cout << header << ' ' << p->pszType;
            header.clear();
        }
    }

    header = "\n R/O:";
    for (auto p = aImageTypes; p->pszType; ++p)
    {
        if (p->pfnRead && !p->pfnWrite && *p->pszType)
        {
            util::cout << header << ' ' << p->pszType;
            header.clear();
        }
    }

    util::cout << '\n';
}

void ReportFileSystems()
{
    std::ostringstream ss;
    ss << "\nSupported filesystems:\n";
    if (fileSystemWrappers.empty())
        ss << "NONE";
    else
    {
        for (const auto& fileSystemWrapper : fileSystemWrappers)
            ss << ' ' << fileSystemWrapper.get();
    }

    util::cout << ss.str() << '\n';
}

void ReportBuildOptions()
{
    static const VectorX<const char*> options{
#ifdef HAVE_ZLIB
        "zlib",
#endif
#ifdef HAVE_BZIP2
        "bzip2",
#endif
#ifdef HAVE_LZMA
        "lzma",
#endif
#ifdef HAVE_WINUSB
        "WinUSB",
#endif
#ifdef HAVE_LIBUSB1
        "libusb1",
#endif
#ifdef HAVE_FTDI
        "FTDI",
#endif
#ifdef HAVE_FTD2XX
        "FTD2XX",
#endif
#ifdef HAVE_CAPSIMAGE
        "CAPSimage",
#endif
#ifdef HAVE_FDRAWCMD_H
        "fdrawcmd.sys",
#endif
    };

    if (!options.empty())
    {
        util::cout << "\nBuild features:\n";
        for (const auto& o : options)
            util::cout << ' ' << o;
        util::cout << "\n";
    }
}

void LongVersion()
{
    Version();
    ReportTypes();
    ReportFileSystems();
    ReportBuildOptions();
#ifdef HAVE_FDRAWCMD_H
    ReportDriverVersion();
#endif
}


extern "C" {
#include "getopt.h" // IWYU pragma: keep
}

enum {
    OPT_RPM = 256, OPT_LOG, OPT_VERSION, OPT_HEAD0, OPT_HEAD1, OPT_GAPMASK, OPT_MAXCOPIES,
    OPT_MAXSPLICE, OPT_CHECK8K, OPT_BYTES, OPT_HDF, OPT_ORDER, OPT_SCALE, OPT_PLLADJUST,
    OPT_PLLPHASE, OPT_ACE, OPT_MX, OPT_AGAT, OPT_NOFM, OPT_STEPRATE, OPT_PREFER, OPT_DEBUG,
    OPT_TRACK_RETRIES, OPT_DISK_RETRIES, OPT_NORMAL_DISK,
    OPT_READSTATS, OPT_PARANOIA, OPT_SKIP_STABLE_SECTORS, OPT_STABILITY_LEVEL,
    OPT_DETECT_DEVFS,
    OPT_BYTE_TOLERANCE_OF_TIME,
    OPT_FDRAW_RESCUE_MODE,
    OPT_UNHIDE_FIRST_SECTOR_BY_TRACK_END_SECTOR
};

static struct option long_options[] =
{
    { "cyls",       required_argument, nullptr, 'c' },
    { "head",       required_argument, nullptr, 'h' },
    { "sectors",    required_argument, nullptr, 's' },
    { "hard-sectors",required_argument,nullptr, 'H' },
    { "retries",    required_argument, nullptr, 'r' },
    { "rescans",    optional_argument, nullptr, 'R' },
    { "double-step",      no_argument, nullptr, 'd' },
    { "verbose",          no_argument, nullptr, 'v' },

    { "no-format",        no_argument, nullptr, 'n' },
    { "minimal",          no_argument, nullptr, 'm' },
    { "gap3",       required_argument, nullptr, 'g' },
    { "interleave", required_argument, nullptr, 'i' },
    { "skew",       required_argument, nullptr, 'k' },
    { "size",       required_argument, nullptr, 'z' },
    { "fill",       required_argument, nullptr, 'F' },
    { "base",       required_argument, nullptr, 'b' },
    { "head0",      required_argument, nullptr, '0' },
    { "head1",      required_argument, nullptr, '1' },

    { "hex",              no_argument, nullptr, 'x' },
    { "force",            no_argument, nullptr, 'f' },
    { "label",      required_argument, nullptr, 'L' },
    { "data-copy",  required_argument, nullptr, 'D' },
    { "encoding",   required_argument, nullptr, 'e' },
    { "datarate",   required_argument, nullptr, 't' },

    { "debug",      optional_argument, nullptr, OPT_DEBUG },
    { "dec",              no_argument, &Options::opt.hex, 0 },
    { "hex-ish",          no_argument, &Options::opt.hex, 2 },
    { "calibrate",        no_argument, &Options::opt.calibrate, 1 },
    { "cpm",              no_argument, &Options::opt.cpm, 1 },
    { "resize",           no_argument, &Options::opt.resize, 1 },
    { "fm-overlap",       no_argument, &Options::opt.fmoverlap, 1 },
    { "multi-format",     no_argument, &Options::opt.multiformat, 1 },
    { "offsets",          no_argument, &Options::opt.offsets, 1 },
    { "abs-offsets",      no_argument, &Options::opt.absoffsets, 1 },
    { "no-offsets",       no_argument, &Options::opt.offsets, 0 },
    { "id-crc",           no_argument, &Options::opt.idcrc, 1 },
    { "no-gap2",          no_argument, &Options::opt.gap2, 0 },
    { "no-gap4b",         no_argument, &Options::opt.gap4b, 0 },
    { "no-gaps",          no_argument, &Options::opt.gaps, GAPS_NONE },
    { "gaps",             no_argument, &Options::opt.gaps, GAPS_CLEAN },
    { "clean-gaps",       no_argument, &Options::opt.gaps, GAPS_CLEAN },
    { "all-gaps",         no_argument, &Options::opt.gaps, GAPS_ALL },
    { "gap2",             no_argument, &Options::opt.gap2, 1 },
    { "keep-overlap",     no_argument, &Options::opt.keepoverlap, 1 },
    { "no-diff",          no_argument, &Options::opt.nodiff, 1 },
    { "no-copies",        no_argument, &Options::opt.maxcopies, 1 },
    { "no-duplicates",    no_argument, &Options::opt.nodups, 1 },
    { "no-dups",          no_argument, &Options::opt.nodups, 1 },
    { "no-check8k",       no_argument, &Options::opt.check8k, 0 },
    { "no-data",          no_argument, &Options::opt.nodata, 1 },
    { "no-wobble",        no_argument, &Options::opt.nowobble, 1 },
    { "no-mt",            no_argument, &Options::opt.mt, 0 },
    { "new-drive",        no_argument, &Options::opt.newdrive, 1 },
    { "old-drive",        no_argument, &Options::opt.newdrive, 0 },
    { "slow-step",        no_argument, &Options::opt.newdrive, 0 },
    { "no-signature",     no_argument, &Options::opt.nosig, 1 },
    { "no-zip",           no_argument, &Options::opt.nozip, 1 },
    { "no-cfa",           no_argument, &Options::opt.nocfa, 1 },
    { "no-identify",      no_argument, &Options::opt.noidentify, 1 },
    { "no-ttb",           no_argument, &Options::opt.nottb, 1},          // undocumented
    { "no-special",       no_argument, &Options::opt.nospecial, 1 },     // undocumented
    { "byte-swap",        no_argument, &Options::opt.byteswap, 1 },
    { "atom",             no_argument, &Options::opt.byteswap, 1 },
    { "ace",              no_argument, nullptr, OPT_ACE },
    { "mx",               no_argument, nullptr, OPT_MX },
    { "agat",             no_argument, nullptr, OPT_AGAT },
    { "quick",            no_argument, &Options::opt.quick, 1 },
    { "repair",           no_argument, &Options::opt.repair, 1},
    { "fix",              no_argument, &Options::opt.fix, 1 },
    { "align",            no_argument, &Options::opt.align, 1 },
    { "a1-sync",          no_argument, &Options::opt.a1sync, 1 },
    { "no-fix",           no_argument, &Options::opt.fix, 0 },
    { "no-fm",            no_argument, nullptr, OPT_NOFM },
    { "no-weak",          no_argument, &Options::opt.noweak, 1 },
    { "merge",            no_argument, &Options::opt.merge, 1 },
    { "trim",             no_argument, &Options::opt.trim, 1 },
    { "flip",             no_argument, &Options::opt.flip, 1 },
    { "legacy",           no_argument, &Options::opt.legacy, 1 },
    { "time",             no_argument, &Options::opt.time, 1 },          // undocumented
    { "tty",              no_argument, &Options::opt.tty, 1 },
    { "help",             no_argument, nullptr, 0 },

    { "log",        optional_argument, nullptr, OPT_LOG },
    { "gap-mask",   required_argument, nullptr, OPT_GAPMASK },
    { "max-copies", required_argument, nullptr, OPT_MAXCOPIES },
    { "max-splice-bits",required_argument, nullptr, OPT_MAXSPLICE },
    { "check8k",    optional_argument, nullptr, OPT_CHECK8K },
    { "rpm",        required_argument, nullptr, OPT_RPM },
    { "bytes",      required_argument, nullptr, OPT_BYTES },
    { "hdf",        required_argument, nullptr, OPT_HDF },
    { "prefer",     required_argument, nullptr, OPT_PREFER },
    { "order",      required_argument, nullptr, OPT_ORDER },
    { "step-rate",  required_argument, nullptr, OPT_STEPRATE },
    { "version",          no_argument, nullptr, OPT_VERSION },
    { "scale",      required_argument, nullptr, OPT_SCALE },
    { "pll-adjust", required_argument, nullptr, OPT_PLLADJUST },
    { "pll-phase",  required_argument, nullptr, OPT_PLLPHASE },

    { "normal-disk",                  no_argument, nullptr, OPT_NORMAL_DISK },     // undocumented. Expects disk as normal: all units (sectors, tracks, sides) have same size, sector ids form a sequence starting by 1.
    { "readstats",                    no_argument, nullptr, OPT_READSTATS },       // undocumented. Looking for good data by the reading statistics. Requires RDSK format image.
    { "paranoia",                     no_argument, nullptr, OPT_PARANOIA },        // undocumented. (Multi good data). Rescues floppy image assuming that a good CRC does not necessarily mean good data. It implies readstats thus requires using RDSK format image. It also sets stability_level as 5 if not specified.
    { "stability-level",        required_argument, nullptr, OPT_STABILITY_LEVEL }, // undocumented.  // The count of samely read data of a sector which is considered stable. < 1 means only good data is stable (backward compatibility).
    { "skip-stable-sectors",          no_argument, nullptr, OPT_SKIP_STABLE_SECTORS }, // undocumented. in repair mode skip those sectors which are already rescued in destination.
    { "track-retries",          required_argument, nullptr, OPT_TRACK_RETRIES },   // undocumented. Amount of track retries. Each retry move the floppy drive head a bit.
    { "detect-devfs",           optional_argument, nullptr, OPT_DETECT_DEVFS },    // undocumented. Detect the device filesystem and if exists use its format.
    { "disk-retries",           required_argument, nullptr, OPT_DISK_RETRIES },    // undocumented. Amount of disk retries. If auto then do it while data improved.
    { "byte-tolerance-of-time", required_argument, nullptr, OPT_BYTE_TOLERANCE_OF_TIME}, // undocumented. Two things are considered at same location if their location differs <= this value. Default is 64.
    { "fdraw-rescue-mode",            no_argument, nullptr, OPT_FDRAW_RESCUE_MODE}, // undocumented. Use the rescue method in fdrawsys_dev. Default is using the all-in method.
    { "unhide-first-sector-by-track-end-sector", no_argument, nullptr, OPT_UNHIDE_FIRST_SECTOR_BY_TRACK_END_SECTOR }, // undocumented. Unhide track starting sector by track ending sector (useful when track ending sector hides track starting sector). Default is false.

    { nullptr, 0, nullptr, 0 }
};

static char short_options[] = "?nmdvfLxb:c:h:s:H:r:R:g:i:k:z:0:1:D:";

// Macro by https://cfengine.com/blog/2021/optional-arguments-with-getopt-long/
#define OPTIONAL_ARGUMENT_IS_PRESENT \
    ((optarg == nullptr && optind < argc_ && argv_[optind][0] != '-') \
     ? (optarg = argv_[optind++]) != nullptr \
     : (optarg != nullptr))

bool BadValue(const char* pcszName_)
{
    util::cout << "Invalid " << pcszName_ << " value '" << optarg << "'\n";
    return false;
}

bool ParseCommandLine(int argc_, char* argv_[])
{
    int arg;
    opterr = 1;

    while ((arg = getopt_long(argc_, argv_, short_options, long_options, nullptr)) != -1)
    {
        switch (arg)
        {
        case 'c':
            util::str_range(optarg, Options::opt.range.cyl_begin, Options::opt.range.cyl_end);

            // -c0 is shorthand for -c0-0
            if (Options::opt.range.cyls() == 0)
                Options::opt.range.cyl_end = 1;
            break;

        case 'h':
        {
            auto heads = util::str_value<int>(optarg);
            if (heads > MAX_DISK_HEADS)
                throw util::exception("invalid head count/select '", optarg, "', expected 0-", MAX_DISK_HEADS);

            Options::opt.range.head_begin = (heads == 1) ? 1 : 0;
            Options::opt.range.head_end = (heads == 0) ? 1 : 2;
            break;
        }

        case 's':
            Options::opt.sectors = util::str_value<long>(optarg);
            break;

        case 'H':
            Options::opt.hardsectors = util::str_value<int>(optarg);
            if (Options::opt.hardsectors <= 1)
                throw util::exception("invalid hard-sector count '", optarg, "'");
            break;

        case 'r':
            Options::opt.retries = RetryPolicy(util::str_value<int>(optarg, true));
            break;

        case 'R':
            Options::opt.rescans = RetryPolicy(util::str_value<int>(optarg, true));
            break;

        case 'n':   Options::opt.noformat = 1; break;
        case 'm':   Options::opt.minimal = 1; break;

        case 'b':   Options::opt.base = util::str_value<int>(optarg); break;
        case 'z':   Options::opt.size = util::str_value<int>(optarg); break;
        case 'g':   Options::opt.gap3 = util::str_value<int>(optarg); break;
        case 'i':   Options::opt.interleave = util::str_value<int>(optarg); break;
        case 'k':   Options::opt.skew = util::str_value<int>(optarg); break;
        case 'D':   Options::opt.datacopy = util::str_value<int>(optarg); break;

        case 'F':
            Options::opt.fill = util::str_value<int>(optarg);
            if (Options::opt.fill > 255)
                throw util::exception("invalid fill value '", optarg, "', expected 0-255");
            break;
        case '0':
            Options::opt.head0 = util::str_value<int>(optarg);
            if (Options::opt.head0 > 1)
                throw util::exception("invalid head0 value '", optarg, "', expected 0 or 1");
            break;
        case '1':
            Options::opt.head1 = util::str_value<int>(optarg);
            if (Options::opt.head1 > 1)
                throw util::exception("invalid head1 value '", optarg, "', expected 0 or 1");
            break;

        case 't':
            Options::opt.datarate = datarate_from_string(optarg);
            if (Options::opt.datarate == DataRate::Unknown)
                throw util::exception("invalid data rate '", optarg, "'");
            break;

        case 'e':
            Options::opt.encoding = encoding_from_string(optarg);
            if (Options::opt.encoding == Encoding::Unknown)
                throw util::exception("invalid encoding '", optarg, "'");
            break;

        case 'd':   Options::opt.step = 2; break;
        case 'f':   ++Options::opt.force; break;
        case 'v':   ++Options::opt.verbose; break;
        case 'x':   Options::opt.hex = 1; break;

        case 'L':   Options::opt.label = optarg; break;

        case OPT_LOG:
            util::log.open(optarg ? optarg : "samdisk.log");
            if (util::log.bad())
                throw util::exception("failed to open log file for writing");
            util::cout.file = &util::log;
            break;

        case OPT_ORDER:
        {
            auto str = util::lowercase(optarg);
            if (str == std::string("cylinders").substr(0, str.length()))
                Options::opt.cylsfirst = 1;
            else if (str == std::string("heads").substr(0, str.length()))
                Options::opt.cylsfirst = 0;
            else
                throw util::exception("invalid order type '", optarg, "', expected 'cylinders' or 'heads'");
            break;
        }

        case OPT_PREFER:
        {
            auto str = util::lowercase(optarg);
            if (str == std::string("track").substr(0, str.length()))
                Options::opt.prefer = PreferredData::Track;
            else if (str == std::string("bitstream").substr(0, str.length()))
                Options::opt.prefer = PreferredData::Bitstream;
            else if (str == std::string("flux").substr(0, str.length()))
                Options::opt.prefer = PreferredData::Flux;
            else
                throw util::exception("invalid data type '", optarg, "', expected track/bitstream/flux");
            break;
        }

        case OPT_ACE:   Options::opt.encoding = Encoding::Ace;   break;
        case OPT_MX:    Options::opt.encoding = Encoding::MX;    break;
        case OPT_AGAT:  Options::opt.encoding = Encoding::Agat;  break;
        case OPT_NOFM:  Options::opt.encoding = Encoding::MFM;   break;

        case OPT_GAPMASK:
            Options::opt.gapmask = util::str_value<int>(optarg);
            break;
        case OPT_MAXCOPIES:
            Options::opt.maxcopies = util::str_value<int>(optarg);
            if (!Options::opt.maxcopies)
                throw util::exception("invalid data copy count '", optarg, "', expected >= 1");
            break;
        case OPT_MAXSPLICE:
            Options::opt.maxsplice = util::str_value<int>(optarg);
            break;
        case OPT_CHECK8K:
            Options::opt.check8k = !optarg ? 1 : util::str_value<int>(optarg);
            break;
        case OPT_RPM:
            // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats, rpm values.
            // This parameter is used for not allowing too slow or too fast disk
            // thus accepting it in a range.
            Options::opt.rpm = util::str_value<int>(optarg);
            if (Options::opt.rpm < 150 || Options::opt.rpm > 360)
                throw util::exception("invalid rpm '", optarg, "', expected between 150 and 360");
            break;
        case OPT_HDF:
            Options::opt.hdf = util::str_value<int>(optarg);
            if (Options::opt.hdf != 10 && Options::opt.hdf != 11)
                throw util::exception("invalid HDF version '", optarg, "', expected 10 or 11");
            break;
        case OPT_SCALE:
            Options::opt.scale = util::str_value<int>(optarg);
            break;
        case OPT_PLLADJUST:
            Options::opt.plladjust = util::str_value<int>(optarg);
            if (Options::opt.plladjust <= 0 || Options::opt.plladjust > MAX_PLL_ADJUST)
                throw util::exception("invalid pll adjustment '", optarg, "', expected 1-", MAX_PLL_ADJUST);
            break;
        case OPT_PLLPHASE:
            Options::opt.pllphase = util::str_value<int>(optarg);
            if (Options::opt.pllphase <= 0 || Options::opt.pllphase > MAX_PLL_PHASE)
                throw util::exception("invalid pll phase '", optarg, "', expected 1-", MAX_PLL_PHASE);
            break;
        case OPT_STEPRATE:
            Options::opt.steprate = util::str_value<int>(optarg);
            if (Options::opt.steprate > 15)
                throw util::exception("invalid step rate '", optarg, "', expected 0-15");
            break;

        case OPT_BYTES:
            util::str_range(optarg, Options::opt.bytes_begin, Options::opt.bytes_end);
            break;

        case OPT_DEBUG:
            if (OPTIONAL_ARGUMENT_IS_PRESENT)
                Options::opt.debug = util::str_value<int>(optarg);
            else
                Options::opt.debug = 1;
            if (Options::opt.debug < 0)
                throw util::exception("invalid debug level '", optarg, "', expected >= 0");
            break;

        case OPT_VERSION:
            LongVersion();
            return false;

        case OPT_TRACK_RETRIES:
        {
            auto str = util::lowercase(optarg);
            if (str == std::string("auto").substr(0, str.length()))
                Options::opt.track_retries = DISK_RETRY_AUTO;
            else
                Options::opt.track_retries = util::str_value<int>(optarg);
            break;
        }

        case OPT_DISK_RETRIES:
        {
            auto str = util::lowercase(optarg);
            if (str == std::string("auto").substr(0, str.length()))
                Options::opt.disk_retries = DISK_RETRY_AUTO;
            else
                Options::opt.disk_retries = util::str_value<int>(optarg);
            break;
        }

        case OPT_NORMAL_DISK:
            Options::opt.normal_disk = true;
            break;

        case OPT_READSTATS:
            Options::opt.readstats = true;
            break;

        case OPT_DETECT_DEVFS:
            if (OPTIONAL_ARGUMENT_IS_PRESENT)
                Options::opt.detect_devfs = optarg;
            else
                Options::opt.detect_devfs = DETECT_FS_AUTO;
            if (!fileSystemWrappers.IsValidFSName(Options::opt.detect_devfs))
                throw util::exception("invalid detect-devfs '", optarg, "', it must be auto or the name of one of the supported filesystems");
            break;

        case OPT_PARANOIA:
            Options::opt.paranoia = true;
            break;

        case OPT_SKIP_STABLE_SECTORS:
            Options::opt.skip_stable_sectors = true;
            break;

        case OPT_BYTE_TOLERANCE_OF_TIME:
            // This parameter is used for matching sectors if difference of their time is within this tolerance.
            Options::opt.byte_tolerance_of_time = util::str_value<int>(optarg);
            if (Options::opt.byte_tolerance_of_time < 0)
                throw util::exception("invalid byte-tolerance-of-time '", optarg, "', expected >= 0");
            if (Options::opt.byte_tolerance_of_time > 127)
                MessageCPP(msgWarning, "byte-tolerance-of-time '", optarg, "' is > 127 but the driver will use 127 as its max value");
            break;

        case OPT_FDRAW_RESCUE_MODE:
            Options::opt.fdraw_rescue_mode = true;
            break;

        case OPT_UNHIDE_FIRST_SECTOR_BY_TRACK_END_SECTOR:
            Options::opt.unhide_first_sector_by_track_end_sector = true;
            break;

        case ':':
        case '?':   // error
            util::cout << '\n';
            return false;

            // long option return
        case 0:
            break;
#ifdef _DEBUG
        default:
            return false;
#endif
        }
    }

    // Fail if there are no non-option arguments
    if (optind >= argc_)
    {
        if (!Options::opt.verbose)
            Usage();

        // Allow -v to show the --version details
        LongVersion();
        return false;
    }

    // The command is the first argument
    char* pszCommand = argv_[optind];

    // Match against known commands
    for (int i = 0; i < cmdEnd; ++i)
    {
        if (!strcasecmp(pszCommand, aszCommands[i]))
        {
            // Fail if a command has already been set
            if (Options::opt.command)
                Usage();

            // Set the command and advance to the next argument position
            Options::opt.command = i;
            ++optind;
            break;
        }
    }

    if (Options::opt.absoffsets) Options::opt.offsets = 1;

    if (Options::opt.stability_level > 0 && (!Options::opt.readstats || !Options::opt.paranoia))
        throw util::exception("invalid usage of stability_level: it also requires readstats and paranoia parameters");
    if (Options::opt.paranoia)
    {
        if (!Options::opt.readstats)
            throw util::exception("invalid usage of paranoia: it also requires readstats parameter");
        if (Options::opt.stability_level <= 0)
        {
            Options::opt.stability_level = STABILITY_LEVEL_DEFAULT;
            Message(msgInfo, "The stability-level is not specified, using %d by default", Options::opt.stability_level);
//            util::cout << "The stability-level is not specified, using " << Options::opt.stability_level << " by default\n";
        }
    }

    return true;
}


enum { argNone, argBlock, argHDD, argBootSector, argDisk, ARG_COUNT };

int GetArgType(const std::string& arg)
{
    if (arg.empty())
        return argNone;

    if (IsBootSector(arg))
        return argBootSector;

    if (IsRecord(arg))
        return argDisk;

    if (IsFloppy(arg))
        return argDisk;

    if (BlockDevice::IsRecognised(arg))
        return argBlock;

    if (IsHddImage(arg))
        return argHDD;

    // Assume a disk or image.
    return argDisk;
}

int main(int argc_, char* argv_[])
{
    auto start_time = std::chrono::system_clock::now();

#ifdef _WIN32
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

    SetUnhandledExceptionFilter(CrashDumpUnhandledExceptionFilter);

#ifndef _DEBUG
    // Check if we've been run from GUI mode with no arguments
    if (!IsConsoleWindow() && util::is_stdout_a_tty() && argc_ == 1)
    {
        FreeConsole();

        char szPath[MAX_PATH];
        GetModuleFileName(GetModuleHandle(NULL), szPath, ARRAYSIZE(szPath));

        auto strCommand = std::string("/k \"") + szPath + "\" --help";
        GetEnvironmentVariable("COMSPEC", szPath, ARRAYSIZE(szPath));

        auto ret = static_cast<int>(reinterpret_cast<ULONG_PTR>(
            ShellExecute(NULL, "open", szPath, strCommand.c_str(), NULL, SW_NORMAL)));

        // Fall back on the old message if it failed
        if (ret < 32)
            MessageBox(nullptr, "I'm a console-mode utility, please run me from a Command Prompt!", "SAMdisk", MB_OK | MB_ICONSTOP);

        return 0;
    }
#endif // _DEBUG
#endif // _WIN32

    bool f = false;

    try
    {
        if (!ParseCommandLine(argc_, argv_))
            return 1;

        // Read at most two non-option command-line arguments
        if (optind < argc_) strncpy(Options::opt.szSource, argv_[optind++], arraysize(Options::opt.szSource) - 1);
        if (optind < argc_) strncpy(Options::opt.szTarget, argv_[optind++], arraysize(Options::opt.szTarget) - 1);
        if (optind < argc_) Usage();

        int nSource = GetArgType(Options::opt.szSource);
        int nTarget = GetArgType(Options::opt.szTarget);

        switch (Options::opt.command)
        {
        case cmdCopy:
        {
            if (nSource == argNone || nTarget == argNone)
                Usage();

            if (nSource == argDisk && IsTrinity(Options::opt.szTarget))
                f = Image2Trinity(Options::opt.szSource, Options::opt.szTarget);          // file/image -> Trinity
            else if ((nSource == argBlock || nSource == argDisk) && (nTarget == argDisk || nTarget == argHDD /*for .raw*/))
                f = ImageToImage(Options::opt.szSource, Options::opt.szTarget);           // image -> image
            else if ((nSource == argHDD || nSource == argBlock) && nTarget == argHDD)
                f = Hdd2Hdd(Options::opt.szSource, Options::opt.szTarget);                // hdd -> hdd
            else if (nSource == argBootSector && nTarget == argDisk)
                f = Hdd2Boot(Options::opt.szSource, Options::opt.szTarget);               // boot -> file
            else if (nSource == argDisk && nTarget == argBootSector)
                f = Boot2Hdd(Options::opt.szSource, Options::opt.szTarget);               // file -> boot
            else if (nSource == argBootSector && nTarget == argBootSector)
                f = Boot2Boot(Options::opt.szSource, Options::opt.szTarget);              // boot -> boot
            else
                Usage();

            break;
        }

        case cmdList:
        {
            if (nTarget != argNone)
                Usage();

            if (nSource == argNone)
                f = ListDrives(Options::opt.verbose);
            else if (nSource == argHDD || nSource == argBlock)
                f = ListRecords(Options::opt.szSource);
            else if (nSource == argDisk)
                f = DirImage(Options::opt.szSource);
            else
                Usage();

            break;
        }

        case cmdDir:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();
#if 0
            if (nSource == argFloppy)
                f = DirFloppy(Options::opt.szSource);
            else
#endif
                if (nSource == argHDD)
                    f = ListRecords(Options::opt.szSource);
                else if (nSource == argDisk)
                    f = DirImage(Options::opt.szSource);
                else
                    Usage();

            break;
        }

        case cmdScan:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            if (nSource == argBlock || nSource == argDisk)
                f = ScanImage(Options::opt.szSource, Options::opt.range);
            else
                Usage();

            break;
        }

        case cmdUnformat:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            // Don't write disk signatures during any formatting
            Options::opt.nosig = true;

            if (nSource == argHDD)
                f = FormatHdd(Options::opt.szSource);
            else if (IsRecord(Options::opt.szSource))
                f = FormatRecord(Options::opt.szSource);
            else if (nSource == argDisk)
                f = UnformatImage(Options::opt.szSource, Options::opt.range);
            else
                Usage();

            break;
        }

        case cmdFormat:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            if (nSource == argHDD)
                FormatHdd(Options::opt.szSource);
            else if (nSource == argBootSector)
                FormatBoot(Options::opt.szSource);
            else if (nSource == argDisk)
                FormatImage(Options::opt.szSource, Options::opt.range);
            else
                Usage();

            break;
        }

        case cmdVerify:
            throw std::logic_error("verify command not yet implemented");

        case cmdCreate:
        {
            if (nSource == argNone)
                Usage();

            if (nSource == argHDD && IsHddImage(Options::opt.szSource) && (nTarget != argNone || Options::opt.sectors != -1))
                f = CreateHddImage(Options::opt.szSource, util::str_value<int>(Options::opt.szTarget));
            else if (nSource == argDisk && nTarget == argNone)
                f = CreateImage(Options::opt.szSource, Options::opt.range);
            else
                Usage();

            break;
        }

        case cmdInfo:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            if (nSource == argHDD || nSource == argBlock)
                f = HddInfo(Options::opt.szSource, Options::opt.verbose);
            else if (nSource == argDisk)
                f = ImageInfo(Options::opt.szSource);
            else
                Usage();

            break;
        }

        case cmdView:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            if (nSource == argHDD || nSource == argBlock)
                f = ViewHdd(Options::opt.szSource, Options::opt.range);
            else if (nSource == argBootSector)
                f = ViewBoot(Options::opt.szSource, Options::opt.range);
            else if (nSource == argDisk)
                f = ViewImage(Options::opt.szSource, Options::opt.range);
            else
                Usage();

            break;
        }

        case cmdRpm:
        {
            if (nSource == argNone || nTarget != argNone)
                Usage();

            if (nSource == argDisk)
                f = DiskRpm(Options::opt.szSource);
            else
                Usage();

            break;
        }

        case cmdVersion:
        {
            if (nSource != argNone || nTarget != argNone)
                Usage();

            LongVersion();
            f = true;
            break;
        }

        default:
            Usage();
            break;
        }
    }
    catch (std::string & e)
    {
        util::cout << "Error: " << colour::RED << e << colour::none << '\n';
    }
    catch (util::exception & e)
    {
        util::cout << colour::RED << "Error: " << e.what() << colour::none << '\n';
    }
    catch (std::system_error & e)
    {
        util::cout << colour::RED << "Error: " << e.what() << colour::none << '\n';
    }
    catch (std::logic_error & e)
    {
        util::cout << colour::RED << "Error: " << e.what() << colour::none << '\n';
    }
    catch (std::exception & e)
    {
        util::cout << colour::RED << "Error: " << e.what() << colour::none << '\n';
    }

    if (Options::opt.time)
    {
        auto end_time = std::chrono::system_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        util::cout << "Elapsed time: " << elapsed_ms << "ms\n";
    }

    util::cout << colour::none << "";
    util::log.close();

    return f ? 0 : 1;
}
