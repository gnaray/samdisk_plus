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
    void adjustDataBitsBeforeOffset(const int sectorOffset, const int gap3_bytes = 0, const bool short_mfm_gap = false) override;
    int gapPreIDAMBits(const bool short_mfm_gap = false) const;
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
};
