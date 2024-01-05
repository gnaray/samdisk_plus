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

void BitstreamTrackBuilder::adjustDataBitsBeforeOffset(int sectorOffset, int gap3_bytes/* = 0*/, bool short_mfm_gap/* = false*/)
{
    if (sectorOffset > m_prevSectorOffset)
    {
        const auto bitRawBits = (m_buffer.encoding == Encoding::FM) ? 2 : 1;
        const auto currentRawBitpos = m_buffer.tell();
        const auto gapPreIDAMBits = (getSyncLength(short_mfm_gap) + IDAM_OVERHEAD_MFM - 1) * 8 * 2; // The IDAM byte has the offset.
        const auto sectorBitpos = sectorOffset - gapPreIDAMBits;
        auto missingBitsToAdd = sectorBitpos - currentRawBitpos / bitRawBits;
        if (missingBitsToAdd > 0)
        {
            const auto gap3Bits = std::min(missingBitsToAdd, gap3_bytes * 8 * 2);
            while (missingBitsToAdd > gap3Bits)
            {
                addBit(true);
                missingBitsToAdd--;
            }
            if (missingBitsToAdd == gap3Bits && gap3Bits >= 16)
                addGap(gap3Bits / 8 / 2);
        }
        else
        {
            const auto idWithGap3Bits = (ID_OVERHEAD_MFM - (IDAM_OVERHEAD_MFM - 1) + gap3_bytes) * 8 * 2;
            const auto protectedBitpos = m_prevSectorOffset + idWithGap3Bits;
            m_buffer.seek(std::max(sectorBitpos, protectedBitpos) * bitRawBits);
        }
        m_prevSectorOffset = sectorOffset;
    }
}

int BitstreamTrackBuilder::gapPreIDAMBits(bool short_mfm_gap/* = false*/) const
{
    return (getSyncLength(short_mfm_gap) + IDAM_OVERHEAD_MFM - 1) * 8 * 2; // The IDAM byte has the offset.
}


void BitstreamTrackBuilder::addIAM()
{
    TrackBuilder::addIAM();
    const auto bitRawBits{ (m_buffer.encoding == Encoding::FM) ? 2 : 1 };
    m_iamOffset = m_buffer.tell() * bitRawBits - 8;
}

int BitstreamTrackBuilder::getIAMPosition() const
{
    const auto bitRawBits{ (m_buffer.encoding == Encoding::FM) ? 2 : 1 };
    // m_iamOffset is not always set, for example in case of short mfm gap or amiga encoding.
    return m_iamOffset > 0 ? m_iamOffset : m_buffer.tell() * bitRawBits;
}

void BitstreamTrackBuilder::addCrc(int size)
{
    auto old_bitpos{ m_buffer.tell() };
    auto byte_bits{ (m_buffer.encoding == Encoding::FM) ? 32 : 16 };
    assert(old_bitpos >= size * byte_bits);
    m_buffer.seek(old_bitpos - size * byte_bits);

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
