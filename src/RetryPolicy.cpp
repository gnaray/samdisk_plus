#include "RetryPolicy.h"
#include "Util.h"

#include <iomanip>

std::string to_string(const RetryPolicy& retryPolicy)
{
    std::ostringstream ss;

    if (retryPolicy.retryTimes == 0)
        ss << "No retry";
    else
    {
        ss << "Retry " << retryPolicy.retryTimes << " time";
        if (retryPolicy.retryTimes > 1)
            ss << 's';
        ss << ' ' << (retryPolicy.sinceLastChange ? "since last change" : "total");
    }
    return ss.str();
}


RetryPolicy::RetryPolicy(const int retryTimesOption)
{
    if (retryTimesOption >= 0)
    {
        retryTimes = retryTimesOption;
        sinceLastChange = false;
    }
    else
    {
        retryTimes = -retryTimesOption;
        sinceLastChange = true;
    }
}

RetryPolicy::RetryPolicy(const int retryTimes, const bool sinceLastChange)
    : retryTimes(retryTimes), sinceLastChange(sinceLastChange)
{
}

