#include "RawTrackMFM.h"
#include "Options.h"
#include "Track.h"
#include "IBMPCBase.h"
#include "ByteBitPosition.h"
#include "BitPositionableByteVector.h"

#include <math.h>
#include <memory>
#include <time.h>

//---------------------------------------------------------------------------

static auto& opt_debug = getOpt<int>("debug");
static auto& opt_normal_disk = getOpt<bool>("normal_disk");

// ====================================

constexpr bool RawTrackContext::DoSectorIdAndDataPositionsCohere(
        const ByteBitPosition& sectorIdByteBitPosition, const ByteBitPosition& dataByteBitPosition) const
{
    if (sectorIdByteBitPosition >= dataByteBitPosition) // The data must be behind the sector id.
        return false;
    // This code is taken from Samdisk/BitstreamDecoder where every databit is
    // stored as two bits (in addition every FM encoded bit is stored as two rawbits).
    // We calculate with databits here thus the code is slightly modified.
    const auto gap2_size_min = GetFmOrMfmGap2Length(dataRate, encoding);
    const auto idam_am_distance = GetFmOrMfmIdamAndAmDistance(dataRate, encoding);
    const auto min_distance = (1 + 6 + gap2_size_min) * 8; // IDAM, ID, gap2 (without sync and DAM.a1sync, why?)
    const auto max_distance = (idam_am_distance + 8) * 8; // IDAM, ID, gap2, sync, DAM.a1sync (gap2: WD177x offset, +8: gap2 may be longer when formatted by different type of controller)

    const auto sectorIdAndDataPositionDistance = static_cast<int>((dataByteBitPosition - sectorIdByteBitPosition).TotalBitPosition());
    return sectorIdAndDataPositionDistance >= min_distance && sectorIdAndDataPositionDistance <= max_distance;
}
// ====================================

void TrackIndexFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    if (orphanDataCapableTrack.trackIndexOffset > 0)
    {
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " IAM at offset " << m_foundByteBitPosition.TotalBitPosition() * 2 << " ignored because found another earlier\n";
    }
    else
    {
        orphanDataCapableTrack.trackIndexOffset = lossless_static_cast<int>(m_foundByteBitPosition.TotalBitPosition() * 2); // Counted in mfmbits.
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " IAM at offset " << orphanDataCapableTrack.trackIndexOffset << "\n";
    }
}
// ====================================

void SectorIdFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    if (opt_normal_disk && !CrcsDiffer()
            && (cyl != rawTrackContext.cylHead.cyl || head != rawTrackContext.cylHead.head))
    {
        Message(msgWarning, "ReadHeaders: track's %s does not match sector's %s, ignoring this sector.",
            CH(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head), CHR(cyl, head, sector));
        orphanDataCapableTrack.cylheadMismatch = true;
    }
    else
    {
        const Header header(cyl, head, sector, sizeId);
        Sector sector(rawTrackContext.dataRate, rawTrackContext.encoding, header);

        sector.offset = lossless_static_cast<int>(m_foundByteBitPosition.TotalBitPosition() * 2); // Counted in mfmbits.
        sector.set_badidcrc(CrcsDiffer());
        sector.set_constant_disk(false);
        orphanDataCapableTrack.track.add(std::move(sector));
        rawTrackContext.sectorIdFromRawTrackLastFound = *this;
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " IDAM (id=" << header.sector << ") at offset " << sector.offset << "\n";
    }
}
// ====================================

void SectorDataRefFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    const uint8_t dam = m_addressMark;
    const auto am_offset = m_foundByteBitPosition.TotalBitPosition() * 2; // Counted in mfmbits.
    const Header header(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head, OrphanDataCapableTrack::ORPHAN_SECTOR_ID, SIZECODE_UNKNOWN);
    Sector sector(rawTrackContext.dataRate, rawTrackContext.encoding, header);
    sector.offset = am_offset;
    sector.dam = dam; // TODO not needed
    sector.set_constant_disk(false);
    if (opt_debug)
        util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " DAM (am=" << dam << ") at offset " << sector.offset << "\n";
    orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
}
// ====================================

void SectorDataFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    if (!rawTrackContext.sectorIdFromRawTrackLastFound.empty() && rawTrackContext.DoSectorIdAndDataPositionsCohere(
                rawTrackContext.sectorIdFromRawTrackLastFound.m_foundByteBitPosition, m_foundByteBitPosition))
    {
        const auto sectorIndex = orphanDataCapableTrack.track.size() - 1; // NOTE Last sector is sectorIdFromRawTrackLastFound except if more close same IDAMs are merged which is super rare.
        auto& sector = orphanDataCapableTrack.track[sectorIndex];
        const bool data_crc_error = CrcsDiffer();
        const uint8_t dam = m_addressMark;

        sector.add_with_readstats(Data{data}, data_crc_error, dam);
        if (opt_debug)
        {
            const auto am_offset = m_foundByteBitPosition.TotalBitPosition() * 2;
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " DAM (am=" << dam << ") at offset " << am_offset << "\n";
        }
    }
    else // Data without id. It is stored in track of orphan sectors with orphan sector id.
    {    // However if cylhead differs at least once during decoding then this kind of orphan data is very unsafe!
        const uint8_t dam = m_addressMark;
        const auto am_offset = lossless_static_cast<int>(m_foundByteBitPosition.TotalBitPosition() * 2); // Counted in mfmbits.
        const Header header(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head, OrphanDataCapableTrack::ORPHAN_SECTOR_ID, SizeToCode(data.size()));
        Sector sector(rawTrackContext.dataRate, rawTrackContext.encoding, header);

        sector.offset = am_offset;
        sector.set_constant_disk(false);
        const bool data_crc_error = CrcsDiffer();
        sector.add_with_readstats(Data(data), data_crc_error, dam);
        orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " DAM (am=" << dam << ") at offset " << sector.offset << " without IDAM\n";
    }
}
// ====================================

const Encoding RawTrackMFM::encoding{ Encoding::MFM }; // Obvious value for RawTrackMFM class.

void RawTrackMFM::Rewind()
{
    m_rawTrackContent.SetByteBitPosition(0);
}
// ====================================
/* The bitstream has two bits for each raw track databits.
 * In addition bitstream is made of little endian bytes.
 * For example: 11010010 in raw track results 1x0x1x1x 0x1x0x0x in bitstream where x is 0 or 1.
 *                                            ^ ^ ^ ^  ^ ^ ^ ^
 * Address mark is special case: 10100001 results 00100010 10010001 in bitstream.
 *                                                ^ ^ ^ ^  ^ ^ ^ ^
 * To convert raw track byte to stream two bytes (e.g. 11010010):
 * Double the bits (x1x1x0x1 x0x0x1x0) then reverse the bytes (1x0x1x1x 0x1x0x0x), correct.
 * If firstly reverse the bytes (01001011) then double the bits (0x1x0x0x 1x0x1x1x), would be wrong.
 */
BitBuffer RawTrackMFM::AsBitstream()
{
    const std::vector<uint8_t> addressMarkBytes{0x44, 0x89, 0x44, 0x89, 0x44, 0x89}; // 0x4489 3 times in reverse bit order.
    const auto readLengthMin = sizeof(AddressMarkSyncInTrack); // Looking for address mark sync only.
    BitPositionableByteVector rawTrackContentForBitBuffer;
    ByteBitPosition lastAddressMarkPosition{0};
    std::vector<uint8_t> somethingInTrackBytes(sizeof(AddressMarkSyncInTrack));
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(somethingInTrackBytes.data());
    for ( ; m_rawTrackContent.RemainingByteLength() >= readLengthMin; m_rawTrackContent.StepBit())
    {
        if (!AddressMarkSyncInTrack::IsValid(m_rawTrackContent.PeekByte())) // Bit of optimisation for speed.
            continue;
        const auto byteBitPositionFound = m_rawTrackContent.GetByteBitPosition();
        auto byteBitPosition = m_rawTrackContent.GetByteBitPosition(); // Using this position for reading bytes.
        m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), sizeof(AddressMarkSyncInTrack), &byteBitPosition);
        if (addressMarkSyncInTrack->IsValid() && AddressMarkInTrack::IsValid(m_rawTrackContent.PeekByte(&byteBitPosition)))
        {
            const auto bitsLen = byteBitPositionFound - lastAddressMarkPosition;
            rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_rawTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);
            rawTrackContentForBitBuffer.WriteBytes(addressMarkBytes);
            lastAddressMarkPosition = byteBitPosition;
            m_rawTrackContent.SetByteBitPosition(--byteBitPosition);
        }
    }
    const auto bitsLen = m_rawTrackContent.BytesBitEndPosition() - lastAddressMarkPosition;
    rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_rawTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);

    // Bitstream scanner expects bits like: 0.:76543210, 1.:76543210, etc. while our content has 0.:01234567, 1.:01234567, etc.
    util::bit_reverse(rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesByteSize());

    // Luckily BitBuffer's default encoding is MFM so no need to set it.
    return BitBuffer(dataRate, rawTrackContentForBitBuffer.Bytes().data(), lossless_static_cast<int>(rawTrackContentForBitBuffer.BytesBitSize()));
}

