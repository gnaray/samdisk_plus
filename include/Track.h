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
    const std::vector<Sector>& sectors() const;
    std::vector<Sector>& sectors();
    const std::vector<Sector>& sectors_view_ordered_by_id() const;
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

    const Sectors good_sectors() const;
    const Sectors stable_sectors() const;
    bool has_all_stable_data(const Sectors &stable_sectors) const;
    int normal_probable_size() const;

    void clear();
    void add(Track&& track);
    AddResult add(Sector&& sector);
    void insert(int index, Sector&& sector);
    Sector remove(int index);
    const Sector& get_sector(const Header& header) const;

    DataRate getDataRate() const;
    Encoding getEncoding() const;
    int getTimeOfOffset(const int offset) const;
    int getOffsetOfTime(const int time) const;
    void syncAndDemultiThisTrackToOffset(const int syncOffset, const int trackLenSingle);

    Track& format(const CylHead& cylhead, const Format& format);
    Data::const_iterator populate(Data::const_iterator it, Data::const_iterator itEnd);

    std::vector<Sector>::reverse_iterator rbegin() { return m_sectors.rbegin(); }
    std::vector<Sector>::iterator begin() { return m_sectors.begin(); }
    std::vector<Sector>::iterator end() { return m_sectors.end(); }
    std::vector<Sector>::iterator find(const Sector& sector);
    std::vector<Sector>::iterator find(const Header& header);
    std::vector<Sector>::iterator findNext(const Header& header, const std::vector<Sector>::iterator& itPrev);
    std::vector<Sector>::iterator findFirstFromOffset(const int offset);
    std::vector<Sector>::iterator findIgnoringSize(const Header& header);
    std::vector<Sector>::iterator find(const Header& header, const DataRate datarate, const Encoding encoding);

    std::vector<Sector>::const_reverse_iterator rbegin() const { return m_sectors.rbegin(); }
    std::vector<Sector>::const_iterator begin() const { return m_sectors.begin(); }
    std::vector<Sector>::const_iterator end() const { return m_sectors.end(); }
    std::vector<Sector>::const_iterator find(const Sector& sector) const;
    std::vector<Sector>::const_iterator find(const Header& header) const;
    std::vector<Sector>::const_iterator findNext(const Header& header, const std::vector<Sector>::const_iterator& itPrev) const;
    std::vector<Sector>::const_iterator findFirstFromOffset(const int offset) const;
    std::vector<Sector>::const_iterator findIgnoringSize(const Header& header) const;
    std::vector<Sector>::const_iterator find(const Header& header, const DataRate datarate, const Encoding encoding) const;

    int tracklen = 0;   // track length in MFM bits
    int tracktime = 0;  // track time in us

private:
    std::vector<Sector> m_sectors{};
    mutable std::vector<Sector> m_sectors_view_ordered_by_id{};

public:
    // Max bitstream position difference for sectors to be considered the same.
    // Used to match sectors between revolutions, and needs to cope with the
    // larger sync differences after weak sectors. We still require the header
    // to match, so only close repeated headers should be a problem.
    static constexpr int COMPARE_TOLERANCE_BYTES = 64;
    static constexpr int COMPARE_TOLERANCE_BITS = COMPARE_TOLERANCE_BYTES * 16; // mfmbits (halfbits)
};
