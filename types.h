//==============================================================================
// PROJECT:         zqloader
// FILE:            types.h
// DESCRIPTION:     Some generic types.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <chrono>
#include <iosfwd>   // forward for std::ostream

using namespace std::chrono_literals;       // never do using namespace in include files. Except this.

/// A (std::chrono) duration in sec that can also store fractional seconds
using Doublesec = std::chrono::duration<double>;
//using DurationTState = std::chrono::duration<std::int64_t, std::ratio<1, 3500000>>;

/// Edge type, used a lot by SampleSender (miniaudio) and Pulsers.
/// Also used at tzx/SymDef hence needs size 8 bits.
enum class Edge : uint8_t
{
    toggle,
    no_change,
    zero,
    one,
};

std::ostream& operator << (std::ostream& p_stream, Edge p_enum);

// order must match dialog combobox
enum class CompressionType : uint8_t
{
    none,       // No compression; just copy to m_dest_address when given
    rle,
    automatic,  // will never be send to spectrum
};

std::ostream& operator << (std::ostream& p_stream, CompressionType p_enum);

