//==============================================================================
// PROJECT:         zqloader
// FILE:            zqloader.cpp
// DESCRIPTION:     Implementation of class ZQLoader.
//                  This is the main ZQLoader interface used for both commandline
//                  as qt ui tool.
// 
// Copyright (c) 2024 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================
// uses pimpl

#include "zqloader.h"
#include "tools.h"
#include "spectrum_loader.h"
#include "taploader.h"
#include "tzxloader.h"
#include "turboblocks.h"
#include "memoryblock.h"
#include <iostream>
#include <fstream>
#include "taptoturboblocks.h"
#include "z80snapshot_loader.h"
#include "samplesender.h"
#include "sampletowav.h"
#include "loader_defaults.h"

#include <filesystem>
namespace fs = std::filesystem;


LIB_API const char *GetVersion()
{
    return "3.0.1 RC1";
}


class ZQLoader::Impl
{
public:

    Impl()  = default;
    Impl(const Impl &) = delete;
    Impl(Impl &&) noexcept = default;
    Impl & operator = (Impl &&) = default;
    Impl & operator = (const Impl &) = delete;

    // Set normal filename (eg the top filename in the dialog)
    void SetNormalFilename(fs::path p_filename, const std::string &p_zxfilename)
    {
        m_normal_filename = std::move(p_filename);
        if(!FileIsZqLoader(m_normal_filename))
        {
            AddNormalSpeedFile(m_normal_filename, p_zxfilename);    // when p_filename emtpy makes it zqloader.tap
        }
    }

    // File is a zqloader.tap file?
    bool FileIsZqLoader(fs::path p_filename) const
    {
        return ToLower(p_filename.stem().string()).find("zqloader") == 0 || p_filename.empty() || p_filename.string()[0] == '[';
    }

    // Set turbo filename eg the 2nd filename in the dialog.
    void SetTurboFilename(fs::path p_filename, const std::string &p_zxfilename )
    {
        if(!p_filename.empty())
        {
            m_turbo_filename = std::move(p_filename);
            AddTurboSpeedFile(m_turbo_filename, p_zxfilename);
        }
    }


    void SetOutputFilename(fs::path p_outputfilename, bool p_allow_overwrite)
    {
        if (ToLower(p_outputfilename.extension().string()) == ".tzx")
        {
            m_action = Action::write_tzx;
        }
        else if (ToLower(p_outputfilename.extension().string()) == ".wav")
        {
            m_action = Action::write_wav;
        }
        m_allow_overwrite = p_allow_overwrite;
        m_output_filename = std::move(p_outputfilename);
    }

    // Set volume left + right (-100, 100)
    void SetVolume(int p_volume_left, int p_volume_right)
    {
        m_volume_left  = p_volume_left;
        m_volume_right = p_volume_right;
        m_sample_sender.SetVolume(m_volume_left, m_volume_right);
    }





    void SetSpectrumClock(int p_spectrum_clock)
    {
        m_spectrumloader.SetTstateDuration(1s / double(p_spectrum_clock));
    }





