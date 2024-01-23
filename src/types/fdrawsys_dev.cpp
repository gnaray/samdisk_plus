// fdrawcmd.sys real device wrapper:
//  http://simonowen.com/fdrawcmd/

#include "config.h"

#ifdef HAVE_FDRAWCMD_H
#include "Platform.h"
#include "fdrawcmd.h"

#include "DiskUtil.h"
#include "Options.h"
#include "DemandDisk.h"
#include "IBMPCBase.h"
#include "FdrawcmdSys.h"
#include "VfdrawcmdSys.h"
#include "Disk.h"
#include "Util.h"
#include "win32_error.h"
#include "RawTrackMFM.h"
#include "OrphanDataCapableTrack.h"
#include "TimedRawDualTrack.h"
#include "MultiScanResult.h"

#include <cstring>
#include <memory>

static auto& opt_base = getOpt<int>("base");
static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_datarate = getOpt<DataRate>("datarate");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_gaps = getOpt<int>("gaps");
static auto& opt_newdrive = getOpt<int>("newdrive");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_rescans = getOpt<RetryPolicy>("rescans");
static auto& opt_retries = getOpt<RetryPolicy>("retries");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_steprate = getOpt<int>("steprate");

class TrackInfo
{
public:
    Encoding encoding{ Encoding::Unknown };
    DataRate dataRate{ DataRate::Unknown };
    int trackTime = 0;
    int trackLenIdeal = 0;
};

class FdrawSysDevDisk final : public DemandDisk
{
public:
    FdrawSysDevDisk(const std::string& path, std::unique_ptr<FdrawcmdSys> fdrawcmd)
            : m_fdrawcmd(std::move(fdrawcmd))
    {
        try
        {
            SetMetadata(path);

            auto srt = (opt_steprate >= 0) ? opt_steprate : (opt_newdrive ? 0xd : 0x8);
            auto hut = 0x0f;
            auto hlt = opt_newdrive ? 0x0f : 0x7f;
            m_fdrawcmd->Specify(srt, hut, hlt);

            m_fdrawcmd->SetMotorTimeout(0);
            m_fdrawcmd->Recalibrate();

            if (!opt_newdrive)
                m_fdrawcmd->SetDiskCheck(false);
        }
        catch (...)
        {
            throw util::exception("failed to initialise fdrawcmd.sys device");
        }
    }

protected:
    bool supports_retries() const override
    {
        return true;
    }

    {
    }

    TrackData load(const CylHead& cylhead, bool /*first_read*/,
            int with_head_seek_to, const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/) override;

    bool preload(const Range& /*range*/, int /*cyl_step*/) override
    {
        return false;
    }

    bool is_constant_disk() const override
    {
        return false;
    }

private:
    void SetMetadata(const std::string& path);
    bool DetectEncodingAndDataRate(int head);
    Track BlindReadHeaders(const CylHead& cylhead, int& firstSectorSeen);
    void ReadSector(const CylHead& cylhead, Track& track, int index, int firstSectorSeen = 0);
    void ReadFirstGap(const CylHead& cylhead, Track& track);

    /* Here are the methods of multi track reading based on CmdTimedMultiScan
     * implemented in driver version >= 1.0.1.12.
     */
    bool ScanAndDetectIfNecessary(const CylHead& cylhead, MultiScanResult& multiScanResult);
    TimedRawDualTrack BlindReadHeaders112(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy);
    void GuessAndAddSectorIdsOfOrphans(Track& track, TimedRawDualTrack& timedRawDualTrack) const;
    static bool GetSectorDataFromRawTrack(TimedRawDualTrack& timedRawDualTrack, const int index);
    bool ReadSectors(const CylHead& cylhead, TimedRawDualTrack& timedRawDualTrack, const DeviceReadingPolicy& deviceReadingPolicy);
    bool ReadAndMergeRawTracks(const CylHead& cylhead, TimedRawDualTrack& timedRawDualTrack);
    std::unique_ptr<FdrawcmdSys> m_fdrawcmd;
    Encoding m_lastEncoding{ Encoding::Unknown };
    DataRate m_lastDataRate{ DataRate::Unknown };
    TrackInfo m_trackInfo[MAX_DISK_CYLS * MAX_DISK_HEADS];
    bool m_warnedMFM128{ false };
};


