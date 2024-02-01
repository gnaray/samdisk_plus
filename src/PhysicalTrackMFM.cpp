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

constexpr bool PhysicalTrackContext::DoSectorIdAndDataOffsetsCohere(
        const int sectorIdOffset, const int dataOffset, const Encoding& encoding) const
{
    return ::DoSectorIdAndDataOffsetsCohere(sectorIdOffset, dataOffset, dataRate, encoding);
}
// ====================================

/*static*/ void TrackIndexInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                  BitPositionableByteVector& physicalTrackContent,
                                                  const PhysicalTrackContext& /*physicalTrackContext*/,
                                                  const Encoding& encoding)
{
    const auto byteBitPositionIAM = physicalTrackContent.GetByteBitPosition();
    if (opt_debug)
        util::cout << "physical_track_mfm_fm " << encoding << " IAM at offset " << byteBitPositionIAM.TotalBitPosition() * 2;
    if (orphanDataCapableTrack.trackIndexOffset > 0)
    {
        if (opt_debug)
            util::cout << " ignored because found another earlier";
    }
    else
        orphanDataCapableTrack.trackIndexOffset = DataBitPositionAsBitOffset(byteBitPositionIAM.TotalBitPosition()); // Counted in mfmbits.
    if (opt_debug)
        util::cout << "\n";
}
// ====================================

/*static*/ void SectorIdInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                BitPositionableByteVector& physicalTrackContent,
                                                const PhysicalTrackContext& physicalTrackContext,
                                                const Encoding& encoding)
{
    const auto byteBitPositionIDAM = physicalTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorIdInPhysicalTrack);
    Data sectorIdInPhysicalTrackBytes(requiredByteLength);
    physicalTrackContent.ReadBytes(sectorIdInPhysicalTrackBytes);

    const auto sectorIdInPhysicalTrack = *reinterpret_cast<SectorIdInPhysicalTrack*>(sectorIdInPhysicalTrackBytes.data());
    const auto addressMark = sectorIdInPhysicalTrack.m_addressMark;
    const auto badCrc = sectorIdInPhysicalTrack.CalculateCrc() != 0;

    if (opt_normal_disk && !badCrc
            && (sectorIdInPhysicalTrack.m_cyl != physicalTrackContext.cylHead.cyl || sectorIdInPhysicalTrack.m_head != physicalTrackContext.cylHead.head))
    {
        Message(msgWarning, "ReadHeaders: track's %s does not match sector's %s, ignoring this sector.",
            CH(physicalTrackContext.cylHead.cyl, physicalTrackContext.cylHead.head), CHR(sectorIdInPhysicalTrack.m_cyl, sectorIdInPhysicalTrack.m_head, sectorIdInPhysicalTrack.m_sector));
        orphanDataCapableTrack.cylheadMismatch = true;
    }
    else
    {
        const Header header = sectorIdInPhysicalTrack.AsHeader();
        Sector sector(physicalTrackContext.dataRate, encoding, header);

        sector.offset = DataBitPositionAsBitOffset(byteBitPositionIDAM.TotalBitPosition()); // Counted in mfmbits.
        sector.set_badidcrc(badCrc);
        sector.set_constant_disk(false);
        if (opt_debug)
            util::cout << "physical_track_mfm_fm " << encoding << " IDAM (am=" << addressMark << ", id=" << header.sector << ") at offset " << sector.offset << "\n";
        orphanDataCapableTrack.track.add(std::move(sector));
    }
}
// ====================================

/*static*/ void SectorDataRefInPhysicalTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                     BitPositionableByteVector& physicalTrackContent,
                                                     const PhysicalTrackContext& physicalTrackContext,
                                                     const Encoding& encoding)
{
    const auto byteBitPositionDAM = physicalTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorDataRefInPhysicalTrack);
    Data sectorDataRefInPhysicalTrackBytes(requiredByteLength);
    physicalTrackContent.ReadBytes(sectorDataRefInPhysicalTrackBytes);

    const auto sectorDataRefInPhysicalTrack = *reinterpret_cast<SectorDataRefInPhysicalTrack*>(sectorDataRefInPhysicalTrackBytes.data());
    const auto addressMark = sectorDataRefInPhysicalTrack.m_addressMark;
    const Header header(physicalTrackContext.cylHead.cyl, physicalTrackContext.cylHead.head, OrphanDataCapableTrack::ORPHAN_SECTOR_ID, SIZECODE_UNKNOWN);
    Sector sector(physicalTrackContext.dataRate, encoding, header);
    sector.offset = DataBitPositionAsBitOffset(byteBitPositionDAM.TotalBitPosition()); // Counted in mfmbits.
    sector.set_constant_disk(false);
    if (opt_debug)
        util::cout << "physical_track_mfm_fm " << encoding << " DAM (am=" << addressMark << ") at offset " << sector.offset << "\n";
    orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
}
// ====================================

