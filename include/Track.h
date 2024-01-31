#pragma once

#include "Sector.h"
#include "Format.h"

class Track
{
public:
    enum class AddResult { Unchanged, Append, Insert, Merge };

public:
    explicit Track(int sectors = 0);    // sectors to reserve

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

    const Sectors good_idcrc_sectors() const;
    const Sectors good_sectors() const;
    const Sectors stable_sectors() const;
    bool has_all_stable_data(const Sectors &stable_sectors) const;
    int normal_probable_size() const;

    void clear();
    void add(Track&& track);
    void add(Sectors&& sectors);
    AddResult add(Sector&& sector, int* mergedSectorIndex = nullptr, bool dryrun = false);
    void insert(int index, Sector&& sector);
    Sector remove(int index);
    const Sector& get_sector(const Header& header) const;

    DataRate getDataRate() const;
    Encoding getEncoding() const;
    int getTimeOfOffset(const int offset) const;
    int getOffsetOfTime(const int time) const;

    static int findMostPopularToleratedDiff(VectorX<int>& diffs);

    bool findSyncOffsetComparedTo(const Track& referenceTrack, int& syncOffset) const;
    void syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle, bool ignoreEndingDataCopy = false);
    int determineBestTrackLen(const int timedTrackLen) const;
    void setTrackLenAndNormaliseTrackTimeAndSectorOffsets(const int trackLen);
    int findReasonableIdOffsetForDataFmOrMfm(const int dataOffset) const;
    IdAndOffsetVector DiscoverTrackSectorScheme() const;

    Track& format(const CylHead& cylhead, const Format& format);
    Data::const_iterator populate(Data::const_iterator it, Data::const_iterator itEnd);

    Sectors::reverse_iterator rbegin() { return m_sectors.rbegin(); }
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
    inline Sectors::iterator findForDataFmOrMfm(const int dataOffset, const int sizeCode)
    {
        auto it = static_cast<const Track&>(*this).findForDataFmOrMfm(dataOffset, sizeCode);
        return m_sectors.erase(it, it);
    }
    Sectors::iterator findDataForSectorIdFmOrMfm(const int sectorIdOffset, const int sizeCode)
    {
        auto it = static_cast<const Track&>(*this).findDataForSectorIdFmOrMfm(sectorIdOffset, sizeCode);
        return m_sectors.erase(it, it);
    }

    Sectors::const_reverse_iterator rbegin() const { return m_sectors.rbegin(); }
    Sectors::const_iterator begin() const { return m_sectors.begin(); }
    Sectors::const_iterator end() const { return m_sectors.end(); }
    Sectors::const_iterator find(const Sector& sector) const;
    Sectors::const_iterator find(const Header& header) const;
    Sectors::const_iterator findNext(const Header& header, const Sectors::const_iterator& itPrev) const;
    Sectors::const_iterator findFirstFromOffset(const int offset) const;
    Sectors::const_iterator findIgnoringSize(const Header& header) const;
    Sectors::const_iterator find(const Header& header, const DataRate datarate, const Encoding encoding) const;
    Sectors::const_iterator findForDataFmOrMfm(const int dataOffset, const int sizeCode) const;
    Sectors::const_iterator findDataForSectorIdFmOrMfm(const int sectorIdOffset, const int sizeCode) const;

    int tracklen = 0;   // track length in MFM bits
    int tracktime = 0;  // track time in us

private:
    Sectors m_sectors{};
    mutable Sectors m_sectors_view_ordered_by_id{};

public:
    // Max bitstream position difference for sectors to be considered the same.
    // Used to match sectors between revolutions, and needs to cope with the
    // larger sync differences after weak sectors. We still require the header
    // to match, so only close repeated headers should be a problem.
    static constexpr int COMPARE_TOLERANCE_BYTES = 64;
    static constexpr int COMPARE_TOLERANCE_BITS = DataBytePositionAsBitOffset(COMPARE_TOLERANCE_BYTES); // mfmbits (halfbits)
};
