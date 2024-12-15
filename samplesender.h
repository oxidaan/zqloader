// ==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.h
// DESCRIPTION:     Definition of class SampleSender.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

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

//    using NextSampleFun  = std::function<void (void)>;
    using NextSampleFun  = std::function<bool (void)>;
    using GetDurationFun = std::function<Doublesec (void)>;
    using GetEdgeFun     = std::function<Edge (void)>;
    using CheckDoneFun   = std::function<bool (void)>;
   
    SampleSender(const SampleSender&) = delete;
    SampleSender &operator =(const SampleSender&) = delete;
public:

    SampleSender() noexcept;
    ~SampleSender() noexcept;

    /// Move CTOR
    SampleSender(SampleSender&&) noexcept;

    /// Default move assign causes problems. Dtor target not called. So thread not stopped.
    SampleSender& operator = (SampleSender&&) noexcept;



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


    /// Set callback called (just) before miniaudio starts
    /// Does not run in miniaudio thread/called from Start.
    SampleSender& SetOnCheckDone(CheckDoneFun p_fun)
    {
        m_OnCheckDone = std::move(p_fun);
        return *this;
    }

    /// Intialize miniaudio when not done so already.
    SampleSender& Init();

    /// Start SampleSender / start miniaudio thread.
    SampleSender& Start();

    /// Wait for SampleSender (that is: wait for all data being send)
    SampleSender& WaitUntilDone();

    /// Stop SampleSender (that is: stop miniaudio thread)
    /// Call *after* WaitUntilDone else aborts.
    SampleSender& Stop();

    /// Is mini audio (thread) running?
    /// (ma_device_is_started)
    bool IsRunning() const;


    /// Run SampleSender (miniadio thread) so Start, wait, Stop.
    SampleSender& Run();

    /// Set volume. When negative basically inverts. 100 is max.
    SampleSender& SetVolume(int p_volume_left, int p_volume_right)
    {
        if (p_volume_left > 100 || p_volume_left < -100 ||
            p_volume_right > 100 || p_volume_right < -100)
        {
            throw std::runtime_error("Volume must be between -100 and 100");
        }
        m_volume_left  = float(p_volume_left) / 100.0f;
        m_volume_right = float(p_volume_right) / 100.0f;
        return *this;
    }


    /// Set sample rate (hz), when not set use device default.
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

    static int GetDeviceSampleRate();

private:

    void Reset();


    // A (static/global) data callback function for MiniAudio.
    static void data_callback(ma_device* pDevice, void* pOutput, const void*, uint32_t frameCount);


    // Get next sound samples,
    // called in miniaudio device thread
    void DataCallback(ma_device* pDevice, void* pOutput, uint32_t frameCount);

    // Get next single sound sample,
    // called in miniaudio device thread (from DataCallback)
    // GetDurationWait -> GetEdge -> OnNextSample
    float GetNextSample(uint32_t p_samplerate);

    // Callback:
    // Get duration to wait for next edge change based on what we need to do.
    // called in miniaudio device thread
    Doublesec GetDurationWait() const
    {
        if (m_OnGetDurationWait)
        {
            return m_OnGetDurationWait();
        }
        return 0ms;
    }


    // Callback:
    // Get value what edge needs to become eg toggle/one/zero.
    // called in miniaudio device thread
    Edge GetEdge() const
    {
        if (m_OnGetEdge)
        {
            return m_OnGetEdge();
        }
        return Edge::no_change;
    }

    // Callback:
    // Called when moving to next sample.
    // Return true when at end.
    // called in miniaudio device thread
    bool NextSample()
    {
        if (m_OnNextSample)
        {
            return m_OnNextSample();
        }
        return false;
    }

    // Callback:
    // Are we done?
    bool CheckDone()
    {
        if (m_OnCheckDone)
        {
            return m_OnCheckDone();
        }
        return true;
    }


private:
    std::unique_ptr<ma_device>   m_device;


    Event                        m_event;
    int                          m_done         = 0;        
    static constexpr int         m_done_event_cnt  = 10;    // see https://github.com/mackron/miniaudio/discussions/490
    bool                         m_edge         = false;    // output value toggles between 1/0
    Doublesec                    m_sample_time  = 0ms;      // time since last edge change

    NextSampleFun                m_OnNextSample;
    GetDurationFun               m_OnGetDurationWait;
    GetEdgeFun                   m_OnGetEdge;
    CheckDoneFun                 m_OnCheckDone;

    float                        m_volume_left  = 1.0f;     // -1.0 .. 1.0
    float                        m_volume_right = 1.0f;     // -1.0 .. 1.0
    uint32_t                     m_sample_rate  = 0;        // Set to 0 to use the device's native sample rate.

}; // class SampleSender