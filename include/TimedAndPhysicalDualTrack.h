#ifndef TIMEDANDPHYSICALDUALTRACK_H
#define TIMEDANDPHYSICALDUALTRACK_H
//---------------------------------------------------------------------------

#include "Track.h"
#include "OrphanDataCapableTrack.h"

class TimedAndPhysicalDualTrack
{
public:
    bool SyncAndDemultiPhysicalToTimed(const int trackLen);

    Track timedTrack{};
    OrphanDataCapableTrack physicalTrackMulti{};
    OrphanDataCapableTrack lastTimedAndPhysicalTrackSingle{}; // It is the last result of SyncAndDemultiAndPhysicalToTimed created from physicalTrackMulti.
    Track finalTimedAndPhysicalTrack{};
};

//---------------------------------------------------------------------------
#endif // TIMEDANDPHYSICALDUALTRACK_H
