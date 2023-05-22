//==============================================================================
// PROJECT:         zqloader
// FILE:            miniaudio.cpp
// DESCRIPTION:     Definition and implementation for classes Pulser and friends.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once


#include "types.h"                  // Edge enum
#include <vector>
#include "datablock.h"              // DataBlock
#include "byte_tools.h"             // eg literal for std::byte
#include "spectrum_consts.h"        // TODO doesn't belong here m_tstate_dur
#include "spectrum_types.h"         // TODO doesn't belong here ZxBlockType
#include "spectrum_loader.h"

/// A pulser is used by miniaudio te create an audio stream.
/// Encodes binary data to a series of (audio) pulses that can be loaded by
/// ancient computers like the ZX spectrum.
/// Typically pulsers will toggle an output edge (as used for audio) after a certain 
/// duration given in T states.
/// Can also create leader tones  / pauses etc.
/// 
/// Base / interface for all pulsers
class Pulser
{
public:
    virtual ~Pulser() {}
    virtual bool Next() = 0;
    virtual int GetTstate() const = 0;
    virtual Edge GetEdge() const = 0;

protected:
    unsigned m_pulsnum = 0;                         // increased after each edge

};

//============================================================================

/// A pulser that when inserted causes a silence of given duration.
/// Does not need data.
/// Does not pulse, although can change pulse output at first call (after wait).
class PausePulser : public Pulser
{
    PausePulser(const PausePulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    PausePulser() = default;
    PausePulser(PausePulser&&) = default;

    /// Set length of pause in milliseconds.
    PausePulser& SetLength(std::chrono::milliseconds p_duration)
    {
        m_duration = p_duration;
        return *this;
    }

    /// Set length of pause in T-states.
    PausePulser& SetLength(int p_states)
    {
        m_duration_in_tstates = p_states;
        return *this;
    }

    /// Set edge value (so sound wave on/off) during pause. 
    /// Can be used to force it to a predefined value for blocks to follow.
    /// Egde::toggle will toggle once at first call.
    /// Note: Pauses first, then sets edge to value as given here.
    PausePulser& SetEdge(Edge p_edge)
    {
        m_edge = p_edge;
        return *this;
    }

    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }

protected:
    bool AtEnd() const
    {
        return m_duration_in_tstates ? true : Clock::now() > (m_timepoint + m_duration);
    }
    virtual bool Next() override
    {
        if (m_pulsnum == 0)
        {
            m_timepoint = Clock::now();     // set start timepoint at first call
            m_pulsnum++;    // just to make sure timepoint only set once
        }
        return AtEnd();
    }
    int GetTstate() const override
    {
        return m_duration_in_tstates;
    }
    Edge GetEdge() const override
    {
        auto edge = m_edge;
        if (m_edge == Edge::toggle)
        {
            const_cast<PausePulser*>(this)->m_edge = Edge::no_change;       // toggle only first time.
        }
        return edge;
    }

private:
    int m_duration_in_tstates = 0;
    Edge m_edge = Edge::no_change;
    std::chrono::milliseconds m_duration = 0ms;
    std::chrono::time_point<Clock> m_timepoint;
};


//============================================================================

