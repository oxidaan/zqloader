//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_loader.cpp
// DESCRIPTION:     Implementation of class SpectrumLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "spectrum_loader.h"
#include "loadbinary.h"
#include "pulsers.h"
#include "spectrum_consts.h"        // g_tstate_dur


/// Convenience: add ZX Spectrum standard leader.


SpectrumLoader::SpectrumLoader() = default;

SpectrumLoader::~SpectrumLoader() = default;

SpectrumLoader& SpectrumLoader::AddLeader(std::chrono::milliseconds p_duration)
{
    TonePulser().SetPattern(g_tstate_leader).
        SetLength(p_duration).
        MoveToLoader(*this);
    return *this;
}

/// Convenience: add standard ZX Spectrum standard sync block. 
/// https://worldofspectrum.org/faq/reference/48kreference.htm
SpectrumLoader& SpectrumLoader::AddSync()
{
    TonePulser().SetPattern(g_tstate_sync1, g_tstate_sync2).MoveToLoader(*this);
    return *this;
}

/// Convenience: add ZX Spectrum standard data block.
/// This is raw data so should already include startbyte + checksum
SpectrumLoader& SpectrumLoader::AddData(DataBlock p_data, int p_pulslen)
{
    DataPulser().
        SetZeroPattern(p_pulslen, p_pulslen).               //  eg 855, 855
        SetOnePattern(2 * p_pulslen, 2 * p_pulslen).        //  eg 1710, 1710
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

SpectrumLoader& SpectrumLoader::WriteTzxFile(std::ostream& p_filewrite)
{
    p_filewrite << "ZXTape!";
    WriteBinary(p_filewrite, std::byte{0x1A});
    WriteBinary(p_filewrite, std::byte{ 1 });
    WriteBinary(p_filewrite, std::byte{ 20 });
    for(const auto &p : m_pulsers)
    {
        p->WriteAsTzxBlock(p_filewrite);
    }
    return *this;
}

// CallBack; runs in miniaudio thread
// Move to next pulse. Return true when (completely) done.
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
    return current.GetTstate() * g_tstate_dur;
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

