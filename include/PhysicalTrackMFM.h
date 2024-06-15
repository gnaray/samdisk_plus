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

    static bool IsValid(uint8_t byte)
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

    size_t ByteSizeBySizeId()
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



class PhysicalTrackContext
{
public:
    PhysicalTrackContext(const CylHead& cylHead, const DataRate& dataRate)
        : cylHead(cylHead), dataRate(dataRate)
    {
    }

    const CylHead cylHead;
    const DataRate dataRate = DataRate::Unknown;
};



class TrackIndexInPhysicalTrack : public AddressMarkInTrack
{
public:
    constexpr TrackIndexInPhysicalTrack(const AddressMarkInTrack& addressMarkInTrack)
        : AddressMarkInTrack(addressMarkInTrack)
    {
    }

    static bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int /*availableBytes*/)
    {
        return addressMark == AddressMark::INDEX;
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
        BitPositionableByteVector& physicalTrackContent, PhysicalTrackContext& physicalTrackContext,
        const Encoding& encoding);

private:
};

class SectorIdInPhysicalTrack : public AddressMarkInTrack, public SectorIdInTrack, public CrcInTrack
{
public:
    constexpr SectorIdInPhysicalTrack(const AddressMarkInTrack& addressMarkInTrack,
        uint8_t cyl, uint8_t head, uint8_t sector, uint8_t sizeId,
        uint8_t crcHigh, uint8_t crcLow)
        : AddressMarkInTrack(addressMarkInTrack), SectorIdInTrack(cyl, head, sector, sizeId),
          CrcInTrack(crcHigh, crcLow)
    {
    }

    static bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int availableBytes)
    {
        return addressMark == AddressMark::ID && intsizeof(SectorIdInPhysicalTrack) <= availableBytes;
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
        BitPositionableByteVector& physicalTrackContent, PhysicalTrackContext& physicalTrackContext,
        const Encoding& encoding);

    CRC16 CalculateCrc() const
    {
        return CRC16(&m_addressMark, sizeof(SectorIdInPhysicalTrack), CRC16::A1A1A1);
    }

    Header AsHeader() const
    {
        return Header(m_cyl, m_head, m_sector, m_sizeId);
    }

private:
};

class SectorDataRefInPhysicalTrack : public AddressMarkInTrack
{
public:
    constexpr SectorDataRefInPhysicalTrack(const AddressMarkInTrack& addressMarkInTrack)
        : AddressMarkInTrack(addressMarkInTrack)
    {
    }

    static bool IsSuitable(uint8_t addressMarkValue, const int availableBytes)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue), availableBytes);
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark, const int availableBytes)
    {
        return (addressMark == AddressMark::DATA || addressMark == AddressMark::ALT_DATA
                || addressMark == AddressMark::DELETED_DATA || addressMark == AddressMark::ALT_DELETED_DATA
                || addressMark == AddressMark::RX02) &&
                (intsizeof(SectorDataRefInPhysicalTrack) + intsizeof(CrcInTrack) <= availableBytes);
    }

    static void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
        BitPositionableByteVector& physicalTrackContent, PhysicalTrackContext& physicalTrackContext,
        const Encoding& encoding);

private:
};

#pragma pack(pop)



// Required by virtual subclasses thus this becomes non aggregate so suggested to declare all 5 (Rule of 3/5/0).
// See https://en.cppreference.com/w/cpp/language/rule_of_three, "C.21: If you define or =delete any copy, move, or destructor function, define or =delete them all."
class SectorDataFromPhysicalTrack
{
public:
    static constexpr const auto PHYSICAL_DATA_OVERHEAD = intsizeof(AddressMarkInTrack) + intsizeof(CrcInTrack);

    // Constructor for the case when physical data is processed first time.
    SectorDataFromPhysicalTrack(const Encoding& encoding, const ByteBitPosition& byteBitPositionFound, Data&& physicalData, bool dataSizeKnown)
        : physicalData(physicalData), encoding(encoding), byteBitPositionFound(byteBitPositionFound),
          addressMark(physicalData[0]), badCrc(dataSizeKnown ? CalculateCrcIsBad() : true)
    {
    }

    static void ProcessInto(Sector& sector, BitPositionableByteVector& physicalTrackContent, const Encoding& encoding,
                            const int nextIdamOffset = 0, const int nextDamOffset = 0);

    static constexpr int PhysicalSizeOf(const int dataSize)
    {
        return PHYSICAL_DATA_OVERHEAD + dataSize;
    }

    static constexpr bool IsSuitable(const int dataSize, const int availableBytes)
    {
        return PhysicalSizeOf(dataSize) <= availableBytes;
    }

    Data GetData() const
    {
        return GetData(physicalData, static_cast<int>(physicalData.end() - physicalData.begin()));
    }

protected:
    // Select real data from physical data.
    static Data GetData(const Data& physicalData, const int physicalSize)
    {
        if (physicalSize <= PHYSICAL_DATA_OVERHEAD)
            return Data();
        return Data(physicalData.begin() + intsizeof(AddressMarkInTrack), physicalData.begin() + physicalSize - intsizeof(CrcInTrack));
    }

    static bool CalculateCrcIsBad(const Encoding& encoding, const Data& physicalData, const int physicalSize)
    {
        CRC16 crc(encoding == Encoding::FM ? CRC16::INIT_CRC : CRC16::A1A1A1);
        return crc.add(physicalData, physicalSize) != 0;
    }

    bool CalculateCrcIsBad() const
    {
        return CalculateCrcIsBad(encoding, physicalData, physicalData.size());
    }

    Data physicalData{};

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

    PhysicalTrackMFM(const Data& physicalTrackContent, const DataRate& dataRate)
        : m_physicalTrackContent(physicalTrackContent), dataRate(dataRate)
    {
    }

    PhysicalTrackMFM(const MEMORY& physicalTrackContent, const DataRate& dataRate)
        : m_physicalTrackContent(physicalTrackContent.pb, physicalTrackContent.size), dataRate(dataRate)
    {
    }

    void Rewind();
    BitBuffer AsMFMBitstream();
    void ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead) const;

    BitPositionableByteVector m_physicalTrackContent{};

    DataRate dataRate = DataRate::Unknown;
    static const Encoding encoding;
};

#endif
