// ==============================================================================
// PROJECT:         zqloader
// FILE:            loader_defaults.h
// DESCRIPTION:     Defaults for ZQ loader
//
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

namespace loader_defaults
{
constexpr int zero_duration                = 81;  // 91;  // @@ see zqloader.asm (old: 118)
constexpr int one_duration                 = 241; // 241; //int(91 + 3.5 * 43); //250;  231 worked better with jsw3.z80!? (old: 293)
constexpr int end_of_byte_delay            = 64;

constexpr int bit_loop_max                 = 0;   // when 0 dont set (so keep defaults at zqloader.z80asm) (old: 12)
constexpr int bit_one_threshold            = 0;   // when 0 dont set (so keep defaults at zqloader.z80asm) (old: 4)

constexpr int volume_left                  = 100;
constexpr int volume_right                 = 100;
constexpr int sample_rate                  = 0;                 // 0 is device default (eg 48000hz)

constexpr CompressionType compression_type = CompressionType::automatic;
}