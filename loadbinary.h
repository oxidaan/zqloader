//==============================================================================
// PROJECT:         zqloader
// FILE:            loadbinary.h
// DESCRIPTION:     Definition of LoadBinary functions.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <iostream>
#include <vector>
//#include <iomanip>

/// Load data binary from given stream.
template <class TData>
TData LoadBinary(std::istream& p_stream)
{
    TData data;
    p_stream.read(reinterpret_cast<char*>(&data), sizeof(TData));
    return data;
}
/// Load a std::string binary from given stream.
template <class TData>
TData LoadBinary(std::istream& p_stream, size_t p_len)
{
    std::vector<char>buf;
    buf.resize(p_len + 1);
    p_stream.read(buf.data(), p_len);
    buf[p_len] = 0;
    return buf.data();
}
/// Write data binary to given stream.
template <class TData>
void WriteBinary(std::ostream& p_stream, const TData &p_data)
{
#if 1
    const char *ptr = reinterpret_cast<const char *>(&p_data);
    for(int n = 0; n < sizeof(TData); n++)
    {
        std::cout << std::hex << int(*ptr & 0xff) << ' ';
        ptr++;
    }
    if constexpr(std::is_same_v<TData, uint8_t>)
    {
        std::cout << std::dec << '(' << p_data << ')' << ' ';
    }
    else if constexpr(std::is_same_v<TData, uint16_t>)
    {
        std::cout << std::dec << '(' << p_data << ')' << ' ';
    }
    else if constexpr(std::is_same_v<TData, uint32_t>)
    {
        std::cout << std::dec << '(' << p_data << ')' << ' ';
    }
    std::cout << std::dec;
#endif
    p_stream.write(reinterpret_cast<const char*>(&p_data), sizeof(TData));
}

