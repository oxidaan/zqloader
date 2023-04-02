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
#include "samplesender.h"
#include "spectrum_types.h"
#include "datablock.h"      // Datablock used

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

public:
    // Ctor
    SpectrumLoader();
    
    // Dtor
    ~SpectrumLoader();




    /// Set (miniaudio) sample sender, and Init.
    SpectrumLoader& SetSampleSender(SampleSender&& p_sample_sender)
    {
        m_sample_sender = std::move(p_sample_sender);
        return Init(m_sample_sender);
    }

    SpectrumLoader& Init()
    {
        m_sample_sender = SampleSender() ;
        return Init(m_sample_sender);
    }


    /// Reset SpectrumLoader.
    SpectrumLoader& Reset()
    {
        m_current_pulser = 0;
        return *this;
    }

    /// And any pulser to process.
    template <class TPulser, typename std::enable_if<std::is_base_of<Pulser, TPulser>::value, int>::type = 0>
    SpectrumLoader& AddPulser(TPulser p_block)
    {
        PulserPtr ptr = std::make_unique< TPulser >(std::move(p_block));
        m_pulsers.push_back(std::move(ptr));
        return *this;
    }

    /// Convenience: add ZX Spectrum standard leader with given duration.
    SpectrumLoader& AddLeader(std::chrono::milliseconds p_duration);

    /// Convenience: add ZX Spectrum standard sync block. 
    SpectrumLoader& AddSync();

    /// Convenience: add ZX Spectrum standard data block.
    /// This is raw data so should already include startbyte + checksum
    SpectrumLoader& AddData(DataBlock p_data, int p_pulslen = 855);

    /// Convenience: add ZX Spectrum standard pause (eg before 2nd leader)
    SpectrumLoader& AddPause(std::chrono::milliseconds p_duration = 500ms);

    /// Convenience: add ZX Spectrum standard leader+sync+data block.
    /// This is raw data which (should) already include startbyte + checksum
    SpectrumLoader& AddLeaderPlusData(DataBlock p_data, int p_pulslen = 855, std::chrono::milliseconds p_leader_duration = 3000ms)
    {
        return AddLeader(p_leader_duration).AddSync().AddData(std::move(p_data), p_pulslen);
    }


    /// Run SpectrumLoader. Play added pulser-blocks.
    SpectrumLoader& Run()
    {
        Reset();
        if(m_pulsers.size())
        {
            m_sample_sender.Run();
        }
        return *this;
    }

    /// Mainly for debugging.
    bool GetLastEdge() const
    {
        return m_sample_sender.GetLastEdge();
    }



private:
    // CallBack; runs in miniaudio thread
    bool Next();


    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Doublesec GetDurationWait() const;

    // Get duration to wait (in seconds)
    // CallBack; runs in miniaudio thread
    // depending on what is to be sent: (1/0/sync/leader).
    Edge GetEdge() const;

    Pulser& GetCurrentBlock() const
    {
        return *(m_pulsers[m_current_pulser]);
    }

    SpectrumLoader& Init(SampleSender& p_sample_sender)
    {
        p_sample_sender.Init().
            SetOnGetDurationWait(std::bind(&SpectrumLoader::GetDurationWait, this)).
            SetOnGetEdge(std::bind(&SpectrumLoader::GetEdge, this)).
            SetOnNextSample(std::bind(&SpectrumLoader::Next, this));
        return *this;
    }

private:
    Pulsers m_pulsers;
    size_t m_current_pulser = 0;
    SampleSender m_sample_sender;

};
