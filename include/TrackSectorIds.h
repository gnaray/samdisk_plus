#pragma once

#include "Header.h"

class TrackSectorIds : public VectorX<int>
{
public:
    using VectorX<int>::VectorX;

    static TrackSectorIds GetIds(const CylHead& cylhead, const int sectors, const int interleave = 0, const int skew = 0, const int offset = 0, const int base = 1);
    static const TrackSectorIds& GetTrackSectorIds(int sectors, int interleave);
    static const TrackSectorIds FindCompleteTrackSectorIdsFor(const TrackSectorIds& incompleteSectorIds, const int sectorsMin = 0);

    int MatchSectorIds(const TrackSectorIds& sectorIds) const;
    bool ExtendableTo(const TrackSectorIds& sectorIds) const;

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

inline std::ostream& operator<<(std::ostream& os, const TrackSectorIds& trackSectorIds) { return os << trackSectorIds.ToString(); }



class IdAndOffset
{
public:
    constexpr IdAndOffset() = default;
    IdAndOffset(int id_, int offset)
        : id(id_), offsetInterval(offset, 0, BaseInterval::ConstructMode::StartAndLength)
    {
    }

    // This case is for unknown id i.e. id must be negativ.
    IdAndOffset(int id_, int offset, int offsetAlt)
        : id(id_), offsetInterval(offset, offsetAlt, BaseInterval::ConstructMode::StartAndEnd)
    {
    }

    int id = -1;
    Interval<int> offsetInterval{};
};



class IdAndOffsetPairs : public VectorX<IdAndOffset>
{
public:
    using VectorX<IdAndOffset>::VectorX;

    TrackSectorIds GetSectorIds() const;
    void ReplaceMissingSectorIdsFrom(const TrackSectorIds& trackSectorIds);
    bool ReplaceMissingIdsByFindingTrackSectorIds(const int sectorsMin = 0);
    IdAndOffsetPairs::const_iterator FindSectorIdByOffset(const int offset) const;
};
