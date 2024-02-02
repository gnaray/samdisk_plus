// fdrawcmd.sys real device wrapper:
//  http://simonowen.com/fdrawcmd/

#ifndef FDRAWSYS_DEV_H
#define FDRAWSYS_DEV_H
//---------------------------------------------------------------------------

#include "config.h"

#ifdef HAVE_FDRAWCMD_H

#include "DemandDisk.h"
#include "FdrawcmdSys.h"
#include "MultiScanResult.h"
#include "TimedAndPhysicalDualTrack.h"

#include <cstring>
#include <memory>

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
    FdrawSysDevDisk(const std::string& path, std::unique_ptr<FdrawcmdSys> fdrawcmd);

protected:
    bool supports_retries() const override;
    bool supports_rescans() const override;
    TrackData load(const CylHead& cylhead, bool first_read,
            int with_head_seek_to, const DeviceReadingPolicy& deviceReadingPolicy = DeviceReadingPolicy{}) override;
    bool preload(const Range& range, int cyl_step) override;
    bool is_constant_disk() const override;

private:
    void SetMetadata(const std::string& path);
    bool DetectEncodingAndDataRate(int head);
    Track BlindReadHeaders(const CylHead& cylhead, int& firstSectorSeen);
    void ReadSector(const CylHead& cylhead, Track& track, int index, int firstSectorSeen = 0);
    void ReadFirstGap(const CylHead& cylhead, Track& track);

    /* Here are the methods of multi track reading based on CmdTimedMultiScan
     * implemented in driver version >= 1.0.1.12 and of physical track reading.
     */
    bool ScanAndDetectIfNecessary(const CylHead& cylhead, MultiScanResult& multiScanResult);
    TimedAndPhysicalDualTrack BlindReadHeaders112(const CylHead& cylhead, const DeviceReadingPolicy& deviceReadingPolicy);
    void DiscardOufOfSpaceSectorsAtTrackEnd(Track& track) const;
    void GuessAndAddSectorIdsOfOrphans(Track& track, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack) const;
    static bool GetSectorDataFromPhysicalTrack(TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack, const int index);
    bool ReadSectors(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack, const DeviceReadingPolicy& deviceReadingPolicy);
    bool ReadAndMergePhysicalTracks(const CylHead& cylhead, TimedAndPhysicalDualTrack& timedAndPhysicalDualTrack);

    std::unique_ptr<FdrawcmdSys> m_fdrawcmd;
    Encoding m_lastEncoding{ Encoding::Unknown };
    DataRate m_lastDataRate{ DataRate::Unknown };
    TrackInfo m_trackInfo[MAX_DISK_CYLS * MAX_DISK_HEADS];
    bool m_warnedMFM128 = false;
};


bool ReadFdrawcmdSys(const std::string& path, std::shared_ptr<Disk>& disk);
bool WriteFdrawcmdSys(const std::string& path, std::shared_ptr<Disk>& disk);

#endif // HAVE_FDRAWCMD_H
//---------------------------------------------------------------------------
#endif // FDRAWSYS_DEV_H
