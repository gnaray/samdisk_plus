#ifndef VECTORX_H
#define VECTORX_H

#include "Cpp_helpers.h"

#include <cstdint>
#include <vector>

template <typename T>
class VectorX : public std::vector<T>
{
public:
    typedef typename VectorX::size_type ST;

    using std::vector<T>::vector;
    using std::vector<T>::operator[];
    using std::vector<T>::insert;
    using std::vector<T>::resize;
    using std::vector<T>::reserve;

    template <typename U>
    explicit VectorX(U count)
        : std::vector<T>(lossless_static_cast<ST>(count))
    {
    }

    template <typename U>
    VectorX(U count, const T& value)
        : std::vector<T>(lossless_static_cast<ST>(count), value)
    {
    }

    template <typename U>
    typename VectorX::reference operator[](U pos)
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U>
    typename VectorX::const_reference operator[](U pos) const
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U>
    typename VectorX::iterator insert(typename VectorX::const_iterator pos, U count, const T& value)
    {
        return std::vector<T>::insert(pos, count, value);
    }

    int size() const
    {
        return lossless_static_cast<int>(std::vector<T>::size());
    }

    template <typename U>
    void resize(U count)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count));
    }

    template <typename U>
    void resize(U count, const T& value)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count), value);
    }

    template <typename U>
    void reserve(U new_cap)
    {
        std::vector<T>::reserve(lossless_static_cast<ST>(new_cap));
    }
};

using Data = VectorX<uint8_t>;

using DataList = VectorX<Data>;

#endif // VECTORX_H
