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
    // We also calculate with bits here though the code is slightly modified.
    const auto gap2_size_min = GetFmOrMfmGap2Length(dataRate, encoding);
    const auto idam_am_distance = GetFmOrMfmIdamAndAmDistance(dataRate, encoding);
    const auto min_distance = (1 + 6 + gap2_size_min) * 8 * 2; // IDAM, ID, gap2 (without sync and DAM.a1sync, why?)
    const auto max_distance = (idam_am_distance + 8) * 8 * 2; // IDAM, ID, gap2, sync, DAM.a1sync (gap2: WD177x offset, +8: gap2 may be longer when formatted by different type of controller)

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
        orphanDataCapableTrack.trackIndexOffset = m_foundByteBitPosition.TotalBitPosition() * 2; // Counted in mfmbits.
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " IAM at offset " << orphanDataCapableTrack.trackIndexOffset << "\n";
    }
}
// ====================================

void SectorIdFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    if (opt_normal_disk && !badCrc
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

        sector.offset = m_foundByteBitPosition.TotalBitPosition() * 2; // Counted in mfmbits.
        sector.set_badidcrc(badCrc);
        sector.set_constant_disk(false);
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " IDAM (id=" << header.sector << ") at offset " << sector.offset << "\n";
        orphanDataCapableTrack.track.add(std::move(sector));
    }
}
// ====================================

/*static*/ SectorDataFromRawTrack SectorDataFromRawTrack::Construct(const int dataSizeCode, const ByteBitPosition& byteBitPosition, const Data& somethingInTrackBytes)
{
    assert(dataSizeCode >= 0 && dataSizeCode <= 7);

    switch (dataSizeCode)
    {
    case 0:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<128>*>(somethingInTrackBytes.data()));
    case 1:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<256>*>(somethingInTrackBytes.data()));
    case 2:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<512>*>(somethingInTrackBytes.data()));
    case 3:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<1024>*>(somethingInTrackBytes.data()));
    case 4:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<2048>*>(somethingInTrackBytes.data()));
    case 5:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<4096>*>(somethingInTrackBytes.data()));
    case 6:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<8192>*>(somethingInTrackBytes.data()));
    case 7:
        return SectorDataFromRawTrack(byteBitPosition,
            *reinterpret_cast<const SectorDataInRawTrack<16384>*>(somethingInTrackBytes.data()));
    default:
        throw util::exception("Can not construct with dataSizeCode = ", dataSizeCode);
    } // end of switch(dataSizeCode)
}
// ====================================

void SectorDataRefFromRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack, RawTrackContext& rawTrackContext) const
{
    const uint8_t dam = m_addressMark;
    const auto am_offset = m_foundByteBitPosition.TotalBitPosition() * 2; // Counted in mfmbits.
    const Header header(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head, OrphanDataCapableTrack::ORPHAN_SECTOR_ID, SIZECODE_UNKNOWN);
    Sector sector(rawTrackContext.dataRate, rawTrackContext.encoding, header);
    sector.offset = am_offset;
    sector.dam = dam; // Comfortable later if setting here.
    sector.set_constant_disk(false);
    if (opt_debug)
        util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " DAM (am=" << dam << ") at offset " << sector.offset << "\n";
    orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
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
    const Data addressMarkBytes{0x44, 0x89, 0x44, 0x89, 0x44, 0x89}; // 0x4489 3 times in reverse bit order.
    const auto readLengthMin = intsizeof(AddressMarkSyncInTrack); // Looking for address mark sync only.
    const auto readLengthMinBits = readLengthMin * 8;
    BitPositionableByteVector rawTrackContentForBitBuffer;
    ByteBitPosition lastAddressMarkPosition{0};
    Data somethingInTrackBytes(readLengthMin);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(somethingInTrackBytes.data());
    for ( ; m_rawTrackContent.RemainingBitLength() >= readLengthMinBits; m_rawTrackContent.StepBit())
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
    return BitBuffer(dataRate, rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesBitSize());
}

// ====================================

std::shared_ptr<ProcessableSomethingFromRawTrack> RawTrackMFM::FindNextSomething()
{
    const auto readLengthMin = static_cast<int>(sizeof(AddressMarkSyncInTrack) + sizeof(SectorIdInRawTrack)); // Either sector id is found or sector data, former is shorter.
    const auto readLengthMinBits = readLengthMin * 8;
    Data somethingInTrackBytes(sizeof(AddressMarkSyncInTrack));
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(somethingInTrackBytes.data());
    for ( ; m_rawTrackContent.RemainingBitLength() >= readLengthMinBits; m_rawTrackContent.StepBit())
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
            } else if (SectorDataRefInRawTrack::IsSuitable(addressMarkValue))
            {
                somethingInTrackBytes.resize(sizeof(SectorDataRefInRawTrack));
                const auto sectorDataRefInRawTrack = reinterpret_cast<SectorDataRefInRawTrack*>(somethingInTrackBytes.data());
                m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
                return std::make_shared<SectorDataRefFromRawTrack>(byteBitPositionFound, *sectorDataRefInRawTrack);
            }
        }
    }
    return nullptr;
}
// ====================================

