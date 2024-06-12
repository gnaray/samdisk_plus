// fdrawcmd.sys real device wrapper:
//  http://simonowen.com/fdrawcmd/

#include "config.h"

#ifdef HAVE_FDRAWCMD_H

#include "types/fdrawsys_dev.h"
#include "Platform.h"
#include "fdrawcmd.h"

#include "DiskUtil.h"
#include "Options.h"
#include "VfdrawcmdSys.h"
#include "win32_error.h"

#include <cstring>
#include <memory>

static auto& opt_base = getOpt<int>("base");
static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_datarate = getOpt<DataRate>("datarate");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_encoding = getOpt<Encoding>("encoding");
static auto& opt_fdraw_rescue_mode = getOpt<bool>("fdraw_rescue_mode");
static auto& opt_gaps = getOpt<int>("gaps");
static auto& opt_newdrive = getOpt<int>("newdrive");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");
static auto& opt_repair = getOpt<int>("repair");
static auto& opt_rescans = getOpt<RetryPolicy>("rescans");
static auto& opt_retries = getOpt<RetryPolicy>("retries");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_steprate = getOpt<int>("steprate");

FdrawSysDevDisk::FdrawSysDevDisk(const std::string& path, std::unique_ptr<FdrawcmdSys> fdrawcmd)
        : m_fdrawcmd(std::move(fdrawcmd))
    {
        try
        {
            if (!m_fdrawcmd->FdReset())
                throw win32_error(GetLastError(), "Reset");

            SetMetadata(path);

            auto srt = (opt_steprate >= 0) ? opt_steprate : (opt_newdrive ? 0xd : 0x8);
            auto hut = 0x0f;
            auto hlt = opt_newdrive ? 0x0f : 0x7f;
            if (!m_fdrawcmd->Specify(srt, hut, hlt))
                throw win32_error(GetLastError(), "Specify");

            if (!m_fdrawcmd->SetMotorTimeout(0))
                throw win32_error(GetLastError(), "SetMotorTimeout");
            if (!m_fdrawcmd->Recalibrate())
                throw win32_error(GetLastError(), "Recalibrate");

            if (!opt_newdrive)
                if (!m_fdrawcmd->SetDiskCheck(false))
                    throw win32_error(GetLastError(), "SetDiskCheck");
        }
        catch (...)
        {
            throw util::exception("failed to initialise fdrawcmd.sys device");
        }
    }

bool FdrawSysDevDisk::supports_retries() const /*override*/
{
    return true;
}

bool FdrawSysDevDisk::supports_rescans() const /*override*/
{
    return m_fdrawcmd->GetVersion().value >= DriverVersion1_0_1_12; // Since new version can rescan.
}

bool FdrawSysDevDisk::preload(const Range& /*range*/, int /*cyl_step*/) /*override*/
{
    return false;
}

