#include "MultiScanResult.h"
#include "Options.h"

static auto& opt_normal_disk = getOpt<bool>("normal_disk");

Track MultiScanResult::DecodeResult(const CylHead& cylhead, const DataRate& dataRate, const Encoding& encoding) const
{
    Track timedTrack;
    const auto mfmbit_us = GetFmOrMfmBitsTime(dataRate);
    timedTrack.tracktime = trackTime();
    timedTrack.tracklen = round_AS<int>(timedTrack.tracktime / mfmbit_us);

    const auto offsetAdjuster = FdrawMeasuredOffsetAdjuster(encoding);
    const int iSup = count();
    for (int i = 0; i < iSup; ++i)
    {
        const auto& scan_header = HeaderArray(i);
        Header header(scan_header.cyl, scan_header.head, scan_header.sector, scan_header.size);
        VerifyCylHeadsMatch(cylhead, header, false, opt_normal_disk);
        Sector sector(dataRate, encoding, header);
        sector.offset = modulo(round_AS<int>(static_cast<int>(scan_header.reltime) / mfmbit_us - offsetAdjuster), timedTrack.tracklen);
        sector.MakeOffsetNot0(false);
        sector.revolution = scan_header.revolution;
        sector.set_constant_disk(false);
        timedTrack.add(std::move(sector));
    }
    return timedTrack;
}