void FdrawSysDevDisk::SetMetadata(const std::string& path)
{
    auto device_path = R"(\\.\)" + path;
    Win32Handle hdev{
        CreateFile(device_path.c_str(), 0, FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr) };

    if (hdev.get() != INVALID_HANDLE_VALUE)
    {
        DWORD dwRet = 0;
        DISK_GEOMETRY dg[8]{};
        if (DeviceIoControl(hdev.get(), IOCTL_STORAGE_GET_MEDIA_TYPES,
            nullptr, 0, &dg, sizeof(dg), &dwRet, nullptr) && dwRet > sizeof(DISK_GEOMETRY))
        {
            auto count = dwRet / sizeof(dg[0]);
            metadata()["bios_type"] = to_string(dg[count - 1].MediaType);
        }
    }

    const auto info = m_fdrawcmd->GetFdcInfo();
    if (info != nullptr)
    {
        static const std::vector<std::string> fdc_types{
            "Unknown", "Unknown1", "Normal", "Enhanced", "82077", "82077AA", "82078_44", "82078_64", "National" };
        static const std::vector<std::string> data_rates{
            "250K", "300K", "500K", "1M", "2M" };

        std::stringstream ss;
        for (size_t i = 0, n = 0; i < data_rates.size(); ++i)
        {
            if (!(info->SpeedsAvailable & (1U << i)))
                continue;

            if (n++) ss << " / ";
            ss << data_rates[i];
        }

        metadata()["fdc_type"] = (info->ControllerType < fdc_types.size()) ? fdc_types[info->ControllerType] : "???";
        metadata()["data_rates"] = ss.str();
    }
}

TrackData FdrawSysDevDisk::load(const CylHead& cylhead, bool /*first_read*/,
        int with_head_seek_to, const DeviceReadingPolicy& deviceReadingPolicy/* = DeviceReadingPolicy{}*/)
{
    // Limiting sector reading as specified in case of normal disk request.
    const auto normal_sector_id_begin = opt_base > 0 ? opt_base : 1;
    const auto normal_sector_id_end = opt_sectors > 0 ? (normal_sector_id_begin + opt_sectors) : 256;

    if (with_head_seek_to >= 0)
        m_fdrawcmd->Seek(with_head_seek_to);
    m_fdrawcmd->Seek(cylhead.cyl);


    auto firstSectorSeen{ 0 };
    auto track = BlindReadHeaders(cylhead, firstSectorSeen);

    bool read_first_gap_requested = opt_gaps >= GAPS_CLEAN;
    // Read sector if either
    // 1) its index is 0 and read first gap is requested, its data is used there for sanity checking.
    // 2) its id is not in specfied headers of good sectors, else it is wasting time.
    // If sector has bad id or has good data, then ReadSector will skip reading it.
    for (int j = 0; j < 2; j++)
    {
        for (int i = j; i < track.size(); i += 2)
        {
            const auto& sector = track[i];
            if ((i == 0 && read_first_gap_requested) || (!deviceReadingPolicy.SkippableSectors().Contains(sector, track.tracklen)
                                                         && (!opt_normal_disk || (sector.header.sector >= normal_sector_id_begin && sector.header.sector < normal_sector_id_end))))
                ReadSector(cylhead, track, i, firstSectorSeen);
        }
    }

    if (read_first_gap_requested)
        ReadFirstGap(cylhead, track);

    return TrackData(cylhead, std::move(track));
}

/* Detect encoding and data rate of the track under the given drive head.
 * If nothing is detected (the track looks like an empty track) then return false.
 * Return true on detection success and m_lastEncoding, m_lastDataRate are valid,
 * otherwise return false and m_lastEncoding, m_lastDataRate are unchanged.
 */
bool FdrawSysDevDisk::DetectEncodingAndDataRate(int head)
{
    FD_CMD_RESULT result{};

    if (m_lastEncoding != Encoding::Unknown && m_lastDataRate != DataRate::Unknown)
    {
        // Try the last successful encoding and data rate.
        m_fdrawcmd->SetEncRate(m_lastEncoding, m_lastDataRate);

        // Return if we found a sector.
        if (m_fdrawcmd->CmdReadId(head, result))
            return true;
    }

    // Prefer MFM to FM.
    for (auto encoding : { Encoding::MFM, Encoding::FM })
    {
        // Prefer higher datarates.
        for (auto datarate : { DataRate::_1M, DataRate::_500K, DataRate::_300K, DataRate::_250K })
        {
            // Skip FM if we're only looking for MFM, or the data rate is 1Mbps.
            if (encoding == Encoding::FM && (opt_encoding == Encoding::MFM || datarate == DataRate::_1M))
                continue;

            // Skip rates not matching user selection.
            if (opt_datarate != DataRate::Unknown && datarate != opt_datarate)
                continue;

            // Skip 1Mbps if the FDC doesn't report it's supported.
            if (datarate == DataRate::_1M)
            {
                FD_FDC_INFO fi{};
                if (!m_fdrawcmd->GetFdcInfo(fi) || !(fi.SpeedsAvailable & FDC_SPEED_1M))
                {
                    // Fail if user selected the rate.
                    if (opt_datarate == DataRate::_1M)
                        throw util::exception("FDC doesn't support 1Mbps data rate");

                    continue;
                }
            }

            m_fdrawcmd->SetEncRate(encoding, datarate);

            // Retry in case of spurious header CRC errors.
            for (auto i = 0; i <= opt_retries; ++i) // TODO originally opt_retries(=5), could be replacable with opt_encratedetect_retries?
            {
                if (m_fdrawcmd->CmdReadId(head, result))
                {
                    // Remember the settings for the first try next time.
                    m_lastEncoding = encoding;
                    m_lastDataRate = datarate;
                    return true;
                }

                // Give up on the current settings if nothing was found.
                if (GetLastError_MP() == ERROR_FLOPPY_ID_MARK_NOT_FOUND ||
                    GetLastError_MP() == ERROR_SECTOR_NOT_FOUND)
                    break;

                // Fail for any reason except a CRC error
                if (GetLastError_MP() != ERROR_CRC)
                    throw win32_error(GetLastError_MP(), "ReadId");
            }
        }
    }

    // Nothing detected.
    return false;
}

