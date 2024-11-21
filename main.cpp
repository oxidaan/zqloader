//==============================================================================
// PROJECT:         zqloader
// FILE:            main.cpp
// DESCRIPTION:     Implementation of main().
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================
// This project use the miniaudio sound library by David Read.

//  
// "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\Z80\jsw2.z80"
// volume_right = -100 usescreen "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\games beetzart\ZX32\ZX Spectrum Files\Golden Oldies\Knight Lore.z80"
// ZZvolume_right = -100 "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\games beetzart\Z80\download\SABRWULF.tap"
// "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\zx32\GALAXINS.TAP"
// bit_one_threshold=4 zero_tstates=110 one_tstates=243 "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\games beetzart\Z80\download\SABRWULF.tap"
// ZZvolume_right = -100 "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\z80\test.z80"
// ZZvolume_right = 100 ZZvolume_left = -100 "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\z80\test.z80"
// ZZvolume_right = 100 ZZvolume_left = -100 "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap"  "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader_test.bin"
// ZZvolume_left=100 ZZvolume_right=100 bit_one_threshold=3 one_tstates=243 
// "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Users\Daan\Downloads\MarioBros(ErbeSoftwareS.A.).tzx\Mario Bros (Erbe).tzx"
// "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\Z80\mm.sna"
// "C:\Projects\Visual Studio\Projects\zqloader\z80\zqloader.tap" "C:\Games\Z80\jsw3.z80"
#if __cplusplus < 201703L
// At MSVC
// Properties -> C/C++ -> Language -> C++ Language Standard -> ISO c++17 Standard
// also set at C / C++ -> Command Line -> Additional options : /Zc:__cplusplus
// plus C / C++ -> Preprocessor -> Use standard conforming preprocessor
#error "Need c++17 or more"
#endif


#ifdef _WIN32
#include <conio.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#endif


#include "spectrum_loader.h"
#include "tzxloader.h"
#include "taptoturboblocks.h"
#include "z80snapshot_loader.h"
#include "turboblocks.h"
#include "samplesender.h"
#include "sampletowav.h"
#include "tools.h"
#include <iostream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

uint16_t Test(TurboBlocks& p_blocks, fs::path p_filename);


#ifndef _WIN32
#include <termios.h>
#include <unistd.h>     // STDIN_FILENO
int Key()
{
    struct termios prev, newatt;
    tcgetattr(STDIN_FILENO, &prev);
    newatt = prev;
    newatt.c_lflag &= ~ECHO;
    newatt.c_lflag &= ~ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &newatt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &prev);
    return ch;
}
#else
int Key()
{
    return ::_getch();
}
#endif

inline std::ofstream OpenFileToWrite(fs::path p_filename, bool p_allow_overwrite)
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

void Version()
{
    std::cout << 1 + &*R"(
ZQLoader version 1.1
(C) 2024 Daan Scherft[Oxidaan].
https://github.com/oxidaan/zqloader
This project uses the miniaudio library by David Read. (https://miniaud.io/)
    )" << std::endl;
}


void Help()
{
    Version();
    std::cout << 1 + &*R"(
This is a turbo loader to load machine code (like games) into a *real* ZX Spectrum at high speed.
This loader is capable of loading a 48K game in about 30 seconds. This time includes the time of 
loading the loader itself (which uses traditional ROM loader/speed) plus a splash screen.  
For it to work it is needed to connect the audio output of your computer to the ZX Spectrum EAR input.
Set the volume of your computer to maximum. Make sure no other sound is playing.
Then type LOAD "" at the ZX spectrum.
This loader generates the sound pulses, that the ZX Spectrum can load as data.
First it will load a turbo loader program to the ZX Spectrum. Then using that it will load
the second given file with turbo speed.


Syntax:
1) zqloader.exe [options] path/to/filename 
2) zqloader.exe [options] path/to/zqloader.tap path/to/turbofile
3) zqloader.exe [options] filename="path/to/zqloader.tap" turbofile="path/to/turbofile" option=value
4) zqloader.exe [options] turbofile="path/to/turbofile" option=value
Arguments:
    path/to/filename        First file: can be a .tap or .tzx file and will be
                            loaded at normal speed into a real ZX spectrum.
                            Only when the file here is 'zqloader.tap' it can
                            load the second file:
    path/to/turbofile       Second file, also a .tap or .tzx or a .z80 (snapshot) file.
                            A game for example.
                            When given will be send to the ZX spectrum at turbo speed.

