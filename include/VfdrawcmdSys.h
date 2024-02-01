#pragma once

#include "PhysicalTrackMFM.h"
#include "config.h"

#include "Platform.h"
#include "fdrawcmd.h"

#include "FdrawcmdSys.h"
#include "Header.h"
#include "Util.h"

#include <bitset>
#include <memory>



class VfdrawcmdSys : public FdrawcmdSys
{
public:
    VfdrawcmdSys(const std::string &path);
    virtual ~VfdrawcmdSys() override = default;
    static std::unique_ptr<VfdrawcmdSys> Open(const std::string &path);
    util::Version& GetVersion() override;
    FD_FDC_INFO* GetFdcInfo() override;
    int GetMaxTransferSize() override;

public:
    bool GetVersion(util::Version& version) override;
    bool GetResult(FD_CMD_RESULT& result) override;
protected:
    bool AdvanceSectorIndexByFindingSectorIds(const OrphanDataCapableTrack& orphanDataCapableTrack, uint8_t count = 1, bool* looped = nullptr);
    void WaitIndex(int head = -1, const bool calcSpinTime = false);
    bool WaitSector(const OrphanDataCapableTrack &orphanDataCapableTrack);
    bool SetPerpendicularMode(int ow_ds_gap_wgate) override;
    void LimitCyl();
    const PhysicalTrackMFM& ReadRawTrack(const CylHead& cylhead);
    OrphanDataCapableTrack& ReadTrackFromRowTrack(const CylHead& cylhead);
public:
    bool SetEncRate(Encoding encoding, DataRate datarate) override;
    bool SetHeadSettleTime(int ms) override;
    bool SetMotorTimeout(int seconds) override;
    bool SetMotorOff() override;
    bool SetDiskCheck(bool enable) override;
    bool GetFdcInfo(FD_FDC_INFO& info) override;
    bool CmdPartId(uint8_t& part_id) override;
    bool Configure(uint8_t eis_efifo_poll_fifothr, uint8_t pretrk) override;
    bool Specify(int step_rate, int head_unload_time, int head_load_time) override;
    bool Recalibrate() override;
    bool Seek(int cyl, int head = -1) override;
    bool RelativeSeek(int head, int offset) override;
    bool CmdVerify(int cyl, int head, int start, int size, int eot) override;
    bool CmdVerify(int phead, int cyl, int head, int sector, int size, int eot) override;
    bool CmdReadTrack(int phead, int cyl, int head, int sector, int size, int eot, MEMORY& mem) override;
    bool CmdRead(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, size_t uOffset_ = 0, bool fDeleted_ = false) override;
    bool CmdWrite(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, bool fDeleted_ = false) override;
    bool CmdFormat(FD_FORMAT_PARAMS* params, int size) override;
    bool CmdFormatAndWrite(FD_FORMAT_PARAMS* params, int size) override;
    bool CmdScan(int head, FD_SCAN_RESULT* scan, int size) override;
    bool CmdTimedScan(int head, FD_TIMED_SCAN_RESULT* timed_scan, int size) override;
    bool CmdTimedMultiScan(int head, int track_retries, FD_TIMED_MULTI_SCAN_RESULT *timed_multi_scan, int size, int byte_tolerance_of_time = -1) override;
    bool CmdReadId(int head, FD_CMD_RESULT& result) override;
    bool FdRawReadTrack(int head, int size, MEMORY& mem) override;
    bool FdSetSectorOffset(int index) override;
    bool FdSetShortWrite(int length, int finetune) override;
    bool FdGetRemainCount(int& remain) override;
    bool FdCheckDisk() override;
    bool FdGetTrackTime(int& microseconds) override;
    bool FdGetMultiTrackTime(FD_MULTI_TRACK_TIME_RESULT& time_tolerance, uint8_t revolutions = 10) override;
    bool FdReset() override;

private:
    static const int DEFAULT_TRACKTIMES[4];
    std::string m_path = "";
    int m_cyl = 0;
    FD_CMD_RESULT m_result{ 0, 0, 0, 0, 0, 0, 0 };
    uint8_t m_fdrate = FD_RATE_250K;
    int m_trackTime = DEFAULT_TRACKTIMES[FD_RATE_250K];
    uint8_t m_waitSectorCount = 0; // The number of sector to wait before these operations: ReadId, ReadData, ReadDeletedData, WriteData, WriteDeletedData, Verify.
    bool m_waitSector = false;
    uint8_t m_currentSectorIndex = 0;

    std::bitset<MAX_DISK_CYLS * MAX_DISK_HEADS> m_rawTrackLoaded{};
    std::map<CylHead, PhysicalTrackMFM> m_rawTracks{};
    std::bitset<MAX_DISK_CYLS * MAX_DISK_HEADS> m_odcTrackDecoded{};
    std::map<CylHead, OrphanDataCapableTrack> m_odcTracks{};
};
