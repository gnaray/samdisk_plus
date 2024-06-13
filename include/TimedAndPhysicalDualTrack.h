#ifndef TIMEDANDPHYSICALDUALTRACK_H
#define TIMEDANDPHYSICALDUALTRACK_H
//---------------------------------------------------------------------------

#include "Track.h"
#include "OrphanDataCapableTrack.h"

class TimedAndPhysicalDualTrack
{
public:
    bool SyncDemultiMergePhysicalUsingTimed(OrphanDataCapableTrack&& toBeMergedODCTrack, const RepeatedSectors& repeatedSectorIds);

    Track timedIdTrack{};
    /* Cumulative last result of adding each newly read synced and demultid
     * physical multi track by SyncDemultiMergePhysicalUsingTimed.
     */
    OrphanDataCapableTrack lastPhysicalTrackSingle{};
    int lastPhysicalTrackSingleScore = 0;

    Track timedIdDataAndPhysicalIdTrack{};
    Track finalAllInTrack{};
};

//---------------------------------------------------------------------------
#endif // TIMEDANDPHYSICALDUALTRACK_H
