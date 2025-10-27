//==============================================================================
// PROJECT:         zqloader
// FILE:            symbols.cpp
// DESCRIPTION:     Implementation of class Symbols
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "symbols.h"
#include <fstream>
#include <string>
#include <sstream>
#include <filesystem>


namespace fs = std::filesystem;



/// Get 16 bit symbol value by name.
/// Throws when symbol not found.
uint16_t Symbols::GetSymbol(const std::string& p_name) const
{
    if (m_symbols.find(p_name) != m_symbols.end())
    {
        auto a = m_symbols.at(p_name);
        return a;
    }
    throw std::runtime_error("Symbol " + p_name + " not found");
}


/// Read/append symbols from given export file.
/// Existing symbols with same name will be overwritten.
/// Throws when error loading.
void Symbols::ReadSymbols(const fs::path &p_filename)
{
    std::ifstream fileread(p_filename);
    if (!fileread)
    {
        throw std::runtime_error("Symbol file " + p_filename.string() + " not found.");
    }
    std::string line;
    while (std::getline(fileread, line))
    {
        std::stringstream ss(line);
        std::string symb;
        ss >> symb;
        if (symb.back() == ':')
        {
            symb.pop_back();
        }
        ss.ignore(1);
        int value;
        std::string equ;
        ss >> equ >> std::hex >> value;
        m_symbols[symb] = uint16_t(value);
    }
}
