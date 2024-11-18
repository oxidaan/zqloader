//==============================================================================
// PROJECT:         zqloader
// FILE:            pulsers.cpp
// DESCRIPTION:     Implementation of some Pulser functions.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================



#include "pulsers.h"
#include "spectrum_consts.h"        // g_tstate_dur



/// Set length of pause in T-states.
PausePulser& PausePulser::SetLength(int p_states)
{
  //  std::cout << "Pause = " << p_states << " T-states" << std::endl;
    m_duration_in_tstates = p_states;
    return *this;
}

/// Set length of pause in milliseconds.
PausePulser& PausePulser::SetLength(std::chrono::milliseconds p_duration)
{
   // std::cout << "Pause = " << p_duration.count() << "ms" << std::endl;
    m_duration_in_tstates = int(p_duration / spectrum::g_tstate_dur);
    return *this;
}


/// Set length in # of pulses that is # complete patterns.
TonePulser& TonePulser::SetLength(unsigned p_max_pulses)
{
    unsigned pattsize = unsigned(m_pattern.size());
    if (pattsize)
    {
        m_max_pulses = pattsize * p_max_pulses;
    }
    else
    {
        m_max_pulses = p_max_pulses;
    }
    return *this;
}

/// Set length in milliseconds, rounds up to complete patterns.
TonePulser& TonePulser::SetLength(std::chrono::milliseconds p_duration)
{
    auto pat_dur = GetPatternDuration();
    if (pat_dur)
    {
        m_max_pulses = unsigned(p_duration / (spectrum::g_tstate_dur * pat_dur));
        unsigned pattsize = unsigned(m_pattern.size());
        m_max_pulses += pattsize - (m_max_pulses % pattsize);    // round up to next multiple of pattsize
    }
    else
    {
        throw std::runtime_error("Cannot set length to time when pattern is unknown. Call SetPattern first.");
    }
    return *this;
}
