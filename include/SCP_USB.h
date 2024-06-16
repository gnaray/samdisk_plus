#pragma once

#include "SuperCardPro.h"

class SuperCardProUSB final : public SuperCardPro
{
public:
    ~SuperCardProUSB() override;
    static std::unique_ptr<SuperCardPro> Open();

private:
    explicit SuperCardProUSB(int fd);

    bool Read(void* p, int len, int* bytes_read) override;
    bool Write(const void* p, int len, int* bytes_written) override;

    int m_fd = -1;
    int m_error = 0; // TODO Why shadowing inherited uint8_t m_error?
};
