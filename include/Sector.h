#pragma once

#include "Header.h"

class Data : public std::vector<uint8_t>
{
public:
    using std::vector<uint8_t>::vector;
    int size() const { return static_cast<int>(std::vector<uint8_t>::size()); }
};

using DataList = std::vector<Data>;


class Sector
{
public:
    enum class Merge { Unchanged, Improved, NewData };

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
    void limit_copies(int max_copies);

    int size() const;
    int data_size() const;

    const DataList& datas() const;
    DataList& datas();
    const Data& data_copy(int copy = 0) const;
    Data& data_copy(int copy = 0);
    int get_best_data_index() const;

    Merge add(Data&& data, bool bad_crc = false, uint8_t dam = 0xfb);
    bool is_constant_disk() const;
    void set_constant_disk(bool constant_disk);
    int copies() const;

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
    bool m_constant_disk = true; // If this sector is part of disk image then true, else it comes from physical device so false.
};

class Sectors : public std::vector<Sector>
{
public:
    Sectors() = default;

    bool has_id_sequence(const int first_id, const int up_to_id) const;
    Headers Sectors::headers() const;
};
