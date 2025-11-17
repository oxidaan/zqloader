// ==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_loader.cpp
// DESCRIPTION:     Implementation of class SpectrumLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "spectrum_loader.h"
#include "loadbinary.h"
#include "pulsers.h"




SpectrumLoader::SpectrumLoader()                                         = default;
SpectrumLoader::SpectrumLoader(SpectrumLoader &&) noexcept               = default;
SpectrumLoader & SpectrumLoader::operator = (SpectrumLoader &&) noexcept = default;
SpectrumLoader::~SpectrumLoader()                                        = default;

// Runs in miniaudio thread, typically
inline void SpectrumLoader::StandbyToActive()
{
    std::unique_lock lock(m_mutex_standby_pulsers.mutex);
    m_active_pulsers = std::move(m_standby_pulsers);
    m_current_pulser = 0;
   // m_time_estimated = 0ms;     // force recalc
}



/// Convenience: add ZX Spectrum standard leader.
SpectrumLoader& SpectrumLoader::AddLeader(std::chrono::milliseconds p_duration)
{
    TonePulser(m_use_standard_clock_for_rom ? spectrum::tstate_dur : GetTstateDuration()).
        SetPattern(spectrum::tstate_leader).
        SetLength(p_duration).
        MoveToLoader(*this);
    return *this;
}



/// Convenience: add ZX Spectrum standard leader that goes on forever: for tuning.
SpectrumLoader& SpectrumLoader::AddEndlessLeader()
{
    TonePulser(m_use_standard_clock_for_rom ? spectrum::tstate_dur : GetTstateDuration()).
        SetPattern(spectrum::tstate_leader).
        SetInfiniteLength().
        MoveToLoader(*this);
    return *this;
}



/// Convenience: add standard ZX Spectrum standard sync block.
/// https://worldofspectrum.org/faq/reference/48kreference.htm
SpectrumLoader& SpectrumLoader::AddSync()
{
    TonePulser(m_use_standard_clock_for_rom ? spectrum::tstate_dur : GetTstateDuration()).
        SetPattern(spectrum::tstate_sync1, spectrum::tstate_sync2).
        MoveToLoader(*this);
    return *this;
}



/// Convenience: add ZX Spectrum standard data block.
/// This is raw data so should already include startbyte + checksum
SpectrumLoader& SpectrumLoader::AddData(DataBlock p_data, int p_pulslen)
{
    DataPulser(m_use_standard_clock_for_rom ? spectrum::tstate_dur : GetTstateDuration()).
        SetZeroPattern(p_pulslen, p_pulslen).                   //  eg 855, 855
        SetOnePattern(2 * p_pulslen, 2 * p_pulslen).            //  eg 1710, 1710
        SetData(std::move(p_data)).
        MoveToLoader(*this);
    return *this;
}



/// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
SpectrumLoader& SpectrumLoader::AddPause(std::chrono::milliseconds p_duration)
{
    PausePulser(m_use_standard_clock_for_rom ? spectrum::tstate_dur : GetTstateDuration()).
        SetLength(p_duration).
        MoveToLoader(*this);
    return *this;
}



///  Write as tzx file to given stream
SpectrumLoader& SpectrumLoader::WriteTzxFile(std::ostream& p_filewrite)
{
    p_filewrite << "ZXTape!";
    WriteBinary(p_filewrite, std::byte{ 0x1A });
    WriteBinary(p_filewrite, std::byte{ 1 });
    WriteBinary(p_filewrite, std::byte{ 20 });
    for(const auto &p : m_standby_pulsers)
    {
        p->WriteAsTzxBlock(p_filewrite);
    }
    return *this;
}



/// Get expected duration (from all standby pulsers)
/// Not 100% accurate because discrepancy between miniaudio sample rate and time we actually need.
Doublesec SpectrumLoader::GetEstimatedDuration() const
{
    return GetDurationInTStates() * m_tstate_dur * 1.2;  // 1.2: see remark at  SampleSender::GetNextSample
}

/// Get duration in TStates.
int SpectrumLoader::GetDurationInTStates() const
{
    if(m_duration_in_tstates == 0)
    {
        for(const auto &p : m_standby_pulsers)
        {
            m_duration_in_tstates += p->GetDurationInTStates();
        }
    }
    return m_duration_in_tstates;
}



/// CallBack; runs in miniaudio thread
/// Move to next pulser. Return true when (completely) done.
bool SpectrumLoader::Next()
{
    Pulser& current = GetCurrentPulser();
    if (current.Next())     // true when at end of pulser
    {
        m_current_pulser++;
        if (IsDone())       // all pulsers are done. Check for more (usually only at end)
        {
            return CheckDone(); // calls callback
        }
    }
    return false;
}



/// CallBack; runs in miniaudio thread
/// Get duration to wait (in seconds)
Doublesec SpectrumLoader::GetDurationWait() const
{
    Pulser& current = GetCurrentPulser();
    return current.GetDurationWait();
}



/// CallBack; runs in miniaudio thread
/// Get edge
/// Get edge depending on what is to be sent: (1/0/sync/leader).
/// Eg a datapulser will return Edge::one when a 1 needs to be send etc.
Edge SpectrumLoader::GetEdge() const
{
    Pulser& current = GetCurrentPulser();
    return current.GetEdge();
}



/// Called when run out of pulsers.
/// Return true when completely done.
bool SpectrumLoader::CheckDone()
{
    if (IsDone())
    {
        StandbyToActive();
        if(IsDone() && m_OnDone)
        {
            m_OnDone();
            StandbyToActive();  // maybe 'm_OnDone' added more
        }
        return IsDone();
    }
    return false;
}
