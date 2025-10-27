// ==============================================================================
// PROJECT:         zqloader
// FILE:            symbols.h
// DESCRIPTION:     Definition of class Symbols
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include <map>
#include <string>
#include <cstdint>
#include <filesystem>


/// Loads and maintains named z80 symbols as saved by sjasmplus to an export file.
/// eg zqloader.exp.
class Symbols
{
public:

    Symbols()                              = default;
    Symbols(Symbols &&)                    = default;
    Symbols(const Symbols &)               = delete;
    Symbols & operator = (Symbols &&)      = default;
    Symbols & operator = (const Symbols &) = delete;


    /// CTOR, loads symbols from given export file. Typical 'zqloader.exp'
    /// Throws when error loading.
    Symbols(const std::filesystem::path &p_filename)
    {
        ReadSymbols(p_filename);
    }


    /// Read/append symbols from given export file.
    /// Existing symbols with same name will be overwritten.
    /// Throws when error loading.
    void ReadSymbols(const std::filesystem::path &p_filename);

    /// Get 16bit symbol value by name.
    /// Throws when not found.
    uint16_t GetSymbol(const std::string& p_name) const;

    /// Convenience; set a byte value at given block at address with give symbol name.
    /// (this is used to modify a z80 snapshot register block)
    /// Throws when symbol not found.
    template <class TDataBlock>
    void SetByte(TDataBlock& p_block, const char* p_name, uint8_t val) const
    {
        p_block[GetSymbol(p_name)] = std::byte{ val };
    }


    /// Convenience; set a 16bit word value at given block at address with give symbol name.
    /// (this is used to modify a z80 snapshot register block)
    /// Throws when symbol not found.
    template <class TDataBlock>
    void SetWord(TDataBlock& p_block, const char* p_name, uint16_t val) const
    {
        auto sbm = GetSymbol(p_name);
        p_block[sbm]     = std::byte(val & 0xff);       // z80 is little endian
        p_block[sbm + 1] = std::byte((val >> 8) & 0xff);
    }

private:

    std::map<std::string, uint16_t>   m_symbols;
};