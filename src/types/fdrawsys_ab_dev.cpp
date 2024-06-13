// fdrawcmd.sys raw reading using drives A+B:
//  http://simonowen.com/fdrawcmd/
//
// The 2-drive technique was developed by Vincent Joguin for his Disk2FDI utility.
// It requires BIOS support for 2 floppy drives, and both connected via one cable.
//
// The technique works as follows:
//  - the double-density disk to read is in A:
//  - a formatted high-density disk is in B:
//  - both drive motors are started
//  - a 16-32K read request is made on drive B:
//  - once data starts flowing, drive select is switched to drive A:
//  - the data returned contains raw bits from drive A:
//
// This allows reading of double-density formats that the PC FDC doesn't normally
// support, such as AmigaDOS. It does require the general format of the disk is
// known in order to know when the track is complete. Multiple attempts are often
// required to see all the expected sectors.

#include "config.h"

#ifdef HAVE_FDRAWCMD_H
#include "Platform.h"
#include "fdrawcmd.h"

#include "Options.h"
//#include "BitstreamTrackBuilder.h"
#include "DemandDisk.h"
#include "FdrawcmdSys.h"

#include "Disk.h"
#include "Util.h"
#include "win32_error.h"

#include <memory>

#define RAW_READ_ATTEMPTS           10      // Default reads per track
#define RAW_READ_SIZE_CODE          7       // 16K

static auto& opt_newdrive = getOpt<int>("newdrive");
static auto& opt_retries = getOpt<RetryPolicy>("retries");
static auto& opt_sectors = getOpt<long>("sectors");
static auto& opt_steprate = getOpt<int>("steprate");

class FdrawSysDevABDisk final : public DemandDisk
{
public:
    FdrawSysDevABDisk(std::unique_ptr<FdrawcmdSys> fdrawcmd)
        : m_fdrawcmd(std::move(fdrawcmd))
    {
        auto srt = (opt_steprate >= 0) ? opt_steprate : (opt_newdrive ? 0xd : 0x8);
        auto hut = 0x0f;
        auto hlt = opt_newdrive ? 0x0f : 0x7f;
        m_fdrawcmd->Specify(srt, hut, hlt);

        m_fdrawcmd->SetMotorTimeout(0);
        m_fdrawcmd->Recalibrate();

        if (!opt_newdrive)
            m_fdrawcmd->SetDiskCheck(false);
    }

protected:
    TrackData load(const CylHead& cylhead, bool /*first_read*/,
        int /*with_head_seek_to*/, const DeviceReadingPolicy& /*deviceReadingPolicy*//* = DeviceReadingPolicy{}*/) override
    {
        m_fdrawcmd->Seek(cylhead.cyl);
        m_fdrawcmd->SetEncRate(Encoding::MFM, DataRate::_500K);
        Track track;

        // TODO opt_retries can not be negative now. Earlier when it was negative
        // the loop went on until target was not modified the specified times. This could be another retry policy.
        auto scanRetries = opt_retries < 0 ? RAW_READ_ATTEMPTS : opt_retries;
        if (!scanRetries.GetSinceLastChange())
        {
            scanRetries.InvertSinceLastChange();
            MessageCPP(msgWarning, "Retries option is inverted to negative value for compatibility reason");
        }

        do
        {
            MEMORY mem(Sector::SizeCodeToLength(RAW_READ_SIZE_CODE));
            if (!m_fdrawcmd->FdRawReadTrack(cylhead.head, RAW_READ_SIZE_CODE, mem))
                throw win32_error(GetLastError_MP(), "FdRawReadTrack");

            util::bit_reverse(mem.pb, mem.size);
            BitBuffer bitbuf(DataRate::_250K, mem.pb, mem.size * 8);
            auto newtrack = TrackData(cylhead, std::move(bitbuf)).track();

            bool modified = false;
            for (auto& sector : newtrack)
            {
                // Add new sectors that don't already exist.
                if (sector.has_good_data() && track.find(sector.header) == track.end())
                {
                    track.add(std::move(sector));
                    modified = true;
                }
            }

            // Once we've seen an Amiga sector, keep trying until we have them all.
            if (!track.empty() && track[0].encoding == Encoding::Amiga)
            {
                if (track.size() == Format(RegularFormat::AmigaDOS).sectors)
                    break;
                continue;
            }

            // Respect a user-defined sector count too.
            if (opt_sectors > 0 && track.size() >= opt_sectors)
                break;

            // If new sectors were found then sign the change.
            if (modified)
                scanRetries.wasChange = true;
        } while (scanRetries.HasMoreRetryMinusMinus());

        return TrackData(cylhead, std::move(track));
    }

    bool preload(const Range&/*range*/, int /*cyl_step*/) override
    {
        return false;
    }

    bool is_constant_disk() const override
    {
        return false;
    }

private:
    std::unique_ptr<FdrawcmdSys> m_fdrawcmd;
};

bool ReadFdrawcmdSysAB(const std::string& path, std::shared_ptr<Disk>& disk)
{
    if (util::lowercase(path) != "ab:")
        return false;

    auto fdrawcmd = FdrawcmdSys::Open(1);
    if (!fdrawcmd)
        throw util::exception(path, " requires non-USB drives A: and B:");

    FD_CMD_RESULT result{};
    fdrawcmd->SetEncRate(Encoding::MFM, DataRate::_500K);
    if (!fdrawcmd->CmdReadId(0, result) || GetLastError_MP() == ERROR_FLOPPY_ID_MARK_NOT_FOUND)
        throw util::exception("please insert a formatted high-density disk in B:");

    fdrawcmd.reset();
    fdrawcmd = FdrawcmdSys::Open(0);
    if (!fdrawcmd)
        throw util::exception("failed to open fdrawcmd.sys A:");
    else if (!fdrawcmd->FdCheckDisk())
        throw win32_error(GetLastError_MP(), "A");

    auto fdrawcmd_dev_disk = std::make_shared<FdrawSysDevABDisk>(std::move(fdrawcmd));
    fdrawcmd_dev_disk->extend(CylHead(83 - 1, 2 - 1));
    fdrawcmd_dev_disk->strType() = "fdrawcmd.sys";
    disk = fdrawcmd_dev_disk;

    return true;
}

bool WriteFdrawcmdSysAB(const std::string& path, std::shared_ptr<Disk>&/*disk*/)
{
    if (util::lowercase(path) != "ab:")
        return false;

    throw util::exception("2-drive writing is impossible");
}

#endif // HAVE_FDRAWCMD_H
