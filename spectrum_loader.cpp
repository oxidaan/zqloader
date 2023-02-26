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
bool SpectrumLoader::Next()
{
    Pulser& current = GetCurrentBlock();
    if (current.Next())
    {
        m_current_block++;
        if (m_current_block >= m_datablocks.size())
        {
            return true;
        }
    }
    return false;
}

// Get duration to wait (in seconds)
// CallBack; runs in miniaudio thread
// depending on what is to be sent: (1/0/sync/leader).
Doublesec SpectrumLoader::GetDurationWait() const
{
    Pulser& current = GetCurrentBlock();
    return current.GetTstate() * m_tstate_dur;
}

// Get duration to wait (in seconds)
// CallBack; runs in miniaudio thread
// depending on what is to be sent: (1/0/sync/leader).
Edge SpectrumLoader::GetEdge() const
{
    Pulser& current = GetCurrentBlock();
    return current.GetEdge();
}
