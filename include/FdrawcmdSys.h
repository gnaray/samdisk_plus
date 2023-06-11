#pragma once

#include "config.h"

#ifdef HAVE_FDRAWCMD_H
#include "Platform.h"
#include "fdrawcmd.h"

#include "Header.h"
#include "Util.h"

#include <memory>

struct handle_closer
{
    using pointer = HANDLE;
    void operator() (HANDLE h) { if (h != INVALID_HANDLE_VALUE) ::CloseHandle(h); }
};

using Win32Handle = std::unique_ptr<std::remove_pointer<HANDLE>::type, handle_closer>;


typedef struct _IOCTL_PARAMS
{
    DWORD code;
    void* inbuf = nullptr;
    int insize = 0;
    void* outbuf = nullptr;
    int outsize = 0;
    DWORD returned = 0;
} IOCTL_PARAMS;

class FdrawcmdSys
{
public:
    FdrawcmdSys(HANDLE hdev);
    static std::unique_ptr<FdrawcmdSys> Open(int device);
    util::Version& GetVersion();
    FD_FDC_INFO* GetFdcInfo();
    int GetMaxTransferSize();

public:
    bool GetVersion(util::Version& version);
    bool GetResult(FD_CMD_RESULT& result);
protected:
    bool SetPerpendicularMode(int ow_ds_gap_wgate);
public:
    bool SetEncRate(Encoding encoding, DataRate datarate);
    bool SetHeadSettleTime(int ms);
    bool SetMotorTimeout(int seconds);
    bool SetMotorOff();
    bool SetDiskCheck(bool enable);
    bool GetFdcInfo(FD_FDC_INFO& info);
    bool CmdPartId(uint8_t& part_id);
    bool Configure(uint8_t eis_efifo_poll_fifothr, uint8_t pretrk);
    bool Specify(int step_rate, int head_unload_time, int head_load_time);
    bool Recalibrate();
    bool Seek(int cyl, int head = -1);
    bool RelativeSeek(int head, int offset);
    bool CmdVerify(int cyl, int head, int start, int size, int eot);
    bool CmdVerify(int phead, int cyl, int head, int sector, int size, int eot);
    bool CmdReadTrack(int phead, int cyl, int head, int sector, int size, int eot, MEMORY& mem);
    bool CmdRead(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, size_t uOffset_ = 0, bool fDeleted_ = false);
    bool CmdWrite(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, bool fDeleted_ = false);
    bool CmdFormat(FD_FORMAT_PARAMS* params, int size);
    bool CmdFormatAndWrite(FD_FORMAT_PARAMS* params, int size);
    bool CmdScan(int head, FD_SCAN_RESULT* scan, int size);
    bool CmdTimedScan(int head, FD_TIMED_SCAN_RESULT* timed_scan, int size);
    bool CmdTimedMultiScan(int head, int track_retries, FD_TIMED_MULTI_SCAN_RESULT *timed_multi_scan, int size, int byte_tolerance_of_time = -1);
    bool CmdReadId(int head, FD_CMD_RESULT& result);
    bool FdRawReadTrack(int head, int size, MEMORY& mem);
    bool FdSetSectorOffset(int index);
    bool FdSetShortWrite(int length, int finetune);
    bool FdGetRemainCount(int& remain);
    bool FdCheckDisk();
    bool FdGetTrackTime(int& microseconds);
    bool FdGetMultiTrackTime(FD_MULTI_TRACK_TIME_RESULT& time_tolerance, uint8_t revolutions = 10);
    bool FdReset();

private:
    static constexpr int RW_GAP = 0x0a;
    static constexpr uint8_t DtlFromSize(int size);

    bool Ioctl(DWORD code, void* inbuf = nullptr, int insize = 0, void* outbuf = nullptr, int outsize = 0, DWORD* returned = nullptr);
    inline bool Ioctl(IOCTL_PARAMS& ioctl_params)
    {
        return Ioctl(ioctl_params.code, ioctl_params.inbuf, ioctl_params.insize,
            ioctl_params.outbuf, ioctl_params.outsize, &ioctl_params.returned);
    }

    uint8_t m_encoding_flags{ FD_OPTION_MFM };  // FD_OPTION_FM or FD_OPTION_MFM only.
    Win32Handle m_hdev{};
    util::Version m_driver_version{};
    FD_FDC_INFO m_fdc_info{};
    bool m_fdc_info_queried = false;
    int m_max_transfer_size = 0;
};

#endif // HAVE_FDRAWCMD_H
