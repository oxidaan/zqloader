// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.cpp
// DESCRIPTION:     Implementation of class TzxLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

//#define DEBUG_TZX
#include "tzxwriter.h"
#include "tzx_types.h"
#include "loadbinary.h"     // WriteBinary
#include <algorithm>

inline void DebugLog(const char *p_str)
{
    (void)p_str;
#ifdef DEBUG_TZX
    std::cout << p_str;
#endif
}

inline bool TzxWriter::PulserIsLeader(const Pulser &p_pulser)
{
    const TonePulser *tonepulser_ptr =  dynamic_cast<const TonePulser *>(&p_pulser);
    if(tonepulser_ptr)
    {
        const auto &pattern = tonepulser_ptr->m_pattern;
        return pattern.size() == 1 || (pattern.size() == 2 && pattern[0] == pattern[1]);
    }    
    return false;
}

inline bool TzxWriter::PulserIsSync(const Pulser &p_pulser)
{
    const TonePulser *tonepulser_ptr =  dynamic_cast<const TonePulser *>(&p_pulser);
    if(tonepulser_ptr)
    {
        const auto &pattern = tonepulser_ptr->m_pattern;
        return pattern.size() == tonepulser_ptr->m_max_pulses;      // just one repeat of single pattern
    }    
    return false;
}



void TzxWriter::WriteTzxFile(const Pulsers& p_pulsers, std::ostream& p_stream)
{
    p_stream << "ZXTape!";
    WriteBinary(p_stream, std::byte{ 0x1A });
    WriteBinary(p_stream, std::byte{ 1 });
    WriteBinary(p_stream, std::byte{ 20 });
    const Pulser *prevprev{};
    const Pulser *prev{};
    const Pulser *current{};
    for(const auto &p : p_pulsers)
    {
        current = p.get();
        if(prevprev && PulserIsLeader(*prevprev) &&
           prev && PulserIsSync(*prev) &&
           dynamic_cast<const DataPulser *>(current))
        {
            prev = nullptr;
            current = nullptr;
        }
        else
        {
            TryWriteAsTzxBlock<TonePulser>(prevprev, p_stream);
            TryWriteAsTzxBlock<PausePulser>(prevprev, p_stream);
            TryWriteAsTzxBlock<DataPulser>(prevprev, p_stream);
        }

        prevprev = prev;
        prev = current;
    }
    TryWriteAsTzxBlock<TonePulser>(prevprev, p_stream);
    TryWriteAsTzxBlock<PausePulser>(prevprev, p_stream);
    TryWriteAsTzxBlock<DataPulser>(prevprev, p_stream);
    TryWriteAsTzxBlock<TonePulser>(prev, p_stream);
    TryWriteAsTzxBlock<PausePulser>(prev, p_stream);
    TryWriteAsTzxBlock<DataPulser>(prev, p_stream);
}

void TzxWriter::WriteAsTzxBlock(const TonePulser &p_pulser, std::ostream &p_stream) const
{
    const auto &pattern = p_pulser.m_pattern;
    if(pattern.size() == 1)
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << p_pulser.m_max_pulses << " Dur=" << ((p_pulser.m_max_pulses * pattern[0]) * p_pulser.m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(p_pulser.m_max_pulses));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else if(pattern.size() == 2 && pattern[0] == pattern[1])
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << 2 * p_pulser.m_max_pulses << " Dur=" << ((2 * p_pulser.m_max_pulses * pattern[0]) * p_pulser.m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(2 * p_pulser.m_max_pulses ));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock (instead of Puretone) because pulses have different length
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block{};
        generalized_data_block.pause        = 0;
        generalized_data_block.totp         = 1;        // one tone
        generalized_data_block.npp          = BYTE(pattern.size());
        generalized_data_block.asp          = 1;        // one symbol

        generalized_data_block.totd         = 0;        // no data
        generalized_data_block.npd          = 0;        // should not be used when totd is 0
        generalized_data_block.asd          = 2;        // should not be used when totd is 0 But '0 = 256'. Must be power of 2.
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "Tone: GeneralizedDataBlock pattern-size=" << pattern.size() << " #pulses=" << p_pulser.m_max_pulses << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
        auto pos_start = p_stream.tellp();
#endif
        WriteBinary(p_stream, generalized_data_block);
        DebugLog(" | SymDef: ");
        // Symbol definitions 
        if( generalized_data_block.totp > 0)        // Note: is 1! This defines the one and only tone.
        {
            for(int n = 0; n < generalized_data_block.asp; n++)      // Note: there is one only!
            {
                GeneralizedDataBlock::SymDef symdef{};
                symdef.m_edge = Edge::toggle;
                WriteBinary(p_stream, symdef);
                for (int i = 0; i < generalized_data_block.npp; i++)
                {
                    WriteBinary(p_stream, WORD(pattern[i]));
                }
            }
            DebugLog(" | Prle: ");
            // Symbol sequence (which symbol + count)
            for (unsigned n = 0u; n < generalized_data_block.totp; n++)     // Note: there is one only!
            {
                GeneralizedDataBlock::Prle prle{};
                prle.m_symbol      = 0; // the one and only symbol
                prle.m_repetitions = WORD(p_pulser.m_max_pulses / generalized_data_block.npp);
                WriteBinary(p_stream, prle);
            }
        }

        if(generalized_data_block.totd > 0)     // Note: is 0. (no data)
        {
            // no data so no SYMDEF[ASD]/PRLE[TOTD]
        }
#ifdef DEBUG_TZX
        auto pos_end = p_stream.tellp();
        if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(0) +  sizeof(generalized_data_block.block_length))
        {
            std::stringstream ss;
            ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
            throw std::runtime_error(ss.str());
        }
