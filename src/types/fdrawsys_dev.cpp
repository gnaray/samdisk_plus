// fdrawcmd.sys real device wrapper:
//  http://simonowen.com/fdrawcmd/

#include "config.h"

#ifdef HAVE_FDRAWCMD_H

#include "types/fdrawsys_dev.h"
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
                throw win32_error(GetLastError_MP(), "Reset");

            SetMetadata(path);

            auto srt = (opt_steprate >= 0) ? opt_steprate : (opt_newdrive ? 0xd : 0x8);
            auto hut = 0x0f;
            auto hlt = opt_newdrive ? 0x0f : 0x7f;
            if (!m_fdrawcmd->Specify(srt, hut, hlt))
                throw win32_error(GetLastError_MP(), "Specify");

            if (!m_fdrawcmd->SetMotorTimeout(0))
                throw win32_error(GetLastError_MP(), "SetMotorTimeout");

            if (!opt_newdrive)
                if (!m_fdrawcmd->SetDiskCheck(false))
                    throw win32_error(GetLastError_MP(), "SetDiskCheck");

            if (!m_fdrawcmd->Recalibrate())
                throw win32_error(GetLastError_MP(), "Recalibrate");
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
            throw win32_error(GetLastError_MP(), "Seek");
        FD_CMD_RESULT result;
        m_fdrawcmd->CmdReadId(cylhead.head, result);
    }
    if (!m_fdrawcmd->Seek(cylhead.cyl))
        throw win32_error(GetLastError_MP(), "Seek");

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

    if (opt_debug >= 2)
        util::cout << "load: showing track\n" << track.ToString(false) << "\n";

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
            throw win32_error(GetLastError_MP(), "SetEncRate");

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
                throw win32_error(GetLastError_MP(), "SetEncRate");

            // Retry in case of spurious header CRC errors.
            // TODO originally opt_retries(=5), could be replaced with opt_encratedetect_retries?
            for (auto retries = opt_retries + 1; retries.HasMoreRetryMinusMinus(); ) // +1 since prechecking the value in the loop.
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

    track.tracktime = tracktime;
    if (scan_result->count > 0) // In case of blank track the encoding and datarate are unsure.
    {
        const auto offsetAdjuster = FdrawMeasuredOffsetAdjuster(m_lastEncoding);
        const auto mfmbit_us = GetFmOrMfmBitsTime(m_lastDataRate);
        track.tracklen = round_AS<int>(track.tracktime / mfmbit_us);

        for (int i = 0; i < scan_result->count; ++i)
        {
            const auto& scan_header = scan_result->HeaderArray(i);
            Header header(scan_header.cyl, scan_header.head, scan_header.sector, scan_header.size);
            VerifyCylHeadsMatch(cylhead, header, false, opt_normal_disk);
            Sector sector(m_lastDataRate, m_lastEncoding, header);

            sector.offset = modulo(round_AS<int>(static_cast<int>(scan_header.reltime) / mfmbit_us - offsetAdjuster), track.tracklen);
            assert(sector.offset > 0);
            sector.set_constant_disk(false);
            track.add(std::move(sector));
        }
    }

    return track;
}

void FdrawSysDevDisk::ReadSector(const CylHead& cylhead, Track& track, int index, int firstSectorSeen)
{
    ReadSectors(cylhead, track, VectorX<int>{index}, firstSectorSeen);
}

