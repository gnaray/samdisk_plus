#include "PhysicalTrackMFM.h"
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

/*static*/ void TrackIndexInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                  BitPositionableByteVector& physicalTrackContent,
                                                  PhysicalTrackContext& /*physicalTrackContext*/,
                                                  const Encoding& encoding)
{
    const auto byteBitPositionIAM = physicalTrackContent.GetByteBitPosition();
    if (opt_debug >= 2)
        util::cout << "physical_track_mfm_fm " << encoding << " IAM at offset " << byteBitPositionIAM.TotalBitPosition() * 2;
    if (orphanDataCapableTrack.trackIndexOffset > 0)
    {
        if (opt_debug >= 2)
            util::cout << " ignored because found another earlier";
    }
    else
        orphanDataCapableTrack.trackIndexOffset = DataBitPositionAsBitOffset(byteBitPositionIAM.TotalBitPosition(), encoding); // Counted in mfmbits (rawbits).
    if (opt_debug >= 2)
        util::cout << "\n";
}
// ====================================

/*static*/ void SectorIdInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
    BitPositionableByteVector& physicalTrackContent, PhysicalTrackContext& physicalTrackContext,
    const Encoding& encoding)
{
    const auto byteBitPositionIDAM = physicalTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorIdInPhysicalTrack);
    Data sectorIdInPhysicalTrackBytes(requiredByteLength);
    physicalTrackContent.ReadBytes(sectorIdInPhysicalTrackBytes);

    const auto sectorIdInPhysicalTrack = *reinterpret_cast<SectorIdInPhysicalTrack*>(sectorIdInPhysicalTrackBytes.data());
    const auto addressMark = sectorIdInPhysicalTrack.m_addressMark;
    const auto badCrc = sectorIdInPhysicalTrack.CalculateCrc() != 0;
    const auto header = sectorIdInPhysicalTrack.AsHeader();
    const auto sectorOffset = DataBitPositionAsBitOffset(byteBitPositionIDAM.TotalBitPosition(), encoding); // Counted in mfmbits (rawbits).
    if (badCrc) // Not allowing bad crc ids.
    {
        if (opt_debug >= 2)
            util::cout << "physical_track_mfm_fm " << encoding << " IDAM (am=" << addressMark << ", id=" << header.sector << ") at offset " << sectorOffset << " is ignored due to bad CRC\n";
        return;
    }

    if (!VerifyCylHeadsMatch(physicalTrackContext.m_cylHead, header, badCrc, opt_normal_disk, true))
        orphanDataCapableTrack.cylheadMismatch = true;
    else
    {
        Sector sector(physicalTrackContext.m_dataRate, encoding, header);
        sector.offset = sectorOffset;
        sector.set_badidcrc(badCrc);
        sector.set_constant_disk(false);
        if (opt_debug >= 2)
            util::cout << "physical_track_mfm_fm " << encoding << " IDAM (am=" << addressMark << ", id=" << header.sector << ") at offset " << sector.offset << "\n";
        orphanDataCapableTrack.track.add(std::move(sector));
    }
}
// ====================================

/*static*/ void SectorDataRefInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                     BitPositionableByteVector& physicalTrackContent,
                                                     PhysicalTrackContext& physicalTrackContext,
                                                     const Encoding& encoding)
{
    const auto byteBitPositionDAM = physicalTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorDataRefInPhysicalTrack);
    Data sectorDataRefInPhysicalTrackBytes(requiredByteLength);
    physicalTrackContent.ReadBytes(sectorDataRefInPhysicalTrackBytes);

    const auto sectorDataRefInPhysicalTrack = *reinterpret_cast<SectorDataRefInPhysicalTrack*>(sectorDataRefInPhysicalTrackBytes.data());
    const auto addressMark = sectorDataRefInPhysicalTrack.m_addressMark;
    const Header header(physicalTrackContext.m_cylHead.cyl, physicalTrackContext.m_cylHead.head, Header::ORPHAN_SECTOR_ID, Header::SIZECODE_UNKNOWN);
    Sector sector(physicalTrackContext.m_dataRate, encoding, header);
    sector.offset = DataBitPositionAsBitOffset(byteBitPositionDAM.TotalBitPosition(), encoding); // Counted in mfmbits (rawbits).
    sector.set_constant_disk(false);
    if (opt_debug >= 2)
        util::cout << "physical_track_mfm_fm " << encoding << " DAM (am=" << addressMark << ") at offset " << sector.offset << "\n";
    orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
}
// ====================================

