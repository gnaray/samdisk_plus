// Calculations related to IBM PC format MFM/FM disks (System/34 compatible)

#include "PlatformConfig.h"
#include "IBMPCBase.h"
#include "Options.h"

#include <algorithm>

static auto& opt_debug = getOpt<int>("debug");
static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_verbose = getOpt<int>("verbose");

std::string to_string(const MEDIA_TYPE& type)
{
    switch (type)
    {
    case F5_1Pt2_512:   return "5.25\" 1.2M";
    case F3_1Pt44_512:  return "3.5\" 1.44M";
    case F3_2Pt88_512:  return "3.5\" 2.88M";
    case F3_720_512:    return "3.5\" 720K";
    case F5_360_512:    return "5.25\" 360K";
    case F5_320_512:    return "5.25\" 320K";
    case F5_320_1024:   return "5.25\" 320K, 1024 bytes/sector";
    case F5_180_512:    return "5.25\" 180K";
    case F5_160_512:    return "5.25\" 160K";
    case F3_640_512:    return "3.5\" 640K";
    case F5_640_512:    return "5.25\" 640K";
    case F5_720_512:    return "5.25\" 720K";
    case F3_1Pt2_512:   return "3.5\" 1.2M";
    case F3_1Pt23_1024: return "3.5\" 1.23M, 1024 bytes/sector";
    case F5_1Pt23_1024: return "5.25\" 1.23M, 1024 bytes/sector";
    case F8_256_128:    return "8\" 256K";
    default:            return "Unknown";
    }
}

int GetTrackOverhead(Encoding encoding)
{
    return (encoding == Encoding::MFM) ? TRACK_OVERHEAD_MFM : TRACK_OVERHEAD_FM;
}

int GetSectorOverhead(Encoding encoding)
{
    return (encoding == Encoding::MFM) ? SECTOR_OVERHEAD_MFM : SECTOR_OVERHEAD_FM;
}

int GetDataOverhead(Encoding encoding)
{
    return (encoding == Encoding::MFM) ? DAM_OVERHEAD_MFM : DAM_OVERHEAD_FM;
}

int GetSyncOverhead(Encoding encoding)
{
    return (encoding == Encoding::MFM) ? SYNC_OVERHEAD_MFM : SYNC_OVERHEAD_FM;
}

// The revolution_time_ms (earlier rpm_time, earlier drive_speed) which is the time duration while the disk rotates once.
// The revolution_time_ms of 360 RPM is 166666,6... floored to int as 166666.
int GetRawTrackCapacity(int revolution_time_ms, DataRate datarate, Encoding encoding)
{
    auto len_bits = bits_per_second(datarate);
    assert(len_bits != 0);

    // Max len_bits is 1000000, max revolution_time_ms is 300000 (at RPM 200).
    // Thus operator order is important to avoid overflow (of int assumed to be 32 bits).
    // Dividing by 1000 two times instead of once to avoid overflow for sure.
    auto len_bytes = len_bits / 1000 * revolution_time_ms / 1000 / 8;
    if (encoding == Encoding::FM) len_bytes >>= 1;
    return len_bytes;
}

int GetTrackCapacity(int revolution_time_ms, DataRate datarate, Encoding encoding)
{
    return GetRawTrackCapacity(revolution_time_ms, datarate, encoding) * 1995 / 2000;
}


void GetSectorIdAndDataOffsetDistanceMinMax(const DataRate& dataRate, const Encoding& encoding, int& distanceMin, int &distanceMax)
{
    // This code is taken from Samdisk/BitstreamDecoder where every databit is
    // stored as two bits (in addition every FM encoded bit is stored as two rawbits).
    // We also calculate with bits here though the code is slightly modified.
    const auto gap2_size_min = GetFmOrMfmGap2Length(dataRate, encoding);
    const auto idam_am_distance = GetFmOrMfmIdamAndDamDistance(dataRate, encoding);
    distanceMin = DataBytePositionAsBitOffset(GetIdOverheadWithoutIdamOverheadSyncOverhead(encoding) + gap2_size_min, encoding); // IDAM, ID, gap2 (without sync and DAM.a1sync, why?)
    distanceMax = DataBytePositionAsBitOffset(idam_am_distance + 8, encoding); // IDAM, ID, gap2, sync, DAM.a1sync (gap2: WD177x offset, +8: gap2 may be longer when formatted by different type of controller)
    const auto toleratedOffsetDistance = tolerated_offset_distance(encoding, opt_byte_tolerance_of_time);
    distanceMin -= DataBytePositionAsBitOffset(2, encoding);
    distanceMax += toleratedOffsetDistance; // Experimental solution. It is required to counterbalance offset sliding when reading physical track. +26 bytes were not enough.
}

CohereResult DoSectorIdAndDataOffsetsCohere(
    const int sectorIdOffset, const int dataOffset, const DataRate& dataRate, const Encoding& encoding, const int trackLen)
{
    assert(trackLen > 0);

    int min_distance, max_distance;
    GetSectorIdAndDataOffsetDistanceMinMax(dataRate, encoding, min_distance, max_distance);

    const auto sectorIdAndDataOffsetDistance = dataOffset - sectorIdOffset;
    if (sectorIdAndDataOffsetDistance < max_distance)
    {
        const auto sectorIdAndDataOffsetDistanceWrapped = modulo(sectorIdAndDataOffsetDistance, trackLen);
        if (sectorIdAndDataOffsetDistanceWrapped < min_distance // dataOffset too low, need to check all (by returning DataTooLate).
            || sectorIdAndDataOffsetDistanceWrapped > max_distance)
            return CohereResult::DataTooLate;
        return CohereResult::DataCoheres;
    }
    if (sectorIdAndDataOffsetDistance < min_distance)
        return CohereResult::DataTooEarly;
    if (sectorIdAndDataOffsetDistance > max_distance)
        return CohereResult::DataTooLate;
    return CohereResult::DataCoheres;
}
