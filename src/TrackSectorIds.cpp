#include "TrackSectorIds.h"

#include <iterator>

/*static*/ TrackSectorIds TrackSectorIds::GetIds(const CylHead& cylhead, const int sectors, const int interleave/* = 0*/, const int skew/* = 0*/, const int offset/* = 0*/, const int base/* = 1*/)
{
    TrackSectorIds ids;
    if (sectors == 0)
        return ids;
    assert(sectors > 0 && interleave >= 0 && base >= 0);
    ids.resize(sectors);
    VectorX<bool> used(sectors);

    const auto base_id = base;

    for (auto s = 0; s < sectors; ++s)
    {
        // Calculate the expected sector index using the interleave and skew
        auto index = modulo(offset + s * interleave + skew * (cylhead.cyl), static_cast<unsigned>(sectors));

        // Find a free slot starting from the expected position
        for (; used[index]; index = (index + 1) % sectors) ;
        used[index] = 1;

        // Assign the sector number, with offset adjustments
        ids[index] = base_id + s;
    }

    return ids;
}

/*static*/ const TrackSectorIds& TrackSectorIds::GetTrackSectorIds(int sectors, int interleave)
{
    static VectorX<VectorX<TrackSectorIds>> TrackSectorIdsCache;

    if (sectors >= TrackSectorIdsCache.size())
        TrackSectorIdsCache.resize(sectors + 1);
    if (interleave >= TrackSectorIdsCache[sectors].size())
        TrackSectorIdsCache[sectors].resize(interleave + 1);
    if (TrackSectorIdsCache[sectors][interleave].empty())
        TrackSectorIdsCache[sectors][interleave] = GetIds(CylHead(0, 0), sectors, interleave);
    return TrackSectorIdsCache[sectors][interleave];
}

/*static*/ const TrackSectorIds TrackSectorIds::FindCompleteTrackSectorIdsFor(const TrackSectorIds& incompleteSectorIds, const int sectorsMin/* = 0*/)
{
    auto sectorsMinLocal = sectorsMin;
    if (sectorsMin == 0)
    {
        const auto foundLastValidId = std::find_if(incompleteSectorIds.rbegin(), incompleteSectorIds.rend(),
                                                 [](int id) { return id >= 0; });
        sectorsMinLocal = std::max(static_cast<int>(incompleteSectorIds.rend() - foundLastValidId),
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
                if (!resultCompleteTrackSectorIds.empty() && !resultCompleteTrackSectorIds.ExtendableTo(trackSectorIds)) // Ambiguous result, not extendable.
                        return TrackSectorIds{}; // Return empty.
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
    constexpr int resultNoOffset = -1; // Return value if no match.
    const int idIndexSup = static_cast<int>(incompleteSectorIds.size());
    const int thisIdIndexSup = static_cast<int>(size());
    // Find a valid sector ID in track sector ids, it must exist there.
    for (int idIndex = 0; idIndex < idIndexSup; idIndex++)
    {
        const auto id = incompleteSectorIds[idIndex];
        if (id >= 0)
        {
            for (int thisIdIndex = 0; thisIdIndex < thisIdIndexSup; thisIdIndex++)
            {
                if (operator[](thisIdIndex) == id)
                {
                    // Now sector ID is found in track sector ids, determine the offset and compare the rest.
                    const auto offset = modulo(idIndex - thisIdIndex, static_cast<unsigned>(thisIdIndexSup));
                    for (++idIndex; idIndex < idIndexSup; idIndex++)
                    {
                        if (incompleteSectorIds[idIndex] >= 0 && incompleteSectorIds[idIndex] !=
                                operator[](modulo(idIndex - offset, static_cast<unsigned>(thisIdIndexSup))))
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

bool TrackSectorIds::ExtendableTo(const TrackSectorIds& sectorIds) const
{
    if (size() > sectorIds.size())
        return false;
    return std::mismatch(cbegin(), cend(), sectorIds.cbegin()).first == cend();
}



TrackSectorIds IdAndOffsetPairs::GetSectorIds() const
{
    TrackSectorIds sectorIds;
    std::transform(begin(), end(), std::back_inserter(sectorIds),
                   [](const IdAndOffset& idAndOffset)
    { return idAndOffset.id; });
    return sectorIds;
}

void IdAndOffsetPairs::ReplaceMissingSectorIdsFrom(const TrackSectorIds& trackSectorIds)
{
    assert(trackSectorIds.size() >= size());

    const auto iSup = static_cast<int>(size());
    for (int i = 0; i < iSup; i++)
    {
        if (data()[i].id < 0)
            data()[i].id = trackSectorIds[i];
    }
}

bool IdAndOffsetPairs::ReplaceMissingIdsByFindingTrackSectorIds(const int sectorsMin/* = 0*/)
{
    const auto completeTrackSectorIds = TrackSectorIds::FindCompleteTrackSectorIdsFor(GetSectorIds(), sectorsMin);
    if (completeTrackSectorIds.empty())
        return false;
    ReplaceMissingSectorIdsFrom(completeTrackSectorIds);
    return true;
}
