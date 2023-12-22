// fdrawcmd.sys device

#include "FdrawcmdSys.h"
#include "win32_error.h"

#ifdef HAVE_FDRAWCMD_H

#include "Cpp_helpers.h"
#include "utils.h"

#include <algorithm>

#define IGNORE_DEBUG 1

#if defined(_DEBUG) && (!defined(IGNORE_DEBUG) || IGNORE_DEBUG == 0)
// Macro overloading reference:
// http://stackoverflow.com/questions/3046889/optional-parameters-with-c-macros/28074198#28074198
// or shorter: https://stackoverflow.com/a/28074198

// These are the macros which are called with proper amount of parameters.
#define IOCTL_3(RESULT,IO_PARAMS,DEBUG_MSG) (RESULT = Ioctl(IO_PARAMS), \
(util::cout << (DEBUG_MSG) << ", success=" << (RESULT) << ", returned=" << (IO_PARAMS).returned << '\n'), (RESULT))
#define IOCTL_2(RESULT,IO_PARAMS) RESULT = Ioctl(IO_PARAMS) // No debug text in third parameter, ignoring debug.
// We do not need IOCTL_1 because at least the result and the ioctl_params structure must be passed.
// We do not need IOCTL_0 because at least the result and the ioctl_params structure must be passed.

#define RETURN_IOCTL_2(IO_PARAMS,DEBUG_MSG) const auto RESULT = Ioctl(IO_PARAMS); \
util::cout << (DEBUG_MSG) << ", success=" << RESULT << ", returned=" << (IO_PARAMS).returned << '\n'; \
return RESULT
#define RETURN_IOCTL_1(IO_PARAMS) return Ioctl(IO_PARAMS) // No debug text in second parameter, ignoring debug.
// We do not need RETURN_IOCTL_0 because at least the ioctl_params structure must be passed.

// Macro magic to use desired macro with optional 0, 1, 2, 3 parameters.
#define IOCTL_FUNC_CHOOSER(_f1, _f2, _f3, _f4, ...) _f4
#define IOCTL_FUNC_RECOMPOSER(argsWithParentheses) IOCTL_FUNC_CHOOSER argsWithParentheses
#define IOCTL_CHOOSE_FROM_ARG_COUNT(...) IOCTL_FUNC_RECOMPOSER((__VA_ARGS__, IOCTL_3, IOCTL_2, IOCTL_1, ))
#define IOCTL_NO_ARG_EXPANDER() ,,IOCTL_0
#define IOCTL_MACRO_CHOOSER(...) IOCTL_CHOOSE_FROM_ARG_COUNT(IOCTL_NO_ARG_EXPANDER __VA_ARGS__ ())

// Macro magic to use desired macro with optional 0, 1, 2 parameters.
#define RETURN_IOCTL_FUNC_CHOOSER(_f1, _f2, _f3, ...) _f3
#define RETURN_IOCTL_FUNC_RECOMPOSER(argsWithParentheses) RETURN_IOCTL_FUNC_CHOOSER argsWithParentheses
#define RETURN_IOCTL_CHOOSE_FROM_ARG_COUNT(...) RETURN_IOCTL_FUNC_RECOMPOSER((__VA_ARGS__, RETURN_IOCTL_2, RETURN_IOCTL_1, ))
#define RETURN_IOCTL_NO_ARG_EXPANDER() ,,RETURN_IOCTL_0
#define RETURN_IOCTL_MACRO_CHOOSER(...) RETURN_IOCTL_CHOOSE_FROM_ARG_COUNT(RETURN_IOCTL_NO_ARG_EXPANDER __VA_ARGS__ ())

