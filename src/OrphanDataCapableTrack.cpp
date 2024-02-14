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

// In the Track class the tracklen is the length of one rotation. Here it is the length of read bytes which were decoded.
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

int OrphanDataCapableTrack::Score() const
{
    /* Data copy: a read sequence of bytes.
     * Data: set of 1 ore more data copies.
     * The goal is defining a scoring which prefers good ids,
     * data connected to id (not orphan), the more good data copy,
     * good data copy read count (stability), the more (bad) data.
     * Good id: 1 point.
     * Good id's data exist: 2 point.
     * Each good data copy: read count point.
     * Orphan's (bad) data exist: 1 point.
     */
    auto score = 0;
    for (const auto& sector : track.sectors())
        if (!sector.has_badidcrc())
        {
            score++; // Good id.
            const auto iSup = sector.copies();
            if (iSup > 0)
            {
                score += 2; // Good id's data exist
                if (sector.has_good_data())
                {
                    for (auto i = 0; i < iSup; i++)
                        score += sector.data_copy_read_stats(i).ReadCount(); // Good data read count.
                }
            }
        }
    score += orphanDataTrack.size();
    return score;
}

// Merge the orphans track into parents track. An orphan can be merged if a coherent parent is found.
/*static*/ void OrphanDataCapableTrack::MergeOrphansIntoParents(Track& orphansTrack, Track& parentsTrack, bool removeOrphanAfterMerge, const std::function<bool (const Sector &)>& considerParentSectorPredicate/* = nullptr*/)
{
    if (orphansTrack.empty() || parentsTrack.empty())
        return;

    // An orphan data and a parent sector are matched if they cohere and there is no closer to one of them that also coheres.
    auto itParent = parentsTrack.begin();
    auto itOrphan = orphansTrack.begin();
    const auto dataRate = orphansTrack.getDataRate();
    const auto encoding = orphansTrack.getEncoding();
    const auto itParentEnd = parentsTrack.end(); // The parents end is constant since merging sector data to parent does not change the parents.
    while (itParent != itParentEnd && itOrphan != orphansTrack.end()) // The orphans end is variable since possibly erasing sector from orphans.
    {
        auto& orphanDataSector = *itOrphan;
        auto& parentSector = *itParent;
        if (considerParentSectorPredicate && !considerParentSectorPredicate(parentSector))
        {
            itParent++;
            continue;
        }

        const auto cohereResult = DoSectorIdAndDataOffsetsCohere(parentSector.offset, orphanDataSector.offset, dataRate, encoding);
        if (cohereResult == CohereResult::DataCoheres)
        {
            const auto itParentNext = itParent + 1;
            if (itParentNext != itParentEnd &&
                    DoSectorIdAndDataOffsetsCohere(itParentNext->offset, orphanDataSector.offset, dataRate, encoding) == CohereResult::DataCoheres)
            {
                itParent = itParentNext;
                continue;
            }
            if (opt_debug)
                util::cout << "MergeOrphansIntoParents: adding orphan data sector (offset=" << orphanDataSector.offset
                           << ", id.sector=" << orphanDataSector.header.sector << ") to parent (offset=" << parentSector.offset
                           << ", id.sector=" << parentSector.header.sector << ")\n";
            if (removeOrphanAfterMerge)
            {
                parentSector.MergeOrphanDataSector(std::move(orphanDataSector));
                itOrphan = orphansTrack.sectors().erase(itOrphan);
                continue;
            }
            else
                parentSector.MergeOrphanDataSector(orphanDataSector);
            itOrphan++;
        }
        else if (cohereResult == CohereResult::DataTooEarly)
            itOrphan++;
        else
            itParent++;
    }
}

// Merge the orphans track of this in parents track of this. An orphan can be merged if a coherent parent is found.
void OrphanDataCapableTrack::MergeOrphansIntoParents(bool removeOrphanAfterMerge)
{
    MergeOrphansIntoParents(orphanDataTrack, track, removeOrphanAfterMerge);
}

