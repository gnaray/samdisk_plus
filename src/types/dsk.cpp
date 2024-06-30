// Extended DSK (EDSK) specification:
//  http://www.cpctech.org.uk/docs/extdsk.html
//
// EDSK extensions for copy-protected disks:
//  http://simonowen.com/misc/extextdsk.txt
//
// John Elliot's rate+encoding extension:
//  http://groups.google.com/group/comp.sys.sinclair/msg/80e4c2d1403ea65c

#ifdef _WIN32
#include "PlatformConfig.h" // For disabling strncpy deprecation.
#endif
#include "Options.h"
#include "DiskUtil.h"
#include "types/dsk.h"
#include "IBMPC.h"
#include "Disk.h"
#include "MemFile.h"
#include "Util.h"

#include <memory>
#include <algorithm>

// ToDo: separate EDSK and DSK support, as EDSK alone is complicated enough!

static auto& opt_fix = getOpt<int>("fix");
static auto& opt_legacy = getOpt<int>("legacy");
static auto& opt_maxcopies = getOpt<int>("maxcopies");
static auto& opt_offsets = getOpt<int>("offsets");
static auto& opt_paranoia = getOpt<bool>("paranoia");
static auto& opt_readstats = getOpt<bool>("readstats");

#define DSK_SIGNATURE           "MV - CPC"
#define EDSK_SIGNATURE          "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
// Must be same long as EDSK_SIGNATURE and first letter must differ from other first letter of other signatures.
#define RDSK_SIGNATURE          "REPAIRER CPC DSK File\r\nDisk-Info\r\n"
#define EDSK_TRACK_SIG          "Track-Info\r\n"
#define EDSK_MAX_TRACK_SIZE     0xff00
#define RDSK_MAX_TRACK_SIZE     0x4000000 // Basically 4 GB would be the limit, but 8192 * 21 * 20 * 10 should be enough (max sector size * max sectors per track * max copies * floppy drive amount).
#define EDSK_OFFSETS_SIG        "Offset-Info\r\n"
#define RDSK_READSTATS_SIG      "ReadStats-Info\r\n"

const int EDSK_DEFAULT_GAP3 = 0x4e;     // default EDSK gap3 size
const int EDSK_DEFAULT_SIZE = 2;        // default EDSK sector size (pretty redundant now)
const int CPC_DEFAULT_GAP3 = 0x52;      // default CPC gap size

struct EDSK_HEADER
{
    char szSignature[34];       // one of the signatures above, depending on DSK/EDSK
    char szCreator[14];         // name of creator (utility/emulator)
    uint8_t bTracks;
    uint8_t bSides;
    uint8_t abTrackSize[2];     // fixed track size (DSK only)
};

struct EDSK_TRACK
{
    char signature[13];         // Track-Info\r\n\0
    uint8_t unused[3];
    uint8_t track;
    uint8_t side;
    uint8_t rate;               // 0=unknown, 1=250/300K, 2=500K, 3=1M
    uint8_t encoding;           // 0=unknown, 1=FM, 2=MFM
    uint8_t size;
    uint8_t sectors;
    uint8_t gap3;
    uint8_t fill;
};

struct EDSK_SECTOR
{
    uint8_t track, side, sector, size;
    uint8_t status1, status2;
    uint8_t datalow, datahigh;
};

struct EDSK_OFFSETS
{
    char signature[14];         // Offset-Info\r\n\0
    uint8_t flags;              // reserved, must be zero
};

struct EDSK_READSTATS_HEADER
{
    char signature[17];         // ReadStats-Info\r\n\0
    uint8_t flags;              // reserved, must be zero
};

class EdskReadstatsElement
{
public:
    EdskReadstatsElement() = default;

    uint16_t readAttempts = 0;
    VectorX<uint16_t> dataReadCount{};

    static size_t DataReadCountByteSize()
    {
        return sizeof(decltype(dataReadCount)::value_type);
    }

    int DataReadCountDataByteSize() const
    {
        return static_cast<int>(EdskReadstatsElement::DataReadCountByteSize()) * dataReadCount.size();
    }

    void TransferFromFile(MemFile& file, const CylHead& cylhead, Sector& sector)
    {
        uint16_t numDatas;
        // Do not call read(buf, len, count) because it can lie.
        if (!file.read(&readAttempts, sizeof(readAttempts))
            || !file.read(&numDatas, sizeof(numDatas))
            )
            throw util::exception("short file reading (readstats) ", cylhead, " sector ", sector.header.sector);
        dataReadCount.resize(numDatas);
        if (numDatas > 0)
        {
            if (!file.read(dataReadCount.data(), DataReadCountDataByteSize()))
                throw util::exception("short file reading (readstats) ", cylhead, " sector ", sector.header.sector);
        }
    }

    void TransferToSector(const CylHead& /*cylhead*/, Sector& sector) const
    {
        const int numDatas = dataReadCount.size();
        if (numDatas != sector.copies())
        {
            const auto hasMaxCopies = sector.copies() == opt_maxcopies;
            if (hasMaxCopies)
                MessageCPP(msgWarning, sector, " readstats contains more copies (", numDatas, ") than max-copies option (",
                    opt_maxcopies, ") allows, ignoring excess readstats copies");
            if (!opt_paranoia && (!hasMaxCopies || sector.has_good_data()))
                throw util::exception("readstats info does not match track info at sector (", sector,
                    "), specifying paranoia parameter should work");
        }
        sector.resize_data(numDatas);
        if (numDatas > 0)
        {
            // Process each data instance.
            for (auto instance = 0; instance < numDatas; ++instance)
                if (instance < opt_maxcopies)
                    sector.set_read_stats(instance, DataReadStats(util::letoh(dataReadCount[instance])));
        }
        sector.set_read_attempts(util::letoh(readAttempts));
    }

