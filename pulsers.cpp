//==============================================================================
// PROJECT:         zqloader
// FILE:            pulsers.cpp
// DESCRIPTION:     Implementation of some Pulser functions.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================



#include "pulsers.h"





/// Set duration in # of pulses that is # complete patterns.
TonePulser& TonePulser::SetLength(unsigned p_num_patterns)
{
    m_forever = false;
    unsigned pattsize = unsigned(m_pattern.size());
    if (pattsize)
    {
        m_max_pulses = pattsize * p_num_patterns;
    }
    else
    {
        m_max_pulses = p_num_patterns;
    }
    return *this;
}

/// Set length in milliseconds
TonePulser& TonePulser::SetLength(std::chrono::milliseconds p_duration)
{
    m_forever = false;
    auto pat_dur = GetPatternDuration();
    if (pat_dur)
    {
        unsigned pattsize = unsigned(m_pattern.size());
        m_max_pulses = pattsize * unsigned(p_duration / (m_tstate_dur * pat_dur));
        // no might cause problems when pattsize is eg 3: m_max_pulses += m_max_pulses % 2;       // round up to next even number.
    }
    else
    {
        throw std::runtime_error("Cannot set length to time when pattern is unknown. Call SetPattern first.");
    }
    return *this;
}



/// Get total duration for this pulser in TStates.
int DataPulser::GetDurationInTStates() const
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
    return tstates;
}