Track FdrawSysDevDisk::BlindReadHeaders(const CylHead& cylhead, int& firstSectorSeen)
{
    Track track;

    auto scan_size = lossless_static_cast<int>(sizeof(FD_TIMED_SCAN_RESULT) + sizeof(FD_TIMED_ID_HEADER) * MAX_SECTORS);
    MEMORY mem(scan_size);
    auto scan_result = reinterpret_cast<FD_TIMED_SCAN_RESULT*>(mem.pb);

    bool areEncodingAndDataRateNewlyDetermined = false;
    if (m_lastEncoding == Encoding::Unknown || m_lastDataRate == DataRate::Unknown)
    {
        if (!DetectEncodingAndDataRate(cylhead.head)) // No encoding and data rate. It is probably empty track.
            return track;
        areEncodingAndDataRateNewlyDetermined = true;
    }

    int tracktime = 0;
    int scanAttempt;
    for (scanAttempt = 3; scanAttempt > 0; scanAttempt--) // The scanAttempt start value could be opt parameter but not so important.
    {
        if (!m_fdrawcmd->CmdTimedScan(cylhead.head, scan_result, scan_size))
            throw win32_error(GetLastError_MP(), "TimedScan");

        // If we have valid older settings ...
        if (!areEncodingAndDataRateNewlyDetermined)
        {   // ... and nothing was found, they might have changed.
            if (scan_result->count == 0)
            {
                if (!DetectEncodingAndDataRate(cylhead.head)) // No encoding and data rate. It is very probably empty track.
                    return track;

                if (!m_fdrawcmd->CmdTimedScan(cylhead.head, scan_result, scan_size))
                    throw win32_error(GetLastError_MP(), "TimedScan");
            }
            areEncodingAndDataRateNewlyDetermined = true; // True if anything is read by detector or scanner.
        }

        tracktime = lossless_static_cast<int>(scan_result->tracktime);
        if (m_fdrawcmd->GetVersion().value < DriverVersion1_0_1_12)
        {
            int tracktimeCorrect;
            if (!m_fdrawcmd->FdGetTrackTime(tracktimeCorrect)) // This also fixes tracktime rarely corrupted by CmdTimedScan.
                throw win32_error(GetLastError_MP(), "GetTrackTime");
            constexpr auto tracktimeTolerance = 0.01;
            if (tracktime < tracktimeCorrect * (1 - tracktimeTolerance) || tracktime > tracktimeCorrect * (1 + tracktimeTolerance))
            {
                Message(msgWarning, "track time of %s is corrupted by older driver, upgrade it if possible. Retrying", CH(cylhead.cyl, cylhead.head));
                continue; // Try again.
            }
        }
        // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats
        // TODO What about Amiga HD disk with 22 sectors per track with 150 RPM? Otherwise the condition is correct.
        if (tracktime > RPM_TIME_200)
            throw util::diskspeedwrong_exception("index-halving cables are no longer supported (rpm <= 200)");
        break;
    }
    if (scanAttempt == 0)
        throw util::diskspeedwrong_exception("disk speed is wrong (track read time is out of tolerated range)");

    firstSectorSeen = scan_result->firstseen;

    if (scan_result->count > 0)
    {
        const auto mfmbit_us = GetFmOrMfmDataBitsTime(m_lastDataRate, m_lastEncoding);
        track.tracktime = tracktime;
        track.tracklen = round_AS<int>(track.tracktime / mfmbit_us);

        for (int i = 0; i < scan_result->count; ++i)
        {
            const auto& scan_header = scan_result->HeaderArray(i);
            if (opt_normal_disk && (scan_header.cyl != cylhead.cyl || scan_header.head != cylhead.head))
            {
                Message(msgWarning, "ReadHeaders: track's %s does not match sector's %s, ignoring this sector.",
                    CH(cylhead.cyl, cylhead.head), CHR(scan_header.cyl, scan_header.head, scan_header.sector));
                continue;
            }
            Header header(scan_header.cyl, scan_header.head, scan_header.sector, scan_header.size);
            Sector sector(m_lastDataRate, m_lastEncoding, header);

            sector.offset = round_AS<int>(scan_header.reltime / mfmbit_us);
            sector.set_constant_disk(false);
            track.add(std::move(sector));
        }
    }

    return track;
}

