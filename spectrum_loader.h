//==============================================================================
// PROJECT:         zqloader
// FILE:            spectrum_loader.cpp
// DESCRIPTION:     Definition of class SpectrumLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

//#include <chrono>
#include <vector>
#include <memory>           // std::unique_ptr
#include <functional>       // std::bind
#include "types.h"
#include "datablock.h"      // Datablock used
#include "spectrum_consts.h"
#include <cstddef>          // std::byte
#include <filesystem>       //  std::filesystem::path 

class Pulser;
class SampleSender;





/// Main ZX spectrum loader class, stores a series of 'pulsers' that generates audio pulses
/// that the ZX spectrum tapeloader routines can understand.
/// When 'Run' plays the attached pulser classes thus loading (typically a complete 
/// program) to the ZX spectrum.
/// Has some ZX spectrum convenience functions.
/// Combines/owns:
/// - A list of Pulser classes to create audio streams.
/// - SampleSender to connect to miniadio.
class SpectrumLoader
{
private:
    using PulserPtr = std::unique_ptr<Pulser>;
    using Pulsers = std::vector<PulserPtr>;
    SpectrumLoader(const SpectrumLoader&) = delete;
    SpectrumLoader & operator =(const SpectrumLoader&) = delete;
public:
    using DoneFun        = std::function<void (void)>;

    // Ctor 
    SpectrumLoader();            
    SpectrumLoader(SpectrumLoader &&);
    SpectrumLoader & operator = (SpectrumLoader &&); 
    // Dtor
    ~SpectrumLoader();   // = default;





    /// And any pulser.
    template <class TPulser, typename std::enable_if<std::is_base_of<Pulser, TPulser>::value, int>::type = 0>
    SpectrumLoader& AddPulser(TPulser p_block)
    {
        m_current_pulser = 0;
        PulserPtr ptr = std::make_unique< TPulser >(std::move(p_block));
        m_pulsers.push_back(std::move(ptr));
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
    SpectrumLoader& AddData(DataBlock p_data, int p_pulslen = spectrum::g_tstate_zero);

    /// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
    SpectrumLoader& AddPause(std::chrono::milliseconds p_duration = 500ms);

    /// Convenience: add ZX Spectrum standard leader+sync+data block.
    /// This is raw data which (should) already include startbyte + checksum
    SpectrumLoader& AddLeaderPlusData(DataBlock p_data, int p_pulslen = spectrum::g_tstate_zero, std::chrono::milliseconds p_leader_duration = 3000ms)
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
        p_sample_sender.SetOnGetDurationWait(std::bind(&SpectrumLoader::GetDurationWait, this)).
                        SetOnGetEdge(std::bind(&SpectrumLoader::GetEdge, this)).
                        SetOnNextSample(std::bind(&SpectrumLoader::Next, this));
        return *this;
    }

    /// Set callback when done.
    SpectrumLoader& SetOnDone(DoneFun p_fun)
    {
        m_OnDone = std::move(p_fun);
        return *this;
    }

    Doublesec GetDuration() const;

private:
    // CallBack; runs in miniaudio thread
    // Move to next pulse. Return true when (completely) done.
    bool Next();


    // Get duration to wait (in seconds) (until next edge)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Doublesec GetDurationWait() const;

    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Edge GetEdge() const;

    // Get current pulser (aka block)
    Pulser& GetCurrentBlock() const
    {
        return *(m_pulsers[m_current_pulser]);
    }

    
    // Calls callback see 'SetOnDone'
    // runs in miniaudio thread
    void Done();



private:
    Pulsers m_pulsers;
    size_t m_current_pulser = 0;
    DoneFun                      m_OnDone;

};
