#pragma once

#include "VectorX.h"

#include <array>
#include <mutex>

class CRC16
{
public:
    static constexpr const uint16_t POLYNOMIAL = 0x1021;
    static constexpr const uint16_t INIT_CRC = 0xffff;
    static constexpr const uint16_t A1A1A1 = 0xcdb4;      // CRC of 0xa1, 0xa1, 0xa1

    explicit CRC16(uint16_t init = INIT_CRC);
    explicit CRC16(const Data& data, uint16_t init = INIT_CRC);

    template<typename T>
    CRC16(const void* buf, T len, uint16_t init_ = INIT_CRC)
        : CRC16(init_)
    {
        add(buf, len);
    }

    operator uint16_t () const;

    void init(uint16_t crc = INIT_CRC);
    uint16_t add(uint8_t byte);

    template<typename T>
    uint16_t add(uint8_t byte, T len)
    {
        while (len-- > 0)
            add(byte);

        return m_crc;
    }

    template<typename T>
    uint16_t add(const void* buf, T len)
    {
        const uint8_t* pb = reinterpret_cast<const uint8_t*>(buf);
        while (len-- > 0)
            add(*pb++);

        return m_crc;
    }

    uint16_t add(const Data& data);

    template<typename T>
    uint16_t add(const Data& data, T len)
    {
        return add(data.data(), len);
    }

    uint8_t lsb() const;
    uint8_t msb() const;

private:
    static void init_crc_table();
    static std::array<uint16_t, 256> s_crc_lookup;
    static std::once_flag flag;

    uint16_t m_crc = INIT_CRC;
};
