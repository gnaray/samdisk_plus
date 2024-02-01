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
        const ByteBitPosition& sectorIdByteBitPosition, const ByteBitPosition& dataByteBitPosition, const Encoding& encoding) const
{
    if (sectorIdByteBitPosition >= dataByteBitPosition) // The data must be behind the sector id.
        return false;
    // This code is taken from Samdisk/BitstreamDecoder where every databit is
    // stored as two bits (in addition every FM encoded bit is stored as two rawbits).
    // We also calculate with bits here though the code is slightly modified.
    const auto gap2_size_min = GetFmOrMfmGap2Length(dataRate, encoding);
    const auto idam_am_distance = GetFmOrMfmIdamAndAmDistance(dataRate, encoding);
    const auto min_distance = DataBytePositionAsBitOffset(GetIdOverheadWithoutIdamOverheadSyncOverhead(encoding) + gap2_size_min); // IDAM, ID, gap2 (without sync and DAM.a1sync, why?)
    const auto max_distance = DataBytePositionAsBitOffset(idam_am_distance + 8); // IDAM, ID, gap2, sync, DAM.a1sync (gap2: WD177x offset, +8: gap2 may be longer when formatted by different type of controller)

    const auto sectorIdAndDataPositionDistance = static_cast<int>((dataByteBitPosition - sectorIdByteBitPosition).TotalBitPosition());
    return sectorIdAndDataPositionDistance >= min_distance && sectorIdAndDataPositionDistance <= max_distance;
}
// ====================================

/*static*/ void TrackIndexInRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                  BitPositionableByteVector& rawTrackContent,
                                                  const RawTrackContext& /*rawTrackContext*/,
                                                  const Encoding& encoding)
{
    const auto byteBitPositionIAM = rawTrackContent.GetByteBitPosition();
    if (opt_debug)
        util::cout << "raw_track_mfm_fm " << encoding << " IAM at offset " << byteBitPositionIAM.TotalBitPosition() * 2;
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

/*static*/ void SectorIdInRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                BitPositionableByteVector& rawTrackContent,
                                                const RawTrackContext& rawTrackContext,
                                                const Encoding& encoding)
{
    const auto byteBitPositionIDAM = rawTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorIdInRawTrack);
    Data sectorIdInRawTrackBytes(requiredByteLength);
    rawTrackContent.ReadBytes(sectorIdInRawTrackBytes);

    const auto sectorIdInRawTrack = *reinterpret_cast<SectorIdInRawTrack*>(sectorIdInRawTrackBytes.data());
    const auto addressMark = sectorIdInRawTrack.m_addressMark;
    const auto badCrc = sectorIdInRawTrack.CalculateCrc() != 0;

    if (opt_normal_disk && !badCrc
            && (sectorIdInRawTrack.m_cyl != rawTrackContext.cylHead.cyl || sectorIdInRawTrack.m_head != rawTrackContext.cylHead.head))
    {
        Message(msgWarning, "ReadHeaders: track's %s does not match sector's %s, ignoring this sector.",
            CH(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head), CHR(sectorIdInRawTrack.m_cyl, sectorIdInRawTrack.m_head, sectorIdInRawTrack.m_sector));
        orphanDataCapableTrack.cylheadMismatch = true;
    }
    else
    {
        const Header header = sectorIdInRawTrack.AsHeader();
        Sector sector(rawTrackContext.dataRate, encoding, header);

        sector.offset = DataBitPositionAsBitOffset(byteBitPositionIDAM.TotalBitPosition()); // Counted in mfmbits.
        sector.set_badidcrc(badCrc);
        sector.set_constant_disk(false);
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << encoding << " IDAM (am=" << addressMark << ", id=" << header.sector << ") at offset " << sector.offset << "\n";
        orphanDataCapableTrack.track.add(std::move(sector));
    }
}
// ====================================

/*static*/ void SectorDataRefInRawTrack::ProcessInto(OrphanDataCapableTrack& orphanDataCapableTrack,
                                                     BitPositionableByteVector& rawTrackContent,
                                                     const RawTrackContext& rawTrackContext,
                                                     const Encoding& encoding)
{
    const auto byteBitPositionDAM = rawTrackContent.GetByteBitPosition();
    const auto requiredByteLength = intsizeof(SectorDataRefInRawTrack);
    Data sectorDataRefInRawTrackBytes(requiredByteLength);
    rawTrackContent.ReadBytes(sectorDataRefInRawTrackBytes);

    const auto sectorDataRefInRawTrack = *reinterpret_cast<SectorDataRefInRawTrack*>(sectorDataRefInRawTrackBytes.data());
    const auto addressMark = sectorDataRefInRawTrack.m_addressMark;
    const Header header(rawTrackContext.cylHead.cyl, rawTrackContext.cylHead.head, OrphanDataCapableTrack::ORPHAN_SECTOR_ID, SIZECODE_UNKNOWN);
    Sector sector(rawTrackContext.dataRate, encoding, header);
    sector.offset = DataBitPositionAsBitOffset(byteBitPositionDAM.TotalBitPosition()); // Counted in mfmbits.
    sector.set_constant_disk(false);
    if (opt_debug)
        util::cout << "raw_track_mfm_fm " << encoding << " DAM (am=" << addressMark << ") at offset " << sector.offset << "\n";
    orphanDataCapableTrack.orphanDataTrack.add(std::move(sector));
}
// ====================================

