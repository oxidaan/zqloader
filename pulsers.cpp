//==============================================================================
// PROJECT:         zqloader
// FILE:            pulsers.cpp
// DESCRIPTION:     Implementation of some Pulser functions.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================



#include "pulsers.h"
//#include "spectrum_consts.h"        // g_tstate_dur



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
    m_duration_in_tstates = int(p_duration / m_tstate_dur);
    return *this;
}

Doublesec PausePulser::GetDuration() const
{
    return m_duration_in_tstates * m_tstate_dur;
}

/// Set length in # of pulses that is # complete patterns.
TonePulser& TonePulser::SetLength(unsigned p_max_pulses)
{
    m_forever = false;
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
    m_forever = false;
    auto pat_dur = GetPatternDuration();
    if (pat_dur)
    {
        m_max_pulses = unsigned(p_duration / (m_tstate_dur * pat_dur));
        unsigned pattsize = unsigned(m_pattern.size());
        m_max_pulses += pattsize - (m_max_pulses % pattsize);    // round up to next multiple of pattsize
    }
    else
    {
        throw std::runtime_error("Cannot set length to time when pattern is unknown. Call SetPattern first.");
    }
    return *this;
}

Doublesec TonePulser::GetDuration() const
{
    return m_max_pulses * GetPatternDuration() * m_tstate_dur;
}


Doublesec DataPulser::GetDuration() const
{
    auto bitnumb4 = m_bitnum;
    auto pulsnumb4 = m_pulsnum;
    int tstates = 0;
    auto me = const_cast<DataPulser *>(this);
    me->m_bitnum = 0;
    me->m_pulsnum = 0;
    do
    {
        tstates += GetTstate();
    }
    while(!me -> Next());
    me->m_bitnum = bitnumb4;
    me->m_pulsnum = pulsnumb4;
    return tstates * m_tstate_dur;
}