// These are the macros that the user can call.
// IOCTL parameters: {writable} bool result, {writable} IOCTL_PARAMS ioctl_params, {optional, util::cout acceptable} debug_text
#define IOCTL(...) IOCTL_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
// RETURN_IOCTL parameters: {writable} IOCTL_PARAMS ioctl_params, {optional, util::cout acceptable} debug_text
#define RETURN_IOCTL(...) RETURN_IOCTL_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)
#else
#define IOCTL(RESULT, IO_PARAMS, ...) RESULT = Ioctl(IO_PARAMS) // Third parameter is debug text, ignoring it.
#define RETURN_IOCTL(IO_PARAMS, ...) return Ioctl(IO_PARAMS) // Second parameter is debug text, ignoring it.
#endif

/*static*/ std::unique_ptr<FdrawcmdSys> FdrawcmdSys::Open(int device_index)
{
    auto path = util::format(R"(\\.\fdraw)", device_index);

    Win32Handle hdev{ CreateFile(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr) };

    if (hdev.get() != INVALID_HANDLE_VALUE)
        return std::make_unique<FdrawcmdSys>(hdev.release());

    return std::unique_ptr<FdrawcmdSys>();
}

FdrawcmdSys::FdrawcmdSys(HANDLE hdev)
{
    m_hdev.reset(hdev);
}

bool FdrawcmdSys::Ioctl(DWORD code, void* inbuf, int insize, void* outbuf, int outsize, DWORD* returned)
{
    DWORD returned_local;
    const auto pReturned = returned == nullptr ? &returned_local : returned;
    *pReturned = 0;
    return !!DeviceIoControl(m_hdev.get(), code, inbuf, insize, outbuf, outsize, pReturned, nullptr);
}

constexpr uint8_t FdrawcmdSys::DtlFromSize(int size)
{
    // Data length used only for 128-byte sectors.
    return (size == 0) ? 0x80 : 0xff;
}

util::Version& FdrawcmdSys::GetVersion()
{
    if (m_driver_version.value == 0)
        if (!GetVersion(m_driver_version))
            throw win32_error(GetLastError_MP(), "GetVersion");
    return m_driver_version;
}

FD_FDC_INFO* FdrawcmdSys::GetFdcInfo()
{
    if (!m_fdc_info_queried)
    {
        if (!GetFdcInfo(m_fdc_info))
            return nullptr;
        m_fdc_info_queried = true;
    }
    return &m_fdc_info;
}

int FdrawcmdSys::GetMaxTransferSize()
{
    if (m_max_transfer_size == 0)
    {
        GetVersion(); // Required for MaxTransferSize.
        GetFdcInfo(); // Required for MaxTransferSize.
        const auto have_max_transfer_size = m_driver_version.value >= DriverVersion1_0_1_12 && m_fdc_info_queried;
                      // Version 12 returns MaxTransferSize. In older version let it be IoBufferSize (32768).
        m_max_transfer_size = have_max_transfer_size ? m_fdc_info.MaxTransferSize : 32768;
    }
    return m_max_transfer_size;
}

////////////////////////////////////////////////////////////////////////////////

bool FdrawcmdSys::GetVersion(util::Version& version)
{
    version.value = 0;
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDRAWCMD_GET_VERSION;
    ioctl_params.outbuf = &version.value;
    ioctl_params.outsize = sizeof(version.value);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::GetVersion"));
}

bool FdrawcmdSys::GetResult(FD_CMD_RESULT& result)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_GET_RESULT;
    ioctl_params.outbuf = &result;
    ioctl_params.outsize = sizeof(result);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::GetResult"));
}

