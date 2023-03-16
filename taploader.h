//==============================================================================
// PROJECT:         zqloader
// FILE:            taploader.h
// DESCRIPTION:     Definition of class TapLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <iostream>
#include <string>
#include <filesystem>

struct DataBlock;




/// Loads tap files.
/// For each tab block found calls virtual HandleTapBlock.
/// See https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
class TapLoader
{
public:
    TapLoader& Load(const std::filesystem::path &p_filename, std::string p_zxfilename);
    
    TapLoader& Load(std::istream& p_stream, std::string p_zxfilename);
    
    DataBlock LoadTapBlock(std::istream& p_stream);

    virtual bool HandleTapBlock(DataBlock p_block, std::string p_zxfilename) = 0;

};
