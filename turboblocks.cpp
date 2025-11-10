// ==============================================================================
// PROJECT:         zqloader
// FILE:            turboblock.cpp
// DESCRIPTION:     Implementation of class TurboBlocks
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================


#include "turboblock.h"
#include "turboblocks.h"
#include "taploader.h"
#include "memoryblock.h"
#include "loader_defaults.h"
#include "spectrum_consts.h"        // SCREEN_END
#include "spectrum_types.h"         // ZxBlockType
#include "symbols.h"
#include <filesystem>
#include "spectrum_loader.h"        // CalculateChecksum
#include <bitset>
#include <iostream>


#define DONT_USE(somevar)   \
    (void) somevar; \
    class{} somevar;    \
    (void) somevar;
#ifdef _MSC_VER
#pragma warning(disable: 4458)
#endif

namespace fs = std::filesystem;

using namespace std::placeholders;



#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#endif








class TurboBlocks::Impl
{
friend class TurboBlocks;


public:


    /// Load given file at normal speed, typically loads zqloader.tap.
    /// Add zqloader.tap file (normal speed).
    /// Patches zqloader.tap eg BIT_LOOP_MAX etc.
    /// Also loads symbol file (has same base name).
    void AddZqLoader(const std::filesystem::path& p_filename)
    {
        std::cout << "Processing zqloader file: " << p_filename << " (normal speed)" << std::endl;

        fs::path filename_exp = p_filename;
        filename_exp.replace_extension("exp");          // zqloader.exp (symbols)
        SetSymbolFilename(filename_exp);

        TapLoader loader;
        loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string)
            {
                return HandleZqLoaderTapBlock(std::move(p_block));
            });
        loader.Load(p_filename, "");
    }

    ///  Is ZqLoader added (with AddZqLoader above)?
    bool IsZqLoaderAdded() const
    {
        return m_zqloader_code.size() != 0;
    }


    // Check and get m_loader_copy_start when not done so already.
    // Will be set when any block overlaps loader at basic.
    uint16_t GetLoaderCopyStart(const MemoryBlocks & p_memory_blocks) const
    {
        if (m_loader_copy_start == 0)
        {
            auto clear = m_symbols.GetSymbol("CLEAR");
            for (auto& block : p_memory_blocks)
            {
                if (Overlaps(block, spectrum::PROG, clear))
                {
                    auto loader_copy_start = spectrum::SCREEN_23RD;       // default when not set otherwise
                    std::cout << "Block overlaps loader at BASIC (=" << spectrum::PROG << ", " << clear <<
                        "). Will copy loader to screen at " <<
                        loader_copy_start + m_symbols.GetSymbol("STACK_SIZE") <<
                        " (loader will use block: start = " << loader_copy_start <<
                        " end = " << loader_copy_start + GetLoaderCodeLength(false) - 1 << ")" << std::endl;
                    return loader_copy_start;
                }
            }
        }
        return m_loader_copy_start;
    }

    // Check if any block overwrites our loader copied loader code (after copy)
    // cut it in two pieces before and after thus leaving space,
    // ignoring the middle part (must be screen or emtpy snapshot region)
    MemoryBlocks MakeSpaceForCopiedLoader(MemoryBlocks p_memory_blocks, uint16_t p_loader_copy_start)
    {
        MemoryBlocks new_blocks;
        uint16_t start = p_loader_copy_start;
        uint16_t end = p_loader_copy_start + GetLoaderCodeLength(false);
        for (auto& block : p_memory_blocks)
        {
            if (Overlaps(block, start, end))
            {
                std::cout << "Block overlaps the copied loader code. Will split in two." << std::endl;
                auto [first, second, third] = SplitBlock3(block, start, end);
                if (first.size() > 0)
                {
                    new_blocks.push_back(std::move(first));
                }
                // ignore 2nd block. Must be screen or emtpy snapshot region. Loader occupy this area!
                if (third.size() > 0)
                {
                    new_blocks.push_back(std::move(third));
                }

            }
            else
            {
                new_blocks.push_back(std::move(block));
            }
        }
        return new_blocks;
    }

    // Check if any block overwrites our (always copied) loader code at upper regions.
    // Check if a block overwrites our loader copied loader code (at upper) (after copy)
    // cut it in three pieces before and after thus leaving space
     MemoryBlocks MakeSpaceForUpperLoader( MemoryBlocks p_memory_blocks)
     {
        MemoryBlock overwrites_loader;
        MemoryBlocks new_blocks;
        int start = m_symbols.GetSymbol("ASM_UPPER_START");
        int end = start + m_symbols.GetSymbol("ASM_UPPER_LEN");
        for (auto& block : p_memory_blocks)
        {
            if (Overlaps(block, start, end))
            {
                if (overwrites_loader.size() != 0)
                {
                    // Note: Should not be possible because of Compact().
                    throw std::runtime_error("Already found a block loading to loader-overlapped region, blocks overlap");
                }
                std::cout << "Block overlaps loader at upper memory region. Adding extra block..." << std::endl;
                auto [first, second, third] = SplitBlock3(block, start, end);
                if (first.size() > 0)
                {
                    new_blocks.push_back(std::move(first));
                }
                overwrites_loader = std::move(second);      // can not have size zero, also checked below again.
                overwrites_loader.m_address = start;
                if (third.size() > 0)
                {
                    new_blocks.push_back(std::move(third));
                }
            }
            else
            {
                new_blocks.push_back(std::move(block));
            }
        }
        new_blocks.push_back(std::move(overwrites_loader));     // so last
        return new_blocks;
    }
    

    /// p_usr_address: when done loading all blocks end start machine code here as in RANDOMIZE USR xxxx
    /// p_clear_address: when done loading put stack pointer here, which is a bit like CLEAR xxxx
    /// To be called after last block was added.
    /// Make sure:
    ///     to add upperblock (overwriting loader) as last in the chain.
    ///     patch loader code.
    ///     first block will copy loader to alternative location.
    /// To be called after adding last AddDataBlock, before MoveToLoader.
    /// p_usr_address: when done loading all blocks end start machine code here as in RANDOMIZE USR xxxxx
    ///     (When 0 return to BASIC)
    /// p_clear_address: when done loading put stack pointer here, which is a bit like CLEAR xxxxx
    void Finalize(uint16_t p_usr_address, uint16_t p_clear_address, int p_last_bank_to_set)
    {
        if (m_memory_blocks.size() == 0)
        {
            throw std::runtime_error("No Memory block added. Nothing to do!");
        }

        // Combine adjacent blocks, combine overlapping blocks; order from high to low.
        auto memory_blocks = Compact(std::move(m_memory_blocks));
        DONT_USE(m_memory_blocks);
        {
            const MemoryBlock& first = memory_blocks.front();
            // First block includes screen but is bigger than that.
            // For visually appealing split of screen to load screen first.
            // Also to add move loader command at end of this first block (later)
            if (first.GetStartAddress() == spectrum::SCREEN_START && first.size() > spectrum::SCREEN_SIZE)
            {
                auto [screen, after] = SplitBlock(first, spectrum::SCREEN_START + spectrum::SCREEN_SIZE);
                memory_blocks.pop_front();          // kick of first and
                memory_blocks.push_front(std::move(after));    // replace with these two;
                memory_blocks.push_front(std::move(screen));   // with screen first
            }
        }

        // Check and set m_loader_copy_start when not done so already.
        // Will be set when any block overlaps loader at basic.
        auto loader_copy_start = GetLoaderCopyStart(memory_blocks);
        DONT_USE(m_loader_copy_start);


        // Make space for new loader location
        // skip when at screen 2/3rd - not need then and is actually slower.
        if (loader_copy_start && loader_copy_start != spectrum::SCREEN_23RD )
        {
            memory_blocks = MakeSpaceForCopiedLoader(std::move(memory_blocks), loader_copy_start);
        }

        memory_blocks = MakeSpaceForUpperLoader(std::move(memory_blocks));



        MemoryBlocksToTurboBlocks(std::move(memory_blocks), loader_copy_start, p_usr_address, p_clear_address, p_last_bank_to_set);



        if (loader_copy_start && !IsZqLoaderAdded())
        {
            std::cout << "ZQLoader already (pre) loaded or not present, cannot patch loader copy code, will use screen to move loader to." << std::endl;
        }
        if (loader_copy_start && IsZqLoaderAdded())
        {
            // loader (control code) will be copied to screen for example
            // patch zqloader itself
            if (p_usr_address == 0)
            {
                throw std::runtime_error("Loader will be moved, so will stack, but at end returns to BASIC, will almost certainly crash");
            }
            uint16_t copy_me_target_location = loader_copy_start + m_symbols.GetSymbol("STACK_SIZE");

            SetDataToZqLoaderTap("COPY_ME_SP", copy_me_target_location);        // before new copied block
            auto copy_me_source_location = m_symbols.GetSymbol("ASM_CONTROL_CODE_START");
            auto copy_me_length = m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN");
            bool overlaps = Overlaps(copy_me_source_location, copy_me_source_location + copy_me_length, copy_me_target_location, copy_me_target_location + copy_me_length);
            bool copy_lddr = copy_me_target_location > copy_me_source_location;
            if (!copy_lddr)
            {
                // when false: use LDIR, copies from low to high
                // when dest address is before source when overlapping
                //   -> xxxxxxxx ->
                //  xxxxxxxx         LDIR copy_lddr==false
                std::cout << "Copy loader backwards (but LDIR working forwards) from " << copy_me_source_location << " to " << copy_me_target_location << " length=" << copy_me_length << " last = " << copy_me_target_location + copy_me_length - 1 << (overlaps ? " (overlaps)" : "") << std::endl;
                SetDataToZqLoaderTap("COPY_ME_DEST", copy_me_target_location);
                SetDataToZqLoaderTap("COPY_ME_SOURCE_OFFSET", copy_me_source_location);
                SetDataToZqLoaderTap("COPY_ME_LDDR_OR_LDIR", 0xb0ed); // LDIR! Endianness swapped
            }
            else
            {
                // when true: use LDDR, copies from high to low
                // when dest address is after source when overlapping
                //   <- xxxxxxxx <-
                //          xxxxxxxx LDDR copy_lddr==true
                std::cout << "Copy loader forwards (but LDDR working backwards) from " << copy_me_source_location << " to " << copy_me_target_location << " length=" << copy_me_length << " last = " << copy_me_target_location + copy_me_length - 1 << (overlaps ? " (overlaps)" : "") << std::endl;
                SetDataToZqLoaderTap("COPY_ME_DEST", copy_me_target_location + copy_me_length - 1);
                SetDataToZqLoaderTap("COPY_ME_SOURCE_OFFSET", copy_me_source_location + copy_me_length - 1);
                SetDataToZqLoaderTap("COPY_ME_LDDR_OR_LDIR", 0xb8ed); // LDDR! Endianness swapped
            }

            SetDataToZqLoaderTap("COPY_ME_END_JUMP", copy_me_target_location);
            std::cout << "\n";

        }



    }

    /// Move all added turboblocks to SpectrumLoader as given at CTOR.
    /// Call after Finalize.
    /// to given SpectrumLoader.
    /// no-op when there are no blocks.
    void MoveToLoader(SpectrumLoader& p_spectrumloader, bool p_is_fun_attribute)
    {
        std::chrono::milliseconds pause_before = 0ms;
        if (IsZqLoaderAdded())        // else probably already preloaded
        {
            p_spectrumloader.AddLeaderPlusData(std::move(m_zqloader_header), spectrum::tstate_quick_zero, 1750ms);
            p_spectrumloader.AddLeaderPlusData(std::move(m_zqloader_code), spectrum::tstate_quick_zero, 1500ms);
            p_spectrumloader.AddPause(100ms); // time needed to start our loader after loading itself (basic!)
        }

        int cnt = 1;
        for (auto& tblock : m_turbo_blocks)
        {
            auto next_pause = tblock.EstimateHowLongSpectrumWillTakeToDecompress(m_decompression_speed); // b4 because moved
            if (!p_is_fun_attribute)
            {
                std::cout << "Block #" << cnt++ << std::endl;
                if (pause_before > 0ms)
                {
                    std::cout << "Pause before = " << pause_before.count() << "ms" << std::endl;
                }
                tblock.DebugDump();
            }
            std::move(tblock).MoveToLoader(p_spectrumloader, pause_before, m_zero_duration, m_one_duration, m_end_of_byte_delay);
            pause_before = next_pause;
        }
        m_turbo_blocks.clear();
    }


    /// Set durations in T states for zero and one pulses.
    /// When 0 keep defaults.
    /// Set durations in T states for zero and one pulses.
    /// When 0 keep defaults.
    void SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
    {
        if (p_zero_duration != 0)
        {
            m_zero_duration = p_zero_duration;
        }
        if (p_one_duration != 0)
        {
            m_one_duration = p_one_duration;
        }
        if (p_end_of_byte_delay != 0)
        {
            m_end_of_byte_delay = p_end_of_byte_delay;
        }
        std::cout << "Around " << (1000ms / spectrum::tstate_dur) / ((m_zero_duration + m_one_duration) / 2) << " bps" << std::endl;
    }




    /// Add just a header with a 'copy to screen' command (no data)
    /// Mainly for debugging!
    void CopyLoaderToScreen(uint16_t p_value)
    {
        m_loader_copy_start = p_value;
        TurboBlock empty;
        m_turbo_blocks.push_back(std::move(empty));
    }







    // Length needed when loader code needs to be moved away from BASIC location
    uint16_t GetLoaderCodeLength(bool p_with_registers) const
    {
        return m_symbols.GetSymbol("STACK_SIZE") +                                 // some space for stack
            m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN") +                       // our code eg decompressor
            m_symbols.GetSymbol("ASM_UPPER_LEN") +                              // needed to reserve space when overwriting data at upper block
            (p_with_registers ? m_symbols.GetSymbol("REGISTER_CODE_LEN") : 0u); // only needed when loading z80 snapshot
    }
   
    /// Take an export file name that will be used to load symbols.
    void SetSymbolFilename(const std::filesystem::path& p_symbol_file_name)
    {
        m_symbols.ReadSymbols(p_symbol_file_name);
        if (m_symbols.GetSymbol("HEADER_LEN") != TurboBlock::GetHeaderSize())
        {
            throw std::runtime_error("TurboBlock::Header length mismatch with zqloader.z80asm");
        }
        std::cout << "Z80 loader total length = " << m_symbols.GetSymbol("TOTAL_LEN") << "\n";

    }
