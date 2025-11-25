// ==============================================================================
// PROJECT:         zqloader
// FILE:            tzxloader.h
// DESCRIPTION:     Definition of class TzxLoader.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#pragma once

#include <functional>
#include <string>
#include "datablock.h"


/// Loads TZX files.
/// Found blocks are handled through function set at SetOnHandleTapBlock.
/// See: http://k1.spdns.de/Develop/Projects/zasm/Info/TZX%20format.html
/// or: https://worldofspectrum.net/TZXformat.html
class TzxLoader
{
    using HandleTapBlockFun = std::function<bool (DataBlock, std::string)>;

public:

    /// CTOR.
    TzxLoader()
    {}


    /// Loads given tzx file from given file. Ignores until given zxfilename is found.
    TzxLoader& Load(const std::filesystem::path &p_filename, const std::string &p_zxfilename);



    /// Set callback when tapblock found
    TzxLoader& SetOnHandleTapBlock(HandleTapBlockFun p_fun)
    {
        m_OnHandleTapBlock = std::move(p_fun);
        return *this;
    }

private:

    bool HandleTapBlock(std::istream& p_stream, std::string p_zxfilename, int p_length);

    //  Loads tzx file from given stream. Ignores until given zxfilename is ound.
    TzxLoader& Read(std::istream& p_stream, const std::string &p_zxfilename);

private:

    HandleTapBlockFun   m_OnHandleTapBlock;
};

