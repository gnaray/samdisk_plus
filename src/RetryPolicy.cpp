#include "RetryPolicy.h"

#include <iomanip>

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