void FdrawSysDevDisk::ReadSector(const CylHead& cylhead, Track& track, int index, int firstSectorSeen)
{
    auto& sector = track[index];

    if (sector.has_badidcrc() || sector.has_good_data(false, opt_normal_disk))
        return;

    auto size = Sector::SizeCodeToRealLength(sector.header.size);
    MEMORY mem(size);

    for (int i = 0; i <= opt_retries; ++i)
    {
        // If the sector id occurs more than once on the track, synchronise to the correct one
        if (track.is_repeated(sector))
        {
            auto offset{ (index + track.size() + firstSectorSeen) % track.size() };
            m_fdrawcmd->FdSetSectorOffset(offset);
        }

        // Invalidate the content so misbehaving FDCs can be identififed.
        memset(mem.pb, 0xee, lossless_static_cast<size_t>(mem.size));

        const Header& header = sector.header;
        if (!m_fdrawcmd->CmdRead(cylhead.head, header.cyl, header.head, header.sector, header.size, 1, mem))
        {
            // Reject errors other than CRC, sector not found and missing address marks
            auto error{ GetLastError_MP() };
            if (error != ERROR_CRC &&
                error != ERROR_SECTOR_NOT_FOUND &&
                error != ERROR_FLOPPY_ID_MARK_NOT_FOUND)
            {
                throw win32_error(error, "Read");
            }
        }

        // Get the controller result for the read to find out more
        FD_CMD_RESULT result{};
        if (!m_fdrawcmd->GetResult(result))
            throw win32_error(GetLastError_MP(), "Result");

        // Try again if header or data field are missing.
        if (result.st1 & (STREG1_MISSING_ADDRESS_MARK | STREG1_NO_DATA))
        {
            sector.add_read_attempts(1);
            continue;
        }

        // Header match not found for a sector we scanned earlier?
        if (result.st1 & STREG1_END_OF_CYLINDER)
        {
            // Warn the user if we suspect the FDC can't handle 128-byte MFM sectors.
            if (!m_warnedMFM128 && sector.encoding == Encoding::MFM && sector.size() == 128)
            {
                Message(msgWarning, "FDC seems unable to read 128-byte sectors correctly");
                m_warnedMFM128 = true;
            }
            sector.add_read_attempts(1);
            continue;
        }

        // Unsure what result.sector is exactly. Sometimes header.sector but usually header.sector+1.
        if (opt_normal_disk && (header.cyl != cylhead.cyl || header.head != cylhead.head
            || result.cyl != header.cyl || result.head != header.head
            || (result.sector != header.sector && result.sector != header.sector + 1)))
        {
            Message(msgWarning, "ReadSector: track's %s does not match sector's %s, ignoring this sector.",
                CH(cylhead.cyl, cylhead.head), CHR(header.cyl, header.head, header.sector));
            sector.add_read_attempts(1);
            continue;
        }

        bool data_crc_error{ (result.st2 & STREG2_DATA_ERROR_IN_DATA_FIELD) != 0 };
        uint8_t dam = (result.st2 & STREG2_CONTROL_MARK) ? IBM_DAM_DELETED : IBM_DAM;

        Data data(mem.pb, mem.pb + mem.size);
        sector.add_with_readstats(std::move(data), data_crc_error, dam);

        // If the read command was successful we're all done.
        if ((result.st0 & STREG0_INTERRUPT_CODE) == 0)
            break;

        // Accept sectors that overlap the next field, as they're unlikely to succeed.
        if (track.data_overlap(sector))
            break;

        // Accept 8K sectors with a recognised checksum method.
        if (track.is_8k_sector() && !ChecksumMethods(mem.pb, size).empty())
            break;
    }
}

