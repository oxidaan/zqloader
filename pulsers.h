//==============================================================================
// PROJECT:         zqloader
// FILE:            pulsers.h
// DESCRIPTION:     Definition and implementation for classes Pulser and friends.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once


#include "types.h"                  // Edge enum
#include "datablock.h"              // DataBlock
#include "byte_tools.h"             // eg literal for std::byte
#include <ostream>                  // std::ostream


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
    Pulser(const Pulser &) = delete;

public:

    Pulser(Doublesec p_tstate_dur)
        : m_tstate_dur(p_tstate_dur)  // T-state duration in seconds
    {}



    Pulser(Pulser&&) = default;
    virtual ~Pulser() 
    {}

    virtual Edge GetEdge() const = 0;       // What to do after wait
    virtual bool Next()          = 0;       // move to next pulse/edge, return true when done.





    // Get duration to wait now as Doublesec (used at miniaudio thread)
    virtual Doublesec GetDurationWait() const final
    {
        return GetTstate() * m_tstate_dur;
    }


    // Get total duration in TStates (to show only)
    virtual int GetDurationInTStates() const = 0;

    // get total duration as Doublesec (to show only)
    virtual Doublesec GetDuration() const final
    {
        return GetDurationInTStates() * m_tstate_dur;
    }

protected:

    // Get # TStates to wait now  (called from  GetDurationWait)
    virtual int GetTstate() const = 0;

protected:

    unsigned          m_pulsnum = 0;                // increased after each edge
    const Doublesec   m_tstate_dur;

};



// ============================================================================

/// A pulser that when inserted causes a silence of given duration.
/// Does not need data.
/// Does not pulse, although can change pulse/edge output at first call (after wait).
class PausePulser : public Pulser
{
    friend class TzxWriter;

    PausePulser(const PausePulser&) = delete;
    using Clock = std::chrono::system_clock;

public:

    PausePulser(Doublesec p_tstate_dur) : Pulser(p_tstate_dur)
    {}



    PausePulser(PausePulser&&) = default;

    /// Set length of pause in milliseconds.
    PausePulser& SetLength(std::chrono::milliseconds p_duration)
    {
        m_duration_in_tstates = int(p_duration / m_tstate_dur);
        return *this;
    }


    /// Set length of pause in T-states.
    PausePulser& SetLength(int p_states)
    {
        m_duration_in_tstates = p_states;
        return *this;
    }

    /// Set edge value (so sound wave on/off) after pause.
    /// Can be used to force it to a predefined value for blocks to follow.
    /// Egde::toggle will toggle the wave value.
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



    Edge GetEdge() const override
    {
        auto edge = m_edge;
        if (m_edge == Edge::toggle)
        {
            const_cast<PausePulser*>( this )->m_edge = Edge::no_change;       // toggle only first time.
        }
        return edge;
    }



    bool Next() override
    {
        return AtEnd();
    }



    int GetDurationInTStates() const override
    {
        return m_duration_in_tstates;
    }

protected:

    int GetTstate() const override
    {
        return m_duration_in_tstates;
    }



    bool AtEnd() const
    {
        return true;
    }

private:

    int    m_duration_in_tstates = 0;
    Edge   m_edge                = Edge::no_change;
};


// ============================================================================

/// A pulser that when inserted can be used to create a tone or pulse sequence
/// with a certain pattern. Eg a leader tone.
/// Does not need data.
class TonePulser : public Pulser
{
    friend class TzxWriter;
    TonePulser(const TonePulser&) = delete;
    using Clock = std::chrono::system_clock;

public:

    TonePulser(Doublesec p_tstate_dur) : Pulser(p_tstate_dur)
    {}



    TonePulser(TonePulser&&) = default;

    /// Set tone pattern using one ore more given T-state durations.
    /// These form one pattern.
    /// Audio output edge well be toggled after each here given T state.
    template<typename ... TParams>
    TonePulser& SetPattern(int p_first, TParams ... p_rest)
    {
        m_pattern.push_back(p_first);
        SetPattern(p_rest ...);
        return *this;
    }


    /// Set length in # of pulses that is # complete patterns.
    TonePulser& SetLength(unsigned p_max_pulses);

    /// Set length in milliseconds, rounds up to complete patterns.
    TonePulser& SetLength(std::chrono::milliseconds p_duration);