void FdrawSysDevDisk::ReadSectors(const CylHead& cylhead, Track& track, const VectorX<int>& indices, int firstSectorSeen)
{
    const auto iSup = indices.size();
    // The retryTimes * 5 is for balancing between reading a sector and a whole track.
    // The use case is reading a DD track of 10 sectors by CmdReadTrack which
    // results in 28672/6250=4,588 revolutions on 1 retrying, so each sector is
    // read almost 5 times per 1 retry by that. Here without * 5 a sector would
    // be read only once per 1 retry.
    // Of course the * 5 should be * 2 when reading a HD track where 2,294
    // revolution is calculated. TODO Consider datarate for the multiplier.
    RetryPolicy retriesSpecial(opt_retries.retryTimes * 5 + 1, opt_retries.GetSinceLastChange()); // +1 since prechecking the value in the loop.
    VectorX<RetryPolicy> sectorRetries(iSup, retriesSpecial);
    bool thereWasRetry;
    do // The reading loop.
    {
        thereWasRetry = false;
        for (auto i = 0; i < iSup; sectorRetries[i]--, i++)
        {
            if (!sectorRetries[i].HasMoreRetry())
                continue;
            thereWasRetry = true;
            const auto index = indices[i];
            auto& sector = track[index];
            if (opt_debug)
                util::cout << "ReadSector: reading " << index << ". sector having ID " << sector.header.sector << "\n";

            if (sector.has_badidcrc() || sector.has_stable_data()) // Originally this was has_good_data(false, opt_normal_disk)) which did not consider 8k checksummable sector.
            {
                sectorRetries[i] = 0;
                continue; //return
            }

            auto size = Sector::SizeCodeToRealLength(sector.header.size);
            MEMORY mem(size);

            // If the sector id occurs more than once on the track, synchronise to the correct one
            if (track.is_repeated(sector))
            {
                auto offset = (index + firstSectorSeen) % track.size();
                if (!m_fdrawcmd->FdSetSectorOffset(offset))
                    throw win32_error(GetLastError_MP(), "SetSectorOffset");
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

            VerifyCylHeadsMatch(cylhead, header, false, opt_normal_disk);
            Header resultHeader(result.cyl, result.head, result.sector, result.size);
            VerifyCylHeadsMatch(resultHeader, header, false, opt_normal_disk);

            const auto data_crc_error = (result.st2 & STREG2_DATA_ERROR_IN_DATA_FIELD) != 0;
            const auto id_crc_error = !data_crc_error && result.st1 & STREG1_DATA_ERROR;
            if (id_crc_error)
                continue;

            const auto anyError = data_crc_error;
            uint8_t dam = (result.st2 & STREG2_CONTROL_MARK) ? IBM_DAM_DELETED : IBM_DAM;
            const auto expectedHeaderSector = header.sector + (anyError ? 0 : 1);
            if (result.sector != expectedHeaderSector)
            {
                MessageCPP(msgWarningAlways, "Read sector (", header,
                    ") returned result which differs from expected id (", expectedHeaderSector,
                    "), ignoring its read data");
                sector.add_read_attempts(1);
                sectorRetries[i] = 0;
                continue; //break
            }

            Data data(mem.pb, mem.pb + mem.size);
            sector.add(std::move(data), data_crc_error, dam);

            // If the read command was successful we're all done.
            if ((result.st0 & STREG0_INTERRUPT_CODE) == 0)
            {
                if (sector.has_stable_data())
                {
                    sectorRetries[i] = 0;
                    continue; //break
                }
                sectorRetries[i].wasChange = true;
                continue;
            }

            // Accept sectors that overlap the next field, as they're unlikely to succeed.
            if (track.data_overlap(sector))
            {
                sectorRetries[i] = 0;
                continue; //break
            }

            // Accept 8K sectors with a recognised checksum method.
            if (track.is_8k_sector() && !ChecksumMethods(mem.pb, size).empty())
            {
                sectorRetries[i] = 0;
                continue; //break
            }
        }
    } while (thereWasRetry);
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

    for (auto retries = opt_retries + 1; retries.HasMoreRetryMinusMinus(); ) // +1 since prechecking the value in the loop.
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
    const auto byte_tolerance_of_time = std::min(opt_byte_tolerance_of_time, 127);
    const bool trackInfoHasEncodingAndDataRate = m_trackInfo[cylhead].encoding != Encoding::Unknown && m_trackInfo[cylhead].dataRate != DataRate::Unknown;
    // Prefer MFM to FM.
    VectorX<Encoding> encodings{ Encoding::MFM, Encoding::FM };
    // Prefer higher datarates.
    VectorX<DataRate> dataRates{ DataRate::_250K, DataRate::_1M, DataRate::_500K, DataRate::_300K };

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
        if (!m_fdrawcmd->CmdTimedMultiScan(cylhead.head, 0, multiScanResult, multiScanResult.size, byte_tolerance_of_time))
            throw win32_error(GetLastError_MP(), "TimedMultiScan");
        if (trackInfoHasEncodingAndDataRate) // No matter if sector is found because earlier found so detection is successful.
            goto Success;
        // Return true if we found a sector.
        if (multiScanResult.count() > 0) // Found sector so detection is successful.
        {
            m_trackInfo[cylhead].encoding = m_lastEncoding;
            m_trackInfo[cylhead].dataRate = m_lastDataRate;
            goto Success;
        }
    }

    if (m_lastEncoding != Encoding::Unknown)
        encodings.findAndMove(m_lastEncoding, 0);
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

            if (!m_fdrawcmd->CmdTimedMultiScan(cylhead.head, 0, multiScanResult, multiScanResult.size, byte_tolerance_of_time))
                throw win32_error(GetLastError_MP(), "TimedMultiScan");
            if (multiScanResult.count() > 0)
            {
                // Remember the settings for the first try next time.
                m_lastEncoding = encoding;
                m_lastDataRate = dataRate;
                m_trackInfo[cylhead].encoding = encoding;
                m_trackInfo[cylhead].dataRate = dataRate;
                goto Success;
            }
        }
    }

    return false; // Nothing detected.