#endif
        DebugLog(" [End Tone: GeneralizedDataBlock]\n");
    }
}



void TzxWriter::WriteAsTzxBlock(const PausePulser &p_pulser, std::ostream& p_stream) const
{
    if(p_pulser.m_duration_in_tstates > 0xffff && p_pulser.m_edge == Edge::no_change)
    {
        auto len_in_ms = WORD((1000 * p_pulser.m_duration_in_tstates * p_pulser.m_tstate_dur).count());
        std::cout << "Pause: PauseOrStopthetapecommand " << len_in_ms << "ms; (TStates=" << p_pulser.m_duration_in_tstates << ")" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::PauseOrStopthetapecommand);
        WriteBinary(p_stream, len_in_ms);
        DebugLog(" [End Pause: PauseOrStopthetapecommand]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock because PauseOrStopthetapecommand is not accurate
        // enough and might need edge at end of pause
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block{};
        generalized_data_block.pause        = 0;
        generalized_data_block.totp         = 2;    // pause, then the edge
        generalized_data_block.npp          = 1;
        generalized_data_block.asp          = 2;    // two symbols one for pause one for edge

        generalized_data_block.totd         = 0;    // no data
        generalized_data_block.npd          = 0;    // should not be used when totd is 0
        generalized_data_block.asd          = 2;    // should not be used when totd is 0 But '0 = 256'. Must be power of 2.
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "Pause: GeneralizedDataBlock TStates=" << p_pulser.m_duration_in_tstates << " Egde= " << p_pulser.m_edge << std::endl;
#ifdef DEBUG_TZX
        auto pos_start = p_stream.tellp();
#endif
        WriteBinary(p_stream, generalized_data_block);

        if( generalized_data_block.totp > 0)        // Note: is 2! This defines the two symbols
        {
            DebugLog(" | Symdef#1: ");
            // first symdef for pause (need to wait first) (this is the actual pause)
            GeneralizedDataBlock::SymDef symdef{};
            symdef.m_edge = Edge::no_change;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(p_pulser.m_duration_in_tstates));

            DebugLog(" | Symdef#2: ");
            // 2nd symdef for edge
            symdef.m_edge = p_pulser.m_edge;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(0));

            DebugLog(" | Prle#1: ");
            GeneralizedDataBlock::Prle prle{};
            prle.m_symbol      = 0; // first symbol
            prle.m_repetitions = 1;
            WriteBinary(p_stream, prle);

            DebugLog(" | Prle#2: ");
            prle.m_symbol      = 1; // 2nd symbol
            prle.m_repetitions = 1;
            WriteBinary(p_stream, prle);
        }
    
        if(generalized_data_block.totd > 0)     // Note: is 0. (no data)
        {
            // no data so no SYMDEF[ASD]/PRLE[TOTD]
        }
#ifdef DEBUG_TZX
        auto pos_end = p_stream.tellp();
        if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(0) +  sizeof(generalized_data_block.block_length))
        {
            std::stringstream ss;
            ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
            throw std::runtime_error(ss.str());
        }
#endif
    }
    DebugLog(" [End Pause: GeneralizedDataBlock]\n");

}



void TzxWriter::WriteAsTzxBlock(const DataPulser &p_pulser, std::ostream& p_stream) const
{
    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = 0;        // no pilot/sync block here (just data)
    generalized_data_block.npp          = 0;        // not used when totp is 0
    generalized_data_block.asp          = 1;        // not used when totp is 0, but 0 is 256.

    generalized_data_block.totd         = DWORD(p_pulser.GetTotalSize() * 8);        // so # bits
    generalized_data_block.npd          = BYTE(std::max(p_pulser.m_zero_pattern.size(), p_pulser.m_one_pattern.size()));  // eg 2 for standard, 1 for turbo
    generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(p_pulser.GetTotalSize()));
    std::cout << "Data: GeneralizedDataBlock blocklen=" << generalized_data_block.GetBlockLength(p_pulser.GetTotalSize()) << " #bytes=" << p_pulser.GetTotalSize() << " totd=" << generalized_data_block.totd << " m_max_pulses=" << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
    auto pos_start = p_stream.tellp();
#endif
    WriteBinary(p_stream, generalized_data_block);
        
    if(generalized_data_block.totp > 0)      // Note: is 0. (no leader/sync))
    {
        // no pulses, so no SYMDEF[ASP]/PRLE[TOTP]
    }

    for(int n = 0; n < generalized_data_block.asd; n++)     // should be 2. Pattern for 0 and 1
    {
        DebugLog(" | SymDef: ");
        GeneralizedDataBlock::SymDef symdef{};
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        const auto & pattern = (n == 0) ? p_pulser.m_zero_pattern : p_pulser.m_one_pattern;
        for (int i = 0; i < generalized_data_block.npd; i++)        // should be 1 for turbo blocks; 2 for standard speed.
        {
            WriteBinary(p_stream, (i < pattern.size()) ? WORD(pattern[i]) : WORD(0));
        }
    }

    DebugLog(" | Data: ");
    for(int i = 0; i < p_pulser.GetTotalSize(); i++)
    {
        WriteBinary(p_stream, p_pulser.GetByte(i));
    }
#ifdef DEBUG_TZX
    auto pos_end = p_stream.tellp();
    if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(GetTotalSize()) +  sizeof(generalized_data_block.block_length))
    {
        std::stringstream ss;
        ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
        throw std::runtime_error(ss.str());
    }
#endif
    DebugLog(" [End Data: GeneralizedDataBlock]\n");
    
}
