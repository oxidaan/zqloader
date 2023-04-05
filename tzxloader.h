//==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.h
// DESCRIPTION:     Definition of class TzxLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================

#pragma once

#include <cstdint>
#include "taploader.h"
#include <filesystem>       // std::filesystem::path


/// Loads TZX files.
/// Result ends up at virtual function at given TapLoader, see CTOR.
/// See: http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
/// or: https://worldofspectrum.net/TZXformat.html
class TzxLoader
{
    using HandleTapBlockFun = std::function<bool(DataBlock, std::string)>;

public:
    /// CTOR takes a Taploader. Because TZX file often 'load data as in tap file'.
    /// Taploader has a virtual function that handles the thus loaded tap blocks.
    TzxLoader()
    {}


    /// Loads given tzx file.
    /// TPath must be std::filesystem::path
    TzxLoader& Load(const std::filesystem::path &p_filename, std::string p_zxfilename);

    ///  Loads tzx file from given stream.
    TzxLoader& Load(std::istream& p_stream, std::string p_zxfilename);

    /// Set callback when tapblock found
    TzxLoader& SetOnHandleTapBlock(HandleTapBlockFun p_fun)
    {
        m_OnHandleTapBlock = std::move(p_fun);
        return *this;
    }
private:
    bool HandleTapBlock(std::istream& p_stream, std::string p_zxfilename, int p_length);
private:
    HandleTapBlockFun m_OnHandleTapBlock;
};


