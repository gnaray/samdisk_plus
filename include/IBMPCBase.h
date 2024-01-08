#pragma once

#include "Platform.h"
#include "Header.h"

constexpr uint8_t IBM_DAM_DELETED = 0xf8;
constexpr uint8_t IBM_DAM_DELETED_ALT = 0xf9;
constexpr uint8_t IBM_DAM_ALT = 0xfa;
constexpr uint8_t IBM_DAM = 0xfb;
constexpr uint8_t IBM_IAM = 0xfc;
constexpr uint8_t IBM_DAM_RX02 = 0xfd;
constexpr uint8_t IBM_IDAM = 0xfe;

constexpr int GAP2_MFM_ED = 41;     // gap2 for MFM 1Mbps (ED)
constexpr int GAP2_MFM_DDHD = 22;   // gap2 for MFM, except 1Mbps (ED)
constexpr int GAP2_FM = 11;         // gap2 for FM (same bit size as MFM due to encoding)

constexpr int SYNC_OVERHEAD_MFM = 12/*x0x00=sync*/; // gap before *am
constexpr int TRACK_OVERHEAD_MFM = 80/*x0x4e=gap4a*/ + SYNC_OVERHEAD_MFM + 4/*3x0c2+0xfc=iam*/ + 50/*x0x4e=gap1*/;   // = 146
constexpr int IDAM_OVERHEAD_MFM = 4/*3xa1+0xfe=idam*/;
constexpr int DAM_OVERHEAD_MFM = 4/*3x0xa1+0xfb=dam*/;
constexpr int ID_OVERHEAD_MFM = IDAM_OVERHEAD_MFM + 4/*CHRN*/ + 2/*crc*/;
constexpr int D_OVERHEAD_MFM = DAM_OVERHEAD_MFM /*+ data_size*/ + 2/*crc*/;
constexpr int SECTOR_OVERHEAD_MFM = SYNC_OVERHEAD_MFM + ID_OVERHEAD_MFM + 22/*x0x4e=gap2*/ +
                                    SYNC_OVERHEAD_MFM + D_OVERHEAD_MFM /*+ gap3*/; // = 62
constexpr int SECTOR_OVERHEAD_ED = GAP2_MFM_ED - GAP2_MFM_DDHD;

constexpr int SYNC_OVERHEAD_FM = 6/*x0x00=sync*/;
constexpr int TRACK_OVERHEAD_FM = 40/*x0x00=gap4a*/ + SYNC_OVERHEAD_FM + 1/*0xfc=iam*/ + 26/*x0x00=gap1*/;       // = 73
constexpr int IDAM_OVERHEAD_FM = 1/*0xfe=idam*/;
constexpr int DAM_OVERHEAD_FM = 1/*0xfb*/;
constexpr int ID_OVERHEAD_FM = IDAM_OVERHEAD_FM + 4/*CHRN*/ + 2/*crc*/;
constexpr int D_OVERHEAD_FM = DAM_OVERHEAD_FM /*+ data_size*/ + 2/*crc*/;
constexpr int SECTOR_OVERHEAD_FM = SYNC_OVERHEAD_FM + ID_OVERHEAD_FM + 11/*x0x00=gap2*/ +
                                   SYNC_OVERHEAD_FM + D_OVERHEAD_FM /*+ gap3*/; // = 33

constexpr int MIN_GAP3 = 1;
constexpr int MAX_GAP3 = 82;    // arbitrary size, to leave a bit more space at the track end

constexpr int RPM_200 = 200;
constexpr int RPM_300 = 300;
constexpr int RPM_360 = 360;
constexpr int MIRCOSEC_PER_MINUTE = 60'000'000;

constexpr double RPM_TIME_MICROSEC(double rpm)
{
    return MIRCOSEC_PER_MINUTE / rpm;
}

template<typename T,
         std::enable_if_t<!std::is_floating_point<T>::value, int> = 0>
inline T RPM_TIME_MICROSEC_AS(double rpm)
{
    return round_AS<T>(RPM_TIME_MICROSEC(rpm));
}

// The RPM_TIME_* do not use RPM_TIME_MICROSEC_AS function because it is not constexpr because of std::round.
constexpr int RPM_TIME_200 = MIRCOSEC_PER_MINUTE / RPM_200;
constexpr int RPM_TIME_300 = MIRCOSEC_PER_MINUTE / RPM_300;
constexpr int RPM_TIME_360 = MIRCOSEC_PER_MINUTE / RPM_360;

const auto SIZE_MASK_765 = 7U;

// uPD765 status register 0
constexpr uint8_t STREG0_INTERRUPT_CODE = 0xc0; // fdrawcmd: STREG0_END_MASK
constexpr uint8_t STREG0_SEEK_END = 0x20; // fdrawcmd: STREG0_SEEK_COMPLETE
constexpr uint8_t STREG0_EQUIPMENT_CHECK = 0x10; // fdrawcmd: STREG0_DRIVE_FAULT
constexpr uint8_t STREG0_NOT_READY = 0x08; // fdrawcmd: STREG0_DRIVE_NOT_READY
constexpr uint8_t STREG0_HEAD_ADDRESS = 0x04; // fdrawcmd: STREG0_HEAD
constexpr uint8_t STREG0_UNIT_SELECT_1 = 0x02; // fdrawcmd: STREG0_DRIVE_2
constexpr uint8_t STREG0_UNIT_SELECT_0 = 0x01; // fdrawcmd: STREG0_DRIVE_1

