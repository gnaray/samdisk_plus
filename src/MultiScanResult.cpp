#include "MultiScanResult.h"
#include "Options.h"

static auto& opt_normal_disk = getOpt<bool>("normal_disk");

Track MultiScanResult::DecodeResult(const CylHead& cylhead, const DataRate& dataRate, const Encoding& encoding, int trackLen/* = 0*/) const
{
    Track timedTrack;
    const auto mfmbit_us = GetFmOrMfmBitsTime(dataRate, encoding);
    if (trackLen > 0)
    {
        timedTrack.tracklen = trackLen;
        timedTrack.tracktime = round_AS<int>(trackLen * mfmbit_us);
    }
    else
    {
        timedTrack.tracktime = trackTime();
        timedTrack.tracklen = round_AS<int>(timedTrack.tracktime / mfmbit_us);
    }

    const int iSup = count();
    for (int i = 0; i < iSup; ++i)
    {
        const auto& scan_header = HeaderArray(i);
        Header header(scan_header.cyl, scan_header.head, scan_header.sector, scan_header.size);
        VerifyCylHeadsMatch(opt_normal_disk, false, cylhead, header, false);
        Sector sector(dataRate, encoding, header);

        sector.offset = round_AS<int>(scan_header.reltime / mfmbit_us);
        sector.revolution = scan_header.revolution;
        sector.set_constant_disk(false);
        timedTrack.add(std::move(sector));
    }
    return timedTrack;
}
