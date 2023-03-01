//==============================================================================
// PROJECT:         zqloader
// FILE:            main.cpp
// DESCRIPTION:     Implementation of main().
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================
// This project use the miniaudio sound library by David Read.


// g++ - O2 - std = c++17 - pthread zqloader.cpp - ldl - Iminiaudio


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




void Help()
{
    std::cout << 1 + &*R"(
ZQLoader

This is a turbo loader to load machine code (typical games) into a *real* ZX Spectrum at high speed.
This loader is capable of loading a 48K game in about 30 seconds. This time includes the time of 
loading the loader itself (which uses traditional ROM loader/speed) plus a splash screen.  
For it to work it is needed to connect the audio output of your computer to the ZX Spectrum EAR input.
Set the volume of your computer to maximum. Then type LOAD "" at the ZX spectrum.
This loader generates the sound pulses, that the ZX Spectrum can load as data.


syntax:
qloader.exe
    path/to/filename        First file: can be a tap or tzx file and will be
                            loaded at normal speed into a real ZX spectrum.
                            Only when the file here is 'zqloader.tap' it can
                            load the second file:
    path/to/filename        Second file, also a .tap or .tzx or a z80
                            snapshot file. When given will be send to the
                            ZX spectrum at turbo speed.
    volume                  A float between 0.0 and 1.0: sets volume.
                            Default 1.0 (max).
    invert_left             Invert the left (stereo) sound channel.
    invert_right            Invert the right (stereo) sound channel.


(C) 2023 Daan Scherft [Oxidaan].
This project uses the miniaudio libarary by David Read. (https://miniaud.io/)

)";
}

int main(int argc, char** argv)
{
    try
    {
        if (argc <= 1)
        {
            Help();
            std::cout << "Please give .tap or tzx filename." << std::endl;
            return -1;
        }

        SpectrumLoader spectrumloader;
        spectrumloader.SetSampleSender(SampleSender());
        {
            fs::path filename = argv[1];
            std::ifstream fileread(filename, std::ios::binary);
            if (!fileread)
            {
                std::cout << "File " << filename << " not found." << std::endl;
                return -2;
            }
            std::cout << "Processing file " << filename << std::endl;

            if (filename.extension() == ".tap" ||
                filename.extension() == ".TAP")
            {
                TapToSpectrumLoader loader(spectrumloader);
                loader.Load(filename, "");
            }
            if (filename.extension() == ".tzx" ||
                filename.extension() == ".TZX")
            {
                TapToSpectrumLoader loader(spectrumloader);
                TzxLoader taploader(loader);
                taploader.Load(filename, "");
            }
        }

        if (argc > 2)
        {
            fs::path filename_exp = argv[1];
            filename_exp.replace_extension("exp");      // qloader.exp (symbols)
            fs::path filename = argv[2];
            TurboBlocks tblocks(filename_exp);
            tblocks.SetCompressionType(CompressionType::automatic);
            if (false)      // test
            {
                TurboBlocks tblocks(filename_exp);
                for (int n = 0; n < 20; n++)
                {
                    Test(tblocks, filename);
                }
                tblocks.MoveToLoader(spectrumloader, TurboBlocks::ReturnToBasic);
            }
            else if (filename.extension() == ".tap" ||
                filename.extension() == ".TAP")
            {
                TapToTurboBlocks loader(tblocks);
                loader.Load(filename, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (filename.extension() == ".tzx" ||
                filename.extension() == ".TZX")
            {
                TapToTurboBlocks loader(tblocks);
                TzxLoader tzxloader(loader);
                tzxloader.Load(filename, "");
                tblocks.MoveToLoader(spectrumloader, loader.GetUsrAddress(), loader.GetClearAddress());
            }
            else if (filename.extension() == ".z80" ||
                filename.extension() == ".Z80")
            {
                Z80SnapShotLoader z80loader;
                // Read file snapshotregs.bin -> regblock
                fs::path snapshot_regs_filename = argv[1];
                snapshot_regs_filename.replace_filename("snapshotregs");
                snapshot_regs_filename.replace_extension("bin");      
                DataBlock regblock;
                regblock.LoadFromFile(snapshot_regs_filename);
                z80loader.Load(filename).SetRegBlock(std::move(regblock)).MoveToTurboBlocks(tblocks);
                tblocks.MoveToLoader(spectrumloader, z80loader.GetUsrAddress());
                //                tblocks.MoveToLoader(spectrumloader, 1);        // @DEBUG
            }
            else if (filename.extension() == ".bin" ||
                filename.extension() == ".BIN")
            {
                // @DEBUG
                auto adr = Test(tblocks, filename);
                tblocks.MoveToLoader(spectrumloader, adr);
            }
        }
        auto start = std::chrono::steady_clock::now();
        spectrumloader.Run();
        auto end = std::chrono::steady_clock::now();
        auto dura = end - start;
        std::cout << "Took: " << std::dec << std::chrono::duration_cast<std::chrono::seconds>(dura).count() << " s" << std::endl;
        std::cout << "Edge = " << spectrumloader.GetLastEdge() << std::endl;
        Key();
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: " << e.what();
        Key();
        return -1;
    }
    return 0;
}

