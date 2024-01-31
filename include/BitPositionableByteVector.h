#ifndef BitPositionableByteVectorH
#define BitPositionableByteVectorH
//---------------------------------------------------------------------------

#include "ByteBitPosition.h"
#include "VectorX.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

inline uint8_t Double4Bits(uint8_t bits)
{
    return (bits & 8) * 0x18u | (bits & 4) * 0xcu | (bits & 2) * 0x6u | (bits & 1) * 0x3u;
}

class BitPositionableByteVector
{
public:
    BitPositionableByteVector() = default;

    BitPositionableByteVector(int size)
        : m_bytes(size), m_byteBitPosition(0)
    {
    }

    BitPositionableByteVector(const Data& bytes)
        : m_bytes(bytes), m_byteBitPosition(0)
    {
    }

    BitPositionableByteVector(const uint8_t* pb, int len)
        : m_bytes(pb, pb + len), m_byteBitPosition(0)
    {
    }

    constexpr ByteBitPosition GetByteBitPosition() const
    {
        return m_byteBitPosition;
    }

    constexpr void SetByteBitPosition(const ByteBitPosition& byteBitPosition)
    {
        m_byteBitPosition = byteBitPosition;
    }

    inline ByteBitPosition BytesBitEndPosition() const
    {
        return ByteBitPosition(BytesBitSize());
    }

    inline uint8_t PeekByteAtPosition(const ByteBitPosition& byteBitPosition) const
    {
        assert(m_bytes.size() * UINT8_T_BIT_SIZE >= byteBitPosition + UINT8_T_BIT_SIZE);
        return byteBitPosition.BitPosition() == 0
            ? m_bytes[byteBitPosition.BytePosition()]
            : static_cast<uint8_t>((m_bytes[byteBitPosition.BytePosition()] << byteBitPosition.BitPosition())
                | (m_bytes[byteBitPosition.BytePosition() + 1] >> (UINT8_T_BIT_SIZE - byteBitPosition.BitPosition())));
    }

    inline uint8_t PeekByte(const ByteBitPosition* byteBitPosition = nullptr) const
    {
        const auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        return PeekByteAtPosition(*byteBitPositionSelected);
    }

    inline uint8_t ReadByte(ByteBitPosition* byteBitPosition = nullptr)
    {
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        const auto result = PeekByteAtPosition(*byteBitPositionSelected);
        byteBitPositionSelected->PreAddBytes(1);
        return result;
    }

    void ReadBytes(uint8_t* mem, int size, ByteBitPosition* byteBitPosition = nullptr)
    {
        if (size == 0)
            return;
        const auto remainingByteSize = RemainingByteLength();
        assert(remainingByteSize >= size);
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        for (auto i = 0; i < size; i++)
            *(mem++) = ReadByte(byteBitPositionSelected);
    }

    inline void ReadBytes(Data& dataBytes, ByteBitPosition* byteBitPosition = nullptr)
    {
        ReadBytes(dataBytes.data(), dataBytes.size(), byteBitPosition);
    }

    inline void StoreByteAtPosition(uint8_t byte, const ByteBitPosition& byteBitPosition)
    {
        if (RemainingByteLength() < 1)
            m_bytes.resize(m_bytes.size() + 1);
        auto bytePosition = byteBitPosition.BytePosition();
        const auto bitPosition = byteBitPosition.BitPosition();
        if (byteBitPosition.BitPosition() == 0)
            m_bytes[byteBitPosition.BytePosition()] = byte;
        else
        {
            m_bytes[bytePosition] =
                    (m_bytes[bytePosition] & static_cast<uint8_t>(0xff00u >> bitPosition)) | byte >> bitPosition;
            ++bytePosition;
            m_bytes[bytePosition] =
                    (m_bytes[bytePosition] & 0xffu >> bitPosition) | static_cast<uint8_t>(byte << (UINT8_T_BIT_SIZE - bitPosition));
        }
    }

    inline void StoreByte(uint8_t byte, ByteBitPosition* byteBitPosition = nullptr)
    {
        const auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        StoreByteAtPosition(byte, *byteBitPositionSelected);
    }

    inline void WriteByte(uint8_t byte, ByteBitPosition* byteBitPosition = nullptr)
    {
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        StoreByteAtPosition(byte, *byteBitPositionSelected);
        byteBitPositionSelected->PreAddBytes(1);
    }

    void WriteBytes(const uint8_t* mem, int size, ByteBitPosition* byteBitPosition = nullptr)
    {
        if (size == 0)
            return;
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        const auto remainingByteSize = RemainingByteLength();
        if (remainingByteSize < size)
            m_bytes.resize(m_bytes.size() + size - remainingByteSize);
        const auto iSup = size;
        for (auto i = 0; i < iSup; i++)
            WriteByte(*(mem++), byteBitPositionSelected);
    }

    void WriteBytes(const Data& dataBytes, ByteBitPosition* byteBitPosition = nullptr)
    {
        WriteBytes(dataBytes.data(), dataBytes.size(), byteBitPosition);
    }

