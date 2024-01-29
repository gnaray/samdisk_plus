#ifndef RawTrackMFMH
#define RawTrackMFMH
//---------------------------------------------------------------------------

class RawTrackMFM; // Required because this and OrphanDataCapableTrack includes each other.

#include "BitBuffer.h"
#include "BitPositionableByteVector.h"
#include "ByteBitPosition.h"
#include "AddressMark.h"
#include "CRC16.h"
#include "Header.h"
#include "Sector.h"
#include "OrphanDataCapableTrack.h"
#include "Util.h"
#include "utils.h"

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

class TrackIndexInRawTrack : public AddressMarkInTrack
{
public:
    constexpr TrackIndexInRawTrack(const AddressMarkInTrack& addressMarkInTrack)
        : AddressMarkInTrack(addressMarkInTrack)
    {
    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue));
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark)
    {
        return addressMark == AddressMark::INDEX;
    }

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

//    static SectorIdInRawTrack ConstructByAddessMarkAndReadingIdAndCrc(
//        const AddressMarkInTrack& addressMarkInTrack, BitPositionableByteVector& trackContent)
//    {
//        SectorIdInRawTrack sectorIdInRawTrack(
//            addressMarkInTrack,
//            trackContent.ReadByte(), trackContent.ReadByte(), trackContent.ReadByte(),
//            trackContent.ReadByte(), trackContent.ReadByte(), trackContent.ReadByte()
//        );
//        return sectorIdInRawTrack;
//    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue));
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark)
    {
        return addressMark == AddressMark::ID;
    }

    CRC16 CalculateCrc() const
    {
        return CRC16(&m_addressMark, sizeof(SectorIdInRawTrack), CRC16::A1A1A1);
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

    static constexpr bool IsSuitable(uint8_t addressMarkValue)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue));
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark)
    {
        return addressMark == AddressMark::DATA || addressMark == AddressMark::ALT_DATA
                || addressMark == AddressMark::DELETED_DATA || addressMark == AddressMark::ALT_DELETED_DATA
                || addressMark == AddressMark::RX02;
    }

private:
};

template<unsigned int S>
class SectorBlockInRawTrack
{
public:
    constexpr SectorBlockInRawTrack(const uint8_t blockBytes[S])
    {
        std::copy(blockBytes, blockBytes + S, bytes);
    }

    uint8_t bytes[S];
};

template<unsigned int S>
class SectorDataInRawTrack : public AddressMarkInTrack, public SectorBlockInRawTrack<S>, public CrcInTrack
{
public:
    constexpr SectorDataInRawTrack(const AddressMarkInTrack& addressMarkInTrack,
        const uint8_t blockBytes[S], uint8_t crcHigh, uint8_t crcLow)
        : AddressMarkInTrack(addressMarkInTrack), SectorBlockInRawTrack<S>(blockBytes),
        CrcInTrack(crcHigh, crcLow)
    {
    }

//    static SectorDataInRawTrack ConstructByAddessMarkAndReadingDataAndCrc(
//        const AddressMarkInTrack& addressMarkInTrack, BitPositionableByteVector& trackContent,
//        int dataByteSize)
//    {
//        SectorDataInRawTrack sectorDataInRawTrack(
//            addressMarkInTrack, dataByteSize,
//            trackContent.ReadByte(), trackContent.ReadByte()
//        );
//        return sectorDataInRawTrack;
//    }

    static constexpr bool IsSuitable(uint8_t addressMarkValue)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue));
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark)
    {
        return addressMark == AddressMark::DATA || addressMark == AddressMark::ALT_DATA
                || addressMark == AddressMark::DELETED_DATA || addressMark == AddressMark::ALT_DELETED_DATA
                || addressMark == AddressMark::RX02;
    }

    CRC16 CalculateCrc() const
    {
        return CRC16(&m_addressMark, sizeof(SectorDataInRawTrack), CRC16::A1A1A1);
    }

};
#pragma pack(pop)



class RawTrackContext;

class SomethingFromRawTrack
{
public:
    constexpr SomethingFromRawTrack(const ByteBitPosition& foundByteBitPosition, const AddressMark& addressMark)
        : m_foundByteBitPosition(foundByteBitPosition), m_addressMark(addressMark)
    {
    }

