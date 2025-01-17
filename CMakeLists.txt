cmake_minimum_required(VERSION 3.14..3.24)

if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.15)
  cmake_policy(SET CMP0091 NEW) # Allow MSVC_RUNTIME_LIBRARY override
endif()

project(samdiskplus LANGUAGES C CXX)
set(PROJECT_TITLE SAMdiskPlus)

if (CMAKE_TOOLCHAIN_FILE)
  message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
  include(${CMAKE_TOOLCHAIN_FILE})
endif()

if (WIN32 OR MSVC OR MINGW OR MSYS)
  set(SYSTEM_TYPE windows)
else()
  if (APPLE)
    set(SYSTEM_TYPE apple)
  else()
    if (UNIX)
      set(SYSTEM_TYPE linux)
    else()
      set(SYSTEM_TYPE unknown)
    endif()
  endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_TYPE x64) # 64 bits
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ARCH_TYPE x86)# 32 bits
endif()

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CheckCXXCompilerFlag)
include(CMakePrintHelpers)

check_type_size(ssize_t SSIZE_T)

check_symbol_exists(O_BINARY fcntl.h HAVE_O_BINARY)

check_include_files(io.h HAVE_IO_H)
check_include_files(winsock2.h HAVE_WINSOCK2_H)
check_include_files(paths.h HAVE_PATHS_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files(sys/disk.h HAVE_SYS_DISK_H)
check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files(arpa/inet.h HAVE_ARPA_INET_H)
check_include_files(netinet/in.h HAVE_NETINET_IN_H)
check_include_files(linux/hdreg.h HAVE_LINUX_HDREG_H)
check_include_files(linux/fs.h HAVE_LINUX_FS_H)
check_include_files(scsi/scsi.h HAVE_SCSI_SCSI_H)
check_include_files(scsi/sg.h HAVE_SCSI_SG_H)
check_include_files(DiskArbitration/DiskArbitration.h HAVE_DISKARBITRATION_DISKARBITRATION_H)

check_function_exists(snprintf HAVE_SNPRINTF)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(lseek64 HAVE_LSEEK64)
check_function_exists(_lseeki64 HAVE__LSEEKI64)
check_function_exists(_strcmpi HAVE__STRCMPI)
check_function_exists(_snprintf HAVE__SNPRINTF)
check_function_exists(sysconf HAVE_SYSCONF)
check_function_exists(getopt_long HAVE_GETOPTLONG)

set(CXXSRC
    src/BitBuffer.cpp src/BitstreamDecoder.cpp src/BitstreamEncoder.cpp
    src/BitstreamTrackBuilder.cpp src/BlockDevice.cpp src/cmd_copy.cpp
    src/cmd_create.cpp src/cmd_dir.cpp src/cmd_format.cpp src/cmd_info.cpp
    src/cmd_list.cpp src/cmd_rpm.cpp src/cmd_scan.cpp src/cmd_verify.cpp
    src/cmd_view.cpp src/CrashDump.cpp src/CRC16.cpp src/DemandDisk.cpp
    src/Disk.cpp src/DiskUtil.cpp src/Driver.cpp src/FdrawcmdSys.cpp
    src/FileSystem.cpp
    src/FluxDecoder.cpp src/FluxTrackBuilder.cpp src/Format.cpp src/HDD.cpp
    src/HDFHDD.cpp src/Header.cpp src/IBMPC.cpp src/IBMPCBase.cpp src/Image.cpp
    src/JupiterAce.cpp src/KF_libusb.cpp src/KF_WinUsb.cpp src/KryoFlux.cpp
    src/MemFile.cpp src/MultiScanResult.cpp src/OrphanDataCapableTrack.cpp
    src/PhysicalTrackMFM.cpp src/precompile.cpp src/Range.cpp
    src/RepairSummaryDisk.cpp src/RetryPolicy.cpp src/SAMCoupe.cpp
    src/SAMdisk.cpp src/SCP_FTD2XX.cpp src/SCP_FTDI.cpp src/SCP_USB.cpp
    src/SCP_Win32.cpp src/Sector.cpp src/SpecialFormat.cpp
    src/SpectrumPlus3.cpp src/SuperCardPro.cpp
    src/TimedAndPhysicalDualTrack.cpp src/Track.cpp
    src/TrackBuilder.cpp src/TrackData.cpp src/TrackDataParser.cpp
    src/TrackSectorIds.cpp
    src/Trinity.cpp src/types.cpp src/Util.cpp src/utils.cpp src/VfdrawcmdSys.cpp
    src/win32_error.cpp
    src/types/1dd.cpp src/types/2d.cpp src/types/a2r.cpp src/types/adf.cpp
    src/types/blk_dev.cpp src/types/bpb.cpp src/types/builtin.cpp
    src/types/cfi.cpp src/types/cpm.cpp src/types/cqm.cpp src/types/cwtool.cpp
    src/types/d2m.cpp src/types/d4m.cpp src/types/d80.cpp src/types/d81.cpp
    src/types/d88.cpp src/types/dfi.cpp src/types/dmk.cpp src/types/do.cpp
    src/types/ds2.cpp src/types/dsc.cpp src/types/dsk.cpp src/types/dti.cpp
    src/types/fd.cpp src/types/fdi.cpp src/types/fdrawsys_ab_dev.cpp
    src/types/fdrawsys_dev.cpp src/types/hfe.cpp src/types/imd.cpp
    src/types/ipf.cpp src/types/kf_dev.cpp src/types/kf_stream.cpp
    src/types/lif.cpp src/types/mbd.cpp src/types/mfi.cpp src/types/mgt.cpp
    src/types/msa.cpp src/types/opd.cpp src/types/pdi.cpp src/types/qdos.cpp
    src/types/raw.cpp src/types/rdsk.cpp
    src/types/record.cpp src/types/s24.cpp src/types/sad.cpp
    src/types/sap.cpp src/types/sbt.cpp src/types/scl.cpp src/types/scp.cpp
    src/types/scp_dev.cpp src/types/sdf.cpp src/types/st.cpp src/types/td0.cpp
    src/types/trd.cpp src/types/trinload.cpp src/types/udi.cpp
    src/types/unsupp.cpp src/types/vfdphysicaltracks_dev.cpp src/types/woz.cpp
    src/filesystems/Fat12FileSystem.cpp src/filesystems/StFat12FileSystem.cpp)

set(CXXH
    include/AddressMark.h include/BitBuffer.h
    include/BitPositionableByteVector.h include/BitstreamDecoder.h
    include/BitstreamEncoder.h include/BitstreamTrackBuilder.h
    include/BlockDevice.h include/ByteBitPosition.h include/CRC16.h
    include/Cpp_helpers.h include/CrashDump.h include/DemandDisk.h
    include/DeviceReadingPolicy.h include/Disk.h include/DiskConstants.h
    include/DiskUtil.h include/FdrawcmdSys.h include/FileIO.h
    include/FileSystem.h include/FluxDecoder.h include/FluxTrackBuilder.h
    include/Format.h include/HDD.h include/HDFHDD.h include/Header.h
    include/IBMPC.h include/IBMPCBase.h include/Image.h include/Interval.h
    include/JupiterAce.h include/KF_WinUsb.h include/KF_libusb.h include/KryoFlux.h
    include/MemFile.h include/MultiScanResult.h include/Options.h
    include/OrphanDataCapableTrack.h include/PhysicalTrackMFM.h
    include/Platform.h include/PlatformConfig.h include/Range.h
    include/RepairSummaryDisk.h include/RetryPolicy.h include/RingedInt.h
    include/SAMCoupe.h include/SAMdisk.h include/SCP_FTD2XX.h
    include/SCP_FTDI.h include/SCP_USB.h include/SCP_Win32.h include/Sector.h
    include/SpecialFormat.h include/SpectrumPlus3.h include/SuperCardPro.h
    include/ThreadPool.h include/TimedAndPhysicalDualTrack.h include/Track.h
    include/TrackBuilder.h include/TrackData.h include/TrackDataParser.h
    include/TrackSectorIds.h include/Trinity.h include/Util.h include/VectorX.h
    include/VfdrawcmdSys.h include/fdrawcmd.h include/opd.h include/qdos.h
    include/resource.h include/trd.h include/types.h include/utils.h
    include/win32_error.h include/winusb_defs.h
    include/types/bpb.h include/types/dsk.h include/types/fdrawsys_dev.h
    include/types/raw.h include/types/record.h
    include/types/sdf.h include/types/vfdphysicaltracks_dev.h
    include/filesystems/Fat12FileSystem.h include/filesystems/StFat12FileSystem.h)

add_executable(${PROJECT_NAME} ${CXXSRC} ${CSRC} ${CXXH})
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

target_include_directories(${PROJECT_NAME} PRIVATE include src)

target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCE_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/")
target_compile_definitions(${PROJECT_NAME} PRIVATE _FILE_OFFSET_BITS=64)

if (DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
else()
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release RelWithDebInfo MinSizeRel.")
endif()
message(STATUS "CMAKE_BUILD_TYPE is: ${CMAKE_BUILD_TYPE}")
if (CMAKE_BUILD_TYPE MATCHES Debug)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _DEBUG=1)
endif()
cmake_print_variables(ARCH_TYPE)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

CHECK_CXX_COMPILER_FLAG("-Weffc++" COMPILER_SUPPORTS_EFFCXX)
if (COMPILER_SUPPORTS_EFFCXX)
  target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Weffc++>)
