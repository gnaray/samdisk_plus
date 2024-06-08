#ifndef VECTORX_H
#define VECTORX_H

#include "Cpp_helpers.h"

#include <cassert>
#include <cstdint>
#include <set>
#include <vector>

template <typename T, typename IT = int, typename Allocator = std::allocator<T>,
          std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
class VectorX : public std::vector<T, Allocator>
{
public:
    typedef typename VectorX::size_type ST;

    using std::vector<T>::vector;
#if _MSC_VER <= 1900
    VectorX() : std::vector<T>::vector() {}
#endif
    using std::vector<T>::operator[];
    using std::vector<T>::capacity;
    using std::vector<T>::insert;
    using std::vector<T>::max_size;
    using std::vector<T>::push_back;
    using std::vector<T>::resize;
    using std::vector<T>::reserve;

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    VectorX(U count, const T& value,
            const Allocator& alloc = Allocator() )
        : std::vector<T>(lossless_static_cast<ST>(count), value, alloc)
    {
    }

#if _MSC_VER > 1900
    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    explicit VectorX(U count,
                     const Allocator& alloc = Allocator() )
        : std::vector<T>(lossless_static_cast<ST>(count), alloc)
    {
    }
#endif

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    typename VectorX::reference at(U pos)
    {
        return std::vector<T>::at(pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    typename VectorX::const_reference at(U pos) const
    {
        return std::vector<T>::at(pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    typename VectorX::reference operator[](U pos)
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    typename VectorX::const_reference operator[](U pos) const
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    typename VectorX::iterator insert(typename VectorX::const_iterator pos, U count, const T& value)
    {
        return std::vector<T>::insert(pos, count, value);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    U size() const
    {
        return lossless_static_cast<U>(std::vector<T>::size());
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    U max_size() const
    {
        return lossless_static_cast<U>(std::vector<T>::max_size());
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    U capacity() const
    {
        return lossless_static_cast<U>(std::vector<T>::capacity());
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    void resize(U count)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count));
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    void resize(U count, const T& value)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count), value);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    void reserve(U new_cap)
    {
        std::vector<T>::reserve(lossless_static_cast<ST>(new_cap));
    }

    void swap_at(const typename VectorX::iterator pos1, const typename VectorX::iterator pos2)
    {
        assert(pos1 != VectorX::end() && pos2 != VectorX::end());
        std::swap(*pos1, *pos2);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    void swap_at(const U pos1, const U pos2)
    {
        assert(pos1 >= 0 && pos1 < VectorX::size() && pos2 >= 0 && pos2 < VectorX::size());
        return swap_at(VectorX::begin() + pos1, VectorX::begin() + pos2);
    }

    void push_back(const VectorX<T>& elements)
    {
        insert(VectorX::end(), elements.begin(), elements.end());
    }

    // Returns index of current location of moved value, >= 0 on success, -1 otherwise. Invalidates iterators like insert and erase methods.
    template <typename U = IT,
              std::enable_if_t<std::is_integral<U>::value> * = nullptr>
    U findAndMove(const T& value, U pos)
    {
        assert(pos >= 0 && pos < VectorX::size());
        const auto itFound = std::find(VectorX::begin(), VectorX::end(), value);
        if (itFound == VectorX::end())
            return -1; // Not found, can not move.
        auto indexFound = itFound - VectorX::begin();
        if (indexFound != pos)
        {
            VectorX::insert(VectorX::begin() + pos, value);
            if (pos < indexFound)
                indexFound++;
            else
                pos--;
            VectorX::erase(VectorX::begin() + indexFound);
        }
        return pos;
    }

    // Returns iterator of current location of moved value, not end() on success, end() otherwise. Invalidates iterators like insert method.
    typename VectorX::iterator findAndMove(const T& value, const typename VectorX::iterator pos)
    {
        const auto posIndex = findAndMove(value, std::distance(VectorX::begin(), pos));
        if (posIndex == -1)
            return VectorX::end();
        return VectorX::begin() + posIndex;
    }

    /* Remove duplicates from the range [first, last).
     * This method is similar to std::unique method but this
     * does not require sorted range, that is why this is slower than std:unique.
     * This method requires an existing std::less<T> method when using it on
     * VectorX<T>.
     * Returns iterator to the new end of the range.
     */
    typename VectorX::iterator unique(typename VectorX::iterator first, typename VectorX::iterator last)
    {
        if (first == last)
            return last;

        std::set<T> s;

        typename VectorX::iterator result = first;
        auto it = first;
        s.insert(*it);
        while (++it != last)
        {
            if (s.find(*it) == s.end())
            {
                s.insert(*it);
                if (++result != it)
                    *result = std::move(*it);
            }
        }
        return ++result;
    }

    /* Remove duplicates from the whole range of vector.
     * This method is similar to std::unique method but this
     * does not require sorted range, that is why this is slower than std:unique.
     * This method requires an existing std::less<T> method when using it on
     * VectorX<T>.
     * Returns iterator to the new end of the range.
     */
    typename VectorX::iterator unique()
    {
        return unique(VectorX::begin(), VectorX::end());
    }

    // Remove duplicates from the range [first, last) using this.unique method.
    typename VectorX::iterator RemoveDuplicates(typename VectorX::iterator first, typename VectorX::iterator last)
    {
        return VectorX::erase(unique(first, last), last);
    }

    // Remove duplicates from the whole range of vector using this.unique method.
    typename VectorX::iterator RemoveDuplicates()
    {
        return RemoveDuplicates(VectorX::begin(), VectorX::end());
    }
};

using Data = VectorX<uint8_t>;

#endif // VECTORX_H