void RawTrackMFM::ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack, const RawTrackContext& rawTrackContext)
{
    static const VectorX<int> sectorDataInRawTrackSizes{
        intsizeof(SectorDataInRawTrack<128>), intsizeof(SectorDataInRawTrack<256>),
        intsizeof(SectorDataInRawTrack<512>), intsizeof(SectorDataInRawTrack<1024>),
        intsizeof(SectorDataInRawTrack<2048>), intsizeof(SectorDataInRawTrack<4096>),
        intsizeof(SectorDataInRawTrack<8192>), intsizeof(SectorDataInRawTrack<16384>)
    };

    const auto sectorIdsIndexSup = orphanDataCapableTrack.track.size();
    auto sectorIdsIndex = 0;
    auto orphanIt = orphanDataCapableTrack.orphanDataTrack.begin();
    while (orphanIt != orphanDataCapableTrack.orphanDataTrack.end())
    {
        auto& orphanSector = *orphanIt;
        m_rawTrackContent.SetByteBitPosition(orphanSector.offset / 2); // hbit to databit
        const auto byteBitPositionFound = m_rawTrackContent.GetByteBitPosition();// Position of DAM.
        auto byteBitPosition = byteBitPositionFound;  // Using this position for reading bytes.

        auto parentSectorIndexFound = false;
        int sectorOffset;
        // Find the closest sector id which coheres.
        while (sectorIdsIndex < sectorIdsIndexSup && (sectorOffset = orphanDataCapableTrack.track[sectorIdsIndex].offset) < orphanSector.offset)
        {
            if (rawTrackContext.DoSectorIdAndDataPositionsCohere(sectorOffset, orphanSector.offset))
                parentSectorIndexFound = true;
            sectorIdsIndex++;
        }
        if (parentSectorIndexFound) // Data belongs to sector id thus its size is provided by the sector id.
        {
            auto& sector = orphanDataCapableTrack.track[sectorIdsIndex - 1]; // The previous is found.
            auto dataSizeCode = sector.header.size;
            if (dataSizeCode > SIZECODE_MAX)
            {
                if (opt_debug)
                    util::cout << "sector has unsupported (invalid?) size code  " << dataSizeCode << " at offset " << sector.offset << ", ignoring it as parent sector\n";
                goto NextOrphan; // Not supported size code, ignoring the sector thus this data as well.
            }
            const auto remainingByteLength = m_rawTrackContent.RemainingBitLength() / 8;
            if (sectorDataInRawTrackSizes[dataSizeCode] > remainingByteLength)
                goto NextOrphan; // Not enough bytes thus crc is bad, and we do not provide bad data from raw track.
            Data somethingInTrackBytes(sectorDataInRawTrackSizes[dataSizeCode]);
            m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
            SectorDataFromRawTrack sectorData = SectorDataFromRawTrack::Construct(dataSizeCode, byteBitPositionFound, somethingInTrackBytes);

            const bool data_crc_error = sectorData.badCrc != 0;
            const uint8_t dam = sectorData.m_addressMark;

            sector.add_with_readstats(std::move(sectorData.data), data_crc_error, dam);
            orphanIt = orphanDataCapableTrack.orphanDataTrack.sectors().erase(orphanIt);
            continue; // Continuing from current orphan which was the next orphan before erasing.
        }
        else
        {   // Sector id not found, setting orphan's data from first byte after DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
            // Determine available bytes.
            m_rawTrackContent.ReadByte(&byteBitPosition); // Read the DAM and ignore it, it is already stored in orphan sector.
            const auto remainingByteLength = m_rawTrackContent.RemainingBitLength() / 8;
            auto availableByteLength = remainingByteLength;
            if (sectorIdsIndex < sectorIdsIndexSup)
            {
                auto availableByteLengthUntilNextIdam = (orphanDataCapableTrack.track[sectorIdsIndex].offset - orphanSector.offset) / 8 / 2
                        - GetIdamOverhead(rawTrackContext.encoding);
                if (availableByteLengthUntilNextIdam < availableByteLength)
                    availableByteLength = availableByteLengthUntilNextIdam;
            }
            if (std::next(orphanIt) != orphanDataCapableTrack.orphanDataTrack.end())
            {
                auto availableByteLengthUntilNextDam = (std::next(orphanIt)->offset - orphanSector.offset) / 8 / 2
                        - GetDamOverhead(rawTrackContext.encoding);
                if (availableByteLengthUntilNextDam < availableByteLength)
                    availableByteLength = availableByteLengthUntilNextDam;
            }
            Data somethingInTrackBytes(availableByteLength);
            m_rawTrackContent.ReadBytes(somethingInTrackBytes.data(), somethingInTrackBytes.size(), &byteBitPosition);
            if (opt_debug)
                util::cout << "raw_track_mfm_fm " << rawTrackContext.encoding << " DAM (am=" << orphanSector.dam << ") at offset " << orphanSector.offset << " without IDAM\n";
            orphanSector.add_with_readstats(std::move(somethingInTrackBytes), true, orphanSector.dam);
        }
NextOrphan:
        orphanIt++;
    }
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
        auto somethingFromRawTrack = FindNextSomething();
        if (somethingFromRawTrack == nullptr)
            break;
        somethingFromRawTrack->ProcessInto(orphanDataCapableTrack, rawTrackContext);
        m_rawTrackContent.StepBit();
    } while (true);

    if (!orphanDataCapableTrack.empty())
    {
        orphanDataCapableTrack.setTrackLen(m_rawTrackContent.BytesBitSize() * 2); // Counted in mfmbits.
        ProcessSectorDataRefs(orphanDataCapableTrack, rawTrackContext);
    }
    return orphanDataCapableTrack;
}
// ====================================

OrphanDataCapableTrack RawTrackMFM::DecodeTrack(const CylHead& cylHead) const
{
    auto rawTrack = *this;
    return rawTrack.DecodeTrack(cylHead);
}

//---------------------------------------------------------------------------
