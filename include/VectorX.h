#ifndef VECTORX_H
#define VECTORX_H

#include "Cpp_helpers.h"

#include <cstdint>
#include <vector>

template <typename T, typename IT = int>
class VectorX : public std::vector<T>
{
public:
    typedef typename VectorX::size_type ST;

    using std::vector<T>::vector;
    using std::vector<T>::operator[];
    using std::vector<T>::insert;
    using std::vector<T>::resize;
    using std::vector<T>::reserve;

    template <typename U = IT>
    explicit VectorX(U count)
        : std::vector<T>(lossless_static_cast<ST>(count))
    {
    }

    template <typename U = IT>
    VectorX(U count, const T& value)
        : std::vector<T>(lossless_static_cast<ST>(count), value)
    {
    }

    template <typename U = IT>
    typename VectorX::reference operator[](U pos)
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT>
    typename VectorX::const_reference operator[](U pos) const
    {
        return std::vector<T>::operator[](pos);
    }

    template <typename U = IT>
    typename VectorX::iterator insert(typename VectorX::const_iterator pos, U count, const T& value)
    {
        return std::vector<T>::insert(pos, count, value);
    }

    template <typename U = IT>
    U size() const
    {
        return lossless_static_cast<U>(std::vector<T>::size());
    }

    template <typename U = IT>
    void resize(U count)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count));
    }

    template <typename U = IT>
    void resize(U count, const T& value)
    {
        std::vector<T>::resize(lossless_static_cast<ST>(count), value);
    }

    template <typename U = IT>
    void reserve(U new_cap)
    {
        std::vector<T>::reserve(lossless_static_cast<ST>(new_cap));
    }
};

using Data = VectorX<uint8_t>;

using DataList = VectorX<Data>;

#endif // VECTORX_H
