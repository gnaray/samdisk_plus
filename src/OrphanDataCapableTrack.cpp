#include "OrphanDataCapableTrack.h"
#include "Options.h"
#include "RingedInt.h"

#include <iomanip>

static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");

bool OrphanDataCapableTrack::empty() const
{
    return track.empty() && orphanDataTrack.empty();
}

DataRate OrphanDataCapableTrack::getDataRate() const
{
    assert(!empty());

    return track.empty() ? orphanDataTrack.getDataRate() : track.getDataRate();
}

Encoding OrphanDataCapableTrack::getEncoding() const
{
    assert(!empty());

    return track.empty() ? orphanDataTrack.getEncoding() : track.getEncoding();
}

int OrphanDataCapableTrack::getTimeOfOffset(const int offset) const
{
    assert(!empty());

    return track.empty() ? orphanDataTrack.getTimeOfOffset(offset) : track.getTimeOfOffset(offset); // microsec
}

int OrphanDataCapableTrack::getOffsetOfTime(const int time) const
{
    assert(!track.empty());

    return track.empty() ? orphanDataTrack.getOffsetOfTime(time) : track.getOffsetOfTime(time); // mfmbits (rawbits)
}

int OrphanDataCapableTrack::getTrackLen() const
{
    assert(track.tracklen == orphanDataTrack.tracklen);

    return track.tracklen;
}

// The tracklen can be the usual length of one rotation but also the length of
// decoded read bytes during multi rotation.
void OrphanDataCapableTrack::setTrackLen(const int trackLen)
{
    track.setTrackLen(trackLen);
    orphanDataTrack.setTrackLen(trackLen);
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
    track.setTrackTime(trackTime);
    orphanDataTrack.setTrackTime(trackTime);
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
     * The goal is defining a scoring which values good ids,
     * data connected to id (not orphan), the more good data copy,
     * good data copy read count (up to stability), the more (bad) data.
     * Good id: 1 point.
     * Good id's data exist: 2 point. (Id and data connection is valued as well.)
     * Each good data copy: read count (at most stability level) point.
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
                if (sector.has_good_data(!opt_normal_disk, opt_normal_disk))
                {
                    for (auto i = 0; i < iSup; i++)
                        score += sector.GetGoodDataCopyStabilityScore(i);
                }
            }
        }
    score += orphanDataTrack.size();
    return score;
}