    /// Set length infinite
    TonePulser& SetInfiniteLength()
    {
        m_forever = true;
        return *this;
    }



    /// Convenience, move me to given loader.
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }



    Edge GetEdge() const override
    {
        return Edge::toggle;
    }



    bool Next() override
    {
        m_pulsnum++;
        return AtEnd();
    }

    int GetDurationInTStates() const override
    {
        return  int(m_pattern.size() ? ((m_max_pulses * GetPatternDuration()) /  m_pattern.size()) : 0);
    }

protected:

    int GetTstate() const override
    {
        auto retval = m_pattern.size() ? m_pattern[m_pulsnum % m_pattern.size()] : 0;
        return retval;
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



    bool AtEnd() const
    {
        return m_forever == false && m_pulsnum >= m_max_pulses;
    }

private:

    std::vector<int>   m_pattern;
    unsigned           m_max_pulses = 0;        // max. pulses that is total of edges.
                                                // Note m_max_pulses % m_pattern.size() should be 0 else ends halfway a pattern.
    bool               m_forever    = false;

}; // class TonePulser

// ============================================================================

/// A pulser that when inserted generated pulses based on the data attached.
/// So needs data attached.
/// Can optionally use 'pulse mode' having fixed length pulses like RS-232.
class DataPulser : public Pulser
{
    friend class TzxWriter;
    DataPulser(const DataPulser&) = delete;

public:

    DataPulser(Doublesec p_tstate_dur) : Pulser(p_tstate_dur)
    {}



    DataPulser(DataPulser&&) = default;


    /// Set Pattern (TStates) used to send a "1"
    template<typename ... TParams>
    DataPulser& SetOnePattern(int p_first, TParams ... p_rest)
    {
        m_one_pattern.push_back(p_first);
        SetOnePattern(p_rest ...);
        return *this;
    }



    /// Set Pattern (TStates) used to send a "0"
    template<typename ... TParams>
    DataPulser& SetZeroPattern(int p_first, TParams ... p_rest)
    {
        m_zero_pattern.push_back(p_first);
        SetZeroPattern(p_rest ...);
        return *this;
    }



    /// When != 0 switches to 'pulse mode' having fixed length pulses (given here) being
    /// high or low for 1 or zero as RS-232.
    /// Needs start and stop bits for synchronisation.
    /// (Tried but not working for ZX Spectrum, but might work for other machines).
    DataPulser& SetPulsDuration(int p_puls_duration)
    {
        m_puls_duration  = p_puls_duration;
        m_start_duration = p_puls_duration;
        m_stop_duration  = p_puls_duration;
        return *this;
    }



    /// Set delay for byte store
    DataPulser& SetEndOfByteDelay(int p_delay)
    {
        m_delay_duration = p_delay;
        return *this;
    }



    /// Set start bit duration ('pulse mode' / RS232 like)
    DataPulser& SetStartBitDuration(int p_puls_duration)
    {
        m_start_duration = p_puls_duration;
        return *this;
    }



    /// Set stop bit duration ('pulse mode' / RS232 like)
    DataPulser& SetStopBitDuration(int p_puls_duration)
    {
        m_stop_duration = p_puls_duration;
        return *this;
    }



    /// Set the data to be pulsed.
    DataPulser& SetData(DataBlock p_data)
    {
        m_data = std::move(p_data);
        return *this;
    }



    /// Move this to given loader
    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }



    Edge GetEdge() const override
    {
        if (!IsPulseMode())
        {
            return Edge::toggle;
        }
        // Here in 'pulse mode' (rs232 like)
        if (GetCurrentBit())
        {
            return Edge::one;
        }
        else
        {
            return Edge::zero;
        }

    }



    bool Next() override
    {
        m_pulsnum++;
        if (m_pulsnum >= GetMaxPuls())
        {
            m_pulsnum = 0;
            m_bitnum++;
        }
        return AtEnd();
    }



    int GetDurationInTStates() const override;

