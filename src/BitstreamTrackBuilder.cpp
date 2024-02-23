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
    return DataBytePositionAsBitOffset(getSyncLength(short_mfm_gap) + GetIdamOverheadSyncOverhead(m_buffer.encoding), m_buffer.encoding);
}

void BitstreamTrackBuilder::adjustDataBitsBeforeOffset(const int sectorOffset, const int gap3_bytes/* = 0*/, const bool short_mfm_gap/* = false*/)
{
    if (sectorOffset > m_prevSectorOffset)
    {
        const auto currentBitpos = m_buffer.tell();
        const auto sectorBitpos = sectorOffset - gapPreIDAMBits(short_mfm_gap);
        auto missingBitsToAdd = sectorBitpos - currentBitpos;
        if (missingBitsToAdd > 0)
        {
            const auto gap3Bits = DataBytePositionAsBitOffset(std::min(BitOffsetAsDataBytePosition(missingBitsToAdd, m_buffer.encoding), gap3_bytes), m_buffer.encoding);
            while (missingBitsToAdd > gap3Bits)
            {
                addBit(true);
                missingBitsToAdd -= sectorBitpos - m_buffer.tell();
            }
            if (missingBitsToAdd == gap3Bits && gap3Bits >= DataBytePositionAsBitOffset(1, m_buffer.encoding))
                addGap(BitOffsetAsDataBytePosition(gap3Bits, m_buffer.encoding));
        }
        else
        {
            const auto idWithGap3Bits = DataBytePositionAsBitOffset(GetIdOverheadWithoutIdamOverheadSyncOverhead(m_buffer.encoding) + gap3_bytes, m_buffer.encoding);
            const auto protectedBitpos = m_prevSectorOffset + idWithGap3Bits;
            m_buffer.seek(std::max(sectorBitpos, protectedBitpos));
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
    const auto currentBitpos = m_buffer.tell();
    auto excessBits = currentBitpos - trackLen;
    if (excessBits > 0)
    {
        const auto protectedBitpos = m_afterLastImportantRawBitPosition;
        if (protectedBitpos < currentBitpos)
        {
            if (protectedBitpos > trackLen)
                excessBits -= protectedBitpos - trackLen;
            m_buffer.remove(excessBits);
        }
    }
}

void BitstreamTrackBuilder::addIAM()
{
    TrackBuilder::addIAM();
    m_iamOffset = m_buffer.tell() - DataBytePositionAsBitOffset(1, m_buffer.encoding);
}

int BitstreamTrackBuilder::getIAMPosition() const
{
    // m_iamOffset is not always set, for example in case of short mfm gap or amiga encoding.
    return m_iamOffset > 0 ? m_iamOffset : m_buffer.tell();
}

void BitstreamTrackBuilder::addCrc(int size)
{
    auto old_bitpos = m_buffer.tell();
    auto byte_rawbits = DataBytePositionAsBitOffset(1, m_buffer.encoding);
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
