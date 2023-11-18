//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_loader.cpp
// DESCRIPTION:     Implementation of class SpectrumLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "spectrum_loader.h"
#include "pulsers.h"

/// Convenience: add ZX Spectrum standard leader.


SpectrumLoader::SpectrumLoader() = default;

SpectrumLoader::~SpectrumLoader() = default;

SpectrumLoader& SpectrumLoader::AddLeader(std::chrono::milliseconds p_duration)
{
    TonePulser().SetPattern(2168).
        SetLength(p_duration).
        MoveToLoader(*this);
    return *this;
}

/// Convenience: add ZX Spectrum standard sync block. 
SpectrumLoader& SpectrumLoader::AddSync()
{
    TonePulser().SetPattern(667, 735).MoveToLoader(*this);
    return *this;
}

/// Convenience: add ZX Spectrum standard data block.
/// This is raw data so should already include startbyte + checksum
SpectrumLoader& SpectrumLoader::AddData(DataBlock p_data, int p_pulslen)
{
    DataPulser().SetBlockType(ZxBlockType::raw).
        SetZeroPattern(p_pulslen, p_pulslen).
        SetOnePattern(2 * p_pulslen, 2 * p_pulslen).
        SetData(std::move(p_data)).
        MoveToLoader(*this);
    return *this;
}

/// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
SpectrumLoader& SpectrumLoader::AddPause(std::chrono::milliseconds p_duration)
{
    PausePulser().SetLength(p_duration).MoveToLoader(*this);
    return *this;
}

// CallBack; runs in miniaudio thread
// Move to next pulse. Return true when done.
bool SpectrumLoader::Next()
{
    Pulser& current = GetCurrentBlock();
    if (current.Next())
    {
        m_current_pulser++;
        if (m_current_pulser >= m_pulsers.size())
        {
            return true;
        }
    }
    return false;
}


// Get duration to wait (in seconds)
// CallBack; runs in miniaudio thread
Doublesec SpectrumLoader::GetDurationWait() const
{
    Pulser& current = GetCurrentBlock();
    return current.GetTstate() * m_tstate_dur;
}

// Get edge 
// CallBack; runs in miniaudio thread
// Get edge depending on what is to be sent: (1/0/sync/leader).
// Eg a datapulser will return Edge::one when a 1 needs to be send etc.
Edge SpectrumLoader::GetEdge() const
{
    Pulser& current = GetCurrentBlock();
    return current.GetEdge();
}

SpectrumLoader &SpectrumLoader::Init(SampleSender &p_sample_sender)
{
    p_sample_sender.Init().
            SetOnGetDurationWait(std::bind(&SpectrumLoader::GetDurationWait, this)).
            SetOnGetEdge(std::bind(&SpectrumLoader::GetEdge, this)).
            SetOnNextSample(std::bind(&SpectrumLoader::Next, this));
    return *this;
}
