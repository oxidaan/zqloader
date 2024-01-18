//==============================================================================
// PROJECT:         zqloader
// FILE:            taploader.cpp
// DESCRIPTION:     Definition of class TapLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#include "taploader.h"
#include <fstream>
#include "datablock.h"
#include "loadbinary.h"
#include <filesystem>

namespace fs = std::filesystem;

/// Load a data block as in TAP format from given stream.
///  [2 byte len] - [spectrum data incl. checksum]
/// Includes type (first) and checksum bytes (last) in the block read.
/// See https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
DataBlock TapLoader::LoadTapBlock(std::istream& p_stream)
{
    auto len = LoadBinary< uint16_t>(p_stream);
    return LoadTapBlock(p_stream, len);
}


/// Load a data block as in TAP format from given stream, 
/// but not the length: length already given.
DataBlock TapLoader::LoadTapBlock(std::istream& p_stream, int p_len)
{
    if (p_stream)
    {
        DataBlock block;
        block.resize(p_len);
        p_stream.read(reinterpret_cast<char*>(block.data()), p_len);
        return block;
    }
    throw std::runtime_error("Error reading tap block");
}



inline bool TapLoader::HandleTapBlock(DataBlock p_block, std::string p_zxfilename)
{
    if (m_OnHandleTapBlock)
    {
        return m_OnHandleTapBlock(std::move(p_block), p_zxfilename);
    }
    return false;
}

/// Load a tap file from given filename.
/// p_zxfilename: the ZX Spectrum file name, eg used to filter / only load certain 
/// program names.
TapLoader& TapLoader::Load(const fs::path &p_filename, std::string p_zxfilename)
{
    std::ifstream fileread(p_filename, std::ios::binary);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    try
    {
        std::cout << "Loading file " << p_filename << std::endl;
        Load(fileread, p_zxfilename);
    }
    catch(const std::exception &e)
    {
       // clarify
       throw std::runtime_error("Reading file: " + p_filename.string() + ": " + e.what());
    }
    return *this;
}


/// Load a tap file from given stream. Stops when tap block was handled: that 
/// is when HandleTapBlock returns true.
/// p_zxfilename: the ZX Spectrum file name, eg used to filter / only load certain 
/// program names.
inline TapLoader& TapLoader::Load(std::istream& p_stream, std::string p_zxfilename)
{
    bool done = false;
    while (p_stream.peek() && p_stream.good() && !done)
    {
        auto block = LoadTapBlock(p_stream);
        done =       HandleTapBlock(std::move(block), p_zxfilename);   
    }
    return *this;
}
