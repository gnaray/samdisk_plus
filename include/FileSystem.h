#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "Disk.h"
#include "Options.h"
#include "Sector.h"

#include <memory>
#include <string>
#include <sstream>
#include <vector>

class FileSystem
{
public:
    virtual ~FileSystem() = default;

    static std::shared_ptr<FileSystem> ConstructByApprovingDisk(Disk& disk);
    static std::string Name();

    virtual std::string GetName() const = 0;
    virtual bool Dir() = 0;
    virtual Format GetFormat() const = 0;
    virtual void SetFormat(const Format& format) = 0;
    virtual bool IsSameNamed(const FileSystem& fileSystem) const = 0;
    virtual bool IsSameNamedWithSameCylHeadSectorsSize(const FileSystem& fileSystem) const = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FileSystemWrapperInterface
{
public:
    virtual ~FileSystemWrapperInterface() = default;

    virtual std::shared_ptr<FileSystem> ConstructByApprovingDisk(Disk& disk) const = 0;
    virtual std::string Name() const = 0;
    virtual std::string ToString(bool onlyRelevantData = true) const = 0;
};

inline std::ostream& operator<<(std::ostream& os, const FileSystemWrapperInterface* fileSystemWrapperInterface) { return os << fileSystemWrapperInterface->ToString(); }

//////////////////////////////////////////////////////////////////////////////

template<typename FS>
class FileSystemWrapper : public FileSystemWrapperInterface
{
public:
    std::shared_ptr<FileSystem> ConstructByApprovingDisk(Disk& disk) const override
    {
        return FS::ConstructByApprovingDisk(disk);
    }

    std::string Name() const override
    {
        return FS::Name();
    }

    std::string ToString(bool /*onlyRelevantData*/ = true) const override
    {
        return Name();
    }

    friend std::string to_string(const FileSystemWrapper<FS>& fileSystemWrapper, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << fileSystemWrapper.ToString(onlyRelevantData);
        return ss.str();
    }
};

template<typename FS> inline std::ostream& operator<<(std::ostream& os, const FileSystemWrapper<FS>& fileSystemWrapper) { return os << fileSystemWrapper.ToString(); }

//////////////////////////////////////////////////////////////////////////////

class FileSystemWrappers : public VectorX<std::unique_ptr<FileSystemWrapperInterface>>
{
public:
    using VectorX<std::unique_ptr<FileSystemWrapperInterface>>::VectorX;

    bool IsValidFSName(const std::string& detectFSHavingName) const;
    bool FindAndSetApprover(Disk &disk, const std::string& detectFSHavingName = DETECT_FS_AUTO) const;
    std::string ToString(bool onlyRelevantData = true) const;
    friend std::string to_string(const FileSystemWrappers& fileSystems, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << fileSystems.ToString(onlyRelevantData);
        return ss.str();
    }
};

inline std::ostream& operator<<(std::ostream& os, const FileSystemWrappers& fileSystemWrappers) { return os << fileSystemWrappers.ToString(); }

extern FileSystemWrappers fileSystemWrappers;

#endif // FILESYSTEM_H