/// A pulser that when inserted can be used to create a tone or pulse sequence 
/// with a certain pattern. Eg a leader tone.
/// Does not need data.
class TonePulser : public Pulser
{
    TonePulser(const TonePulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    TonePulser() = default;
    TonePulser(TonePulser&&) = default;

    /// Set tone pattern using one ore more given T-state durations.
    /// These form one pattern.
    /// Audio output edge well be toggled after here given each given T state.
    template<typename ... TParams>
    TonePulser& SetPattern(int p_first, TParams ... p_rest)
    {
        m_pattern.push_back(p_first);
        SetPattern(p_rest...);
        return *this;
    }

    /// Set length in # of pulses that is # complete patterns.
    TonePulser& SetLength(unsigned p_max_pulses)
    {
        unsigned pattsize = unsigned(m_pattern.size());
        if (pattsize)
        {
            m_max_pulses = pattsize * p_max_pulses;
        }
        else
        {
            m_max_pulses = p_max_pulses;
        }
        return *this;
    }

    /// Set length in milliseconds, rounds up to complete patterns.
    TonePulser& SetLength(std::chrono::milliseconds p_duration)
    {
        auto pat_dur = GetPatternDuration();
        if (pat_dur)
        {
            m_max_pulses = unsigned(p_duration / (m_tstate_dur * pat_dur));
            unsigned pattsize = unsigned(m_pattern.size());
            m_max_pulses += pattsize - (m_max_pulses % pattsize);    // round up to next multiple of pattsize
        }
        else
        {
            throw std::runtime_error("Cannot set length to time when pattern is unknown. Call SetPattern first.");
        }
        return *this;
    }
    
    /// Convenience, move me to given loader.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }
private:
    void SetPattern()
    {
        if (m_max_pulses == 0)
        {
            SetLength(1);       // default 1 pattern
        }
    }
    int GetPatternDuration() const
    {
        int retval = 0;
        for (auto pat : m_pattern)
        {
            retval += pat;
        }
        return retval;
    }
    int GetFirstPattern() const
    {
        return m_pattern.size() ? m_pattern[0] : 0;
    }
    bool AtEnd() const
    {
        return m_pulsnum >= m_max_pulses;
    }
    virtual bool Next() override
    {
        m_pulsnum++;
        return AtEnd();
    }
    int GetTstate() const override
    {
        auto retval = m_pattern.size() ? m_pattern[m_pulsnum % m_pattern.size()] : 0;
        return retval;
    }
    Edge GetEdge() const override
    {
        return Edge::toggle;
    }
private:

    std::vector<int> m_pattern;
    unsigned m_max_pulses = 0;

};

//============================================================================

/// A pulser that when inserted generated pulses based on the data attached.
/// So needs data attached.
/// Can optionally use 'pulse mode' having fixed length pulses like RS-232.
class DataPulser : public Pulser
{
    DataPulser(const DataPulser&) = delete;
public:
    DataPulser() = default;
    DataPulser(DataPulser&&) = default;


    /// Set Pattern (TStates) used to send a "1"
    template<typename ... TParams>
    DataPulser& SetOnePattern(int p_first, TParams ... p_rest)
    {
        m_one_pattern.push_back(p_first);
        SetOnePattern(p_rest...);
        return *this;
    }

    /// Set Pattern (TStates) used to send a "0"
    template<typename ... TParams>
    DataPulser& SetZeroPattern(int p_first, TParams ... p_rest)
    {
        m_zero_pattern.push_back(p_first);
        SetZeroPattern(p_rest...);
        return *this;
    }

    /// When != 0 switches to 'pulse mode' having fixed length pulses (given here) being 
    /// high or low for 1 or zero as RS-232.
    /// Needs start and stop bits for synchronisation.
    DataPulser& SetPulsDuration(int p_puls_duration)
    {
        m_puls_duration = p_puls_duration;
        m_start_duration = p_puls_duration;
        m_stop_duration = p_puls_duration;
        return *this;
    }

    /// Set start bit duration when in 'pulse mode'.
    DataPulser& SetStartBitDuration(int p_puls_duration)
    {
        m_start_duration = p_puls_duration;
        return *this;
    }

    /// Set stop bit duration when in 'pulse mode'.
    DataPulser& SetStopBitDuration(int p_puls_duration)
    {
        m_stop_duration = p_puls_duration;
        return *this;
    }
    /// Set the data to be pulsed.
    DataPulser& SetData(DataBlock p_data)
    {
        m_data = std::move(p_data);
        CalcChecksum();
        return *this;
    }

    /// Move this to given loader
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }

    /// Set block type
    DataPulser& SetBlockType(ZxBlockType p_type)
    {
        m_type = p_type;
        return *this;
    }


protected:

    virtual int GetTstate() const override
    {
        if (m_puls_duration == 0)       // not in 'pulse mode'
        {
            if (NeedStartSync())
            {
                return m_start_duration;
            }
            if (GetCurrentBit())
            {
                return m_one_pattern[m_pulsnum];
            }
            else
            {
                return m_zero_pattern[m_pulsnum];
            }
        }
        // Here in 'pulse mode'
        auto bitnum = m_bitnum % 10;    // 0 -> 9
        if (bitnum == 0)
        {
            return m_start_duration;
        }
        else if (bitnum == 9)
        {
            return m_stop_duration;
        }
        else
        {
            return m_puls_duration;
        }
    }
    virtual Edge GetEdge() const override
    {
        if (m_puls_duration == 0)   // not in 'pulse mode'
        {
            return Edge::toggle;
        }
        // Here in 'pulse mode'
        if (GetCurrentBit())
        {
            return Edge::one;
        }
        else
        {
            return Edge::zero;
        }

    }