void FdrawSysDevDisk::ReadFirstGap(const CylHead& cylhead, Track& track)
{
    if (track.empty())
        return;

    auto& sector = track[0];

    if (sector.has_badidcrc() || track.data_overlap(sector))
        return;

    // Read a size
    auto size_code = sector.header.size + 1;
    auto size = Sector::SizeCodeToRealLength(size_code);
    MEMORY mem(size);

    for (int i = 0; i <= opt_retries; ++i)
    {
        // Invalidate the content so misbehaving FDCs can be identififed.
        memset(mem.pb, 0xee, lossless_static_cast<size_t>(mem.size));

        if (!m_fdrawcmd->CmdReadTrack(cylhead.head, 0, 0, 0, size_code, 1, mem))
        {
            // Reject errors other than CRC, sector not found and missing address marks
            auto error{ GetLastError_MP() };
            if (error != ERROR_CRC &&
                error != ERROR_SECTOR_NOT_FOUND &&
                error != ERROR_FLOPPY_ID_MARK_NOT_FOUND)
            {
                throw win32_error(error, "ReadTrack");
            }
        }

        FD_CMD_RESULT result{};
        if (!m_fdrawcmd->GetResult(result))
            throw win32_error(GetLastError_MP(), "Result");

        if (result.st1 & (STREG1_MISSING_ADDRESS_MARK | STREG1_END_OF_CYLINDER))
            continue;
        else if (result.st2 & STREG2_MISSING_ADDRESS_MARK_IN_DATA_FIELD)
            continue;

        // Sanity check the start of the track against a good copy.
        if (sector.has_good_data())
        {
            const auto data = sector.data_copy();
            if (std::memcmp(data.data(), mem.pb, lossless_static_cast<size_t>(data.size())))
            {
                Message(msgWarning, "track read of %s doesn't match first sector content", CH(cylhead.cyl, cylhead.head));
                break;
            }
        }

        auto extent = track.data_extent_bytes(sector);
        sector.add(Data(mem.pb, mem.pb + extent), sector.has_baddatacrc(), sector.dam);
        break;
    }
}

///////////////////////////////////////////////////////////////////////////////
/* Here are the methods of multi track reading based on CmdTimedMultiScan
 * implemented in driver version >= 1.0.1.12.
 */
bool FdrawSysDevDisk::ScanAndDetectIfNecessary(const CylHead& cylhead, MultiScanResult &multiScanResult)
{
    const bool trackInfoHasEncodingAndDataRate = m_trackInfo[cylhead].encoding != Encoding::Unknown && m_trackInfo[cylhead].dataRate != DataRate::Unknown;
    if (trackInfoHasEncodingAndDataRate && (m_trackInfo[cylhead].encoding != m_lastEncoding || m_trackInfo[cylhead].dataRate != m_lastDataRate))
    {
        m_lastEncoding = m_trackInfo[cylhead].encoding;
        m_lastDataRate = m_trackInfo[cylhead].dataRate;
        if (!m_fdrawcmd->SetEncRate(m_lastEncoding, m_lastDataRate))
            throw win32_error(GetLastError_MP(), "SetEncRate");
    }
    if (m_lastEncoding != Encoding::Unknown && m_lastDataRate != DataRate::Unknown)
    {
        // Try the last successful encoding and data rate.
        if (!m_fdrawcmd->CmdTimedMultiScan(cylhead.head, 0, multiScanResult, multiScanResult.size, opt_byte_tolerance_of_time))
            throw win32_error(GetLastError_MP(), "TimedMultiScan");
        if (trackInfoHasEncodingAndDataRate) // Not found sector but earlier found so detection is successful.
            return true;
        // Return true if we found a sector.
        if (multiScanResult.count() > 0) // Found sector so detection is successful.
        {
            m_trackInfo[cylhead].encoding = m_lastEncoding;
            m_trackInfo[cylhead].dataRate = m_lastDataRate;
            return true;
        }
    }

    // Prefer MFM to FM.
    for (auto encoding : { Encoding::MFM, Encoding::FM })
    {
        // Prefer higher datarates.
        for (auto dataRate : { DataRate::_1M, DataRate::_500K, DataRate::_300K, DataRate::_250K })
        {
            // Skip FM if we're only looking for MFM, or the data rate is 1Mbps.
            if (encoding == Encoding::FM && (opt_encoding == Encoding::MFM || dataRate == DataRate::_1M))
                continue;

            // Skip rates not matching user selection.
            if (opt_datarate != DataRate::Unknown && dataRate != opt_datarate)
                continue;

            // Skip 1Mbps if the FDC doesn't report it's supported.
            if (dataRate == DataRate::_1M)
            {
                FD_FDC_INFO fi{};
                if (!m_fdrawcmd->GetFdcInfo(fi) || !(fi.SpeedsAvailable & FDC_SPEED_1M))
                {
                    // Fail if user selected the rate.
                    if (opt_datarate == DataRate::_1M)
                        throw util::exception("FDC doesn't support 1Mbps data rate");

                    continue;
                }
            }

            if (!m_fdrawcmd->SetEncRate(encoding, dataRate))
                throw win32_error(GetLastError_MP(), "SetEncRate");

            if (!m_fdrawcmd->CmdTimedMultiScan(cylhead.head, 0, multiScanResult, multiScanResult.size, opt_byte_tolerance_of_time))
                throw win32_error(GetLastError_MP(), "TimedMultiScan");
            if (multiScanResult.count() > 0)
            {
                // Remember the settings for the first try next time.
                m_lastEncoding = encoding;
                m_lastDataRate = dataRate;
                m_trackInfo[cylhead].encoding = encoding;
                m_trackInfo[cylhead].dataRate = dataRate;
                return true;
            }
        }
    }

    return false; // Nothing detected.
}

