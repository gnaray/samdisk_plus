// fdrawcmd.sys driver checking

#include "config.h"

#ifdef HAVE_FDRAWCMD_H
#include "Platform.h"
#include "fdrawcmd.h"

#include "FdrawcmdSys.h"
#include "utils.h"

util::Version GetDriverVersion()
{
    util::Version version{ 0 };

    const HANDLE h = CreateFile(R"(\\.\fdrawcmd)", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD ret, dwVersion = 0;
        if (DeviceIoControl(h, IOCTL_FDRAWCMD_GET_VERSION, nullptr, 0, &dwVersion, sizeof(dwVersion), &ret, nullptr))
            version.value = dwVersion;
        CloseHandle(h);
    }

    return version;
}


bool CheckDriver()
{
    const auto version = GetDriverVersion();
    const auto compatible = (version.value & 0xffff0000) == (FDRAWCMD_VERSION & 0xffff0000);

    // Compatible driver installed?
    if (version.value && compatible)
        return true;

    // If a version was found we're not compatible with it
    if (version.value)
        throw util::exception("installed fdrawcmd.sys is incompatible, please update");
    else
        util::cout << "\nFloppy features require fdrawcmd.sys from " << colour::CYAN << "http://simonowen.com/fdrawcmd/" << colour::none << '\n';

    return false;
}

bool IsFdcDriverRunning()
{
    auto running = false;

    // Open the Service Control manager
    SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, GENERIC_READ);
    if (hSCM)
    {
        // Open the FDC service, for fdc.sys (floppy disk controller driver)
        SC_HANDLE hService = OpenService(hSCM, "fdc", GENERIC_READ);
        if (hService)
        {
            SERVICE_STATUS ss;
            if (QueryServiceStatus(hService, &ss))
            {
                // If fdc.sys is running then fdrawcmd.sys is supported
                running = ss.dwCurrentState == SERVICE_RUNNING;
            }

            CloseServiceHandle(hService);
        }

        CloseServiceHandle(hSCM);
    }

    return running;
}


bool ReportDriverVersion()
{
    const auto version = GetDriverVersion();

    if (!version.value)
    {
        if (IsFdcDriverRunning())
            util::cout << "\nfdrawcmd.sys is not currently installed.\n";
        else
            util::cout << "\nfdrawcmd.sys is not supported on this system.\n";

        return false;
    }

    // Report the version number of the active driver
    util::cout << util::fmt("\nfdrawcmd.sys version %hhu.%hhu.%hhu.%hhu is installed.",
        version.MajorValue(), version.MinorValue(), version.MaintenanceValue(), version.BuildValue());

    if (version.value < FDRAWCMD_VERSION) util::cout << ' ' << colour::YELLOW << "[update available]" << colour::none;
    util::cout << '\n';

    return true;
}

#endif // HAVE_FDRAWCMD_H
