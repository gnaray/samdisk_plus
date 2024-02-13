#pragma once

#include "PlatformConfig.h"
#include "Disk.h"
#include "DiskUtil.h"
#include "Header.h"
#include "HDD.h"

// copy
bool ImageToImage(const std::string& src_path, const std::string& dst_path);
bool Image2Trinity(const std::string& path, const std::string& trinity_path);
bool Hdd2Hdd(const std::string& src_path, const std::string& dst_path);
bool Hdd2Boot(const std::string& hdd_path, const std::string& boot_path);
bool Boot2Hdd(const std::string& boot_path, const std::string& hdd_path);
bool Boot2Boot(const std::string& src_path, const std::string& dst_path);

// create
bool CreateImage(const std::string& path, Range range);
bool CreateHddImage(const std::string& path, int nSizeMB_);

// dir
bool Dir(Disk& disk);
bool DirImage(const std::string& path);
bool IsMgtDirSector(const Sector& sector);

// list
bool ListDrives(int nVerbose_);
bool ListRecords(const std::string& path);
void ListDrive(const std::string& path, const HDD& hdd, int verbose);

// scan
bool ScanImage(const std::string& path, Range range);
void ScanTrack(const CylHead& cylhead, const Track& track, ScanContext& context, const UniqueSectors& ignored_sectors = UniqueSectors{});

// format, verify
bool FormatHdd(const std::string& path);
bool FormatBoot(const std::string& path);
bool FormatRecord(const std::string& path);
bool FormatImage(const std::string& path, Range range);
bool UnformatImage(const std::string& path, Range range);

// rpm
bool DiskRpm(const std::string& path);

// info
bool HddInfo(const std::string& path, int nVerbose_);
bool ImageInfo(const std::string& path);

// view
bool ViewImage(const std::string& path, Range range);
bool ViewHdd(const std::string& path, Range range);
bool ViewBoot(const std::string& path, Range range);

// fdrawcmd.sys driver functions
bool CheckDriver();
bool ReportDriverVersion();