// Merge the orphans track into parents track. An orphan can be merged if a coherent parent is found.
/*static*/ void OrphanDataCapableTrack::MergeOrphansIntoParents(Track& orphansTrack,
    Track& parentsTrack, bool removeOrphanAfterMerge,
    const std::function<bool (const Sector &)>& considerParentSectorPredicate/* = nullptr*/)
{
    assert(orphansTrack.tracklen == parentsTrack.tracklen);
    if (orphansTrack.empty() || parentsTrack.empty())
        return;

    // An orphan data and a parent sector are matched if they cohere and there is no closer to one of them that also coheres.
    const auto trackLen = parentsTrack.tracklen;
    const auto itParentBegin = parentsTrack.begin();
    const auto itParentEnd = parentsTrack.end(); // The parents end is constant since merging sector data to parent does not change the parents.
    auto itOrphan = orphansTrack.begin();
    while (itOrphan != orphansTrack.end()) // The orphans end is variable since possibly erasing sector from orphans.
    {
        auto& orphanDataSector = *itOrphan;
        const auto parentOffsetInterval = orphanDataSector.GetOffsetIntervalSuitableForParent(trackLen);
        auto itParent = itParentBegin;
        for (; itParent != itParentEnd; itParent++)
        {
            auto& parentSector = *itParent;
            if (considerParentSectorPredicate && !considerParentSectorPredicate(parentSector))
                continue;
            if (parentOffsetInterval.IsRingedWithin(parentSector.offset))
            {
                const auto itParentNext = itParent + 1;
                if (itParentNext != itParentEnd && parentOffsetInterval.IsRingedWithin(itParentNext->offset))
                    continue;
                if (opt_debug)
                    util::cout << "MergeOrphansIntoParents: adding orphan data sector (offset=" << orphanDataSector.offset
                    << ", id.sector=" << orphanDataSector.header.sector << ") to parent (offset=" << parentSector.offset
                    << ", id.sector=" << parentSector.header.sector << ")\n";
                if (removeOrphanAfterMerge)
                {
                    parentSector.MergeOrphanDataSector(std::move(orphanDataSector));
                    itOrphan = orphansTrack.sectors().erase(itOrphan);
                }
                else
                {
                    parentSector.MergeOrphanDataSector(orphanDataSector);
                    itOrphan++;
                }
                break;
            }
        }
        if (itParent == itParentEnd)
            itOrphan++;
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
        if (BitOffsetAsDataBytePosition(orphanDataCapableTrack.orphanDataTrack.tracklen - sector.offset, orphanDataCapableTrack.getEncoding()) == sector.data_size())
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
    if (syncOffset < 0 || offsetMin - syncOffset >= Sector::OFFSET_ALMOST_0) // Offset must not be almost 0.
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

void OrphanDataCapableTrack::join()
{
    assert(track.tracklen == orphanDataTrack.tracklen);

    if (!orphanDataTrack.sectors().empty())
    {
        track.sectors().push_back(std::move(orphanDataTrack.sectors()));
        orphanDataTrack.sectors().clear();
        std::sort(track.begin(), track.end(), Sector::CompareByOffset);
    }
}

// The orphan data track must be empty.
void OrphanDataCapableTrack::disjoin()
{
    auto& trackSectors = track.sectors();
    auto& orphanSectors = orphanDataTrack.sectors();
    assert(orphanSectors.empty());
    for (auto it = trackSectors.begin(); it != trackSectors.end(); )
    {
        if (it->IsOrphan())
        {
            orphanSectors.push_back(std::move(*it));
            it = trackSectors.erase(it);
        }
        else
            it++;
    }
    orphanDataTrack.tracklen = track.tracklen;
    orphanDataTrack.tracktime = track.tracktime;
}

void ShowTrackLayout(const Track& multiTrack, const Track& demultidTrack)
{
    if (demultidTrack.empty())
        return;
    //    const auto& encoding = demultidTrack.getEncoding();
    const auto demultidTrackLen = demultidTrack.tracklen;
    const auto revs = multiTrack.tracklen / demultidTrackLen + 1;
    VectorX<RingedInt> revIndices;
    auto offsetMin = 0;
    auto indexMin = 0;
    util::cout << "            demultid   ";
    std::string line = " id offset d2next rv   ";;
    const auto multiISup = multiTrack.size();
    const auto& sector0 = demultidTrack[0];
    for (auto r = 0; r < revs; r++)
    {
        util::cout << "       rev " << std::setw(2) << r << "  ";
        line += "offset d2next  ";
        const int offsetMax = std::min(offsetMin + demultidTrackLen, multiTrack.tracklen);
        auto indexSup = indexMin;
        while (indexSup < multiISup && multiTrack[indexSup].offset < offsetMax)
            indexSup++;
        auto indexStart = indexMin;
        while (indexStart < indexSup)
        {
            const auto multiOffsetDemultid = modulo(multiTrack[indexStart].offset, static_cast<unsigned>(demultidTrackLen));
            if (are_offsets_tolerated_same(multiOffsetDemultid, sector0.offset, sector0.encoding, opt_byte_tolerance_of_time, demultidTrackLen)
                || multiOffsetDemultid > sector0.offset)
                break;
            indexStart++;
        };
        // If indexStart is not found then it will become indexMin in RingedInt.
        revIndices.push_back(RingedInt(indexStart, indexSup, indexMin));
        offsetMin = offsetMax;
        indexMin = indexSup;
    }
    util::cout << "\n";
    util::cout << line << "\n";
    std::string datLine;
    const auto demultidISup = demultidTrack.size();
    for (auto i = 0; i < demultidISup; i++)
    {
        const auto& sector = demultidTrack[i];
        if (sector.offset >= demultidTrackLen)
            int error = 1;
        RingedInt iNext(i, demultidISup);
        while (demultidTrack[(++iNext).Value()].IsOrphan() ^ sector.IsOrphan()) ;
        const auto d2next = demultidTrack[iNext.Value()].offset
            + (iNext <= i ? demultidTrackLen : 0) - sector.offset;
        util::cout << std::setw(3) << sector.header.sector << " "
            << std::setw(6) << sector.offset << " "
            << std::setw(6) << d2next << " "
            << std::setw(2) << sector.revolution << "   ";
        const auto sectorIsIdAndHasData = !sector.IsOrphan() && sector.has_data();
        if (sectorIsIdAndHasData)
        {
            std::ostringstream ss;
            ss << std::setw(3) << "dat" << " "
                << std::setw(6) << sector.offset + sector.gap3 << " " // TODO gap3
                << std::setw(6) << sector.gap3 << " "
                << std::setw(2) << sector.revolution << "   ";
            datLine = ss.str();
        }
        for (auto r = 0; r < revs; r++)
        {
            auto multiOffsetFound = false;
            const auto revIndex = revIndices[r];
            if (!revIndex.IsEmpty())
            {
                auto revIndex = revIndices[r].Value();
                const auto& multiSector = multiTrack[revIndex];
                const auto multiOffset = multiSector.offset;
                const auto multiOffsetDemultid = modulo(multiOffset, static_cast<unsigned>(demultidTrackLen));
                RingedInt iMultiNext(revIndex, multiISup);
                while (multiTrack[(++iMultiNext).Value()].IsOrphan() ^ multiSector.IsOrphan());
                const auto d2next = (iMultiNext <= revIndex ?
                    multiTrack.tracklen : multiTrack[iMultiNext.Value()].offset
                    ) - multiOffset;
                line = "";
                if (are_offsets_tolerated_same(multiOffsetDemultid, sector.offset, sector.encoding, opt_byte_tolerance_of_time, demultidTrackLen)
                    && (sector.header == multiSector.header))
                {
                    std::ostringstream ss;
                    ss << std::setw(6) << multiOffset << " " << std::setw(6) << d2next;
                    line = ss.str();
                    if (sectorIsIdAndHasData)
                    {
                        std::ostringstream ssd;
                        ssd << std::setw(6) << multiOffset + multiSector.gap3 << " ";
                        if (multiSector.gap3 > 0)
                            ssd << std::setw(6) << multiSector.gap3;
                        else
                            ssd << "      ";
                        datLine += ssd.str();
                    }
                    revIndices[r]++;
                    multiOffsetFound = true;
                }
            }
            if (!multiOffsetFound)
            {
                std::ostringstream ss;
                ss << "      " << " " << "      ";
                line = ss.str();
                if (sectorIsIdAndHasData)
                    datLine += ss.str();
            }
            util::cout << line << "  ";
            if (sectorIsIdAndHasData)
                datLine += "  ";
        }
        util::cout << "\n";
        if (sectorIsIdAndHasData)
            util::cout << datLine << "\n";
    }
}
void OrphanDataCapableTrack::TuneOffsetsToEachOtherByMin(OrphanDataCapableTrack& otherOrphanDataCapableTrack)
{
    join();
    otherOrphanDataCapableTrack.join();
    track.TuneOffsetsToEachOtherByMin(otherOrphanDataCapableTrack.track);
    otherOrphanDataCapableTrack.disjoin();
    disjoin();
}

// This track must be single rev. See Track::syncAndDemultiThisTrackToOffset 2)a)
void OrphanDataCapableTrack::syncUnlimitedToOffset(const int syncOffset)
{
    join();
    track.syncUnlimitedToOffset(syncOffset);
    disjoin();
}

// This track must be single rev. See Track::syncAndDemultiThisTrackToOffset 3)a)
void OrphanDataCapableTrack::syncLimitedToOffset(const int syncOffset)
{
    join();
    track.syncLimitedToOffset(syncOffset);
    disjoin();
}

// This track must be multi rev. See Track::syncAndDemultiThisTrackToOffset 2)b), 3)b)
void OrphanDataCapableTrack::demultiAndSyncUnlimitedToOffset(const int syncOffset, const int trackLenSingle)
{
    join();
    track.demultiAndSyncUnlimitedToOffset(syncOffset, trackLenSingle);
    disjoin();
}

int OrphanDataCapableTrack::determineBestTrackLen(const int timedTrackLen) const
{
    assert(timedTrackLen > 0);

    return track.determineBestTrackLen(timedTrackLen);
}

void OrphanDataCapableTrack::FixOffsetsByTimedToAvoidRepeatedSectorWhenMerging(Track& timedTrack, const RepeatedSectors& repeatedSectorIds)
{
    if (!opt_normal_disk) // Not normal disk can have repeated sectors.
        return;
    assert(track.tracklen == timedTrack.tracklen);

    const auto iSup = track.size();
    const auto iTimedSup = timedTrack.size();
    if (iSup == 0 || iTimedSup == 0) // One of the two tracks is empty.
        return;
    RingedInt iTimedNext(0, iTimedSup);
    for (auto i = 0; i < iSup; i++)
    {
        const auto& sector = track[i];
        const auto itTimed = timedTrack.find(sector.header);
        if (itTimed == timedTrack.end())
            continue;
        if (!itTimed->is_sector_tolerated_same(sector, opt_byte_tolerance_of_time, timedTrack.tracklen)
            || iTimedSup < 2)
        {
            if (!timedTrack.DetermineOffsetDistanceMinMaxAverage(repeatedSectorIds))
                MessageCPP(msgWarningAlways, "Can not fix offsets (", itTimed->offset,
                    ", ", sector.offset, ") of sector (", sector,
                    ") to avoid repeating because can not determine id offset distance");
            else
            {
                const auto iTimedStart = itTimed - timedTrack.begin();
                iTimedNext = iTimedStart;
                while (timedTrack[(++iTimedNext).Value()].IsOrphan()); // Safety orphan check, in theory timed track can not have orphan.
                if (iTimedNext == iTimedStart) // Safety index check, in theory this condition is always false (due to DetermineOffsetDistanceMinMaxAverage).
                    continue; // There is only 1 not orphan.
                const auto& timedNextSector = timedTrack[iTimedNext.Value()];
                const auto timedWrappedOffsetDiff = itTimed->OffsetDistanceFromThisTo(timedNextSector, timedTrack.tracklen);
                const auto dataWrappedOffsetDiff = sector.OffsetDistanceFromThisTo(timedNextSector, timedTrack.tracklen);
                // The range between timed sector and next timed sector is examined.
                const int betweenSectors = round_AS<int>(timedWrappedOffsetDiff / timedTrack.idOffsetDistanceInfo.offsetDistanceAverage);
                const auto predictedOffsetDistance = round_AS<int>(betweenSectors * timedTrack.idOffsetDistanceInfo.offsetDistanceAverage);
                // The offset closer to predicted wins so the other offset is adjusted to winner offset.
                if (std::abs(timedWrappedOffsetDiff - predictedOffsetDistance) < std::abs(dataWrappedOffsetDiff - predictedOffsetDistance))
                {
                    util::cout << "FixOffsetsByTimedToAvoidRepeatedSectorWhenMerging: Modified this sector ("
                        << sector << ") offset (" << sector.offset << ") to " << itTimed->offset << "\n";
                    i = std::min(i - 1, track.SetSectorOffsetAt(i, itTimed->offset));
                }
                else
                {
                    util::cout << "FixOffsetsByTimedToAvoidRepeatedSectorWhenMerging: Modified timed sector ("
                        << *itTimed << ") offset (" << itTimed->offset << ") to " << sector.offset << "\n";
                    timedTrack.SetSectorOffsetAt(iTimedStart, sector.offset);
                }
            }
        }
    }
}

void OrphanDataCapableTrack::ShowOffsets() const
{
    track.ShowOffsets();
    orphanDataTrack.ShowOffsets();
}
