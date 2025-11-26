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
#include "spectrum_consts.h"

inline void DebugLog(const char *p_str)
{
    (void)p_str;
#ifdef DEBUG_TZX
    std::cout << p_str;
#endif
}

// Is given (Tone)Pulser a leader tone?
inline bool TzxWriter::IsPulserLeader(const Pulser &p_pulser)
{
    const TonePulser *pulser_ptr =  dynamic_cast<const TonePulser *>(&p_pulser);
    if(pulser_ptr)
    {
        const auto &pattern = pulser_ptr->GetPattern();
        return pulser_ptr->GetMaxPulses() > pattern.size() &&       // more than a single
            (pattern.size() == 1 || (pattern.size() == 2 && pattern[0] == pattern[1]));
    }    
    return false;
}

// Is given Pulser a sync?
inline bool TzxWriter::IsPulserSync(const Pulser &p_pulser)
{
    const TonePulser *pulser_ptr =  dynamic_cast<const TonePulser *>(&p_pulser);
    if(pulser_ptr)
    {
        const auto &pattern = pulser_ptr->GetPattern();
        return pattern.size() == pulser_ptr->GetMaxPulses();      // just one repeat of single pattern
    }    
    return false;
}


// Is given Pulser spectrum data at normal/standard ROM speed?
// Can write as ID 10 - Standard Speed Data Block
inline bool TzxWriter::IsPulserSpectrumData(const Pulser &p_pulser)
{
    const DataPulser *pulser_ptr =  dynamic_cast<const DataPulser *>(&p_pulser);
    if(pulser_ptr)
    {
        const auto &one_pattern = pulser_ptr->GetOnePattern();
        const auto &zero_pattern = pulser_ptr->GetZeroPattern();
        return one_pattern.size()  == 2 &&  one_pattern[0] == spectrum::tstate_one  &&  one_pattern[1] == spectrum::tstate_one &&
               zero_pattern.size() == 2 && zero_pattern[0] == spectrum::tstate_zero && zero_pattern[1] == spectrum::tstate_zero;

    }
    return false;
}


// Is given Pulser a zqloader turbo data block? With 2 same edges for each zero/one.
// (Can write as ID 11 - Turbo Speed Data Block)
inline bool TzxWriter::IsPulserTurboData(const Pulser &p_pulser)
{
    const DataPulser *pulser_ptr =  dynamic_cast<const DataPulser *>(&p_pulser);
    if(pulser_ptr ) // && !IsPulserSpectrumData(p_pulser))
    {
        const auto &one_pattern = pulser_ptr->GetOnePattern();
        const auto &zero_pattern = pulser_ptr->GetZeroPattern();
        return one_pattern.size()  == 2 &&  one_pattern[0] == one_pattern[1] &&
               zero_pattern.size() == 2 && zero_pattern[0] == zero_pattern[1];

    }
    return false;
}


// Is given Pulser a zqloader turbo data block? One edge per bit.
// (Needs ID 19 - Generalized Data Block)
inline bool TzxWriter::IsZqLoaderTurboData(const Pulser &p_pulser)
{
    const DataPulser *pulser_ptr =  dynamic_cast<const DataPulser *>(&p_pulser);
    return pulser_ptr && !IsPulserSpectrumData(*pulser_ptr) && !IsPulserTurboData(*pulser_ptr);
}

