//==============================================================================
// PROJECT:         zqloader
// FILE:            zqloader.h
// DESCRIPTION:     Definition of class ZQLoader.
//                  This is the main ZQLoader interface used for both commandline
//                  as qt ui tool.
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

/// This is the main ZQLoader interface used for both commandline
/// as qt ui tool.
class LIB_API ZQLoader
{
public:
    enum class Action
    {
        play_audio,
        write_wav,
        write_tzx,
    };
    enum class LoaderLocation
    {
        automatic,
        screen,
    };
    using DoneFun = std::function<void(void)>;
public:
    ZQLoader();
    ~ZQLoader();

    /// Set path/to/file to normal speed load (tzx,tap)
    /// When p_filename empty try to find zqloader.tap and use that.
    /// Eg top filename in dialog.
    ZQLoader &SetNormalFilename(std::filesystem::path p_filename, const std::string &p_zxfilename = "");

    /// Set path/to/file to turboload (tzx,tap,z80,sna)
    /// Eg 2nd filename in dialog.
    ZQLoader &SetTurboFilename(std::filesystem::path p_filename, const std::string &p_zxfilename = "");

    /// Set output file; this makes action write_wav or write_tzx.
    ZQLoader &SetOutputFilename(std::filesystem::path p_filename, bool p_allow_overwrite);

    /// Set Volume (-100 -- 100).
    ZQLoader &SetVolume(int p_volume_left, int p_volume_right);

    /// Set miniaudio sample rate. 0 = device default. 
    ZQLoader& SetSampleRate(uint32_t p_sample_rate);

    ///  Set zqloader duration parameters.
    ZQLoader& SetBitLoopMax(int p_value);
    ///  Set zqloader duration parameters.
    ZQLoader& SetZeroMax(int p_value);
    ///  Set zqloader duration parameters.
    ZQLoader& SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay);
    ///  Set zqloader IO values
    ZQLoader& SetIoValues(int p_io_init_value, int p_io_xor_value);

    /// Set compression used: none/rle/automatic.
    ZQLoader& SetCompressionType(CompressionType p_compression_type);

    /// Set DeCompression speed (kb/sec)
    ZQLoader& SetDeCompressionSpeed(int p_kb_per_sec);

    ///  Set time to wait after ZQLoader was loaded
    ZQLoader& SetInitialWait(std::chrono::milliseconds p_initial_wait);

    /// Set clock frequency in hz
    ZQLoader& SetSpectrumClock(int p_spectrum_clock);

    // Ignore speed as set by SetTstateDuration for normal speed (=rom) loading routines
    // Then take 3.5Mhz.
    ZQLoader &SetUseStandaardSpeedForRom(bool p_to_what);

    /// What to do (on Run/Start):
    /// Play sound/ or write wav file/ or write tzx file.
    ZQLoader& SetAction(Action p_what);

    /// Address where to put loader when loading snapshot or when overwriting BASIC.
    /// When <>0 always move loader to that location.
    ZQLoader &SetLoaderCopyTarget(uint16_t p_address);

    /// Where to put loader when loading snapshot: automatic or screen.
    ZQLoader &SetLoaderCopyTarget(LoaderLocation p_where);

    /// Overwrite loader when at screen with attributes?
    ZQLoader &SetFunAttribs(bool p_value);

    /// Dont call machine code (USR) at end of loading, return to BASIC instead
    ZQLoader& SetDontCallUser(bool p_value);

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

    // Add zqloader.tap to be preloaded
    ZQLoader &SetPreload();

    // Was zqloader.tap added to be preloaded?
    bool IsPreLoaded() const;
    
    /// Play an infinite leader tone for tuning.
    ZQLoader &PlayleaderTone();

    /// Only used for fun attributes
    ZQLoader &AddDataBlock(DataBlock p_block, uint16_t p_start_address);

    /// Time at end so actual time needed once done.
    std::chrono::milliseconds GetTimeNeeded() const;
    
    /// Current running/loading time so since start.
    std::chrono::milliseconds GetCurrentTime() const;
    
    /// Estimated time needed calculated before.
    /// Note: this is not 100% accurate.
    std::chrono::milliseconds GetEstimatedDuration() const;

    /// Get time in TStates.
    int GetDurationInTStates() const;

    /// path to current zqloader.exe (this program)
    /// (only to help find zqloader.tap)
    ZQLoader& SetExeFilename(std::filesystem::path p_filename);

    /// Set callback when done.
    ZQLoader& SetOnDone(DoneFun p_fun);

    ///  Get miniadio native device sample rate
    uint32_t GetDeviceSampleRate() const;

    // Run Test
    void Test();

    static void Version(bool p_with_markup = false);

    static bool WriteTextToAttr(DataBlock &out_attr, const std::string &p_text, std::byte p_color, bool p_center, int p_col);

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};


LIB_API const char *GetVersion();