/*static*/ void SectorDataFromRawTrack::ProcessInto(Sector& sector, BitPositionableByteVector& rawTrackContent,
                                                    const Encoding& encoding,
                                                    const int nextIdamOffset/* = 0*/, const int nextDamOffset/* = 0*/)
{
    const auto byteBitPositionDAM = rawTrackContent.GetByteBitPosition();
    if (sector.header.size != SIZECODE_UNKNOWN)
    {
        const auto dataSize = sector.size();
        const auto requiredByteLength = RawSizeOf(dataSize);
        Data sectorDataInRawTrackBytes(requiredByteLength);
        rawTrackContent.ReadBytes(sectorDataInRawTrackBytes);
        const SectorDataFromRawTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInRawTrackBytes), true);
        const bool data_crc_error = sectorData.badCrc;
        const auto dam = sectorData.addressMark;
        sector.add_with_readstats(sectorData.GetData(), data_crc_error, dam);
    }
    else
    {   // Sector id not found, setting orphan's data from DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
        // Determine available bytes.
        auto& orphanSector = sector;
        const auto remainingByteLength = rawTrackContent.RemainingByteLength();
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
        Data sectorDataInRawTrackBytes(availableByteLength);
        rawTrackContent.ReadBytes(sectorDataInRawTrackBytes);
        SectorDataFromRawTrack sectorData(encoding, byteBitPositionDAM, std::move(sectorDataInRawTrackBytes), false);
        const bool data_crc_error = sectorData.badCrc; // Always true in this case (because size is unknown).
        const auto dam = sectorData.addressMark;
        if (opt_debug)
            util::cout << "raw_track_mfm_fm " << encoding << " DAM (am=" << dam << ") at offset " << orphanSector.offset << " without IDAM\n";
        orphanSector.add_with_readstats(std::move(sectorData.rawData), data_crc_error, dam);
    }
}
// ====================================

/*static*/ Data SectorDataFromRawTrack::GetGoodDataUpToSize(const Sector& rawSector, const int sectorSize)
{
    const auto& rawData = rawSector.data_copy();
    auto rawDataSize = rawData.size();
    const auto rawSectorSize = SectorDataFromRawTrack::RawSizeOf(sectorSize);
    if (rawDataSize > rawSectorSize) // Not using more data than requested.
        rawDataSize = rawSectorSize;
    if (CalculateCrcIsBad(rawSector.encoding, rawData, rawDataSize))
        return Data();
    return GetData(rawData, rawDataSize);
}
// ====================================

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
BitBuffer RawTrackMFM::AsMFMBitstream()
{
    const Data addressMarkBytes{0x44, 0x89, 0x44, 0x89, 0x44, 0x89}; // 0x4489 3 times in reverse bit order.
    const auto readLengthMin = intsizeof(AddressMarkSyncInTrack); // Looking for address mark sync only.
    BitPositionableByteVector rawTrackContentForBitBuffer;
    ByteBitPosition lastAddressMarkPosition(0);
    Data addressMarkSyncInRawTrackBytes(readLengthMin);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(addressMarkSyncInRawTrackBytes.data());
    while (m_rawTrackContent.RemainingByteLength() >= readLengthMin)
    {
        if (AddressMarkSyncInTrack::IsValid(m_rawTrackContent.PeekByte())) // Bit of optimisation for speed.
        {
            const auto byteBitPositionFound = m_rawTrackContent.GetByteBitPosition();
            m_rawTrackContent.ReadBytes(addressMarkSyncInRawTrackBytes);
            if (addressMarkSyncInTrack->IsValid() && AddressMarkInTrack::IsValid(m_rawTrackContent.PeekByte()))
            {
                const auto bitsLen = byteBitPositionFound - lastAddressMarkPosition;
                rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_rawTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);
                rawTrackContentForBitBuffer.WriteBytes(addressMarkBytes);
                lastAddressMarkPosition = m_rawTrackContent.GetByteBitPosition();
                continue;
            }
            else
                m_rawTrackContent.SetByteBitPosition(byteBitPositionFound);
        }
        m_rawTrackContent.StepBit();
    }
    const auto bitsLen = m_rawTrackContent.BytesBitEndPosition() - lastAddressMarkPosition;
    rawTrackContentForBitBuffer.CopyBitsDoubledFrom(m_rawTrackContent, bitsLen.TotalBitPosition(), &lastAddressMarkPosition);

    // Bitstream scanner expects bits like: 0.:76543210, 1.:76543210, etc. while our content has 0.:01234567, 1.:01234567, etc.
    util::bit_reverse(rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesByteSize());

    // BitBuffer's default encoding is MFM so no need to set it.
    return BitBuffer(dataRate, rawTrackContentForBitBuffer.Bytes().data(), rawTrackContentForBitBuffer.BytesBitSize());
}

