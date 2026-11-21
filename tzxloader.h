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
    using HandleIncomingBlockFun = std::function<bool (DataBlock, std::string, int)>;

public:

    /// CTOR.
    TzxLoader()
    {}


    /// Loads given tzx file from given file. Ignores until given zxfilename is found.
    TzxLoader& Load(const std::filesystem::path &p_filename, const std::string &p_zxfilename);



    /// Set callback when block found
    TzxLoader& SetOnIncomingBlock(HandleIncomingBlockFun p_fun)
    {
        m_OnHandleIncomingBlock = std::move(p_fun);
        return *this;
    }

private:

    bool HandleIncomingBlock(DataBlock p_block, std::string p_string_data, int p_is_zq_loader_tzx);

    //  Loads tzx file from given stream. Ignores until given zxfilename is ound.
    TzxLoader& Read(std::istream& p_stream, const std::string &p_zxfilename);

private:
    bool m_is_zq_loader_tzx = false;
    HandleIncomingBlockFun   m_OnHandleIncomingBlock;
};

