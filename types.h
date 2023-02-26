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
using namespace std::chrono_literals;       // never do using namespace in include files. Except this.

/// A (std::chrono) duration in sec that can also store fractional seconds
using Doublesec = std::chrono::duration<double>;


// Edge type, used a lot by SampleSender (miniadio) and Pulsers.
enum class Edge
{
    one,
    zero,
    no_change,
    toggle
};