bool FdrawcmdSys::SetPerpendicularMode(int ow_ds_gap_wgate)
{
    FD_PERPENDICULAR_PARAMS pp{};
    pp.ow_ds_gap_wgate = lossless_static_cast<uint8_t>(ow_ds_gap_wgate);
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_PERPENDICULAR_MODE;
    ioctl_params.inbuf = &pp;
    ioctl_params.insize = sizeof(pp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetPerpendicularMode: ow_ds_gap_wgate=", ow_ds_gap_wgate));
}

bool FdrawcmdSys::SetEncRate(Encoding encoding, DataRate datarate)
{
    if (encoding != Encoding::MFM && encoding != Encoding::FM)
        throw util::exception("unsupported encoding (", encoding, ") for fdrawcmd.sys");

    // Set perpendicular mode and write-enable for 1M data rate
    SetPerpendicularMode((datarate == DataRate::_1M) ? 0xbc : 0x00);

    uint8_t rate;
    switch (datarate)
    {
    case DataRate::_250K:   rate = FD_RATE_250K; break;
    case DataRate::_300K:   rate = FD_RATE_300K; break;
    case DataRate::_500K:   rate = FD_RATE_500K; break;
    case DataRate::_1M:     rate = FD_RATE_1M; break;
    default:
        throw util::exception("unsupported datarate (", datarate, ")");
    }

    m_encoding_flags = encoding == Encoding::MFM ? FD_OPTION_MFM : FD_OPTION_FM;

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_DATA_RATE;
    ioctl_params.inbuf = &rate;
    ioctl_params.insize = sizeof(rate);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetEncRate: encoding=", encoding, ", datarate=", datarate));
}

bool FdrawcmdSys::SetHeadSettleTime(int ms)
{
    auto hst = limited_static_cast<uint8_t>(ms);
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_HEAD_SETTLE_TIME;
    ioctl_params.inbuf = &hst;
    ioctl_params.insize = sizeof(hst);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetHeadSettleTime: ms=", ms));
}

bool FdrawcmdSys::SetMotorTimeout(int seconds)
{
    auto timeout = limited_static_cast<uint8_t>(std::min(3, seconds));
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_MOTOR_TIMEOUT;
    ioctl_params.inbuf = &timeout;
    ioctl_params.insize = sizeof(timeout);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetMotorTimeout: seconds=", seconds));
}

bool FdrawcmdSys::SetMotorOff()
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_MOTOR_OFF;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetMotorOff"));
}

bool FdrawcmdSys::SetDiskCheck(bool enable)
{
    uint8_t check{ static_cast<uint8_t>(enable ? 1 : 0) };
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_DISK_CHECK;
    ioctl_params.inbuf = &check;
    ioctl_params.insize = sizeof(check);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::SetDiskCheck: enable=", enable));
}

bool FdrawcmdSys::GetFdcInfo(FD_FDC_INFO& info)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_GET_FDC_INFO;
    ioctl_params.outbuf = &info;
    ioctl_params.outsize = sizeof(info);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::GetFdcInfo"));
}

bool FdrawcmdSys::CmdPartId(uint8_t& part_id)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_PART_ID;
    ioctl_params.outbuf = &part_id;
    ioctl_params.outsize = sizeof(part_id);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdPartId"));
}

bool FdrawcmdSys::Configure(uint8_t eis_efifo_poll_fifothr, uint8_t pretrk)
{
    FD_CONFIGURE_PARAMS cp{};
    cp.eis_efifo_poll_fifothr = eis_efifo_poll_fifothr;
    cp.pretrk = pretrk;

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_CONFIGURE;
    ioctl_params.inbuf = &cp;
    ioctl_params.insize = sizeof(cp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::Configure: eis_efifo_poll_fifothr=", eis_efifo_poll_fifothr,
        ", pretrk=", pretrk));
}

bool FdrawcmdSys::Specify(int step_rate, int head_unload_time, int head_load_time)
{
    const auto srt = static_cast<uint8_t>(step_rate & 0x0f);
    const auto hut = static_cast<uint8_t>(head_unload_time & 0x0f);
    const auto hlt = static_cast<uint8_t>(head_load_time & 0x7f);

    FD_SPECIFY_PARAMS sp{};
    sp.srt_hut = static_cast<uint8_t>(srt << 4) | hut;
    sp.hlt_nd = static_cast<uint8_t>(hlt << 1) | 0;

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_SPECIFY;
    ioctl_params.inbuf = &sp;
    ioctl_params.insize = sizeof(sp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::Specify: step_rate=", step_rate,
        ", head_unload_time=", head_unload_time, ", head_load_time=", head_load_time));
}