TimedRawDualTrack FdrawSysDevDisk::BlindReadHeaders112(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy)
{
    if (m_fdrawcmd->GetVersion().value < DriverVersion1_0_1_12)
        throw util::exception("BlindReadHeaders112 method requires driver version 1.12 at least");

    if (m_trackInfo[cylhead].trackTime <= 0)
    {   // Premeasuring tracktime once would be enough for constant speed drive but would not be for variable speed drives.
        // In addition CmdTimed*Scan measures tracktime only if it was not measured earlier although CmdTimed*Scan might adjust it.
        FD_MULTI_TRACK_TIME_RESULT track_time;
        if (!m_fdrawcmd->FdGetMultiTrackTime(track_time, 1))
            throw win32_error(GetLastError_MP(), "GetMultiTrackTime"); // "not available for this disk type"
        m_trackInfo[cylhead].trackTime = lossless_static_cast<int>(track_time.spintime);
        // variable speed is out of scope. however this solution supports variable speed unless m_trackInfo[all] is set as spintime.
    }

    TimedRawDualTrack timedRawDualTrack;
    // Find the sectors by scanning the floppy disk both timed and raw.
    auto rawTrackRescans = std::max(opt_rescans, opt_retries);
    DeviceReadingPolicy deviceReadingPolicyForScanning = deviceReadingPolicy;
    auto timedTrackRescans = opt_rescans;
    do
    {
        MultiScanResult multiScanResult(MAX_SECTORS);
        if (!ScanAndDetectIfNecessary(cylhead, multiScanResult))
            return timedRawDualTrack;

        auto timedTrackTime = multiScanResult.trackTime();
        // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats
        // TODO What about Amiga HD disk with 150 RPM? Otherwise the condition is correct.
        if (timedTrackTime > RPM_TIME_200)
            throw util::diskspeedwrong_exception("index-halving cables are no longer supported (rpm <= 200)");
        m_trackInfo[cylhead].trackTime = timedTrackTime;
        if (m_lastEncoding != Encoding::MFM) // Currently only MFM encoding is supported due to ReadAndMergeRawTracks method.
            return timedRawDualTrack;

        if (multiScanResult.count() > 0)
        {
            if (m_trackInfo[cylhead].trackLenIdeal <= 0)
            {
                ReadAndMergeRawTracks(cylhead, timedRawDualTrack);
                rawTrackRescans--;
            }
            Track newTimedTrack = multiScanResult.DecodeResult(cylhead, m_lastDataRate, m_lastEncoding, m_trackInfo[cylhead].trackLenIdeal);
            if (m_trackInfo[cylhead].trackLenIdeal > 0)
                newTimedTrack.setTrackLenAndNormaliseTrackTimeAndSectorOffsets(m_trackInfo[cylhead].trackLenIdeal);

            const auto sectorAmountPrev = timedRawDualTrack.timedTrack.size();
            timedRawDualTrack.timedTrack.add(std::move(newTimedTrack));
            if (timedRawDualTrack.timedTrack.size() > sectorAmountPrev)
            {
                deviceReadingPolicyForScanning = deviceReadingPolicy;
                deviceReadingPolicyForScanning.AddSkippableSectors(timedRawDualTrack.timedTrack.good_idcrc_sectors());
                if (timedTrackRescans.sinceLastChange)
                    timedTrackRescans = opt_rescans;
            }
        }
    } while (timedTrackRescans-- > 0 && deviceReadingPolicyForScanning.WantMoreSectors());

    // If more scanning is required then try to find sectors from the raw track.
    do
    {
        bool foundNewSector = rawTrackRescans >= 0 && ReadAndMergeRawTracks(cylhead, timedRawDualTrack);
        if (m_trackInfo[cylhead].trackLenIdeal > 0 && (foundNewSector || timedRawDualTrack.lastTimedRawTrackSingle.empty()))
        {
            if (timedRawDualTrack.SyncAndDemultiRawToTimed(m_trackInfo[cylhead].trackLenIdeal)) // Updates lastTimedRawTrackSingle.
            {
                const auto sectorAmountPrev = timedRawDualTrack.finalTimedAndRawTrack.size();
                // Merging last timed raw track with timed track and orphan ids into final timed and raw track.
                auto finalTimedAndRawTrackLocal = timedRawDualTrack.lastTimedRawTrackSingle.track;
                finalTimedAndRawTrackLocal.add(Track(timedRawDualTrack.timedTrack));
                GuessAndAddSectorIdsOfOrphans(finalTimedAndRawTrackLocal, timedRawDualTrack);
                finalTimedAndRawTrackLocal.add(std::move(timedRawDualTrack.finalTimedAndRawTrack));
                timedRawDualTrack.finalTimedAndRawTrack = finalTimedAndRawTrackLocal;
                if (timedRawDualTrack.finalTimedAndRawTrack.size() > sectorAmountPrev)
                {
                    deviceReadingPolicyForScanning = deviceReadingPolicy;
                    deviceReadingPolicyForScanning.AddSkippableSectors(timedRawDualTrack.finalTimedAndRawTrack.good_idcrc_sectors());
                }
            }
        }
        if (!deviceReadingPolicyForScanning.WantMoreSectors())
        {
            if (ReadSectors(cylhead, timedRawDualTrack, deviceReadingPolicy))
                break; // Scanning and reading is complete.
        }
        if (foundNewSector)
        {
            if (rawTrackRescans.sinceLastChange)
                rawTrackRescans = std::max(opt_rescans, opt_retries);
        }
    } while (rawTrackRescans-- > 0);

    // Remove not normal sector headers at the track end.
    if (opt_normal_disk)
    {
        while (!timedRawDualTrack.finalTimedAndRawTrack.empty())
        {
            auto it = timedRawDualTrack.finalTimedAndRawTrack.rbegin();
            const auto& sector = *it;
            const auto lengthWithoutOuterGaps = GetFmOrMfmSectorOverheadFromOffsetToDataCrcEnd(sector.datarate, sector.encoding, sector.size());
            if (sector.offset + lengthWithoutOuterGaps * 8 * 2 < timedRawDualTrack.finalTimedAndRawTrack.tracklen) // It fits, no problem.
                break;
            timedRawDualTrack.finalTimedAndRawTrack.sectors().erase(std::next(it).base());
        }
    }

    ReadSectors(cylhead, timedRawDualTrack, deviceReadingPolicy);

    return timedRawDualTrack;
}

