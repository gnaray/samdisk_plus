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

typedef uint16_t crc_t;

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

//	static SectorIdInRawTrack ConstructByAddessMarkAndReadingIdAndCrc(
//		const AddressMarkInTrack& addressMarkInTrack, BitPositionableByteVector& trackContent)
//	{
//		SectorIdInRawTrack sectorIdInRawTrack(
//			addressMarkInTrack,
//			trackContent.ReadByte(), trackContent.ReadByte(), trackContent.ReadByte(),
//			trackContent.ReadByte(), trackContent.ReadByte(), trackContent.ReadByte()
//		);
//		return sectorIdInRawTrack;
//	}

    static constexpr bool IsSuitable(uint8_t addressMarkValue)
    {
        return AddressMark::IsValid(addressMarkValue) && IsSuitable(AddressMark(addressMarkValue));
    }

    static constexpr bool IsSuitable(const AddressMark& addressMark)
    {
        return addressMark == AddressMark::ID;
    }

    crc_t CalculateCrc() const
	{
        return CRC16(&m_addressMark, static_cast<size_t>(&m_crcHigh - reinterpret_cast<const uint8_t*>(&m_addressMark)), CRC16::A1A1A1);
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
    constexpr SectorBlockInRawTrack(uint8_t blockBytes[S])
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
        uint8_t blockBytes[S], uint8_t crcHigh, uint8_t crcLow)
        : AddressMarkInTrack(addressMarkInTrack), SectorBlockInRawTrack<S>(blockBytes),
        CrcInTrack(crcHigh, crcLow)
	{
	}

//	static SectorDataInRawTrack ConstructByAddessMarkAndReadingDataAndCrc(
//		const AddressMarkInTrack& addressMarkInTrack, BitPositionableByteVector& trackContent,
//		int dataByteSize)
//	{
//		SectorDataInRawTrack sectorDataInRawTrack(
//			addressMarkInTrack, dataByteSize,
//			trackContent.ReadByte(), trackContent.ReadByte()
//		);
//		return sectorDataInRawTrack;
//	}

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

    crc_t CalculateCrc() const
	{
        return CRC16(&m_addressMark, static_cast<size_t>(&m_crcHigh - reinterpret_cast<const uint8_t*>(&m_addressMark)), CRC16::A1A1A1);
	}

};
#pragma pack(pop)

class CrcFromTrackAndCalculated
{
public:
    constexpr CrcFromTrackAndCalculated(crc_t crcFromTrack, crc_t crcCalculated)
        : m_crc_from_track(crcFromTrack), m_crc_calculated(crcCalculated)
    {
    }

    CrcFromTrackAndCalculated(const CrcFromTrackAndCalculated&) = default;
    CrcFromTrackAndCalculated(CrcFromTrackAndCalculated&&) = default;
    CrcFromTrackAndCalculated& operator=(const CrcFromTrackAndCalculated&) = default;
    CrcFromTrackAndCalculated& operator=(CrcFromTrackAndCalculated&&) = default;
    virtual ~CrcFromTrackAndCalculated() = default; // Required by virtual subclasses thus this becomes non aggregate so suggested to declare all 5 (Rule of 3/5/0).
    // See https://en.cppreference.com/w/cpp/language/rule_of_three, "C.21: If you define or =delete any copy, move, or destructor function, define or =delete them all."

    constexpr crc_t CrcFromTrack() const
	{
		return m_crc_from_track;
	}

    constexpr crc_t CrcCalculated() const
	{
		return m_crc_calculated;
	}

    constexpr bool CrcsDiffer() const
	{
		return m_crc_from_track != m_crc_calculated;
	}
private:
    crc_t m_crc_from_track = 0;
    crc_t m_crc_calculated = 0;
};



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

    virtual void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const = 0;

    ByteBitPosition m_foundByteBitPosition;
    AddressMark m_addressMark;
};