    void CopyBytesFrom(BitPositionableByteVector& srcBytes, int byteLen, ByteBitPosition* srcByteBitPosition = nullptr, ByteBitPosition* dstByteBitPosition = nullptr)
    {
        if (byteLen == 0)
            return;
        auto srcByteBitPositionSelected = srcByteBitPosition == nullptr ? &srcBytes.m_byteBitPosition : srcByteBitPosition;
        auto dstByteBitPositionSelected = dstByteBitPosition == nullptr ? &m_byteBitPosition : dstByteBitPosition;
        const auto srcRemainingByteSize = srcBytes.RemainingByteLength(srcByteBitPositionSelected);
        assert(srcRemainingByteSize >= byteLen);
        const auto dstRemainingByteSize = RemainingByteLength();
        if (dstRemainingByteSize < byteLen)
            m_bytes.resize(m_bytes.size() + byteLen - dstRemainingByteSize);
        for (auto i = 0; i < byteLen; i++)
            WriteByte(ReadByte(srcByteBitPositionSelected), dstByteBitPositionSelected);
    }

    uint8_t ReadBits(int bitsLen, ByteBitPosition* byteBitPosition = nullptr)
    {
        if (bitsLen == 0)
            return 0;
        assert(bitsLen < UINT8_T_BIT_SIZE);
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        const auto remainingBitSize = RemainingBitLength(byteBitPositionSelected);
        assert(remainingBitSize >= bitsLen);
        uint8_t result;
        const auto remainingBitsInByte = UINT8_T_BIT_SIZE - byteBitPositionSelected->BitPosition();
        if (bitsLen > remainingBitsInByte)
            result = PeekByteAtPosition(*byteBitPositionSelected) >> byteBitPositionSelected->BitPosition();
        else
            result = ((m_bytes[byteBitPositionSelected->BytePosition()] & (0xffu >> byteBitPositionSelected->BitPosition())) >> (remainingBitsInByte - bitsLen));
        *byteBitPositionSelected += bitsLen;
        return result;
    }

    void WriteBits(uint8_t bits, int bitsLen, ByteBitPosition* byteBitPosition = nullptr)
    {
        if (bitsLen == 0)
            return;
        assert(bitsLen < UINT8_T_BIT_SIZE);
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        const auto remainingBitSize = RemainingBitLength();
        if (remainingBitSize < bitsLen)
            m_bytes.resize(m_bytes.size() + ByteSizeHavingBits(bitsLen - remainingBitSize));
        const auto bytePosition = byteBitPositionSelected->BytePosition();
        const auto bitPosition = byteBitPositionSelected->BitPosition();
        const auto remainingBitsInByte = UINT8_T_BIT_SIZE - bitPosition;
        if (bitsLen > remainingBitsInByte)
        {
            const auto bitMask = static_cast<uint8_t>(0xffu >> bitsLen);
            auto peekByte = PeekByteAtPosition(*byteBitPositionSelected);
            peekByte =
                    (peekByte & bitMask)
                    | (bits << static_cast<uint8_t>(UINT8_T_BIT_SIZE - bitsLen) & ~bitMask);
            StoreByteAtPosition(peekByte, *byteBitPositionSelected);
//            WriteBits(bits >> written, bitsLen - written, byteBitPositionSelected);
        }
        else
        {
            const auto bitMask = static_cast<uint8_t>(0xff00u >> bitPosition | 0xffu >> (bitPosition + bitsLen));
            m_bytes[bytePosition] =
                    (m_bytes[bytePosition] & bitMask)
                    | (bits << static_cast<uint8_t>(remainingBitsInByte - bitsLen) & ~bitMask);
        }
        *byteBitPositionSelected += bitsLen;
    }

    void CopyBitsFrom(BitPositionableByteVector& srcBits, int bitsLen, ByteBitPosition* srcByteBitPosition = nullptr, ByteBitPosition* dstByteBitPosition = nullptr)
    {
        if (bitsLen == 0)
            return;
        auto srcByteBitPositionSelected = srcByteBitPosition == nullptr ? &srcBits.m_byteBitPosition : srcByteBitPosition;
        auto dstByteBitPositionSelected = dstByteBitPosition == nullptr ? &m_byteBitPosition : dstByteBitPosition;
        const auto srcRemainingBitSize = srcBits.RemainingBitLength(srcByteBitPositionSelected);
        assert(srcRemainingBitSize >= bitsLen);
        const auto dstRemainingBitSize = RemainingBitLength();
        if (dstRemainingBitSize < bitsLen)
            m_bytes.resize(m_bytes.size() + ByteSizeHavingBits(bitsLen - dstRemainingBitSize));
        // Copy bits so dst will be at byte boundary.
        if (dstByteBitPositionSelected->BitPosition() > 0)
        {
            auto dstRemainingBitsInByte = std::min(UINT8_T_BIT_SIZE - dstByteBitPositionSelected->BitPosition(), bitsLen);
            auto srcBitsBits = srcBits.ReadBits(dstRemainingBitsInByte, srcByteBitPositionSelected);
            WriteBits(srcBitsBits, dstRemainingBitsInByte, dstByteBitPositionSelected);
            bitsLen -= dstRemainingBitsInByte;
        }
        // Copy bytes because dst is at byte boundary.
        if (bitsLen >= UINT8_T_BIT_SIZE)
        {
            const auto byteLen = bitsLen / UINT8_T_BIT_SIZE;
            for (auto i = 0; i < byteLen; i++)
                WriteByte(srcBits.ReadByte(srcByteBitPositionSelected), dstByteBitPositionSelected);
            bitsLen -= byteLen * UINT8_T_BIT_SIZE;
        }
        // Copy the remaning bits.
        if (bitsLen > 0)
        {
            auto srcBitsBits = srcBits.ReadBits(bitsLen, srcByteBitPositionSelected);
            WriteBits(srcBitsBits, bitsLen, dstByteBitPositionSelected);
            bitsLen -= bitsLen;
        }
    }