void FdrawSysDevDisk::GuessAndAddSectorIdsOfOrphans(Track& track, TimedRawDualTrack& timedRawDualTrack) const
{
    // If there is no cylhead mismatch and there are orphan datas then guess their sector id.
    if (timedRawDualTrack.lastTimedRawTrackSingle.cylheadMismatch || timedRawDualTrack.lastTimedRawTrackSingle.orphanDataTrack.empty())
        return;
    IdAndOffsetVector idAndOffsetVector;
    for (const auto& orphanDataSector : timedRawDualTrack.lastTimedRawTrackSingle.orphanDataTrack)
    {

        if (opt_debug)
            util::cout << "GuessAndAddSectorIdsOfOrphans: processing sector, offset=" << orphanDataSector.offset << ", ID=" << orphanDataSector.header.sector << "\n";

        const auto it = track.findForDataFmOrMfm(orphanDataSector.offset, orphanDataSector.data_size());
        if (it == track.end()) // The end is dynamic since adding sector to track.
        {
            util::cout << "GuessAndAddSectorIdsOfOrphans: Orphan has no parent, offset=" << orphanDataSector.offset << "\n";
            if (idAndOffsetVector.empty())
            {
                // Let us discover the format scheme if possible.
                idAndOffsetVector = track.DiscoverTrackSectorScheme();
                if (idAndOffsetVector.empty())
                    return;
            }
            // Missing sector ID, try to find it in the found format scheme.
            const auto sectorId = orphanDataSector.FindParentSectorIdByOffset(idAndOffsetVector);
            if (sectorId >= 0)
            {
                auto parentSector = orphanDataSector;
                parentSector.header.sector = sectorId;
                parentSector.offset = track.findReasonableIdOffsetForDataFmOrMfm(orphanDataSector.offset);
                track.add(std::move(parentSector));
            }
        }
    }
}

/*static*/ bool FdrawSysDevDisk::GetSectorDataFromRawTrack(TimedRawDualTrack& timedRawDualTrack, const int index)
{
    auto& finalTrack = timedRawDualTrack.finalTimedAndRawTrack;
    auto& sector = finalTrack[index];

    if (sector.has_badidcrc())
        return false;
    if (sector.has_good_data(false, opt_normal_disk))
        return true;

    if (opt_debug)
        util::cout << "RawReadSector: reading " << index << ". sector having ID " << sector.header.sector << "\n";

    // If there is no cylhead mismatch and there are orphan datas then try to find the orphan data of sector.
    if (timedRawDualTrack.lastTimedRawTrackSingle.cylheadMismatch || timedRawDualTrack.lastTimedRawTrackSingle.orphanDataTrack.empty())
        return false;
    const auto it = timedRawDualTrack.lastTimedRawTrackSingle.orphanDataTrack.findDataForSectorIdFmOrMfm(sector.offset, sector.size());
    if (it != timedRawDualTrack.lastTimedRawTrackSingle.orphanDataTrack.end())
    {
        if (it->has_good_data(false, opt_normal_disk))
        {
            sector.add_with_readstats(Data(it->data_copy()));
            return true;
        }
    }
    return false;
}