bool FdrawcmdSys::Recalibrate()
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_RECALIBRATE;
    // ToDo: should we check TRACK0 and retry if not signalled?
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::Recalibrate"));
}

bool FdrawcmdSys::Seek(int cyl, int head /*= -1*/)
{
    if (cyl == 0)
    {
        util::cout << util::format("FdrawcmdSys::Seek(alias recalibrate): cyl=", cyl, ", head=", head) << '\n';
        return Recalibrate();
    }

    FD_SEEK_PARAMS sp{};
    sp.cyl = static_cast<uint8_t>(cyl);
    int sp_size = sizeof(sp);
    if (head >= 0)
    {
        if (head < 0 || head > 1)
            throw util::exception("unsupported head (", head, ")");
        sp.head = static_cast<uint8_t>(head);
    }
    else
        sp_size -= sizeof(sp.head);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_SEEK;
    ioctl_params.inbuf = &sp;
    ioctl_params.insize = sizeof(sp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::Seek: cyl=", cyl, ", head=", head));
}

bool FdrawcmdSys::RelativeSeek(int head, int offset)
{
    FD_RELATIVE_SEEK_PARAMS rsp{};
    rsp.flags = (offset > 0) ? FD_OPTION_DIR : 0;
    rsp.head = static_cast<uint8_t>(head);
    rsp.offset = static_cast<uint8_t>(std::abs(offset));

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_RELATIVE_SEEK;
    ioctl_params.inbuf = &rsp;
    ioctl_params.insize = sizeof(rsp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::RelativeSeek: (flags=", rsp.flags,
        "), head=", head, ", offset=", offset));
}

bool FdrawcmdSys::CmdVerify(int cyl, int head, int sector, int size, int eot)
{
    return CmdVerify(head, cyl, head, sector, size, eot);
}

bool FdrawcmdSys::CmdVerify(int phead, int cyl, int head, int sector, int size, int eot)
{
    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = m_encoding_flags;
    rwp.phead = static_cast<uint8_t>(phead);
    rwp.cyl = static_cast<uint8_t>(cyl);
    rwp.head = static_cast<uint8_t>(head);
    rwp.sector = static_cast<uint8_t>(sector);
    rwp.size = static_cast<uint8_t>(size);
    rwp.eot = static_cast<uint8_t>(eot);
    rwp.gap = RW_GAP;
    rwp.datalen = DtlFromSize(size);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_VERIFY;
    ioctl_params.inbuf = &rwp;
    ioctl_params.insize = sizeof(rwp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdVerify: (flags=", rwp.flags,
        "), phead=", phead, "cyl=", cyl, ", head=", head, ", sector=", sector,
        ", size=", size, ", eot=", eot, ", (gap=", rwp.gap, ")"));
}

/*
 * Input:
 * - eot: number of sectors
 * - mem: the memory where the read sectors are stored. Its size is auto calculated if 0.
 */
bool FdrawcmdSys::CmdReadTrack(int phead, int cyl, int head, int sector, int size, int eot, MEMORY& mem)
{
    /* Def.: SectorSize is the size of sector in bytes calculated by SizeCodeToRealLength(rwp.size).
     * 1) The sector is ignored by FDC and is 1 by default.
     * 2) Must be rwp.eot >= output_size / SectorSize else FDC
     *    returns "Sector not found" error.
     * 3) Should be output_size <= (MaxTransferSize / SectorSize + 1) * SectorSize else it is waste of memory.
     * 4) Must be output_size <= mem.size else FDC returns
     *    "Invalid memory access" error.
     * 5) The bytes read value returned by fdrawcmd is usually 0 except when
     *    rwp.eot > output_size / SectorSize and output_size % SectorSize = 0.
     * 6) Consequences:
     *    The rwp.eot specifies the amount of sectors to read.
     *    The best if rwp.eot = output_size / SectorSize + 1.
     *    Also best if output_size = eot * SectorSize.
     *    Thus best if rwp.eot = eot + 1.
     *    All best if output_size = min(mem.size, eot * SectorSize, (MaxTransferSize / sector_size + 1) * sector_size).
     */

    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = m_encoding_flags;
    rwp.phead = static_cast<uint8_t>(phead);
    rwp.cyl = static_cast<uint8_t>(cyl);
    rwp.head = static_cast<uint8_t>(head);
    rwp.sector = static_cast<uint8_t>(sector);
    rwp.size = static_cast<uint8_t>(size);
    rwp.eot = limited_static_cast<uint8_t>(eot + 1); // +1 for 5) above.
    rwp.gap = RW_GAP;
    rwp.datalen = DtlFromSize(size);

    const auto sector_size = Sector::SizeCodeToRealLength(rwp.size);
    auto output_size = std::min(eot * sector_size, (GetMaxTransferSize() / sector_size + 1) * sector_size);
    if (mem.size > 0)
    {
        if (mem.size < output_size)
        {
            mem.resize((mem.size / sector_size) * sector_size); // Flooring to sector_size boundary for 5) above.
            output_size = mem.size;
        }
        else
            mem.fill();
    }
    else
        mem.resize(output_size);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_READ_TRACK;
    ioctl_params.inbuf = &rwp;
    ioctl_params.insize = sizeof(rwp);
    ioctl_params.outbuf = mem.pb;
    ioctl_params.outsize = output_size;
    bool result;
    IOCTL(result, ioctl_params, util::format("FdrawcmdSys::CmdReadTrack: (flags=", rwp.flags,
        "), phead=", phead, ", cyl=", cyl, ", head=", head, ", sector=", sector,
        ", size=", size, ", eot=", eot, ", (gap=", rwp.gap, "), bufferlen=", mem.size,
        ", output_size = ", output_size));
    if (result && output_size != ioctl_params.returned)
        util::cout << "Warning: CmdReadTrack reports reading " << ioctl_params.returned << " bytes instead of " << output_size << '\n';
    return result;
}

bool FdrawcmdSys::CmdRead(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, size_t data_offset, bool deleted)
{
    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = m_encoding_flags;
    rwp.phead = static_cast<uint8_t>(phead);
    rwp.cyl = static_cast<uint8_t>(cyl);
    rwp.head = static_cast<uint8_t>(head);
    rwp.sector = static_cast<uint8_t>(sector);
    rwp.size = static_cast<uint8_t>(size);
    rwp.eot = static_cast<uint8_t>(sector + count);
    rwp.gap = RW_GAP;
    rwp.datalen = DtlFromSize(size);

    const auto total_size = count * Sector::SizeCodeToRealLength(rwp.size);
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = deleted ? IOCTL_FDCMD_READ_DELETED_DATA : IOCTL_FDCMD_READ_DATA;
    ioctl_params.inbuf = &rwp;
    ioctl_params.insize = sizeof(rwp);
    ioctl_params.outbuf = mem.pb + data_offset;
    ioctl_params.outsize = total_size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdRead: (flags=", rwp.flags,
        "), phead=", phead, ", cyl=", cyl, ", head=", head, ", sector=", sector,
        ", size=", size, ", count=", count, ", deleted=", deleted, ", (gap=", rwp.gap,
        "), data_offset=", data_offset, ", bufferlen=", mem.size, ", outputlen = ", total_size));
}

