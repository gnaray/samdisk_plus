#include "TimedAndPhysicalDualTrack.h"
#include "Options.h"

static auto& opt_debug = getOpt<int>("debug");

bool TimedAndPhysicalDualTrack::SyncAndDemultiPhysicalToTimed(const int trackLen)
{
    assert(trackLen > 0);

    OrphanDataCapableTrack lastTimedAndPhysicalTrackSingleLocal = physicalTrackMulti;
    lastTimedAndPhysicalTrackSingleLocal.syncAndDemultiThisTrackToOffset(0, trackLen, false); // Only demulting here.
    // Sync by Time and Physical diff.
    int syncOffset;
    if (!lastTimedAndPhysicalTrackSingleLocal.track.findSyncOffsetComparedTo(timedTrack, syncOffset)) // Can not sync.
        return false;
    lastTimedAndPhysicalTrackSingleLocal.syncAndDemultiThisTrackToOffset(syncOffset, trackLen, true); // Only syncing here.
    lastTimedAndPhysicalTrackSingle = lastTimedAndPhysicalTrackSingleLocal;
    return true;
}
