#include "TimedAndPhysicalDualTrack.h"
#include "Options.h"

static auto& opt_byte_tolerance_of_time = getOpt<int>("byte_tolerance_of_time");
static auto& opt_debug = getOpt<int>("debug");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");

inline int IndexInDirection(const int index, const int indexSup, const bool forward)
{
    assert(indexSup > 0);
    return forward ? index : indexSup - 1 - index;
}

// The to be merged track must be multi track, it can contain orphan track.
bool TimedAndPhysicalDualTrack::SyncDemultiMergePhysicalUsingTimed(
    OrphanDataCapableTrack&& toBeMergedODCTrack, const RepeatedSectors& repeatedSectorIds)
{
    const int trackLenAbout = timedIdDataAndPhysicalIdTrack.tracklen;
    assert(trackLenAbout > 0);
    if (toBeMergedODCTrack.empty())
        return false;

    const auto& MIDTrack = toBeMergedODCTrack.track; // MID = multiIdData
    const auto& MIDOrphanDataTrack = toBeMergedODCTrack.orphanDataTrack;
    auto& referenceTrack = timedIdDataAndPhysicalIdTrack;
    if (referenceTrack.empty() || MIDTrack.empty())
        return false;
    const auto& encoding = MIDTrack.getEncoding();
    assert(referenceTrack.getDataRate() == MIDTrack.getDataRate());
    assert(referenceTrack.getEncoding() == encoding);

    if (opt_debug >= 2)
    {
        util::cout << "SyncDemultiMergePhysicalUsingTimed: showing toBeMergedODCTrack offsets before processing it:\n"
            << toBeMergedODCTrack.ToString(false) << "\n";

    }

    const auto lastSectorIndex = MIDOrphanDataTrack.size() - 1;
    if (lastSectorIndex >= 0) // Not empty.
    {
        const auto& MIDOrphanDataSector = MIDOrphanDataTrack[lastSectorIndex];
        // Does the last orphan data MIDOrphanDataSector end at track end? If yes then we ignore it since it can be broken.
        if (BitOffsetAsDataBytePosition(MIDOrphanDataTrack.tracklen
            - MIDOrphanDataSector.offset, encoding) == MIDOrphanDataSector.data_size())
        {
            if (opt_debug)
                util::cout << "SyncDemultiMergePhysicalUsingTimed: ignoring possibly broken orphan data sector at track end (offset=" << MIDOrphanDataSector.offset << ", id.sector=" << MIDOrphanDataSector.header.sector << ")\n";
            toBeMergedODCTrack.orphanDataTrack.remove(lastSectorIndex);
        }
    }

    const auto timedIdOffsetDistanceExist = referenceTrack.DetermineOffsetDistanceMinMaxAverage(repeatedSectorIds);
    constexpr auto acceptedSectorIndexDistanceMax = 3;
    // Find all pairs of matching sectors, those will be the bases.
    const auto sectorBasePairs = MIDTrack.FindMatchingSectors(referenceTrack, repeatedSectorIds);

    auto lastPhysicalTrackSingleLocal = lastPhysicalTrackSingle;
    lastPhysicalTrackSingleLocal.setTrackLen(trackLenAbout);
    if (opt_debug >= 2)
        util::cout << "SyncDemultiMergePhysicalUsingTimed: tracklenAbout = " << trackLenAbout << "\n";
    std::set<int> MIDMergedSectorIndices;
    std::set<int> MIDMergedOrphanSectorIndices;
    const auto iMIDSup = MIDTrack.size();
    const auto iMIDOrphanSup = MIDOrphanDataTrack.size();
    auto lookForward = true;
    do { // Do two rounds. First round is looking forward, second round is looking backward.
        auto iMIDSectorBase = -1;
        auto iMIDSectorBaseNext = iMIDSup;
        for (auto iMID = 0; iMID < iMIDSup; iMID++)
        {
            if (sectorBasePairs.find(IndexInDirection(iMID, iMIDSup, lookForward)) != sectorBasePairs.end())
            {
                iMIDSectorBase = iMID;
                break;
            }
        }
        if (iMIDSectorBase < 0) // Can not find a base.
            return false;
        auto syncOffset = 0;
        for (auto iMIDOrphan = 0, iMID = 0; iMID < iMIDSup; iMID++)
        {
            const auto& MIDSector = MIDTrack[IndexInDirection(iMID, iMIDSup, lookForward)];
            if (iMID == iMIDSectorBase || iMID == iMIDSectorBaseNext)
            {
                if (iMID == iMIDSectorBaseNext)
                {   // Sync demulti the orphans between previous (iMIDSectorBase) and current (iMIDSectorBaseNext) sectors.
                    const auto& MIDSectorBase = MIDTrack[IndexInDirection(iMIDSectorBase, iMIDSup, lookForward)];
                    const auto searchOrphanOffsetSup = iMIDOrphan < iMIDOrphanSup && iMIDSectorBaseNext < iMIDSup
                        ? MIDTrack[IndexInDirection(iMIDSectorBaseNext, iMIDSup, lookForward)].offset
                        : (lookForward ? MIDOrphanDataTrack.tracklen : -1);
                    for (; iMIDOrphan < iMIDOrphanSup; iMIDOrphan++)
                    {
                        const auto& MIDOrphanSector = MIDOrphanDataTrack[IndexInDirection(iMIDOrphan, iMIDOrphanSup, lookForward)];
                        if (MIDMergedOrphanSectorIndices.find(IndexInDirection(iMIDOrphan, iMIDOrphanSup, lookForward)) != MIDMergedOrphanSectorIndices.cend())
                            continue;
                        if ((lookForward && MIDOrphanSector.offset < MIDSectorBase.offset)
                            || (!lookForward && MIDOrphanSector.offset > MIDSectorBase.offset))
                        {
                            if (opt_debug >= 2)
                                util::cout << "SyncDemultiMergePhysicalUsingTimed: ignoring orphan sector at offset ("
                                << MIDOrphanSector.offset << ") without preceding base (>" << acceptedSectorIndexDistanceMax << ")\n";
                            continue;
                        }
                        if ((lookForward && MIDOrphanSector.offset >= searchOrphanOffsetSup)
                            || (!lookForward && MIDOrphanSector.offset <= searchOrphanOffsetSup))
                            break;
                        const auto offsetDistanceAverage = timedIdOffsetDistanceExist
                            ? referenceTrack.idOffsetDistanceInfo.offsetDistanceAverage
                            : GetFmOrMfmSectorOverhead(MIDOrphanSector.datarate, MIDSectorBase.encoding,
                                MIDSectorBase.size()); // Using MIDSectorBase because MIDOrphanSector has no size.
                        const auto MIDOrphanSectorAndBaseDistance = lookForward ? MIDOrphanSector.offset - MIDSectorBase.offset
                            : MIDSectorBase.offset - MIDOrphanSector.offset;
                        if (round_AS<int>(MIDOrphanSectorAndBaseDistance / offsetDistanceAverage) > acceptedSectorIndexDistanceMax)
                            break;
                        auto MIDOrphanSectorCopy = MIDOrphanSector;
                        MIDOrphanSectorCopy.revolution = MIDOrphanSectorCopy.offset / trackLenAbout;
                        MIDOrphanSectorCopy.offset = modulo(MIDOrphanSectorCopy.offset - syncOffset, trackLenAbout);
                        if (MIDOrphanSectorCopy.offset == 0)
                            MIDOrphanSectorCopy.offset = 1;
                        if (opt_debug >= 2)
                            util::cout << "SyncDemultiMergePhysicalUsingTimed: synced orphan sector from "
                            << MIDOrphanSector.offset << " to " << MIDOrphanSectorCopy.offset << ")\n";
                        lastPhysicalTrackSingleLocal.orphanDataTrack.add(std::move(MIDOrphanSectorCopy));
                        MIDMergedOrphanSectorIndices.emplace(IndexInDirection(iMIDOrphan, iMIDOrphanSup, lookForward));
                    }
                    iMIDSectorBase = iMIDSectorBaseNext;
                }
                // Find next base.
                if (iMIDSectorBaseNext == iMIDSup)
                    iMIDSectorBaseNext = iMIDSectorBase;
                for (++iMIDSectorBaseNext; iMIDSectorBaseNext < iMIDSup; iMIDSectorBaseNext++)
                    if (sectorBasePairs.find(IndexInDirection(iMIDSectorBaseNext, iMIDSup, lookForward)) != sectorBasePairs.end())
                        break;
                const auto& MIDSectorBase = MIDTrack[IndexInDirection(iMIDSectorBase, iMIDSup, lookForward)];
                syncOffset = MIDSectorBase.offset - referenceTrack[sectorBasePairs.at(IndexInDirection(iMIDSectorBase, iMIDSup, lookForward))].offset;
                if (opt_debug >= 2)
                    util::cout << "SyncDemultiMergePhysicalUsingTimed: sector ("
                    << MIDSectorBase << ") base at offset (" << MIDSectorBase.offset
                    << "), syncOffset (" << syncOffset << ")\n";
            }
            if (MIDMergedSectorIndices.find(IndexInDirection(iMID, iMIDSup, lookForward)) != MIDMergedSectorIndices.cend())
                continue;
            if (iMID < iMIDSectorBase)
            {
                if (opt_debug >= 2)
                    util::cout << "SyncDemultiMergePhysicalUsingTimed: ignoring sector ("
                        << MIDSector << ") at offset (" << MIDSector.offset
                        << ") without preceding base\n";
                continue;
            }
            const auto& MIDSectorBase = MIDTrack[IndexInDirection(iMIDSectorBase, iMIDSup, lookForward)];
            const auto offsetDistanceAverage = timedIdOffsetDistanceExist
                ? referenceTrack.idOffsetDistanceInfo.offsetDistanceAverage
                : GetFmOrMfmSectorOverhead(MIDSectorBase.datarate, MIDSectorBase.encoding,
                    MIDSectorBase.size());
            const auto MIDSectorAndBaseDistance = lookForward ? MIDSector.offset - MIDSectorBase.offset
                : MIDSectorBase.offset - MIDSector.offset;
            if (round_AS<int>(MIDSectorAndBaseDistance / offsetDistanceAverage) > acceptedSectorIndexDistanceMax)
            {
                if (opt_debug >= 2)
                    util::cout << "SyncDemultiMergePhysicalUsingTimed: ignoring sector ("
                        << MIDSector << ") at offset (" << MIDSector.offset
                        << ") too far from base (>" << acceptedSectorIndexDistanceMax << ")\n";
                continue;
            }
            auto MIDSectorCopy = MIDSector;
            MIDSectorCopy.revolution = MIDSectorCopy.offset / trackLenAbout;
            MIDSectorCopy.offset = modulo(MIDSectorCopy.offset - syncOffset, trackLenAbout);
            if (MIDSectorCopy.offset == 0)
                MIDSectorCopy.offset = 1;
            if (opt_debug >= 2)
                util::cout << "SyncDemultiMergePhysicalUsingTimed: synced sector ("
                << MIDSectorCopy << ") from " << MIDSector.offset << " to " << MIDSectorCopy.offset << ")\n";
            lastPhysicalTrackSingleLocal.track.add(std::move(MIDSectorCopy));
            MIDMergedSectorIndices.emplace(IndexInDirection(iMID, iMIDSup, lookForward));
        }
        lookForward = !lookForward;
    } while (!lookForward);
    lastPhysicalTrackSingleLocal.MergeOrphansIntoParents(true);

    lastPhysicalTrackSingleLocal.cylheadMismatch = toBeMergedODCTrack.cylheadMismatch;
    lastPhysicalTrackSingle = std::move(lastPhysicalTrackSingleLocal);

    const auto scoreNew = lastPhysicalTrackSingle.Score();
    const bool foundNewValuableSomething = scoreNew > lastPhysicalTrackSingleScore; // Found new valuable something.
    lastPhysicalTrackSingleScore = scoreNew;
    return foundNewValuableSomething;
}
