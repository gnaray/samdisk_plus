#pragma once

#include "BitBuffer.h"
#include "Disk.h"
#include "Header.h"
#include "Sector.h"
#include "Track.h"

#include <set>

constexpr int MAX_SIDES = 2;
constexpr int MAX_TRACKS = 128;         // Internal format maximum, needed for 1MB TRD
constexpr int MAX_SECTORS = 144;

constexpr int MIN_SECTOR_SIZE = 128;


struct ScanContext
{
    CylHead last_cylhead{};
    Sector sector{ DataRate::Unknown, Encoding::Unknown, Header(0, 0, 1, 2) };
    int sectors = 0;
    int gap3 = 0;
    bool custom_cyl = false;
    bool custom_head = false;
    bool warned = false;
};


constexpr int DUMP_OFFSETS = 1;
constexpr int DUMP_DIFF = 2;
constexpr int DUMP_READSTATS = 4;

void DumpTrack(const CylHead& cylhead, const Track& track, const ScanContext& context, int flags = 0, const Sectors& ignored_sectors = Sectors{});
bool NormaliseTrack(const CylHead& cylhead, Track& track);
bool NormaliseBitstream(BitBuffer& bitbuf);
int RepairTrack(const CylHead& cylhead, Track& track, const Track& src_track, const Sectors& ignored_sectors = Sectors{});

std::vector<std::pair<char, size_t>> DiffSectorCopies(const Sector& sector);

Sector GetTypicalSector(const CylHead& cylhead, const Track& track, const Sector& last);

extern const std::string BAD_SECTOR_SIGN;
extern const std::string MISSING_SECTOR_SIGN;

bool WriteRegularDisk(FILE* f_, Disk& disk, const Format& format);
bool WriteAppleDODisk(FILE* f_, Disk& disk, const Format& format);

bool test_remove_gap2(const Data& data, int offset);
bool test_remove_gap3(const Data& data, int offset, int& gap3);
bool test_remove_gap4b(const Data& data, int offset);


enum class ChecksumType
{
    None,           // No checksum, known valid
    Constant_8C15,  // 8C 15
    Sum_1800,       // Sum of 0x1800 bytes
    XOR_1800,       // XOR of 0x1800 bytes
    XOR_18A0,       // XOR of 0x18a0 bytes
    CRC_D2F6_1800,  // CRC-16 (init=D2F6) for 0x1800 bytes
    CRC_D2F6_1802,  // CRC-16 (init=D2F6) for 0x1802 bytes
};

std::set<ChecksumType> ChecksumMethods(const uint8_t* buf, int len);
std::string ChecksumName(std::set<ChecksumType> method);
std::string ChecksumNameShort(std::set<ChecksumType> methods);
int ChecksumLength(ChecksumType method);

void scale_flux(std::vector<uint32_t>& flux_rev, uint64_t numerator, uint64_t denominator);
