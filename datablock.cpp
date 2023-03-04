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
#include <filesystem>

namespace fs = std::filesystem;

template <>
DataBlock& DataBlock::LoadFromFile(const fs::path &p_filename)
{
    std::ifstream fileread(p_filename, std::ios::binary | std::ios::ate);
    if (!fileread)
    {
        throw std::runtime_error("File " + p_filename.string() + " not found.");
    }
    int filelen = int(fileread.tellg());
    fileread.seekg(0, std::ios::beg);
    resize(filelen);
    fileread.read(reinterpret_cast<char*>(data()), filelen);
    return *this;
}
