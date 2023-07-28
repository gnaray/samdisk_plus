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

bool FileSystemWrappers::FindAndSetApprover(Disk& disk) const
{
    for (auto&& fileSystemWrapperInterface : *this)
    {
        if ((disk.GetFileSystem() = fileSystemWrapperInterface->ConstructByApprovingDisk(disk)))
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
