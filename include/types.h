// Macro magic for supported disk types

// First block is only for providing header file declarations.
#ifndef TypesH
#define TypesH

#include "Disk.h"
#include "MemFile.h"

#include <memory>

using IMAGE_READFUNC = bool (*)(MemFile&, std::shared_ptr<Disk>&);
using IMAGE_WRITEFUNC = bool (*)(FILE*, std::shared_ptr<Disk>&);
using DEVICE_READFUNC = bool(*)(const std::string&, std::shared_ptr<Disk>&);
using DEVICE_WRITEFUNC = bool(*)(const std::string&, std::shared_ptr<Disk>&);

struct IMAGE_ENTRY
{
    const char* pszType;
    IMAGE_READFUNC pfnRead;
    IMAGE_WRITEFUNC pfnWrite;
};

struct DEVICE_ENTRY
{
    const char* pszType;
    DEVICE_READFUNC pfnRead;
    DEVICE_WRITEFUNC pfnWrite;
};

#endif // end of ifndef TypesH

// Second block is either for providing header file declarations or cpp source file definitions.
#if !defined(TypesH2) || defined(DEFINITIONS_FOR_CPP)
#define TypesH2

#ifndef DEFINITIONS_FOR_CPP

extern IMAGE_ENTRY aImageTypes[];
extern DEVICE_ENTRY aDeviceTypes[];

#define ADD_IMAGE_RW(x)     bool Read##x (MemFile&, std::shared_ptr<Disk> &); \
                            bool Write##x (FILE*,   std::shared_ptr<Disk> &);
#define ADD_IMAGE_RO(x)     bool Read##x (MemFile&, std::shared_ptr<Disk> &);
#define ADD_IMAGE_WO(x)     bool Write##x (FILE*,   std::shared_ptr<Disk> &);
#define ADD_IMAGE_HIDDEN_RO(x)  bool Read##x (MemFile&, std::shared_ptr<Disk> &);

#define ADD_DEVICE(x)       bool Read##x (const std::string &, std::shared_ptr<Disk> &); \
                            bool Write##x (const std::string &, std::shared_ptr<Disk> &);
#else

#define ADD_IMAGE_RW(x)     { #x, Read##x, Write##x },
#define ADD_IMAGE_RO(x)     { #x, Read##x, nullptr },
#define ADD_IMAGE_WO(x)     { #x, nullptr, Write##x },
#define ADD_IMAGE_HIDDEN_RO(x)  { "", Read##x, nullptr },

#define ADD_DEVICE(x)       { #x, Read##x, Write##x },

IMAGE_ENTRY aImageTypes[] = {

#endif

    // Types with a header signature
    ADD_IMAGE_RW(DSK)
    ADD_IMAGE_RW(RDSK)
    ADD_IMAGE_RO(TD0)
    ADD_IMAGE_RW(SAD)
    ADD_IMAGE_RO(SCL)
    ADD_IMAGE_RW(FDI)
    ADD_IMAGE_RW(DTI)
    ADD_IMAGE_RO(IPF)
    ADD_IMAGE_RO(MSA)
    ADD_IMAGE_RO(CQM)
    ADD_IMAGE_RO(CWTOOL)
    ADD_IMAGE_RO(UDI)
    ADD_IMAGE_RW(IMD)
    ADD_IMAGE_RO(SBT)
    ADD_IMAGE_RO(DFI)
    ADD_IMAGE_RO(SCP)
    ADD_IMAGE_RO(STREAM)
    ADD_IMAGE_RW(HFE)
    ADD_IMAGE_RW(MFI)
    ADD_IMAGE_RW(QDOS)
    ADD_IMAGE_RW(SAP)
    ADD_IMAGE_RO(WOZ)
    ADD_IMAGE_RO(PDI)
    ADD_IMAGE_RO(A2R)

    // Types with distinctive fields
    ADD_IMAGE_RO(D80)
    ADD_IMAGE_RO(ST)
    ADD_IMAGE_RO(BPB)
    ADD_IMAGE_RW(ADF)
    ADD_IMAGE_RO(DMK)
    ADD_IMAGE_RW(MBD)
    ADD_IMAGE_RW(OPD)
    ADD_IMAGE_RW(D88)
    ADD_IMAGE_RW(1DD)

    // Raw types, identified by size or extension only
    ADD_IMAGE_RW(2D)
    ADD_IMAGE_RW(TRD)
    ADD_IMAGE_RW(LIF)
    ADD_IMAGE_RO(CFI)
    ADD_IMAGE_RO(DSC)
    ADD_IMAGE_RO(SDF)
    ADD_IMAGE_RO(S24)
    ADD_IMAGE_RW(D2M)
    ADD_IMAGE_RW(D4M)
    ADD_IMAGE_RW(D81)
    ADD_IMAGE_RW(MGT)
    ADD_IMAGE_RO(DS2)
    ADD_IMAGE_RW(CPM)
    ADD_IMAGE_RW(FD)
    ADD_IMAGE_WO(DO)
    ADD_IMAGE_RW(RAW)
    ADD_IMAGE_HIDDEN_RO(Unsupported)

    #ifdef DEFINITIONS_FOR_CPP
    {
     nullptr, nullptr, nullptr
    }   // aImageTypes list terminator
};
#endif


#ifdef DEFINITIONS_FOR_CPP

DEVICE_ENTRY aDeviceTypes[] = {

#endif

ADD_DEVICE(SuperCardPro)
ADD_DEVICE(KryoFlux)
ADD_DEVICE(TrinLoad)
ADD_DEVICE(BDOS)
ADD_DEVICE(BuiltIn)
ADD_DEVICE(BlockDevice)

#ifdef HAVE_FDRAWCMD_H
ADD_DEVICE(FdrawcmdSys)
ADD_DEVICE(FdrawcmdSysAB)
#endif


#ifdef DEFINITIONS_FOR_CPP
{
    nullptr, nullptr, nullptr
}   // aDeviceTypes list terminator
};
#endif

#undef ADD_IMAGE_RW
#undef ADD_IMAGE_RO
#undef ADD_IMAGE_WO
#undef ADD_IMAGE_HIDDEN_RO

#undef ADD_DEVICE

#endif // end of ifndef TypesH2
