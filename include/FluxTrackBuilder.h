#pragma once

#include "TrackBuilder.h"

class FluxTrackBuilder final : public TrackBuilder
{
public:
    FluxTrackBuilder(const CylHead& cylhead, DataRate datarate, Encoding encoding);

    void addRawBit(bool one) override;
    void adjustDataBitsBeforeOffset(const int sectorOffset, const int gap3_bytes = 0, const bool short_mfm_gap = false) override;
    void justAddedImportantBits() override;
    void addWeakBlock(int length);

    VectorX<uint32_t>& buffer();

    static const int PRECOMP_NS{ 240 };

private:
    CylHead m_cylhead{};
    VectorX<uint32_t> m_flux_times{};
    uint32_t m_bitcell_ns = 0;
    uint32_t m_flux_time = 0;
    bool m_last_bit = false;
    bool m_curr_bit = false ;
};
