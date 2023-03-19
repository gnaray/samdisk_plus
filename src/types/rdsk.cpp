// Extended DSK (EDSK) specification:
//  http://www.cpctech.org.uk/docs/extdsk.html
//
// EDSK extensions for copy-protected disks:
//  http://simonowen.com/misc/extextdsk.txt
//
// John Elliot's rate+encoding extension:
//  http://groups.google.com/group/comp.sys.sinclair/msg/80e4c2d1403ea65c
//
// This is RDSK disk image which is extended version of EDSK.
// It is considered as EDSK version 2.
// It supports readstats info and big tracks (size can be > 0xff00).

#include "SAMdisk.h"
#include "IBMPC.h"
#include "types/dsk.h"

bool ReadRDSK(MemFile& file, std::shared_ptr<Disk>& disk)
{
    return ReadDSK(file, disk, 2);
}


bool WriteRDSK(FILE* f_, std::shared_ptr<Disk>& disk)
{
    return WriteDSK(f_, disk, 2);
}
