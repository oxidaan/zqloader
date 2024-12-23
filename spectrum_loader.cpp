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
#include "spectrum_consts.h"        // g_tstate_dur


/// Convenience: add ZX Spectrum standard leader.


SpectrumLoader::SpectrumLoader()                                = default;
SpectrumLoader::SpectrumLoader(SpectrumLoader &&)               = default;
SpectrumLoader & SpectrumLoader::operator = (SpectrumLoader &&) = default;
SpectrumLoader::~SpectrumLoader()                               = default;

// Runs in miniaudio thread, typically
inline void SpectrumLoader::StandbyToActive()
{
    std::unique_lock lock(m_mutex_standby_pulsers.mutex);
    m_active_pulsers = std::move(m_standby_pulsers);
    m_current_pulser = 0;
   // m_time_estimated = 0ms;     // force recalc
}



SpectrumLoader& SpectrumLoader::AddLeader(std::chrono::milliseconds p_duration)
{
    TonePulser().SetPattern(spectrum::g_tstate_leader).
    SetLength(p_duration).
    MoveToLoader(*this);
    return *this;
}



/// Convenience: add ZX Spectrum standard leader that goes on forever: for tuning.
SpectrumLoader& SpectrumLoader::AddEndlessLeader()
{
    TonePulser().SetPattern(spectrum::g_tstate_leader).
    SetInfiniteLength().
    MoveToLoader(*this);
    return *this;
}



/// Convenience: add standard ZX Spectrum standard sync block.
/// https://worldofspectrum.org/faq/reference/48kreference.htm
SpectrumLoader& SpectrumLoader::AddSync()
{
    TonePulser().SetPattern(spectrum::g_tstate_sync1, spectrum::g_tstate_sync2).MoveToLoader(*this);
    return *this;
}



/// Convenience: add ZX Spectrum standard data block.
/// This is raw data so should already include startbyte + checksum
SpectrumLoader& SpectrumLoader::AddData(DataBlock p_data, int p_pulslen)
{
    DataPulser().
    SetZeroPattern(p_pulslen, p_pulslen).                   //  eg 855, 855
    SetOnePattern(2 * p_pulslen, 2 * p_pulslen).            //  eg 1710, 1710
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
    if(m_time_estimated == 0ms)
    {
        for(const auto &p : m_standby_pulsers)
        {
            m_time_estimated += p->GetDuration();
        }
        m_time_estimated = m_time_estimated * 1.2;   // see remark at  SampleSender::GetNextSample
    }
    return m_time_estimated;
}



/// CallBack; runs in miniaudio thread
/// Move to next pulse. Return true when (completely) done.
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
    return current.GetTstate() * spectrum::g_tstate_dur;
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