More options can be given with syntax: option=value, or just 'option value' or option="some value" or
'--option=value' :
    volume_left = value            
    volume_right = value    A number between -100 and 100: sets volume for left or right 
                            sound (stereo) channel.
                            Default 100 (max). A negative value eg -100 inverts this channel.
                            When both are negative both channels are inverted.
    samplerate = value      Sample rate for audio. Default 0 meaning take device native sample rate.
                            S/a miniaudio documentation.
    usescreen or -s         When loading a snapshot, normally it will try to find empty space for
                            the loader. Only when not found it uses the lower 2/3 of screen for 
                            that. With this option it will allways use the screen.
    fun_attribs or -f       When using screen for a snapshot, giving this paremeter overwrites
                            the loader garbage at screen with a funny text attribute text.

    zero_tstates = value
    one_tstates = value     The number of TStates a zero / one pulse will take when using the 
                            zqloader/turboloader. Not giving this (or 0) uses a default that 
                            worked for me (91/241). 
                            These parameters are used at the host side so directly effect 
                            loading speed.
    end_of_byte_delay = value
                            Extra delay in TSTates after each byte. Eg time needed to store
                            the byte, calculate checksum etc. Not giving this (or 0) uses a 
                            default that worked for me (64). 
    bit_one_threshold = value
                            A time value in 50xTStates used at Z80 turboloader indicating the
                            time between edges when it is considered a 'one' - below this time
                            it is considered a 'zero'. Related to 'one_tstates' above.
                            Not giving this (or 0) uses a default that worked for me (4) - 
                            (or automatically calculated out of bit_loop_max).
    bit_loop_max = value    A time value in 50xTstates used at Z80 turboloader indicating the
                            maximum time between edges treated as valid 'one' value. Above this 
                            a timeout error will occur.
                            Not giving this (or 0) uses a default that worked for me (12)
                            These parameters are used at the ZX-spectrum side so must match
                            zero_tstates/one_tstates.
                            Eg extra speed: bit_one_threshold=3 one_tstates=243

    outputfile="path/to/filename.wav"
                            When a wav file is given: write result to given WAV (audio) file instead of 
                            playing sound.
    outputfile="path/to/filename.tzx"
                            When a tzx file given: write result as tzx file instead of playing sound. **
    wav or -w               Write a wav file as above with same as turbo (2nd) filename but with wav extension.
    tzx or -t               Write a tzx file as above with same as turbo (2nd) filename but with tzx extension. **
    overwrite or -o         When given allows overwriting above output file when already exists, else gives
                            error in that case. Eg: -wo create wav file, overwrite previous one.

    key = yes/no/error      When done wait for key: yes=always, no=never or only when an error
                            occurred (which is the default).
    **) tzx files is experimental and not fully tested. It uses 'ID 19 - Generalized Data Block' a lot but I
        can't find a tool that can actually play it.
    )" << std::endl;
 //   Version();
}