bool FdrawSysDevDisk::is_constant_disk() const /*override*/
{
    return false;
}


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
        static const VectorX<std::string> fdc_types{
            "Unknown", "Unknown1", "Normal", "Enhanced", "82077", "82077AA", "82078_44", "82078_64", "National" };
        static const VectorX<std::string> data_rates{
            "250K", "300K", "500K", "1M", "2M" };

        std::stringstream ss;
        for (auto i = 0, n = 0; i < data_rates.size(); ++i)
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
    {
        if (!m_fdrawcmd->Seek(with_head_seek_to))
            throw win32_error(GetLastError(), "Seek");
        FD_CMD_RESULT result;
        m_fdrawcmd->CmdReadId(cylhead.head, result);
    }
    if (!m_fdrawcmd->Seek(cylhead.cyl))
        throw win32_error(GetLastError(), "Seek");

    auto firstSectorSeen = 0;
    const bool read_first_gap_requested = opt_gaps >= GAPS_CLEAN;
    TimedAndPhysicalDualTrack timedAndPhysicalDualTrack;
    Track trackBefore112;
    TrackData bitstreamTrackData;
    bool usingScanner112 = opt_fdraw_rescue_mode && m_fdrawcmd->GetVersion().value >= DriverVersion1_0_1_12;
    if (usingScanner112)
        timedAndPhysicalDualTrack = BlindReadHeaders112(cylhead, deviceReadingPolicy);
    usingScanner112 &= m_lastEncoding == Encoding::MFM; // Currently it supports only MFM encoding.
    if (!usingScanner112)
        trackBefore112 = BlindReadHeaders(cylhead, firstSectorSeen);
    auto& track = usingScanner112 ? timedAndPhysicalDualTrack.finalAllInTrack : trackBefore112;

    if (opt_debug)
        for (int i = 0; i < track.size(); i++)
            util::cout << "load: track " << i << ". sector having ID " << track[i].header.sector << " and offset " << track[i].offset << "\n";

    if (!usingScanner112)
    {
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
        if (!m_fdrawcmd->SetEncRate(m_lastEncoding, m_lastDataRate))
            throw win32_error(GetLastError(), "SetEncRate");

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

            if (!m_fdrawcmd->SetEncRate(encoding, datarate))
                throw win32_error(GetLastError(), "SetEncRate");

            // Retry in case of spurious header CRC errors.
            for (auto i = 0; i <= opt_retries; ++i) // TODO originally opt_retries(=5), could be replaced with opt_encratedetect_retries?
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

    auto scan_size = intsizeof(FD_TIMED_SCAN_RESULT) + intsizeof(FD_TIMED_ID_HEADER) * MAX_SECTORS;
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

        tracktime = static_cast<int>(scan_result->tracktime);
        if (m_fdrawcmd->GetVersion().value < DriverVersion1_0_1_12)
        {
            int tracktimeCorrect;
            if (!m_fdrawcmd->FdGetTrackTime(tracktimeCorrect)) // This also fixes tracktime rarely corrupted by CmdTimedScan.
                throw win32_error(GetLastError_MP(), "GetTrackTime");
            constexpr auto tracktimeTolerance = 0.01;
            if (tracktime < tracktimeCorrect * (1 - tracktimeTolerance) || tracktime > tracktimeCorrect * (1 + tracktimeTolerance))
            {
                Message(msgWarning, "track time of %s is corrupted by older driver, upgrade it if possible. Retrying", strCH(cylhead.cyl, cylhead.head).c_str());
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
        const auto mfmbit_us = GetFmOrMfmBitsTime(m_lastDataRate);
        track.tracktime = tracktime;
        track.tracklen = round_AS<int>(track.tracktime / mfmbit_us);

        for (int i = 0; i < scan_result->count; ++i)
        {
            const auto& scan_header = scan_result->HeaderArray(i);
            Header header(scan_header.cyl, scan_header.head, scan_header.sector, scan_header.size);
            VerifyCylHeadsMatch(cylhead, header, false, opt_normal_disk);
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
    if (opt_debug)
        util::cout << "ReadSector: reading " << index << ". sector having ID " << sector.header.sector << "\n";

    if (sector.has_badidcrc() || sector.has_stable_data()) // Originally this was has_good_data(false, opt_normal_disk)) which did not consider 8k checksummable sector.
        return;

    auto size = Sector::SizeCodeToRealLength(sector.header.size);
    MEMORY mem(size);

    const auto readRetriesInit = opt_retries;
    auto readRetries = readRetriesInit;
    do // The reading loop.
    {
        // If the sector id occurs more than once on the track, synchronise to the correct one
        if (track.is_repeated(sector))
        {
            auto offset = (index + firstSectorSeen) % track.size();
            if (!m_fdrawcmd->FdSetSectorOffset(offset))
                throw win32_error(GetLastError(), "SetSectorOffset");
        }

        // Invalidate the content so misbehaving FDCs can be identififed.
        memset(mem.pb, 0xee, static_cast<size_t>(mem.size));

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
        VerifyCylHeadsMatch(cylhead, header, false, opt_normal_disk);
        Header resultHeader(result.cyl, result.head, result.sector, result.size);
        VerifyCylHeadsMatch(resultHeader, header, false, opt_normal_disk);
        if (opt_normal_disk && (result.sector != header.sector && result.sector != header.sector + 1))
        {
            MessageCPP(msgWarning, "sector's id.sector (", header.GetRecordAsString(),
                       ") does not match sector's id.sector (", resultHeader.GetRecordAsString(), "), ignoring this sector.");
            sector.add_read_attempts(1);
            continue;
        }

        bool data_crc_error =(result.st2 & STREG2_DATA_ERROR_IN_DATA_FIELD) != 0;
        uint8_t dam = (result.st2 & STREG2_CONTROL_MARK) ? IBM_DAM_DELETED : IBM_DAM;

        Data data(mem.pb, mem.pb + mem.size);
        sector.add(std::move(data), data_crc_error, dam);

        // If the read command was successful we're all done.
        if ((result.st0 & STREG0_INTERRUPT_CODE) == 0)
        {
            if (sector.has_stable_data())
                break;
            if (readRetries.sinceLastChange)
                readRetries = readRetriesInit;
            continue;
        }

        // Accept sectors that overlap the next field, as they're unlikely to succeed.
        if (track.data_overlap(sector))
            break;

        // Accept 8K sectors with a recognised checksum method.
        if (track.is_8k_sector() && !ChecksumMethods(mem.pb, size).empty())
            break;
    } while (readRetries-- > 0);
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
        memset(mem.pb, 0xee, static_cast<size_t>(mem.size));

        if (!m_fdrawcmd->CmdReadTrack(cylhead.head, 0, 0, 1, size_code, 1, mem))
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
            if (std::memcmp(data.data(), mem.pb, static_cast<size_t>(data.size())))
            {
                Message(msgWarning, "track read of %s doesn't match first sector content", strCH(cylhead.cyl, cylhead.head).c_str());
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
    VectorX<Encoding> encodings{ Encoding::MFM, Encoding::FM };
    if (m_lastEncoding != Encoding::Unknown)
        encodings.findAndMove(m_lastEncoding, 0);
    // Prefer higher datarates.
    VectorX<DataRate> dataRates{ DataRate::_1M, DataRate::_500K, DataRate::_300K, DataRate::_250K };
    if (m_lastDataRate != DataRate::Unknown)
        dataRates.findAndMove(m_lastDataRate, 0);
    for (auto encoding : encodings)
    {
        for (auto dataRate : dataRates)
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

TimedAndPhysicalDualTrack FdrawSysDevDisk::BlindReadHeaders112(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy)
{
    if (m_fdrawcmd->GetVersion().value < DriverVersion1_0_1_12)
        throw util::exception("BlindReadHeaders112 method requires driver version 1.12 at least");

    if (m_trackInfo[cylhead].trackTime <= 0)
    {   // Premeasuring tracktime once would be enough for constant speed drive but would not be for variable speed drives.
        // In addition CmdTimed*Scan measures tracktime only if it was not measured earlier although CmdTimed*Scan might adjust it.
        FD_MULTI_TRACK_TIME_RESULT track_time;
        if (!m_fdrawcmd->FdGetMultiTrackTime(track_time, 1))
            throw win32_error(GetLastError_MP(), "GetMultiTrackTime"); // "not available for this disk type"
        m_trackInfo[cylhead].trackTime = static_cast<int>(track_time.spintime);
        // variable speed is out of scope. however this solution supports variable speed unless m_trackInfo[all] is set as spintime.
    }

    TimedAndPhysicalDualTrack timedAndPhysicalDualTrack;
    // Find the sectors by scanning the floppy disk both timed and physical.
    const auto physicalTrackRescansInit = std::max(opt_rescans, opt_retries);
    auto physicalTrackRescans = physicalTrackRescansInit + 1; // +1 for the initial scanning.
    const auto timedTrackRescansInit = opt_rescans;
    DeviceReadingPolicy deviceReadingPolicyForScanning = deviceReadingPolicy;
    auto timedTrackRescans = timedTrackRescansInit;
    do // The scanning loop.
    {
        MultiScanResult multiScanResult(MAX_SECTORS);
        if (!ScanAndDetectIfNecessary(cylhead, multiScanResult))
            return timedAndPhysicalDualTrack;

        auto timedTrackTime = multiScanResult.trackTime();
        // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats
        // TODO What about Amiga HD disk with 150 RPM? Otherwise the condition is correct.
        if (timedTrackTime > RPM_TIME_200)
            throw util::diskspeedwrong_exception("index-halving cables are no longer supported (rpm <= 200)");
        m_trackInfo[cylhead].trackTime = timedTrackTime;
        if (m_lastEncoding != Encoding::MFM) // Currently only MFM encoding is supported due to ReadAndMergePhysicalTracks method.
            return timedAndPhysicalDualTrack;

        if (multiScanResult.count() > 0)
        {
            if (m_trackInfo[cylhead].trackLenIdeal <= 0)
            {
                physicalTrackRescans--;
                if (ReadAndMergePhysicalTracks(cylhead, timedAndPhysicalDualTrack)) // Found new id when trackLenIdeal was unknown.
                {
                    if (physicalTrackRescans.sinceLastChange)
                        physicalTrackRescans = physicalTrackRescansInit;
                }
            }
            Track newTimedTrack = multiScanResult.DecodeResult(cylhead, m_lastDataRate, m_lastEncoding);
            if (m_trackInfo[cylhead].trackLenIdeal > 0)
                newTimedTrack.setTrackLenAndNormaliseTrackTimeAndSectorOffsets(m_trackInfo[cylhead].trackLenIdeal);

            const auto sectorAmountPrev = timedAndPhysicalDualTrack.timedIdTrack.size();
            timedAndPhysicalDualTrack.timedIdTrack.add(std::move(newTimedTrack));
            if (timedAndPhysicalDualTrack.timedIdTrack.size() > sectorAmountPrev)
            {
                deviceReadingPolicyForScanning = deviceReadingPolicy;
                deviceReadingPolicyForScanning.AddSkippableSectors(timedAndPhysicalDualTrack.timedIdTrack.good_idcrc_sectors());
                if (timedTrackRescans.sinceLastChange)
                    timedTrackRescans = timedTrackRescansInit;
            }
        }
    } while (timedTrackRescans-- > 0 && deviceReadingPolicyForScanning.WantMoreSectors());
    // Out of space sectors at track end are not normal, discard them if option normal disk is specified.
    DiscardOutOfSpaceSectorsAtTrackEnd(timedAndPhysicalDualTrack.timedIdTrack);

    timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack = timedAndPhysicalDualTrack.timedIdTrack;

    // If more sectors are required then try to find sectors in the physical track as well.
    bool initialRound = true;
    do // The reading and scanning loop.
    {
        const auto endingRound = !initialRound && physicalTrackRescans <= 0;
        if (endingRound || !deviceReadingPolicyForScanning.WantMoreSectors()) // Ending round or do not want more sectors.
        {
            ReadSectors(cylhead, timedAndPhysicalDualTrack, deviceReadingPolicy);
            if (timedAndPhysicalDualTrack.finalAllInTrack.has_all_stable_data(deviceReadingPolicy.SkippableSectors()))
                break; // Scanning and reading is complete, all data is stable.
            if (endingRound)
                break; // Ending round so reading is finished.
        }
        if (!initialRound && physicalTrackRescans > 0)
        {
            physicalTrackRescans--;
            if (ReadAndMergePhysicalTracks(cylhead, timedAndPhysicalDualTrack)) // Found new id when trakLenIdeal was unknown.
            {
                if (physicalTrackRescans.sinceLastChange)
                    physicalTrackRescans = physicalTrackRescansInit;
            }
        }
        if (m_trackInfo[cylhead].trackLenIdeal > 0)
        {
            // Updates timedAndPhysicalDualTrack.{lastPhysicalTrackSingle, syncedTimedAndPhysicalTracks}.
            if (timedAndPhysicalDualTrack.SyncAndDemultiPhysicalToTimed(m_trackInfo[cylhead].trackLenIdeal)) // Found new valuable something.
            {
                if (physicalTrackRescans.sinceLastChange)
                    physicalTrackRescans = physicalTrackRescansInit;
            }
            if (timedAndPhysicalDualTrack.syncedTimedAndPhysicalTracks)
            {
                // Out of space sectors at track end are not normal, discard them if option normal disk is specified.
                DiscardOutOfSpaceSectorsAtTrackEnd(timedAndPhysicalDualTrack.lastPhysicalTrackSingle.track);
                const auto sectorAmountPrev = timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.size();
                // Merging ids of last time synced physical track single with ids of timed id data and physical track (timed track included) and with orphan guessed ids.
                auto timedIdDataAndPhysicalIdTrackLocal = timedAndPhysicalDualTrack.lastPhysicalTrackSingle.track.CopyWithoutSectorData();
                timedIdDataAndPhysicalIdTrackLocal.add(timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.CopyWithoutSectorData());
                GuessAndAddSectorIdsOfOrphans(timedAndPhysicalDualTrack.lastPhysicalTrackSingle, timedIdDataAndPhysicalIdTrackLocal);
                //Merging whole (ids (done earlier) and data of) timed id data and physical id track.
                timedIdDataAndPhysicalIdTrackLocal.add(std::move(timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack));
                // The merged result becomes the new timed id data and physical track.
                timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack = std::move(timedIdDataAndPhysicalIdTrackLocal);
                if (timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.size() > sectorAmountPrev)
                {
                    deviceReadingPolicyForScanning = deviceReadingPolicy;
                    deviceReadingPolicyForScanning.AddSkippableSectors(timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.good_idcrc_sectors());
                }
            }
        }
        initialRound = false;
    } while (true);

    return timedAndPhysicalDualTrack;
}

// Remove not normal sector headers at the track end. The track must not be orphan data track.
void FdrawSysDevDisk::DiscardOutOfSpaceSectorsAtTrackEnd(Track& track) const
{
    if (opt_normal_disk)
    {
        while (!track.empty())
        {
            auto it = track.rbegin();
            const auto& sector = *it;
            assert(sector.header.size != SIZECODE_UNKNOWN);
            const auto lengthWithoutOuterGaps = GetFmOrMfmSectorOverheadFromOffsetToDataCrcEnd(sector.datarate, sector.encoding, sector.size());
            if (sector.offset + DataBytePositionAsBitOffset(lengthWithoutOuterGaps, track.getEncoding()) < track.tracklen) // It fits, no more problem.
                break;
            if (opt_debug)
                util::cout << "DiscardOutOfSpaceSectorsAtTrackEnd: discarding sector (offset=" << sector.offset << ", id.sector=" << sector.header.sector << ")\n";
            track.sectors().erase(std::next(it).base());
        }
    }
}

void FdrawSysDevDisk::GuessAndAddSectorIdsOfOrphans(const OrphanDataCapableTrack& timeSyncedPhysicalTrackSingle, Track& track) const
{
    // If there is no cylhead mismatch and there are orphan datas then guess their sector id.
    if (timeSyncedPhysicalTrackSingle.cylheadMismatch || timeSyncedPhysicalTrackSingle.orphanDataTrack.empty())
        return;

    for (const auto& orphanDataSector : timeSyncedPhysicalTrackSingle.orphanDataTrack)
    {

        if (opt_debug)
            util::cout << "GuessAndAddSectorIdsOfOrphans: processing orphan sector (offset=" << orphanDataSector.offset << ", id.sector=" << orphanDataSector.header.sector << ")\n";

        // Find any sector which coheres, it does not matter if closest or not.
        const auto it = track.findSectorForDataFmOrMfm(orphanDataSector.offset, orphanDataSector.header.size, false);
        // If not found sector id then let us guess it.
        if (it == track.end()) // The end is dynamic since adding sector to track.
        {
            if (opt_debug)
                util::cout << "GuessAndAddSectorIdsOfOrphans: Orphan has no parent (offset=" << orphanDataSector.offset << ")\n";
            // Let us discover the track sector scheme once if possible.
            if (track.idAndOffsetPairs.empty() && !track.DiscoverTrackSectorScheme(RepeatedSectors()))
                return;
            // Missing sector id, try to find it in the found track sector scheme.
            const auto sectorId = orphanDataSector.FindParentSectorIdByOffset(track.idAndOffsetPairs, track.tracklen);
            if (sectorId >= 0)
            {
                // The data will be referred later by the parent sector so skip data.
                auto parentSector = orphanDataSector.CopyWithoutData(false); // Resets read_attempts.
                parentSector.header.sector = sectorId;
                parentSector.header.size = track[0].header.size;
                parentSector.offset = track.findReasonableIdOffsetForDataFmOrMfm(orphanDataSector.offset);
                if (opt_debug)
                    util::cout << "GuessAndAddSectorIdsOfOrphans: added sector as parent (offset=" << parentSector.offset << ", id.sector=" << parentSector.header.sector << ")\n";
                track.add(std::move(parentSector));
            }
        }
    }
}

void FdrawSysDevDisk::ReadSectors(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack, const DeviceReadingPolicy& deviceReadingPolicy)
{
    // Limiting sector reading as specified in case of normal disk request.
    const auto normal_sector_id_begin = opt_base > 0 ? opt_base : 1;
    const auto normal_sector_id_end = opt_sectors > 0 ? (normal_sector_id_begin + static_cast<int>(opt_sectors)) : 256;

    auto& track = timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack;
    const auto sectorFilterPredicate = [&deviceReadingPolicy, &track, normal_sector_id_begin, normal_sector_id_end](const Sector& sector)
    {
        return !deviceReadingPolicy.SkippableSectors().Contains(sector, track.tracklen, opt_repair > 0)
                && (!opt_normal_disk || (sector.header.sector >= normal_sector_id_begin && sector.header.sector < normal_sector_id_end));
    };
    const auto iSup = track.size();
    for (int j = 0; j < 2; j++)
    {
        for (int i = j; i < iSup; i += 2)
        {
            const auto& sector = track[i];
            if (sectorFilterPredicate(sector))
            {
                if (sector.read_attempts() == 0) // Read only if did not read earlier, it will read opt_retries times.
                {
                    if (opt_debug)
                        util::cout << "ReadSectors: reading officially " << i << ". sector (id.sector=" << sector.header.sector << ")\n";
                    ReadSector(cylhead, track, i, 0);
                }
            }
        }
    }
    timedAndPhysicalDualTrack.finalAllInTrack = track;
    if (!timedAndPhysicalDualTrack.lastPhysicalTrackSingle.cylheadMismatch && timedAndPhysicalDualTrack.syncedTimedAndPhysicalTracks)
        timedAndPhysicalDualTrack.lastPhysicalTrackSingle.MergeInto(timedAndPhysicalDualTrack.finalAllInTrack, sectorFilterPredicate);
}

bool FdrawSysDevDisk::ReadAndMergePhysicalTracks(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack)
{
    assert(m_lastEncoding == Encoding::MFM); // Currently this method handles only MFM track.
    assert(m_lastDataRate != DataRate::Unknown);
    MEMORY mem;

    if (!m_fdrawcmd->CmdReadTrack(cylhead.head, cylhead.cyl, cylhead.head, 1, 8, 1, mem)) // Read one big 32K sector.
    {
        MessageCPP(msgWarningAlways, "Could not read ", strCH(cylhead.cyl, cylhead.head),
            " at once, it is either blank or prevents from being read");
        return false;
    }
    PhysicalTrackMFM toBeMergedPhysicalTrack(mem, m_lastDataRate);
    const auto sectorIdAmountPrev = timedAndPhysicalDualTrack.physicalTrackMulti.track.size();
    timedAndPhysicalDualTrack.physicalTrackMulti.MergePhysicalTrack(cylhead, toBeMergedPhysicalTrack);
    if (timedAndPhysicalDualTrack.physicalTrackMulti.cylheadMismatch)
        throw util::diskforeigncylhead_exception(make_string(
                                                     "cyl head mismatch found during processing physical track"));
    const bool foundNewSectorId = timedAndPhysicalDualTrack.physicalTrackMulti.track.size() > sectorIdAmountPrev;
    if (foundNewSectorId && m_trackInfo[cylhead].trackLenIdeal <= 0) // Found new sector id so there is a chance for determining best tracklen.
    {
        const auto bestTrackLen = timedAndPhysicalDualTrack.physicalTrackMulti.determineBestTrackLen(GetFmOrMfmTimeBitsAsRounded(m_lastDataRate, m_trackInfo[cylhead].trackTime));
        if (bestTrackLen > 0)
        {
            m_trackInfo[cylhead].trackLenIdeal = bestTrackLen;
            if (timedAndPhysicalDualTrack.timedIdTrack.tracklen > 0)
                timedAndPhysicalDualTrack.timedIdTrack.setTrackLenAndNormaliseTrackTimeAndSectorOffsets(m_trackInfo[cylhead].trackLenIdeal);
        }
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool ReadFdrawcmdSys(const std::string& path, std::shared_ptr<Disk>& disk)
{
    const auto devidx = (util::lowercase(path) == "b:") ? 1 : 0;
    const auto virtualFloppyDevice = IsVfd(path);
    if (!virtualFloppyDevice && !IsFloppyDevice(path))
        return false;
    const auto virtualFloppyDevicePath = virtualFloppyDevice && path.length() > 4 ? path.substr(4) : "";
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