// ====================================

static const std::vector<size_t> sectorDataInRawTrackSizes{
    sizeof(SectorDataInRawTrack<128>), sizeof(SectorDataInRawTrack<256>),
    sizeof(SectorDataInRawTrack<512>), sizeof(SectorDataInRawTrack<1024>),
    sizeof(SectorDataInRawTrack<2048>), sizeof(SectorDataInRawTrack<4096>),
    sizeof(SectorDataInRawTrack<8192>), sizeof(SectorDataInRawTrack<16384>)
};

std::shared_ptr<SomethingFromRawTrack> RawTrackMFM::FindNextSomething(const RawTrackContext& rawTrackContext) //const SectorIdFromRawTrack* lastSectorId/* = nullptr*/)
{
    const auto readLengthMin = sizeof(AddressMarkSyncInTrack) + sizeof(SectorIdInRawTrack); // Either sector id is found or sector data, former is shorter.
    std::vector<uint8_t> somethingInTrackBytes(sizeof(AddressMarkSyncInTrack));
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(somethingInTrackBytes.data());
    for ( ; m_rawTrackContent.RemainingByteLength() >= readLengthMin; m_rawTrackContent.StepBit())
	{
        if (!AddressMarkSyncInTrack::IsValid(m_rawTrackContent.PeekByte())) // Bit of optimisation for speed.
			continue;
        auto byteBitPosition = m_rawTrackContent.GetByteBitPosition(); // Using this position for reading bytes.
        m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), sizeof(AddressMarkSyncInTrack), &byteBitPosition);
        if (addressMarkSyncInTrack->IsValid())
		{
            const auto byteBitPositionFound = byteBitPosition; // Position of ?AM.
            const auto addressMarkValue = m_rawTrackContent.PeekByte(&byteBitPosition);
            if (TrackIndexInRawTrack::IsSuitable(addressMarkValue))
            {
                somethingInTrackBytes.resize(sizeof(TrackIndexInRawTrack));
                const auto trackIndexInRawTrack = reinterpret_cast<TrackIndexInRawTrack*>(somethingInTrackBytes.data());
                m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
                return std::make_shared<TrackIndexFromRawTrack>(byteBitPositionFound, *trackIndexInRawTrack);
            } else if (SectorIdInRawTrack::IsSuitable(addressMarkValue))
            {
                somethingInTrackBytes.resize(sizeof(SectorIdInRawTrack));
                const auto sectorIdInRawTrack = reinterpret_cast<SectorIdInRawTrack*>(somethingInTrackBytes.data());
                m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
                return std::make_shared<SectorIdFromRawTrack>(byteBitPositionFound, *sectorIdInRawTrack);
            } else if (SectorDataInRawTrack<128>::IsSuitable(addressMarkValue)) // <Size> is irrelevant.
            {
                const auto dataSizePreferred = !rawTrackContext.sectorIdFromRawTrackLastFound.empty() ? rawTrackContext.sectorIdFromRawTrackLastFound.DataSize() : 512; // Floppies have 512 bytes (sizeId = 2) sectors usually.
                const auto remainingByteLength = m_rawTrackContent.RemainingByteLength();
                size_t dataSizeCodeBegin = 0;
                size_t dataSizeCodeEnd = 8;
                if (!rawTrackContext.sectorIdFromRawTrackLastFound.empty() &&
                        rawTrackContext.DoSectorIdAndDataPositionsCohere(rawTrackContext.sectorIdFromRawTrackLastFound.m_foundByteBitPosition, byteBitPositionFound))
                {
                    dataSizeCodeBegin = rawTrackContext.sectorIdFromRawTrackLastFound.sizeId;
                    dataSizeCodeEnd = dataSizeCodeBegin + 1;
                }
                size_t dataSizeCode;
                for (dataSizeCode = dataSizeCodeBegin; dataSizeCode < dataSizeCodeEnd; dataSizeCode++)
                {
                    if (sectorDataInRawTrackSizes[dataSizeCode] > remainingByteLength)
                        break;
                }
                if (dataSizeCode == dataSizeCodeBegin) // There are not enough remaining bytes.
                    break;
                dataSizeCodeEnd = dataSizeCode--; // The dataSizeCode is not suitable but its decremented value is suitable.
                const size_t maxSectorDataSize = sectorDataInRawTrackSizes[dataSizeCode];
                somethingInTrackBytes.resize(maxSectorDataSize);
                m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
                std::shared_ptr<SectorDataFromRawTrack> resultBest;
                for (dataSizeCode = dataSizeCodeBegin; dataSizeCode < dataSizeCodeEnd; dataSizeCode++)
                {
                    std::shared_ptr<SectorDataFromRawTrack> result;
                    switch (dataSizeCode)
                    {
                    case 0:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<128>*>(somethingInTrackBytes.data()));
                        break;
                    case 1:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<256>*>(somethingInTrackBytes.data()));
                        break;
                    case 2:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<512>*>(somethingInTrackBytes.data()));
                        break;
                    case 3:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<1024>*>(somethingInTrackBytes.data()));
                        break;
                    case 4:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<2048>*>(somethingInTrackBytes.data()));
                        break;
                    case 5:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<4096>*>(somethingInTrackBytes.data()));
                        break;
                    case 6:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<8192>*>(somethingInTrackBytes.data()));
                        break;
                    case 7:
                        result = std::make_shared<SectorDataFromRawTrack>(byteBitPositionFound,
                            *reinterpret_cast<SectorDataInRawTrack<16384>*>(somethingInTrackBytes.data()));
                        break;
                    } // end of switch(dataSizeCode)
                    if (result == nullptr)
                        continue;
                    if (resultBest == nullptr || (resultBest->CrcsDiffer() && result->CrcsDiffer() && resultBest->data.size() != lossless_static_cast<int>(dataSizePreferred))
                            || (resultBest->CrcsDiffer() && !result->CrcsDiffer())
                            || (!resultBest->CrcsDiffer() && !result->CrcsDiffer() && resultBest->data.size() != lossless_static_cast<int>(dataSizePreferred)))
                        resultBest = result;
                }
                if (resultBest != nullptr)
                    return resultBest;
				break;
            } // endif SectorDataInRawTrack<128>::IsSuitable(addressMarkValue)
		}
	}
	return nullptr;
}
// ====================================
/*
About address mark:
Clocked:    44 89 44 89 44 89 or 44 A9 44 A9 44 A9
Declocked:  A1    A1    A1
Clock:      0A    0A    0A    or 0E    0E    0E
In BitstreamDecoder: sync_mask = opt_a1sync ? 0xffdfffdf
a1syncmask: FF DF FF DF FF DF
The a1syncmask removes the 2nd clocksign from the byte 89.
*/
OrphanDataCapableTrack RawTrackMFM::DecodeTrack(const CylHead& cylHead)
{
    OrphanDataCapableTrack orphanDataCapableTrack;
    RawTrackContext rawTrackContext{cylHead, dataRate, encoding};

	do
	{
        auto somethingFromRawTrack = FindNextSomething(rawTrackContext);
		if (somethingFromRawTrack == nullptr)
			break;
        somethingFromRawTrack->ProcessInto(orphanDataCapableTrack, rawTrackContext);
        m_rawTrackContent.StepBit();
    } while (true);

    if (!orphanDataCapableTrack.empty())
        orphanDataCapableTrack.setTrackLen(lossless_static_cast<int>(m_rawTrackContent.BytesBitSize() * 2)); // Counted in mfmbits.
    return orphanDataCapableTrack;
}
// ====================================

OrphanDataCapableTrack RawTrackMFM::DecodeTrack(const CylHead& cylHead) const
{
    auto rawTrack = *this;
    return rawTrack.DecodeTrack(cylHead);
}

//---------------------------------------------------------------------------