    SomethingFromRawTrack(const SomethingFromRawTrack&) = default;
    SomethingFromRawTrack(SomethingFromRawTrack&&) = default;
    SomethingFromRawTrack& operator=(const SomethingFromRawTrack&) = default;
    SomethingFromRawTrack& operator=(SomethingFromRawTrack&&) = default;
    virtual ~SomethingFromRawTrack() = default;

    ByteBitPosition m_foundByteBitPosition;
    AddressMark m_addressMark;
};



class ProcessableSomethingFromRawTrack : public SomethingFromRawTrack
{
public:
    using SomethingFromRawTrack::SomethingFromRawTrack;

    virtual void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const = 0;
};



class TrackIndexFromRawTrack : public ProcessableSomethingFromRawTrack
{
public:
    TrackIndexFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const TrackIndexInRawTrack& trackIndexInRawTrack)
        : ProcessableSomethingFromRawTrack(foundByteBitPosition, trackIndexInRawTrack.m_addressMark)
    {
    }

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;
};

class SectorIdFromRawTrack : public ProcessableSomethingFromRawTrack
{
public:
    SectorIdFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const SectorIdInRawTrack& sectorIdInRawTrack)
        : ProcessableSomethingFromRawTrack(foundByteBitPosition, sectorIdInRawTrack.m_addressMark),
          cyl(sectorIdInRawTrack.m_cyl), head(sectorIdInRawTrack.m_head),
          sector(sectorIdInRawTrack.m_sector), sizeId(sectorIdInRawTrack.m_sizeId),
          badCrc(sectorIdInRawTrack.CalculateCrc() != 0)
    {
    }

    SectorIdFromRawTrack& operator=(const SectorIdFromRawTrack& sectorIdFromRawTrack) = default;

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;

    constexpr size_t DataSize() const
    {
        return SectorIdInTrack::ByteSizeBySizeId(sizeId);
    }
    constexpr bool empty() const
    {
        return m_foundByteBitPosition == 0;
    }

    uint8_t cyl;
    uint8_t head;
    uint8_t sector;
    uint8_t sizeId;
    bool badCrc;
};

class SectorDataRefFromRawTrack : public ProcessableSomethingFromRawTrack
{
public:
    SectorDataRefFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const SectorDataRefInRawTrack& sectorDataInRawTrack)
        : ProcessableSomethingFromRawTrack(foundByteBitPosition, sectorDataInRawTrack.m_addressMark)
    {
    }

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;
};

class SectorDataFromRawTrack : public SomethingFromRawTrack
{
public:
    template<unsigned int S>
    SectorDataFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const SectorDataInRawTrack<S>& sectorDataInRawTrack)
        : SomethingFromRawTrack(foundByteBitPosition, sectorDataInRawTrack.m_addressMark),
        data(sectorDataInRawTrack.bytes, sectorDataInRawTrack.bytes + S),
        badCrc(sectorDataInRawTrack.CalculateCrc() != 0)
    {
    }


    static SectorDataFromRawTrack Construct(const int dataSizeCode, const ByteBitPosition& byteBitPosition, const Data& somethingInTrackBytes);

    Data data{};
    bool badCrc;
};



class RawTrackContext
{
public:
    RawTrackContext(const CylHead& cylHead, const DataRate& dataRate, const Encoding& encoding)
        : cylHead(cylHead), dataRate(dataRate), encoding(encoding)
    {
    }
    constexpr bool DoSectorIdAndDataPositionsCohere(
            const ByteBitPosition& sectorIdByteBitPosition, const ByteBitPosition& dataByteBitPosition) const;

    CylHead cylHead;
    DataRate dataRate = DataRate::Unknown;
    Encoding encoding = Encoding::Unknown;
};



class RawTrackMFM
{
public:
    RawTrackMFM() = default;

    RawTrackMFM(const Data& rawTrackContent, const DataRate& dataRate)
        : m_rawTrackContent(rawTrackContent), dataRate(dataRate)
    {
    }

    RawTrackMFM(const MEMORY& rawTrackContent, const DataRate& dataRate)
        : m_rawTrackContent(rawTrackContent.pb, rawTrackContent.size), dataRate(dataRate)
    {
    }

    void Rewind();
    BitBuffer AsBitstream();
    std::shared_ptr<ProcessableSomethingFromRawTrack> FindNextSomething();
    void ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack, const RawTrackContext& rawTrackContext);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead) const;

    BitPositionableByteVector m_rawTrackContent{};

    DataRate dataRate = DataRate::Unknown;
    static const Encoding encoding;
};

#endif