int main(int argc, char** argv)
{
    int er = -1;
    CommandLine cmdline(argc, argv);
    try
    {
        if (!cmdline.HasParameters())
        {
            Help();
            throw std::runtime_error("Please give a .tap or .tzx filename as runtime argument.");
        }
        if ( cmdline.HasParameter("help") || cmdline.HasParameter("h"))
        {
            Help();
            return 0;    
        }
        Version();
        if (cmdline.HasParameter("version") || cmdline.HasParameter("v"))
        {
            return 0;
        }

        // either only parameter (if 1 given) or 2nd to last parameter if 2 or more parameters
        // or filename="path/to/file" given
        fs::path filename = cmdline.GetParameter("filename", "");
        if (filename.empty())
        {
            filename = (cmdline.GetNumParameters() == 1) ? cmdline.GetParameter(1) : cmdline.GetParameter(cmdline.GetNumParameters() - 1);
        }


         

        // either last parameter if 2 or more parameters
        // or turbofile="path/to/file" given
        fs::path filename2 = cmdline.GetParameter("turbofile", "");
        if (filename2.empty() && cmdline.GetNumParameters() >= 2)
        {
            filename2 = cmdline.GetLastParameter();
        }

        bool is_zqloader = ToLower(filename.stem().string()) == "zqloader";


        if ((is_zqloader && filename2.empty()) || ToLower(filename2.stem().string()) == "zqloader")
        {
            throw std::runtime_error(1 + &*R"(
When using zqloader.tap a 2nd filename is needed as runtime argument,
with the program to be turboloaded. A game for example. 
Else the ZX Spectrum will not do anything after loading the turbo loader,
except waiting.
)");
        }
        if (!is_zqloader && !filename2.empty())
        {
            filename = "zqloader.tap";
            is_zqloader = true;
//            throw std::runtime_error(1 + &*R"(
//A second filename argument and/or parameters are only usefull when using zqloader.tap 
//(which is the turbo loader) as (1st) file.
//)");
        }

        // Check if file exist if not try at current executable directory
        if (!std::filesystem::exists(filename))
        {
            // try at current executable directory by default 
            filename = fs::path(argv[0]).parent_path() / filename;
            // (if still not exist will throw anyway)
        }

        SpectrumLoader spectrumloader;

        if(!is_zqloader)
        {
            std::cout << "Processing normal speed file: " << filename << std::endl;
            // filename is tap/tzx file to be normal loaded into the ZX Spectrum.
            if (ToLower(filename.extension().string()) == ".tap")
            {
                TapLoader taploader;
                taploader.SetOnHandleTapBlock([&](DataBlock p_block, std::string)
                {
                    spectrumloader.AddLeaderPlusData(std::move(p_block), spectrum::g_tstate_quick_zero, 1750ms);//.AddPause(100ms);
                    return false;
                }
                );
                taploader.Load(filename, "");
            }
            if (ToLower(filename.extension().string()) == ".tzx")
            {
                TzxLoader tzxloader;
                tzxloader.SetOnHandleTapBlock([&](DataBlock p_block, std::string)
                {
                    spectrumloader.AddLeaderPlusData(std::move(p_block), spectrum::g_tstate_quick_zero, 1750ms);//.AddPause(100ms);
                    return false;
                }
                );
                tzxloader.Load(filename, "");
            }
            else
            {
                throw std::runtime_error("Unknown file type for filename: " + filename.string() + " (extension not tap / tzx)");
            }
        }
        else    // is zqloader
        {   
            std::cout << "Processing zqloader file: " << filename << " (normal speed)" << std::endl;
            // filename2 is the tap/tzx/z80 file to be turbo-loaded into the ZX Spectrum.

            fs::path filename_exp = filename;
            filename_exp.replace_extension("exp");      // zqloader.exp (symbols)

            auto zero_tstates = cmdline.GetParameter("zero_tstates", 0);                  // 0 is default
            auto one_tstates = cmdline.GetParameter("one_tstates", 0);                    // 0 is default
            auto end_of_byte_delay = cmdline.GetParameter("end_of_byte_delay", 0);        // 0 is default
            auto bit_loop_max = cmdline.GetParameter<int>("bit_loop_max", 0);             // When 0 dont overwrite default @ zqloader.z80asm
            auto bit_one_threshold = cmdline.GetParameter<int>("bit_one_threshold", 0);   // When 0 dont overwrite default @ zqloader.z80asm

            TurboBlocks tblocks{filename_exp};
            tblocks.SetCompressionType(CompressionType::automatic).
                    SetDurations(zero_tstates, one_tstates, end_of_byte_delay).
                    SetBitLoopMax(bit_loop_max).SetBitOneThreshold(bit_one_threshold).
                    Load(filename, "");     // zqloader.tap
            if (cmdline.HasParameter("test"))
            {
                auto adr = Test(tblocks, "");
                tblocks.Finalyze(adr).MoveToLoader(spectrumloader);
            }
            else if (ToLower(filename2.extension().string()) == ".tap")
            {
                std::cout << "Processing tap file: " << filename2 << " (turbo speed)" << std::endl;
                TapToTurboBlocks tab_to_turbo_blocks{tblocks};
                TapLoader loader;
                loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string p_zxfilename)
                {
                    return tab_to_turbo_blocks.HandleTapBlock(std::move(p_block), p_zxfilename);
                }
                );                
                loader.Load(filename2, "");
                tblocks.Finalyze(tab_to_turbo_blocks.GetUsrAddress(), tab_to_turbo_blocks.GetClearAddress()).MoveToLoader(spectrumloader);
            }
            else if (ToLower(filename2.extension().string()) == ".tzx")
            {
                std::cout << "Processing tzx file: " << filename2 << " (turbo speed)" << std::endl;

                TapToTurboBlocks tab_to_turbo_blocks{tblocks};
                TzxLoader loader;
                loader.SetOnHandleTapBlock([&](DataBlock p_block, std::string p_zxfilename)
                {
                    return tab_to_turbo_blocks.HandleTapBlock(std::move(p_block), p_zxfilename);
                }
                );
                loader.Load(filename2, "");
                tblocks.Finalyze(tab_to_turbo_blocks.GetUsrAddress(), tab_to_turbo_blocks.GetClearAddress()).MoveToLoader(spectrumloader);
            }
            else if (ToLower(filename2.extension().string()) == ".z80" ||
                     ToLower(filename2.extension().string()) == ".sna")
            {
                std::cout << "Processing snapshot file: " << filename2 << " (turbo speed)" << std::endl;

                Z80SnapShotLoader snapshotloader;
                // Read file snapshotregs.bin (created by sjasmplus) -> regblock 
                fs::path snapshot_regs_filename = filename; 
                snapshot_regs_filename.replace_filename("snapshotregs");
                snapshot_regs_filename.replace_extension("bin");      
                DataBlock regblock;
                regblock.LoadFromFile(snapshot_regs_filename);
                bool allways_use_screen = cmdline.HasParameter("usescreen")  || cmdline.HasParameter("s");
                bool write_fun_attribs =  cmdline.HasParameter("fun_attribs")|| cmdline.HasParameter("f");
                snapshotloader.Load(filename2).SetRegBlock(std::move(regblock)).MoveToTurboBlocks(tblocks, allways_use_screen, write_fun_attribs);
                tblocks.Finalyze(snapshotloader.GetUsrAddress()).MoveToLoader(spectrumloader);
                //                tblocks.MoveToLoader(spectrumloader, 1);        // @DEBUG
            }
            else if (ToLower(filename2.extension().string()) == ".bin")
            {
                // @DEBUG
                auto adr = Test(tblocks, filename2);
                tblocks.Finalyze(adr).MoveToLoader(spectrumloader);
            }
            else
            {
                throw std::runtime_error("Unknown file type for filename: " + filename2.string() + " (extension not tap / tzx / z80)");
            }

        }

        // When outputfile="path/to/filename" or -w or -wav given: 
        // Convert to wav file instead of outputting sound
        fs::path outputfilename = cmdline.GetParameter("outputfile", "");
        if(outputfilename.empty() && (cmdline.HasParameter("wav") || cmdline.HasParameter("w")))
        {
            // When -w or -wav is given create output filename out of 2nd given filename (filename2)
            if( !filename2.empty() && ToLower(filename2.extension().string()) != ".wav")
            {
                outputfilename = filename2;
                outputfilename.replace_extension("wav");
            }
        }
        // same for tzx
        if(outputfilename.empty() && (cmdline.HasParameter("tzx") || cmdline.HasParameter("t")))
        {
            if (!filename2.empty() && ToLower(filename2.extension().string()) != ".tzx")
            {
                outputfilename = filename2;
                outputfilename.replace_extension("tzx");
            }
        }
        auto vol1 = cmdline.GetParameter("volume_left", 100);
        auto vol2 = cmdline.GetParameter("volume_right", 100);
        auto samplerate = cmdline.GetParameter("samplerate", 0);
        auto start = std::chrono::steady_clock::now();      // to measure duration only
        if(outputfilename.empty())
        {
            // normal, so play as sound
            SampleSender sample_sender;
            spectrumloader.Attach(sample_sender);
            sample_sender.SetVolume(vol1, vol2).SetSampleRate(samplerate);
            sample_sender.Run();            // Play!
        }
        else if (ToLower(outputfilename.extension().string()) == ".wav")
        {
            // Write to wav file
            std::ofstream filewrite = OpenFileToWrite(outputfilename, cmdline.HasParameter("overwrite") || cmdline.HasParameter("o"));
            SampleToWav wav_writer;
            spectrumloader.Attach(wav_writer);
            wav_writer.SetVolume(vol1, vol2).SetSampleRate(samplerate);
            wav_writer.WriteToFile(filewrite);
            std::cout << "Written " << outputfilename << " with size: " << wav_writer.GetSize() << " and duration: " << wav_writer.GetDuration().count() << "s"  << std::endl;
        }
        else if (ToLower(outputfilename.extension().string()) == ".tzx")
        {
            // Write to tzx file
            std::ofstream filewrite = OpenFileToWrite(outputfilename, cmdline.HasParameter("overwrite") || cmdline.HasParameter("o"));
            spectrumloader.WriteTzxFile(filewrite);
            std::cout << "Written " << outputfilename << std::endl;
        }

        auto end = std::chrono::steady_clock::now();
        auto dura = end - start;
        std::cout << "Took: " << std::dec << std::chrono::duration_cast<std::chrono::milliseconds>(dura).count() << " ms" << std::endl;
      //  std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
        er = 0;
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what() << std::endl;
    }
    auto key = cmdline.GetParameter("key", "error");
    if( key == "yes" ||
       (key == "error" && er < 0))
    {
        std::cout << "Key..." << std::endl;
        Key();
    }
    return er;
}

