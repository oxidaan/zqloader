//==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.h
// DESCRIPTION:     Some ZX spectrum consts
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <cstdint>      // uint16_t
#include "types.h"      // Doublesec

static const int m_spectrum_clock = 3500000;
static const Doublesec m_tstate_dur = 1s / double(m_spectrum_clock); // Tstate duration as std::duration in seconds

static const uint16_t SCREEN_23RD = 16384 + 4 * 1024;
static const uint16_t SCREEN_END  = 16384 + 6 * 1024;