    static EdskReadstatsElement CreateTransferFromFile(MemFile& file, const CylHead& cylhead, Sector& sector)
    {
        EdskReadstatsElement edskReadstatsElement;
        edskReadstatsElement.TransferFromFile(file, cylhead, sector);
        return edskReadstatsElement;
    }

    static void TransferFromFileToSector(MemFile& file, const CylHead& cylhead, Sector& sector)
    {
        const EdskReadstatsElement edskReadstatsElement = CreateTransferFromFile(file, cylhead, sector);
        edskReadstatsElement.TransferToSector(cylhead, sector);
    }

    void TransferToFile(FILE* file) const
    {
        const uint16_t numDatas = static_cast<uint16_t>(dataReadCount.size());
        const auto readAttemptsSize = sizeof(readAttempts);
        const auto numDatasSize = sizeof(numDatas);
        if (fwrite(&readAttempts, readAttemptsSize, 1, file) != 1
            || fwrite(&numDatas, numDatasSize, 1, file) != 1)
            throw util::exception("write error (readstats)");
        if (numDatas > 0)
        {
            if (fwrite(dataReadCount.data(), EdskReadstatsElement::DataReadCountByteSize(), numDatas, file) != numDatas)
                throw util::exception("write error (readstats)");
        }
    }

    void TransferFromSector(const CylHead& /*cylhead*/, const Sector& sector, int numCopies = -1)
    {
        readAttempts = lossless_static_cast<uint16_t>(util::htole(sector.read_attempts()));
        int numDatas = numCopies < 0 ? sector.copies() : numCopies;
        dataReadCount.resize(0);
        dataReadCount.reserve(numDatas);
        // Process each data instance.
        for (auto instance = 0; instance < numDatas; ++instance)
            dataReadCount.push_back(lossless_static_cast<uint16_t>(util::htole(sector.data_copy_read_stats(instance).ReadCount())));
    }

    static EdskReadstatsElement CreateTransferFromSector(const CylHead& cylhead, const Sector& sector, int numCopies = -1)
    {
        EdskReadstatsElement edskReadstatsElement;
        edskReadstatsElement.TransferFromSector(cylhead, sector, numCopies);
        return edskReadstatsElement;
    }

    static void TransferFromSectorToFile(FILE* file, const CylHead& cylhead, const Sector& sector)
    {
        const EdskReadstatsElement edskReadstatsElement = CreateTransferFromSector(cylhead, sector);
        edskReadstatsElement.TransferToFile(file);
    }
};

#define SR1_SUCCESS                     0x00
#define SR1_CANNOT_FIND_ID_ADDRESS      0x01
#define SR1_WRITE_PROTECT_DETECTED      0x02
#define SR1_CANNOT_FIND_SECTOR_ID       0x04
#define SR1_RESERVED1                   0x08
#define SR1_OVERRUN                     0x10
#define SR1_CRC_ERROR                   0x20
#define SR1_RESERVED2                   0x40
#define SR1_END_OF_CYLINDER             0x80

#define SR2_SUCCESS                     0x00
#define SR2_MISSING_ADDRESS_MARK        0x01    // data field not found
#define SR2_BAD_CYLINDER                0x02
#define SR2_SCAN_COMMAND_FAILED         0x04
#define SR2_SCAN_COMMAND_EQUAL          0x08
#define SR2_WRONG_CYLINDER_DETECTED     0x10
#define SR2_CRC_ERROR_IN_SECTOR_DATA    0x20
#define SR2_SECTOR_WITH_DELETED_DATA    0x40
#define SR2_RESERVED                    0x80


