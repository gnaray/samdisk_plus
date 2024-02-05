#ifndef VECTORX_H
#define VECTORX_H

#include "Cpp_helpers.h"

#include <cassert>
#include <cstdint>
#include <vector>

template <typename T, typename IT = int,
          std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
class VectorX : public std::vector<T>
{
public:
    typedef typename VectorX::size_type ST;

    using std::vector<T>::vector;
    using std::vector<T>::operator[];
    using std::vector<T>::insert;
    using std::vector<T>::resize;
    using std::vector<T>::reserve;

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    explicit VectorX(U count)
        : std::vector<T>(lossless_static_cast<ST>(count))
    {
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    VectorX(U count, const T& value)
        : std::vector<T>(lossless_static_cast<ST>(count), value)
    {
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    typename VectorX::reference at(U pos)
    {
        return std::vector<T>::at(pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    typename VectorX::const_reference at(U pos) const
    {
        return std::vector<T>::at(pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    typename VectorX::reference operator[](U pos)
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    typename VectorX::const_reference operator[](U pos) const
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    typename VectorX::iterator insert(typename VectorX::const_iterator pos, U count, const T& value)
    {
        return std::vector<T>::insert(pos, count, value);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    U size() const
    {
        return lossless_static_cast<U>(std::vector<T>::size());
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    void resize(U count)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count));
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    void resize(U count, const T& value)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count), value);
    }

    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
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
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
    void swap_at(const U pos1, const U pos2)
    {
        assert(pos1 >= 0 && pos1 < VectorX::size() && pos2 >= 0 && pos2 < VectorX::size());
        return swap_at(VectorX::begin() + pos1, VectorX::begin() + pos2);
    }

    // Returns index of current location of moved value, >= 0 on success, -1 otherwise. Invalidates iterators like insert and erase methods.
    template <typename U = IT,
              std::enable_if_t<std::is_integral<IT>::value> * = nullptr>
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
};

using Data = VectorX<uint8_t>;

#endif // VECTORX_H
