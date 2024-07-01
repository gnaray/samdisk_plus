# SAMdiskPlus

## Introduction

[SAMdiskPlus](https://github.com/gnaray/samdisk_plus.git) is based on
[SAMdisk](https://github.com/simonowen/samdisk), please read its
[readme file](https://github.com/simonowen/samdisk/blob/main/ReadMe.md) first.  

The main goal of this project is rescuing floppy disks
instead of cloning protected floppy disks. If a floppy disk is in perfect
condition then cloning is better than rescuing because it copies the same
as rescuing plus the copy protection bits. However if a floppy disk is in weak
condition then rescuing is better than cloning because the cloning method
misinterprets bad bits as copy protection bits instead of retrying to read
the bits again until those become good bits (hopefully).

The rescuing copy method is quite different from cloning copy method so the
user must pass the --fdraw-rescue-mode command line option to SAMdisk.exe to
use rescuing. Currently the rescuing method supports only MFM encoding and only
when using the latest (**1.0.1.12**) version of fdrawcmd.sys driver.
Run "SAMdisk.exe -v" to see if the fdrawcmd.sys driver is available and its
version is at least (**1.0.1.12**). See also **System Requirements**.

## New options

To support rescuing the following command line options are introduced in
SAMdisk plus which also affect cloning.  
**--normal-disk**: Expects disk as normal i.e. all units (sectors, tracks, sides)
  have same size, sector ids form a sequence starting by 1, sector headers
  match the cylinder and head of the containing track.  
**--readstats**: Administrates the read attempts of a sector and read count of
  its each data (typically bad CRC sector has more bad data copies), called
  reading statistics. It can be saved only in RDSK format image currently.  
**--paranoia**: Does not trust good CRC data because a bad data can also have good
  CRC rarely when reading a wear sector. It enables a sector having multiple
  different good data. A good data becomes final good or stable data when its
  read count reaches the stability level (see below). Since it relies on read
  count it requires readstats option. It also relies on stability level so it
  requires stability-level option (uses its default value if not specified).  
**--stability-level \<N>**: The good CRC data of a sector becomes stable when its read
  count reaches N. Thus N<=1 means any good data is stable. Default value is 3.  
**--skip-stable-sectors**: Skips reading sectors which are stable in destination
  already in repair mode thus it requires --repair option. Note that the
  --disk-retries option can also provide --repair option, see there.  
**--track-retries \<N>**: The amount of track retries when copying. See RetryAmount.  
**--disk-retries \<N>**: The amount of disk retries when copying. See RetryAmount.
  When copying a disk without --repair option the destination is ignored and
  overwritten. However if the --disk-retries option is also specified and is
  not 0 then the repair mode is automatically activated after the copying of
  first try (round) i.e. when retrying. This way the --skip-stable-sectors
  option can work together with --disk-retries option instead of --repair
  option.  
**--detect-devfs [filesystemname]**: Detects the specified filesystem on the used
  device(s) in order to use its format (makes rescuing faster). If the
  filesystemname is not specified then all known filesystem will be tried to be
  detected. To see the supported filesystem names run "SAMdisk.exe -v".  
**--byte-tolerance-of-time \<N>**: Considers two things the same (sector header when
  headers are the same, or sector data) at same location if their location
  difference is less or equal this value (in bytes). Default value is 64. It is
  highly recommended NOT to change it.  
**--fdraw-rescue-mode**: Uses the rescuing method when using the fdrawsys_dev.
  Default is using the cloning method.  
**--unhide-first-sector-by-track-end-sector**: Tries to unhide/fill the track
  starting sector by the nearby track ending sector (used when rescuing and it
  should be specified only when the track starting sector can not be found or
  its data is unreadable while the track ending sector starts close to track
  end and its data is readable). Default is false.

RetryAmount: It is an integer number with 3 cases.
- It is 0: No retrying occurs.
- It is positive: Retrying occurs RetryAmount times.
- It is negative: Retrying occurs -RetryAmount times. However when read data
  is improved during the retries then the retry counter is restarted.
  For example when RetryAmount is -3 and read data is improved on 2nd retry
  then reading will be retried 3 times again (at least).

Note that using rescuing method is the best with --normal-disk option. However
that option is not required thus the user can try to copy a not normal disk by
the rescuing method. It does not mean that a copy protected disk can be cloned
this way but a less normal disk might be rescued as well.

When using rescuing method together with debug option and it is not 0 then the
read physical tracks are saved to the folder where destination is. These files
are named something like "DATE TIME Raw track (cyl CYL head HEAD).pt" where
capital letter names are replaced with actual values. CYL has 2 digits, HEAD
has 1 digit. E.g. "2024-06-26 11.53.39 Raw track (cyl 02 head 0).pt"

## Filesytems

The filesystem feature is introduced. A detected filesystem makes rescuing
faster but also useful when the user requests the dir command. Currently the
FAT12 and STFAT12 filesystems are implemented and they can print the root
directory only (usually enough).
The dir command auto detects the filesystem even if the --detect-devfs option
is not specified.

## Examples

#### Dir a weak floppy disk in drive a: using rescuing method  
<code>SAMdisk.exe dir --fdraw-rescue-mode a:</code>

#### Dir a weak floppy disk in drive a: using rescuing method, paranoia mode  
<code>SAMdisk.exe dir --fdraw-rescue-mode --normal-disk --readstats --paranoia a:</code>

#### Dir an st image file which was created with paranoia option  
<code>SAMdisk.exe dir --readstats --paranoia f:\\image.st</code>

#### Scan first 2 track of a weak floppy disk in drive a: using rescuing method, printing offsets and verbosely for guessing the disk format  
<code>SAMdisk.exe scan --fdraw-rescue-mode --normal-disk --offsets -v -c0,2 a:</code>

#### Scan an st image file which was created with paranoia option, printing offsets and verbosely  
<code>SAMdisk.exe scan --readstats --paranoia --offsets -v f:\\disk.rdsk</code>

#### Copy a weak disk in drive a: using rescuing method, detecting filesystem of drive a:  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --detect-devfs -- a: f:\\disk.rdsk</code>

#### Copy a weak disk in drive a: using rescuing method, paranoia mode, detecting filesystem of drive a:  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --detect-devfs -- a: f:\\disk.rdsk</code>

#### Copy a weak disk in drive a: using rescuing method, paranoia mode, detecting filesystem of drive a:, retrying track reading, retrying disk reading, skipping stable sectors (repairing activated by disk-retries)  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --detect-devfs --skip-stable-sectors --track-retries=-2 --disk-retries=-1 -- a: f:\\disk.rdsk</code>

#### Copy to st image file from rdsk image file which was created with paranoia option  
<code>SAMdisk.exe copy --readstats --paranoia --normal-disk f:\\disk.rdsk f:\\disk.st</code>

### Author's examples

As it can be seen there are many options and their combinations are endless.
The better the options are selected and combined together the better result
is achieved. The following examples were used by the author in the process of
rescuing weak Atari ST and DOS floppy disks.

#### Copy a weak disk (although the retry numbers are disk condition dependent)  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --detect-devfs --skip-stable-sectors --track-retries=-2 --disk-retries=-1 --retries=-3 --rescans=-10 --new-drive --time --offsets -v --debug 0 -- a: f:\\disk.rdsk</code>

#### Repair copy a weak disk (although the retry numbers are disk condition dependent)  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --detect-devfs --skip-stable-sectors --track-retries=-3 --disk-retries=-1 --retries=-10 --rescans=-10 --new-drive --time --offsets -v --debug 0 --repair -- a: f:\\disk.rdsk</code>

#### Copy a weak disk without filesystem (thus not trying to detect it)  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --skip-stable-sectors --track-retries=-2 --disk-retries=-1 --retries=-3 --rescans=-10 --new-drive --time --offsets -v --debug 0 -- a: f:\\disk.rdsk</code>

#### Copy first 3 tracks of a weak disk without filesystem (thus not trying to detect it) for further inspection  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --skip-stable-sectors --track-retries=-2 --disk-retries=-1 --retries=-3 --rescans=-10 --new-drive --time --offsets -v --debug 0 -c0,3 -- a: f:\\disk.rdsk</code>

#### Copy a weak disk without filesystem (thus not trying to detect it) but with known format (e.g. guessing from scan command result), 82 tracks, 10 sectors:  
<code>SAMdisk.exe copy --fdraw-rescue-mode --normal-disk --readstats --paranoia --skip-stable-sectors --track-retries=-2 --disk-retries=-1 --retries=-3 --rescans=-10 --new-drive --time --offsets -v --debug 0 -c0,82 -s10 -- a: f:\\disk.rdsk</code>

#### View physical track 13 at both sides saved in f:\\ in debug mode:  
<code>SAMdisk.exe view -v -c13,1 -h2 vfdpt:f:\\</code>

## Development

This project is still not perfect. Use it at your own risk although I believe
it should not harm anything. Its development has probably ended.
If there will be interest and motivation then the development might go on.

## System Requirements

The [latest code](https://github.com/gnaray/samdisk_plus.git) builds under Windows,
Linux and macOS, and should be portable to other systems. Building requires a
C++ compiler with **C++14** support, such as Visual Studio 2015, g++ 4.9+, or
Clang 3.6+. Note that [SAMdisk](https://github.com/simonowen/samdisk) switched
to C++17 so this project is a standalone version instead of forked version.

All platforms require the [CMake](https://cmake.org/) build system. Windows
users can use the _Open Folder_ option in Visual Studio 2017 or later to
trigger the built-in CMake generator (or running <code>"cmake ."</code> in the project root
folder then opening the created sln file in Visual Studio 2015). A number of
optional libraries will be used if detected at configuration time.

This project contains the [CAPSimage](http://www.softpres.org/download) library for
IPF/CTRaw/KFStream/Draft support, and the [old FTDI](https://www.intra2net.com/en/developer/libftdi/download.php), [new FTDI](http://www.ftdichip.com/Drivers/D2XX.htm)
Linux libraries for SuperCard Pro device support. Windows users should download and copy
the necessary libs into the proper (ftdi/windows/x86, ftdi/windows/x64) and
(ftd2xx/windows/x86, ftd2xx/windows/x64) folders.

The rescuing method requires the latest (**1.0.1.12**) version of
[fdrawcmd.sys](https://github.com/gnaray/fdrawcmd/tree/multirevolution)
Windows filter driver. Unfortunately I can not provide a signed binary version
of it thus even if I would share the unsigned version then it would work only
in a Windows switched to test mode.

## License

The SAMdiskPlus source code is released under the
[MIT license](https://tldrlegal.com/license/mit-license).

Since the [CAPSimage](http://www.softpres.org/download) library is in the
project, please read its [License](https://github.com/gnaray/samdisk_plus/blob/main/capsimage/LICENCE.txt)
as well. If that license is unacceptable then simply rename the capsimage
folder and rebuild the project starting from running cmake in orded to omit
it from the binary.

## Contact

Gábor Náray  
<drszamo@gmail.com>