Success:
    if (m_trackInfo[cylhead].trackTime <= 0)
    {
        // Measuring tracktime now as some drives requires known datarate and encoding.
        // This solution supports variable speed drives.
        FD_MULTI_TRACK_TIME_RESULT track_time;
        if (!m_fdrawcmd->FdGetMultiTrackTime(track_time, 1))
            throw win32_error(GetLastError_MP(), "GetMultiTrackTime"); // "not available for this disk type"
        m_trackInfo[cylhead].trackTime = static_cast<int>(track_time.spintime);
        if (opt_debug >= 1)
            util::cout << "ScanAndDetectIfNecessary: measured track time " << m_trackInfo[cylhead].trackTime << "\n";
    }
    // TimedMultiScan's tracktime must be adjusted because both
    // a) The TimedMultiScan does not modify tracktime, instead the method simply returns it.
    // b) Either
    //    1) TimedMultiScan has previously measured probably wrong tracktime when datarate was unknown.
    //    2) A previous track is scanned again but its tracktime differs due to the drive having variable speed.
    multiScanResult.SetTrackTime(m_trackInfo[cylhead].trackTime);
    return true;
}

TimedAndPhysicalDualTrack FdrawSysDevDisk::RescueTrack(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy)
{
    TimedAndPhysicalDualTrack timedAndPhysicalDualTrack;
    // Find the sectors by scanning the floppy disk both timed and physical.
    const auto physicalTrackRescansInit = std::max(opt_rescans, opt_retries); // TODO wrong
    auto deviceReadingPolicyForScanning = deviceReadingPolicy;
    auto timedTrackRescans = opt_rescans;
    auto startTimeScanningLoop = StartStopper("Scanning loop");
    do // The scanning loop.
    {
        if (opt_debug >= 1)
            util::cout << "BlindReadHeaders112: scanning loop begin, timedTrackRescans=" << timedTrackRescans << "\n";
        MultiScanResult multiScanResult(MAX_SECTORS);
        auto startTime = StartStopper("ScanAndDetectIfNecessary");
        if (!ScanAndDetectIfNecessary(cylhead, multiScanResult)) // Provides m_trackInfo[cylhead].trackTime.
            return timedAndPhysicalDualTrack;
        StopStopper(startTime, "ScanAndDetectIfNecessary");

        // https://en.wikipedia.org/wiki/List_of_floppy_disk_formats
        // TODO What about Amiga HD disk with 150 RPM? Otherwise the condition is correct.
        if (m_trackInfo[cylhead].trackTime > RPM_TIME_200)
            throw util::diskspeedwrong_exception("index-halving cables are no longer supported (rpm <= 200)");

        if (m_lastEncoding != Encoding::MFM) // Currently only MFM encoding is supported due to ReadAndMergePhysicalTracks method.
            return timedAndPhysicalDualTrack;

        if (multiScanResult.count() > 0) // In case of blank track the encoding and datarate are unsure.
        {
            auto newTimedTrack = multiScanResult.DecodeResult(cylhead, m_lastDataRate, m_lastEncoding);
            if (opt_debug >= 1)
                util::cout << "BlindReadHeaders112: scanned track time " << newTimedTrack.tracktime << " and track len " << newTimedTrack.tracklen << " while scanning\n";

            if (opt_debug >= 3)
            {
                util::cout << "BlindReadHeaders112: showing newTimedTrack:\n"
                    << newTimedTrack.ToString(false) << "\n";
                util::cout << "BlindReadHeaders112: showing timedIdTrack:\n"
                    << timedAndPhysicalDualTrack.timedIdTrack.ToString(false) << "\n";
            }
            newTimedTrack.CollectRepeatedSectorIdsInto(repeatedSectorIds);
            newTimedTrack.Validate(repeatedSectorIds);
            const auto sectorAmountPrev = timedAndPhysicalDualTrack.timedIdTrack.size();
            timedAndPhysicalDualTrack.timedIdTrack.TuneOffsetsToEachOtherByMin(newTimedTrack); // TODO Uses min(), would average() be better?
            timedAndPhysicalDualTrack.timedIdTrack.add(std::move(newTimedTrack));
            timedAndPhysicalDualTrack.timedIdTrack.CollectRepeatedSectorIdsInto(repeatedSectorIds);
            timedAndPhysicalDualTrack.timedIdTrack.Validate(repeatedSectorIds);
            if (timedAndPhysicalDualTrack.timedIdTrack.size() > sectorAmountPrev)
            {
                deviceReadingPolicyForScanning = deviceReadingPolicy;
                deviceReadingPolicyForScanning.AddSkippableSectors(timedAndPhysicalDualTrack.timedIdTrack.good_idcrc_sectors());
                timedTrackRescans.wasChange = true;
            }
        }
    } while (timedTrackRescans.HasMoreRetryMinusMinus() && deviceReadingPolicyForScanning.WantMoreSectors());
    StopStopper(startTimeScanningLoop, "Scanning loop");

    if (opt_debug >= 2)
    {
        util::cout << "BlindReadHeaders112: scanning loop end, showing timedIdTrack\n"
            << timedAndPhysicalDualTrack.timedIdTrack.ToString(false) << "\n";
    }

    do
    {
        if (timedAndPhysicalDualTrack.timedIdTrack.empty())
            break;
        timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack = timedAndPhysicalDualTrack.timedIdTrack;
        // Must precede AdjustSuspiciousOffsets.
        timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.DropSectorsFromNeighborCyls(cylhead, cyls());
        timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.AdjustSuspiciousOffsets(repeatedSectorIds);

        // If more sectors are required then try to find sectors in the physical track as well.
        auto physicalTrackRescans = physicalTrackRescansInit + 1; // +1 since prechecking the value in the loop.
        auto startTimeScanningReadingLoop = StartStopper("Scanning reading loop");
        do // The reading and scanning loop.
        {
            if (opt_debug >= 2)
                util::cout << "BlindReadHeaders112: scanning and reading loop begin, physicalTrackRescans=" << physicalTrackRescans << "\n";
            const auto endingRound = physicalTrackRescans <= 0;
            if (endingRound || !deviceReadingPolicyForScanning.WantMoreSectors()) // Ending round or do not want more sectors.
            {
                ReadSectors(cylhead, timedAndPhysicalDualTrack, deviceReadingPolicy, false);
                auto deviceReadingPolicyForReading = deviceReadingPolicy;
                deviceReadingPolicyForReading.AddSkippableSectors(timedAndPhysicalDualTrack.finalAllInTrack.stable_sectors());
                if (!deviceReadingPolicyForReading.WantMoreSectors())
                    break; // Scanning and reading is complete, all data is stable.
                if (endingRound)
                    break; // Ending round so reading is finished.
            }
            if (physicalTrackRescans.HasMoreRetry())
            {
                physicalTrackRescans--;
                auto startTimeReadAndMergePhysicalTracks = StartStopper("ReadAndMergePhysicalTracks");
                if (ReadAndMergePhysicalTracks(cylhead, timedAndPhysicalDualTrack)) // Found better scored track.
                    physicalTrackRescans.wasChange = true;
                StopStopper(startTimeReadAndMergePhysicalTracks, "ReadAndMergePhysicalTracks");
            }
            if (!timedAndPhysicalDualTrack.lastPhysicalTrackSingle.empty())
            {
                const auto sectorAmountPrev = timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.size();
                timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.add(timedAndPhysicalDualTrack.lastPhysicalTrackSingle.track.CopyWithoutSectorData());
                timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.MergeRepeatedSectors();
                if (timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.size() > sectorAmountPrev)
                {
                    deviceReadingPolicyForScanning = deviceReadingPolicy;
                    deviceReadingPolicyForScanning.AddSkippableSectors(timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.good_idcrc_sectors());
                }
            }
            if (opt_debug >= 2)
            {
                util::cout << "BlindReadHeaders112: scanning and reading loop end, showing timedIdDataAndPhysicalIdTrack\n"
                    << timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack.ToString(false) << "\n";
            }
            // TODO When lastPhysicalTrackSingle has an orphan data sector, it means there is no parent
            // sector id for that, OK. If e.g. its preceding sector is bad then the offset of our orphan
            // data sector might be too low or too high (shortly: wrong) thus it does not find its
            // parent sector id. The problem is that even if our orphan data sector with good offset is
            // found later, it will be merged into the one with wrong offset thus it is lost.
            // Would dropping our oprhan data sector help? Not really, because then we could drop one
            // with good offset thus the guesser could not find an id for it.
            // The solution would be using offset interval in Sector and extending it when merging the
            // other sector in it. Not a simple change.
        } while (true);
        StopStopper(startTimeScanningReadingLoop, "Scanning reading loop");
    } while (false);

    if (!timedAndPhysicalDualTrack.finalAllInTrack.empty())
    {
        auto deviceReadingPolicyForReading = deviceReadingPolicy;
        auto sectorIndexWithSectorIdAndOffset = timedAndPhysicalDualTrack.finalAllInTrack.FindSectorDetectingInvisibleFirstSector(repeatedSectorIds);
        if (sectorIndexWithSectorIdAndOffset.sectorIndex < 0)
            sectorIndexWithSectorIdAndOffset = timedAndPhysicalDualTrack.finalAllInTrack.FindCloseSectorPrecedingUnreadableFirstSector();
        if (sectorIndexWithSectorIdAndOffset.sectorIndex >= 0)
        {   // Find the found sector in skippable sectors.
            const auto& sectorDetector = timedAndPhysicalDualTrack.finalAllInTrack[sectorIndexWithSectorIdAndOffset.sectorIndex];
            if (sectorDetector.read_attempts() == 0)
            {
                const auto it = deviceReadingPolicyForReading.SkippableSectors().FindToleratedSameSector(
                    timedAndPhysicalDualTrack.finalAllInTrack[sectorIndexWithSectorIdAndOffset.sectorIndex],
                    opt_byte_tolerance_of_time, timedAndPhysicalDualTrack.finalAllInTrack.tracklen);
                // Remove the found sector from skippable sectors if it is there so it can be read.
                if (it != deviceReadingPolicyForReading.SkippableSectors().cend())
                    deviceReadingPolicyForReading.RemoveSkippableSector(it);
                // Unsetting wanted sectors so the found sector will be read (as well*).
                // *: If wanted sectors would be more flexible then adding the found sector to it would be enough.
                deviceReadingPolicyForReading.SetWantedSectorHeaderSectors(Interval<int>());
                ReadSectors(cylhead, timedAndPhysicalDualTrack, deviceReadingPolicyForReading, true);
            }
            timedAndPhysicalDualTrack.finalAllInTrack.MakeVisibleFirstSector(sectorIndexWithSectorIdAndOffset);
        }

        // Must precede AdjustSuspiciousOffsets and GuessAndAddSectorIdsOfOrphans.
        timedAndPhysicalDualTrack.finalAllInTrack.DropSectorsFromNeighborCyls(cylhead, cyls());
        timedAndPhysicalDualTrack.finalAllInTrack.AdjustSuspiciousOffsets(repeatedSectorIds, true, true);
        GuessAndAddSectorIdsOfOrphans(timedAndPhysicalDualTrack.lastPhysicalTrackSingle, timedAndPhysicalDualTrack.finalAllInTrack);
        timedAndPhysicalDualTrack.finalAllInTrack.EnsureNotAlmost0Offset();
        timedAndPhysicalDualTrack.finalAllInTrack.Validate(repeatedSectorIds);
    }

    return timedAndPhysicalDualTrack;
}

