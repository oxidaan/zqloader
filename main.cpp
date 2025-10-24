//==============================================================================
// PROJECT:         zqloader
// FILE:            main.cpp
// DESCRIPTION:     Implementation of main() / console version.
// 
// Copyright (c) 2023 Daan Scherft [Oxidaan]
// This project uses the MIT license. See LICENSE.txt for details.
//==============================================================================
// This project use the miniaudio sound library by David Read.


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


#include "tools.h"          // CommandLine
#include <iostream>
#include <filesystem>
#include "zqloader.h"
#include "loader_defaults.h"
namespace fs = std::filesystem;



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
ZQLoader version 2.0
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
    4) auto finds zqloader.tap; given file is read at turbo speed.

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
    zero_max = value        Maximum number of IN's (before an edge is seen) to be considered
                            a 'zero'.
                            Minimum value is 1 (need at least one IN to see an edge)
                            Not giving this (or 0) uses a default that worked for me (3).
    bit_loop_max = value    Maximum number of IN's without an edge seen to be considered a 
                            valid 'one'; above this a timeout error will occur.
                            Not giving this (or 0) uses a default that worked for me (100)
                            This value can safely been made larger, does not affect speeds.
                            These parameters are used at the ZX-spectrum side so must match
                            zero_tstates/one_tstates.
                            Eg extra speed: zero_max=3 one_tstates=243

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

        // either last parameter if 2 or more parameters
        // or turbofile="path/to/file" given
        fs::path filename2 = cmdline.GetParameter("turbofile", "");
        if (filename2.empty() && cmdline.GetNumParameters() >= 2)
        {
            filename2 = cmdline.GetLastParameter();
        }

        // either only parameter (if 1 given) or 2nd to last parameter if 2 or more parameters
        // or filename="path/to/file" given
        fs::path filename = cmdline.GetParameter("filename", "");
        if (filename.empty())
        {
            filename = (cmdline.GetNumParameters() == 1 && filename2.empty()) ? cmdline.GetParameter(1) :
                       (cmdline.GetNumParameters() >= 2) ? cmdline.GetParameter(cmdline.GetNumParameters() - 1):
                       "";
        }




        ZQLoader zqloader;

        // When outputfile="path/to/filename" or -w or -wav given: 
        // Convert to wav file instead of outputting sound
        zqloader.SetOutputFilename(fs::path(cmdline.GetParameter("outputfile", "")), cmdline.HasParameter("overwrite") || cmdline.HasParameter("o"));
        if(cmdline.HasParameter("wav") || cmdline.HasParameter("w"))
        {
            zqloader.SetAction(ZQLoader::Action::write_wav);
        }
        else if(cmdline.HasParameter("tzx") || cmdline.HasParameter("t"))
        {
            zqloader.SetAction(ZQLoader::Action::write_tzx);
        }

        zqloader.SetBitLoopMax(cmdline.GetParameter<int>("bit_loop_max", 0)).
                    SetZeroMax(cmdline.GetParameter<int>("zero_max", 0)).
                  SetDurations(cmdline.GetParameter("zero_tstates", 0),
                               cmdline.GetParameter("one_tstates", 0),
                               cmdline.GetParameter("end_of_byte_delay", 0));

        zqloader.SetVolume(cmdline.GetParameter("volume_left", loader_defaults::volume_left),cmdline.GetParameter("volume_right", loader_defaults::volume_right));
        // zqloader.SetCompressionType(CompressionType::none); // @DEBUG

        if(cmdline.HasParameter("usescreen") || cmdline.HasParameter("s"))
        {
            zqloader.SetSnapshotLoaderLocation(ZQLoader::LoaderLocation::screen);
        }
        else
        {
            zqloader.SetSnapshotLoaderLocation(cmdline.GetParameter<uint16_t>("new_loader_location", 0));
        }

        zqloader.SetNormalFilename(filename).SetTurboFilename(filename2);

        
        zqloader.Run();
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

