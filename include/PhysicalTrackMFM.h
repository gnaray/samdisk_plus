#ifndef PhysicalTrackMFMH
#define PhysicalTrackMFMH
//---------------------------------------------------------------------------

class PhysicalTrackMFM; // Required because this and OrphanDataCapableTrack includes each other.

#include "BitBuffer.h"
#include "BitPositionableByteVector.h"
#include "ByteBitPosition.h"
#include "AddressMark.h"
#include "CRC16.h"
#include "Header.h"
#include "OrphanDataCapableTrack.h"
#include "Util.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// =================== General definitions ===========================

constexpr uint8_t ADDRESS_MARK_SIGN = 0xA1; // It must be present 3 times consequitively.

#pragma pack(push)
#pragma pack(1) // Byte-accurate alignment is required (for Borland C++).
class AddressMarkSyncInTrack
{
public:
    constexpr AddressMarkSyncInTrack()
        : sign0(ADDRESS_MARK_SIGN), sign1(ADDRESS_MARK_SIGN),
        sign2(ADDRESS_MARK_SIGN)
    {
    }

    static constexpr bool IsValid(uint8_t byte)
    {
        return byte == ADDRESS_MARK_SIGN;
    }

    bool IsValid() const
    {
        return sign0 == ADDRESS_MARK_SIGN && sign1 == ADDRESS_MARK_SIGN && sign2 == ADDRESS_MARK_SIGN;
    }

    uint8_t sign0;
    uint8_t sign1;
    uint8_t sign2;
};

class AddressMarkInTrack
{
public:
    constexpr AddressMarkInTrack(const AddressMark& addressMark)
        : m_addressMark(addressMark)
    {
    }

    static constexpr bool IsValid(uint8_t byte)
    {
        return AddressMark::IsValid(byte);
    }

    AddressMark m_addressMark = AddressMark::UNDEFINED;
};

class SectorIdInTrack
{
public:
    constexpr SectorIdInTrack(uint8_t cyl, uint8_t head, uint8_t sector, uint8_t sizeId)
        : m_cyl(cyl), m_head(head), m_sector(sector), m_sizeId(sizeId)
    {
    }

    static constexpr size_t ByteSizeBySizeId(uint8_t sizeId)
    {
        return 128u << sizeId;
    }

    constexpr size_t ByteSizeBySizeId()
    {
        return SectorIdInTrack::ByteSizeBySizeId(m_sizeId);
    }

    uint8_t m_cyl;
    uint8_t m_head;
    uint8_t m_sector;
    uint8_t m_sizeId;
};

class CrcInTrack
{
public:
    constexpr CrcInTrack(uint8_t crcHigh, uint8_t crcLow)
        : m_crcHigh(crcHigh), m_crcLow(crcLow)
    {
    }

    uint8_t m_crcHigh;
    uint8_t m_crcLow;
};



class RawTrackContext
{
public:
    RawTrackContext(const CylHead& cylHead, const DataRate& dataRate)
        : cylHead(cylHead), dataRate(dataRate)
    {
    }

    constexpr bool DoSectorIdAndDataPositionsCohere(const ByteBitPosition& sectorIdByteBitPosition, const ByteBitPosition& dataByteBitPosition, const Encoding& encoding) const;

    CylHead cylHead;
    DataRate dataRate = DataRate::Unknown;
};



class TrackIndexInRawTrack : public AddressMarkInTrack
{
public:
    constexpr TrackIndexInRawTrack(const AddressMarkInTrack& addressMarkInTrack)
        : AddressMarkInTrack(addressMarkInTrack)
    {
    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int /*availableBytes*/)
    {
        return addressMark == AddressMark::INDEX;
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, BitPositionableByteVector& rawTrackContent, const RawTrackContext& rawTrackContext, const Encoding& encoding);

private:
};

class SectorIdInRawTrack : public AddressMarkInTrack, public SectorIdInTrack, public CrcInTrack
{
public:
    constexpr SectorIdInRawTrack(const AddressMarkInTrack& addressMarkInTrack,
        uint8_t cyl, uint8_t head, uint8_t sector, uint8_t sizeId,
        uint8_t crcHigh, uint8_t crcLow)
        : AddressMarkInTrack(addressMarkInTrack), SectorIdInTrack(cyl, head, sector, sizeId),
          CrcInTrack(crcHigh, crcLow)
    {
    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int availableBytes)
    {
        return addressMark == AddressMark::ID && intsizeof(SectorIdInRawTrack) <= availableBytes;
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, BitPositionableByteVector& rawTrackContent, const RawTrackContext& rawTrackContext, const Encoding& encoding);

