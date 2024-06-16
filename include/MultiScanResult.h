#ifndef MULTISCANRESULT_H
#define MULTISCANRESULT_H
//---------------------------------------------------------------------------

#include "Util.h"
#include "fdrawcmd.h"

class MultiScanResult : public MEMORY
{
public:
    explicit MultiScanResult(int sectorCount)
        : MEMORY((intsizeof(FD_TIMED_MULTI_SCAN_RESULT) + intsizeof(FD_TIMED_MULTI_ID_HEADER)) * sectorCount)
    {
    }

    inline operator FD_TIMED_MULTI_SCAN_RESULT* ()
    {
        return reinterpret_cast<FD_TIMED_MULTI_SCAN_RESULT*>(pb);
    }
    inline operator const FD_TIMED_MULTI_SCAN_RESULT* () const
    {
        return reinterpret_cast<FD_TIMED_MULTI_SCAN_RESULT*>(pb);
    }
    inline int count() const
    {
        return this->operator const FD_TIMED_MULTI_SCAN_RESULT*()->count;
    }
    inline int trackTime() const
    {
        return static_cast<int>(this->operator const FD_TIMED_MULTI_SCAN_RESULT*()->tracktime);
    }
    inline void SetTrackTime(const int trackTime)
    {
        this->operator FD_TIMED_MULTI_SCAN_RESULT*()->tracktime = static_cast<unsigned>(trackTime);
    }
    inline int byteToleranceOfTime() const
    {
        return this->operator const FD_TIMED_MULTI_SCAN_RESULT*()->byte_tolerance_of_time;
    }
    inline int trackRetries() const
    {
        return this->operator const FD_TIMED_MULTI_SCAN_RESULT*()->track_retries;
    }
    inline const FD_TIMED_MULTI_ID_HEADER* HeaderArray() const
    {
        return (this->operator const FD_TIMED_MULTI_SCAN_RESULT*())->HeaderArray();
    }
    inline const FD_TIMED_MULTI_ID_HEADER& HeaderArray(const int index) const
    {
        return const_cast<FD_TIMED_MULTI_SCAN_RESULT*>(this->operator const FD_TIMED_MULTI_SCAN_RESULT*())->HeaderArray()[index];
    }

    Track DecodeResult(const CylHead& cylhead, const DataRate& dataRate, const Encoding& encoding) const;
};

//---------------------------------------------------------------------------
#endif // MULTISCANRESULT_H
