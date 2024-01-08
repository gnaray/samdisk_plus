#ifndef ORPHANDATACAPABLETRACK_H
#define ORPHANDATACAPABLETRACK_H
//---------------------------------------------------------------------------

class OrphanDataCapableTrack; // Required because this and RawTrackMFM includes each other.

#include "RawTrackMFM.h"
#include <Track.h>

class OrphanDataCapableTrack
{
public:
    OrphanDataCapableTrack() = default;

    bool empty() const;
    DataRate getDataRate() const;
    Encoding getEncoding() const;
    int getTimeOfOffset(const int offset) const;
    int getOffsetOfTime(const int time) const;
    int getTrackLen() const;
    void setTrackLen(const int trackLen);
    int getTrackTime() const;
    void setTrackTime(const int trackTime);
    void add(OrphanDataCapableTrack&& orphanDataCapableTrack);
    void set(OrphanDataCapableTrack&& orphanDataCapableTrack);

protected:
    static int findMostPopularDiff(std::vector<int>& diffs);

public:
    void mergeRawTrack(const CylHead& cylhead, const RawTrackMFM& toBeMergedRawTrack);
    void mergeRawTrack(OrphanDataCapableTrack&& toBeMergedODCTrack);
    void sync(int offsetDiffBest, OrphanDataCapableTrack& targetTrack);
    int determineBestTrackTime(const int timedTrackTime) const;

    static constexpr int ORPHAN_SECTOR_ID = 256;
    Track track{};
    Track orphanDataTrack{};
    bool cylheadMismatch = false;
    int trackIndexOffset = 0;
};

//---------------------------------------------------------------------------
#endif // ORPHANDATACAPABLETRACK_H