    // runs in miniaudio / samplesender thread
    void OnDone()
    {
        if(m_is_busy)
        {
            m_time_needed = GetCurrentTime();
            std::cout << "Took: " << std::dec << m_time_needed.count() << " ms" << std::endl;
            m_is_busy = false;
        }
        if(m_OnDone)
        {
            m_OnDone();
        }
        
    }

    
    // Estimated duration
    // Not 100% accurate because discrepancy between miniaudio sample rate and time we actually need.
    // See remark at  SampleSender::GetNextSample
    std::chrono::milliseconds GetEstimatedDuration() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(m_spectrumloader.GetEstimatedDuration());
    }


    void PlayleaderTone()
    {
        m_spectrumloader.AddEndlessLeader();
        m_spectrumloader.Attach(m_sample_sender);
        m_sample_sender.SetVolume(m_volume_left, m_volume_right).SetSampleRate(m_sample_rate);
        m_sample_sender.Start();            // Play!
        m_is_busy = true;
    }


    void Run(bool p_threaded)
    {
        Check();
        m_turboblocks.DebugDump();
        std::cout << "Estimated duration: " << GetEstimatedDuration().count() << "ms  (" <<  m_spectrumloader.GetDurationInTStates() << " TStates)" << std::endl;
        m_spectrumloader.SetOnDone([this]
        {
            OnDone();
        });
        m_start_time = std::chrono::steady_clock::now();      // to measure duration only
        if(m_action == Action::play_audio)
        {
            // normal, so play as sound
            m_spectrumloader.Attach(m_sample_sender);
            m_sample_sender.SetVolume(m_volume_left, m_volume_right).SetSampleRate(m_sample_rate);
            if(p_threaded)
            {
                m_is_busy = true;
                m_sample_sender.Start();           // Play! Thread
            }
            else
            {
                m_is_busy = true;
                m_sample_sender.Run();            // Play! Wait for finish
                // Reset();
            }
        }
        else if (m_action == Action::write_wav)
        {
            // Write to wav file
            auto outputfilename     = GetOutputFilename();
            std::ofstream filewrite = OpenFileToWrite(outputfilename, m_allow_overwrite);
            SampleToWav wav_writer;
            m_spectrumloader.Attach(wav_writer);
            wav_writer.SetVolume(m_volume_left, m_volume_right).SetSampleRate(m_sample_rate);
            wav_writer.WriteToFile(filewrite);
            std::cout << "Written " << outputfilename << " with size: " << wav_writer.GetSize() << " and duration: " << wav_writer.GetDuration().count() << "s" << std::endl;
            Reset();
        }
        else if (m_action == Action::write_tzx)
        {
            // Write to tzx file
            auto outputfilename     = GetOutputFilename();
            std::ofstream filewrite = OpenFileToWrite(outputfilename, m_allow_overwrite);
            m_spectrumloader.WriteTzxFile(filewrite);
            std::cout << "Written " << outputfilename << std::endl;
            Reset();
        }
    }

    /// Only used for fun attributs
    void AddDataBlock(DataBlock p_block, uint16_t p_start_address)
    {
        m_turboblocks.AddMemoryBlock({std::move(p_block), p_start_address});
        m_turboblocks.MoveToLoader(m_spectrumloader, true);
    }


    /// Stop/cancel playing immidiately
    /// Keeps preloaded state
    /// Can cause tape loading error when not calling WaitUntilDone first.
    void Stop()
    {
        auto is_preloaded = m_is_preloaded; // keep
        Reset();
        m_is_preloaded = is_preloaded;
    }



    /// reset by move assigning empty/new instance.
    /// (Will stop sending immidiately).
    /// Can cause tape loading error when not calling WaitUntilDone first.
    void Reset()
    {
        auto onDone = std::move(m_OnDone);  // keep call back
        auto exe_path = std::move(m_exe_path);
        SampleSender remove = std::move(m_sample_sender);   // <- Because else move assign causes problems. Dtor target not called. So ma_device_uninit not called.
        *this = Impl();
        m_exe_path = std::move(exe_path);
        m_OnDone = std::move(onDone);
    }


    void SetPreload()
    {
        m_128_mode = true;
        AddZqLoader(m_normal_filename);
        m_turboblocks.MoveToLoader(m_spectrumloader, true);
        m_is_preloaded = true;
    }






    std::chrono::milliseconds GetCurrentTime() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    }





    


    /// Try to find the path/to/zqloader.tap 
    /// (when p_filename has no path yet)
    /// Throws when file not found.
    fs::path FindZqLoaderTapfile(const fs::path &p_filename) const
    {
        fs::path filename = p_filename;
        if(filename.empty() || filename.string()[0] == '[')
        {
            filename = m_128_mode ? "zqloader128.tap" : "zqloader.tap";
        }
        if(!FileIsZqLoader(filename))
        {
            throw std::runtime_error("First file needs to be a version of zqloader (now: '" + filename.string() + "')");
        }

        if(filename == filename.filename())     // given just zqloader.tap w/o path so try to find
        {
            // Try to find zqloader.tap file
            if (!std::filesystem::exists(filename))
            {
                // try at current executable directory by default
                filename = m_exe_path / filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // try at current executable directory/z80 by default
                filename = m_exe_path / "z80" / filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // then workdir
                filename = std::filesystem::current_path() / filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // then workdir/z80
                filename = std::filesystem::current_path() / "z80" / filename.filename();
            }
        }
        if (!std::filesystem::exists(filename))
        {
            throw std::runtime_error("ZQLoader file '" + filename.string() + "' not found. (checked: "
            + m_exe_path.string() + ", " + std::filesystem::current_path().string() +
            ") Please give path/to/zqloader.tap. (this is the tap file that contains the ZX Spectrum turboloader)");
        }
        return filename;
    }
    void Test()
    {
        extern uint16_t Test(TurboBlocks & p_blocks, const fs::path &p_filename);
        auto adr = Test(m_turboblocks, "");
        m_turboblocks.Finalize(adr);
        m_turboblocks.MoveToLoader(m_spectrumloader);
    }
   
    void SetLoaderCopyTarget(uint16_t p_address)
    {
        m_new_loader_location = p_address;
        if(p_address != 0)      // fixed address (not automatic)
        {
            m_turboblocks.SetLoaderCopyTarget(p_address);
        }
    }

