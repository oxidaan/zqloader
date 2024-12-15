// ==============================================================================
// PROJECT:         zqloader
// FILE:            zqloader.cpp
// DESCRIPTION:     Definition of class SampleToWav.
//
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
// ==============================================================================

#include "tools.h"
#include "zqloader.h"
#include "spectrum_loader.h"
#include "taploader.h"
#include "tzxloader.h"
#include "turboblocks.h"
#include <iostream>
#include <fstream>
#include "taptoturboblocks.h"
#include "z80snapshot_loader.h"
#include "samplesender.h"
#include "sampletowav.h"
#include "loader_defaults.h"

#include <filesystem>
namespace fs = std::filesystem;



class ZQLoader::Impl
{
public:

    Impl()  = default;
    Impl(const Impl &) = delete;
    Impl(Impl &&) = default;
    Impl & operator = (Impl &&) = default;
    Impl & operator = (const Impl &) = delete;


    void SetNormalFilename(fs::path p_filename)
    {
        m_normal_filename = std::move(p_filename);
        if(!m_is_zqloader)
        {
            m_is_zqloader = ToLower(m_normal_filename.stem().string()) == "zqloader";
        }
        if(!m_is_zqloader)
        {
            AddNormalSpeedFile(GetNormalFilename());
        }
        else
        {
            AddZqLoaderFile(GetNormalFilename());
        }
    }

    void SetTurboFilename(fs::path p_filename )
    {
        if(!p_filename.empty())
        {
            m_turbo_filename = std::move(p_filename);
            m_is_zqloader = true;
            AddTurboSpeedFile(m_turbo_filename);
        }
    }


    void SetOutputFilename(fs::path p_outputfilename)
    {
        if (ToLower(p_outputfilename.extension().string()) == ".tzx")
        {
            m_action = Action::write_tzx;
        }
        else if (ToLower(p_outputfilename.extension().string()) == ".wav")
        {
            m_action = Action::write_wav;
        }
        m_output_filename = std::move(p_outputfilename);
    }

    // Set volume left + right (-100, 100)
    void SetVolume(int p_volume_left, int p_volume_right)
    {
        m_volume_left  = p_volume_left;
        m_volume_right = p_volume_right;
        m_sample_sender.SetVolume(m_volume_left, m_volume_right);
    }


    void SetSampleRate(uint32_t p_sample_rate)
    {
        m_sample_rate = p_sample_rate;
    }


    void  SetBitLoopMax(int p_value)
    {
        m_turboblocks.SetBitLoopMax(p_value);
    }


    void SetBitOneThreshold(int p_value)
    {
        m_turboblocks.SetBitOneThreshold(p_value);
    }


    void SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
    {
        m_turboblocks.SetDurations(p_zero_duration, p_one_duration, p_end_of_byte_delay);
    }


    void SetCompressionType(CompressionType p_compression_type)
    {
        m_turboblocks.SetCompressionType(p_compression_type);
    }


    void SetAction(Action p_what)
    {
        m_action = p_what;
    }


    void SetUseScreen()
    {
        m_new_loader_location = spectrum::SCREEN_23RD;
    }


    void SetNewLoaderLocation(uint16_t p_address)
    {
        m_new_loader_location = p_address;
    }


    void SetFunAttribs(bool p_value)
    {
        m_use_fun_attribs = p_value;
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

    /// Set callback when done.
    void SetOnDone(DoneFun p_fun)
    {
        m_OnDone = std::move(p_fun);
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
//                std::cout << "Took: " << std::dec << GetCurrentTime().count() << " ms" << std::endl;
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
        }
        else if (m_action == Action::write_tzx)
        {
            // Write to tzx file
            auto outputfilename     = GetOutputFilename();
            std::ofstream filewrite = OpenFileToWrite(outputfilename, m_allow_overwrite);
            m_spectrumloader.WriteTzxFile(filewrite);
            std::cout << "Written " << outputfilename << std::endl;
        }
    }

    void AddDataBlock(DataBlock p_block, uint16_t p_start_address)
    {
        m_turboblocks.AddDataBlock(std::move(p_block), p_start_address);
        m_turboblocks.MoveToLoader(m_spectrumloader);
    }


    /// Stop/cancel playing immidiately
    /// Keeps preloaded state
    /// can cause tape loading error when not call WaitUntilDone
    void Stop()
    {
        auto is_preloaded = m_is_preloaded; // keep
        Reset();
        m_is_preloaded = is_preloaded;
    }

    /// Wait until all data have send.
    /// Note: at preloading fun attibutes this will probably 
    /// wait forever.
    void WaitUntilDone()
    {
        m_sample_sender.WaitUntilDone();
    }