bool FdrawcmdSys::CmdWrite(int phead, int cyl, int head, int sector, int size, int count, MEMORY& mem, bool deleted)
{
    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = m_encoding_flags;
    rwp.phead = static_cast<uint8_t>(phead);
    rwp.cyl = static_cast<uint8_t>(cyl);
    rwp.head = static_cast<uint8_t>(head);
    rwp.sector = static_cast<uint8_t>(sector);
    rwp.size = static_cast<uint8_t>(size);
    rwp.eot = static_cast<uint8_t>(sector + count);
    rwp.gap = RW_GAP;
    rwp.datalen = DtlFromSize(size);

    const auto total_size = count * Sector::SizeCodeToRealLength(rwp.size);
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = deleted ? IOCTL_FDCMD_WRITE_DELETED_DATA : IOCTL_FDCMD_WRITE_DATA;
    ioctl_params.inbuf = &rwp;
    ioctl_params.insize = sizeof(rwp);
    ioctl_params.outbuf = mem.pb;
    ioctl_params.outsize = total_size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdWrite: (flags=", rwp.flags,
        "), phead=", phead, ", cyl=", cyl, ", head=", head, ", sector=", sector,
        ", size=", size, ", count=", count, ", deleted=", deleted, ", (gap=", rwp.gap,
        "), bufferlen=", mem.size, ", outputlen = ", total_size));
}

