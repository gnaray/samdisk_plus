#include "OrphanDataCapableTrack.h"
#include "Options.h"

static auto& opt_debug = getOpt<int>("debug");

bool OrphanDataCapableTrack::empty() const
{
    return track.empty() && orphanDataTrack.empty();
}

DataRate OrphanDataCapableTrack::getDataRate() const
{
    assert(!track.empty());

    return track.getDataRate();
}

Encoding OrphanDataCapableTrack::getEncoding() const
{
    assert(!track.empty());

    return track.getEncoding();
}

int OrphanDataCapableTrack::getTimeOfOffset(const int offset) const
{
    assert(!track.empty());

    return track.getTimeOfOffset(offset); // microsec
}

int OrphanDataCapableTrack::getOffsetOfTime(const int time) const
{
    assert(!track.empty());

    return track.getOffsetOfTime(time); // mfmbits
}

int OrphanDataCapableTrack::getTrackLen() const
{
    return track.tracklen;
}

// In a standard Track object the tracklen is the length of one rotation. Here it is the length of read bytes which were decoded.
void OrphanDataCapableTrack::setTrackLen(const int trackLen)
{
    track.tracklen = trackLen;
    orphanDataTrack.tracklen = trackLen;
    if (!track.empty())
    {
        track.tracktime = getTimeOfOffset(trackLen);
        orphanDataTrack.tracktime = track.tracktime;
    }
}

void OrphanDataCapableTrack::addTrackLen(const int trackLen)
{
    setTrackLen(getTrackLen() + trackLen);
}

int OrphanDataCapableTrack::getTrackTime() const
{
    return track.tracktime;
}

void OrphanDataCapableTrack::setTrackTime(const int trackTime)
{
    track.tracktime = trackTime;
    orphanDataTrack.tracktime = trackTime;
    if (!track.empty())
    {
        track.tracklen = getOffsetOfTime(trackTime);
        orphanDataTrack.tracklen = track.tracktime;
    }
}

void OrphanDataCapableTrack::addTrackTime(const int trackTime)
{
    setTrackTime(getTrackTime() + trackTime);
}

void OrphanDataCapableTrack::add(OrphanDataCapableTrack&& orphanDataCapableTrack)
{
    track.add(std::move(orphanDataCapableTrack.track));
    orphanDataTrack.add(std::move(orphanDataCapableTrack.orphanDataTrack));
    cylheadMismatch |= orphanDataCapableTrack.cylheadMismatch;
}

int OrphanDataCapableTrack::size() const
{
    return track.size() + orphanDataTrack.size();
}

void OrphanDataCapableTrack::mergeRawTrack(const CylHead& cylhead, const RawTrackMFM& toBeMergedRawTrack)
{
    auto orphanDataCapableTrack = toBeMergedRawTrack.DecodeTrack(cylhead);
    mergeRawTrack(std::move(orphanDataCapableTrack));
}

void OrphanDataCapableTrack::mergeRawTrack(OrphanDataCapableTrack&& toBeMergedODCTrack)
{
    if (toBeMergedODCTrack.empty())
    {
        setTrackLen(toBeMergedODCTrack.getTrackLen());
        return;
    }
    if (empty())
    {
        *this = toBeMergedODCTrack;
        return;
    }
    // Sync the two tracks then add the to be merged track with synced sector offsets to the merged track (this).
    int syncOffsetBest;
    if (!toBeMergedODCTrack.track.findSyncOffsetComparedTo(track, syncOffsetBest)) // Can not sync.
    {
        if (toBeMergedODCTrack.track.size() < track.size() || toBeMergedODCTrack.orphanDataTrack.size() <= orphanDataTrack.size()) // The to be merged track is worse.
            return;
        *this = toBeMergedODCTrack;
        return;
    }
    // Secondly apply the sync preferably on to be merged track (if the offsets can remain positive).
    toBeMergedODCTrack.syncThisToOtherAsMulti(syncOffsetBest, *this);
    // Thirdly merge the track thus its sectors. Plus merge the tracklen and tracktime,
    // which could be be calculated from this tracklen plus the added sectors at the beginning or at the end. Unimportant.
    add(std::move(toBeMergedODCTrack));
}

// Apply the sync preferably on this track. However the offsets must be positive, thus opposite sync the targetTrack if have to.
void OrphanDataCapableTrack::syncThisToOtherAsMulti(int syncOffset, OrphanDataCapableTrack& targetODCTrack)
{
    if (syncOffset == 0 || track.empty())
        return;
    const auto offsetMin = orphanDataTrack.empty() ? track.begin()->offset : std::min(track.begin()->offset, orphanDataTrack.begin()->offset);
    if (syncOffset < 0 || offsetMin - syncOffset > 0)
    {
        for (auto& s : track.sectors())
            s.offset -= syncOffset;
        for (auto& s : orphanDataTrack.sectors())
            s.offset -= syncOffset;
        addTrackLen(-syncOffset);
    }
    else
    {
        for (auto& s : targetODCTrack.track.sectors())
            s.offset += syncOffset;
        for (auto& s : targetODCTrack.orphanDataTrack.sectors())
            s.offset += syncOffset;
        targetODCTrack.addTrackLen(syncOffset);
    }
}

void OrphanDataCapableTrack::syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle)
{
    track.syncAndDemultiThisTrackToOffset(syncOffset, trackLenSingle);
    orphanDataTrack.syncAndDemultiThisTrackToOffset(syncOffset, trackLenSingle);
}

int OrphanDataCapableTrack::determineBestTrackLen(const int timedTrackLen) const
{
    assert(timedTrackLen > 0);

    return track.determineBestTrackLen(timedTrackLen);
}
