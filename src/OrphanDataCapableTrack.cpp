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

    return track[0].datarate;
}

Encoding OrphanDataCapableTrack::getEncoding() const
{
    assert(!track.empty());

    return track[0].encoding;
}

int OrphanDataCapableTrack::getTimeOfOffset(const int offset) const
{
    assert(!track.empty());
    const auto mfmbit_us = GetFmOrMfmDataBitsTime(track.begin()->datarate, track.begin()->encoding); // microsec/mfmbits
    return round_AS<int>(offset * mfmbit_us);
}

int OrphanDataCapableTrack::getOffsetOfTime(const int time) const
{
    assert(!track.empty());
    const auto mfmbit_us = GetFmOrMfmDataBitsTime(track.begin()->datarate, track.begin()->encoding); // microsec/mfmbits
    return round_AS<int>(time / mfmbit_us);
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

/*static*/ int OrphanDataCapableTrack::findMostPopularDiff(std::vector<int>& diffs)
{
    assert(diffs.size() > 0);
    std::sort(diffs.begin(), diffs.end());
    typedef std::pair<int, int> ParticipantsAndAverage;
    std::vector<ParticipantsAndAverage> participantsAndAveragedOffsetDiffs;
    const auto diffsEnd = diffs.end();
    for (auto it = diffs.begin(); it != diffsEnd; it++)
    {
        const auto it0 = it;
        auto s = *(it++);
        while (it != diffsEnd && *it < *it0 + Track::COMPARE_TOLERANCE_BITS)
            s += *(it++);
        participantsAndAveragedOffsetDiffs.push_back(std::make_pair(it - it0, s / (it - it0)));
        it--;
    }
    std::sort(participantsAndAveragedOffsetDiffs.begin(), participantsAndAveragedOffsetDiffs.end(),
              [](const ParticipantsAndAverage& a, const ParticipantsAndAverage& b) { return a.first > b.first || (a.first == b.first && a.second < b.second); });
    return participantsAndAveragedOffsetDiffs[0].second;
}

void OrphanDataCapableTrack::mergeRawTrack(const CylHead& cylhead, const RawTrackMFM& toBeMergedRawTrack)
{
    auto orphanDataCapableTrack = toBeMergedRawTrack.DecodeTrack(cylhead);
    mergeRawTrack(std::move(orphanDataCapableTrack));
}

void OrphanDataCapableTrack::mergeRawTrack(OrphanDataCapableTrack&& toBeMergedTrack)
{
    if (toBeMergedTrack.empty())
        return;
    if (empty())
    {
        *this = toBeMergedTrack;
        return;
    }
    /* Sync the two tracks then add the to be merged track with synced sector offsets to the merged track (this). */
    // Firstly find the best sync (offset diff).
    std::vector<int> offsetDiffs;
    const auto trackEnd = track.end();
    for (auto& s : toBeMergedTrack.track.sectors())
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
        if (toBeMergedTrack.track.size() <= track.size()) // Can not sync and to be merged track is worse.
            return;
        set(std::move(toBeMergedTrack));
        return;
    }
    const auto offsetDiffBest = findMostPopularDiff(offsetDiffs);
    // Secondly apply the sync preferably on to be merged track (if the offsets can remain positive).
    toBeMergedTrack.sync(offsetDiffBest, *this);
    // Thirdly merge the track thus its sectors.
    add(std::move(toBeMergedTrack));
    // Merge the tracklen. In real it should be calculated from this tracklen
    // plus the added sectors at the beginning or at the end. It is not so important.
    setTrackLen(std::max(getTrackLen(), toBeMergedTrack.getTrackLen()));
}

// Apply the sync preferably on this track. However the offsets must be positive, thus sync the targetTrack if have to.
void OrphanDataCapableTrack::sync(int offsetDiff, OrphanDataCapableTrack& targetTrack)
{
    if (track.empty() || offsetDiff == 0)
        return;
    if (offsetDiff < 0 && track.begin()->offset + offsetDiff <= 0)
    {
        for (auto& s : targetTrack.track.sectors())
            s.offset -= offsetDiff;
        for (auto& s : targetTrack.orphanDataTrack.sectors())
            s.offset -= offsetDiff;
    }
    else
    {
        for (auto& s : track.sectors())
            s.offset += offsetDiff;
        for (auto& s : targetTrack.orphanDataTrack.sectors())
            s.offset += offsetDiff;
    }
}

int OrphanDataCapableTrack::determineBestTrackTime(const int timedTrackTime) const
{
    assert(timedTrackTime > 0);
    if (track.empty())
        return 0;
    std::vector<int> offsetDiffs;
    auto sectorsOrderedByHeader = track.sectors();
    std::sort(sectorsOrderedByHeader.begin(), sectorsOrderedByHeader.end(),
              [](const Sector& a, const Sector& b) { return a.header < b.header || (a.header == b.header && a.offset < b.offset); });
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
    const auto offsetDiffBest = findMostPopularDiff(offsetDiffs); // This can be multiple tracktime. It must be reduced.
    const auto timeDiffBest = getTimeOfOffset(offsetDiffBest);
    const auto multi = static_cast<int>(std::round(static_cast<double>(timeDiffBest) / timedTrackTime));
    if (multi == 0)
    {
        if (opt_debug)
            util::cout << "determineBestTrackTime found timeDiffBest " << timeDiffBest << " to be too low compared to timedTrackTime " << timedTrackTime << "\n";
        return 0;
    }
    const auto trackTimeBest = static_cast<int>(timeDiffBest / multi);
    if (opt_debug)
    {
        const auto allowedTimeTolerance = getTimeOfOffset(Track::COMPARE_TOLERANCE_BITS);
        if (trackTimeBest - timedTrackTime > allowedTimeTolerance || timedTrackTime - trackTimeBest > allowedTimeTolerance)
            util::cout << "determineBestTrackTime found trackTimeBest " << trackTimeBest << " to be outside of tolerated timedTrackTime " << timedTrackTime << "\n";
    }
    if (opt_debug)
        util::cout << "determineBestTrackTime found trackTimeBest " << trackTimeBest << "\n";
    return trackTimeBest;
}