    /// reset by move assigning empty/new instance.
    /// (Will stop sending immidiately).
    /// can cause tape loading error when not call WaitUntilDone
    void Reset()
    {
        auto onDone = std::move(m_OnDone);  // keep call back
        SampleSender remove = std::move(m_sample_sender);   // <- Because else move assign causes problems. Dtor target not called. So ma_device_uninit not called.
        *this = Impl();
        m_OnDone = std::move(onDone);
    }


    bool IsBusy() const
    {
        return m_sample_sender.IsRunning();     // stays busy during preloading attribs
        // else not busy during fun attribs so cannot stop this.
        //return m_is_busy;
    }

    bool IsPreLoaded() const
    {
        return m_is_preloaded;
    }

    void SetPreload()
    {
        AddZqLoaderFile(GetNormalFilename());
        m_turboblocks.MoveToLoader(m_spectrumloader);
        m_is_preloaded = true;
    }

    // Time last action took
    std::chrono::milliseconds GetTimeNeeded() const
    {
        return m_time_needed;
    }


    std::chrono::milliseconds GetCurrentTime() const
    {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    }





    /// path to current zqloader.exe (this program)
    /// (only to help find zqloader.tap)
    void SetExeFilename(fs::path p_filename)
    {
        m_exe_path = std::move(p_filename);
    }



    /// Find the path/to/zqloader.tap 
    /// Throws when not found.
    fs::path FindZqLoaderTapfile(fs::path p_filename)
    {
        if(ToLower(p_filename.stem().string()) != "zqloader")
        {
            throw std::runtime_error(1 + &*R"(
A second filename argument and/or parameters are only usefull when using zqloader.tap 
(which is the turbo loader) as (1st) file.
)");
        }
        fs::path filename = p_filename;
        if(p_filename == p_filename.filename())     // giver just zqloader.tap w/o path so try to find
        {
            // Try to find zqloader.tap file
            if (!std::filesystem::exists(p_filename))
            {
                // try at current executable directory by default
                filename = m_exe_path / p_filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // try at current executable directory/z80 by default
                filename = m_exe_path / "z80" / p_filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // then workdir
                filename = std::filesystem::current_path() / p_filename.filename();
            }
            if (!std::filesystem::exists(filename))
            {
                // then workdir/z80
                filename = std::filesystem::current_path() / "z80" / p_filename.filename();
            }
        }
        if (!std::filesystem::exists(filename))
        {
            throw std::runtime_error("ZQLoader file " + p_filename.string() + " not found");
        }
        return filename;
    }
    void Test()
    {
        extern uint16_t Test(TurboBlocks & p_blocks, fs::path p_filename);
        auto adr = Test(m_turboblocks, "");
        m_turboblocks.Finalize(adr).MoveToLoader(m_spectrumloader);
    }

private:



    std::ofstream OpenFileToWrite(fs::path p_filename, bool p_allow_overwrite)
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
    void AddNormalSpeedFile(fs::path p_filename)
    {
        if(!p_filename.empty())
        {
            std::cout << "Processing normal speed file: " << p_filename << std::endl;
            // filename is tap/tzx file to be normal loaded into the ZX Spectrum.
            if (ToLower(p_filename.extension().string()) == ".tap")
            {
                AddNormalSpeedFile<TapLoader>(p_filename, m_spectrumloader);
            }
            else if (ToLower(p_filename.extension().string()) == ".tzx")
            {
                AddNormalSpeedFile<TzxLoader>(p_filename, m_spectrumloader);
            }
            else
            {
                throw std::runtime_error("Unknown file type for filename: " + p_filename.string() + " (extension not tap / tzx)");
            }
        }
    }

    void AddZqLoaderFile(fs::path p_filename)
    {
        if(!m_is_preloaded && !m_turboblocks.IsZqLoaderAdded())
        {
            auto filename = FindZqLoaderTapfile(p_filename);
            m_turboblocks.AddZqLoader(filename);                 // zqloader.tap
        }
    }

