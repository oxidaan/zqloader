// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.cpp
// DESCRIPTION:     Implementation of class TzxLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

// #define DEBUG_TZX
#include "tzxwriter.h"
#include "tzx_types.h"
#include "loadbinary.h"     // WriteBinary
#include <algorithm>
#include "spectrum_consts.h"
#include <string>
#include <sstream>


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


// Is given Pulser a 'normal' turbo data block? With 2 same edges for each zero/one.
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
    std::cout << "ZXTape!" << std::endl;
    WriteBinary(p_stream, std::byte{ 0x1A });
    WriteBinary(p_stream, std::byte{ 1 });
    WriteBinary(p_stream, std::byte{ 20 });
    WriteInfo(p_stream);
    const Pulser *prevprev{};
    const Pulser *prev{};
    const Pulser *current{};
    TzxBlockWriter writer(p_stream, p_tstate_dur);
    int zqblocks = 0;       // logging only
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
            if(prevprev && IsPulserLeader(*prevprev))
            {
                zqblocks++;
                std::cout << "Block # " << zqblocks << " (ZQLoader header) ";
            }
            else
            {
                std::cout << "Block # " << zqblocks << " (ZQLoader data) ";
            }
            WriteAsZqLoaderTurboData(p_stream, dynamic_cast<const TonePulser *>(prevprev), *dynamic_cast<const TonePulser *>(prev), *dynamic_cast<const DataPulser *>(current));

            prev = nullptr;
            current = nullptr;
        }
        else if(prevprev)
        {
            prevprev->Accept(writer);       // -> WriteAsTzxBlock (visitor pattern)
        }

        prevprev = prev;
        prev = current;
    }
    // Write remaining last two blocks when not already done so.
    if(prevprev)
    {
        prevprev->Accept(writer);        // -> WriteAsTzxBlock
    }
    if(prev)
    {
        prev->Accept(writer);            // -> WriteAsTzxBlock
    }
}

void TzxWriter::WriteInfo(std::ostream& p_stream)
{
    WriteBinary(p_stream, TzxBlockType::Textdescription);
    extern const char *GetVersion();
    std::string s = std::string("File written by ZQLoader version ") + GetVersion();
    WriteBinary(p_stream, BYTE(s.length()));
    for(const auto c: s)
    {
        WriteBinary(p_stream, c);
    }
}