/*static*/ void SectorDataFromPhysicalTrack::ProcessInto(Sector& sector, BitPositionableByteVector& physicalTrackContent,
                                                    const Encoding& encoding,
                                                    const int nextIdamOffset/* = 0*/, const int nextDamOffset/* = 0*/)
{
    const auto byteBitPositionDAM = physicalTrackContent.GetByteBitPosition();
    if (!sector.HasUnknownSize())
    {
        const auto dataSize = sector.size();
        const auto requiredByteLength = PhysicalSizeOf(dataSize);
        Data sectorDataInPhysicalTrackBytes(requiredByteLength);
        physicalTrackContent.ReadBytes(sectorDataInPhysicalTrackBytes);
        const SectorDataFromPhysicalTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInPhysicalTrackBytes), true);
        const bool data_crc_error = sectorData.m_badCrc;
        const auto dam = sectorData.m_addressMark;
        // Next line is a possibility. The gap2 does not exist in Sector but its value would be the following.
        //sector.gap2 = DataBitPositionAsBitOffset(sectorData.byteBitPositionFound.TotalBitPosition(), encoding) - sector.offset;
        sector.add(sectorData.GetData(), data_crc_error, dam);
    }
    else
    {   // Sector id not found, setting orphan's data from DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
        // Determine available bytes.
        auto& orphanSector = sector;
        const auto remainingByteLength = physicalTrackContent.RemainingByteLength();
        auto availableByteLength = remainingByteLength;
        if (nextIdamOffset > 0)
        {
            const auto availableByteLengthUntilNextIdam = BitOffsetAsDataBytePosition(nextIdamOffset - orphanSector.offset, encoding)
                    - GetIdamOverhead(encoding);
            if (availableByteLengthUntilNextIdam < availableByteLength)
                availableByteLength = availableByteLengthUntilNextIdam;
        }
        if (nextDamOffset > 0)
        {
            const auto availableByteLengthUntilNextDam = BitOffsetAsDataBytePosition(nextDamOffset - orphanSector.offset, encoding)
                    - GetDamOverhead(encoding);
            if (availableByteLengthUntilNextDam < availableByteLength)
                availableByteLength = availableByteLengthUntilNextDam;
        }
        // Read the bytes, create sector data then add its data to orphan sector.
        Data sectorDataInPhysicalTrackBytes(availableByteLength);
        physicalTrackContent.ReadBytes(sectorDataInPhysicalTrackBytes);
        SectorDataFromPhysicalTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInPhysicalTrackBytes), false);
        const bool data_crc_error = sectorData.m_badCrc; // Always true in this case (because size is unknown).
        const auto dam = sectorData.m_addressMark;
        if (opt_debug >= 2)
            util::cout << "physical_track_mfm_fm " << encoding << " DAM (am=" << dam << ") at offset " << orphanSector.offset << " without IDAM\n";
        orphanSector.add(std::move(sectorData.m_physicalData), data_crc_error, dam);
    }
}

void PhysicalTrackMFM::Rewind()
{
    m_physicalTrackContent.SetByteBitPosition(0);
}
// ====================================
/* The bitstream has two bits for each physical track databits.
 * In addition bitstream is made of little endian bytes.
 * For example: 11010010 in physical track results 1x0x1x1x 0x1x0x0x in bitstream where x is 0 or 1.
 *                                            ^ ^ ^ ^  ^ ^ ^ ^
 * Address mark is special case: 10100001 results 00100010 10010001 in bitstream.
 *                                                ^ ^ ^ ^  ^ ^ ^ ^
 * To convert physical track byte to stream two bytes (e.g. 11010010):
 * Double the bits (x1x1x0x1 x0x0x1x0) then reverse the bytes (1x0x1x1x 0x1x0x0x), correct.
 * If firstly reverse the bytes (01001011) then double the bits (0x1x0x0x 1x0x1x1x), would be wrong.
 */