private:



    // Handle the tap block for zqloader.tap - so our loader itself.
    // Called when reading tap file.
    // patch certain parameters, then move to spectrumloader (normal speed)
    bool HandleZqLoaderTapBlock(DataBlock p_block)
    {
        using namespace spectrum;
        TapeBlockType type = TapeBlockType(p_block[0]);
        if (type == TapeBlockType::header)
        {
            // can here (like otla) change the name as zx spectrum sees it
            m_zqloader_header = std::move(p_block);
        }
        else if (type == TapeBlockType::data)
        {
            m_zqloader_code = std::move(p_block);
            SetDataToZqLoaderTap("BIT_LOOP_MAX", std::byte(m_bit_loop_max));
            SetDataToZqLoaderTap("BIT_ONE_THESHLD", std::byte(m_bit_loop_max - m_zero_max));
            SetDataToZqLoaderTap("IO_INIT_VALUE", std::byte(m_io_init_value));
            SetDataToZqLoaderTap("IO_XOR_VALUE", std::byte(m_io_xor_value));
        }

        // m_spectrumloader.AddLeaderPlusData(std::move(p_block), g_tstate_quick_zero, 1750ms);
        return false;
    }

    // Get corrected address at datablock as read from zqloader.tap
    uint16_t GetZqLoaderSymbolAddress(const char* p_name) const
    {
        uint16_t adr = 1 + m_symbols.GetSymbol(p_name); // + 1 because of start byte
        uint16_t asm_upper_start = m_symbols.GetSymbol("ASM_UPPER_START");
        if (adr >= asm_upper_start)
        {
            // correct when (symbol value) is in the upper regions
            // so after DISP (eg BIT_LOOP_MAX,BIT_ONE_THESHLD)
            // because at the loaded block containing zqloader the address is lower.
            adr -= asm_upper_start;
            uint16_t asm_upper_start_offset = m_symbols.GetSymbol("ASM_UPPER_START_OFFSET");
            adr += asm_upper_start_offset;
        }
        adr -= m_symbols.GetSymbol("TOTAL_START");
        return adr;
    }


    // Convenience
    // Set a 8 bit byte or 16 bit word to the TAP block that contains ZQLoader Z80. To set parameters to ZQLoader.
    // p_block: block to modify data to, should be zqloader.tap.
    // p_name: symbol name as exported by sjasmplus, see Symbols.
    // p_value: new byte value.
    /// Convenience
    /// Set a 8 bit byte or 16 bit word to the TAP block that contains ZQLoader Z80. To set parameters to ZQLoader.
    /// p_block: block to modify data to, should be zqloader.tap.
    /// p_name: symbol name as exported by sjasmplus, see Symbols.
    /// p_value: new byte value.
    void SetDataToZqLoaderTap(const char* p_name, uint16_t p_value)
    {
        uint16_t adr = GetZqLoaderSymbolAddress(p_name);
        m_zqloader_code[adr] = std::byte(p_value & 0xff);   // z80 is little endian
        m_zqloader_code[adr + 1] = std::byte((p_value >> 8) & 0xff);
        RecalculateChecksum(m_zqloader_code);
        std::cout << "Patching word '" << p_name << "' to: " << int(p_value) << " hex= " << std::hex << int(p_value) << std::dec << std::endl;
    }
    void SetDataToZqLoaderTap(const char* p_name, std::byte p_value)
    {
        uint16_t adr = GetZqLoaderSymbolAddress(p_name);
        m_zqloader_code[adr] = p_value;
        RecalculateChecksum(m_zqloader_code);
        std::cout << "Patching byte '" << p_name << "' to: " << int(p_value) << " hex= " << std::hex << int(p_value) << std::dec << " bin= " << std::bitset<8>(int(p_value)) << std::endl;
    }


    void MemoryBlocksToTurboBlocks(MemoryBlocks p_memory_blocks, uint16_t p_loader_copy_start, uint16_t p_usr_address, uint16_t p_clear_address, int p_last_bank_to_set)
    {
        TurboBlock *prev = nullptr;
        TurboBlock *prevprev = nullptr;
        int prev_bank_set = -1;
        for (auto& block : p_memory_blocks)
        {
            prevprev = prev;
            if(block.size())
            {
                if(prev && block.m_bank >= 0 && block.m_bank != prev_bank_set)
                {
                    prev->SwitchBankTo(block.m_bank);
                    prev_bank_set = block.m_bank;
                }
                if(&block == &p_memory_blocks.back())
                {
                    // This is the block that overwrites our loader at upper.
                    prev = &AddTurboBlock(std::move(block), p_loader_copy_start + m_symbols.GetSymbol("STACK_SIZE") + m_symbols.GetSymbol("ASM_CONTROL_CODE_LEN"));
                }        
                else
                {
                    prev = &AddTurboBlock(std::move(block));
                }
            }
        }
        if(prevprev && p_last_bank_to_set >=0 && p_last_bank_to_set != prev_bank_set)
        {
            prevprev->SwitchBankTo(p_last_bank_to_set);
            prevprev->DebugDump();
        }
        if (m_turbo_blocks.size() > 0)
        {
            // When indicated, after loading *first* block, loader will be copied. (eg to screen)
            // TODO goes wrong (but throws) when already has 'SetBank'
            if (p_loader_copy_start)
            {
                m_turbo_blocks.front().SetAfterBlockDo(TurboBlock::AfterBlock::CopyLoader);
            }
            // What should be done after last block
            if (p_usr_address)
            {
                m_turbo_blocks.back().SetUsrStartAddress(p_usr_address);
            }
            else
            {
                m_turbo_blocks.back().SetAfterBlockDo(TurboBlock::AfterBlock::ReturnToBasic);
            }
            m_turbo_blocks.back().SetClearAddress(p_clear_address);
        }
    }
    // Add given MemoryBlock as turbo block.
    // So convert to TurboBlock.
    // Add given datablock as turbo block at given start address
    // to end of list of turboblocks
    TurboBlock &AddTurboBlock(MemoryBlock&& p_block, uint16_t p_load_address = 0)
    {
        if (m_turbo_blocks.size() == 0)
        {
            // when first block overlaps loader *at basic*, add an empty block before it
            // will get SetCopyLoader (like CopyLoaderToScreen)
            // (note: this can not be a screen)
            if (Overlaps(p_block, spectrum::PROG, m_symbols.GetSymbol("CLEAR")))
            {
                TurboBlock empty;      
                m_turbo_blocks.push_back(std::move(empty));
            }
        }
        TurboBlock tblock;
        tblock.SetDestAddress(uint16_t(p_block.m_address));
        // tblock.SetBank(p_block.m_bank);
        if(p_load_address)
        {
            tblock.SetLoadAddress(p_load_address);
        }
        tblock.SetData(std::move(p_block.m_datablock), m_compression_type);
        m_turbo_blocks.push_back(std::move(tblock));
        return m_turbo_blocks.back();
    }


    static void RecalculateChecksum(DataBlock& p_block)
    {
        p_block[p_block.size() - 1] = 0_byte;       // Checksum, recalculate (needs to be zero b4 CalculateChecksum)
        p_block[p_block.size() - 1] = spectrum::CalculateChecksum(0_byte, p_block);
    }

