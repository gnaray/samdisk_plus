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
          m_wantedSectorHeaderIdsUnskippableValid(false), m_lookForPossibleSectors(lookForPossibleSectors)

    {
    }

    const Interval<int>& WantedSectorHeaderIds() const
    {
        return m_wantedSectorHeaderIds;
    }

    void SetWantedSectorHeaderIds(const Interval<int>& wantedSectorHeaderIds)
    {
        m_wantedSectorHeaderIds = wantedSectorHeaderIds;
        m_wantedSectorHeaderIdsUnskippableValid = false;
    }

    const Sectors& SkippableSectors() const
    {
        return m_skippableSectors;
    }

    void SetSkippableSectors(const Sectors& skippableSectors)
    {
        m_skippableSectors = skippableSectors;
        m_wantedSectorHeaderIdsUnskippableValid = false;
    }

    bool LookForPossibleSectors() const
    {
        return m_lookForPossibleSectors;
    }

    void SetLookForPossibleSectors(bool lookForPossibleSectors)
    {
        m_lookForPossibleSectors = lookForPossibleSectors;
    }

    const std::set<int>& SelectWantedSectorHeaderIdsUnskippable()
    {
        if (!m_wantedSectorHeaderIdsUnskippableValid)
        {
            m_wantedSectorHeaderIdsUnskippable = m_skippableSectors.NotContaining(m_wantedSectorHeaderIds);
            m_wantedSectorHeaderIdsUnskippableValid = true;
        }
        return m_wantedSectorHeaderIdsUnskippable;
    }

    friend std::string to_string(const DeviceReadingPolicy& deviceReadingPolicy, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        bool writingStarted = false;
        std::string s = to_string(deviceReadingPolicy.m_wantedSectorHeaderIds, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            ss << "Wanted sector ids {" << s << "}";
            writingStarted = true;
        }
        s = to_string(deviceReadingPolicy.m_skippableSectors, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            if (writingStarted)
                ss << ", ";
            else
                writingStarted = true;
            ss << "Skippable sectors {" << s << "}";
        }
        return ss.str();
    }

protected:
    Interval<int> m_wantedSectorHeaderIds{};
    Sectors m_skippableSectors{};
    std::set<int> m_wantedSectorHeaderIdsUnskippable{}; // Dynamically calculated.
    bool m_wantedSectorHeaderIdsUnskippableValid = false;
    bool m_lookForPossibleSectors = true;
};

inline std::ostream& operator<<(std::ostream& os, const DeviceReadingPolicy& deviceReadingPolicy) { return os << to_string(deviceReadingPolicy); }