// Add the orphan data capable track to this after fixing (removing last orphan data sector to track end) and syncing.
void OrphanDataCapableTrack::AddWithSyncAndBrokenEndingFix(OrphanDataCapableTrack&& orphanDataCapableTrack)
{
    const auto lastSectorIndex = orphanDataCapableTrack.orphanDataTrack.size() - 1;
    if (lastSectorIndex >=0) // Not empty.
    {
        const auto& sector = orphanDataCapableTrack.orphanDataTrack[lastSectorIndex];
        // Does the last orphan data sector end at track end? If yes then we ignore it since it can be broken.
        if (BitOffsetAsDataBytePosition(orphanDataCapableTrack.orphanDataTrack.tracklen - sector.offset) == sector.data_size())
        {
            if (opt_debug)
                util::cout << "AddWithSyncAndBrokenEndingFix: ignoring possibly broken orphan data sector at track end (offset=" << sector.offset << ", id.sector=" << sector.header.sector << ")\n";
            orphanDataCapableTrack.orphanDataTrack.remove(lastSectorIndex);
        }
    }

    if (empty())
    {
        *this = orphanDataCapableTrack;
        return;
    }
    if (orphanDataCapableTrack.empty())
        return;

    // Sync the two tracks then add the to be merged track with synced sector offsets to the merged track (this).
    int syncOffsetBest;
    if (!orphanDataCapableTrack.track.findSyncOffsetComparedTo(track, syncOffsetBest)) // Can not sync.
    {
        if (opt_debug)
            util::cout << "AddWithSyncAndBrokenEndingFix: can not sync the tracks, choosing the better one\n";
        if (orphanDataCapableTrack.track.size() > track.size() ||
                (orphanDataCapableTrack.track.size() == track.size() && orphanDataCapableTrack.orphanDataTrack.size() > orphanDataTrack.size())) // The to be merged track is better.
            *this = orphanDataCapableTrack;
        return;
    }
    // Apply the sync preferably on to be merged track (if the offsets can remain positive), otherwise on this.
    orphanDataCapableTrack.syncThisToOtherAsMulti(syncOffsetBest, *this);
    // Merge the to be merged track (including the tracklen and tracktime and cylheadMismatch).
    add(std::move(orphanDataCapableTrack));
}

// Merge the orphan data capable track into the target track.
void OrphanDataCapableTrack::MergeInto(Track& targetTrack, const std::function<bool (const Sector &)>& considerTargetSectorPredicate/* = nullptr*/)
{
    assert(targetTrack.tracklen == track.tracklen && targetTrack.tracklen == orphanDataTrack.tracklen);

    targetTrack.add(Sectors(track.sectors()), considerTargetSectorPredicate);
    MergeOrphansIntoParents(orphanDataTrack, targetTrack, false, considerTargetSectorPredicate);
}

// Merge the physical track into this after it is decoded into an orphan data capable track.
void OrphanDataCapableTrack::MergePhysicalTrack(const CylHead& cylhead, const PhysicalTrackMFM& toBeMergedPhysicalTrack)
{
    auto orphanDataCapableTrack = toBeMergedPhysicalTrack.DecodeTrack(cylhead);
    MergeUnsyncedBrokenEndingTrack(std::move(orphanDataCapableTrack));
}

// Merge the orphan data capable track into this after fixing (removing last orphan data sector to track end) and syncing, and merge the orphan data sectors into possible parents.
void OrphanDataCapableTrack::MergeUnsyncedBrokenEndingTrack(OrphanDataCapableTrack&& toBeMergedODCTrack)
{
    AddWithSyncAndBrokenEndingFix(std::move(toBeMergedODCTrack));

    // If there are orphan data sectors then merge the them into possible parents.
    MergeOrphansIntoParents(true);
}

// Apply the sync preferably on this track. However the offsets must be positive, thus opposite sync the targetTrack if have to.
void OrphanDataCapableTrack::syncThisToOtherAsMulti(const int syncOffset, OrphanDataCapableTrack& targetODCTrack)
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

void OrphanDataCapableTrack::syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle, bool syncOnly)
{
    track.syncAndDemultiThisTrackToOffset(syncOffset, trackLenSingle, syncOnly);
    orphanDataTrack.syncAndDemultiThisTrackToOffset(syncOffset, trackLenSingle, syncOnly);
}

int OrphanDataCapableTrack::determineBestTrackLen(const int timedTrackLen) const
{
    assert(timedTrackLen > 0);

    return track.determineBestTrackLen(timedTrackLen);
}