    CRC16 CalculateCrc() const
    {
        return CRC16(&m_addressMark, sizeof(SectorIdInRawTrack), CRC16::A1A1A1);
    }

    Header AsHeader() const
    {
        return Header(m_cyl, m_head, m_sector, m_sizeId);
    }

private:
};

class SectorDataRefInRawTrack : public AddressMarkInTrack
{
public:
    constexpr SectorDataRefInRawTrack(const AddressMarkInTrack& addressMarkInTrack)
        : AddressMarkInTrack(addressMarkInTrack)
    {
    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int availableBytes)
    {
        return (addressMark == AddressMark::DATA || addressMark == AddressMark::ALT_DATA
                || addressMark == AddressMark::DELETED_DATA || addressMark == AddressMark::ALT_DELETED_DATA
                || addressMark == AddressMark::RX02) &&
                (intsizeof(SectorDataRefInRawTrack) + intsizeof(CrcInTrack) <= availableBytes);
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, BitPositionableByteVector& rawTrackContent, const RawTrackContext& rawTrackContext, const Encoding& encoding);

private:
};

#pragma pack(pop)



// Required by virtual subclasses thus this becomes non aggregate so suggested to declare all 5 (Rule of 3/5/0).
// See https://en.cppreference.com/w/cpp/language/rule_of_three, "C.21: If you define or =delete any copy, move, or destructor function, define or =delete them all."
class SectorDataFromRawTrack
{
public:
    // Constructor for the case when raw data is processed first time.
    SectorDataFromRawTrack(const Encoding& encoding, const ByteBitPosition& byteBitPositionFound, Data&& rawData, bool dataSizeKnown)
        : rawData(rawData), encoding(encoding), byteBitPositionFound(byteBitPositionFound),
          addressMark(rawData[0]), badCrc(dataSizeKnown ? CalculateCrcIsBad() : true)
    {
    }

    static void ProcessInto(Sector& sector, BitPositionableByteVector& rawTrackContent, const Encoding& encoding,
                            const int nextIdamOffset = 0, const int nextDamOffset = 0);

    // Method for the case when good data of raw data is requested because its size became known.
    static Data GetGoodDataUpToSize(const Sector& rawSector, const int sectorSize);

    static constexpr int RawSizeOf(const int dataSize)
    {
        return intsizeof(AddressMarkInTrack) + dataSize + intsizeof(CrcInTrack);
    }

    static constexpr bool IsSuitable(const int dataSize, const int availableBytes)
    {
        return RawSizeOf(dataSize) <= availableBytes;
    }

    Data GetData() const
    {
        return GetData(rawData, static_cast<int>(rawData.end() - rawData.begin()));
    }

protected:
    // Select real data from raw data.
    static Data GetData(const Data& rawData, const int rawSize)
    {
        if (rawSize <= intsizeof(AddressMarkInTrack) + intsizeof(CrcInTrack))
            return Data();
        return Data(rawData.begin() + intsizeof(AddressMarkInTrack), rawData.begin() + rawSize - intsizeof(CrcInTrack));
    }

    static bool CalculateCrcIsBad(const Encoding& encoding, const Data& rawData, const int rawSize)
    {
        CRC16 crc(encoding == Encoding::FM ? CRC16::INIT_CRC : CRC16::A1A1A1);
        return crc.add(rawData, rawSize) != 0;
    }

    bool CalculateCrcIsBad() const
    {
        return CalculateCrcIsBad(encoding, rawData, rawData.size());
    }

    Data rawData{};

public:
    Encoding encoding;
    ByteBitPosition byteBitPositionFound;
    AddressMark addressMark;
    bool badCrc;
};



class PhysicalTrackMFM
{
public:
    PhysicalTrackMFM() = default;

    PhysicalTrackMFM(const Data& rawTrackContent, const DataRate& dataRate)
        : m_rawTrackContent(rawTrackContent), dataRate(dataRate)
    {
    }

    PhysicalTrackMFM(const MEMORY& rawTrackContent, const DataRate& dataRate)
        : m_rawTrackContent(rawTrackContent.pb, rawTrackContent.size), dataRate(dataRate)
    {
    }

    void Rewind();
    BitBuffer AsMFMBitstream();
    void ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack, const RawTrackContext& rawTrackContext);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead) const;

    BitPositionableByteVector m_rawTrackContent{};

    DataRate dataRate = DataRate::Unknown;
    static const Encoding encoding;
};

#endif