BitBuffer PhysicalTrackMFM::AsMFMBitstream()
{
    const Data addressMarkBytes{0x44, 0x89, 0x44, 0x89, 0x44, 0x89}; // 0x4489 3 times in reverse bit order.
    const auto readLengthMin = intsizeof(AddressMarkSyncInTrack); // Looking for address mark sync only.
    BitPositionableByteVector rawTrackContentForBitBuffer;
    ByteBitPosition lastAddressMarkPosition(0);
    Data addressMarkSyncInPhysicalTrackBytes(readLengthMin);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(addressMarkSyncInPhysicalTrackBytes.data());
    while (m_physicalTrackContent.RemainingByteLength() >= readLengthMin)
    {
        if (AddressMarkSyncInTrack::IsValid(m_physicalTrackContent.PeekByte())) // Bit of optimisation for speed.
        {
            const auto byteBitPositionFound = m_physicalTrackContent.GetByteBitPosition();
            m_physicalTrackContent.ReadBytes(addressMarkSyncInPhysicalTrackBytes);
            if (addressMarkSyncInTrack->IsValid() && AddressMarkInTrack::IsValid(m_physicalTrackContent.PeekByte()))
            {
                const auto bitsLen = byteBitPositionFound - lastAddressMarkPosition;
                rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_physicalTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);
                rawTrackContentForBitBuffer.WriteBytes(addressMarkBytes);
                lastAddressMarkPosition = m_physicalTrackContent.GetByteBitPosition();
                continue;
            }
            else
                m_physicalTrackContent.SetByteBitPosition(byteBitPositionFound);
        }
        m_physicalTrackContent.StepBit();
    }
    const auto bitsLen = m_physicalTrackContent.BytesBitEndPosition() - lastAddressMarkPosition;
    rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_physicalTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);

    // Bitstream scanner expects bits like: 0.:76543210, 1.:76543210, etc. while our content has 0.:01234567, 1.:01234567, etc.
    util::bit_reverse(rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesByteSize());

    // BitBuffer's default encoding is MFM so no need to set it.
    return BitBuffer(m_dataRate, rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesBitSize());
}