/*static*/ void SectorDataFromPhysicalTrack::ProcessInto(Sector& sector, BitPositionableByteVector& physicalTrackContent,
                                                    const Encoding& encoding,
                                                    const int nextIdamOffset/* = 0*/, const int nextDamOffset/* = 0*/)
{
    const auto byteBitPositionDAM = physicalTrackContent.GetByteBitPosition();
    if (sector.header.size != SIZECODE_UNKNOWN)
    {
        const auto dataSize = sector.size();
        const auto requiredByteLength = PhysicalSizeOf(dataSize);
        Data sectorDataInPhysicalTrackBytes(requiredByteLength);
        physicalTrackContent.ReadBytes(sectorDataInPhysicalTrackBytes);
        const SectorDataFromPhysicalTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInPhysicalTrackBytes), true);
        const bool data_crc_error = sectorData.badCrc;
        const auto dam = sectorData.addressMark;
        sector.add_with_readstats(sectorData.GetData(), data_crc_error, dam);
    }
    else
    {   // Sector id not found, setting orphan's data from DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
        // Determine available bytes.
        auto& orphanSector = sector;
        const auto remainingByteLength = physicalTrackContent.RemainingByteLength();
        auto availableByteLength = remainingByteLength;
        if (nextIdamOffset > 0)
        {
            const auto availableByteLengthUntilNextIdam = BitOffsetAsDataBytePosition(nextIdamOffset - orphanSector.offset)
                    - GetIdamOverhead(encoding);
            if (availableByteLengthUntilNextIdam < availableByteLength)
                availableByteLength = availableByteLengthUntilNextIdam;
        }
        if (nextDamOffset > 0)
        {
            const auto availableByteLengthUntilNextDam = BitOffsetAsDataBytePosition(nextDamOffset - orphanSector.offset)
                    - GetDamOverhead(encoding);
            if (availableByteLengthUntilNextDam < availableByteLength)
                availableByteLength = availableByteLengthUntilNextDam;
        }
        // Read the bytes, create sector data then add its data to orphan sector.
        Data sectorDataInPhysicalTrackBytes(availableByteLength);
        physicalTrackContent.ReadBytes(sectorDataInPhysicalTrackBytes);
        SectorDataFromPhysicalTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInPhysicalTrackBytes), false);
        const bool data_crc_error = sectorData.badCrc; // Always true in this case (because size is unknown).
        const auto dam = sectorData.addressMark;
        if (opt_debug)
            util::cout << "physical_track_mfm_fm " << encoding << " DAM (am=" << dam << ") at offset " << orphanSector.offset << " without IDAM\n";
        orphanSector.add_with_readstats(std::move(sectorData.physicalData), data_crc_error, dam);
    }
}
// ====================================

/*static*/ Data SectorDataFromPhysicalTrack::GetGoodDataUpToSize(const Sector& physicalSector, const int sectorSize)
{
    const auto& physicalData = physicalSector.data_copy();
    auto physicalDataSize = physicalData.size();
    const auto physicalSectorSize = SectorDataFromPhysicalTrack::PhysicalSizeOf(sectorSize);
    if (physicalDataSize > physicalSectorSize) // Not using more data than requested.
        physicalDataSize = physicalSectorSize;
    /* TODO If the physical data size is less than physical sector size then the data ends at next AM or track end.
     * It means the data contains gap3 and sync thus its crc will be bad.
     * I am not sure which bytes the FDC reads latest but theoretically we could find the end of good data
     * by calculating crc for each data length and if it becomes 0 then probably we found the correct length,
     * and the last two bytes are the crc.
     */
    if (CalculateCrcIsBad(physicalSector.encoding, physicalData, physicalDataSize))
        return Data();
    return GetData(physicalData, physicalDataSize);
}
// ====================================

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
    return BitBuffer(dataRate, rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesBitSize());
}

