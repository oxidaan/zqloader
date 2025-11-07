//==============================================================================
// PROJECT:         zqloader
// FILE:            datablock.cpp
// DESCRIPTION:     Implementation of class DataBlock.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================




#include "datablock.h"
#include <fstream>


namespace fs = std::filesystem;


DataBlock LoadFromFile(const fs::path &p_filename)
{
    std::ifstream fileread(p_filename, std::ios::binary | std::ios::ate);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    int filelen = int(fileread.tellg());
    fileread.seekg(0, std::ios::beg);
    DataBlock blk;
    blk.resize(filelen);
    fileread.read(reinterpret_cast<char*>(blk.data()), filelen);
    return blk;
}


void SaveToFile(const DataBlock &p_to_save, const fs::path &p_filename)
{
    std::ofstream filewrite(p_filename, std::ios::binary);
    if (!filewrite)
    {
        throw std::runtime_error("Cannot open file " + p_filename.string() + ".");
    }
    filewrite.write(reinterpret_cast<const char*>(p_to_save.data()), p_to_save.size());
}
