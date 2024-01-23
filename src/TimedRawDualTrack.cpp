#include "TimedRawDualTrack.h"
#include "Options.h"

static auto& opt_debug = getOpt<int>("debug");

bool TimedRawDualTrack::SyncAndDemultiRawToTimed(const int trackLen)
{
    assert(trackLen > 0);

    OrphanDataCapableTrack lastTimedRawTrackSingleLocal = rawTrackMulti;
    lastTimedRawTrackSingleLocal.syncAndDemultiThisTrackToOffset(0, trackLen); // Only demulting here.
    // Sync by Time and Raw diff.
    int syncOffset;
    if (!lastTimedRawTrackSingleLocal.track.findSyncOffsetComparedTo(timedTrack, syncOffset)) // Can not sync.
        return false;
    lastTimedRawTrackSingleLocal.syncAndDemultiThisTrackToOffset(syncOffset, trackLen); // Syncing here.
    lastTimedRawTrackSingle = lastTimedRawTrackSingleLocal;
    return true;
}
