#pragma once

#include "Header.h"

class Data : public std::vector<uint8_t>
{
public:
    using std::vector<uint8_t>::vector;
    int size() const { return static_cast<int>(std::vector<uint8_t>::size()); }
};

using DataList = std::vector<Data>;


class Sector;

class DataReadStats
{
public:
    DataReadStats() = default;
    DataReadStats(int read_count)
        : m_read_count(read_count)
    {
    }

    int ReadCount() const
    {
        return m_read_count;
    }

    DataReadStats& operator +=(const DataReadStats& rhs)
    {
        m_read_count += rhs.m_read_count;
        return *this;
    }

    DataReadStats operator +(const DataReadStats& rhs) const
    {
        DataReadStats result = *this;
        return result += rhs;
    }

private:
    int m_read_count = 0; // Amount of reading (good or bad) data of the owner sector (provided only by not constant (real) disks).
};


class Sector
{
public:
    enum class Merge { Unchanged, Matched, Improved, NewData };

private:
    void process_merge_result(const Merge& ret, int new_read_attempts, const DataReadStats& new_data_read_stats,
        bool readstats_counter_mode, int affected_data_index, const DataReadStats& improved_data_read_stats);

public:
    Sector(DataRate datarate, Encoding encoding, const Header& header = Header(), int gap3 = 0);
    bool operator==(const Sector& sector) const;

    Merge merge(Sector&& sector);

    bool has_data() const;
    bool has_good_data() const;
    bool has_gapdata() const;
    bool has_shortdata() const;
    bool has_normaldata() const;
    bool has_badidcrc() const;
    bool has_baddatacrc() const;
    bool is_deleted() const;
    bool is_altdam() const;
    bool is_rx02dam() const;
    bool is_8k_sector() const;
    bool is_checksummable_8k_sector() const;

    void set_badidcrc(bool bad = true);
    void set_baddatacrc(bool bad = true);
    void erase_data(int instance);
    void resize_data(int count);
    void remove_data();
    void remove_gapdata(bool keep_crc = false);
    bool has_stable_data() const;
    bool are_copies_full(int max_copies) const;
    void limit_copies(int max_copies);

    int size() const;
    int data_size() const;

    const DataList& datas() const;
    DataList& datas();
    const Data& data_copy(int copy = 0) const;
    Data& data_copy(int copy = 0);
    const DataReadStats& Sector::data_copy_read_stats(int instance = 0) const;
    DataReadStats& Sector::data_copy_read_stats(int instance = 0);
    int get_best_data_index() const;
    int read_attempts() const;
    void set_read_attempts(int read_attempts);
    void add_read_attempts(int read_attempts);
    bool is_constant_disk() const;
    void set_constant_disk(bool constant_disk);
    void fix_readstats();

    Merge add(Data&& data, bool bad_crc = false, uint8_t dam = 0xfb, int* affected_data_index = nullptr, DataReadStats& improved_data_read_stats = DataReadStats());
    Merge add_with_readstats(Data&& new_data, bool new_bad_crc = false, uint8_t new_dam = 0xfb,
        int new_read_attempts = 1, const DataReadStats& new_data_read_stats = DataReadStats(1), bool readstats_counter_mode = true, bool update_this_read_attempts = true);
    int copies() const;
    void add_read_stats(int instance, DataReadStats&& data_read_stats);
    void set_read_stats(int instance, DataReadStats&& data_read_stats);

    static int SizeCodeToRealSizeCode(int size);
    static int SizeCodeToLength(int size);

public:
    Header header{ 0,0,0,0 };               // cyl, head, sector, size
    DataRate datarate = DataRate::Unknown;  // 250Kbps
    Encoding encoding = Encoding::Unknown;  // MFM
    int offset = 0;                         // bitstream offset from index, in bits
    int gap3 = 0;                           // inter-sector gap size
    uint8_t dam = 0xfb;                     // data address mark

private:
    bool m_bad_id_crc = false;
    bool m_bad_data_crc = false;
    std::vector<Data> m_data{};         // copies of sector data
    std::vector<DataReadStats> m_data_read_stats{}; // Readstats of copies of sector data.
    int m_read_attempts = 0; // Amount of reading data attempts of this sector (provided only by real disks).
    bool m_constant_disk = true; // If this sector is part of disk image then true, else it comes from physical device so false.
};

class Sectors : public std::vector<Sector>
{
public:
    Sectors() = default;

    bool has_id_sequence(const int first_id, const int up_to_id) const;
    Headers Sectors::headers() const;
};
