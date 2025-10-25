// ==============================================================================
// PROJECT:         zqloader
// FILE:            sampletowav.cpp
// DESCRIPTION:     Implementation for class SampleToWav.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "sampletowav.h"
#include <cstring>      // memcpy
#include <iostream>

// Get # bytes of data (that is not # samples) without header
size_t SampleToWav::GetDataByteSize() const
{
    return m_data.size() - sizeof(Header);
}



// Write wav header, needs data to be written first, because needs GetDataByteSize
void SampleToWav::WriteHeader()
{
    auto bits_per_sample = sizeof(SampleType) * 8;
    auto num_channels    = 2;
    auto sub_chunk1_size = 16;
    auto sub_chunk2_size = GetDataByteSize();

    memcpy(GetHeader().ChunkID, "RIFF", 4);
    GetHeader().ChunkSize = uint32_t(4 + 8 + sub_chunk1_size + 8 + sub_chunk2_size);
    memcpy(GetHeader().Format, "WAVE", 4);
    memcpy(GetHeader().Subchunk1ID, "fmt ", 4);
    GetHeader().Subchunk1Size = sub_chunk1_size;
    GetHeader().AudioFormat   = 1;      // PCM
    GetHeader().NumChannels   = uint16_t(num_channels);
    GetHeader().SampleRate    = m_sample_rate;
    GetHeader().ByteRate      = uint32_t(m_sample_rate * num_channels * bits_per_sample);
    GetHeader().BlockAlign    = uint16_t(num_channels * bits_per_sample / 8);
    GetHeader().BitsPerSample = uint16_t(bits_per_sample);
    memcpy(GetHeader().Subchunk2ID, "data", 4);
    GetHeader().Subchunk2Size = uint32_t(sub_chunk2_size);
}



SampleToWav &SampleToWav::Run()
{
    ReserveHeaderSpace();        // reserve space first!
    Doublesec sample_period = 1s / double(m_sample_rate);
    // static Doublesec prev {};
    bool done               = CheckDone();
    while(!done)
    {
        Doublesec time_to_wait = GetDurationWait();
        //  if(time_to_wait != prev)
        //  {
        //      prev = time_to_wait;
        //      std::cout << prev.count() << ' ';
        //  }
        m_sample_time += sample_period;
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
//            m_sample_time -= time_to_wait;
            OnNextSample();
            done = CheckDone();
        }
        SampleType value       = m_edge ? std::numeric_limits<SampleType>::max() :
                                 std::numeric_limits<SampleType>::min();
        SampleType value_left  = SampleType(float(value) * m_volume_left);
        SampleType value_right = SampleType(float(value) * m_volume_right);
        AddSample(value_left);
        AddSample(value_right);
    }
    AddSample(0);       // fixes loading in ZXSpin
    AddSample(0);       // fixes loading in ZXSpin
    WriteHeader();
    return *this;
}



/// Write stored buffer to given WAV file.
SampleToWav& SampleToWav::WriteToFile(std::ostream &p_stream)
{
    Run();
    p_stream.write(reinterpret_cast<const char *>(m_data.data()), m_data.size());
    return *this;
}



void SampleToWav::AddSample(SampleType p_sample)
{
    m_data.push_back(std::byte(p_sample & 0xff));
    m_data.push_back(std::byte((p_sample >> 8) & 0xff ));
}
