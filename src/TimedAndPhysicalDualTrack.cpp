#include "TimedAndPhysicalDualTrack.h"
#include "Options.h"

static auto& opt_debug = getOpt<int>("debug");

bool TimedAndPhysicalDualTrack::SyncAndDemultiPhysicalToTimed(const int trackLen)
{
    assert(trackLen > 0);

    lastPhysicalTrackSingle = physicalTrackMulti;
    syncedTimedAndPhysicalTracks = false;

    lastPhysicalTrackSingle.syncAndDemultiThisTrackToOffset(0, trackLen, false); // Only demulting here.

    // Sync by Time and Physical diff.
    int syncOffset;
    if (lastPhysicalTrackSingle.track.findSyncOffsetComparedTo(timedIdTrack, syncOffset)) // Found sync offset.
    {
        lastPhysicalTrackSingle.syncAndDemultiThisTrackToOffset(syncOffset, trackLen, true); // Only syncing here.
        syncedTimedAndPhysicalTracks = true;
    }

    const auto scoreNew = lastPhysicalTrackSingle.Score();
    const bool foundNewValuableSomething = scoreNew > lastPhysicalTrackSingleScore; // Found new valuable something.
    lastPhysicalTrackSingleScore = scoreNew;
    return foundNewValuableSomething;
}
