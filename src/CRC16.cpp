// CRC-16-CCITT implementation

#include "CRC16.h"
#include "Cpp_helpers.h"

typedef std::array<uint16_t, 256> Uint16Array256;
typedef Uint16Array256::size_type Uint16Array256ST;
Uint16Array256 CRC16::s_crc_lookup;
std::once_flag CRC16::flag;


CRC16::CRC16(uint16_t init_)
{
    std::call_once(flag, init_crc_table);
    init(init_);
}

CRC16::CRC16(const void* buf, size_t len, uint16_t init_)
{
    std::call_once(flag, init_crc_table);
    init(init_);
    add(buf, len);
}

void CRC16::init_crc_table()
{
    if (!s_crc_lookup[0])
    {
        for (int i = 0; i < 256; ++i)
        {
            uint16_t crc = lossless_static_cast<uint16_t>(i << 8);

            for (int j = 0; j < 8; ++j)
                crc = static_cast<uint16_t>(crc << 1) ^ ((crc & 0x8000) ? POLYNOMIAL : 0);

            s_crc_lookup[static_cast<Uint16Array256ST>(i)] = crc;
        }
    }
}

CRC16::operator uint16_t () const
{
    return m_crc;
}

void CRC16::init(uint16_t init_crc)
{
    m_crc = init_crc;
}

uint16_t CRC16::add(uint8_t byte)
{
    m_crc = static_cast<uint16_t>(m_crc << 8) ^ s_crc_lookup[((m_crc >> 8) ^ byte) & 0xff];
    return m_crc;
}

uint16_t CRC16::add(uint8_t byte, int len)
{
    if (len <= 0)
        return m_crc;
    return add(byte, static_cast<size_t>(len));
}

uint16_t CRC16::add(uint8_t byte, size_t len)
{
    while (len-- > 0)
        add(byte);

    return m_crc;
}

uint16_t CRC16::add(const void* buf, int len)
{
    if (len <= 0)
        return m_crc;
    return add(buf, static_cast<size_t>(len));
}

uint16_t CRC16::add(const void* buf, size_t len)
{
    const uint8_t* pb = reinterpret_cast<const uint8_t*>(buf);
    while (len-- > 0)
        add(*pb++);

    return m_crc;
}

uint8_t CRC16::msb() const
{
    return m_crc >> 8;
}

uint8_t CRC16::lsb() const
{
    return m_crc & 0xff;
}