private:



    static std::ofstream OpenFileToWrite(const fs::path &p_filename, bool p_allow_overwrite)
    {
        if (!p_allow_overwrite && std::filesystem::exists(p_filename))
        {
            throw std::runtime_error("File to write (" + p_filename.string() + ") already exists. Please remove first.");
        }
        std::ofstream filewrite(p_filename, std::ios::binary);
        if (!filewrite)
        {
            throw std::runtime_error("Could not open file " + p_filename.string() + " for writing");
        }
        return filewrite;
    }


    // Add given normal speed file (tap/tzx) to m_spectrumloader
    void AddNormalSpeedFile(const fs::path &p_filename, const std::string &p_zxfilename)
    {
        if(!p_filename.empty())
        {
            std::cout << "Processing normal speed file: " << p_filename << std::endl;
            // filename is tap/tzx file to be normal loaded into the ZX Spectrum.
            if (ToLower(p_filename.extension().string()) == ".tap")
            {
                AddNormalSpeedFile<TapLoader>(p_filename, m_spectrumloader, p_zxfilename);
            }
            else if (ToLower(p_filename.extension().string()) == ".tzx")
            {
                AddNormalSpeedFile<TzxLoader>(p_filename, m_spectrumloader, p_zxfilename);
            }
            else
            {
                throw std::runtime_error("Unknown file type for filename: " + p_filename.string() + " (extension not tap / tzx)");
            }
        }
    }

    // Add/load zqloader.tap (to turboblocks) when not already done so.
    // throws when p_filename is not zqloader or could not be found/
    void AddZqLoader(const fs::path &p_filename)
    {
        if(!m_is_preloaded && !m_turboblocks.IsZqLoaderAdded())
        {
            auto filename = FindZqLoaderTapfile(p_filename);
            m_turboblocks.AddZqLoader(filename);                 // zqloader.tap
        }
    }

    void AddTurboSpeedFile(const fs::path &p_filename, const std::string &p_zxfilename)
    {
        if (ToLower(p_filename.extension().string()) == ".tap")
        {
            std::cout << "Processing tap file: " << p_filename << " (turbo speed)" << std::endl;
            AddFileToTurboBlocks<TapLoader>(p_filename, p_zxfilename);
        }
        else if (ToLower(p_filename.extension().string()) == ".tzx")
        {
            std::cout << "Processing tzx file: " << p_filename << " (turbo speed)" << std::endl;
            AddFileToTurboBlocks<TzxLoader>(p_filename, p_zxfilename);
        }
        else if (ToLower(p_filename.extension().string()) == ".z80" ||
                 ToLower(p_filename.extension().string()) == ".sna")
        {
            std::cout << "Processing snapshot file: " << p_filename << " (turbo speed)" << std::endl;
            AddSnapshotToTurboBlocks(p_filename);
        }
        else if (ToLower(p_filename.extension().string()) == ".bin" ||
                 p_filename == "testdata")
        {
            // reserved for Test
            extern uint16_t Test(TurboBlocks & p_blocks, const fs::path &p_filename);
            auto adr = Test(m_turboblocks, p_filename);
            m_turboblocks.Finalize(adr);
        }
        else if(!p_filename.empty())
        {
            throw std::runtime_error("Unknown file type for filename: " + p_filename.string() + " (extension not tap / tzx / z80)");
        }

        m_turboblocks.MoveToLoader(m_spectrumloader);
    }


    
    // Add given file (tap/tzx) to given TurboBlocks so uses turbo speed.
    // (adds all blocks present in given file).
    // TLoader is TapLoader or TzxLoader.
    // Finalize will already be called.
    template<class Tloader>
    void AddFileToTurboBlocks(const fs::path &p_filename, const std::string &p_zxfilename)
    {
        m_128_mode = false;
        AddZqLoader(m_normal_filename);
        TapToTurboBlocks tab_to_turbo_blocks{ m_turboblocks };
        Tloader tap_or_tzx_loader;
        tap_or_tzx_loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string p_zxfilename)
        {
            return tab_to_turbo_blocks.HandleTapBlock(std::move(p_block), p_zxfilename);
        });
        tap_or_tzx_loader.Load(p_filename, p_zxfilename);
        if(m_turboblocks.size() != tab_to_turbo_blocks.GetNumberLoadCode())
        {
            std::cout << "<b>Warning: Number of found code blocks (" << m_turboblocks.size() << ") not equal to LOAD \"\" CODE statements in BASIC (" << tab_to_turbo_blocks.GetNumberLoadCode() << ")!</b>\n" << std::endl;
        }
        auto size = m_turboblocks.Finalize(tab_to_turbo_blocks.GetUsrAddress(), tab_to_turbo_blocks.GetClearAddress());
        if(size == 0)
        {
            throw std::runtime_error("No blocks present in file: '" + p_filename.string() + "' that could be turboloaded (note: can only handle code blocks, not BASIC)");
        }
    }

    // Add given snapshot file (z80/sna) to given TurboBlocks so uses turbo speed.
    // Finalize will already be called.
    void AddSnapshotToTurboBlocks(const fs::path &p_filename)
    {
        SnapShotLoader snapshotloader;
        snapshotloader.Load(p_filename);
        m_128_mode = !snapshotloader.Is48KSnapShot();
        AddZqLoader(m_normal_filename);

        // Read file snapshotregs.bin (created by sjasmplus) -> regblock
        fs::path snapshot_regs_filename = FindZqLoaderTapfile(m_normal_filename);
        snapshot_regs_filename.replace_filename("snapshotregs");
        snapshot_regs_filename.replace_extension("bin");
        DataBlock regblock = LoadFromFile(snapshot_regs_filename);
        snapshotloader.SetRegBlock(std::move(regblock)).MoveToTurboBlocks(m_turboblocks, m_new_loader_location, m_use_fun_attribs);
        m_turboblocks.Finalize(snapshotloader.GetUsrAddress(), 0, snapshotloader.GetLastBankToSwitchTo());
    }



    // Add given file (tap/tzx) to given SpectrumLoader so uses normal speed
    // TLoader is TapLoader or TzxLoader
    template<class Tloader>
    void AddNormalSpeedFile(const fs::path &p_filename, SpectrumLoader &p_spectrum_loader, const std::string &p_zxfilename)
    {
        Tloader tap_or_tzx_loader;
        tap_or_tzx_loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string)
                                              {
                                                  p_spectrum_loader.AddLeaderPlusData(std::move(p_block), spectrum::tstate_quick_zero, 1750ms); // .AddPause(100ms);
                                                  return false;
                                              }
                                              );
        tap_or_tzx_loader.Load(p_filename, p_zxfilename);
    }




    
    void Check()
    {
        // also called when preload clicked, then 2nd file not needed to be present.
        if(FileIsZqLoader(m_normal_filename) && m_turbo_filename.empty() && !m_is_preloaded)
        {
            throw std::runtime_error(1 + &*R"(
When using zqloader.tap a 2nd filename is needed as runtime argument,
with the program to be turboloaded. A game for example. 
Else the ZX Spectrum will not do anything after loading the turbo loader,
except waiting.
)");
        }

        if(m_spectrumloader.GetEstimatedDuration() == 0ms)
        {
            throw std::runtime_error(1 + &*R"(
No files added. Nothing to do.
Please add a normal and or turbo speed file.
)");
        }
    }




    // Determine output (wav/tzx) filename to write.
    // Can auto create output file name out-off turbo filename.
    fs::path GetOutputFilename() const
    {
        if(m_output_filename.empty() && !m_turbo_filename.empty())
        {
            // When -w or -wav is given create output filename out of 2nd given filename (filename2)
            auto outputfilename = m_turbo_filename;
            if( m_action == Action::write_wav )
            {
                outputfilename.replace_extension("wav");
                return outputfilename;
            }
            if( m_action == Action::write_tzx )
            {
                outputfilename.replace_extension("tzx");
                return outputfilename;
            }
        }
        if(m_output_filename.empty())
        {
            throw std::runtime_error("Could not determine output filename");
        }
        return m_output_filename;
    }




