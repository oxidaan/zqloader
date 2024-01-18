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
#include <functional>
struct DataBlock;




/// Loads tap files (from file).
/// For each tap block found calls virtual HandleTapBlock.
/// See https://sinclair.wiki.zxnet.co.uk/wiki/TAP_format
class TapLoader
{
public:
    using HandleTapBlockFun = std::function<bool(DataBlock, std::string)>;

    /// Load a tap file from given filename.
    /// p_zxfilename: the ZX Spectrum file name, eg used to filter / only load certain 
    /// program names.
    TapLoader& Load(const std::filesystem::path &p_filename, std::string p_zxfilename);
    
    /// Load a tap file from given stream. Stops when HandleTapBlock returns true.
    TapLoader& Load(std::istream& p_stream, std::string p_zxfilename);

    /// Load a data block as in TAP format from given stream.    
    DataBlock LoadTapBlock(std::istream& p_stream);

    /// Load a data block as in TAP format from given stream, 
    /// but not the length: length already given (when parsing TZX files)
    DataBlock LoadTapBlock(std::istream& p_stream, int p_length);



    /// Set callback when tapblock found.
    TapLoader& SetOnHandleTapBlock(HandleTapBlockFun p_fun)
    {
        m_OnHandleTapBlock = std::move(p_fun);
        return *this;
    }
private:
    bool HandleTapBlock(DataBlock p_block, std::string p_zxfilename);
private:
    HandleTapBlockFun m_OnHandleTapBlock;

};
