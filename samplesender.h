//==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.h
// DESCRIPTION:     Definition of class SampleSender.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <functional>           // std::function
#include "types.h"              // Doublesec
#include "event.h"              // Event (member)

struct ma_device;

/// This class maintains / wraps miniaudio.
/// Uses to send (sound) samples (as pulses) using miniadio. 
/// Forms connection between miniaudio and the 'Pulser' classes, using call-backs.
/// Used call-backs are set with:
/// SetOnGetDurationWait
/// SetOnNextSample
/// SetOnGetEdge
class SampleSender
{
public:
    using NextSampleFun = std::function<bool(void)>;
    using GetDurationFun = std::function<Doublesec(void)>;
    using GetEdgeFun = std::function<Edge(void)>;
public:
    SampleSender();
    ~SampleSender();

    /// CTOR with bool calls Init (so intialize miniaudio)
    SampleSender(bool);


    /// Move CTOR
    SampleSender(SampleSender&&);
    
    /// Move assign
    SampleSender& operator = (SampleSender&&);



    /// Set callback to get wait duration (in seconds/DoubleSec)_
    /// This should get duration to wait for next edge change based 
    /// on what we need to do.
    SampleSender& SetOnGetDurationWait(GetDurationFun p_fun)
    {
        m_OnGetDurationWait = std::move(p_fun);
        return *this;
    }

    /// Set callback to move to next sample.
    /// Given function should move to next sample. Return true when done.
    SampleSender& SetOnNextSample(NextSampleFun p_fun)
    {
        m_OnNextSample = std::move(p_fun);
        return *this;
    }

    /// Set callback to get value what edge needs to become.
    /// Given function should return an Edge enum (eg One/Zero/Toggle)
    /// So what needs to be done with the sound output binary signal.
    SampleSender& SetOnGetEdge(GetEdgeFun p_fun)
    {
        m_OnGetEdge = std::move(p_fun);
        return *this;
    }

    /// Intialize miniaudio when not done so already.
    SampleSender& Init();

    /// Start SampleSender / start miniaudio thread.
    SampleSender& Start();

    /// Wait for SampleSender (that is: wait for miniaudio thread to stop)
    SampleSender& Wait();

    /// Stop SampleSender (that is: stop miniaudio thread)
    SampleSender& Stop();



    /// Run SampleSender (miniadio thread) so start, wait, stop.
    SampleSender& Run()
    {
        Start();
        Wait();
        Stop();
        return *this;
    }

    /// Set volume. When negative basically inverts. 100 is max.
    SampleSender& SetVolume(int p_volume_left, int p_volume_right)
    {
        if (p_volume_left  > 100 || p_volume_left  < -100 ||
            p_volume_right > 100 || p_volume_right < -100)
        {
            throw std::runtime_error("Volume must be between -100 and 100");
        }
        m_volume_left  = float(p_volume_left)  / 100.0f;
        m_volume_right = float(p_volume_right) / 100.0f;
        return *this;
    }

    /// Set sample rate, when not set use device default.
    SampleSender& SetSampleRate(uint32_t p_sample_rate)
    {
        m_sample_rate = p_sample_rate;
        return *this;
    }


    /// Get (last) edge / mainly for debugging
    bool GetLastEdge() const
    {
        return m_edge;
    }

private:
    void Reset()
    {
        m_edge = false;
        //        m_edge = true;      // Does NOT work
        m_sample_time = 0ms;
    }

    // A (static/global) data callback function for MiniAudio.
    static void data_callback(ma_device* pDevice, void* pOutput, const void*, uint32_t frameCount);


    // Get next sound samples, 
    // called in miniaudio device thread
    void DataCallback(ma_device* pDevice, void* pOutput, uint32_t frameCount);

    // Get next single sound sample, 
    // called in miniaudio device thread (from DataCallback)
    // GetDurationWait -> GetEdge -> OnNextSample
    float GetNextSample(uint32_t p_samplerate);

    // Get duration to wait for next edge change based on what we need to do.
    Doublesec GetDurationWait() const
    {
        if (m_OnGetDurationWait)
        {
            return m_OnGetDurationWait();
        }
        return 0ms;
    }

    /// Get value what edge needs to become eg toggle/one/zero.
    Edge GetEdge() const
    {
        if (m_OnGetEdge)
        {
            return m_OnGetEdge();
        }
        return Edge::no_change;
    }

    bool OnNextSample()
    {
        if (m_OnNextSample)
        {
            return m_OnNextSample();
        }
        return false;
    }
   
private:
    Event m_event;
    bool m_done = false;
    bool m_is_running = false;
    std::unique_ptr<ma_device> m_device;
    NextSampleFun  m_OnNextSample;
    GetDurationFun m_OnGetDurationWait;
    GetEdgeFun     m_OnGetEdge;
    bool m_edge = false;                                    // output value toggles between 1/0
    Doublesec m_sample_time = 0ms;                          // time since last edge change

    float m_volume_left = 1.0f;                             // -1.0 .. 1.0
    float m_volume_right = 1.0f;                            // -1.0 .. 1.0
    uint32_t m_sample_rate = 0;                             // Set to 0 to use the device's native sample rate.

};

