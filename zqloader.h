//==============================================================================
// PROJECT:         zqloader
// FILE:            zqloader.h
// DESCRIPTION:     Definition of class SampleToWav.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================


#pragma once

#include <memory>           // std::unique_ptr
#include <functional>
#include <filesystem>
#include "types.h"          // CompressionType


#ifdef _MSC_VER
#pragma warning(disable: 4251)    //  class '' needs to have dll-interface to be used by clients of class ''
// Build, dll
#if defined( LIB_BUILD )
    #define LIB_API  __declspec( dllexport )
    //#pragma message( "Build, DLL")
#endif

// Use, dll
#if !defined( LIB_BUILD ) 
    #define LIB_API  __declspec( dllimport )
    //#pragma message( "Use, DLL")
#endif
#else
#define LIB_API
#endif

struct DataBlock;


class LIB_API ZQLoader
{
public:
    enum class Action
    {
        play_audio,
        write_wav,
        write_tzx,
    };
    using DoneFun = std::function<void(void)>;
public:
    ZQLoader();
    ~ZQLoader();

    /// Set path/to/file to normal speed load (tzx,tap)
    /// When empty use (and find) zqloader.tap.
    ZQLoader &SetNormalFilename(std::filesystem::path p_filename);

    ///  Set path/to/file to turboload (tzx,tap,z80,sna)
    ZQLoader &SetTurboFilename(std::filesystem::path p_filename);

    /// Set output file when action is write_wav or write_tzx.
    ZQLoader &SetOutputFilename(std::filesystem::path p_filename);

    /// Set Volume (-100 -- 100).
    ZQLoader &SetVolume(int p_volume_left, int p_volume_right);

    /// Set miniaudio sample rate. 0 = device default. 
    ZQLoader& SetSampleRate(uint32_t p_sample_rate);

    ///  Set zqloader duration parameters.
    ZQLoader& SetBitLoopMax(int p_value);
    ///  Set zqloader duration parameters.
    ZQLoader& SetBitOneThreshold(int p_value);
    ///  Set zqloader duration parameters.
    ZQLoader& SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay);

    /// Set compression used: none/rle/automatic.
    ZQLoader& SetCompressionType(CompressionType p_compression_type);

    /// What to do (on Run/Start):
    /// Play sound/ or write wav file/ or write tzx file.
    ZQLoader& SetAction(Action p_what);

    /// When loading snapshot put loader at screen.
    ZQLoader &SetUseScreen();

    /// Address where to put loader when loading snapshot, 0 = auto.
    ZQLoader &SetNewLoaderLocation(uint16_t p_address);

    /// Overwrite loader when at screen with attributes?
    ZQLoader &SetFunAttribs(bool p_value);

    ///  Reset, stop, wipe all added data (files)
    ZQLoader& Reset();

    // Start/wait/stop 
    ZQLoader& Run();

    // Start threaded
    ZQLoader& Start();

    // Stop/abort sound thread
    ZQLoader& Stop();

    ZQLoader & WaitUntilDone();

    ///  Busy (playing sound)?
    bool IsBusy() const;

    bool IsPreLoaded() const;
    
    /// Play a infinite leader tone for tuning.
    ZQLoader &PlayleaderTone();

    ZQLoader &AddDataBlock(DataBlock p_block, uint16_t p_start_address);

    /// Time at end so actual time needed once done.
    std::chrono::milliseconds GetTimeNeeded() const;
    
    /// Current running/loading time so since start.
    std::chrono::milliseconds GetCurrentTime() const;
    
    /// Estimated time needed calculated before.
    /// Note: this is not 100% accurate.
    std::chrono::milliseconds GetEstimatedDuration() const;

    /// path to current zqloader.exe (this program)
    /// (only to help find zqloader.tap)
    ZQLoader& SetExeFilename(std::filesystem::path p_filename);

    /// Set callback when done.
    ZQLoader& SetOnDone(DoneFun p_fun);

    ///  Get miniadio native device sample rate
    int GetDeviceSampleRate() const;

    ///  Get path/to/zqloader.tap
    std::filesystem::path GetZqLoaderFile() const;

    void Test();

    static bool WriteTextToAttr(DataBlock &out_attr, const std::string &p_text, std::byte p_color, bool p_center, int p_col);

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};

