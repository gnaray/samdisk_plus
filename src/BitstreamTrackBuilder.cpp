// Buffer for assembling bitstream data (incomplete)

#include "BitstreamTrackBuilder.h"

#define INIT_BITSIZE   5000000

BitstreamTrackBuilder::BitstreamTrackBuilder(DataRate datarate, Encoding encoding)
    : TrackBuilder(datarate, encoding), m_buffer(datarate, encoding)
{
}

int BitstreamTrackBuilder::size() const
{
    return m_buffer.size();
}

void BitstreamTrackBuilder::setEncoding(Encoding encoding)
{
    TrackBuilder::setEncoding(encoding);
    m_buffer.encoding = encoding;
}

void BitstreamTrackBuilder::addRawBit(bool bit)
{
    m_buffer.add(bit);
}

int BitstreamTrackBuilder::gapPreIDAMBits(const bool short_mfm_gap/* = false*/) const
{
    return DataBytePositionAsBitOffset(getSyncLength(short_mfm_gap) + GetIdamOverheadSyncOverhead(m_buffer.encoding));
}

void BitstreamTrackBuilder::adjustDataBitsBeforeOffset(const int sectorOffset, const int gap3_bytes/* = 0*/, const bool short_mfm_gap/* = false*/)
{
    if (sectorOffset > m_prevSectorOffset)
    {
        const auto currentBitpos = rawOffsetToOffset(m_buffer.tell());
        const auto sectorBitpos = sectorOffset - gapPreIDAMBits(short_mfm_gap);
        auto missingBitsToAdd = sectorBitpos - currentBitpos;
        if (missingBitsToAdd > 0)
        {
            const auto gap3Bits = DataBytePositionAsBitOffset(std::min(BitOffsetAsDataBytePosition(missingBitsToAdd), gap3_bytes));
            while (missingBitsToAdd > gap3Bits)
            {
                addBit(true);
                missingBitsToAdd--;
            }
            if (missingBitsToAdd == gap3Bits && gap3Bits >= DataBytePositionAsBitOffset(1))
                addGap(BitOffsetAsDataBytePosition(gap3Bits));
        }
        else
        {
            const auto idWithGap3Bits = DataBytePositionAsBitOffset(GetIdOverheadWithoutIdamOverheadSyncOverhead(m_buffer.encoding) + gap3_bytes);
            const auto protectedBitpos = m_prevSectorOffset + idWithGap3Bits;
            m_buffer.seek(offsetToRawOffset(std::max(sectorBitpos, protectedBitpos)));
        }
        m_prevSectorOffset = sectorOffset;
    }
}

void BitstreamTrackBuilder::justAddedImportantBits()
{
    m_afterLastImportantRawBitPosition = m_buffer.tell();
}

void BitstreamTrackBuilder::cutExcessUnimportantDataBitsAtTheEnd(const int trackLen)
{
    const auto currentBitpos = rawOffsetToOffset(m_buffer.tell());
    auto excessBits = currentBitpos - trackLen;
    if (excessBits > 0)
    {
        const auto protectedBitpos = rawOffsetToOffset(m_afterLastImportantRawBitPosition);
        if (protectedBitpos < currentBitpos)
        {
            if (protectedBitpos > trackLen)
                excessBits -= protectedBitpos - trackLen;
            m_buffer.remove(offsetToRawOffset(excessBits));
        }
    }
}

void BitstreamTrackBuilder::addIAM()
{
    TrackBuilder::addIAM();
    m_iamOffset = rawOffsetToOffset(m_buffer.tell()) - DataBytePositionAsBitOffset(1);
}

int BitstreamTrackBuilder::getIAMPosition() const
{
    // m_iamOffset is not always set, for example in case of short mfm gap or amiga encoding.
    return m_iamOffset > 0 ? m_iamOffset : rawOffsetToOffset(m_buffer.tell());
}

void BitstreamTrackBuilder::addCrc(int size)
{
    auto old_bitpos{ m_buffer.tell() };
    auto byte_rawbits = bitRawBits() * DataBytePositionAsBitOffset(1);
    assert(old_bitpos >= size * byte_rawbits);
    m_buffer.seek(old_bitpos - size * byte_rawbits);

    CRC16 crc{};
    while (size-- > 0)
        crc.add(m_buffer.read_byte());

    // Seek back to the starting position to write the CRC.
    m_buffer.seek(old_bitpos);
    addByte(crc >> 8);
    addByte(crc & 0xff);
}

BitBuffer& BitstreamTrackBuilder::buffer()
{
    return m_buffer;
}

DataRate BitstreamTrackBuilder::datarate() const
{
    return m_buffer.datarate;
}

Encoding BitstreamTrackBuilder::encoding() const
{
    return m_buffer.encoding;
}
