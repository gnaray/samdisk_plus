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

constexpr int AM_CRC = 2;

constexpr int SYNC_OVERHEAD_MFM = 12/*x0x00=sync*/; // gap before *am
constexpr int TRACK_OVERHEAD_MFM = 80/*x0x4e=gap4a*/ + SYNC_OVERHEAD_MFM + 4/*3x0c2+0xfc=iam*/ + 50/*x0x4e=gap1*/;   // = 146
constexpr int IDAM_OVERHEAD_MFM = 4/*3xa1+0xfe=idam*/;
constexpr int DAM_OVERHEAD_MFM = 4/*3x0xa1+0xfb=dam*/;
constexpr int ID_OVERHEAD_MFM = IDAM_OVERHEAD_MFM + 4/*CHRN*/ + AM_CRC/*crc*/;
constexpr int D_OVERHEAD_MFM = DAM_OVERHEAD_MFM /*+ data_size*/ + AM_CRC/*crc*/;
constexpr int SECTOR_OVERHEAD_MFM = SYNC_OVERHEAD_MFM + ID_OVERHEAD_MFM + GAP2_MFM_DDHD/*x0x4e=gap2*/ +
                                    SYNC_OVERHEAD_MFM + D_OVERHEAD_MFM /*+ gap3*/; // = 62
constexpr int SECTOR_OVERHEAD_ED = SECTOR_OVERHEAD_MFM - GAP2_MFM_DDHD + GAP2_MFM_ED;

constexpr int SYNC_OVERHEAD_FM = 6/*x0x00=sync*/;
constexpr int TRACK_OVERHEAD_FM = 40/*x0x00=gap4a*/ + SYNC_OVERHEAD_FM + 1/*0xfc=iam*/ + 26/*x0x00=gap1*/;       // = 73
constexpr int IDAM_OVERHEAD_FM = 1/*0xfe=idam*/;
constexpr int DAM_OVERHEAD_FM = 1/*0xfb*/;
constexpr int ID_OVERHEAD_FM = IDAM_OVERHEAD_FM + 4/*CHRN*/ + AM_CRC/*crc*/;
constexpr int D_OVERHEAD_FM = DAM_OVERHEAD_FM /*+ data_size*/ + AM_CRC/*crc*/;
constexpr int SECTOR_OVERHEAD_FM = SYNC_OVERHEAD_FM + ID_OVERHEAD_FM + GAP2_FM/*x0x00=gap2*/ +
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
    // Ignoring add_drain_time and considering len_bytes=1, the result is one of {8, 16, 26.666, 53.333, 32, 64}.
    assert(datarate != DataRate::Unknown);
    const auto uTime = 1'000'000 * (encoding == Encoding::FM ? 2 : 1) / (bits_per_second(datarate) / 8.);
    return uTime * len_bytes + (add_drain_time ? (uTime * 69 / 100) : 0);     // 0.69 250Kbps bytes @300rpm = 86us = FDC data drain time
}

// Return the number of microseconds for given (default 1) mfmbits (halfbits) at the given rate.
inline double GetFmOrMfmDataBitsTime(DataRate datarate, Encoding encoding, int len_fmormfmbits = 1, bool add_drain_time = false)
{
    return GetFmOrMfmDataBytesTime(datarate, encoding, len_fmormfmbits, add_drain_time) / 16;
}

// Return the number of microseconds as rounded integer for given (default 1) mfmbits (halfbits) at the given rate.
inline int GetFmOrMfmDataBitsTimeAsRounded(DataRate datarate, Encoding encoding, int len_fmormfmbits = 1, bool add_drain_time = false)
{
    // Rounding happens if encoding is MFM and datarate is 1 Mbps and len_fmormfmbits is odd, or if datarate is 300 Kbps and len_fmormfmbits mod 3 is not 0.
    return round_AS<int>(GetFmOrMfmDataBitsTime(datarate, encoding, len_fmormfmbits, add_drain_time));
}

// Return the number of bytes as rounded integer for given microseconds at the given rate.
// Carefully using this method because it returns a rounded result which can be much rounded at low datarate and two digits time.
inline int GetFmOrMfmTimeDataBytesAsRounded(DataRate datarate, Encoding encoding, int time)
{
    return round_AS<int>(time / GetFmOrMfmDataBytesTime(datarate, encoding));
}

// Return the number of mfmbits (halfbits) as rounded integer for given microseconds at the given rate.
// Carefully using this method because it returns a rounded result which can be much rounded at low datarate and one digit time.
inline int GetFmOrMfmTimeDataBitsAsRounded(DataRate datarate, Encoding encoding, int time)
{
    return round_AS<int>(time / GetFmOrMfmDataBitsTime(datarate, encoding));
}

int GetTrackOverhead(Encoding encoding);
int GetSectorOverhead(Encoding encoding);
int GetDataOverhead(Encoding encoding);
int GetSyncOverhead(Encoding encoding);
int GetRawTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding);
int GetTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding);