private:

    DataBlock                     m_zqloader_header;               // standard zx header for zqloader
    DataBlock                     m_zqloader_code;                 // block with entire code for zqloader
    MemoryBlocks                  m_memory_blocks;
    std::list<TurboBlock>       m_turbo_blocks;                  // turbo blocks to load
    //std::unique_ptr<TurboBlock>   m_upper_block;                   // when a block is found that overlaps our loader (nullptr when not) must be loaded last
    uint16_t                      m_loader_copy_start = 0;         // start of free space were our loader can be copied to, begins with stack, then Control code copied from basic
    CompressionType               m_compression_type        = loader_defaults::compression_type;
    Symbols                       m_symbols;                       // named symbols as read from EXP file
    int                           m_zero_duration           = loader_defaults::zero_duration;
    int                           m_one_duration            = loader_defaults::one_duration;
    int                           m_end_of_byte_delay       = loader_defaults::end_of_byte_delay;
    int                           m_bit_loop_max            = loader_defaults::bit_loop_max;        // aka ONE_MAX, the wait for edge loop counter until timeout  
    int                           m_zero_max                = loader_defaults::zero_max;            // aka ZERO_MAX sees a 'one' when waited more than this number of cycli at wait for edge
    int                           m_io_init_value           = loader_defaults::io_init_value;        // aka ONE_MAX, the wait for edge loop counter until timeout  
    int                           m_io_xor_value            = loader_defaults::io_xor_value;            // aka ZERO_MAX sees a 'one' when waited more than this number of cycli at wait for edge
    int                           m_decompression_speed     = loader_defaults::decompression_speed;     // kb/second time spectrum needsto decompress before sending next block
}; // class TurboBlocks








