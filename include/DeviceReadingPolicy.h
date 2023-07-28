#pragma once

#include "Sector.h"

class DeviceReadingPolicy
{
public:
    DeviceReadingPolicy() = default;

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds)
        : m_wantedSectorHeaderIds(wantedSectorHeaderIds)
    {
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds, bool lookForPossibleSectors)
        : m_wantedSectorHeaderIds(wantedSectorHeaderIds), m_lookForPossibleSectors(lookForPossibleSectors)
    {
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderIds, const Sectors& skippableSectors, bool lookForPossibleSectors = true)
        : m_wantedSectorHeaderIds(wantedSectorHeaderIds), m_skippableSectors(skippableSectors),
          m_unskippableWantedSectorHeaderIdsValid(false), m_lookForPossibleSectors(lookForPossibleSectors)

    {
    }

    const Interval<int>& WantedSectorHeaderIds() const
    {
        return m_wantedSectorHeaderIds;
    }

    void SetWantedSectorHeaderIds(const Interval<int>& wantedSectorHeaderIds)
    {
        m_wantedSectorHeaderIds = wantedSectorHeaderIds;
        m_unskippableWantedSectorHeaderIdsValid = false;
    }

    const Sectors& SkippableSectors() const
    {
        return m_skippableSectors;
    }

    void SetSkippableSectors(const Sectors& skippableSectors)
    {
        m_skippableSectors = skippableSectors;
        m_unskippableWantedSectorHeaderIdsValid = false;
    }

    void AddSkippableSectors(const Sectors& skippableSectors)
    {
        m_skippableSectors.insert(m_skippableSectors.end(), skippableSectors.begin(), skippableSectors.end());
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

    const std::set<int>& UnskippableWantedSectorHeaderIds() const
    {
        if (!m_unskippableWantedSectorHeaderIdsValid)
        {
            m_unskippableWantedSectorHeaderIds = m_skippableSectors.NotContaining(m_wantedSectorHeaderIds);
            m_unskippableWantedSectorHeaderIdsValid = true;
        }
        return m_unskippableWantedSectorHeaderIds;
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
    Sectors m_skippableSectors{};
    mutable std::set<int> m_unskippableWantedSectorHeaderIds{}; // Dynamically calculated.
    mutable bool m_unskippableWantedSectorHeaderIdsValid = false; // Dynamically signs that corresponding value is valid.
    bool m_lookForPossibleSectors = true;
};

inline std::ostream& operator<<(std::ostream& os, const DeviceReadingPolicy& deviceReadingPolicy) { return os << to_string(deviceReadingPolicy); }