void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const TonePulser &p_pulser, std::ostream &p_stream) const
{
    const auto &pattern = p_pulser.GetPattern();
    auto max_pulses = p_pulser.GetMaxPulses();
    if(pattern.size() == 1)
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << max_pulses << " Dur=" << ((max_pulses * pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary(p_stream, WORD(pattern[0]));
        WriteBinary(p_stream, WORD(max_pulses));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else if(pattern.size() == 2 && pattern[0] == pattern[1])
    {
        std::cout << "Tone: Puretone patt=" << pattern[0] << " #pulses=" << 2 * max_pulses << " Dur=" << ((2 * max_pulses * pattern[0]) * m_tstate_dur).count() << "sec" << std::endl;
        WriteBinary(p_stream, TzxBlockType::Puretone);
        std::cout << "  ";
        WriteBinary(p_stream, WORD(pattern[0]));
        WriteBinary(p_stream, WORD(2 * max_pulses ));
        DebugLog(" [End Tone: Puretone]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock (instead of Puretone) because pulses have different length
        WriteBinary(p_stream, TzxBlockType::GeneralizedDataBlock);
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
                prle.m_symbol      = BYTE(totp); // the one and only symbol
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
/// Long pauses are written as PauseOrStopthetapecommand
/// Short ones as (empty) GeneralizedDataBlock, can also have edge at end.
void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const PausePulser &p_pulser, std::ostream& p_stream) const
{
    auto edge = p_pulser.GetEdgeAfterWait();
    auto duration_in_tstates = p_pulser.GetDurationInTStates();
    auto len_in_ms = WORD((1000.0 * duration_in_tstates * m_tstate_dur).count());
    if(duration_in_tstates > 35000 /* 0xffff */ && edge == Edge::no_change) // 10ms
    {
        std::cout << "Pause: PauseOrStopthetapecommand " << len_in_ms << "ms; (TStates=" << duration_in_tstates << ")" << std::endl;
        WriteBinary(p_stream, TzxBlockType::PauseOrStopthetapecommand);
        WriteBinary(p_stream, len_in_ms);
        DebugLog(" [End Pause]\n");
    }
    else
    {
        // need to write as GeneralizedDataBlock because PauseOrStopthetapecommand is not accurate
        // enough and might need edge at end of pause
        WriteBinary(p_stream, TzxBlockType::GeneralizedDataBlock);
        GeneralizedDataBlock generalized_data_block{};
        generalized_data_block.pause        = 0;
        generalized_data_block.totp         = 2;    // pause, then the edge
        generalized_data_block.npp          = 1;
        generalized_data_block.asp          = 2;    // two symbols one for pause one for edge

        generalized_data_block.totd         = 0;    // no data
        generalized_data_block.npd          = 0;    // should not be used when totd is 0
        generalized_data_block.asd          = 2;    // should not be used when totd is 0 But '0 = 256'. Must be power of 2.
        generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(0));
        std::cout << "Pause: GeneralizedDataBlock " << len_in_ms << "ms; (TStates=" << duration_in_tstates << ") Egde= " << edge << std::endl;
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
            // 2nd symdef for just an edge
            symdef.m_edge = edge;
            WriteBinary(p_stream, symdef);
            WriteBinary(p_stream, WORD(0));     // no waits here


            // write pause (1x) - sync(1x) sequence.
            for (unsigned totp = 0; totp < generalized_data_block.totp; totp++)     // is 2
            {
                DebugLog((" | Prle#" + std::to_string(totp) + ": ").c_str());
                GeneralizedDataBlock::Prle prle {};
                prle.m_symbol = BYTE(totp);
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
        DebugLog(" [End Pause: GeneralizedDataBlock]\n");
    }

}


/// Write a generalizedDataBlock with *just* data.
/// No pilot/sync.
void TzxWriter::TzxBlockWriter::WriteAsTzxBlock(const DataPulser &p_pulser, std::ostream& p_stream) const
{
    const auto &one_pattern = p_pulser.GetOnePattern();
    const auto &zero_pattern = p_pulser.GetZeroPattern();
    const auto total_size = p_pulser.GetTotalSize();        // # bytes

    WriteBinary(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = 0;        // no pilot/sync block here (just data)
    generalized_data_block.npp          = 0;        // not used when totp is 0
    generalized_data_block.asp          = 1;        // not used when totp is 0, but 0 is 256.

    generalized_data_block.totd         = DWORD(total_size * 8);        // so # bits
    generalized_data_block.npd          = BYTE(std::max(zero_pattern.size(), one_pattern.size()));  // eg 2 for standard, 1 for turbo
    generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1

    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(total_size));
    std::cout << "Data only: GeneralizedDataBlock: #bytes: " << total_size << "  " << generalized_data_block << std::endl;
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
        DebugLog((" | SymDef:#" + std::to_string(asd )).c_str());
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
    // The actual data follows here
    for(int i = 0; i < total_size; i++)
    {
        WriteBinary(p_stream, p_pulser.GetByte(i));
    }
#ifdef DEBUG_TZX
    auto pos_end = p_stream.tellp();
    if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(total_size) +  sizeof(generalized_data_block.block_length))
    {
        std::stringstream ss;
        ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
        throw std::runtime_error(ss.str());
    }
#endif
    DebugLog(" [End Data: GeneralizedDataBlock]\n");
    
}

// Write as ID 0x10 - StandardSpeedDataBlock.
// Has predefined pilot and sync so these parametersare not used.
void TzxWriter::WriteAsStandardSpectrum(std::ostream& p_stream, const TonePulser &p_pilot, const TonePulser &p_sync, const DataPulser &p_data)
{
    (void)p_pilot;
    (void)p_sync;
    auto len = p_data.GetTotalSize();
    std::cout << "StandardSpeedDataBlock, data length = " << len << std::endl;
    WriteBinary(p_stream, TzxBlockType::StandardSpeedDataBlock);
    WriteBinary(p_stream, WORD(1000));     // pause after this block
    WriteBinary(p_stream, WORD(len));

    for(int i = 0; i < len; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
    }

}

// Write as ID 0x11 - Turbo Speed Data Block
void TzxWriter::WriteAsTurboData(std::ostream& p_stream, const TonePulser &p_pilot, const TonePulser &p_sync, const DataPulser &p_data)
{
    auto len = p_data.GetTotalSize();
    WriteBinary(p_stream, TzxBlockType::TurboSpeedDataBlock);
    TurboSpeedDataBlock block{};
    block.length_of_pilot_pulse = WORD(p_pilot.GetPattern()[0]);
    block.length_of_sync_first_pulse = WORD(p_sync.GetPattern()[0]);
    block.length_of_sync_second_pulse = WORD(p_sync.GetPattern()[1]);
    block.length_of_zero_bit_pulse = WORD(p_data.GetZeroPattern()[0]);
    block.length_of_one_bit_pulse = WORD(p_data.GetOnePattern()[0]);
    block.length_of_pilot_tone = WORD(p_pilot.GetMaxPulses());
    block.used_bits_last_byte = 8;
    block.pause = 0;
    block.length[0] = len & 0xff;           // bit of weird 24 bit length
    block.length[1] = BYTE((len & 0xff00) >> 8);
    block.length[2] = BYTE((len & 0xff0000) >> 16);
    std::cout << "TurboSpeedDataBlock: Pilot Length = " << (1000.0 * block.length_of_pilot_tone * block.length_of_pilot_pulse * spectrum::tstate_dur.count()) << "  " <<  block << std::endl;
    WriteBinary(p_stream, block);
    for(int i = 0; i < len; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
    }
    DebugLog("End TurboSpeedDataBlock\n");
}


/// Write as ID 0x19 - GeneralizedDataBlock
/// A ZQ Loader turbo block has:
/// *optionally* a pilot tone (before header only)
/// a sync; (can also be minisync)
/// then data with one edge per bit.
/// p_leader can be nullptr when there is no pilot tone.
void TzxWriter::WriteAsZqLoaderTurboData(std::ostream &p_stream, const TonePulser *p_leader, const TonePulser &p_sync, const DataPulser &p_data)
{
    const auto &one_pattern = p_data.GetOnePattern();
    const auto &zero_pattern = p_data.GetZeroPattern();
    const auto len = p_data.GetTotalSize();        // # bytes
    auto delay_after_byte = p_data.GetExtraDelayAfterByte();

    const TonePulser *leader = p_leader ? IsPulserLeader(*p_leader) ? p_leader : nullptr : nullptr;

    WriteBinary(p_stream, TzxBlockType::GeneralizedDataBlock);

    GeneralizedDataBlock generalized_data_block{};
    generalized_data_block.pause        = 0;

    generalized_data_block.totp         = leader ? 2 : 1;        // a pilot + sync OR just a sync (then its the minisync)
    generalized_data_block.npp          = BYTE(std::max(leader ? leader->GetPattern().size() : 0, p_sync.GetPattern().size()));
    generalized_data_block.asp          = leader ? 2 : 1;

    if(!delay_after_byte)
    {
        generalized_data_block.totd         = DWORD(len * 8);        // so # bits
        generalized_data_block.npd          = BYTE(std::max(zero_pattern.size(), one_pattern.size()));  // should be 1 for zqloader turbo blocks
        generalized_data_block.asd          = 2; // we have a pattern for 0 and for 1
    }
    else
    {
        generalized_data_block.totd         = DWORD(len);        // so # bytes
        generalized_data_block.npd          = BYTE(std::max(8* zero_pattern.size(), 8* one_pattern.size()));  // should be 8
        generalized_data_block.asd          = 0; // 0=256! we have a pattern for 256 byte values
    }
    generalized_data_block.block_length = DWORD(generalized_data_block.GetBlockLength(len));
    std::cout << "GeneralizedDataBlock: #bytes: " << len << "  " << generalized_data_block << std::endl;
#ifdef DEBUG_TZX
    auto pos_start = p_stream.tellp();
#endif
    WriteBinary(p_stream, generalized_data_block);

   if(generalized_data_block.totp > 0)      // 2 (sync + pilot)  or 1 (minisync)
   {
        for(int asp = 0; asp < generalized_data_block.asp; asp++)       // also 2 or 1
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
        for (unsigned totp = 0; totp < generalized_data_block.totp; totp++)     // 2 or 1
        {
            GeneralizedDataBlock::Prle prle {};
            prle.m_symbol = BYTE(totp);
            prle.m_repetitions = WORD((totp == 0 && leader) ?
                                        leader->GetMaxPulses() / leader->GetPattern().size()
                                        : 1);
            WriteBinary(p_stream, prle);
        }
    }
    // "0=256"
    auto maxasd = generalized_data_block.asd == 0 ? 256 : generalized_data_block.asd;
    for(int asd = 0; asd < maxasd; asd++)  // should be 2. Pattern for '0' and '1' 
    {
        DebugLog(" | SymDef: ");
        GeneralizedDataBlock::SymDef symdef{};
        symdef.m_edge = Edge::toggle;      // toggle
        WriteBinary(p_stream, symdef);
        if(!delay_after_byte)
        {
            const auto & pattern = (asd == 0) ? zero_pattern : one_pattern;
            for (int npd = 0; npd < generalized_data_block.npd; npd++)        // should be 1 for our turbo blocks
            {
                WriteBinary(p_stream, (npd < pattern.size()) ? WORD(pattern[npd]) : WORD(0));
            }
        }
        else
        {
            for (int npd = 0; npd < generalized_data_block.npd ; npd++)        // should be = 8
            {
                bool bit = (asd >> (7 - npd)) & 0x1;
                if(bit)
                {
                    WriteBinary(p_stream, WORD(one_pattern[0] +  (npd == 7 ? delay_after_byte : 0)) );
                }
                else
                {
                    WriteBinary(p_stream, WORD(zero_pattern[0] + (npd == 7 ? delay_after_byte : 0)) );
                }
            }
        }
    }
    DebugLog(" | Data: ");
    for(int i = 0; i < len; i++)
    {
        WriteBinary(p_stream, p_data.GetByte(i));
    }
#ifdef DEBUG_TZX
    auto pos_end = p_stream.tellp();
    if(unsigned(pos_end - pos_start) != generalized_data_block.GetBlockLength(len) +  sizeof(generalized_data_block.block_length))
    {
        std::stringstream ss;
        ss << "TZX issues detected:" << pos_end << ' ' << pos_start << ' ' << pos_end - pos_start << ' ' << generalized_data_block.GetBlockLength(0) << ' ' << sizeof(generalized_data_block.block_length) << std::endl;
        throw std::runtime_error(ss.str());
    }
#endif
    DebugLog(" [End Data: GeneralizedDataBlock]\n");
}
