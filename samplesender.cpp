//==============================================================================
// PROJECT:         zqloader
// FILE:            samplesender.cpp
// DESCRIPTION:     Implementation for class SampleSender.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "samplesender.h"
#include <miniaudio.h>
#include <iostream>
#include <thread>

// CTOR
SampleSender::SampleSender() = default;
SampleSender::SampleSender(SampleSender&&) = default;
SampleSender& SampleSender::operator = (SampleSender&&) = default;

/// CTOR with bool calls Init
SampleSender::SampleSender(bool)
{
    Init();
}



SampleSender::~SampleSender()
{
    if (m_device)
    {
        Wait();
        Stop();
        ma_device_uninit(m_device.get());
    }
}


/// Intialize miniadio when not done so already.
SampleSender& SampleSender::Init()
{
    if (!m_device)
    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
        config.playback.channels = 2;             // Set to 0 to use the device's native channel count.
        config.sampleRate = m_sample_rate;        // @@ Set to 0 to use the device's native sample rate.
        config.dataCallback = data_callback;      // This function will be called when miniaudio needs more data.
        config.pUserData = this;   // Can be accessed from the device object (device.pUserData).

        auto device = std::make_unique<ma_device>();
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
    if (!m_is_running)
    {
        Reset();
        Init();
        m_is_running = true;
        ma_device_start(m_device.get());     // The device is sleeping by default so you'll need to start it manually.
    }
    return *this;
}

/// Wait for SampleSender (that is: wait for miniaudio thread to stop)
SampleSender& SampleSender::Wait()
{
    if (m_is_running)
    {
        m_event.wait();
        std::this_thread::sleep_for(500ms); // otherwise stops before all data was send
    }
    return *this;
}

/// Stop SampleSender (that is: stop miniaudio thread)
SampleSender& SampleSender::Stop()
{
    if (m_is_running)
    {
        ma_device_stop(m_device.get());
        m_is_running = false;
    }
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


/// Get next sound samples, 
/// called in miniaudio device thread
void SampleSender::DataCallback(ma_device* pDevice, void* pOutput, uint32_t frameCount)
{
    auto sampleRate = pDevice->sampleRate;
    auto channels = pDevice->playback.channels;     // # channels eg 2 is stereo
    float* foutput = reinterpret_cast<float*>(pOutput);
    int index = 0;
    for (auto n = 0u; n < frameCount; n++)
    {
        float value = m_done ? 0.0f : GetNextSample(sampleRate);
        for (auto chan = 0u; chan < channels; chan++)
        {
            foutput[index] = (chan == 0) ? m_volume_left  * value : 
                             (chan == 1) ? m_volume_right * value : 0.0f;
            index++;
        }
    }
    if (m_done)
    {
        m_event.Signal();
    }
}

/// Get next sample (audio value), depending on attached Pulsers.
/// This is typically 1.0 or -1.0 depending on edge; s/a GetEdge.
/// (any volume setting not used here)
float SampleSender::GetNextSample(uint32_t p_samplerate)
{
    Doublesec sample_period = 1s / double(p_samplerate);
    m_sample_time += sample_period;
    // Get duration to wait for next edge change based on what we need to do
    Doublesec time_to_wait = GetDurationWait();
    if (m_sample_time > time_to_wait)
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
        // m_sample_time = time_to_wait - m_sample_time;
        m_sample_time = 0s;
        if (OnNextSample())
        {
            m_done = true;
        }
    }
    return m_edge ? 1.0f : -1.0f;
}