    virtual bool Next() override
    {
        if (NeedStartSync())
        {
            m_need_start_sync = false;
        }
        else
        {
            m_pulsnum++;
            if (m_pulsnum >= GetMaxPuls())
            {
                m_pulsnum = 0;
                m_bitnum++;
                m_need_start_sync = true;
            }
        }
        return AtEnd();
    }
private:

    bool NeedStartSync() const
    {
        return m_puls_duration == 0 &&      // else in pulse mode, has its own start bits
            m_need_start_sync &&            // else just done
            m_start_duration &&             // master switch using start sync at all
            m_bitnum % 8 == 0;              // only at first bit
    }

    void SetOnePattern() {}
    void SetZeroPattern() {}

    std::byte GetByte(unsigned p_index) const
    {
        if (m_type == ZxBlockType::header ||
            m_type == ZxBlockType::data)
        {
            if (p_index == 0)
            {
                return std::byte(m_type);
            }
            else if (p_index <= m_data.size())
            {
                return m_data[p_index - 1];
            }
            else
            {
                return m_checksum;
            }
        }
        else
        {
            return m_data[p_index];
        }
    }
    void SetSize(size_t p_size)
    {
        m_data.resize(p_size);
    }
    size_t GetTotalSize() const
    {
        return (m_type == ZxBlockType::raw) ? m_data.size() :
            m_data.size() + 1 + 1;   // + type byte + checksum
    }
    void CalcChecksum()
    {
        m_checksum = CalculateChecksum(std::byte(m_type), m_data);         // type is included in checksum calc   
    }

    std::byte* GetDataPtr()
    {
        return m_data.data();
    }





    bool AtEnd() const
    {
        return (m_bitnum / 8) >= GetTotalSize();
    }


    unsigned GetMaxPuls() const
    {
        if (GetCurrentBit())
        {
            return unsigned(m_one_pattern.size());
        }
        else
        {
            return unsigned(m_zero_pattern.size());
        }
    }
    bool GetCurrentBit() const
    {
        if (m_puls_duration == 0)
        {
            auto bytenum = m_bitnum / 8;
            auto bitnum = m_bitnum % 8;    // 0 -> 7
            bitnum = 7 - bitnum;            // 8 -> 0 spectrum saves most sign bit first
            const std::byte& byte = GetByte(bytenum);
            bool retval = bool((byte >> bitnum) & 0x1_byte);
            return retval;
        }
        else
        {
            // in puls mode needs start and stop bit for synchronisation
            auto bytenum = m_bitnum / 10;
            auto bitnum = m_bitnum % 10;    // 0 -> 9
            if (bitnum == 0)
            {
                return m_start_bit;
            }
            else if (bitnum == 9)
            {
                return !m_start_bit;
            }
            else
            {
                bitnum = 8 - bitnum;            // 8 -> 0 spectrum saves most sign bit first
                const std::byte& byte = GetByte(bytenum);
                bool retval = bool((byte >> bitnum) & 0x1_byte);
                return retval;
            }
        }
    }
private:
    unsigned m_bitnum = 0;          // bitnum from start in block to be send.
    bool m_need_start_sync = true;
    ZxBlockType m_type;             // returned as first byte unless raw
    DataBlock m_data;
    std::byte m_checksum{};         // returned as last byte unless raw

    std::vector<int> m_zero_pattern;
    std::vector<int> m_one_pattern;
    int m_puls_duration = 0;        // when !0 go to puls mode (! toggle)
    int m_start_duration = 0;       // default s/a m_puls_duration
    int m_stop_duration = 0;        // default s/a m_puls_duration
    bool m_start_bit = true;        // value for start bit. Stopbit is always !m_start_bit
    // and m_start_bit should also be true, see table at zqloader.asm.
    // this means sync should end with 0!
};


//============================================================================

/// A pulser that when inserted can be used to show debug output.
/// To debug only.
/// Does not need data, neither pulses, so has no effect.
class DebugPulser : public Pulser
{
    DebugPulser(const DebugPulser&) = delete;
    using Clock = std::chrono::system_clock;
public:
    DebugPulser() = default;
    DebugPulser(DebugPulser&&) = default;


    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }

protected:
    virtual bool Next() override
    {
        return true;
    }
    int GetTstate() const override
    {
        return 0;
    }
    Edge GetEdge() const override
    {
        return Edge::no_change;
    }

private:
};