bool FdrawcmdSys::CmdFormat(FD_FORMAT_PARAMS* params, int size)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_FORMAT_TRACK;
    ioctl_params.inbuf = params;
    ioctl_params.insize = size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdFormat: p.flags=", params->flags,
        ", p.phead=", params->phead, ", p.size=", params->size, ", p.sectors=", params->sectors,
        ", p.gap=", params->gap, ", p.fill=", params->fill, ", size=", size));
}

bool FdrawcmdSys::CmdFormatAndWrite(FD_FORMAT_PARAMS* params, int size)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_FORMAT_AND_WRITE;
    ioctl_params.inbuf = params;
    ioctl_params.insize = size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdFormatAndWrite: p.flags=", params->flags,
        ", p.phead=", params->phead, ", p.size=", params->size, ", p.sectors=", params->sectors,
        ", p.gap=", params->gap, ", p.fill=", params->fill, ", size=", size));
}

bool FdrawcmdSys::CmdScan(int head, FD_SCAN_RESULT* scan, int size)
{
    FD_SCAN_PARAMS sp{};
    sp.flags = m_encoding_flags;
    sp.head = static_cast<uint8_t>(head);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SCAN_TRACK;
    ioctl_params.inbuf = &sp;
    ioctl_params.insize = sizeof(sp);
    ioctl_params.outbuf = scan;
    ioctl_params.outsize = size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdScan: (flags=", sp.flags,
        "), head=", head, ", size=", size));
}

bool FdrawcmdSys::CmdTimedScan(int head, FD_TIMED_SCAN_RESULT* timed_scan, int size)
{
    FD_SCAN_PARAMS sp{};
    sp.flags = m_encoding_flags;
    sp.head = static_cast<uint8_t>(head);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_TIMED_SCAN_TRACK;
    ioctl_params.inbuf = &sp;
    ioctl_params.insize = sizeof(sp);
    ioctl_params.outbuf = timed_scan;
    ioctl_params.outsize = size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdTimedScan: (flags=", sp.flags,
        "), head=", head, ", size=", size));
}

bool FdrawcmdSys::CmdTimedMultiScan(int head, int track_retries,
                                    FD_TIMED_MULTI_SCAN_RESULT* timed_multi_scan, int size,
                                    int byte_tolerance_of_time /* = -1 */)
{
    if (head < 0 || head > 1)
        throw util::exception("unsupported head (", head, ")");
    if (track_retries == 0)
        throw util::exception("unsupported track_retries (", track_retries, ")");
    FD_MULTI_SCAN_PARAMS msp{};
    msp.flags = m_encoding_flags;
    msp.head = lossless_static_cast<uint8_t>(head);
    msp.track_retries = lossless_static_cast<int8_t>(track_retries);
    msp.byte_tolerance_of_time = lossless_static_cast<int8_t>(byte_tolerance_of_time);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_TIMED_MULTI_SCAN_TRACK;
    ioctl_params.inbuf = &msp;
    ioctl_params.insize = sizeof(msp);
    ioctl_params.outbuf = timed_multi_scan;
    ioctl_params.outsize = size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdTimedMultiScan: (flags=", msp.flags,
        "), head=", head, ", track_retries=", track_retries,
        ", size=", size, ", byte_tolerance_of_time=", byte_tolerance_of_time));
}

