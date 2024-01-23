#ifndef TIMEDRAWDUALTRACK_H
#define TIMEDRAWDUALTRACK_H
//---------------------------------------------------------------------------

#include <Track.h>
#include "OrphanDataCapableTrack.h"

class TimedRawDualTrack
{
public:
    bool SyncAndDemultiRawToTimed(const int trackLen);

    Track timedTrack{};
    OrphanDataCapableTrack rawTrackMulti{};
    OrphanDataCapableTrack lastTimedRawTrackSingle{}; // It is last result of SyncAndDemultiRawToTimed created from rawTrackMulti.
    Track finalTimedAndRawTrack{};
};

//---------------------------------------------------------------------------
#endif // TIMEDRAWDUALTRACK_H
