// ==============================================================================
// PROJECT:         zqloader
// FILE:            sampletowav.h
// DESCRIPTION:     Definition of class SampleToWav.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include <functional>           // std::function
#include "types.h"              // Doublesec
#include "datablock.h"
#include <iostream>

/// Largely same interface as SampleSender but writes to wav file instead of outputting sound.
/// Used call-backs are set with:
/// SetOnGetDurationWait
/// SetOnNextSample
/// SetOnGetEdge
class SampleToWav
{
    using SampleType = int16_t;

    // WAV file header.
    // http://tiny.systems/software/soundProgrammer/WavFormatDocs.pdf
    struct Header
    {
        char       ChunkID[4];      // RIFF
        uint32_t   ChunkSize;
        char       Format[4];       // WAVE
        char       Subchunk1ID[4];  // "fmt "
        uint32_t   Subchunk1Size;   // 16 for PCM, the size of the block until Subchunk2ID
        uint16_t   AudioFormat;     // PCM = 1
        uint16_t   NumChannels;     // mono=1; stereo=2 etc
        uint32_t   SampleRate;
        uint32_t   ByteRate;
        uint16_t   BlockAlign;
        uint16_t   BitsPerSample;
        char       Subchunk2ID[4];  // "data"
        uint32_t   Subchunk2Size;   // # bytes in data
    };

    static_assert(sizeof(Header) == 44, "Check size of Header");

public:

    using NextSampleFun  = std::function<bool (void)>;
    using GetDurationFun = std::function<Doublesec (void)>;
    using GetEdgeFun     = std::function<Edge (void)>;
    using CheckDoneFun   = std::function<bool (void)>;

public:

    SampleToWav()  = default;
    ~SampleToWav() = default;


    /// Set callback to get wait duration (in seconds/DoubleSec)_
    /// This should get duration to wait for next edge change based
    /// on what we need to do.
    SampleToWav& SetOnGetDurationWait(GetDurationFun p_fun)
    {
        m_OnGetDurationWait = std::move(p_fun);
        return *this;
    }


    /// Set callback to move to next sample.
    /// Given function should move to next sample. Return true when done.
    SampleToWav& SetOnNextSample(NextSampleFun p_fun)
    {
        m_OnNextSample = std::move(p_fun);
        return *this;
    }


    /// Set callback to get value what edge needs to become.
    /// Given function should return an Edge enum (eg One/Zero/Toggle)
    /// So what needs to be done with the sound output binary signal.
    SampleToWav& SetOnGetEdge(GetEdgeFun p_fun)
    {
        m_OnGetEdge = std::move(p_fun);
        return *this;
    }


    SampleToWav& SetOnCheckDone(CheckDoneFun p_fun)
    {
        m_OnCheckDone = std::move(p_fun);
        return *this;
    }


    /// Run: fill internal buffer with WAV data.
    /// Called by WriteToFile
    SampleToWav& Run();

    /// Run; then write stored buffer to given WAV file.
    SampleToWav& WriteToFile(std::ostream &p_file);

    /// Set volume. When negative basically inverts. 100 is max.
    SampleToWav& SetVolume(int p_volume_left, int p_volume_right)
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


    /// Set sample rate (hz), when not set use 48000 which is s/a miniaudio default.
    SampleToWav& SetSampleRate(int p_sample_rate)
    {
        if(p_sample_rate)       // else use default
        {
            m_sample_rate = p_sample_rate;
        }
        return *this;
    }


    /// Get (final) wav file size (logging only)
    size_t GetSize() const
    {
        return m_data.size();
    }


    /// Get (final) wav duration (logging only)
    Doublesec GetDuration() const
    {
        return Doublesec((GetDataByteSize() / (2 * sizeof(SampleType))) / m_sample_rate);
    }

private:

    void ReserveHeaderSpace()
    {
        if (m_data.size() < sizeof(Header))
        {
            m_data.resize(sizeof(Header));
        }
    }


    Header& GetHeader()
    {
        ReserveHeaderSpace();
        return *reinterpret_cast<Header *>(m_data.data());
    }


    const Header& GetHeader() const
    {
        return *reinterpret_cast<const Header*>(m_data.data());
    }


    void WriteHeader();
    void AddSample(SampleType p_sample);

    size_t GetDataByteSize() const;

    void Reset()
    {
        m_edge        = false;
        m_sample_time = 0ms;
    }


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


    bool CheckDone()
    {
        if (m_OnCheckDone)
        {
            return m_OnCheckDone();
        }
        return true;
    }

private:

    DataBlock        m_data;
    NextSampleFun    m_OnNextSample;
    GetDurationFun   m_OnGetDurationWait;
    GetEdgeFun       m_OnGetEdge;
    CheckDoneFun     m_OnCheckDone;
    bool             m_edge        = false;                 // output value toggles between 1/0
    Doublesec        m_sample_time = 0ms;                   // time since last edge change
//    int m_sample_rate = 44100;                            // CD standard
//    int m_sample_rate = 70000;                            // spectrum clock 3500000/50
    int              m_sample_rate  = 48000;                // s/a miniaudio

    float            m_volume_left  = 1.0f;                 // -1.0 .. 1.0
    float            m_volume_right = 1.0f;                 // -1.0 .. 1.0

}; // class SampleToWav