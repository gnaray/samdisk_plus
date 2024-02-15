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

    /* The last result of physicalTrackMulti created from physicalTrackMulti by
     * SyncAndDemultiPhysicalToTimed. It is demultid (i.e. Single) and also
     * synced if syncedTimedAndPhysicalTracks is true.
     */
    OrphanDataCapableTrack lastPhysicalTrackSingle{};
    bool syncedTimedAndPhysicalTracks = false;
    int lastPhysicalTrackSingleScore = 0;

    Track timedIdDataAndPhysicalIdTrack{};
    Track finalAllInTrack{};
};

//---------------------------------------------------------------------------
#endif // TIMEDANDPHYSICALDUALTRACK_H
