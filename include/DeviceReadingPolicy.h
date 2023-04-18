#pragma once

#include "Header.h"
#include "Sector.h"

class DeviceReadingPolicy
{
public:
    DeviceReadingPolicy() = default;

    DeviceReadingPolicy(const Headers& fileSystemWantedSectorHeaders)
        : m_fileSystemWantedSectorHeaders(fileSystemWantedSectorHeaders)
    {
    }

    DeviceReadingPolicy(const Headers& fileSystemWantedSectorHeaders, bool lookForPossibleSectors)
        : m_fileSystemWantedSectorHeaders(fileSystemWantedSectorHeaders), m_lookForPossibleSectors(lookForPossibleSectors)
    {
    }

    DeviceReadingPolicy(const Headers& fileSystemWantedSectorHeaders, const Sectors& skippableSectors, bool lookForPossibleSectors = true)
        : m_fileSystemWantedSectorHeaders(fileSystemWantedSectorHeaders), m_skippableSectors(skippableSectors), m_lookForPossibleSectors(lookForPossibleSectors)
    {
    }

    Headers FileSystemWantedSectorHeaders() const
    {
        return m_fileSystemWantedSectorHeaders;
    }

    void SetFileSystemWantedSectorHeaders(const Headers& fileSystemWantedSectorHeaders)
    {
        m_fileSystemWantedSectorHeaders = fileSystemWantedSectorHeaders;
    }

    Sectors SkippableSectors() const
    {
        return m_skippableSectors;
    }

    void SetSkippableSectors(const Sectors& skippableSectors)
    {
        m_skippableSectors = skippableSectors;
    }

    bool LookForPossibleSectors() const
    {
        return m_lookForPossibleSectors;
    }

    void SetLookForPossibleSectors(bool lookForPossibleSectors)
    {
        m_lookForPossibleSectors = lookForPossibleSectors;
    }

    friend std::string to_string(const DeviceReadingPolicy& deviceReadingPolicy, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        bool writingStarted = false;
        std::string s = to_string(deviceReadingPolicy.m_fileSystemWantedSectorHeaders, onlyRelevantData);
        if (!onlyRelevantData || !s.empty())
        {
            ss << "Wanted sectors {" << s << "}";
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
    Headers m_fileSystemWantedSectorHeaders{};
    Sectors m_skippableSectors{};
    bool m_lookForPossibleSectors = true;
};

inline std::ostream& operator<<(std::ostream& os, const DeviceReadingPolicy& deviceReadingPolicy) { return os << to_string(deviceReadingPolicy); }
