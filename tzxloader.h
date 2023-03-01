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

public:
    /// CTOR taks a Taploader. Because TZX file often 'load data as in tap file'.
    /// Taploader has a virtual function that handles the thus loaded tap blocks.
    TzxLoader(TapLoader& p_taploader) :
        m_taploader(p_taploader)
    {}


    /// Loads given tzx file.
    /// TPath must be std::filesystem::path
    TzxLoader& Load(const std::filesystem::path &p_filename, std::string p_zxfilename);

    ///  Loads tzx file from given stream.
    TzxLoader& Load(std::istream& p_stream, std::string p_zxfilename);

private:
    bool LoadDataAsInTapFile(std::istream& p_stream, std::string p_zxfilename, size_t p_ignore);
private:
    TapLoader& m_taploader;
};