// uPD765 status register 1
constexpr uint8_t STREG1_END_OF_CYLINDER = 0x80; // fdrawcmd: STREG1_END_OF_DISKETTE
constexpr uint8_t STREG1_RESERVED_6 = 0x40; // fdrawcmd: STREG1_RESERVED2
constexpr uint8_t STREG1_DATA_ERROR = 0x20; // fdrawcmd: STREG1_CRC_ERROR
constexpr uint8_t STREG1_OVERRUN = 0x10; // fdrawcmd: STREG1_DATA_OVERRUN
constexpr uint8_t STREG1_RESERVED_3 = 0x08; // fdrawcmd: STREG1_RESERVED1
constexpr uint8_t STREG1_NO_DATA = 0x04; // fdrawcmd: STREG1_SECTOR_NOT_FOUND
constexpr uint8_t STREG1_NOT_WRITEABLE = 0x02; // fdrawcmd: STREG1_WRITE_PROTECTED
constexpr uint8_t STREG1_MISSING_ADDRESS_MARK = 0x01; // fdrawcmd: STREG1_ID_NOT_FOUND

// uPD765 status register 2
constexpr uint8_t STREG2_RESERVED_7 = 0x80; // fdrawcmd: STREG2_RESERVED
constexpr uint8_t STREG2_CONTROL_MARK = 0x40; // fdrawcmd: STREG2_DELETED_DATA
constexpr uint8_t STREG2_DATA_ERROR_IN_DATA_FIELD = 0x20; // fdrawcmd: STREG2_CRC_ERROR
constexpr uint8_t STREG2_WRONG_CYLINDER = 0x10; // fdrawcmd: STREG2_WRONG_CYLINDER
constexpr uint8_t STREG2_SCAN_EQUAL_HIT = 0x08; // fdrawcmd: STREG2_SCAN_EQUAL
constexpr uint8_t STREG2_SCAN_NOT_SATISFIED = 0x04; // fdrawcmd: STREG2_SCAN_FAIL
constexpr uint8_t STREG2_BAD_CYLINDER = 0x02; // fdrawcmd: STREG2_BAD_CYLINDER
constexpr uint8_t STREG2_MISSING_ADDRESS_MARK_IN_DATA_FIELD = 0x01; // fdrawcmd: STREG2_DATA_NOT_FOUND

/* This is a summarisation of fdcrawcmd driver setting last error depending on fdrawcmd status in CheckFdcResult.
 * It is not necessarily up to date but useful when handling errors produced by the driver.
 * fdrawcmd status => LastError
 * ----------------------------
 * STREG0_END_MASK == STREG0_END_NORMAL => STATUS_SUCCESS // ERROR_SUCCESS
 * STREG1_CRC_ERROR || STREG2_CRC_ERROR => STATUS_CRC_ERROR // ERROR_CRC
 * STREG1_DATA_OVERRUN => STATUS_DATA_OVERRUN // ERROR_IO_DEVICE
 * STREG1_SECTOR_NOT_FOUND || STREG1_END_OF_DISKETTE => STATUS_NONEXISTENT_SECTOR // ERROR_SECTOR_NOT_FOUND
 * STREG2_BAD_CYLINDER => STATUS_FLOPPY_WRONG_CYLINDER // ERROR_FLOPPY_WRONG_CYLINDER
 * STREG2_DATA_NOT_FOUND => STATUS_NONEXISTENT_SECTOR // ERROR_SECTOR_NOT_FOUND
 * STREG1_WRITE_PROTECTED => STATUS_MEDIA_WRITE_PROTECTED // ERROR_WRITE_PROTECT
 * STREG1_ID_NOT_FOUND => STATUS_FLOPPY_ID_MARK_NOT_FOUND // ERROR_FLOPPY_ID_MARK_NOT_FOUND
 * STREG2_WRONG_CYLINDER => STATUS_FLOPPY_WRONG_CYLINDER // ERROR_FLOPPY_WRONG_CYLINDER
 * otherwise => STATUS_FLOPPY_UNKNOWN_ERROR
 */

std::string to_string(const MEDIA_TYPE& type);

// Return the number of microseconds for given (default 1) bytes at the given rate.
// The calculation for add_drain_time is incomprehensible, luckily that parameter is never used.
inline double GetFmOrMfmDataBytesTime(DataRate datarate, Encoding encoding, int len_bytes = 1, bool add_drain_time = false)
{
    assert(datarate != DataRate::Unknown);
    const auto uTime = 1'000'000 * (encoding == Encoding::FM ? 2 : 1) / (bits_per_second(datarate) / 8.);
    return uTime * len_bytes + (add_drain_time ? (uTime * 69 / 100) : 0);     // 0.69 250Kbps bytes @300rpm = 86us = FDC data drain time
}

// Return the number of microseconds for given (default 1) mfmbits (halfbits) at the given rate.
inline double GetFmOrMfmDataBitsTime(DataRate datarate, Encoding encoding, int len_fmormfmbits = 1, bool add_drain_time = false)
// Return the number of bytes for given microseconds at the given rate.
inline int GetFmOrMfmBitTimeDataBytes(DataRate datarate, Encoding encoding, int time)
{
    return GetFmOrMfmDataBytesTime(datarate, encoding, len_fmormfmbits, add_drain_time) / 16;
    return round_AS<int>(time / GetFmOrMfmDataBytesTime(datarate, encoding));
}

{
}

// Return the number of mfmbits (halfbits) for given microseconds at the given rate.
inline int GetFmOrMfmBitTimeDataBits(DataRate datarate, Encoding encoding, int time)
{
    return round_AS<int>(time / GetFmOrMfmDataBitsTime(datarate, encoding));
}

int GetTrackOverhead(Encoding encoding);
int GetSectorOverhead(Encoding encoding);
int GetDataOverhead(Encoding encoding);
int GetSyncOverhead(Encoding encoding);
int GetRawTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding);
int GetTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding);
