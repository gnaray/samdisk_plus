#include "Range.h"
#include "Util.h"

#include <iomanip>

Range::Range(int num_cyls, int num_heads)
    : Range(0, num_cyls, 0, num_heads)
{
}

Range::Range(int cyl_begin_, int cyl_end_, int head_begin_, int head_end_)
    : cyl_begin(cyl_begin_), cyl_end(cyl_end_), head_begin(head_begin_), head_end(head_end_)
{
    assert(cyl_begin >= 0 && cyl_begin <= cyl_end);
    assert(head_begin >= 0 && head_begin <= head_end);
}

bool Range::empty() const
{
    return cyls() <= 0 || heads() <= 0;
}

int Range::cyls() const
{
    return cyl_end - cyl_begin;
}

int Range::heads() const
{
    return head_end - head_begin;
}

bool Range::contains(const CylHead& cylhead)
{
    return cylhead.cyl >= cyl_begin && cylhead.cyl < cyl_end &&
        cylhead.head >= head_begin && cylhead.head < head_end;
}

void Range::each(const std::function<void(const CylHead & cylhead)>& func, bool cyls_first/*=false*/) const
{
    if (cyls_first && heads() > 1)
    {
        for (auto head = head_begin; head < head_end; ++head)
            for (auto cyl = cyl_begin; cyl < cyl_end; ++cyl)
                func(CylHead(cyl, head));
    }
    else
    {
        for (auto cyl = cyl_begin; cyl < cyl_end; ++cyl)
            for (auto head = head_begin; head < head_end; ++head)
                func(CylHead(cyl, head));
    }
}

std::string Range::ToString(bool /*onlyRelevantData*//* = true*/) const
{
    if (empty())
        return "All Tracks";

    std::ostringstream ss;
    auto separator = ", ";

    if (cyls() == 1)
        ss << "Cyl " << CylStr(cyl_begin);
    else if (cyl_begin == 0)
    {
        ss << std::setw(2) << cyl_end << " Cyls";
        separator = " ";
    }
    else
        ss << "Cyls " << CylStr(cyl_begin) << '-' << CylStr(cyl_end - 1);

    if (heads() == 1)
        ss << " Head " << head_begin;
    else if (head_begin == 0)
        ss << separator << head_end << " Heads";
    else
        ss << " Heads " << head_begin << '-' << (head_end - 1);

    return ss.str();
}