class TrackIndexFromRawTrack : public SomethingFromRawTrack
{
public:
    TrackIndexFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const TrackIndexInRawTrack& sectorIdInRawTrack)
        : SomethingFromRawTrack(foundByteBitPosition, sectorIdInRawTrack.m_addressMark)
    {
    }

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;
};

class SectorIdFromRawTrack : public SomethingFromRawTrack, public CrcFromTrackAndCalculated
{
public:
    SectorIdFromRawTrack(
		const ByteBitPosition& foundByteBitPosition,
		const SectorIdInRawTrack& sectorIdInRawTrack)
        : SomethingFromRawTrack(foundByteBitPosition, sectorIdInRawTrack.m_addressMark),
          CrcFromTrackAndCalculated{static_cast<crc_t>((sectorIdInRawTrack.m_crcHigh << 8) | sectorIdInRawTrack.m_crcLow),
                                    sectorIdInRawTrack.CalculateCrc()},
          cyl(sectorIdInRawTrack.m_cyl), head(sectorIdInRawTrack.m_head),
          sector(sectorIdInRawTrack.m_sector), sizeId(sectorIdInRawTrack.m_sizeId)
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
};

class SectorDataRefFromRawTrack : public SomethingFromRawTrack
{
public:
    SectorDataRefFromRawTrack(
        const ByteBitPosition& foundByteBitPosition,
        const SectorDataRefInRawTrack& sectorDataInRawTrack)
        : SomethingFromRawTrack(foundByteBitPosition, sectorDataInRawTrack.m_addressMark)
    {
    }

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;
};

class SectorDataFromRawTrack : public SomethingFromRawTrack, public CrcFromTrackAndCalculated
{
public:
	template<unsigned int S>
    SectorDataFromRawTrack(
		const ByteBitPosition& foundByteBitPosition,
		const SectorDataInRawTrack<S>& sectorDataInRawTrack)
        : SomethingFromRawTrack(foundByteBitPosition, sectorDataInRawTrack.m_addressMark),
        CrcFromTrackAndCalculated{static_cast<crc_t>((sectorDataInRawTrack.m_crcHigh << 8) | sectorDataInRawTrack.m_crcLow),
                                  sectorDataInRawTrack.CalculateCrc()},
        data(sectorDataInRawTrack.bytes, sectorDataInRawTrack.bytes + S)
	{
	}

    void ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const override;

    Data data{};
};



class RawTrackContext
{
public:
    RawTrackContext(const CylHead& cylHead, const DataRate& dataRate, const Encoding& encoding) // Constructs empty object.
        : cylHead(cylHead), dataRate(dataRate), encoding(encoding),
          sectorIdFromRawTrackLastFound(ByteBitPosition(), SectorIdInRawTrack(AddressMarkInTrack(AddressMark()),
                                                                              0, 0, 0, 0, 0, 0))
    {
    }
    constexpr bool DoSectorIdAndDataPositionsCohere(
            const ByteBitPosition& sectorIdByteBitPosition, const ByteBitPosition& dataByteBitPosition) const;

    CylHead cylHead;
    DataRate dataRate = DataRate::Unknown;
    Encoding encoding = Encoding::Unknown;
    SectorIdFromRawTrack sectorIdFromRawTrackLastFound;
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
        : m_rawTrackContent(rawTrackContent.pb, lossless_static_cast<size_t>(rawTrackContent.size)), dataRate(dataRate)
    {
    }

    void Rewind();
    BitBuffer AsBitstream();
    std::shared_ptr<SomethingFromRawTrack> FindNextSomething(const RawTrackContext& rawTrackContext); //const SectorIdFromRawTrack* lastSectorId = nullptr);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead);
    OrphanDataCapableTrack DecodeTrack(const CylHead& cylHead) const;

    BitPositionableByteVector m_rawTrackContent{};

    DataRate dataRate = DataRate::Unknown;
    static const Encoding encoding;
};

#endif