TimedAndPhysicalDualTrack FdrawSysDevDisk::BlindReadHeaders112(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy)
{
    if (m_fdrawcmd->GetVersion().value < DriverVersion1_0_1_12)
        throw util::exception("BlindReadHeaders112 method requires driver version 1.12 at least");
    constexpr auto iMax = 2;
    for (auto i = 0; i <= iMax; i++)
    {
        try
        {
            return RescueTrack(cylhead, deviceReadingPolicy);
        }
        catch (const util::overlappedrepeatedsector_exception& e)
        {   // Offset time syncing problem (thus neighbor repeated sectors appear), ignore loaded track and try again.
            MessageCPP(msgWarningAlways, e.what(), i == iMax ? ", skipping reading track" : ", restarting reading track (giving a chance to handle it)");
        }
        catch (const util::repeatedsector_exception& e)
        {   // Offset time syncing problem (thus neighbor repeated sectors appear), ignore loaded track and try again.
            MessageCPP(msgWarningAlways, e.what(), i == iMax ? ", skipping reading track" : ", restarting reading track (giving a chance to handle it)");
        }
    }

    return TimedAndPhysicalDualTrack();
}

// Sector must be orphan data sector.
// Return false if discovering track sector scheme failed.
bool FdrawSysDevDisk::GuessAndAddSectorId(const Sector& sector, Track& track) const
{
    // Let us discover the track sector scheme once if possible.
    if (track.idAndOffsetPairs.empty())
        if (!track.DiscoverTrackSectorScheme(repeatedSectorIds))
            return false;
    // Missing sector id, try to find it in the found track sector scheme.
    const auto sectorId = sector.FindParentSectorIdByOffset(track.idAndOffsetPairs, track.tracklen);
    if (sectorId >= 0)
    {
        // The data will be referred later by the parent sector so skip data.
        auto parentSector = sector.CopyWithoutData(false); // Resets read_attempts.
        parentSector.header.sector = sectorId;
        parentSector.header.size = track[0].header.size;
        parentSector.offset = track.findReasonableIdOffsetForDataFmOrMfm(sector.offset);
        if (opt_debug)
            util::cout << "GuessAndAddSectorId: added sector as parent (offset=" << parentSector.offset
            << ", id.sector=" << parentSector.header.sector << ")\n";
        track.add(std::move(parentSector));
    }
    else
    {
//        util::cout << track.ToString(false) << "\n";
        MessageCPP(msgWarningAlways, "GuessAndAddSectorId: discovered track but can not find the parent of sector (",
            sector, ") at offset (", sector.offset, ")");
    }
    return true;
}

