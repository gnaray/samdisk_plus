#ifndef ORPHANDATACAPABLETRACK_H
#define ORPHANDATACAPABLETRACK_H
//---------------------------------------------------------------------------

class OrphanDataCapableTrack; // Required because this and PhysicalTrackMFM includes each other.

#include "PhysicalTrackMFM.h"
#include "Track.h"

class OrphanDataCapableTrack
{
public:
    bool empty() const;
    DataRate getDataRate() const;
    Encoding getEncoding() const;
    int getTimeOfOffset(const int offset) const;
    int getOffsetOfTime(const int time) const;
    int getTrackLen() const;
    void setTrackLen(const int trackLen);
    void addTrackLen(const int trackLen);
    int getTrackTime() const;
    void setTrackTime(const int trackTime);
    void addTrackTime(const int trackTime);
    void add(OrphanDataCapableTrack&& orphanDataCapableTrack);
    int size() const;

    static void MergeOrphansIntoParents(Track& orphansTrack, Track& parentsTrack, bool removeOrphanAfterMerge, const std::function<bool (const Sector &)>& considerParentSectorPredicate = nullptr);

    void MergeOrphansIntoParents(bool removeOrphanAfterMerge);
    void AddWithSyncAndBrokenEndingFix(OrphanDataCapableTrack&& orphanDataCapableTrack);
    void MergeInto(Track& targetTrack, const std::function<bool (const Sector &)>& considerTargetSectorPredicate = nullptr);
    void MergePhysicalTrack(const CylHead& cylhead, const PhysicalTrackMFM& toBeMergedPhysicalTrack);
    void MergeUnsyncedBrokenEndingTrack(OrphanDataCapableTrack&& toBeMergedODCTrack);
    void syncThisToOtherAsMulti(const int offsetDiffBest, OrphanDataCapableTrack& targetODCTrack);
    void syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle, bool syncOnly);
    int determineBestTrackLen(const int timedTrackTime) const;

    static constexpr int ORPHAN_SECTOR_ID = 256;
    Track track{};
    Track orphanDataTrack{};
    bool cylheadMismatch = false;
    int trackIndexOffset = 0;
};

//---------------------------------------------------------------------------
#endif // ORPHANDATACAPABLETRACK_H
