#include "TrackSectorIds.h"

/*static*/ TrackSectorIds TrackSectorIds::GetIds(const CylHead& cylhead, const int sectors, const int interleave/* = 0*/, const int skew/* = 0*/, const int offset/* = 0*/, const int base/* = 1*/)
{
    const auto u_sectors = lossless_static_cast<size_t>(sectors);
    TrackSectorIds ids;
    if (sectors == 0)
        return ids;
    ids.resize(u_sectors);
    std::vector<bool> used(u_sectors);

    const auto base_id = base;

    for (auto s = 0; s < sectors; ++s)
    {
        // Calculate the expected sector index using the interleave and skew
        auto index = static_cast<size_t>((offset + s * interleave + skew * (cylhead.cyl)) % sectors);

        // Find a free slot starting from the expected position
        for (; used[index]; index = (index + 1) % u_sectors) ;
        used[index] = 1;

        // Assign the sector number, with offset adjustments
        ids[index] = base_id + s;
    }

    return ids;
}

/*static*/ const TrackSectorIds& TrackSectorIds::GetTrackSectorIds(int sectors, int interleave)
{
    static std::vector<std::vector<TrackSectorIds>> TrackSectorIdsCache;

    const auto u_sectors = lossless_static_cast<size_t>(sectors);
    const auto u_interleave = lossless_static_cast<size_t>(interleave);
    if (u_sectors >= TrackSectorIdsCache.size())
        TrackSectorIdsCache.resize(u_sectors + 1);
    if (u_interleave >= TrackSectorIdsCache[u_sectors].size())
        TrackSectorIdsCache[u_sectors].resize(u_interleave + 1);
    if (TrackSectorIdsCache[u_sectors][u_interleave].empty())
        TrackSectorIdsCache[u_sectors][u_interleave] = GetIds(CylHead(0, 0), sectors, interleave);
    return TrackSectorIdsCache[u_sectors][u_interleave];
}

/*static*/ const TrackSectorIds TrackSectorIds::FindCompleteTrackSectorIdsFor(const TrackSectorIds& incompleteSectorIds, const int sectorsMin/* = 0*/)
{
    auto sectorsMinLocal = sectorsMin;
    if (sectorsMin == 0)
    {
        const auto foundLastValidId = std::find_if(incompleteSectorIds.rbegin(), incompleteSectorIds.rend(),
                                                 [](int id) { return id >= 0; });
        // -1 + 1: -1 because reverse iterator gives next to found, +1 because size = found + 1.
        sectorsMinLocal = std::max(static_cast<int>(std::distance(foundLastValidId, incompleteSectorIds.rend())) - 1 + 1,
                                   *std::max_element(incompleteSectorIds.begin(), incompleteSectorIds.end()));
    }
    TrackSectorIds resultCompleteTrackSectorIds;
    const int sectorsSup = static_cast<int>(incompleteSectorIds.size()) + 1;
    for (int sectors = sectorsMinLocal; sectors < sectorsSup; sectors++)
    {
        for (int interleave = 1; interleave < sectors; interleave++)
        {
            const auto& trackSectorIds = GetTrackSectorIds(sectors, interleave);
            const auto offset = trackSectorIds.MatchSectorIds(incompleteSectorIds);
            if (offset >= 0)
            {
                if (!resultCompleteTrackSectorIds.empty()) // Ambiguous results.
                    return TrackSectorIds{}; // Returning empty.
                resultCompleteTrackSectorIds = trackSectorIds;
                if (offset > 0)
                    std::rotate(resultCompleteTrackSectorIds.rbegin(), resultCompleteTrackSectorIds.rbegin() + offset, resultCompleteTrackSectorIds.rend());
            }
        }
    }
    return resultCompleteTrackSectorIds;
}

int TrackSectorIds::MatchSectorIds(const TrackSectorIds& incompleteSectorIds) const
{
    typedef std::vector<int>::size_type SectorIdsST;
    constexpr int resultNoOffset = -1; // Return value if no match.
    const int sectorsSup = static_cast<int>(incompleteSectorIds.size());
    const int thisSectorsSup = static_cast<int>(size());
    // Find a valid sector ID in track sector ids, it must exist there.
    for (int sector = 0; sector < sectorsSup; sector++)
    {
        const auto u_sector = static_cast<SectorIdsST>(sector);
        const auto id = incompleteSectorIds[u_sector];
        if (id >= 0)
        {
            for (int thisSector = 0; thisSector < thisSectorsSup; thisSector++)
            {
                if (data()[static_cast<SectorIdsST>(thisSector)] == id)
                {
                    // Now sector ID is found in track sector ids, determine the offset and compare the rest.
                    const auto offset = (sector - thisSector + thisSectorsSup) % thisSectorsSup;
                    for (++sector; sector < sectorsSup; sector++)
                    {
                        const auto u_sector = static_cast<SectorIdsST>(sector);
                        if (incompleteSectorIds[u_sector] >= 0 && incompleteSectorIds[u_sector] !=
                                data()[static_cast<SectorIdsST>((sector - offset + thisSectorsSup) % thisSectorsSup)])
                            return resultNoOffset;
                    }
                    return offset; // Match!
                }
            }
            return resultNoOffset; // The sector ID not found, it must be out of normal range.
        }
    }
    return resultNoOffset; // All IDs are unknown, nothing to match.
}



TrackSectorIds IdAndOffsetVector::GetSectorIds() const
{
    TrackSectorIds sectorIds;
    std::transform(begin(), end(), std::back_inserter(sectorIds),
                   [](const IdAndOffset& idAndOffset)
    { return idAndOffset.id; });
    return sectorIds;
}

void IdAndOffsetVector::ReplaceMissingSectorIdsFrom(const TrackSectorIds& trackSectorIds)
{
    assert(trackSectorIds.size() >= size());

    const auto iSup = static_cast<int>(trackSectorIds.size());
    for (int i = 0; i < iSup; i++)
    {
        const auto u_i = static_cast<IdAndOffsetVectorST>(i);
        if (data()[u_i].id < 0)
            data()[u_i].id = trackSectorIds[u_i];
    }
}

bool IdAndOffsetVector::ReplaceMissingIdsByFindingTrackSectorIds(const int sectorsMin/* = 0*/)
{
    const auto completeTrackSectorIds = TrackSectorIds::FindCompleteTrackSectorIdsFor(GetSectorIds(), sectorsMin);
    if (completeTrackSectorIds.empty())
        return false;
    ReplaceMissingSectorIdsFrom(completeTrackSectorIds);
    return true;
}