void FdrawSysDevDisk::GuessAndAddSectorIdsOfOrphans(const OrphanDataCapableTrack& timeSyncedPhysicalTrackSingle, Track& track) const
{
    // If there is no cylhead mismatch and there are orphan datas then guess their sector id.
    if (timeSyncedPhysicalTrackSingle.orphanDataTrack.empty())
        return;

    const auto sectorAmount = track.size();
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
            if (!GuessAndAddSectorId(orphanDataSector, track))
            {
                MessageCPP(msgWarningAlways, "Could not discover track sector scheme");
                return;
            }
        }
    }
    if (track.size() > sectorAmount)
        track.MergeRepeatedSectors();
}

void FdrawSysDevDisk::ReadSectors(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack,
    const DeviceReadingPolicy& deviceReadingPolicy, const bool directlyIntoFinal)
{
    auto& track = directlyIntoFinal ? timedAndPhysicalDualTrack.finalAllInTrack
        : timedAndPhysicalDualTrack.timedIdDataAndPhysicalIdTrack;
    if (track.empty())
        return;

    // Limiting sector reading as specified in case of normal disk request.
    const auto normal_sector_id_begin = opt_base > 0 ? opt_base : 1;
    const auto normal_sector_id_end = opt_sectors > 0 ? (normal_sector_id_begin + static_cast<int>(opt_sectors)) : 256;
    const auto sectorFilterPredicate = [&deviceReadingPolicy, &track,
        normal_sector_id_begin, normal_sector_id_end](const Sector& sector)
    {
        // TODO Question: ignore offsets or not? When repairing, the repairer ignores offsets currently,
        // but when copying the offsets are considered. Passing opt_repairis better than nothing,
        // but we can not tell if loading this track will be used for repairing or not.
        return !deviceReadingPolicy.SkippableSectors().Contains(sector, track.tracklen, opt_repair > 0)
            && (deviceReadingPolicy.WantedSectorHeaderSectors().IsEmpty() || deviceReadingPolicy.IsWanted(sector.header.sector))
            && (!opt_normal_disk || (sector.header.sector >= normal_sector_id_begin && sector.header.sector < normal_sector_id_end));
    };
    const auto iSup = track.size();
    std::map<int, int> sectorMainIndex; // The goal is reading into same sector even if sectors vector is growing.
    for (auto i = 0; i < iSup; i++) // Store most read (main) index of each sector id.
    {
        const auto& sector = track[i];
        const auto it = sectorMainIndex.find(sector.header.sector);
        if (it == sectorMainIndex.end())
            sectorMainIndex.emplace(sector.header.sector, i);
        else if (sector.read_attempts() > track[it->second].read_attempts())
            it->second = i;
    }
    VectorX<int> indices;
    for (int j = 0; j < 2; j++)
    {
        auto c = j;
        for (int i = 0; i < iSup; i++)
        {   // Select each 2nd main index. When a sector is repeated, its main index is determined above.
            const auto& sector = track[i];
            if (sectorMainIndex.find(sector.header.sector)->second == i)
            {
                if (c == 0 && sector.read_attempts() == 0 && sectorFilterPredicate(sector)) // Read only if did not read earlier, it will read opt_retries times.
                    indices.push_back(i);
                c = 1 - c;
            }
        }
    }
    if (!indices.empty())
    {
        auto startTimeReadSector = StartStopper("ReadSectors");
        ReadSectors(cylhead, track, indices, 0);
        StopStopper(startTimeReadSector, "ReadSectors");
        if (opt_debug >= 2)
            util::cout << "ReadSectors: showing timedIdDataAndPhysicalIdTrack\n" << track.ToString(false) << "\n";
    }
    if (!directlyIntoFinal)
        timedAndPhysicalDualTrack.finalAllInTrack = track;
    if (!timedAndPhysicalDualTrack.lastPhysicalTrackSingle.empty())
    {
        timedAndPhysicalDualTrack.lastPhysicalTrackSingle.MergeInto(timedAndPhysicalDualTrack.finalAllInTrack, sectorFilterPredicate);
        timedAndPhysicalDualTrack.finalAllInTrack.MergeRepeatedSectors();
    }
}