TurboBlocks::TurboBlocks() :
    m_pimpl( new Impl() )
{}

TurboBlocks::~TurboBlocks() = default;


TurboBlocks::TurboBlocks(TurboBlocks &&) noexcept               = default;

TurboBlocks & TurboBlocks::operator = (TurboBlocks &&) noexcept = default;



TurboBlocks& TurboBlocks::AddZqLoader(const std::filesystem::path& p_filename)
{
    m_pimpl->AddZqLoader(p_filename);
    return *this;
}

bool TurboBlocks::IsZqLoaderAdded() const
{
    return m_pimpl->IsZqLoaderAdded();
}

size_t TurboBlocks::size() const
{
    return m_pimpl->m_turbo_blocks.size(); 
}

TurboBlocks& TurboBlocks::AddMemoryBlock(MemoryBlock p_block)
{
    m_pimpl->m_memory_blocks.push_back(std::move(p_block));
    return *this;
}

TurboBlocks &TurboBlocks::Finalize(uint16_t p_usr_address, uint16_t p_clear_address, int p_last_bank_to_set)
{
    m_pimpl->Finalize(p_usr_address, p_clear_address, p_last_bank_to_set);
    return *this;
}

TurboBlocks & TurboBlocks::MoveToLoader(SpectrumLoader& p_spectrumloader, bool p_is_fun_attribute)
{
    m_pimpl->MoveToLoader(p_spectrumloader, p_is_fun_attribute);
    return *this;
}

