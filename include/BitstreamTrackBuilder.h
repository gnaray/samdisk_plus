#pragma once

#include "BitBuffer.h"
#include "TrackBuilder.h"

class BitstreamTrackBuilder final : public TrackBuilder
{
public:
    BitstreamTrackBuilder(DataRate datarate, Encoding encoding);

    int size() const;
    void setEncoding(Encoding encoding) override;
    void addRawBit(bool bit) override;
    inline constexpr int bitRawBits() const
    {
        return (m_buffer.encoding == Encoding::FM) ? 2 : 1;
    }
    inline constexpr int rawOffsetToOffset(const int rawOffset) const
    {
        return rawOffset / bitRawBits();
    }
    inline constexpr int offsetToRawOffset(const int offset) const
    {
        return offset * bitRawBits();
    }
    int gapPreIDAMBits(const bool short_mfm_gap = false) const;
    void adjustDataBitsBeforeOffset(const int sectorOffset, const int gap3_bytes = 0, const bool short_mfm_gap = false) override;
    void justAddedImportantBits() override;
    void cutExcessUnimportantDataBitsAtTheEnd(const int trackLen);
    void addIAM() override;
    int getIAMPosition() const;
    void addCrc(int size);

    BitBuffer& buffer();
    DataRate datarate() const;
    Encoding encoding() const;

private:
    BitBuffer m_buffer;
    int m_iamOffset = 0;
    int m_prevSectorOffset = 0; // Always must be >= 0.
    int m_afterLastImportantRawBitPosition = 0;
};