void TzxWriter::WriteTzxFile(const Pulsers& p_pulsers, std::ostream& p_stream,  Doublesec p_tstate_dur)
{
    p_stream << "ZXTape!";
    WriteBinary(p_stream, std::byte{ 0x1A });
    WriteBinary(p_stream, std::byte{ 1 });
    WriteBinary(p_stream, std::byte{ 20 });
    const Pulser *prevprev{};
    const Pulser *prev{};
    const Pulser *current{};
    TzxBlockWriter writer(p_stream, p_tstate_dur);
    for(const auto &p : p_pulsers)
    {
        current = p.get();

        // Is it leader-sync-data? A standard Spectrum block?
        if(prevprev && IsPulserLeader(*prevprev) &&
           prev && IsPulserSync(*prev) &&
           IsPulserSpectrumData(*current))
        {
            WriteAsStandardSpectrum(p_stream, *dynamic_cast<const TonePulser *>(prevprev), *dynamic_cast<const TonePulser *>(prev), *dynamic_cast<const DataPulser *>(current));
            prev = nullptr;
            current = nullptr;
        }
        // Is it leader-sync-data? A zqloader turbo block?
        else if(prevprev && IsPulserLeader(*prevprev) &&
           prev && IsPulserSync(*prev) &&
           IsPulserTurboData(*current))
        {
            WriteAsTurboData(p_stream,*dynamic_cast<const TonePulser *>(prevprev), *dynamic_cast<const TonePulser *>(prev), *dynamic_cast<const DataPulser *>(current));
            prev = nullptr;
            current = nullptr;
        }
        else if(/*prevprev && IsPulserLeader(*prevprev) && */
           prev && IsPulserSync(*prev) &&
           IsZqLoaderTurboData(*current))
        {
            WriteAsZqLoaderTurboData(p_stream, dynamic_cast<const TonePulser *>(prevprev), *dynamic_cast<const TonePulser *>(prev), *dynamic_cast<const DataPulser *>(current));
            prev = nullptr;
            current = nullptr;
        }
        else if(prevprev)
        {
            prevprev->Accept(writer);       // -> WriteAsTzxBlock
        }

        prevprev = prev;
        prev = current;
    }
    // Write remaining last two blocks when not already done so.
    prevprev->Accept(writer);        // -> WriteAsTzxBlock
    prev->Accept(writer);            // -> WriteAsTzxBlock
}