public:
    SpectrumLoader                          m_spectrumloader;
    TurboBlocks                             m_turboblocks;
    SampleSender                            m_sample_sender;

    uint16_t                                m_new_loader_location = 0; // 0 = auto
    bool                                    m_use_fun_attribs     = false;
    int                                     m_volume_left         = loader_defaults::volume_left;
    int                                     m_volume_right        = loader_defaults::volume_right;
    uint32_t                                m_sample_rate         = loader_defaults::sample_rate;
    bool                                    m_allow_overwrite     = false; // see OpenFileToWrite
    fs::path                                m_normal_filename;             // usually zqloader.tap
    fs::path                                m_turbo_filename;
    fs::path                                m_output_filename;             // writing wav or tzx
    fs::path                                m_exe_path;                    // s/a argv[0]
    Action                                  m_action = Action::play_audio;

    bool                                    m_is_busy = false;
    bool                                    m_128_mode = false;
    bool                                    m_is_preloaded = false;
    DoneFun                                 m_OnDone;

    std::chrono::steady_clock::time_point   m_start_time{};
    std::chrono::milliseconds               m_time_needed{};
}; // class ZQLoader::Impl



ZQLoader::ZQLoader()
    :m_pimpl( new Impl())
{}



ZQLoader::~ZQLoader()
{}  // = default;