bool ReadDSK(MemFile& file, std::shared_ptr<Disk>& disk, int edsk_version)
{
    // For RDSK (edsk version >= 2) the size of abHeader must have been increased in order
    // to have enough space for storing 4 byte long track sizes for MAX TRACKS (128) tracks.
    // Since abHeader has dynamic size, it is now vector instead of plain array.
    VectorX<uint8_t> ab(edsk_version >= 2 ? 1024 : 256);

    if (!file.rewind() || !file.read(ab.data(), ab.size()))
        return false;

    auto peh = reinterpret_cast<EDSK_HEADER*>(ab.data());
    if (memcmp(peh->szSignature, EDSK_SIGNATURE, 8) &&  // match: "EXTENDED" only
        memcmp(peh->szSignature, DSK_SIGNATURE, 8)) // match: "MV - CPC"
    {   // RDSK is EDSK version 2, so version < 2 can not read RDSK.
        if (edsk_version < 2 || memcmp(peh->szSignature, RDSK_SIGNATURE, 8))
        return false;
    }

    auto pbIndex = reinterpret_cast<const uint8_t*>(peh + 1);
    auto pbIndex32 = reinterpret_cast<const uint32_t*>(pbIndex); // For RDSK.
    auto max_cyls = (ab.size() - intsizeof(EDSK_HEADER)) / MAX_SIDES;
    bool fEDSK = peh->szSignature[0] == EDSK_SIGNATURE[0] || peh->szSignature[0] == RDSK_SIGNATURE[0];

    // Warn if the deprecated 'random data errors' flag is set
    if (peh->bSides & 0x80)
    {
        Message(msgWarning, "ignoring deprecated 'random data errors' flag");
        peh->bSides &= ~0x80;
    }

    uint8_t cyls = peh->bTracks;
    uint8_t heads = peh->bSides;

    if (heads > MAX_SIDES)
        throw util::exception("invalid head count (", heads, ")");
    else if (cyls > MAX_TRACKS || cyls > max_cyls)
        throw util::exception("invalid cylinder count (", cyls, ")");

    disk->metadata()["creator"] = util::trim(std::string(peh->szCreator, sizeof(peh->szCreator)));

    MEMORY mem(edsk_version >= 2 ? RDSK_MAX_TRACK_SIZE : EDSK_MAX_TRACK_SIZE);
    bool fWarned6K = false;

    for (uint8_t cyl = 0; cyl < cyls; ++cyl)
    {
        for (uint8_t head = 0; head < heads; ++head)
        {
            CylHead cylhead(cyl, head);
            static const DataRate abEDSKRates[] = { DataRate::_250K, DataRate::_250K, DataRate::_500K, DataRate::_1M };

            // Remember the start position
            auto uTrackStart = file.tell();

            // EDSK supports variable track size, DSK has it fixed for the entire image
            auto uTrackSize = fEDSK ? (*pbIndex++ << 8) : ((peh->abTrackSize[1] << 8) | peh->abTrackSize[0]);
            if (edsk_version >= 2)
            {
                uTrackSize = lossless_static_cast<int>(util::letoh(*(pbIndex32++)));
            }

            // EDSK doesn't store blank tracks, as indicated by zero size in the index
            if (fEDSK && !uTrackSize)
            {
                disk->write(cylhead, Track());
                continue;
            }

            // SugarBox (<= 0.22) doesn't write blank tracks at the end of the disk.
            if (cyl >= 40 && file.tell() == file.size())
            {
                Message(msgWarning, "%s track header is missing, assuming blank track", strCH(cyl, head).c_str());
                disk->write(cylhead, Track());
                continue;
            }

            EDSK_TRACK th;
            if (!file.read(&th, sizeof(th)))
                throw util::exception("short file reading ", cylhead);

            // Check for "Track-Info", tolerating \r\n missing in bad images (Addams Family)
            if (memcmp(th.signature, EDSK_TRACK_SIG, 10))
                throw util::exception("track signature missing on ", cylhead);

            // Warn if the track sector size is invalid
            if (!fEDSK && th.size > SIZE_MASK_765)
                Message(msgWarning, "invalid sector size code (%s) on %s", SizeStr(th.size), strCH(cyl, head).c_str());

            if (th.track != cyl || th.side != head)
            {
                if (th.track != cyl) Message(msgWarning, "header track %s != cyl %s", CylStr(th.track), CylStr(cyl));
                else Message(msgWarning, "header side %s != head %s", HeadStr(th.side), HeadStr(head));
            }

            // SAMdisk initially implemented John Elliot's rate+encoding extension in the wrong place.
            // If we find values in the wrong place, and none set in the right place, move them over!
            if (th.rate == 0 && th.unused[0]) { th.rate = th.unused[0]; th.unused[0] = 0; }
            if (th.encoding == 0 && th.unused[1]) { th.encoding = th.unused[1]; th.unused[1] = 0; }

            // Warn if the unused fields are used
            if (th.unused[0] || th.unused[1] || th.unused[2])
                Message(msgWarning, "unused fields are non-zero (%02X %02X %02X) on %s", th.unused[0], th.unused[1], th.unused[2], strCH(cyl, head).c_str());

            DataRate datarate = (th.rate >= 1 && th.rate <= 3) ? abEDSKRates[th.rate] : DataRate::_250K;
            Encoding encoding = (th.encoding != 1) ? Encoding::MFM : Encoding::FM;
            Track track(th.sectors);

            // Determine the track header size, and the start of the data beyond it
            // Round the header size to the next 256-byte boundary
            int uSectorHeaders = (intsizeof(EDSK_TRACK) + th.sectors * intsizeof(EDSK_SECTOR) + 0xff) & ~0xff;
            uSectorHeaders -= sizeof(EDSK_TRACK);
            int uMinimum = th.sectors * sizeof(EDSK_SECTOR);

            // CPCDiskXP writes a truncated track header if the final track is blank.
            // Check if only the minimum header size has been supplied.
            if (uMinimum < uSectorHeaders && (file.size() - file.tell()) == uMinimum)
                Message(msgWarning, "%s track header is shorter than index size", strCH(cyl, head).c_str());
            else if (!file.read(mem, uSectorHeaders))
                throw util::exception("short file reading ", cylhead, " sector headers");

            for (uint8_t sec = 0; sec < th.sectors; ++sec)
            {
                auto ps = &reinterpret_cast<EDSK_SECTOR*>(mem.pb)[sec];
                Sector sector(datarate, encoding, Header(ps->track, ps->side, ps->sector, ps->size));

                // Set gap3 on all but final setor
                if (sec < th.sectors - 1)
                {
                    sector.gap3 = th.gap3;

                    // If the sector uses a default gap3 size, clear it. This allows overrides by stored gap3
                    // in ALLGAPS images, and also prevents the default value showing up in track scans.
                    // If the same image is written back, any unset gap3 will be restored to the same value.
                    if (sector.gap3 == EDSK_DEFAULT_GAP3 || sector.gap3 == CPC_DEFAULT_GAP3)
                        sector.gap3 = 0;
                }

                uint8_t status1 = ps->status1, status2 = ps->status2;
                bool id_crc_error = (status1 & SR1_CRC_ERROR) != 0;
                bool data_not_found = (status2 & SR2_MISSING_ADDRESS_MARK) != 0;
                bool data_crc_error = (status2 & SR2_CRC_ERROR_IN_SECTOR_DATA) != 0;
                bool deleted_dam = (status2 & SR2_SECTOR_WITH_DELETED_DATA) != 0;

                // uPD765 sets both id and data flags for data CRC errors
                if (data_crc_error)
                    id_crc_error = false;
                else if (id_crc_error)
                    sector.set_badidcrc();

                // Check for an impossible flag combo, used for storing placeholder sectors (Logo Professor test)
                if (status1 & SR1_CRC_ERROR && status2 & SR2_MISSING_ADDRESS_MARK)
                {
                    Message(msgWarning, "unsupported placeholder sector on %s", strCHSR(cyl, head, sec, sector.header.sector).c_str());
                    status2 &= ~SR2_MISSING_ADDRESS_MARK;
                }

                // Check for bits that shouldn't be set in the status bytes
                if ((status1 & (SR1_WRITE_PROTECT_DETECTED | SR1_RESERVED1 | SR1_RESERVED2)) ||
                    (status2 & (SR2_SCAN_COMMAND_FAILED | SR2_SCAN_COMMAND_EQUAL | SR2_RESERVED)))
                {
                    Message(msgWarning, "invalid status (ST1=%02X ST2=%02X) for %s", status1, status2, strCHSR(cyl, head, sec, sector.header.sector).c_str());

                    // Clear the unexpected bits and continue
                    status1 &= ~(SR1_WRITE_PROTECT_DETECTED | SR1_RESERVED1 | SR1_RESERVED2);
                    status2 &= ~(SR2_SCAN_COMMAND_FAILED | SR2_SCAN_COMMAND_EQUAL | SR2_RESERVED);
                }

                // Match expected flag combinations, to identify unusual combinations that could indicate a bogus dump
                if (!(
                    (status1 == SR1_SUCCESS && status2 == SR2_SUCCESS) ||                   // normal data
                    (status1 == SR1_SUCCESS && status2 == SR2_SECTOR_WITH_DELETED_DATA) ||  // deleted data
                    (status1 == SR1_END_OF_CYLINDER && status2 == SR2_SUCCESS) ||                   // end of track?
                    (status1 == SR1_CRC_ERROR && status2 == SR2_SUCCESS) ||                 // id crc error
                    (status1 == SR1_CRC_ERROR && status2 == SR2_CRC_ERROR_IN_SECTOR_DATA) ||    // normal data CRC error
                    (status1 == SR1_CRC_ERROR && status2 == (SR2_CRC_ERROR_IN_SECTOR_DATA | SR2_SECTOR_WITH_DELETED_DATA)) ||   // deleted data CRC error
                    (status1 == SR1_CANNOT_FIND_ID_ADDRESS && status2 == SR2_MISSING_ADDRESS_MARK) ||       // data field missing (some FDCs set AM in ST1)
                    (status1 == SR1_SUCCESS && status2 == SR2_MISSING_ADDRESS_MARK) ||      // data field missing (some FDCs don't)
                    (status1 == SR1_CANNOT_FIND_SECTOR_ID && status2 == SR2_SUCCESS) ||                 // CHRN mismatch
                    (status1 == SR1_CANNOT_FIND_SECTOR_ID && status2 == SR2_WRONG_CYLINDER_DETECTED)        // CHRN mismatch, including wrong cylinder
                    ))
                {
                    Message(msgWarning, "unusual status flags (ST1=%02X ST2=%02X) for %s", status1, status2, strCHSR(cyl, head, sec, sector.header.sector).c_str());
                }

                auto native_size = sector.size();
                auto data_size = fEDSK ? (ps->datahigh << 8) | ps->datalow : Sector::SizeCodeToLength(th.size);
                auto num_copies = data_not_found ? 0 : 1;

                // EDSK data field with gap data or multiple copies?
                if (fEDSK && data_size > native_size)
                {
                    // Multiple error copies extension?
                    // Also accept 48K as 3x16K copies, as found in early public test images.
                    // Multiple good copies extension (paranoia) means that crc error is unconcerned.
                    // Note: in theory e.g. a 512 bytes sector can have 96 copies which provides 49152 length not meaning 48K sector!
                    // Note: checking data size if equals to 49152 is unnecessary because 49152 % any native size equals to 0.
                    if ((data_crc_error || opt_paranoia) && ((data_size % native_size) == 0 || data_size == 49152))
                    {
                        // Accept 48K as 3x16K, regardless of native size
                        if (data_size == 49152)
                            num_copies = 3;

                        // Otherwise it's a straight multiple
                        else
                            num_copies = data_size / native_size;

                        // Calculate the size stored for each copy
                        data_size /= num_copies;
                    }
                }

                // Process each copy
                for (auto u = 0; u < num_copies; ++u)
                {
                    // Allocate enough space for the data, with at least the native size
                    Data data(data_size);

                    // Read the data copy
                    if (!file.read(data))
                        throw util::exception("short file reading ", cylhead, " sector ", sector.header.sector);

                    // CPDRead sometimes stores too much data. If we find a data size that is an exact multiple on an
                    // error free sector, trim it down to the natural size.
                    if (opt_fix != 0 && disk->metadata()["creator"].substr(0, 3) == "CPD" &&
                        data_size > native_size && !data_crc_error && !(data_size % native_size))
                    {
                        // Example: Discology +3.dsk (SDP)
                        Message(msgFix, "dropping suspicious excess data on %s", strCHSR(cyl, head, sec, sector.header.sector).c_str());
                        data.resize(native_size);
                    }

                    // Data CRC, size 1 above multiple of native, and dummy marker byte at the end of the data?
                    // ToDo: document horrid hack
                    if (data_crc_error && (data_size % native_size) == 1 && data[data_size - 1] == 123)
                    {
                        // Remove the dummy byte
                        data.resize(--data_size);
                    }

                    // Old-style DSK images have some restrictions
                    if (!fEDSK)
                    {
                        // Sectors without a data field have no data
                        if (id_crc_error || data_not_found)
                            continue;

                        // Gap data is not supported
                        if (data_size > native_size)
                            data.resize(data_size);
                    }

                    if (id_crc_error || data_not_found)
                        Message(msgWarning, "ignoring stored data on %s, which has no data field", strCHSR(cyl, head, sec, sector.header.sector).c_str());
                    else
                    {
                        auto res = sector.add(std::move(data), data_crc_error, deleted_dam ? IBM_DAM_DELETED : IBM_DAM);
                        if (res == Sector::Merge::Unchanged)
                            MessageCPP(msgInfo, "ignoring identical data copy of ", strCHSR(cyl, head, sec, sector.header.sector));
                        else if (res == Sector::Merge::NewDataOverLimit)
                            MessageCPP(msgWarning, "ignoring excess data copy of ", strCHSR(cyl, head, sec, sector.header.sector));
                    }
                }

                track.add(std::move(sector));
            }

            // Calculate the used track size, rounded up to the next 256 bytes
            auto uTrackEnd = (file.tell() + 0xff) & ~0xff;

            // Seek past the size indicated by the index
            file.seek(uTrackStart + uTrackSize);

            // With EDSK images the used size should match what the index says
            if (fEDSK && (uTrackEnd - uTrackStart) != uTrackSize)
            {
                // If the low 16-bits are different, we'll stick with trusting the index size
                if (((uTrackEnd - uTrackStart) & 0xffff) != uTrackSize)
                    Message(msgWarning, "%s size (%u) does not match index entry (%u)", strCH(cyl, head).c_str(), uTrackEnd - uTrackStart, uTrackSize);
                else
                {
                    // The track size probably overflowed (some WinAPE images), so trust the used size
                    Message(msgWarning, "%s size (%u) does not match index entry (%u)", strCH(cyl, head).c_str(), uTrackEnd - uTrackStart, uTrackSize);
                    file.seek(uTrackEnd);
                }
            }

            // Legacy EDSK images only store 6K data for 8K sectors, so any
            // checksum bytes the disk may have had are lost.
            if (!fWarned6K && track.is_8k_sector() && track[0].data_size() == 6144)
            {
                Message(msgWarning, "missing checksums needed to verify 8K sector integrity");
                fWarned6K = true;
            }

            disk->write(cylhead, std::move(track));
        }
    }

    // Check for Offset-Info header at normal file end
    EDSK_OFFSETS eo;
    if (fEDSK && file.read(&eo, sizeof(eo), 1))
    {
        if (!memcmp(eo.signature, EDSK_OFFSETS_SIG, 11))
        {
            Range(cyls, heads).each([&](const CylHead& cylhead) {
                auto track = disk->read_track(cylhead);

                uint16_t val;
                if (file.read(&val, sizeof(val), 1))
                    track.tracklen = util::letoh(val) * 16; // decode to bitstream bits

                for (auto& sector : track.sectors())
                {
                    if (file.read(&val, sizeof(val), 1))
                    {
                        sector.offset = util::letoh(val) * 16;  // decode to bitstream bits
                        if (sector.offset < 16)
                        {
                            MessageCPP(msgWarning, "Read (almost) 0 offset of ", sector.header, ", setting it as not 0");
                            sector.offset = 16;
                        }
                    }
                }

                disk->write(cylhead, std::move(track));
                });
        }
        else
        {
            // Undo non-matching read
            file.seek(file.tell() - intsizeof(eo));
        }
    }

    // Check for ReadStats-Info header at normal file end
    EDSK_READSTATS_HEADER ersh;
    if (fEDSK && edsk_version >= 2 && file.read(&ersh, sizeof(ersh), 1))
    {
        auto file_has_readstats = !memcmp(ersh.signature, RDSK_READSTATS_SIG, 14);
        Range(cyls, heads).each([&](const CylHead& cylhead) {
            auto track = disk->read_track(cylhead);
            for (auto& sector : track.sectors())
            {
                if (file_has_readstats)
                    EdskReadstatsElement::TransferFromFileToSector(file, cylhead, sector);
                else
                    sector.fix_readstats();
            }

            disk->write(cylhead, std::move(track));

        });
        if (!file_has_readstats)
        {
            // Undo non-matching read
            file.seek(file.tell() - intsizeof(ersh));
        }
    }

    // Check for blank track headers that RealSpectrum adds to files, despite the spec saying it shouldn't:
    // "A size of "0" indicates an unformatted track. In this case there is no data, and no track
    //  information block for this track in the image file!"
    EDSK_TRACK* pth = reinterpret_cast<EDSK_TRACK*>(ab.data());
    const int track_info_length = std::min(256, static_cast<int>(ab.size())); // TrackInfo block is 256 long at least.
    while (file.read(ab.data(), track_info_length, 1))
    {
        if (!memcmp(pth->signature, EDSK_TRACK_SIG, 10) && !pth->sectors)
        {
            // Example: Fun School 2 For The Over-8s.dsk (SDP +3)
            Message(msgWarning, "blank %s should not have EDSK track block", strCH(pth->track, pth->side).c_str());
        }
        else
        {
            // Undo non-matching read
            file.seek(file.tell() - track_info_length);
            break;
        }
    }

    const auto uTail = file.size() - file.tell();
    if (uTail > 0)
    {
        auto pbTail = file.data().data() + file.tell();

        if (!memcmp(pbTail, pbTail + 1, static_cast<size_t>(uTail - 1)))
        {
            // Example: Compilation Disk 048 (19xx)(-).zip  (TOSEC CPC Compilations)
            Message(msgWarning, "file ends with %u bytes of %02X filler", uTail, *pbTail);
        }
        else
        {
            // Example: Silva (1985)(Lankhor)(fr)[cr Genesis][t Genesis].zip (TOSEC CPC Games)
            Message(msgWarning, "%u bytes of unused data found at end of file:", uTail);
            util::hex_dump(file.data().begin(), file.data().end(), file.tell());
        }
    }

    disk->strType() = edsk_version >= 2 ? "RDSK" : "EDSK";
    return true;
}