protected:

    /// Get # TStates to wait now
    int GetTstate() const override
    {
        if (!IsPulseMode())       // not in 'pulse mode' (so normal)
        {
            if (GetCurrentBit())
            {
                return m_one_pattern[m_pulsnum] + GetExtraDelay();
            }
            else
            {
                return m_zero_pattern[m_pulsnum] + GetExtraDelay();
            }
        }
        else
        {
            // Here in 'pulse mode' (so rs232 like)
            auto bitnum = m_bitnum % 10;    // 0 -> 9
            if (bitnum == 0)
            {
                return m_start_duration;
            }
            else if (bitnum == 9)
            {
                return m_stop_duration;
            }
            return m_puls_duration;
        }
    }

private:

    // When true is in 'pulse' mode, so rs232 like: no toggles but
    // fixed one for 1 and zero for 0.
    // Else false: normal mode.
    bool IsPulseMode() const
    {
        return m_puls_duration != 0;
    }



    // Get delay ZX spectrum needs at end of byte to store it.
    // Extra delay before each byte except first.
    int GetExtraDelay() const
    {
        if(  ( m_bitnum % 8 ) == 0 &&          // before first bit of all bytes except
             m_bitnum != 0 &&                  // very first bit
             m_pulsnum == 0)                   // also only before first pulse of first bit pattern
        {
            return m_delay_duration;
        }
        return 0;
    }



    void SetOnePattern()
    {
    }



    void SetZeroPattern()
    {
    }



    std::byte GetByte(unsigned p_index) const
    {
        return m_data[p_index];
    }



    void SetSize(size_t p_size)
    {
        m_data.resize(p_size);
    }



    // Data size in bytes
    size_t GetTotalSize() const
    {
        return m_data.size();
    }



    std::byte* GetDataPtr()
    {
        return m_data.data();
    }



    bool AtEnd() const
    {
        return ( m_bitnum / 8 ) >= GetTotalSize();
    }



    // # pulses(=edges) a single bit either 1 or zero needs.
    // At Spectrum typically 2 (2 edges per bit) (but ZX81 has higher)
    // Our turboloader typically 1.
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



    // Get current bit, based on m_bitnum
    bool GetCurrentBit() const
    {
        if (!IsPulseMode())
        {
            // normal mode.
            auto bytenum          = m_bitnum / 8;
            auto bitnum           = m_bitnum % 8; // 0 -> 7
            bitnum = 7 - bitnum;                  // 8 -> 0 spectrum saves most sign bit first
            const std::byte& byte = GetByte(bytenum);
            bool retval           = bool(( byte >> bitnum ) & 0x1_byte);
            return retval;
        }
        else
        {
            // in puls mode (rs232) needs start and stop bit for synchronisation
            auto bytenum = m_bitnum / 10;
            auto bitnum  = m_bitnum % 10;   // 0 -> 9
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
                bool retval           = bool(( byte >> bitnum ) & 0x1_byte);
                return retval;
            }
        }
    }

private:

    unsigned           m_bitnum = 0; // bitnum from start in block to be send (runs to total # bits of data)
    DataBlock          m_data;

    std::vector<int>   m_zero_pattern;
    std::vector<int>   m_one_pattern;
    int                m_puls_duration  = 0;    // when !0 go to puls mode (rs232 like) (! toggle), then this duration of each pulse.
    int                m_start_duration = 0;    // In puls mode: start bit duration default s/a m_puls_duration
    int                m_stop_duration  = 0;    // stop bit duration  default s/a m_puls_duration
    int                m_delay_duration = 0;    // extra wait duration and end of byte to store byte.
    bool               m_start_bit      = true; // value for start bit. Stopbit is always !m_start_bit
    // and m_start_bit should also be true, see table at zqloader.asm.
    // this means sync should end with 0!
}; // class DataPulser


// ============================================================================

/// A pulser that when inserted can be used to show debug output.
/// To debug only.
/// Does not need data, neither pulses, so has no effect.
class DebugPulser : public Pulser
{
    DebugPulser(const DebugPulser&) = delete;
    using Clock = std::chrono::system_clock;

public:

    DebugPulser(Doublesec p_tstate_dur) : Pulser(p_tstate_dur)
    {}



    DebugPulser(DebugPulser&&) = default;


    template<class TLoader>
    void MoveToLoader(TLoader& p_loader)
    {
        p_loader.AddPulser(std::move(*this));
    }





    int GetDurationInTStates() const override
    {
        return 0;
    }

protected:

    bool Next() override
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