    void AddTurboSpeedFile(fs::path p_filename)
    {
        AddZqLoaderFile(GetNormalFilename());


        if (ToLower(p_filename.extension().string()) == ".tap")
        {
            std::cout << "Processing tap file: " << p_filename << " (turbo speed)" << std::endl;
            AddFileToTurboBlocks<TapLoader>(p_filename, m_turboblocks);
        }
        else if (ToLower(p_filename.extension().string()) == ".tzx")
        {
            std::cout << "Processing tzx file: " << p_filename << " (turbo speed)" << std::endl;
            AddFileToTurboBlocks<TzxLoader>(p_filename, m_turboblocks);
        }
        else if (ToLower(p_filename.extension().string()) == ".z80" ||
                 ToLower(p_filename.extension().string()) == ".sna")
        {
            std::cout << "Processing snapshot file: " << p_filename << " (turbo speed)" << std::endl;
            AddSnapshotToTurboBlocks(p_filename, m_turboblocks);
        }
        else if(!p_filename.empty())
        {
            throw std::runtime_error("Unknown file type for filename: " + p_filename.string() + " (extension not tap / tzx / z80)");
        }

        m_turboblocks.MoveToLoader(m_spectrumloader);
    }


    
    // Add given file (tap/tzx) to given TurboBlocks so uses turbo speed
    // TLoader is TapLoader or TzxLoader
    // Finalize already called
    template<class Tloader>
    void AddFileToTurboBlocks(fs::path p_filename, TurboBlocks &p_tblocks)
    {
        TapToTurboBlocks tab_to_turbo_blocks{ p_tblocks };
        Tloader tap_or_tzx_loader;
        tap_or_tzx_loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string p_zxfilename)
                                              {
                                                  return tab_to_turbo_blocks.HandleTapBlock(std::move(p_block), p_zxfilename);
                                              }
                                              );
        tap_or_tzx_loader.Load(p_filename, "");
        p_tblocks.Finalize(tab_to_turbo_blocks.GetUsrAddress(), tab_to_turbo_blocks.GetClearAddress());
    }

    // Add given snapshot file (z80/sna) to given TurboBlocks so uses turbo speed
    // Finalize already called
    void AddSnapshotToTurboBlocks(fs::path p_filename, TurboBlocks &p_tblocks)
    {
        SnapShotLoader snapshotloader;
        // Read file snapshotregs.bin (created by sjasmplus) -> regblock
        fs::path snapshot_regs_filename = FindZqLoaderTapfile(GetNormalFilename());
        snapshot_regs_filename.replace_filename("snapshotregs");
        snapshot_regs_filename.replace_extension("bin");
        DataBlock regblock;
        regblock.LoadFromFile(snapshot_regs_filename);
        if(!p_tblocks.IsZqLoaderAdded())      // else already done at AddZqLoader 
        {
            fs::path filename_exp = FindZqLoaderTapfile(GetNormalFilename());
            filename_exp.replace_extension("exp");          // zqloader.exp (symbols)
            p_tblocks.SetSymbolFilename(filename_exp);      
        }
        snapshotloader.Load(p_filename).SetRegBlock(std::move(regblock)).MoveToTurboBlocks(p_tblocks, m_new_loader_location, m_use_fun_attribs);
        p_tblocks.Finalize(snapshotloader.GetUsrAddress());
    }



    // Add given file (tap/tzx) to given SpectrumLoader so uses normal speed
    // TLoader is TapLoader or TzxLoader
    template<class Tloader>
    void AddNormalSpeedFile(fs::path p_filename, SpectrumLoader &p_spectrum_loader)
    {
        Tloader tap_or_tzx_loader;
        tap_or_tzx_loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string)
                                              {
                                                  p_spectrum_loader.AddLeaderPlusData(std::move(p_block), spectrum::g_tstate_quick_zero, 1750ms); // .AddPause(100ms);
                                                  return false;
                                              }
                                              );
        tap_or_tzx_loader.Load(p_filename, "");
    }




    
    void Check()
    {
        if(m_is_zqloader && m_turbo_filename.empty() && !m_is_preloaded)
        {
            throw std::runtime_error(1 + &*R"(
When using zqloader.tap a 2nd filename is needed as runtime argument,
with the program to be turboloaded. A game for example. 
Else the ZX Spectrum will not do anything after loading the turbo loader,
except waiting.
)");
        }

        if(m_turbo_filename.empty() && GetNormalFilename().empty())
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

    fs::path GetNormalFilename() const
    {
        if(m_is_zqloader && m_normal_filename.empty())
        {
            return "zqloader.tap";  // will try to find path
        }
        return m_normal_filename;
    }


private:

    SpectrumLoader                          m_spectrumloader;
    TurboBlocks                             m_turboblocks;
    SampleSender                            m_sample_sender;

    uint16_t                                m_new_loader_location = 0; // 0 = auto
    bool                                    m_use_fun_attribs     = false;
    int                                     m_volume_left         = loader_defaults::volume_left;
    int                                     m_volume_right        = loader_defaults::volume_right;
    int                                     m_sample_rate         = loader_defaults::sample_rate;
    bool                                    m_allow_overwrite     = false; // see OpenFileToWrite
    fs::path                                m_normal_filename;             // usually zqloader.tap
    fs::path                                m_turbo_filename;
    fs::path                                m_output_filename;             // writing wav or tzx
    fs::path                                m_exe_path;                    // s/a argv[0]
    Action                                  m_action = Action::play_audio;

    bool                                    m_is_busy = false;
    bool                                    m_is_zqloader         = false; // zqloader.tap seen
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



ZQLoader& ZQLoader::SetNormalFilename(fs::path p_filename)
{
    m_pimpl->SetNormalFilename(std::move(p_filename));
    return *this;
}



ZQLoader& ZQLoader::SetTurboFilename(fs::path p_filename)
{
    m_pimpl->SetTurboFilename(std::move(p_filename));
    return *this;
}



ZQLoader& ZQLoader::SetOutputFilename(fs::path p_filename)
{
    m_pimpl->SetOutputFilename(std::move(p_filename));
    return *this;
}



ZQLoader& ZQLoader::SetVolume(int p_volume_left, int p_volume_right)
{
    m_pimpl->SetVolume(p_volume_left, p_volume_right);
    return *this;
}



ZQLoader& ZQLoader::SetSampleRate(uint32_t p_sample_rate)
{
    m_pimpl->SetSampleRate(p_sample_rate);
    return *this;
}



ZQLoader& ZQLoader::SetBitLoopMax(int p_value)
{
    m_pimpl->SetBitLoopMax(p_value);
    return *this;
}



ZQLoader& ZQLoader::SetBitOneThreshold(int p_value)
{
    m_pimpl->SetBitOneThreshold(p_value);
    return *this;
}



ZQLoader& ZQLoader::SetDurations(int p_zero_duration, int p_one_duration, int p_end_of_byte_delay)
{
    m_pimpl->SetDurations(p_zero_duration, p_one_duration, p_end_of_byte_delay);
    return *this;
}



ZQLoader& ZQLoader::SetCompressionType(CompressionType p_compression_type)
{
    m_pimpl->SetCompressionType(p_compression_type);
    return *this;
}



ZQLoader& ZQLoader::SetAction(Action p_what)
{
    m_pimpl->SetAction(p_what);
    return *this;
}



ZQLoader& ZQLoader::SetUseScreen()
{
    m_pimpl->SetUseScreen();
    return *this;
}



ZQLoader& ZQLoader::SetNewLoaderLocation(uint16_t p_address)
{
    m_pimpl->SetNewLoaderLocation(p_address);
    return *this;
}



ZQLoader& ZQLoader::SetFunAttribs(bool p_value)
{
    m_pimpl->SetFunAttribs(p_value);
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


ZQLoader& ZQLoader::WaitUntilDone()
{
    m_pimpl->WaitUntilDone();
    return *this;
}





bool ZQLoader::IsBusy() const
{
    return m_pimpl->IsBusy();
}

ZQLoader & ZQLoader::SetPreload()
{
    m_pimpl->SetPreload();
    return *this;
}

bool ZQLoader::IsPreLoaded() const
{
    return m_pimpl->IsPreLoaded();
}


ZQLoader& ZQLoader::PlayleaderTone()
{
    m_pimpl->PlayleaderTone();
    return *this;
}



std::chrono::milliseconds ZQLoader::GetTimeNeeded() const
{
    return m_pimpl->GetTimeNeeded();
}



std::chrono::milliseconds ZQLoader::GetCurrentTime() const
{
    return m_pimpl->GetCurrentTime();
}

std::chrono::milliseconds ZQLoader::GetEstimatedDuration() const
{
    return m_pimpl->GetEstimatedDuration();
}


ZQLoader &ZQLoader::SetExeFilename(fs::path p_filename)
{
    m_pimpl->SetExeFilename(std::move(p_filename));
    return *this;
}

/// Set callback when done.

ZQLoader& ZQLoader::SetOnDone(DoneFun p_fun)
{
    m_pimpl->SetOnDone(std::move(p_fun));
    return *this;
}

int ZQLoader::GetDeviceSampleRate() const
{
    return SampleSender::GetDeviceSampleRate();
}

std::filesystem::path ZQLoader::GetZqLoaderFile() const
{
    return m_pimpl->FindZqLoaderTapfile("zqloader.tap");
}

void ZQLoader::Test()
{
    return m_pimpl->Test();
}

// static
bool ZQLoader::WriteTextToAttr(DataBlock& out_attr, const std::string& p_text, std::byte p_color, bool p_center, int p_col)
{
    return ::WriteTextToAttr(out_attr, p_text, p_color, p_center, p_col);
}

ZQLoader& ZQLoader::AddDataBlock(DataBlock p_block, uint16_t p_start_address)
{
    m_pimpl->AddDataBlock(std::move(p_block), p_start_address);
    return *this;
}