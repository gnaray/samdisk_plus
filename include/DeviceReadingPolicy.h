#pragma once

#include "Sector.h"

// If wanted sector header ids is empty then it implies looking for possible sectors.
class DeviceReadingPolicy
{
public:
    DeviceReadingPolicy() = default;

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderSectors)
        : DeviceReadingPolicy(wantedSectorHeaderSectors, true)
    {
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderSectors, bool lookForPossibleSectors)
        : m_wantedSectorHeaderSectors(wantedSectorHeaderSectors), m_lookForPossibleSectors(lookForPossibleSectors)
    {
        assert(!m_wantedSectorHeaderSectors.IsEmpty() || lookForPossibleSectors);
    }

    DeviceReadingPolicy(const Interval<int>& wantedSectorHeaderSectors, const UniqueSectors& skippableSectors, bool lookForPossibleSectors = true)
        : DeviceReadingPolicy(wantedSectorHeaderSectors, lookForPossibleSectors)
    {
        assert(skippableSectors.empty() || skippableSectors.trackLen > 0);

        m_skippableSectors = skippableSectors;
    }

    const Interval<int>& WantedSectorHeaderSectors() const
    {
        return m_wantedSectorHeaderSectors;
    }

    void SetWantedSectorHeaderSectors(const Interval<int>& wantedSectorHeaderSectors)
    {
        m_wantedSectorHeaderSectors = wantedSectorHeaderSectors;
        m_unskippableWantedSectorHeaderSectorsValid = false;
        if (m_wantedSectorHeaderSectors.IsEmpty())
            m_lookForPossibleSectors = true;
    }

    bool IsWanted(int sector) const
    {
        return !m_wantedSectorHeaderSectors.IsEmpty()
            && m_wantedSectorHeaderSectors.Where(sector) == BaseInterval::Location::Within;
    }

    const UniqueSectors& SkippableSectors() const
    {
        return m_skippableSectors;
    }

    void SetSkippableSector(const Sector& skippableSector, const int trackLen)
    {
        m_skippableSectors.clear();
        m_skippableSectors.trackLen = trackLen;
        m_skippableSectors.insert(skippableSector);
        m_unskippableWantedSectorHeaderSectorsValid = false;
    }

    void SetSkippableSectors(const UniqueSectors& skippableSectors)
    {
        m_skippableSectors = skippableSectors;
        m_unskippableWantedSectorHeaderSectorsValid = false;
    }

    void AddSkippableSector(const Sector& skippableSector, const int trackLen)
    {
        UniqueSectors skippableSectors(trackLen);
        skippableSectors.insert(skippableSector);
        AddSkippableSectors(skippableSectors);
    }

    void RemoveSkippableSector(const UniqueSectors::const_iterator& itSkippableSector)
    {
        m_skippableSectors.erase(itSkippableSector);
    }

    void AddSkippableSectors(const UniqueSectors& skippableSectors)
    {
        if (skippableSectors.empty())
            return;

        if (m_skippableSectors.trackLen > 0)
            assert(skippableSectors.trackLen > 0);
        else if (!m_skippableSectors.empty())
            assert(skippableSectors.trackLen == 0);

        if (m_skippableSectors.trackLen > 0 && m_skippableSectors.trackLen != skippableSectors.trackLen)
        {
            const auto normaliser = static_cast<double>(m_skippableSectors.trackLen) / skippableSectors.trackLen;
            for (auto& skippableSector : skippableSectors)
            {
                if (!m_skippableSectors.Contains(skippableSector, skippableSectors.trackLen))
                {
                    auto normalisedSector = skippableSector;
                    normalisedSector.offset = round_AS<int>(normalisedSector.offset * normaliser);
                    m_skippableSectors.insert(normalisedSector);
                }
            }
        }
        else
        {
            if (m_skippableSectors.empty())
                m_skippableSectors = skippableSectors;
            else for (auto& skippableSector : skippableSectors)
            {
                if (!m_skippableSectors.Contains(skippableSector, skippableSectors.trackLen))
                    m_skippableSectors.insert(skippableSector);
            }
        }
        m_unskippableWantedSectorHeaderSectorsValid = false;
    }

    void ClearSkippableSectors()
    {
        m_skippableSectors.clear();
        m_unskippableWantedSectorHeaderSectorsValid = false;
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
        if (!m_unskippableWantedSectorHeaderSectorsValid)
        {
            m_unskippableWantedSectorHeaderSectorsEmpty = !m_skippableSectors.AnyIdsNotContainedInThis(m_wantedSectorHeaderSectors);
            m_unskippableWantedSectorHeaderSectorsValid = true;
        }
        return m_unskippableWantedSectorHeaderSectorsEmpty;
    }

public:
    bool WantMoreSectors() const
    {
        return (LookForPossibleSectors() // else m_wantedSectorHeaderSectors is not empty.
                || !UnskippableWantedSectorHeaderIdsEmpty());
    }

    std::string ToString(bool onlyRelevantData = true) const
    {
        std::ostringstream ss;
        bool writingStarted = false;
        std::string s = to_string(m_wantedSectorHeaderSectors, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            ss << "Wanted sector header sectors = {" << s << "}";
            writingStarted = true;
        }
        s = to_string(m_skippableSectors, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            if (writingStarted)
                ss << ", ";
            else
                writingStarted = true;
            ss << "Skippable sectors = {" << s << "} (tracklen = " << m_skippableSectors.trackLen << ")";
        }
        if (writingStarted)
            ss << ", ";
        else
            writingStarted = true;
        ss << "Look for possible sectors = " << m_lookForPossibleSectors;
        return ss.str();
    }

    friend std::string to_string(const DeviceReadingPolicy& deviceReadingPolicy, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << deviceReadingPolicy.ToString(onlyRelevantData);
        return ss.str();
    }

protected:
    Interval<int> m_wantedSectorHeaderSectors{};
    UniqueSectors m_skippableSectors{};
    mutable bool m_unskippableWantedSectorHeaderSectorsEmpty = false; // Dynamically calculated.
    mutable bool m_unskippableWantedSectorHeaderSectorsValid = false; // Dynamically signs that corresponding value is valid.
    bool m_lookForPossibleSectors = true;
};

inline std::ostream& operator<<(std::ostream& os, const DeviceReadingPolicy& deviceReadingPolicy) { return os << deviceReadingPolicy.ToString(); }
