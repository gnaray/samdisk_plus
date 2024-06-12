#pragma once

#include "Sector.h"
#include "Format.h"

#include <map>

struct SectorIndexWithSectorIdAndOffset
{
    int sectorIndex = -1;
    int sectorId = -1;
    int offset = -1;
};

class RepeatedSectors : public std::map<int, VectorX<int>>
{
public:
    std::shared_ptr<const VectorX<int>> FindOffsetsById(const int sectorId) const;
    std::shared_ptr<int> FindToleratedOffsetsById(const int sectorId, const int offset,
        const Encoding& encoding, const int byte_tolerance_of_time, const int trackLen) const;
};

struct IdOffsetDistanceInfo
{
    double offsetDistanceMin = 0;
    double offsetDistanceAverage = 0;
    double offsetDistanceMax = 0;
    std::set<int> ignoredIds{};
    std::set<int> notAverageFarFromNextIds{};

    inline bool IsEmpty() const
    {
        return offsetDistanceAverage == 0;
    }

    inline void Reset()
    {
        *this = std::move(IdOffsetDistanceInfo());
    }
};

class Track
{
public:
    enum class AddResult { Unchanged, Append, Insert, Merge };
    enum class SyncMode { None, RevolutionLimited, Unlimited };

public:
    explicit Track(int sectors = 0);    // sectors to reserve
    Track CopyWithoutSectorData() const;

    bool empty() const;
    int size() const;
    const Sectors& sectors() const;
    Sectors& sectors();
    const Sectors& sectors_view_ordered_by_id() const;
    const Sector& operator [] (int index) const;
    Sector& operator [] (int index);
    int index_of(const Sector& sector) const;

    int data_extent_bits(const Sector& sector) const;
    int data_extent_bytes(const Sector& sector) const;
    bool data_overlap(const Sector& sector) const;
    bool is_mixed_encoding() const;
    bool is_8k_sector() const;
    bool is_repeated(const Sector& sector) const;
    bool has_all_good_data() const;
    bool has_any_good_data() const;

    const UniqueSectors good_idcrc_sectors() const;
    const Sectors good_sectors() const;
    const UniqueSectors stable_sectors() const;
    bool has_all_stable_data(const UniqueSectors& stable_sectors) const;
    int normal_probable_size() const;

    void clear();
    void add(Track&& track);
    void add(Sectors&& sectors, const std::function<bool (const Sector &)>& sectorFilterPredicate = nullptr);
    AddResult merge(Sector&& sector, const Sectors::iterator& it);
    AddResult add(Sector&& sector, int* affectedSectorIndex = nullptr, bool dryrun = false);
    void insert(int index, Sector&& sector);
    Sector remove(int index);
    const Sector& get_sector(const Header& header) const;

    DataRate getDataRate() const;
    Encoding getEncoding() const;
    int getTimeOfOffset(const int offset) const;
    int getOffsetOfTime(const int time) const;
    void setTrackLen(const int trackLen);
    void setTrackTime(const int trackTime);
    inline int OffsetDistanceFromTo(const int indexFrom, const int indexTo) const
    {
        assert(tracklen > 0);
        return m_sectors[indexFrom].OffsetDistanceFromThisTo(m_sectors[indexTo], tracklen);
    }

    static int findMostPopularToleratedDiff(VectorX<int>& diffs, const Encoding& encoding);
    std::map<int, int> FindMatchingSectors(const Track& otherTrack, const RepeatedSectors& repeatedSectorIds) const;
    bool DiscoverTrackSectorScheme(const RepeatedSectors& repeatedSectorIds);
    void ShowOffsets() const;
    bool DetermineOffsetDistanceMinMaxAverage(const RepeatedSectors& repeatedSectorIds);
    void CollectRepeatedSectorIdsInto(RepeatedSectors& repeatedSectorIds) const;
    void MergeByAvoidingRepeatedSectors(Track&& track);
    void MergeRepeatedSectors();
    SectorIndexWithSectorIdAndOffset FindSectorDetectingInvisibleFirstSector(const RepeatedSectors& repeatedSectorIds);
    SectorIndexWithSectorIdAndOffset FindCloseSectorPrecedingUnreadableFirstSector();
    void MakeVisibleFirstSector(const SectorIndexWithSectorIdAndOffset& sectorIndexAndSectorId);
    void AdjustSuspiciousOffsets(const RepeatedSectors& repeatedSectorIds, const bool redetermineOffsetDistance = false, const bool useOffsetDistanceBalancer = false);
    void Validate(const RepeatedSectors& repeatedSectorIds = RepeatedSectors()) const;
    void DropSectorsFromNeighborCyls(const CylHead& cylHead, const int trackSup);
    void EnsureNotAlmost0Offset();

