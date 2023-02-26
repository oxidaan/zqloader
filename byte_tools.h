#pragma once
#include <cstddef>      // std::byte

/// Define literal for std::byte
/// eg 123_byte
inline std::byte operator"" _byte(unsigned long long p_int)
{
    return std::byte(p_int);
}


/// Make std::byte 'usable' for comression algorithms.
inline std::byte& operator ++(std::byte& p_byte)
{
    p_byte = std::byte(int(p_byte) + 1);
    return p_byte;
}
/// Make std::byte 'usable' for comression algorithms.
inline std::byte& operator --(std::byte& p_byte)
{
    p_byte = std::byte(int(p_byte) - 1);
    return p_byte;
}

/// Make std::byte 'usable' for comression algorithms.
/// Get maximum value for given TData including...
template <class TData>
TData GetMax()
{
    return std::numeric_limits<TData>::max();
}

/// ... maximum value for std::byte
/// Make std::byte 'usable' for comression algorithms.
template <>
inline std::byte GetMax<std::byte>()
{
    return 255_byte;
}

