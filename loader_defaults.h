// ==============================================================================
// PROJECT:         zqloader
// FILE:            loader_defaults.h
// DESCRIPTION:     Defaults for ZQ loader
//
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once
namespace loader_tstates
{
// T-states. Values depend only on z80 code. See zqloader.z80asm
constexpr int wait_for_edge_loop_duration  = 43;    // Polling loop duration
constexpr int bit_loop_duration            = 91;    // Bit loop duration
constexpr int byte_loop_loop_duration      = 155;   // Byte loop duration
constexpr int end_of_byte_delay            = byte_loop_loop_duration - bit_loop_duration ;   // Byte loop duration
}

namespace loader_defaults
{
constexpr double wanted_zero_cyclii        = 1.0;
constexpr int zero_max                     = 3;   // max #INs to see a zero.  Minimal 1 (needs at least one IN to see edge) (old: 4->3) aka ZERO_MAX
                                                  // must be > wanted_zero_cyclii   (eg >= 2)
constexpr double wanted_one_cyclii         = 4.5; // must be > zero_max

constexpr int bit_loop_max                 = 100; // max #INs before timeout. when 0 dont set (so keep defaults at zqloader.z80asm) (old: 12)

constexpr int zero_duration                = loader_tstates::bit_loop_duration - 10 + int((wanted_zero_cyclii - 1.0) * loader_tstates::wait_for_edge_loop_duration);  // bit_loop ; 81;  // @@ see zqloader.asm (old: 118)
constexpr int one_duration                 = loader_tstates::bit_loop_duration      + int((wanted_one_cyclii  - 1.0) * loader_tstates::wait_for_edge_loop_duration);// 241; //int(91 + 3.5 * 43); //250;  231 worked better with jsw3.z80!? (old: 293)

constexpr int end_of_byte_delay            = loader_tstates::end_of_byte_delay;  // 64, should not change


constexpr int volume_left                  = 100;
constexpr int volume_right                 = 100;
constexpr int sample_rate                  = 0;                 // 0 is device default (eg 48000hz)

constexpr CompressionType compression_type = CompressionType::automatic;


}
