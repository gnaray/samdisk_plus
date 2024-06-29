#pragma once

#include "config.h" // IWYU pragma: keep

#ifdef HAVE_FTDI

#include "PlatformConfig.h" // On Windows: it must precede ftdi.h which includes Windows parts.
#include "SuperCardPro.h"

#include <ftdi.h>

class SuperCardProFTDI final : public SuperCardPro
{
public:
    SuperCardProFTDI(const SuperCardProFTDI&) = delete;
    void operator= (const SuperCardProFTDI&) = delete;
    ~SuperCardProFTDI();
    static std::unique_ptr<SuperCardPro> Open();

private:
    explicit SuperCardProFTDI(ftdi_context* hdev);

    bool Read(void* p, int len, int* bytes_read) override;
    bool Write(const void* p, int len, int* bytes_written) override;

    ftdi_context* m_hdev;
    int m_status = 0;
};

#endif // HAVE_FTDI