// ====================================
// Process sector data refs, storing the data either in parent sector or orphan sector depending on if there are coherent parent and orphan pairs.
void PhysicalTrackMFM::ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack)
{
    Track& parentsTrack = orphanDataCapableTrack.track;
    Track& orphansTrack = orphanDataCapableTrack.orphanDataTrack;
    assert(orphansTrack.tracklen == parentsTrack.tracklen);
    if (parentsTrack.empty() || orphansTrack.empty())
        return;

    const auto trackLen = parentsTrack.tracklen;
    auto itParent = parentsTrack.begin();
    auto itOrphan = orphansTrack.begin();
    const auto itParentEnd = parentsTrack.end(); // The parents end is constant since merging sector data to parent does not change the parents.
    // An orphan data and a parent sector are matched if they cohere and there is no closer to one of them that also coheres.
    while (itOrphan != orphansTrack.end()) // The orphans end is variable since possibly erasing sector from orphans.
    {
        auto& orphanDataSector = *itOrphan;
        m_physicalTrackContent.SetByteBitPosition(BitOffsetAsDataBitPosition(orphanDataSector.offset, m_encoding)); // mfmbits (rawbits) to databits
        if (itParent != itParentEnd)
        {
            auto& parentSector = *itParent;
            // Find the closest sector id which coheres.
            const auto cohereResult = DoSectorIdAndDataOffsetsCohere(parentSector.offset, orphanDataSector.offset, m_dataRate, m_encoding, trackLen);
            if (cohereResult == CohereResult::DataCoheres)
            {
                itParent++; // Next parent.
                if (itParent != itParentEnd &&
                        DoSectorIdAndDataOffsetsCohere(itParent->offset, orphanDataSector.offset, m_dataRate, m_encoding, trackLen)
                    == CohereResult::DataCoheres)
                    continue;
                // Orphan data belongs to parent sector thus it is not orphan anymore and its size is provided by the id.sector.
                const auto dataSize = parentSector.size();
                const auto availableBytes = m_physicalTrackContent.RemainingByteLength();
                if (!SectorDataFromPhysicalTrack::IsSuitable(dataSize, availableBytes))
                    itOrphan++; // Not enough bytes thus crc is bad, and we do not provide broken (too short) data from physical track.
                else
                {
                    SectorDataFromPhysicalTrack::ProcessInto(parentSector, m_physicalTrackContent, orphanDataSector.encoding);
                    itOrphan = orphansTrack.sectors().erase(itOrphan);
                }
                continue;
            }
            else if (cohereResult == CohereResult::DataTooLate) // Parent is before orphan.
            {
                itParent++;
                continue;
            }
        }
        // Parent is after orphan, setting orphan's data from first byte after DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
        itOrphan++; // Next orphan.
        SectorDataFromPhysicalTrack::ProcessInto(orphanDataSector, m_physicalTrackContent, orphanDataSector.encoding,
                                            itParent != itParentEnd ? itParent->offset : 0,
                                            itOrphan != orphansTrack.end() ? itOrphan->offset : 0);
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
OrphanDataCapableTrack PhysicalTrackMFM::DecodeTrack(const CylHead& cylHead)
{
    m_encoding = Encoding::MFM; // Now only MFM encoding is supported. Decoding a FM track has high false-positive risk.
    OrphanDataCapableTrack orphanDataCapableTrack;
    PhysicalTrackContext physicalTrackContext(cylHead, m_dataRate);

    const auto trackLen = DataBitPositionAsBitOffset(m_physicalTrackContent.BytesBitSize(), m_encoding); // Counted in mfmbits (rawbits).
    orphanDataCapableTrack.setTrackLen(trackLen * 2); // Setting double the tracklen to avoid wrapping.
    constexpr const auto addressMarkSyncInPhysicalTrackLength = intsizeof(AddressMarkSyncInTrack);
    Data addressMarkSyncInPhysicalTrackBytes(addressMarkSyncInPhysicalTrackLength);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(addressMarkSyncInPhysicalTrackBytes.data());
    constexpr const auto addressMarkOverheadInPhysicalTrackLength = addressMarkSyncInPhysicalTrackLength + 1;
    while (m_physicalTrackContent.RemainingByteLength() >= addressMarkOverheadInPhysicalTrackLength)
    {
        if (AddressMarkSyncInTrack::IsValid(m_physicalTrackContent.PeekByte())) // Bit of optimisation for speed.
        {
            auto byteBitPosition = m_physicalTrackContent.GetByteBitPosition();
            m_physicalTrackContent.ReadBytes(addressMarkSyncInPhysicalTrackBytes);
            if (addressMarkSyncInTrack->IsValid())
            {
                const auto addressMarkValue = m_physicalTrackContent.PeekByte();
                const auto availableBytes = m_physicalTrackContent.RemainingByteLength();
                if (TrackIndexInPhysicalTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    TrackIndexInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, m_encoding);
                    continue;
                }
                else if (SectorIdInPhysicalTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorIdInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, m_encoding);
                    continue;
                }
                else if (SectorDataRefInPhysicalTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorDataRefInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, m_encoding);
                    continue;
                }
            }
            m_physicalTrackContent.SetByteBitPosition(byteBitPosition);
        }
        m_physicalTrackContent.StepBit();
    }

    ProcessSectorDataRefs(orphanDataCapableTrack);
    orphanDataCapableTrack.setTrackLen(trackLen); // Setting the real tracklen which sets tracktime too if track is not empty.

    return orphanDataCapableTrack;
}
// ====================================

OrphanDataCapableTrack PhysicalTrackMFM::DecodeTrack(const CylHead& cylHead) const
{
    auto physicalTrack = *this;
    return physicalTrack.DecodeTrack(cylHead);
}

//---------------------------------------------------------------------------
