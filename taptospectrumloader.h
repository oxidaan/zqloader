//==============================================================================
// PROJECT:         zqloader
// FILE:            taptospectrumloader.h
// DESCRIPTION:     Definition of class TapToSpectrumLoader.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include "taploader.h"
#include "spectrum_loader.h"


/// A taploader that feeds its loaded data blocks to given 
/// SpectrumLoader
class TapToSpectrumLoader : public TapLoader
{
public:
    TapToSpectrumLoader(SpectrumLoader& p_spectrumloader) :
        m_spectrumloader(p_spectrumloader)
    {}


    virtual bool HandleTapBlock(DataBlock p_block, std::string p_zxfilename) override
    {
        m_spectrumloader.AddLeaderPlusData(std::move(p_block), 700, 1750ms);//.AddPause(100ms);
        return false;
    }
private:
    SpectrumLoader& m_spectrumloader;
};
