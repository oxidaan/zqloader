// ==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.cpp
// DESCRIPTION:     Implementation for class SampleSender.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "samplesender.h"
#include <miniaudio.h>
#include <iostream>
#include <thread>

// CTORs
SampleSender::SampleSender() noexcept = default;
SampleSender::SampleSender(SampleSender &&) noexcept = default;
SampleSender& SampleSender::operator = (SampleSender&&) noexcept = default;



SampleSender::~SampleSender()
{
    if (m_device)
    {
        Stop();
        ma_device_uninit(m_device.get());
    }
}



/// Intialize miniadio when not done so already.
SampleSender& SampleSender::Init()
{
    if (!m_device)
    {
        ma_device_config config  = ma_device_config_init(ma_device_type_playback);
        config.playback.format   = ma_format_f32; // Set to ma_format_unknown to use the device's native format.
        config.playback.channels = 2;             // Set to 0 to use the device's native channel count.
        config.sampleRate        = m_sample_rate; // Set to 0 to use the device's native sample rate.
        config.dataCallback      = data_callback; // This function will be called when miniaudio needs more data.
        config.pUserData         = this;          // Can be accessed from the device object (device.pUserData).

        auto device = std::make_unique<ma_device>();
//        auto device = new ma_device;
        if (ma_device_init(nullptr, &config, device.get()) != MA_SUCCESS)
        {
            throw std::runtime_error("Failed to initialize the device.");
        }
        std::cout << "Sample rate = " << device->sampleRate << std::endl;
        m_device = std::move(device);
    }
    return *this;
}

/// Start SampleSender / start miniaudio thread.
SampleSender& SampleSender::Start()
{
    if (!IsRunning())
    {
        Reset();
        Init();
        ma_device_start(m_device.get());     // The device is sleeping by default so you'll need to start it manually.
    }
    return *this;
}



/// Stop SampleSender (that is: stop miniaudio thread)
/// Call *after* Wait else aborts sending data.
SampleSender& SampleSender::Stop()
{
    if (IsRunning())
    {
        ma_device_stop(m_device.get());
    }
    return *this;
}


/// Wait for SampleSender (that is: wait for all data to have been send)
/// Has nothing to do with waiting for thread to finish.
SampleSender& SampleSender::WaitUntilDone()
{
    if (IsRunning())
    {
        m_event.wait();
    }
    return *this;
}





bool SampleSender::IsRunning() const
{
    return m_device && ma_device_is_started(m_device.get());
}






/// Run SampleSender (miniadio thread) so start, wait, stop.
/// convenience.
SampleSender& SampleSender::Run()
{
    Start();
    WaitUntilDone();
//    std::this_thread::sleep_for(500ms); // otherwise stops before all data was send
    // see https://github.com/mackron/miniaudio/discussions/490
    // replaced with m_done_event_cnt
    Stop();
    return *this;
}



// A (static/global) data callback function for MiniAudio.
// Redirect immidiately to corresponding class member function immidiately.
static_assert(sizeof(ma_uint32) == sizeof(uint32_t));
void SampleSender::data_callback(ma_device * pDevice, void* pOutput, const void*, ma_uint32 frameCount)
{
    SampleSender& samplesender = *static_cast<SampleSender*>(pDevice->pUserData);
    samplesender.DataCallback(pDevice, pOutput, frameCount);
}


// static
int SampleSender::GetDeviceSampleRate()
{
    SampleSender temp;
    temp.Init();
    return temp.m_device->sampleRate;
}

inline void SampleSender::Reset()
{
    m_done = 0;
    m_edge = false;
    //        m_edge = true;      // Does NOT work
    m_sample_time = 0ms;
    m_event.Reset();
}

/// Get next sound samples,
/// called in miniaudio device thread
inline void SampleSender::DataCallback(ma_device* pDevice, void* pOutput, uint32_t frameCount)
{
    auto sampleRate = pDevice->sampleRate;
    auto channels   = pDevice->playback.channels;   // # channels eg 2 is stereo
    float* foutput  = reinterpret_cast<float*>(pOutput);
    int index       = 0;
    for (auto n = 0u; n < frameCount; n++)
    {
        m_done = (m_done == 0) ? CheckDone() : m_done + 1;
        float value = m_done ? 0.0f : GetNextSample(sampleRate);
        for (auto chan = 0u; chan < channels; chan++)
        {
            foutput[index] = (chan == 0) ? m_volume_left * value :
                             (chan == 1) ? m_volume_right * value : 0.0f;
            index++;
        }
    }
    if(m_done)
    {
        if(m_done > m_done_event_cnt)  // see https://github.com/mackron/miniaudio/discussions/490 
        {
            m_event.Signal();
        }
    }
}



/// Get next sample (audio value), depending on attached Pulsers.
/// This is typically 1.0 or -1.0 depending on edge; s/a GetEdge.
/// (any volume setting not used here)
inline float SampleSender::GetNextSample(uint32_t p_samplerate)
{
    Doublesec sample_period = 1s / double(p_samplerate);
    m_sample_time += sample_period;
    // Get duration to wait for next edge change based on what we need to do
    Doublesec time_to_wait = GetDurationWait();
    if (m_sample_time >= time_to_wait)
    {
        // Get value what edge needs to become
        Edge edge = GetEdge();
        if (edge == Edge::toggle)
        {
            m_edge = !m_edge;
        }
        else if (edge == Edge::one)
        {
            m_edge = true;
        }
        else if (edge == Edge::zero)
        {
            m_edge = false;
        }
        // catch up. Causes sound to be irregular.
        // Even leader tone is noticable (hearable) irregular.
        // But this makes loading take a little longer than expected (s/a SpectrumLoader::GetDuration)
        // m_sample_time = m_sample_time - time_to_wait ;
        m_sample_time = 0s;
        NextSample();   // true when at end.
        //if (NextSample()) // true when at end.
        //{
            //m_done = 1;       // >0 : at end
        //}
    }
    return m_edge ? 1.0f : -1.0f;
}