// Calculations related to IBM PC format MFM/FM disks (System/34 compatible)

#include "PlatformConfig.h"
#include "IBMPCBase.h"
#include "Options.h"

#include <algorithm>

static auto& opt_debug = getOpt<int>("debug");
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
    return (encoding == Encoding::MFM) ? DATA_OVERHEAD_MFM : DATA_OVERHEAD_FM;
}

int GetSyncOverhead(Encoding encoding)
{
    return (encoding == Encoding::MFM) ? SYNC_OVERHEAD_MFM : SYNC_OVERHEAD_FM;
}

// The drive_speed is rpm_time which is the time duration while the disk rotates once.
// The drive_speed of 360 RPM is 166666,6... floored to int as 166666.
int GetRawTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding)
{
    auto len_bits = bits_per_second(datarate);
    assert(len_bits != 0);

    // Max len_bits is 1000000, max drive_speed is 300000 (at RPM 200).
    // Thus operator order is important to avoid overflow (of int assumed to be 32 bits).
    // Dividing by 1000 two times instead of once to avoid overflow for sure.
    auto len_bytes = len_bits / 1000 * drive_speed / 1000 / 8;
    if (encoding == Encoding::FM) len_bytes >>= 1;
    return len_bytes;
}

int GetTrackCapacity(int drive_speed, DataRate datarate, Encoding encoding)
{
    return GetRawTrackCapacity(drive_speed, datarate, encoding) * 1995 / 2000;
}
