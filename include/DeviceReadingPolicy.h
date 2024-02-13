#pragma once

#include "Sector.h"

// If wanted sector header ids is empty then it implies looking for possible sectors.
class DeviceReadingPolicy
{
public:
    DeviceReadingPolicy() = default;

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds)
        : DeviceReadingPolicy(wantedSectorHeaderIds, true)
    {
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds, bool lookForPossibleSectors)
        : m_wantedSectorHeaderIds(wantedSectorHeaderIds), m_lookForPossibleSectors(lookForPossibleSectors)
    {
        assert(!m_wantedSectorHeaderIds.IsEmpty() || lookForPossibleSectors);
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds, const UniqueSectors& skippableSectors, bool lookForPossibleSectors = true)
        : DeviceReadingPolicy(wantedSectorHeaderIds, lookForPossibleSectors)
    {
        m_skippableSectors = skippableSectors;
    }

    const Interval<int>& WantedSectorHeaderIds() const
    {
        return m_wantedSectorHeaderIds;
    }

    void SetWantedSectorHeaderIds(const Interval<int>& wantedSectorHeaderIds)
    {
        m_wantedSectorHeaderIds = wantedSectorHeaderIds;
        m_unskippableWantedSectorHeaderIdsValid = false;
        if (m_wantedSectorHeaderIds.IsEmpty())
            m_lookForPossibleSectors = true;
    }

    const UniqueSectors& SkippableSectors() const
    {
        return m_skippableSectors;
    }

    void SetSkippableSectors(const UniqueSectors& skippableSectors)
    {
        m_skippableSectors = skippableSectors;
        m_unskippableWantedSectorHeaderIdsValid = false;
    }

    void AddSkippableSectors(const UniqueSectors& skippableSectors)
    {
        m_skippableSectors.insert(skippableSectors.begin(), skippableSectors.end());
        m_unskippableWantedSectorHeaderIdsValid = false;
    }

    void ClearSkippableSectors()
    {
        m_skippableSectors.clear();
        m_unskippableWantedSectorHeaderIdsValid = false;
    }

    bool LookForPossibleSectors() const
    {
        return m_lookForPossibleSectors;
    }

    void SetLookForPossibleSectors(bool lookForPossibleSectors)
    {
        m_lookForPossibleSectors = lookForPossibleSectors;
    }

protected:
    bool UnskippableWantedSectorHeaderIdsEmpty() const
    {
        if (!m_unskippableWantedSectorHeaderIdsValid)
        {
            m_unskippableWantedSectorHeaderIdsEmpty = m_skippableSectors.AnyIdsNotContainedInThis(m_wantedSectorHeaderIds);
            m_unskippableWantedSectorHeaderIdsValid = true;
        }
        return m_unskippableWantedSectorHeaderIdsEmpty;
    }

public:
    bool WantMoreSectors() const
    {
        return (LookForPossibleSectors() // else m_wantedSectorHeaderIds is not empty.
                || !UnskippableWantedSectorHeaderIdsEmpty());
    }

    friend std::string to_string(const DeviceReadingPolicy& deviceReadingPolicy, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        bool writingStarted = false;
        std::string s = to_string(deviceReadingPolicy.m_wantedSectorHeaderIds, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            ss << "Wanted sector ids = " << s;
            writingStarted = true;
        }
        s = to_string(deviceReadingPolicy.m_skippableSectors, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            if (writingStarted)
                ss << ", ";
            else
                writingStarted = true;
            ss << "Skippable sectors = {" << s << "}";
        }
        if (writingStarted)
            ss << ", ";
        else
            writingStarted = true;
        ss << "Look for possible sectors = " << deviceReadingPolicy.m_lookForPossibleSectors;
        return ss.str();
    }

protected:
    Interval<int> m_wantedSectorHeaderIds{};
    UniqueSectors m_skippableSectors{};
    mutable bool m_unskippableWantedSectorHeaderIdsEmpty = false; // Dynamically calculated.
    mutable bool m_unskippableWantedSectorHeaderIdsValid = false; // Dynamically signs that corresponding value is valid.
    bool m_lookForPossibleSectors = true;
};

inline std::ostream& operator<<(std::ostream& os, const DeviceReadingPolicy& deviceReadingPolicy) { return os << to_string(deviceReadingPolicy); }
