#pragma once

#include "Header.h"

class TrackSectorIds : public VectorX<int>
{
public:
    using VectorX<int>::vector;

    static TrackSectorIds GetIds(const CylHead& cylhead, const int sectors, const int interleave = 0, const int skew = 0, const int offset = 0, const int base = 1);
    static const TrackSectorIds& GetTrackSectorIds(int sectors, int interleave);
    static const TrackSectorIds FindCompleteTrackSectorIdsFor(const TrackSectorIds& incompleteSectorIds, const int sectorsMin = 0);

    int MatchSectorIds(const TrackSectorIds& sectorIds) const;

    std::string ToString(bool onlyRelevantData = true) const
    {
        std::ostringstream ss;
        ss << "size=" << size() << ", capacity=" << capacity();
        if (!onlyRelevantData)
        {
            bool writingStarted = false;
            ss << ", elements={";
            std::for_each(cbegin(), cend(), [&](const int id) {
                if (writingStarted)
                    ss << ' ';
                else
                    writingStarted = true;
                ss << id;
            });
            ss << "}";
        }
        return ss.str();
    }

    friend std::string to_string(const TrackSectorIds& trackSectorIds, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << trackSectorIds.ToString(onlyRelevantData);
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const TrackSectorIds& trackSectorIds) { return os << to_string(trackSectorIds); }



class IdAndOffset
{
public:
    IdAndOffset() = default;
    IdAndOffset(int id, int offset) : id(id), offset(offset)
    {
    }

    int id = -1;
    int offset = -1;
};

class IdAndOffsetVector : public VectorX<IdAndOffset>
{
public:
    using VectorX<IdAndOffset>::vector;

    TrackSectorIds GetSectorIds() const;
    void ReplaceMissingSectorIdsFrom(const TrackSectorIds& trackSectorIds);
    bool ReplaceMissingIdsByFindingTrackSectorIds(const int sectorsMin = 0);
};