bool FdrawcmdSys::CmdReadId(int head, FD_CMD_RESULT& result)
{
    FD_READ_ID_PARAMS rip{};
    rip.flags = m_encoding_flags;
    rip.head = static_cast<uint8_t>(head);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FDCMD_READ_ID;
    ioctl_params.inbuf = &rip;
    ioctl_params.insize = sizeof(rip);
    ioctl_params.outbuf = &result;
    ioctl_params.outsize = sizeof(result);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::CmdReadId: (flags=", rip.flags, "), head=", head));
}

bool FdrawcmdSys::FdRawReadTrack(int head, int size, MEMORY& mem)
{
    FD_RAW_READ_PARAMS rrp{};
    rrp.flags = FD_OPTION_MFM;
    rrp.head = static_cast<uint8_t>(head);
    rrp.size = static_cast<uint8_t>(size);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_RAW_READ_TRACK;
    ioctl_params.inbuf = &rrp;
    ioctl_params.insize = sizeof(rrp);
    ioctl_params.outbuf = mem.pb;
    ioctl_params.outsize = mem.size;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdRawReadTrack: (flags=", rrp.flags,
        "), head=", head, ", size=", size, ", bufferlen=", mem.size));
}

bool FdrawcmdSys::FdSetSectorOffset(int index)
{
    FD_SECTOR_OFFSET_PARAMS sop{};
    sop.sectors = limited_static_cast<uint8_t>(index);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_SECTOR_OFFSET;
    ioctl_params.inbuf = &sop;
    ioctl_params.insize = sizeof(sop);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdSetSectorOffset: index=", index));
}

bool FdrawcmdSys::FdSetShortWrite(int length, int finetune)
{
    FD_SHORT_WRITE_PARAMS swp{};
    swp.length = static_cast<DWORD>(length);
    swp.finetune = static_cast<DWORD>(finetune);

    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_SET_SHORT_WRITE;
    ioctl_params.inbuf = &swp;
    ioctl_params.insize = sizeof(swp);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdSetShortWrite: length=", length, ", finetune=", finetune));
}

bool FdrawcmdSys::FdGetRemainCount(int& remain)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_GET_REMAIN_COUNT;
    ioctl_params.outbuf = &remain;
    ioctl_params.outsize = sizeof(remain);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdGetRemainCount"));
}

bool FdrawcmdSys::FdCheckDisk()
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_CHECK_DISK;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdCheckDisk"));
}

bool FdrawcmdSys::FdGetTrackTime(int& microseconds)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_GET_TRACK_TIME;
    ioctl_params.outbuf = &microseconds;
    ioctl_params.outsize = sizeof(microseconds);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdGetTrackTime"));
}

bool FdrawcmdSys::FdGetMultiTrackTime(FD_MULTI_TRACK_TIME_RESULT& track_time, uint8_t revolutions /* = 10*/)
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_GET_MULTI_TRACK_TIME;
    ioctl_params.inbuf = &revolutions;
    ioctl_params.insize = sizeof(revolutions);
    ioctl_params.outbuf = &track_time;
    ioctl_params.outsize = sizeof(track_time);
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdGetMultiTrackTime: revolutions=", revolutions));
}

bool FdrawcmdSys::FdReset()
{
    IOCTL_PARAMS ioctl_params{};
    ioctl_params.code = IOCTL_FD_RESET;
    RETURN_IOCTL(ioctl_params, util::format("FdrawcmdSys::FdReset"));
}

#endif // HAVE_FDRAWCMD_H