TurboBlocks& TurboBlocks::SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
{
    m_pimpl->SetDurations(p_zero_duration, p_one_duration, p_end_of_byte_delay);
    return *this;
}

TurboBlocks& TurboBlocks::SetBitLoopMax(int p_value)
{
    if(p_value)
    {
        m_pimpl->m_bit_loop_max = p_value;
    }
    return *this;
}

TurboBlocks& TurboBlocks::SetZeroMax(int p_value)
{
    if(p_value)
    {
        m_pimpl->m_zero_max = p_value;
    }
    return *this;
}

TurboBlocks& TurboBlocks::SetIoValues(int p_io_init_value, int p_io_xor_value)
{
    m_pimpl->m_io_init_value = p_io_init_value;
    m_pimpl->m_io_xor_value = (p_io_xor_value | 0b01000000);      // edge needs to be xored always (dialog does this too)
    return *this;
}

TurboBlocks& TurboBlocks::SetCompressionType(CompressionType p_compression_type)
{
    m_pimpl->m_compression_type = p_compression_type;
    return *this;
}

TurboBlocks& TurboBlocks::SetDeCompressionSpeed(int p_kb_per_sec)
{
    m_pimpl->m_decompression_speed = p_kb_per_sec;
    return *this;
}

TurboBlocks& TurboBlocks::CopyLoaderToScreen(uint16_t p_value)
{
    m_pimpl->CopyLoaderToScreen(p_value);
    return *this;
}

/// Set start of free space to copy loader including space for sp.
TurboBlocks&  TurboBlocks::SetLoaderCopyTarget(uint16_t p_value )
{
    m_pimpl->m_loader_copy_start = p_value;
    return *this;
}


/// Convenience public read access to Symbols as loaded by CTOR.
const Symbols& TurboBlocks::GetSymbols() const
{
    return m_pimpl->m_symbols;
}


uint16_t TurboBlocks::GetLoaderCodeLength(bool p_with_registers ) const
{
    return m_pimpl->GetLoaderCodeLength(p_with_registers);
}

TurboBlocks& TurboBlocks::SetSymbolFilename(const std::filesystem::path& p_symbol_file_name)
{
    m_pimpl->SetSymbolFilename(p_symbol_file_name);
    return *this;
}
