// ====================================

void RawTrackMFM::ProcessSectorDataRefs(OrphanDataCapableTrack& orphanDataCapableTrack, const RawTrackContext& rawTrackContext)
{
    const auto sectorIdsIndexSup = orphanDataCapableTrack.track.size();
    auto sectorIdsIndex = 0;
    auto orphanIt = orphanDataCapableTrack.orphanDataTrack.begin();
    while (orphanIt != orphanDataCapableTrack.orphanDataTrack.end())
    {
        auto& orphanSector = *orphanIt;
        m_rawTrackContent.SetByteBitPosition(BitOffsetAsDataBitPosition(orphanSector.offset)); // mfmbits to databits

        auto parentSectorIndexFound = false;
        int sectorOffset;
        // Find the closest sector id which coheres.
        while (sectorIdsIndex < sectorIdsIndexSup && (sectorOffset = orphanDataCapableTrack.track[sectorIdsIndex].offset) < orphanSector.offset)
        {
            if (rawTrackContext.DoSectorIdAndDataPositionsCohere(sectorOffset, orphanSector.offset, orphanSector.encoding))
                parentSectorIndexFound = true;
            sectorIdsIndex++;
        }
        if (parentSectorIndexFound) // Data belongs to sector id thus its size is provided by the sector id.
        {
            auto& sector = orphanDataCapableTrack.track[sectorIdsIndex - 1]; // The previous is found.
            const auto dataSize = sector.header.size;
            const auto availableBytes = m_rawTrackContent.RemainingByteLength();
            if (!SectorDataFromRawTrack::IsSuitable(dataSize, availableBytes))
                goto NextOrphan; // Not enough bytes thus crc is bad, and we do not provide bad data from raw track.
            SectorDataFromRawTrack::ProcessInto(sector, m_rawTrackContent, orphanSector.encoding);
            orphanIt = orphanDataCapableTrack.orphanDataTrack.sectors().erase(orphanIt);
            continue; // Continuing from current orphan which was the next orphan before erasing.
        }
        else
        {   // Sector id not found, setting orphan's data from first byte after DAM until first overhead byte of next ?AM (or track end if there is no next ?AM).
            SectorDataFromRawTrack::ProcessInto(orphanSector, m_rawTrackContent, orphanSector.encoding,
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
OrphanDataCapableTrack RawTrackMFM::DecodeTrack(const CylHead& cylHead)
{
    const auto encoding = Encoding::MFM; // Now only MFM encoding is supported. Decoding a FM track has high false-positive risk.
    OrphanDataCapableTrack orphanDataCapableTrack;
    const RawTrackContext rawTrackContext(cylHead, dataRate);

    const auto readLengthMin = intsizeof(AddressMarkSyncInTrack);
    Data addressMarkSyncInRawTrackBytes(readLengthMin);
    const auto addressMarkSyncInTrack = reinterpret_cast<AddressMarkSyncInTrack*>(addressMarkSyncInRawTrackBytes.data());
    while (m_rawTrackContent.RemainingByteLength() >= readLengthMin)
    {
        if (AddressMarkSyncInTrack::IsValid(m_rawTrackContent.PeekByte())) // Bit of optimisation for speed.
        {
            auto byteBitPosition = m_rawTrackContent.GetByteBitPosition();
            m_rawTrackContent.ReadBytes(addressMarkSyncInRawTrackBytes);
            if (addressMarkSyncInTrack->IsValid())
            {
                const auto addressMarkValue = m_rawTrackContent.PeekByte();
                const auto availableBytes = m_rawTrackContent.RemainingByteLength();
                if (TrackIndexInRawTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    TrackIndexInRawTrack::ProcessInto(orphanDataCapableTrack, m_rawTrackContent, rawTrackContext, encoding);
                    continue;
                }
                else if (SectorIdInRawTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorIdInRawTrack::ProcessInto(orphanDataCapableTrack, m_rawTrackContent, rawTrackContext, encoding);
                    continue;
                }
                else if (SectorDataRefInRawTrack::IsSuitable(addressMarkValue, availableBytes))
                {
                    SectorDataRefInRawTrack::ProcessInto(orphanDataCapableTrack, m_rawTrackContent, rawTrackContext, encoding);
                    continue;
                }
            }
            m_rawTrackContent.SetByteBitPosition(byteBitPosition);
        }
        m_rawTrackContent.StepBit();
    }

    if (!orphanDataCapableTrack.empty())
    {
        orphanDataCapableTrack.setTrackLen(DataBitPositionAsBitOffset(m_rawTrackContent.BytesBitSize())); // Counted in mfmbits.
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
