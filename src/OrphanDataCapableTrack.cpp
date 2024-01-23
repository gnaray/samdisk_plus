#include "OrphanDataCapableTrack.h"
#include "IBMPCBase.h"
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

void OrphanDataCapableTrack::set(OrphanDataCapableTrack&& orphanDataCapableTrack)
{
    const auto cylheadMismatchTemp = cylheadMismatch;
    *this = orphanDataCapableTrack;
    cylheadMismatch |= cylheadMismatchTemp;
}

void OrphanDataCapableTrack::mergeRawTrack(const CylHead& cylhead, const RawTrackMFM& toBeMergedRawTrack)
{
    auto orphanDataCapableTrack = toBeMergedRawTrack.DecodeTrack(cylhead);
    mergeRawTrack(std::move(orphanDataCapableTrack));
}

void OrphanDataCapableTrack::mergeRawTrack(OrphanDataCapableTrack&& toBeMergedODCTrack)
{
    if (toBeMergedODCTrack.empty())
        return;
    if (empty())
    {
        *this = toBeMergedODCTrack;
        return;
    }
    /* Sync the two tracks then add the to be merged track with synced sector offsets to the merged track (this). */
    // Firstly find the best sync (offset diff).
    std::vector<int> offsetDiffs;
    const auto trackEnd = track.end();
    for (auto& s : toBeMergedODCTrack.track.sectors())
    {
        auto itMerged = track.find(s.header);
        while (itMerged != trackEnd)
        {
            offsetDiffs.push_back(itMerged->offset - s.offset);
            itMerged = track.findNext(s.header, itMerged);
        }
    }
    if (offsetDiffs.empty())
    {
        if (toBeMergedODCTrack.track.size() <= track.size()) // Can not sync and to be merged track is worse.
            return;
        set(std::move(toBeMergedODCTrack));
        return;
    }
    const auto offsetDiffBest = findMostPopularDiff(offsetDiffs);
    // Secondly apply the sync preferably on to be merged track (if the offsets can remain positive).
    toBeMergedODCTrack.syncThisToOtherAsMulti(offsetDiffBest, *this);
    // Thirdly merge the track thus its sectors. Plus merge the tracklen and tracktime,
    // which could be be calculated from this tracklen plus the added sectors at the beginning or at the end. Unimportant.
    add(std::move(toBeMergedODCTrack));
}

// Apply the sync preferably on this track. However the offsets must be positive, thus sync the targetTrack if have to.
void OrphanDataCapableTrack::syncThisToOtherAsMulti(int offsetDiff, OrphanDataCapableTrack& targetODCTrack)
{
    if (track.empty() || offsetDiff == 0)
        return;
    if (offsetDiff < 0 && track.begin()->offset + offsetDiff <= 0)
    {
        for (auto& s : targetODCTrack.track.sectors())
            s.offset -= offsetDiff;
        for (auto& s : targetODCTrack.orphanDataTrack.sectors())
            s.offset -= offsetDiff;
        targetODCTrack.addTrackLen(-offsetDiff);
    }
    else
    {
        for (auto& s : track.sectors())
            s.offset += offsetDiff;
        for (auto& s : orphanDataTrack.sectors())
            s.offset += offsetDiff;
        addTrackLen(offsetDiff);
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

    if (track.empty())
        return 0;
    std::vector<int> offsetDiffs;
    auto sectorsOrderedByHeader = track.sectors();
    std::sort(sectorsOrderedByHeader.begin(), sectorsOrderedByHeader.end(),
              [](const Sector& s1, const Sector& s2) { return s1.header < s2.header || (s1.header == s2.header && s1.offset < s2.offset); });
    const auto sectorsOrderedByHeaderEnd = sectorsOrderedByHeader.end();
    for (auto it = sectorsOrderedByHeader.begin(); it != sectorsOrderedByHeaderEnd; it++)
    {
        const auto it0 = it;
        while (++it != sectorsOrderedByHeaderEnd && it->header == it0->header)
        {
            offsetDiffs.push_back(it->offset - it0->offset);
        }
        it = it0;
    }
    if (offsetDiffs.empty())
        return 0;
    const auto offsetDiffBest = findMostPopularDiff(offsetDiffs); // This can be multiple tracklen. It must be reduced.
    const auto multi = round_AS<int>(static_cast<double>(offsetDiffBest) / timedTrackLen);
    if (multi == 0)
    {
        if (opt_debug)
            util::cout << "determineBestTrackLen found offsetDiffBest " << offsetDiffBest << " to be too low compared to timedTrackLen " << timedTrackLen << "\n";
        return 0;
    }
    const auto trackLenBest = round_AS<int>(static_cast<double>(offsetDiffBest) / multi);
    if (opt_debug)
    {
        if (std::abs(trackLenBest - timedTrackLen) > Track::COMPARE_TOLERANCE_BITS)
            util::cout << "determineBestTrackLen found trackLenBest " << trackLenBest << " to be outside of tolerated timedTrackLen " << timedTrackLen << "\n";
    }
    if (opt_debug)
        util::cout << "determineBestTrackTime found trackLenBest " << trackLenBest << "\n";
    return trackLenBest;
}
