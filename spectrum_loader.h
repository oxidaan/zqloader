// ==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_loader.cpp
// DESCRIPTION:     Definition of class SpectrumLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

// #include <chrono>
#include <vector>
#include <memory>           // std::unique_ptr
#include <functional>       // std::function
#include "types.h"
#include "datablock.h"      // Datablock used
#include "spectrum_consts.h"
#include <mutex>

class Pulser;
class SampleSender;

// Not sure why std::mutex is not movable, really.
struct MovableMutex
{
    MovableMutex()                     = default;
    MovableMutex(const MovableMutex &) = delete;
    // When moved, it is just not locked. And asume p_other neither.
    MovableMutex(MovableMutex &&) noexcept
    {}
    MovableMutex &operator = (MovableMutex &&) noexcept
    {
        return *this;
    }

    MovableMutex &operator = (const MovableMutex &) = delete;
    std::mutex   mutex;
};




/// Main ZX spectrum loader class, stores a series of 'pulsers' that generates audio pulses
/// that the ZX spectrum tapeloader routines can understand.
/// When 'Run' plays the attached pulser classes thus loading (typically a complete
/// program) to the ZX spectrum.
/// Has some ZX spectrum convenience functions.
/// Combines/owns:
/// - A list of Pulser classes to create audio streams. (both standby as now active)
/// Attaches to a SampleSender(=miniaudio) through call backs.
class SpectrumLoader
{
private:

    using PulserPtr = std::unique_ptr<Pulser>;
    using Pulsers   = std::vector<PulserPtr>;
    SpectrumLoader(const SpectrumLoader&)              = delete;
    SpectrumLoader & operator =(const SpectrumLoader&) = delete;

public:

    using DoneFun = std::function<void (void)>;

    // Ctor
    SpectrumLoader();
    SpectrumLoader(SpectrumLoader &&) noexcept;
    SpectrumLoader & operator = (SpectrumLoader &&) noexcept;
    // Dtor
    ~SpectrumLoader();   // = default;



    /// Add any pulser.
    template <class TPulser, typename std::enable_if<std::is_base_of<Pulser, TPulser>::value, int>::type = 0>
    SpectrumLoader& AddPulser(TPulser p_pulser)
    {
        PulserPtr ptr = std::make_unique< TPulser >(std::move(p_pulser));
        std::unique_lock lock(m_mutex_standby_pulsers.mutex);
        m_standby_pulsers.push_back(std::move(ptr));
        m_duration_in_tstates = 0;     // force recalc
        return *this;
    }


    /// Convenience: add ZX Spectrum standard leader with given duration.
    SpectrumLoader& AddLeader(std::chrono::milliseconds p_duration);

    /// Convenience: add ZX Spectrum standard leader that goes on forever: for tuning.
    SpectrumLoader& AddEndlessLeader();

    /// Convenience: add ZX Spectrum standard sync block.
    SpectrumLoader& AddSync();

    /// Convenience: add ZX Spectrum standard data block.
    /// This is raw data so should already include startbyte + checksum
    SpectrumLoader& AddData(DataBlock p_data, int p_pulslen = spectrum::tstate_zero);

    /// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
    SpectrumLoader& AddPause(std::chrono::milliseconds p_duration = 500ms);

    /// Convenience: add ZX Spectrum standard leader+sync+data block.
    /// This is raw data which (should) already include startbyte + checksum
    SpectrumLoader& AddLeaderPlusData(DataBlock p_data, int p_pulslen = spectrum::tstate_zero, std::chrono::milliseconds p_leader_duration = 3000ms)
    {
        return AddLeader(p_leader_duration).AddSync().AddData(std::move(p_data), p_pulslen);
    }


    /// Write all added pulsers data as a TZX file. Not fully working though.
    SpectrumLoader& WriteTzxFile(std::ostream& p_file);

    // Set the call backs
    // TSampleSender is SampleSender or SampleToWav
    template<class TSampleSender>
    SpectrumLoader& Attach(TSampleSender& p_sample_sender)
    {
        p_sample_sender.SetOnGetDurationWait([this]
            {
                return GetDurationWait();
            });
        p_sample_sender.SetOnGetEdge([this]
            { 
                return GetEdge();
            });
        p_sample_sender.SetOnNextSample([this]
            {
                return Next();
            });
        p_sample_sender.SetOnCheckDone([this]
            {   
                return CheckDone();
            });
        return *this;
    }


    /// Set callback when done.
    SpectrumLoader& SetOnDone(DoneFun p_fun)
    {
        m_OnDone = std::move(p_fun);
        return *this;
    }


    /// Get expected duration (from all standby pulsers)
    /// Not 100% accurate because discrepancy between miniaudio sample rate and time we actually need.
    Doublesec GetEstimatedDuration() const;
    int GetDurationInTStates() const;

    SpectrumLoader &SetTstateDuration(Doublesec p_to_what) 
    {
        m_tstate_dur = p_to_what;
        return *this;
    }
    Doublesec GetTstateDuration() const
    {
        return m_tstate_dur;
    }

    // Ignore speed as set by SetTstateDuration for normal speed (=rom) loading routines
    // Then take 3.5Mhz.
    SpectrumLoader &SetUseStandaardSpeedForRom(bool p_to_what) 
    {
        m_use_standard_clock_for_rom = p_to_what;
        return *this;
    }
private:

    // CallBack; runs in miniaudio thread
    // Move to next pulse.
    bool Next();


    // Get duration to wait (in seconds) (until next edge)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Doublesec GetDurationWait() const;

    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Edge GetEdge() const;

    // Get current pulser
    Pulser& GetCurrentPulser() const
    {
        return *(m_active_pulsers[m_current_pulser]);
    }


    // Calls callback see 'SetOnDone'
    // runs in miniaudio thread
    bool CheckDone();

    /// Return true when done (that is no more pulsers)
    bool IsDone() const
    {
        return m_current_pulser >= m_active_pulsers.size();
    }


    void StandbyToActive();

private:

    Pulsers             m_active_pulsers;
    Pulsers             m_standby_pulsers;
    MovableMutex        m_mutex_standby_pulsers;
    size_t              m_current_pulser = 0;
    DoneFun             m_OnDone;
    //mutable Doublesec   m_time_estimated{};
    mutable int         m_duration_in_tstates{};
    Doublesec           m_tstate_dur = spectrum::tstate_dur;
    bool                m_use_standard_clock_for_rom = false;
}; // class SpectrumLoader