bool FdrawSysDevDisk::ReadSectors(const CylHead& cylhead, TimedRawDualTrack& timedRawDualTrack, const DeviceReadingPolicy& deviceReadingPolicy)
{
    // Limiting sector reading as specified in case of normal disk request.
    const auto normal_sector_id_begin = opt_base > 0 ? opt_base : 1;
    const auto normal_sector_id_end = opt_sectors > 0 ? (normal_sector_id_begin + opt_sectors) : 256;

    bool allDataGood = true;
    const auto iSup = timedRawDualTrack.finalTimedAndRawTrack.size();
    for (int j = 0; j < 2; j++)
    {
        for (int i = j; i < iSup; i += 2)
        {
            const auto& sector = timedRawDualTrack.finalTimedAndRawTrack[i];
            if ((!deviceReadingPolicy.SkippableSectors().Contains(sector, timedRawDualTrack.finalTimedAndRawTrack.tracklen)
                 && (!opt_normal_disk || (sector.header.sector >= normal_sector_id_begin && sector.header.sector < normal_sector_id_end))))
            {
                if (sector.read_attempts() == 0)
                    ReadSector(cylhead, timedRawDualTrack.finalTimedAndRawTrack, i, 0);
                if (!GetSectorDataFromRawTrack(timedRawDualTrack, i))
                    allDataGood = false; // Sector is still not good.
            }
        }
    }
    return allDataGood;
}

bool FdrawSysDevDisk::ReadAndMergeRawTracks(const CylHead& cylhead, TimedRawDualTrack& timedRawDualTrack)
{
    assert(m_lastEncoding == Encoding::MFM); // Currently this method handles only MFM track.
    assert(m_lastDataRate != DataRate::Unknown);
    MEMORY mem;

    if (!m_fdrawcmd->CmdReadTrack(cylhead.head, cylhead.cyl, cylhead.head, 0, 8, 1, mem)) // Read one big 32K sector.
        throw win32_error(GetLastError_MP(), "ReadTrack");
    RawTrackMFM toBeMergedRawTrack(mem, m_lastDataRate);
    const auto sectorIdAmountPrev = timedRawDualTrack.rawTrackMulti.track.size();
    const auto sectorAmountPrev = timedRawDualTrack.rawTrackMulti.size();
    timedRawDualTrack.rawTrackMulti.mergeRawTrack(cylhead, toBeMergedRawTrack);
    const bool foundNewSectorId = timedRawDualTrack.rawTrackMulti.track.size() > sectorIdAmountPrev;
    const bool foundNewSector = timedRawDualTrack.rawTrackMulti.size() > sectorAmountPrev;
    if (m_trackInfo[cylhead].trackLenIdeal <= 0 && foundNewSectorId)
    {
        const auto bestTrackLen = timedRawDualTrack.rawTrackMulti.determineBestTrackLen(GetFmOrMfmTimeDataBitsAsRounded(m_lastDataRate, m_lastEncoding, m_trackInfo[cylhead].trackTime));
        if (bestTrackLen > 0)
        {
            m_trackInfo[cylhead].trackLenIdeal = bestTrackLen;
            if (timedRawDualTrack.timedTrack.tracklen > 0)
                timedRawDualTrack.timedTrack.setTrackLenAndNormaliseTrackTimeAndSectorOffsets(m_trackInfo[cylhead].trackLenIdeal);
        }
    }
    return foundNewSector;
}

///////////////////////////////////////////////////////////////////////////////

bool ReadFdrawcmdSys(const std::string& path, std::shared_ptr<Disk>& disk)
{
    const auto devidx = (util::lowercase(path) == "b:") ? 1 : 0;
    const auto virtualFloppyDevice = IsVfd(path);
    const auto virtualFloppyDevicePath = path.substr(4);
    if (!virtualFloppyDevice && !IsFloppyDevice(path))
        return false;
    const auto fdrawcmdSysName = std::string(virtualFloppyDevice ? "v": "") + "fdrawcmd.sys";
    auto fdrawcmd = virtualFloppyDevice ? VfdrawcmdSys::Open(virtualFloppyDevicePath) : FdrawcmdSys::Open(devidx);
    if (!fdrawcmd)
        throw util::exception("failed to open ", fdrawcmdSysName, " device");
    auto fdrawcmd_dev_disk = std::make_shared<FdrawSysDevDisk>(path, std::move(fdrawcmd));
    fdrawcmd_dev_disk->extend(CylHead(83 - 1, 2 - 1));

    fdrawcmd_dev_disk->strType() = fdrawcmdSysName;
    disk = fdrawcmd_dev_disk;

    return true;
}

bool WriteFdrawcmdSys(const std::string& path, std::shared_ptr<Disk>&/*disk*/)
{
    if (!IsFloppyDevice(path))
        return false;

    throw util::exception("fdrawcmd.sys writing not yet implemented");
}

#endif // HAVE_FDRAWCMD_H