bool ReadDSK(MemFile& file, std::shared_ptr<Disk>& disk)
{
    return ReadDSK(file, disk, 1);
}


bool WriteDSK(FILE* f_, std::shared_ptr<Disk>& disk, int edsk_version)
{
    MEMORY mem(edsk_version >= 2 ? RDSK_MAX_TRACK_SIZE : EDSK_MAX_TRACK_SIZE);
    auto pbTrack = mem.pb;

    // For RDSK (edsk version >= 2) the size of abHeader must have been increased in order
    // to have enough space for storing 4 byte long track sizes for MAX TRACKS (128) tracks.
    // Since abHeader has dynamic size, it is now vector instead of plain array.
    VectorX<uint8_t> abHeader(edsk_version >= 2 ? 1024 : 256); // EDSK file header is fixed at 256 bytes - don't change!
    auto peh = reinterpret_cast<EDSK_HEADER*>(abHeader.data());
    auto pbIndex = reinterpret_cast<uint8_t*>(peh + 1);
    auto pbIndex32 = reinterpret_cast<uint32_t*>(pbIndex); // For RDSK.
    auto max_cyls = (abHeader.size() - intsizeof(EDSK_HEADER)) / MAX_SIDES;

    memcpy(peh->szSignature, edsk_version >= 2 ? RDSK_SIGNATURE : EDSK_SIGNATURE, (edsk_version >= 2 ? sizeof(RDSK_SIGNATURE) : sizeof(EDSK_SIGNATURE)) - 1);
    memcpy(peh->szCreator, util::fmt("SAMdisk%02u%02u%02u", YEAR % 100, MONTH + 1, DAY).c_str(), sizeof(peh->szCreator));

    peh->bTracks = static_cast<uint8_t>(disk->cyls());
    peh->bSides = static_cast<uint8_t>(disk->heads());

    if (peh->bTracks > max_cyls)
        throw util::exception("too many cylinders for EDSK");
    else if (peh->bSides > MAX_SIDES)
        throw util::exception("too many heads for EDSK");

    bool add_offsets_block = true;
    VectorX<uint16_t> offsets;
    offsets.reserve((peh->bTracks + 1) * peh->bSides);

    // Saving readstats if requested and writing RDSK image file.
    bool opt_add_readstats_block = opt_readstats && edsk_version >= 2;
    bool opt_paranoia_local = opt_paranoia && opt_add_readstats_block;
    bool opt_legacy_local = opt_legacy != 0 && edsk_version < 2;
    VectorX<EdskReadstatsElement> readstats_vector;
    readstats_vector.reserve((peh->bTracks + 1) * peh->bSides * disk->read_track(CylHead(0, 0)).size());

    fseek(f_, abHeader.size(), SEEK_SET);

    for (uint8_t cyl = 0; cyl < peh->bTracks; ++cyl)
    {
        for (uint8_t head = 0; head < peh->bSides; ++head)
        {
            CylHead cylhead(cyl, head);
            auto& track = disk->read_track(cylhead);

            if (track.is_mixed_encoding())
                throw util::exception(cylhead, " is mixed-density, which EDSK doesn't support");

            bool added_sector_offsets = false;
            offsets.push_back(util::htole(static_cast<uint16_t>(track.tracklen / 16))); // encode from bitstream bits

            auto pt = reinterpret_cast<EDSK_TRACK*>(mem.pb);
            auto ps = reinterpret_cast<EDSK_SECTOR*>(pt + 1);

            Sector typical = GetTypicalSector(cylhead, track, Sector(DataRate::Unknown, Encoding::Unknown));

            // The standard track header is 256 bytes, but to allow more than 29 sectors we'll
            // round up the required size to the next 256-byte boundary
            uint32_t track_header_size = static_cast<uint32_t>((sizeof(EDSK_TRACK) + static_cast<unsigned>(track.size()) * sizeof(EDSK_SECTOR) + 0xff) & static_cast<unsigned>(~0xff));

            memset(mem, 0, track_header_size);
            memcpy(pt->signature, EDSK_TRACK_SIG, sizeof(pt->signature));
            pt->track = cyl;
            pt->side = head;
            pt->sectors = static_cast<uint8_t>(track.size());
            pt->fill = 0xe5;
            pt->size = static_cast<uint8_t>(track.size() ? typical.header.size : EDSK_DEFAULT_SIZE);
            pt->gap3 = static_cast<uint8_t>(typical.gap3 ? typical.gap3 : EDSK_DEFAULT_GAP3);

            auto datarate = track.size() ? track[0].datarate : DataRate::Unknown;
            auto encoding = track.size() ? track[0].encoding : Encoding::Unknown;

            switch (datarate)
            {
            default:                pt->rate = 0;   break;
            case DataRate::_250K:   pt->rate = 1;   break;
            case DataRate::_300K:   pt->rate = 1;   break;
            case DataRate::_500K:   pt->rate = 2;   break;
            case DataRate::_1M:     pt->rate = 3;   break;
            }

            pt->encoding = (encoding == Encoding::FM) ? 1 : 0;

            // Assume 300rpm to determine approximate track capacity
            uint32_t track_size = 0;

            // Space saving flags, to squeeze the track into the limited EDSK space
            bool fFitLegacy = opt_legacy_local != 0;
            bool fFitErrorCopies = false, fFitErrorSize = false;
            auto uFitSize = Sector::SizeCodeToLength(GetUnformatSizeCode(datarate));

            // Loop to fit any sectors
            while (track.size())
            {
                // Start with the size of the track header
                track_size = track_header_size;

                // Point to the start of the data area and the sector space
                auto pb = mem + track_header_size;

                VectorX<EdskReadstatsElement> track_readstats_vector;
                if (opt_add_readstats_block)
                    track_readstats_vector.reserve(pt->sectors);
                for (int i = 0; i < pt->sectors; ++i)
                {
                    auto sector = track[i];

                    auto revolution_time_ms = (sector.datarate == DataRate::_300K) ? RPM_TIME_360 : RPM_TIME_300;
                    auto track_capacity = GetTrackCapacity(revolution_time_ms, sector.datarate, sector.encoding);

                    // If any offsets are zero we can't generate append an offsets block.
                    if (!sector.offset)
                    {
                        add_offsets_block = false;
                        if (opt_offsets == 1)
                            MessageCPP(msgWarning, "Not writing offsets because ", sector.header, " has offset 0");
                    }
                    // Take care to only output offsets on the first pass around the fitting loop.
                    else if (!added_sector_offsets)
                    {
                        // This dsk format converts datarate 300Kbps to 250Kbps so do it on sectors as well.
                        sector.normalise_datarate(datarate);
                        offsets.push_back(util::htole(static_cast<uint16_t>(sector.offset / 16))); // encode from bitstream bits
                    }

                    // Accept only normal and deleted DAMs, removing the data field for other types.
                    // Hercule II (CPC) has a non-standard DAM (0xFD), and expects it to be unreadable.
                    if (sector.dam != IBM_DAM && sector.dam != IBM_DAM_DELETED)
                    {
                        Message(msgWarning, "discarding data from %s due to non-standard DAM %02x",
                            strCHR(cyl, head, sector.header.sector).c_str(), sector.dam);
                        sector.remove_data();
                    }

                    uint8_t status1 = 0, status2 = 0;
                    if (sector.has_badidcrc()) status1 |= SR1_CRC_ERROR;
                    if (!sector.has_badidcrc() && !sector.has_data()) status2 |= SR2_MISSING_ADDRESS_MARK;
                    if (sector.has_baddatacrc()) { status1 |= SR1_CRC_ERROR; status2 |= SR2_CRC_ERROR_IN_SECTOR_DATA; }
                    if (sector.is_deleted()) status2 |= SR2_SECTOR_WITH_DELETED_DATA;

                    auto num_copies = sector.copies();
                    auto data_size = sector.data_size();
                    auto real_size = sector.size();

                    // Clip extended sizes to the unformat size, to ensure we have a complete revolution
                    if (sector.header.size > 7 && data_size > uFitSize)
                        data_size = uFitSize;

                    // Preserve multiple copies on 8K tracks by extending them to full size
                    if (num_copies > 1 && track.is_8k_sector())
                        data_size = real_size;

                    // Warn if other (error) sectors are shorter than real size
                    if (num_copies > 1 && data_size != real_size)
                    {
                        if (data_size > real_size)
                            Message(msgWarning, "discarding gaps from multiple copies of %s", strCHR(cyl, head, sector.header.sector).c_str());
                        else if (sector.offset && sector.offset + real_size < track.tracklen)
                            Message(msgWarning, "short data field in multiple copies of %s", strCHR(cyl, head, sector.header.sector).c_str());

                        data_size = real_size;
                    }
                    // Drop extra copies on sectors larger than the track, unless it's
                    // an 8K sector with an error during the first 6K of data.
                    else if (data_size > track_capacity && !track.is_8k_sector())
                        num_copies = 1;

                    // Drop any extra copies of error sectors
                    if (fFitErrorCopies)
                    {
                        if (sector.has_baddatacrc() && num_copies > 1)
                            num_copies = 1;
                    }

                    // Cut extended sectors down to zero data
                    if (fFitErrorSize)
                    {
                        if (sector.has_baddatacrc() && data_size > uFitSize)
                            data_size = uFitSize;
                    }

                    // Force to legacy format?
                    if (fFitLegacy)
                    {
                        if (num_copies > 1) num_copies = 1;
                        if (sector.header.size == 6 && data_size > 6144) data_size = 6144;
                        if (sector.header.size >= 7) data_size = 0;
                        if (data_size > real_size) data_size = real_size;
                    }

                    int copy;
                    // Copy the sector data into place
                    for (copy = 0; copy < num_copies; ++copy)
                    {
                        // Sector having good CRC and multiple copies is allowed only in paranoia mode.
                        if (copy > 0 && !sector.has_baddatacrc() && !opt_paranoia_local)
                            break;

                        const Data& data = sector.data_copy(copy);

                        // Only copy if there's room - we'll check it fits later
                        if (static_cast<int>(track_size) + data_size < mem.size)
                        {
                            if (data.size() >= data_size)
                                memcpy(pb, data.data(), static_cast<size_t>(data_size));
                            else
                            {
                                // Extend 8K sectors to full size to preserve multiple copies
                                memcpy(pb, data.data(), static_cast<size_t>(data.size()));
                                memset(pb + data.size(), 0, static_cast<size_t>(data_size - data.size()));
                            }

                            // Single copy, data CRC error and size that conflicts with multiple copies extension?
                            if (data_size && sector.copies() == 1 && sector.has_baddatacrc() &&
                                data_size != real_size && (data_size % real_size) == 0)
                            {
                                // Write a dummy marker byte to the end of the data, and increment the stored size
                                pb[data_size++] = 123;
                            }
                        }

                        pb += data_size;
                        track_size += static_cast<size_t>(data_size);
                    }
                    const auto new_num_copies = copy;
                    if (opt_add_readstats_block)
                        track_readstats_vector.push_back(EdskReadstatsElement::CreateTransferFromSector(cylhead, sector, new_num_copies));

                    ps[i].track = static_cast<uint8_t>(sector.header.cyl);
                    ps[i].side = static_cast<uint8_t>(sector.header.head);
                    ps[i].sector = static_cast<uint8_t>(sector.header.sector);
                    ps[i].size = static_cast<uint8_t>(sector.header.size);
                    ps[i].status1 = status1;
                    ps[i].status2 = status2;

                    data_size *= new_num_copies;
                    ps[i].datalow = data_size & 0xff;
                    ps[i].datahigh = static_cast<uint8_t>(data_size >> 8);
                }

                // If the track fits, we're done
                if (static_cast<int>(track_size) <= mem.size)
                {
                    // Clear any unused space, then break out to save it
                    memset(mem + track_size, 0, static_cast<size_t>(mem.size) - track_size);
                    // Add the track readstats to final readstats
                    readstats_vector.insert(readstats_vector.end(), track_readstats_vector.begin(), track_readstats_vector.end());
                    break;
                }

                // Flag that sector offsets have already been generated.
                added_sector_offsets = true;

                // Try again using various techniques to make it fit
                if (!fFitErrorCopies) { fFitErrorCopies = true; continue; }
                if (!fFitErrorSize) { fFitErrorSize = true; continue; }
                if (uFitSize > 128) { uFitSize /= 2; continue; }
                if (!fFitLegacy) { fFitLegacy = true; continue; }

                // If we run out of methods, fail
                throw util::exception(cylhead, " size (", track_size, ") exceeds EDSK track limit (", EDSK_MAX_TRACK_SIZE, ")");
            }

            // Round the size up to the next 256-byte boundary, and store the MSB in the index
            track_size = (track_size + 0xff) & static_cast<unsigned>(~0xff);

            if (edsk_version >= 2)
            {
                *(pbIndex32++) = util::htole(track_size);
            }
            else
                *pbIndex++ = static_cast<uint8_t>(track_size >> 8);

            // Write the track to the image
            if (fwrite(pbTrack, 1, track_size, f_) != track_size)
                throw util::exception("write error (track)");
        }
    }

    // Add offsets if available, unless they're disabled
    if (!opt_legacy_local && add_offsets_block)
    {
        EDSK_OFFSETS eo = { EDSK_OFFSETS_SIG, 0 };
        if (fwrite(&eo, sizeof(eo), 1, f_) != 1)
            throw util::exception("write error (offsets)");
        if (fwrite(offsets.data(), sizeof(uint16_t), static_cast<size_t>(offsets.size()), f_) != static_cast<size_t>(offsets.size()))
            throw util::exception("write error (offsets)");
    }

    // Add readstats if available, unless they're disabled
    if (opt_add_readstats_block)
    {
        EDSK_READSTATS_HEADER ersh = { RDSK_READSTATS_SIG, 0 };
        if (fwrite(&ersh, sizeof(ersh), 1, f_) != 1)
            throw util::exception("write error (readstats)");
        for (auto readstats : readstats_vector)
        {
            readstats.TransferToFile(f_);
        }
    }

    fseek(f_, 0, SEEK_SET);
    if (fwrite(abHeader.data(), static_cast<size_t>(abHeader.size()), 1, f_) != 1)
        throw util::exception("write error");
    fseek(f_, 0, SEEK_END);

    return true;
}

bool WriteDSK(FILE* f_, std::shared_ptr<Disk>& disk)
{
    return WriteDSK(f_, disk, 1);
}
