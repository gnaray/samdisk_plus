#include "FileSystem.h"
#include "Util.h"
#include "filesystems/Fat12FileSystem.h"
#include "filesystems/StFat12FileSystem.h"

#include <algorithm>

FileSystemWrappers InitFileSystemWrappers()
{
    FileSystemWrappers fileSystemWrappers;
    fileSystemWrappers.emplace_back(new FileSystemWrapper<StFat12FileSystem>());
    fileSystemWrappers.emplace_back(new FileSystemWrapper<Fat12FileSystem>());
    // Add more filesystems here.
    return fileSystemWrappers;
}
FileSystemWrappers fileSystemWrappers = InitFileSystemWrappers();

//////////////////////////////////////////////////////////////////////////////

bool FileSystemWrappers::IsValidFSName(const std::string& detectFSHavingName) const
{
    if (detectFSHavingName.empty())
        return false;
    const bool detectFSAll = util::caseInSensCompare(detectFSHavingName, DETECT_FS_AUTO);
    if (detectFSAll)
        return true;
    for (auto&& fileSystemWrapperInterface : *this)
    {
        if (util::caseInSensCompare(fileSystemWrapperInterface->Name(), detectFSHavingName))
            return true;
    }
    return false;
}

bool FileSystemWrappers::FindAndSetApprover(Disk& disk, const std::string& detectFSHavingName/* = DETECT_FS_AUTO*/) const
{
    if (detectFSHavingName.empty())
        return false;
    const bool detectFSAll = util::caseInSensCompare(detectFSHavingName, DETECT_FS_AUTO);
    for (auto&& fileSystemWrapperInterface : *this)
    {
        if ((detectFSAll || util::caseInSensCompare(fileSystemWrapperInterface->Name(), detectFSHavingName))
                && (disk.GetFileSystem() = fileSystemWrapperInterface->ConstructByApprovingDisk(disk)))
        {
            if (!disk.GetTypeDomesticFileSystemNames().empty()
                    && disk.GetTypeDomesticFileSystemNames().find(disk.GetFileSystem()->GetName()) == disk.GetTypeDomesticFileSystemNames().end())
                Message(msgWarning, "%s disk type at path (%s) has foreign %s filesystem",
                        disk.strType().c_str(), disk.GetPath().c_str(), disk.GetFileSystem()->GetName().c_str());
            return true;
        }
    }
    return false;
}

std::string FileSystemWrappers::ToString(bool onlyRelevantData/* = true*/) const
{
    std::ostringstream ss;
    if (!onlyRelevantData || !empty())
    {
        bool writingStarted = false;
        for (auto&& fileSystemWrapperInterface : *this)
        {
            if (writingStarted)
                ss << ' ';
            else
                writingStarted = true;
            ss << fileSystemWrapperInterface->ToString(onlyRelevantData);
        }
    }
    return ss.str();
}
