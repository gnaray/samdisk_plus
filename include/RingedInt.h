#ifndef RingedIntH
#define RingedIntH
//---------------------------------------------------------------------------

class RingedInt
{
public:
    RingedInt()
        : RingedInt(0, 0)
    {
    }

    RingedInt(int value, int sup, int min = 0)
        : m_min(min), m_sup(sup)
    {
        m_value = ValueToRingedInt(value);
    }

    inline int Value() const
    {
        return m_value;
    }

    inline int Min() const
    {
        return m_min;
    }

    inline int Sup() const
    {
        return m_sup;
    }

    void SetSup(int sup)
    {
        m_sup = sup;
        assert(m_sup >= m_min);
    }

    inline int Max() const
    {
        return m_sup - 1;
    }

    inline bool IsEmpty() const
    {
        return m_sup == m_min;
    }

    int ValueToRingedInt(const int value) const
    {
        assert(m_sup >= m_min);
        if (IsEmpty())
            return m_min;
        return modulo(value - m_min, static_cast<unsigned>(m_sup - m_min)) + m_min;
    }

/*  // Too many ambigous operator overloads...
    constexpr operator size_t() const
    {
        return Value();
    }
*/
//	auto operator<=>(const RingedInt&) const = default;

    constexpr friend bool operator==(const RingedInt& lhs, const RingedInt& rhs)
    {
        return lhs.m_value == rhs.m_value && lhs.m_min == rhs.m_min && lhs.m_sup == rhs.m_sup;
    }

    constexpr friend bool operator==(const RingedInt& lhs, int value)
    {
        return lhs.m_value == value;
    }

    constexpr friend bool operator<(const RingedInt& lhs, int value)
    {
        return lhs.m_value < value;
    }

    RingedInt& operator=(int value)
    {
        m_value = ValueToRingedInt(value);
        return *this;
    }

    RingedInt& operator+=(int value)
    {
        *this = ValueToRingedInt(m_value + value);
        return *this;
    }

    // Friends defined inside class body are inline and are hidden from non-ADL lookup.
    // Passing lhs by value helps optimize chained a+b+c,
    // otherwise both parameters may be const references.
    friend RingedInt operator+(RingedInt lhs, int value)
    {
        lhs += value; // reuse compound assignment
        return lhs; // return the result by value (uses move constructor)
    }

    RingedInt& operator++()
    {
        assert(m_sup >= m_min);
        if (IsEmpty())
            m_value = m_min;
        else if (++m_value == m_sup)
            m_value = m_min;
        return *this;
    }

    RingedInt operator++(int)
    {
        RingedInt temp = *this;
        ++*this;

        return temp;
    }

    RingedInt& operator-=(int value)
    {
        *this = ValueToRingedInt(m_value - value);
        return *this;
    }

    // Friends defined inside class body are inline and are hidden from non-ADL lookup.
    // Passing lhs by value helps optimize chained a+b+c,
    // otherwise both parameters may be const references.
    friend RingedInt operator-(RingedInt lhs, int value)
    {
        lhs -= value; // reuse compound assignment
        return lhs; // return the result by value (uses move constructor)
    }

    RingedInt& operator--()
    {
        assert(m_sup > m_min);
        if (IsEmpty())
            m_value = m_min;
        else if (m_value-- == m_min)
            m_value = m_sup - 1;
        return *this;
    }

    RingedInt operator--(int)
    {
        RingedInt temp = *this;
        --*this;

        return temp;
    }

    RingedInt PreIncUnwrapped()
    {
        assert(m_sup >= m_min);
        if (IsEmpty())
            m_value = m_min;
        else
            m_value++;
        return *this;
    }

    RingedInt PostIncUnwrapped()
    {
        assert(m_sup >= m_min);
        RingedInt temp = *this;
        PreIncUnwrapped();
        return temp;
    }

private:
    // Order is important for relational (e.g. spaceship) operator.
    int m_value = 0;
    int m_min = 0;
    int m_sup = 0; // sup = max + 1.
};

constexpr bool operator!=(const RingedInt& lhs, const RingedInt& rhs)
{
    return !(lhs == rhs);
}

constexpr bool operator!=(const RingedInt& lhs, int value)
{
    return !(lhs == value);
}

constexpr inline bool operator<=(const RingedInt& lhs, int value) { return lhs < value || lhs == value; }
constexpr inline bool operator> (const RingedInt& lhs, int value) { return !(lhs <= value); }
constexpr inline bool operator>=(const RingedInt& lhs, int value) { return !(lhs < value); }

//---------------------------------------------------------------------------
#endif
