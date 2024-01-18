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

static const int g_spectrum_clock = 3500000;
static const Doublesec g_tstate_dur = 1s / double(g_spectrum_clock); // Tstate duration as std::duration in seconds

static const uint16_t SCREEN_23RD = 16384 + 4 * 1024;
static const uint16_t SCREEN_END  = 16384 + 6 * 1024;

// https://worldofspectrum.org/faq/reference/48kreference.htm
static const int g_tstate_leader = 2168;
static const int g_tstate_sync1 = 667;
static const int g_tstate_sync2 = 735;
static const int g_tstate_zero = 855;
static const int g_tstate_one = 1710;
// To speed up even ROM loading a bit:
static const int g_tstate_quick_zero = 700;




