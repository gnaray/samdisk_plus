#ifndef TIMEDANDPHYSICALDUALTRACK_H
#define TIMEDANDPHYSICALDUALTRACK_H
//---------------------------------------------------------------------------

#include "Track.h"
#include "OrphanDataCapableTrack.h"

class TimedAndPhysicalDualTrack
{
public:
    bool SyncAndDemultiPhysicalToTimed(const int trackLen);

    Track timedIdTrack{};
    OrphanDataCapableTrack physicalTrackMulti{};
    OrphanDataCapableTrack lastTimeSyncedPhysicalTrackSingle{}; // It is the last result of SyncAndDemultiAndPhysicalToTimed created from physicalTrackMulti.
    Track timedIdDataAndPhysicalIdTrack{};
    Track finalAllInTrack{};
};

//---------------------------------------------------------------------------
#endif // TIMEDANDPHYSICALDUALTRACK_H
