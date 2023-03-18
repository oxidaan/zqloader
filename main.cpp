//==============================================================================
// PROJECT:         zqloader
// FILE:            main.cpp
// DESCRIPTION:     Implementation of main().
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================
// This project use the miniaudio sound library by David Read.


#if __cplusplus < 201703L
// At MSVC
// Properties -> C/C++ -> Language -> C++ Language Standard -> ISO c++17 Standard
// also set at C / C++ -> Command Line -> Additional options : /Zc:__cplusplus
// plus C / C++ -> Preprocessor -> Use standard comforming preprocessor
#error "Need c++17 or more"
#endif


#ifdef _WIN32
#include <conio.h>
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4456) // declaration of '' hides previous local declaration
#endif


#include "spectrum_loader.h"
#include "taptospectrumloader.h"
#include "tzxloader.h"
#include "taptoturboblocks.h"
#include "z80snapshot_loader.h"
#include "turboblock.h"
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


void Version()
{
    std::cout << 1 + &*R"(
ZQLoader version 0.1
(C) 2023 Daan Scherft[Oxidaan].
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
1) zqloader.exe [options] path/to/filename1 
2) zqloader.exe [options] path/to/zqloader.tap path/to/turbofile
3) zqloader.exe [options] filename="path/to/zqloader.tap" turbofile="path/to/turbofile" 
options:
    path/to/filename1       First file: can be a tap or tzx file and will be
                            loaded at normal speed into a real ZX spectrum.
                            Only when the file here is 'zqloader.tap' it can
                            load the second file:
    path/to/turbofile       Second file, also a .tap or .tzx or a z80
                            snapshot file. A game for example.
                            When given will be send to the
                            ZX spectrum at turbo speed.
    volume_left = value            
    volume_right = value    A number between -100 and 100: sets volume for left or right 
                            sound (stereo) channel.
                            Default 100 (max). A negative value eg -100 inverts this channel.
    samplerate = value      Samplerate for audio. Default 0 meaning take device native sample rate.
                            S/a miniadio documentation.
    zero_tstates = value
    one_tstates = value     The number of TStates a zero / one pulse will take when using the 
                            zqloader/turboloader. Not giving this (or 0) uses a default that 
                            worked for me.
    key = yes/no/error      When done wait for key: always, never or only when an error occurred.
    )" << std::endl;
 //   Version();
}