endif()

if (CMAKE_BUILD_TOOL MATCHES "make")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wshadow -pedantic)
elseif (CMAKE_BUILD_TOOL MATCHES "(msdev|devenv|nmake)")
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()

CHECK_CXX_COMPILER_FLAG("-Wsuggest-override" COMPILER_SUPPORTS_WSUGGEST)
if (COMPILER_SUPPORTS_WSUGGEST)
  target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>)
endif()

CHECK_CXX_COMPILER_FLAG("-stdlib=libc++" COMPILER_SUPPORTS_LIBCXX)
CHECK_CXX_COMPILER_FLAG("-stdlib=libstdc++" COMPILER_SUPPORTS_LIBSTDCXX)
if (COMPILER_SUPPORTS_LIBSTDCXX AND NOT APPLE)
  target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>)
endif()

set(CMAKE_THREAD_PREFER_PTHREAD pthread)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

find_package(ZLIB)
if (ZLIB_FOUND)
  message(STATUS "zlib: ${ZLIB}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARY})
  set(HAVE_ZLIB 1)
  find_package(PkgConfig)
  if (PKGCONFIG_FOUND)
    pkg_check_modules(MINIZIP minizip)
    if (MINIZIP_FOUND)
      target_include_directories(${PROJECT_NAME} PRIVATE ${MINIZIP_INCLUDE_DIRS})
      target_link_libraries(${PROJECT_NAME} ${MINIZIP_LIBRARIES})
      set(HAVE_MINIZIP 1)
    else()
      set(MINIZIPSRC minizip/ioapi.c minizip/unzip.c)
      target_sources(${PROJECT_NAME} PRIVATE ${MINIZIPSRC})
      target_include_directories(${PROJECT_NAME} PRIVATE minizip)
    endif()
  endif()
else()
  message(STATUS "zlib not found")
endif()

if (NOT HAVE_GETOPTLONG)
    set(GETOPTSRC getopt/getopt.c)
    target_sources(${PROJECT_NAME} PRIVATE ${GETOPTSRC})
    target_include_directories(${PROJECT_NAME} PRIVATE getopt)
endif()

find_package(BZip2)
if (BZIP2_FOUND)
  message(STATUS "BZip2: ${LibLZMA}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${BZIP2_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${BZIP2_LIBRARIES})
  set(HAVE_BZIP2 1)
else()
  message(STATUS "BZip2 not found")
endif()

find_package(LibLZMA)
if (LIBLZMA_FOUND)
  message(STATUS "libLZMA: ${LibLZMA}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${LIBLZMA_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${LIBLZMA_LIBRARIES})
  set(HAVE_LZMA 1)
else()
  message(STATUS "libLZMA not found")
endif()

find_library(WINUSB_LIBRARY NAMES winusb ENV LD_LIBRARY_PATH)
if (WINUSB_LIBRARY)
  message(STATUS "Found winusb: ${WINUSB_LIBRARY}")
  target_link_libraries(${PROJECT_NAME} ${WINUSB_LIBRARY} delayimp.lib)
  set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/DELAYLOAD:winusb.dll")
  set(HAVE_WINUSB 1)
else()
  message(STATUS "winusb not found")
endif()

find_library(LIBUSB1_LIBRARY NAMES usb-1.0 ENV LD_LIBRARY_PATH)
find_path(LIBUSB1_INCLUDE_DIR NAMES libusb.h PATH_SUFFIXES libusb-1.0)
if (LIBUSB1_LIBRARY AND LIBUSB1_INCLUDE_DIR)
  message(STATUS "Found libusb-1.0: ${LIBUSB1_LIBRARY}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB1_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${LIBUSB1_LIBRARY})
  set(HAVE_LIBUSB1 1)
else()
  message(STATUS "libusb-1.0 not found")
endif()

find_library(FTD2XX_LIBRARY NAMES ftd2xx HINTS ${CMAKE_SOURCE_DIR}/ftd2xx/${SYSTEM_TYPE}/${ARCH_TYPE} ENV LD_LIBRARY_PATH)
find_path(FTD2XX_INCLUDE_DIR ftd2xx.h HINTS ${CMAKE_SOURCE_DIR}/ftd2xx)
if (FTD2XX_LIBRARY AND FTD2XX_INCLUDE_DIR)
  message(STATUS "Found FTD2XX: ${FTD2XX_LIBRARY}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${FTD2XX_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${FTD2XX_LIBRARY})
  set(HAVE_FTD2XX 1)
else()
  message(STATUS "FTD2XX not found")
endif()

find_library(FTDI_LIBRARY NAMES ftdi ftdi1 HINTS ${CMAKE_SOURCE_DIR}/ftdi/${SYSTEM_TYPE}/${ARCH_TYPE} ENV LD_LIBRARY_PATH)
find_path(FTDI_INCLUDE_DIR ftdi.h HINTS ${CMAKE_SOURCE_DIR}/ftdi)
if (FTDI_LIBRARY AND FTDI_INCLUDE_DIR)
  message(STATUS "Found FTDI: ${FTDI_LIBRARY}")
  target_include_directories(${PROJECT_NAME} PRIVATE ${FTDI_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} ${FTDI_LIBRARY})
  set(HAVE_FTDI 1)
else()
  message(STATUS "FTDI not found")
endif()

find_library(CAPSIMAGE_LIBRARY NAMES capsimage CAPSimage CAPSImg HINTS ${CMAKE_SOURCE_DIR}/capsimage/${SYSTEM_TYPE}/${ARCH_TYPE} ENV LD_LIBRARY_PATH)
find_path(CAPSIMAGE_INCLUDE_DIR caps/CapsLibAll.h HINTS ${CMAKE_SOURCE_DIR}/capsimage)
if (CAPSIMAGE_LIBRARY AND CAPSIMAGE_INCLUDE_DIR)
  message(STATUS "Found CAPSimage: ${CAPSIMAGE_LIBRARY}")

  set(CMAKE_REQUIRED_LIBRARIES ${CAPSIMAGE_LIBRARY})
  check_function_exists(CAPSSetRevolution HAVE_CAPSSETREVOLUTION)

  if (CAPSIMAGE_INCLUDE_DIR AND HAVE_CAPSSETREVOLUTION)
    target_include_directories(${PROJECT_NAME} PRIVATE ${CAPSIMAGE_INCLUDE_DIR})
    if (APPLE)
      set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-F /Library/Frameworks -weak_framework CAPSImage")
    else()
      target_link_libraries(${PROJECT_NAME} ${CAPSIMAGE_LIBRARY})
    endif()
    set(HAVE_CAPSIMAGE 1)
  else()
    message(STATUS "CAPSimage is too old, version 5.1 or later required!")
  endif()
elseif (CAPSIMAGE_LIBRARY)
  message(WARNING "CAPSimage library found but header files are missing!\nPlease ensure caps/CapsLibAll.h is in your include path.")
else()
  message(STATUS "CAPSimage not found")
endif()

if (WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_TITLE})
  target_link_libraries(${PROJECT_NAME} setupapi ws2_32)
endif()
# Setting HAVE_FDRAWCMD_H in all cases, not only for WIN32.
set(HAVE_FDRAWCMD_H 1)

if (APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9)
  set(CMAKE_OSX_ARCHITECTURES i386;x86_64)
  find_library(DA_FRAMEWORK DiskArbitration)
  find_library(CF_FRAMEWORK CoreFoundation)
  find_library(IOKIT_FRAMEWORK IOKit)
  target_link_libraries(${PROJECT_NAME} ${DA_FRAMEWORK} ${CF_FRAMEWORK} ${IOKIT_FRAMEWORK})
endif()

configure_file(config.h.in config.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