bool FdrawSysDevDisk::ReadAndMergePhysicalTracks(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack)
{
    assert(m_lastEncoding == Encoding::MFM); // Currently this method handles only MFM track.
    assert(m_lastDataRate != DataRate::Unknown);
    MEMORY mem;

    if (!m_fdrawcmd->CmdReadTrack(cylhead.head, cylhead.cyl, cylhead.head, 1, 8, 1, mem)) // Read one big 32K sector.
    {
        MessageCPP(msgWarningAlways, "Could not read ", cylhead,
            " at once, it is either blank or prevents from being read");
        return false;
    }
    PhysicalTrackMFM toBeMergedPhysicalTrack(mem, m_lastDataRate);
    auto& destODCTrack = timedAndPhysicalDualTrack.lastPhysicalTrackSingle;

    auto toBeMergedODCTrack = toBeMergedPhysicalTrack.DecodeTrack(cylhead);
    const auto prevScore = timedAndPhysicalDualTrack.lastPhysicalTrackSingleScore;
    const auto foundBetterScore = timedAndPhysicalDualTrack.SyncDemultiMergePhysicalUsingTimed(
        std::move(toBeMergedODCTrack), repeatedSectorIds);

    if (destODCTrack.cylheadMismatch && opt_normal_disk)
        MessageCPP(msgWarningAlways, "Suspicious: ", cylhead, " does not match at least 1 sector's cyl head on physical track");
    if (foundBetterScore)
    {
        if (opt_debug >= 1)
            util::cout << "ReadAndMergePhysicalTracks: found better score "
                << timedAndPhysicalDualTrack.lastPhysicalTrackSingleScore << " than " << prevScore << "\n";
    }
    return foundBetterScore;
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