ZQLoader& ZQLoader::SetNormalFilename(fs::path p_filename, const std::string &p_zxfilename )
{
    m_pimpl->SetNormalFilename(std::move(p_filename), p_zxfilename);
    return *this;
}



ZQLoader& ZQLoader::SetTurboFilename(fs::path p_filename, const std::string &p_zxfilename)
{
    m_pimpl->SetTurboFilename(std::move(p_filename), p_zxfilename);
    return *this;
}



ZQLoader& ZQLoader::SetOutputFilename(fs::path p_filename, bool p_allow_overwrite)
{
    m_pimpl->SetOutputFilename(std::move(p_filename), p_allow_overwrite);
    return *this;
}



ZQLoader& ZQLoader::SetVolume(int p_volume_left, int p_volume_right)
{
    m_pimpl->SetVolume(p_volume_left, p_volume_right);
    return *this;
}



ZQLoader& ZQLoader::SetSampleRate(uint32_t p_sample_rate)
{
    m_pimpl->m_sample_rate = p_sample_rate;
    return *this;
}



ZQLoader& ZQLoader::SetBitLoopMax(int p_value)
{
    m_pimpl->m_turboblocks.SetBitLoopMax(p_value);
    return *this;
}



ZQLoader& ZQLoader::SetZeroMax(int p_value)
{
    m_pimpl->m_turboblocks.SetZeroMax(p_value);
    return *this;
}

ZQLoader& ZQLoader::SetIoValues(int p_io_init_value, int p_io_xor_value)
{
    m_pimpl->m_turboblocks.SetIoValues(p_io_init_value, p_io_xor_value);
    return *this;
}



ZQLoader& ZQLoader::SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
{
    m_pimpl->m_turboblocks.SetDurations(p_zero_duration, p_one_duration, p_end_of_byte_delay);
    return *this;
}



ZQLoader& ZQLoader::SetCompressionType(CompressionType p_compression_type)
{
    m_pimpl->m_turboblocks.SetCompressionType(p_compression_type);
    return *this;
}

ZQLoader& ZQLoader::SetDeCompressionSpeed(int p_kb_per_sec)
{
    m_pimpl->m_turboblocks.SetDeCompressionSpeed(p_kb_per_sec);
    return *this;
}

ZQLoader& ZQLoader::SetInitialWait(std::chrono::milliseconds p_initial_wait)
{
    m_pimpl->m_turboblocks.SetInitialWait( p_initial_wait);
    return *this;
}


ZQLoader& ZQLoader::SetSpectrumClock(int p_spectrum_clock)
{
    m_pimpl->SetSpectrumClock(p_spectrum_clock);
    return *this;
}

ZQLoader& ZQLoader::SetUseStandaardSpeedForRom(bool p_to_what)
{
    m_pimpl->m_spectrumloader.SetUseStandaardSpeedForRom(p_to_what);
    return *this;
}