int main(int argc, char** argv)
{
    int er = -1;
    CommandLine cmdline(argc, argv);
    try
    {
        if (argc <= 1 || cmdline.TryGetParameter("--help") || cmdline.TryGetParameter("-h"))
        {
            Help();
            throw std::runtime_error("Please give a .tap or .tzx filename as runtime argument.");
        }
        if (cmdline.TryGetParameter("--version") || cmdline.TryGetParameter("-v"))
        {
            Version();
            return 0;
        }

        // either only parameter (if 1 given) or 2nd to last parameter if 2 or more parameters
        // or filename="path/to/file" given
        fs::path filename = cmdline.GetParameter("filename", "");
        if (filename.empty())
        {
            filename = (cmdline.GetNumParameters() == 1) ? cmdline.GetParameter(1) : cmdline.GetParameter(cmdline.GetNumParameters() - 1);
        }
         
        SampleSender sample_sender;
        auto vol1 = cmdline.GetParameter("volume_left", 100);
        auto vol2 = cmdline.GetParameter("volume_right", 100);
        auto samplerate = cmdline.GetParameter("samplerate", 0);
        sample_sender.SetVolume(vol1, vol2).SetSampleRate(samplerate);
        SpectrumLoader spectrumloader;
        spectrumloader.SetSampleSender(std::move(sample_sender));
        {
            std::ifstream fileread(filename, std::ios::binary);
            if (!fileread)
            {
                // try at executable directory by default 
                filename = fs::path(argv[0]).parent_path() / filename;
                fileread.open(filename, std::ios::binary);
            }
            if (!fileread)
            {
                throw std::runtime_error("File " + filename.string() + " not found.");
            }
            if (ToLower(filename.stem().string()) == "zqloader" && argc <= 2)
            {
                throw std::runtime_error(1 + &*R"(
When using zqloader.tap a 2nd filename is needed as runtime argument,
with the program to be turboloaded. A game for example. 
Else the ZX Spectrum will not do anything after loading the turbo loader,
except waiting.
)");
            }
            std::cout << "Processing file " << filename << std::endl;

            if (ToLower(filename.extension().string()) == ".tap" )
            {
                TapToSpectrumLoader loader(spectrumloader);
                loader.Load(filename, "");
            }
            if (ToLower(filename.extension().string()) == ".tzx")
            {
                TapToSpectrumLoader loader(spectrumloader);
                TzxLoader taploader(loader);
                taploader.Load(filename, "");
            }
        }

        // either last parameter if 2 or more parameters
        // or turbofile="path/to/file" given
        fs::path filename2 = cmdline.GetParameter("turbofile", "");
        if (filename2.empty())
        {
            filename2 = cmdline.GetLastParamer();
        }

        if (!filename2.empty())
        {
            if (ToLower(filename.stem().string()) != "zqloader")
            {
                throw std::runtime_error(1 + &*R"(
A second filename argument is only usefull when using zqloader.tap 
(which is the turbo loader) as 1st file.");
)");
            }
            fs::path filename_exp = filename;
            filename_exp.replace_extension("exp");      // zqloader.exp (symbols)

            TurboBlocks tblocks(filename_exp);
            tblocks.SetCompressionType(CompressionType::automatic);
            auto zero_tstates = cmdline.GetParameter("zero_tstates", 0);       // 0 is default
            auto one_tstates = cmdline.GetParameter("one_tstates", 0);         // 0 is default
            tblocks.SetDurations(zero_tstates, one_tstates);
            std::cout << "Processing turbo file " << filename2 << std::endl;
/*
            if (false)      // test
            {
                TurboBlocks tblocks(filename_exp);
                for (int n = 0; n < 20; n++)
                {
                    Test(tblocks, filename);
                }
                tblocks.MoveToLoader(spectrumloader, TurboBlocks::ReturnToBasic);
            }
            else 
*/
            if (ToLower(filename2.extension().string()) == ".tap")
            {
                TapToTurboBlocks loader(tblocks);
                loader.Load(filename2, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (ToLower(filename2.extension().string()) == ".tzx")
            {
                TapToTurboBlocks loader(tblocks);
                TzxLoader tzxloader(loader);
                tzxloader.Load(filename2, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (ToLower(filename2.extension().string()) == ".z80")
            {
                Z80SnapShotLoader z80loader;
                // Read file snapshotregs.bin -> regblock 
                fs::path snapshot_regs_filename = filename; 
                snapshot_regs_filename.replace_filename("snapshotregs");
                snapshot_regs_filename.replace_extension("bin");      
                DataBlock regblock;
                regblock.LoadFromFile(snapshot_regs_filename);
                z80loader.Load(filename2).SetRegBlock(std::move(regblock)).MoveToTurboBlocks(tblocks);
                tblocks.MoveToLoader(spectrumloader, z80loader.GetUsrAddress());
                //                tblocks.MoveToLoader(spectrumloader, 1);        // @DEBUG
            }
            else if (ToLower(filename2.extension().string()) == ".bin")
            {
                // @DEBUG
                auto adr = Test(tblocks, filename2);
                tblocks.MoveToLoader(spectrumloader, adr);
            }
        }
        auto start = std::chrono::steady_clock::now();
        spectrumloader.Run();
        auto end = std::chrono::steady_clock::now();
        auto dura = end - start;
        std::cout << "Took: " << std::dec << std::chrono::duration_cast<std::chrono::seconds>(dura).count() << " s" << std::endl;
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

