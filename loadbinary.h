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
    p_stream.write(reinterpret_cast<const char*>(&p_data), sizeof(TData));
}