ZQLoader& ZQLoader::SetAction(Action p_what)
{
    m_pimpl->m_action = p_what;
    return *this;
}



ZQLoader& ZQLoader::SetLoaderCopyTarget(uint16_t p_address)
{
    m_pimpl->SetLoaderCopyTarget(p_address);
    return *this;
}

ZQLoader& ZQLoader::SetLoaderCopyTarget(LoaderLocation p_where)
{
    if (p_where == LoaderLocation::automatic)
    {
        SetLoaderCopyTarget(0);
    }
    else if (p_where == LoaderLocation::screen)
    {
        SetLoaderCopyTarget(spectrum::SCREEN_23RD);
    }
    return *this;
}





ZQLoader& ZQLoader::SetFunAttribs(bool p_value)
{
    m_pimpl->m_use_fun_attribs = p_value;;
    return *this;
}



ZQLoader& ZQLoader::Reset()
{
    m_pimpl->Reset();
    return *this;
}

/// Not threaded, (or wait for thread)
ZQLoader& ZQLoader::Run()
{
    m_pimpl->Run(false);
    return *this;
}


/// threaded (returns immidiately)
ZQLoader& ZQLoader::Start()
{
    m_pimpl->Run(true);
    return *this;
}



ZQLoader& ZQLoader::Stop()
{
    m_pimpl->Stop();
    return *this;
}


/// Wait until all data have send.
/// Note: at preloading fun attibutes this will probably 
/// wait forever.
ZQLoader& ZQLoader::WaitUntilDone()
{
    m_pimpl->m_sample_sender.WaitUntilDone();
    return *this;
}





bool ZQLoader::IsBusy() const
{
    //return m_sample_sender.IsRunning();     // stays busy during preloading attribs
    // else not busy during fun attribs so cannot stop this.
    // else double check @ signalDone not working
    return m_pimpl->m_is_busy;
}

ZQLoader & ZQLoader::SetPreload()
{
    m_pimpl->SetPreload();
    return *this;
}



bool ZQLoader::IsPreLoaded() const
{
    return m_pimpl->m_is_preloaded;
}


ZQLoader& ZQLoader::PlayleaderTone()
{
    m_pimpl->PlayleaderTone();
    return *this;
}


// Time last action took
std::chrono::milliseconds ZQLoader::GetTimeNeeded() const
{
    return m_pimpl->m_time_needed;
}



std::chrono::milliseconds ZQLoader::GetCurrentTime() const
{
    return m_pimpl->GetCurrentTime();
}

std::chrono::milliseconds ZQLoader::GetEstimatedDuration() const
{
    return m_pimpl->GetEstimatedDuration();
}

int ZQLoader::GetDurationInTStates() const
{
    return m_pimpl->m_spectrumloader.GetDurationInTStates();
}

/// path to current zqloader.exe (this program)
/// (only to help find zqloader.tap)
ZQLoader &ZQLoader::SetExeFilename(fs::path p_filename)
{
    m_pimpl->m_exe_path = std::move(p_filename);
    return *this;
}

/// Set callback when done.
ZQLoader& ZQLoader::SetOnDone(DoneFun p_fun)
{
    m_pimpl->m_OnDone = std::move(p_fun);
    return *this;
}

uint32_t ZQLoader::GetDeviceSampleRate() const
{
    return SampleSender::GetDeviceSampleRate();
}


void ZQLoader::Test()
{
    return m_pimpl->Test();
}


ZQLoader& ZQLoader::AddDataBlock(DataBlock p_block, uint16_t p_start_address)
{
    m_pimpl->AddDataBlock(std::move(p_block), p_start_address);
    return *this;
}

// static
bool ZQLoader::WriteTextToAttr(DataBlock& out_attr, const std::string& p_text, std::byte p_color, bool p_center, int p_col)
{
    return ::WriteTextToAttr(out_attr, p_text, p_color, p_center, p_col);
}


// static
void ZQLoader::Version()
{
    std::cout << "ZQLoader version " << GetVersion() << "\n";
    std::cout << 1 + &*R"(
Copyright (c) 2025 Daan Scherft [Oxidaan].
https://github.com/oxidaan/zqloader
This project uses the MIT license. See LICENSE.txt for details.
    )" << std::endl;
}