    void CopyBitsDoubledFrom(BitPositionableByteVector& srcBits, int bitsLen, ByteBitPosition* srcByteBitPosition = nullptr, ByteBitPosition* dstByteBitPosition = nullptr)
    {
        if (bitsLen == 0)
            return;
        auto srcByteBitPositionSelected = srcByteBitPosition == nullptr ? &srcBits.m_byteBitPosition : srcByteBitPosition;
        auto dstByteBitPositionSelected = dstByteBitPosition == nullptr ? &m_byteBitPosition : dstByteBitPosition;
        const auto srcRemainingBitSize = srcBits.RemainingBitLength(srcByteBitPositionSelected);
        assert(srcRemainingBitSize >= bitsLen);
        auto dstBitsLen = bitsLen * 2; // The bits are written doubled.
        const auto dstRemainingBitSize = RemainingBitLength();
        if (dstRemainingBitSize < dstBitsLen)
            m_bytes.resize(m_bytes.size() + ByteSizeHavingBits(dstBitsLen - dstRemainingBitSize));
        while (bitsLen >= UINT8_T_BIT_SIZE)
        {
            // Copy bits so dst will be at byte boundary.
            auto srcBitsByte = srcBits.ReadByte(srcByteBitPositionSelected);
            WriteByte(Double4Bits(srcBitsByte >> 4), dstByteBitPositionSelected);
            WriteByte(Double4Bits(srcBitsByte & 0xfu), dstByteBitPositionSelected);
            bitsLen -= UINT8_T_BIT_SIZE;
        }
        if (bitsLen > 0)
        {
            auto srcBitsBits = srcBits.ReadBits(bitsLen, srcByteBitPositionSelected);
            dstBitsLen = bitsLen * 2; // The bits are written doubled.
            if (dstBitsLen >= UINT8_T_BIT_SIZE)
            {
                WriteByte(Double4Bits(srcBitsBits >> (bitsLen - 4)), dstByteBitPositionSelected);
                dstBitsLen -= UINT8_T_BIT_SIZE;
            }
            WriteBits(Double4Bits(srcBitsBits), dstBitsLen, dstByteBitPositionSelected);
        }
    }

    constexpr void StepBit(ByteBitPosition* byteBitPosition = nullptr)
    {
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        (*byteBitPositionSelected)++;
    }

    constexpr void StepBytes(int bytes, ByteBitPosition* byteBitPosition = nullptr)
    {
        auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        (*byteBitPositionSelected).PreAddBytes(bytes);
    }

    int RemainingByteLength(const ByteBitPosition* byteBitPosition = nullptr)
    {
        const auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        auto affectedBytePosition = byteBitPositionSelected->BytePosition() + (byteBitPositionSelected->BitPosition() > 0 ? 1 : 0);
        assert(BytesByteSize() >= affectedBytePosition);
        return BytesByteSize() - affectedBytePosition;
    }

    int RemainingBitLength(const ByteBitPosition* byteBitPosition = nullptr)
    {
        const auto byteBitPositionSelected = byteBitPosition == nullptr ? &m_byteBitPosition : byteBitPosition;
        auto affectedBytePosition = byteBitPositionSelected->TotalBitPosition();
        assert(BytesBitSize() >= affectedBytePosition);
        return BytesBitSize() - affectedBytePosition;
    }

    inline int ByteSizeHavingBits(int bitSize)
    {
        return bitSize == 0 ? 0 : (bitSize - 1) / UINT8_T_BIT_SIZE + 1;
    }

    Data& Bytes()
    {
        return m_bytes;
    }

    const Data& Bytes() const
    {
        return m_bytes;
    }

    inline int BytesByteSize() const
    {
        return m_bytes.size();
    }

    inline int BytesBitSize() const
    {
        return BytesByteSize() * UINT8_T_BIT_SIZE;
    }

private:
    Data m_bytes{};
    ByteBitPosition m_byteBitPosition{};
};


//---------------------------------------------------------------------------
#endif
