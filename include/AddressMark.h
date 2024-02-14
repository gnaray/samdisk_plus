#ifndef AddressMarkH
#define AddressMarkH
//---------------------------------------------------------------------------

// https://deramp.com/downloads/floppy_drives/FD1771%20Floppy%20Controller.pdf

#include "utils.h"

#include <string>

class AddressMark
{
public:
    enum AddressMarkEnum : uint8_t
    {
        UNDEFINED = 0,
        INDEX = 0xFC,
        ID = 0xFE,
        BAD_TRACK_ID = 0xFE, // Not sure what this is. Maybe not 0xFE, maybe 0xFF?. See http://www.nj7p.org/Manuals/PDFs/Intel/AFN-00223B.pdf
        DATA = 0xFB,
        ALT_DATA = 0xFA,
        DELETED_DATA = 0xF8,
        ALT_DELETED_DATA = 0xF9,
        RX02 = 0xFD
    };

    AddressMark() = default;

    constexpr AddressMark(uint8_t addressMarkValue)
    {
         *this = addressMarkValue;
    }

    constexpr AddressMark(const AddressMark& addressMark) : m_addressMark(addressMark)
    {
    }

    constexpr AddressMark(const AddressMarkEnum& addressMarkEnum)
     : m_addressMark(addressMarkEnum)
    {
    }

    AddressMark& operator=(const AddressMark& addressMark)
    {
        m_addressMark = addressMark.m_addressMark;
        return *this;
    }

/*
    constexpr AddressMark& operator=(const AddressMarkEnum& addressMarkEnum)
    {
        m_addressMark = addressMarkEnum;
        return *this;
    }
*/
    constexpr AddressMark& operator=(const uint8_t addressMarkValue)
    {
        switch (addressMarkValue) {
        case UNDEFINED:
            m_addressMark = UNDEFINED;
            break;
        case INDEX:
            m_addressMark = INDEX;
            break;
        case ID:
            m_addressMark = ID;
            break;
        //case BAD_TRACK_ID:
        //    m_addressMark = BAD_TRACK_ID;
        //    break;
        case DATA:
            m_addressMark = DATA;
            break;
        case ALT_DATA:
            m_addressMark = ALT_DATA;
            break;
        case DELETED_DATA:
            m_addressMark = DELETED_DATA;
            break;
        case ALT_DELETED_DATA:
            m_addressMark = ALT_DELETED_DATA;
            break;
        case RX02:
            m_addressMark = RX02;
            break;
        default:
            throw util::exception("Invalid addressmark value (addressMarkValue)");
        }
        return *this;
    }

    static constexpr bool IsValid(const uint8_t addressMarkValue)
    {
        switch (addressMarkValue) {
        case UNDEFINED:
        case INDEX:
        case ID:
        //case BAD_TRACK_ID:
        case DATA:
        case ALT_DATA:
        case DELETED_DATA:
        case ALT_DELETED_DATA:
        case RX02:
            return true;
        default: return false;
        }
    }

    // Allow switch and comparisons.
    constexpr operator AddressMarkEnum() const
    { return m_addressMark; }

    constexpr bool operator==(const AddressMark& rhs) const
    { return m_addressMark == rhs.m_addressMark; }
    constexpr bool operator!=(const AddressMark& rhs) const
    { return !(*this == rhs); }
    constexpr bool operator==(const AddressMarkEnum& rhs) const
    { return m_addressMark == rhs; }
    constexpr bool operator!=(const AddressMarkEnum& rhs) const
    { return !(*this == rhs); }

    constexpr bool IsId() const
    { return m_addressMark == ID; }

    constexpr bool IsData() const
    { return m_addressMark == DATA; }

    constexpr bool IsDeletedData() const
    { return m_addressMark == DELETED_DATA; }

    std::string ToString(bool /*onlyRelevantData*/ = true) const
    {
        switch (m_addressMark) {
        case UNDEFINED: return "UNDEFINED";
        case INDEX: return "INDEX";
        case ID: return "ID";
        //case BAD_TRACK_ID: return "BAD_TRACK_ID";
        case DATA: return "DATA";
        case ALT_DATA: return "ALT_DATA";
        case DELETED_DATA: return "DELETED_DATA";
        case ALT_DELETED_DATA: return "ALT_DELETED_DATA";
        case RX02: return "RX02";
        }
        return "invalid AddressMarkEnum error";
    }

    friend std::string to_string(const AddressMark& addressMark, bool onlyRelevantData = true)
    {
        std::ostringstream ss;
        ss << addressMark.ToString(onlyRelevantData);
        return ss.str();
    }

private:
    AddressMarkEnum m_addressMark{UNDEFINED};
};

inline std::ostream& operator<<(std::ostream& os, const AddressMark& addressMark) { return os << addressMark.ToString(); }


//---------------------------------------------------------------------------
#endif