inline int GetIdamOverhead(const Encoding& encoding)
{
    return encoding == Encoding::FM ? IDAM_OVERHEAD_FM : IDAM_OVERHEAD_MFM;
}

inline int GetIdamOverheadSyncOverhead(const Encoding& encoding)
{
    return encoding == Encoding::FM ? IDAM_OVERHEAD_FM - 1 : IDAM_OVERHEAD_MFM - 1;
}

inline int GetDamOverhead(const Encoding& encoding)
{
    return encoding == Encoding::FM ? DAM_OVERHEAD_FM : DAM_OVERHEAD_MFM;
}

inline int GetDamOverheadSyncOverhead(const Encoding& encoding)
{
    return encoding == Encoding::FM ? DAM_OVERHEAD_FM - 1 : DAM_OVERHEAD_MFM - 1;
}

inline int GetIdOverhead(const Encoding& encoding)
{
    return encoding == Encoding::FM ? ID_OVERHEAD_FM : ID_OVERHEAD_MFM;
}

inline int GetDOverhead(const Encoding& encoding, const int dataSize = 0)
{
    return (encoding == Encoding::FM ? D_OVERHEAD_FM : D_OVERHEAD_MFM) + dataSize;
}

inline int GetFmOrMfmSyncLength(const Encoding& encoding, bool short_mfm_gap = false) // short_mfm_gap is about gap3 (after data crc) and sync (before IDAM overhead).
{
    return (encoding == Encoding::FM) ? SYNC_OVERHEAD_FM : (short_mfm_gap ? 3 : SYNC_OVERHEAD_MFM);
}

inline int GetFmOrMfmGap2Length(const DataRate& datarate, const Encoding& encoding)
{
    return (encoding == Encoding::FM) ? GAP2_FM : (datarate == DataRate::_1M) ? GAP2_MFM_ED : GAP2_MFM_DDHD;
}

inline int GetFmOrMfmGap3Length(bool short_mfm_gap = false)
{
    return short_mfm_gap ? MIN_GAP3 : 25; // Why 25? Usually min 24, sometimes 40. Using 25 as in IBMPC/FitTrackIBMPC.
}

inline int GetFmOrMfmGap3LengthMax()
{
    return 40; // 40 is based on docs. It can be increased if needed but not above MAX_GAP3.
}

inline int GetFmOrMfmGap2PlusSyncLength(const DataRate& datarate, const Encoding& encoding)
{
    return GetFmOrMfmGap2Length(datarate, encoding) + GetFmOrMfmSyncLength(encoding); // Gap2 not to be shorted.
}

inline int GetFmOrMfmGap3PlusSyncLength(const Encoding& encoding, bool short_mfm_gap = false)
{
    return GetFmOrMfmGap3Length(short_mfm_gap) + GetFmOrMfmSyncLength(encoding, short_mfm_gap);
}

inline int GetFmOrMfmGap3PlusSyncLengthMax(const Encoding& encoding)
{
    return GetFmOrMfmGap3LengthMax() + GetFmOrMfmSyncLength(encoding);
}

inline int GetFmOrMfmSectorOverheadWithoutSync(const DataRate& datarate, const Encoding& encoding, const int dataSize = 0)
{
    return GetIdOverhead(encoding) + GetFmOrMfmGap2PlusSyncLength(datarate, encoding) + GetDOverhead(encoding) + dataSize;
}

inline int GetFmOrMfmSectorOverheadWithoutSyncAndDataCrc(const DataRate& datarate, const Encoding& encoding, const int dataSize = 0)
{
    return GetFmOrMfmSectorOverheadWithoutSync(datarate, encoding, dataSize) - AM_CRC;
}

inline int GetFmOrMfmSectorOverheadFromOffsetToDataCrcEnd(const DataRate& datarate, const Encoding& encoding, const int dataSize = 0)
{
    return GetFmOrMfmSectorOverheadWithoutSync(datarate, encoding, dataSize) - GetIdamOverheadSyncOverhead(encoding);
}

inline int GetFmOrMfmIdamAndAmDistance(const DataRate& datarate, const Encoding& encoding)
{
    return GetIdOverhead(encoding) - GetIdamOverheadSyncOverhead(encoding)
            + GetFmOrMfmGap2PlusSyncLength(datarate, encoding) + GetDamOverheadSyncOverhead(encoding);
}

inline int GetFmOrMfmSectorOverhead(const DataRate& datarate, const Encoding& encoding, const int dataSize = 0, bool short_mfm_gap = false)
{
    return GetFmOrMfmSyncLength(encoding, short_mfm_gap) + GetFmOrMfmSectorOverheadWithoutSync(datarate, encoding, dataSize);
}

inline int GetFmOrMfmSectorOverheadWithGap3(const DataRate& datarate, const Encoding& encoding, const int dataSize = 0, bool short_mfm_gap = false)
{
    return GetFmOrMfmSectorOverhead(datarate, encoding, dataSize, short_mfm_gap) + GetFmOrMfmGap3Length(short_mfm_gap);
}
