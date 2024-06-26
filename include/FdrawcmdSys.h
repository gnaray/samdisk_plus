#pragma once

#include "config.h"

#ifdef HAVE_FDRAWCMD_H
#include "fdrawcmd.h"

#include "Header.h"
#include "Util.h"

#include <memory>
#include <string>

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



constexpr uint32_t DriverVersion1_0_1_12 = 0x0100010c;
constexpr DataRate FDRATE_TO_DATARATE[4]{ DataRate::_500K, DataRate::_300K, DataRate::_250K, DataRate::_1M }; // FDRates to DataRates.
uint8_t datarateToFdRate(const DataRate& datarate);
Encoding fdEncodingToEncoding(const uint8_t fdEncoding);
uint8_t encodingToFdEncoding(const Encoding& encoding);



class FdrawcmdSys
{
public:
    FdrawcmdSys(HANDLE hdev);
    virtual ~FdrawcmdSys() = default;
    static std::unique_ptr<FdrawcmdSys> Open(int device);
    virtual util::Version& GetVersion();
    virtual FD_FDC_INFO* GetFdcInfo();
    virtual int GetMaxTransferSize();

public:
    virtual bool GetVersion(util::Version& version);
    virtual bool GetResult(FD_CMD_RESULT& result);
protected:
    virtual bool SetPerpendicularMode(int ow_ds_gap_wgate);
public:
    virtual bool SetEncRate(Encoding encoding, DataRate datarate);
    virtual bool SetHeadSettleTime(int ms);
    virtual bool SetMotorTimeout(int seconds);
    virtual bool SetMotorOff();
    virtual bool SetDiskCheck(bool enable);
    virtual bool GetFdcInfo(FD_FDC_INFO& info);
    virtual bool CmdPartId(uint8_t& part_id);
    virtual bool Configure(uint8_t eis_efifo_poll_fifothr, uint8_t pretrk);
    virtual bool Specify(int step_rate, int head_unload_time, int head_load_time);
    virtual bool Recalibrate();
    virtual bool Seek(int cyl, int head = -1);
    virtual bool RelativeSeek(int head, int offset);
    virtual bool CmdVerify(int cyl, int head, int start, int size, int eot);
    virtual bool CmdVerify(int phead, int cyl, int head, int sector, int size, int eot);
    virtual bool CmdReadTrack(int phead, int cyl, int head, int sector, int size, int eot, MEMORY& mem);
    virtual bool CmdRead(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, size_t uOffset_ = 0, bool fDeleted_ = false);
    virtual bool CmdWrite(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, bool fDeleted_ = false);
    virtual bool CmdFormat(FD_FORMAT_PARAMS* params, int size);
    virtual bool CmdFormatAndWrite(FD_FORMAT_PARAMS* params, int size);
    virtual bool CmdScan(int head, FD_SCAN_RESULT* scan, int size);
    virtual bool CmdTimedScan(int head, FD_TIMED_SCAN_RESULT* timed_scan, int size);
    virtual bool CmdTimedMultiScan(int head, int track_retries, FD_TIMED_MULTI_SCAN_RESULT *timed_multi_scan, int size, int byte_tolerance_of_time = -1);
    virtual bool CmdReadId(int head, FD_CMD_RESULT& result);
    virtual bool FdRawReadTrack(int head, int size, MEMORY& mem);
    virtual bool FdSetSectorOffset(int index);
    virtual bool FdSetShortWrite(int length, int finetune);
    virtual bool FdGetRemainCount(int& remain);
    virtual bool FdCheckDisk();
    virtual bool FdGetTrackTime(int& microseconds);
    virtual bool FdGetMultiTrackTime(FD_MULTI_TRACK_TIME_RESULT& time_tolerance, uint8_t revolutions = 10);
    virtual bool FdReset();

    static const std::string RawTrackFileNamePattern;

private:
    static constexpr int RW_GAP = 0x0a;
    static constexpr uint8_t DtlFromSize(int size);

    bool Ioctl(DWORD code, void* inbuf = nullptr, int insize = 0, void* outbuf = nullptr, int outsize = 0, DWORD* returned = nullptr);
    inline bool Ioctl(IOCTL_PARAMS& ioctl_params)
    {
        return Ioctl(ioctl_params.code, ioctl_params.inbuf, ioctl_params.insize,
            ioctl_params.outbuf, ioctl_params.outsize, &ioctl_params.returned);
    }

protected:
    Win32Handle m_hdev{};
    util::Version m_driver_version{};
    FD_FDC_INFO m_fdc_info{};
    bool m_fdc_info_queried = false;
    int m_max_transfer_size = 0;
    uint8_t m_encoding_flags{ FD_OPTION_MFM };  // FD_OPTION_FM or FD_OPTION_MFM only.
};

#endif // HAVE_FDRAWCMD_H
