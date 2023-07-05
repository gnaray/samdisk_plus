// RPM command

#include "Disk.h"
#include "IBMPC.h"
#include "Image.h"
#include "Options.h"

#include <iomanip>
#include <memory>

static auto& opt_force = getOpt<int>("force");
static auto& opt_range = getOpt<Range>("range");
static auto& opt_rescans = getOpt<int>("rescans");
static auto& opt_retries = getOpt<int>("retries");

bool DiskRpm(const std::string& path)
{
    auto disk = std::make_shared<Disk>();
    ReadImage(path, disk);

    // Default to using cyl 0 head 0, but allow the user to override it
    CylHead cylhead(opt_range.empty() ? 0 :
        opt_range.cyl_end + 1, opt_range.head_end);

    auto forever = opt_force && util::is_stdout_a_tty();
    opt_retries = opt_rescans = 0;

    // Display 5 revolutions, or run forever if forced
    for (auto i = 0; forever || i < 5; ++i)
    {
        auto& track = disk->read_track(cylhead, true);

        if (!track.tracktime)
        {
            if (i == 0)
                throw util::exception("not available for this disk type");

            break;
        }

        auto time_us = track.tracktime;
        auto rpm = MIRCOSEC_PER_MINUTE / lossless_static_cast<double>(track.tracktime);

        std::stringstream ss;
        ss << std::setw(6) << time_us << " = " <<
            std::setprecision(2) << std::fixed << rpm << " rpm";

        if (forever)
            util::cout << "\r" << ss.str() << "  (Ctrl-C to stop)";
        else
            util::cout << ss.str() << "\n";

        util::cout.screen->flush();
    }

    return true;
}