// ====================================

void PhysicalTrackMFM::ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack, const PhysicalTrackContext& physicalTrackContext)
{
    const auto sectorIdsIndexSup = orphanDataCapableTrack.track.size();
    auto sectorIdsIndex = 0;
    auto orphanIt = orphanDataCapableTrack.orphanDataTrack.begin();
    while (orphanIt != orphanDataCapableTrack.orphanDataTrack.end())
    {
        auto& orphanSector = *orphanIt;
        m_physicalTrackContent.SetByteBitPosition(BitOffsetAsDataBitPosition(orphanSector.offset)); // mfmbits to databits

        auto parentSectorIndexFound = false;
        int sectorOffset;
        // Find the closest sector id which coheres.
        while (sectorIdsIndex < sectorIdsIndexSup && (sectorOffset = orphanDataCapableTrack.track[sectorIdsIndex].offset) < orphanSector.offset)
        {
            if (physicalTrackContext.DoSectorIdAndDataOffsetsCohere(sectorOffset, orphanSector.offset, orphanSector.encoding))
                parentSectorIndexFound = true;
            sectorIdsIndex++;
        }
        if (parentSectorIndexFound) // Data belongs to sector id thus its size is provided by the sector id.
        {
            auto& sector = orphanDataCapableTrack.track[sectorIdsIndex - 1]; // The previous is found.
            const auto dataSize = sector.header.size;
            const auto availableBytes = m_physicalTrackContent.RemainingByteLength();
            if (!SectorDataFromPhysicalTrack::IsSuitable(dataSize, availableBytes))
                goto NextOrphan; // Not enough bytes thus crc is bad, and we do not provide bad data from physical track.
            SectorDataFromPhysicalTrack::ProcessInto(sector, m_physicalTrackContent, orphanSector.encoding);
            orphanIt = orphanDataCapableTrack.orphanDataTrack.sectors().erase(orphanIt);
            continue; // Continuing from current orphan which was the next orphan before erasing.
        }
        else
        {   // Sector id not found, setting orphan's data from first byte after DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
            SectorDataFromPhysicalTrack::ProcessInto(orphanSector, m_physicalTrackContent, orphanSector.encoding,
                                                sectorIdsIndex < sectorIdsIndexSup ? orphanDataCapableTrack.track[sectorIdsIndex].offset : 0,
                                                (orphanIt + 1) != orphanDataCapableTrack.orphanDataTrack.end() ? (orphanIt + 1)->offset : 0);
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
OrphanDataCapableTrack PhysicalTrackMFM::DecodeTrack(const CylHead& cylHead)
{
    const auto encoding = Encoding::MFM; // Now only MFM encoding is supported. Decoding a FM track has high false-positive risk.
    OrphanDataCapableTrack orphanDataCapableTrack;
    const PhysicalTrackContext physicalTrackContext(cylHead, dataRate);

    const auto readLengthMin = intsizeof(AddressMarkSyncInTrack);
    Data addressMarkSyncInPhysicalTrackBytes(readLengthMin);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(addressMarkSyncInPhysicalTrackBytes.data());
    while (m_physicalTrackContent.RemainingByteLength() >= readLengthMin)
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
                    TrackIndexInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, encoding);
                    continue;
                }
                else if (SectorIdInPhysicalTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorIdInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, encoding);
                    continue;
                }
                else if (SectorDataRefInPhysicalTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorDataRefInPhysicalTrack::ProcessInto(orphanDataCapableTrack, m_physicalTrackContent, physicalTrackContext, encoding);
                    continue;
                }
            }
            m_physicalTrackContent.SetByteBitPosition(byteBitPosition);
        }
        m_physicalTrackContent.StepBit();
    }

    if (!orphanDataCapableTrack.empty())
    {
        orphanDataCapableTrack.setTrackLen(DataBitPositionAsBitOffset(m_physicalTrackContent.BytesBitSize())); // Counted in mfmbits.
        ProcessSectorDataRefs(orphanDataCapableTrack, physicalTrackContext);
    }
    return orphanDataCapableTrack;
}
// ====================================

OrphanDataCapableTrack PhysicalTrackMFM::DecodeTrack(const CylHead& cylHead) const
{
    auto physicalTrack = *this;
    return physicalTrack.DecodeTrack(cylHead);
}

//---------------------------------------------------------------------------