void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const TonePulser &p_pulser, std::ostream &p_stream) const
{
    const auto &pattern = p_pulser.GetPattern();
    auto max_pulses = p_pulser.GetMaxPulses();
    if(pattern.size() == 1)
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << max_pulses << " Dur=" << ((max_pulses * pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(max_pulses));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else if(pattern.size() == 2 && pattern[0] == pattern[1])
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << 2 * max_pulses << " Dur=" << ((2 * max_pulses * pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary<TzxBlockType>(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary<WORD>(p_stream, WORD(pattern[0]));
        WriteBinary<WORD>(p_stream, WORD(2 * max_pulses ));
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
        std::cout << "Tone: GeneralizedDataBlock pattern-size=" << pattern.size() << " #pulses=" << max_pulses << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
        auto pos_start = p_stream.tellp();
#endif
        WriteBinary(p_stream, generalized_data_block);
        DebugLog(" | SymDef: ");
        // Symbol definitions 
        if( generalized_data_block.totp > 0)        // Note: is 1! This defines the one and only tone.
        {
            for(int asp = 0; asp < generalized_data_block.asp; asp++)      // Note: there is one only!
            {
                GeneralizedDataBlock::SymDef symdef{};
                symdef.m_edge = Edge::toggle;
                WriteBinary(p_stream, symdef);
                for (int npp = 0; npp < generalized_data_block.npp; npp++)
                {
                    WriteBinary(p_stream, WORD(pattern[npp]));
                }
            }
            DebugLog(" | Prle: ");
            // Symbol sequence (which symbol + count)
            for (unsigned totp = 0u; totp < generalized_data_block.totp; totp++)     // Note: there is one only!
            {
                GeneralizedDataBlock::Prle prle{};
                prle.m_symbol      = totp; // the one and only symbol
                prle.m_repetitions = WORD(max_pulses / generalized_data_block.npp);
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



/// Write a pause
void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const PausePulser &p_pulser, std::ostream& p_stream) const
{
    auto edge = p_pulser.GetEdgeAfterWait();
    auto duration_in_tstates = p_pulser.GetDurationInTStates();
    if(duration_in_tstates > 0xffff && edge == Edge::no_change)
    {
        auto len_in_ms = WORD((1000 * duration_in_tstates * m_tstate_dur).count());
        std::cout << "Pause: PauseOrStopthetapecommand " << len_in_ms << "ms; (TStates=" << duration_in_tstates << ")" << std::endl;
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
        std::cout << "Pause: GeneralizedDataBlock TStates=" << duration_in_tstates << " Egde= " << edge << std::endl;
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
            WriteBinary(p_stream, WORD(duration_in_tstates));

            DebugLog(" | Symdef#2: ");
            // 2nd symdef for edge
            symdef.m_edge = edge;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(0));


            for (int totp = 0; totp < generalized_data_block.totp; totp++)
            {
                DebugLog((" | Prle#" + std::to_string(totp) + ": ").c_str());
                GeneralizedDataBlock::Prle prle {};
                prle.m_symbol = totp;
                prle.m_repetitions = 1;
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
    }
    DebugLog(" [End Pause: GeneralizedDataBlock]\n");

}


/// Write a generalizedDataBlock with *just* data.
void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const DataPulser &p_pulser, std::ostream& p_stream) const
{
    const auto &one_pattern = p_pulser.GetOnePattern();
    const auto &zero_pattern = p_pulser.GetZeroPattern();
    const auto total_size = p_pulser.GetTotalSize();        // # bytes

    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = 0;        // no pilot/sync block here (just data)
    generalized_data_block.npp          = 0;        // not used when totp is 0
    generalized_data_block.asp          = 1;        // not used when totp is 0, but 0 is 256.

    generalized_data_block.totd         = DWORD(total_size * 8);        // so # bits
    generalized_data_block.npd          = BYTE(std::max(zero_pattern.size(), one_pattern.size()));  // eg 2 for standard, 1 for turbo
    generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(total_size));
    std::cout << "Data: GeneralizedDataBlock blocklen=" << generalized_data_block.GetBlockLength(total_size) << " #bytes=" << total_size << " totd=" << generalized_data_block.totd << " m_max_pulses=" << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
    auto pos_start = p_stream.tellp();
#endif
    WriteBinary(p_stream, generalized_data_block);
        
    if(generalized_data_block.totp > 0)      // Note: is 0. (no leader/sync))
    {
        // no pulses, so no SYMDEF[ASP]/PRLE[TOTP]
    }

    for(int asd = 0; asd < generalized_data_block.asd; asd++)     // should be 2. Pattern for 0 and 1
    {
        DebugLog(" | SymDef: ");
        GeneralizedDataBlock::SymDef symdef{};
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        const auto & pattern = (asd == 0) ? zero_pattern : one_pattern;
        for (int npd = 0; npd < generalized_data_block.npd; npd++)        // should be 1 for turbo blocks; 2 for standard speed.
        {
            WriteBinary(p_stream, (npd < pattern.size()) ? WORD(pattern[npd]) : WORD(0));
        }
    }

    DebugLog(" | Data: ");
    for(int i = 0; i < total_size; i++)
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

void TzxWriter::WriteAsStandardSpectrum(std::ostream& p_stream, const TonePulser &p_pilot, const TonePulser &p_sync, const DataPulser &p_data)
{
    (void)p_pilot;
    (void)p_sync;
    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::StandardSpeedDataBlock);
    WriteBinary<WORD>(p_stream, 1000);     // pause after this block
    auto len = p_data.GetTotalSize();
    WriteBinary<WORD>(p_stream, len);
    std::cout << "StandardSpeedDataBlock, data length = " << len << std::endl;

    for(int i = 0; i < len; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
    }

}

// Write as ID 11 - Turbo Speed Data Block
void TzxWriter::WriteAsTurboData(std::ostream& p_stream, const TonePulser &p_pilot, const TonePulser &p_sync, const DataPulser &p_data)
{
    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::TurboSpeedDataBlock);
    TurboSpeedDataBlock block{};
    block.length_of_pilot_pulse = p_pilot.GetPattern()[0];
    block.length_of_sync_first_pulse = p_sync.GetPattern()[0];
    block.length_of_sync_second_pulse = p_sync.GetPattern()[1];
    block.length_of_zero_bit_pulse = p_data.GetZeroPattern()[0];
    block.length_of_one_bit_pulse = p_data.GetOnePattern()[0];
    block.length_of_pilot_tone = p_pilot.GetMaxPulses();
    block.used_bits_last_byte = 8;
    block.pause = 0;
    auto len = p_data.GetTotalSize();
    block.length[0] = len & 0xff;           // bit of weird 24 bit length
    block.length[1] = len & 0xff00 >> 8;
    block.length[2] = len & 0xff0000 >> 16;
    WriteBinary<TurboSpeedDataBlock>(p_stream, block);
    std::cout << "TurboSpeedDataBlock, data length = " << len << std::endl;
    for(int i = 0; i < len; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
    }
}



void TzxWriter::WriteAsZqLoaderTurboData(std::ostream &p_stream, const TonePulser *p_leader, const TonePulser &p_sync, const DataPulser &p_data)
{
    const auto &one_pattern = p_data.GetOnePattern();
    const auto &zero_pattern = p_data.GetZeroPattern();
    const auto total_size = p_data.GetTotalSize();        // # bytes

    const TonePulser *leader = p_leader ? IsPulserLeader(*p_leader) ? p_leader : nullptr : nullptr;

    WriteBinary<TzxBlockType>(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = leader ? 2 : 1;        // a pilot + sync OR just a sync (then its the minisync)
    generalized_data_block.npp          = std::max(leader ? leader->GetPattern().size() : 0, p_sync.GetPattern().size()) ;
    generalized_data_block.asp          = leader ? 2 : 1;

    generalized_data_block.totd         = DWORD(total_size * 8);        // so # bits
    generalized_data_block.npd          = BYTE(std::max(zero_pattern.size(), one_pattern.size()));  // should be 1 for zqloader turbo blocks
    generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(total_size));
    std::cout << "Data: GeneralizedDataBlock blocklen=" << generalized_data_block.GetBlockLength(total_size) << " #bytes=" << total_size << " totd=" << generalized_data_block.totd << " m_max_pulses=" << " block_length=" << generalized_data_block.block_length << std::endl;
#ifdef DEBUG_TZX
    auto pos_start = p_stream.tellp();
#endif
    WriteBinary(p_stream, generalized_data_block);

   if(generalized_data_block.totp > 0)
   {
        for(int asp = 0; asp < generalized_data_block.asp; asp++)
        {
            DebugLog(" | Symdef: ");
            GeneralizedDataBlock::SymDef symdef{};
            symdef.m_edge = Edge::toggle;
            WriteBinary(p_stream, symdef);
            for(int npp = 0; npp < generalized_data_block.npp; npp++)
            {
                const auto pattern = (asp == 0 && leader) ? leader->GetPattern() : p_sync.GetPattern();
                WriteBinary(p_stream, (npp < pattern.size()) ? WORD(pattern[npp]) : WORD(0));
            }
        }
        for (int totp = 0; totp < generalized_data_block.totp; totp++)
        {
            GeneralizedDataBlock::Prle prle {};
            prle.m_symbol = totp;
            prle.m_repetitions = (totp == 0 && leader) ? leader->GetMaxPulses() / leader->GetPattern().size()
                                 : 1;
            WriteBinary(p_stream, prle);
        }
    }


    for(int asd = 0; asd < generalized_data_block.asd; asd++)     // should be 2. Pattern for '0' and '1'
    {
        DebugLog(" | SymDef: ");
        GeneralizedDataBlock::SymDef symdef{};
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        const auto & pattern = (asd == 0) ? zero_pattern : one_pattern;
        for (int npd = 0; npd < generalized_data_block.npd; npd++)        // should be 1 for turbo blocks; 2 for standard speed.
        {
            WriteBinary(p_stream, (npd < pattern.size()) ? WORD(pattern[npd]) : WORD(0));
        }
    }

    DebugLog(" | Data: ");
    for(int i = 0; i < total_size; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
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
