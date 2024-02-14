#include "RetryPolicy.h"

#include <iomanip>

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

std::string RetryPolicy::ToString(bool /*onlyRelevantData*//* = true*/) const
{
    std::ostringstream ss;

    if (retryTimes == 0)
        ss << "No retry";
    else
    {
        ss << "Retry " << retryTimes << " time";
        if (retryTimes > 1)
            ss << 's';
        ss << ' ' << (sinceLastChange ? "since last change" : "total");
    }
    return ss.str();
}