    void TuneOffsetsToEachOtherByMin(Track& otherTrack);
    int SetSectorOffsetAt(const int index, const int offset);
    bool MakeOffsetsNot0(const bool warn = true);
    void syncUnlimitedToOffset(const int syncOffset);
    void syncLimitedToOffset(const int syncOffset);
    void demultiAndSyncUnlimitedToOffset(const int syncOffset, const int trackLenSingle);
    void demultiAndSyncLimitedToOffset(const int syncOffset, const int trackLenSingle);
protected:
    void syncAndDemultiThisTrackToOffset(const int syncOffset, bool demulti, const SyncMode& syncMode, int trackLenSingle = 0);

public:
    void AnalyseMultiTrack(const int trackLenBest) const;
    int determineBestTrackLen(const int timedTrackLen) const;
    int findReasonableIdOffsetForDataFmOrMfm(const int dataOffset) const;

    Track& format(const CylHead& cylhead, const Format& format);
    Data::const_iterator populate(Data::const_iterator it, Data::const_iterator itEnd, const bool signIncompleteData = false);

    Sectors::reverse_iterator rbegin() { return m_sectors.rbegin(); }
    Sectors::reverse_iterator rend() { return m_sectors.rend(); }
    Sectors::iterator begin() { return m_sectors.begin(); }
    Sectors::iterator end() { return m_sectors.end(); }
    Sectors::iterator find(const Sector& sector)
    {
        auto it = static_cast<const Track&>(*this).find(sector);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator find(const Header& header)
    {
        auto it = static_cast<const Track&>(*this).find(header);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator findNext(const Header& header, const Sectors::iterator& itPrev)
    {
        auto it = static_cast<const Track&>(*this).findNext(header, itPrev);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator find(const Header& header, const int offset)
    {
        auto it = static_cast<const Track&>(*this).find(header, offset);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator findToleratedSame(const Header& header, const int offset, int tracklen)
    {
        auto it = static_cast<const Track&>(*this).findToleratedSame(header, offset, tracklen);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator findFirstFromOffset(const int offset)
    {
        auto it = static_cast<const Track&>(*this).findFirstFromOffset(offset);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator findIgnoringSize(const Header& header)
    {
        auto it = static_cast<const Track&>(*this).findIgnoringSize(header);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator find(const Header& header, const DataRate datarate, const Encoding encoding)
    {
        auto it = static_cast<const Track&>(*this).find(header, datarate, encoding);
        return m_sectors.erase(it, it);
    }
    inline Sectors::iterator findSectorForDataFmOrMfm(const int dataOffset, const int sizeCode, bool findClosest = true)
    {
        auto it = static_cast<const Track&>(*this).findSectorForDataFmOrMfm(dataOffset, sizeCode, findClosest);
        return m_sectors.erase(it, it);
    }

    Sectors::const_reverse_iterator rbegin() const { return m_sectors.rbegin(); }
    Sectors::const_reverse_iterator rend() const { return m_sectors.rend(); }
    Sectors::const_iterator begin() const { return m_sectors.begin(); }
    Sectors::const_iterator end() const { return m_sectors.end(); }
    Sectors::const_iterator find(const Sector& sector) const;
    Sectors::const_iterator find(const Header& header) const;
    Sectors::const_iterator findNext(const Header& header, const Sectors::const_iterator& itPrev) const;
    Sectors::const_iterator find(const Header& header, const int offset) const;
    Sectors::const_iterator findToleratedSame(const Header& header, const int offset, int tracklen) const;
    Sectors::const_iterator findFirstFromOffset(const int offset) const;
    Sectors::const_iterator findIgnoringSize(const Header& header) const;
    Sectors::const_iterator find(const Header& header, const DataRate datarate, const Encoding encoding) const;
    Sectors::const_iterator findSectorForDataFmOrMfm(const int dataOffset, const int sizeCode, bool findClosest = true) const;

    int tracklen = 0;   // track length in MFM bits
    int tracktime = 0;  // track time in us
    IdAndOffsetPairs idAndOffsetPairs{};
    IdOffsetDistanceInfo idOffsetDistanceInfo{};

private:
    Sectors m_sectors{};
    mutable Sectors m_sectors_view_ordered_by_id{};

public:
    // Max bitstream position difference for sectors to be considered the same.
    // Used to match sectors between revolutions, and needs to cope with the
    // larger sync differences after weak sectors. We still require the header
    // to match, so only close repeated headers should be a problem.
    static constexpr int COMPARE_TOLERANCE_BYTES = 64;
};
