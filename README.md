ZQloader
====

This is a turbo loader used to load machine code games into a **real** unmodified ZX Spectrum at high speed.  

This loader is capable of loading a 48K game in about *25-30 seconds*. (Jetset Willy snapshot in 19 seconds!). This time includes the time of loading the loader itself (which uses traditional ROM loader/speed) plus a splash screen.  

See demo at https://www.youtube.com/watch?v=mMA2A-ZmxiA

The idea is that when using a computer to generate the loading-sounds a much higher speed can be achieved as compared to the old tapes. After all the accuracy when generating loading sounds from a computer is much higher than those old wobbly tape recorders.   

The loader is coded into a BASIC `REM` statement to have it loaded in one step, thus avoiding the need to load an additional machine code block (which would take extra time). At the ZX Spectrum all you have to type is `LOAD ""`.

To speed up even more data is compressed before loading.

The project code is portable and runs under both Windows and Linux.

Obviously this project has lots in common with [Otla](https://github.com/sweetlilmre/otla). Actually I was at 2/3 of developing this when I found out about Otla....  
However Otla seems barely maintained and uses CppBuilder - I think. Unlike Otla this project can also be compiled for **Linux**. And unlike Otla it uses compression to speed up even more.


Project
----
There are two parts: 

The Z80 assembled ZQloader.
--
This is the turbo loader running at the ZX spectrum, written in Z80 assembly. It is stored entirely into a BASIC `REM` statement, so it can be loaded in just one step, and all you have to type is `LOAD ""`.  

Once loaded it copies itself to upper memory regions because lower RAM is [contended](https://en.wikipedia.org/wiki/Contended_memory) and not quick/stable enough for loading algortihms. Then it starts waiting for incoming turbo blocks.
More about the [Z80 assembled ZQloader]


zqloader.exe coded in C++ (17) 
---
Runs on the host computer. It uses [miniaudio](https://github.com/mackron/miniaudio) to generated loading sounds.  
First it uploads the ZQloader machine code (described above) itself to the ZX spectrum. It uses traditional ROM speed for that.

After that it will read a TAP, TZX or Z80/sna (snapshot) file. For TAP or TZX files it will recognize a basic loader, taking the start address from that (as in `RANDOMIZE USR XXXX`) (also read `CLEAR` value, if any). Then it will load 1 or more code blocks, which can be a spash screen or machine code, compress them, and send these at turbo speed into the ZX spectrum. 
More about the [C++ ZQloader...]


Compression
---
It uses a simple RLE compression algorithm that only reduces size with 20-40% thereabout. This is not that much (eg [ZX0](https://github.com/einar-saukas/ZX0) should do much better). But essential is it can be decompressed at the ZX spectrum at he same memory block - so decompressed data will overwrite compressed data during decompression. ZX0 can do this also but seems to always need to have some minimal extra space at the end (see `delta` at [ZX0 readme](https://github.com/einar-saukas/ZX0#readme)). Also I've found compressing takes long with ZX0, thus spoiling the entire idea of a quick loader.  
The RLE compression algorithm compresses the most used byte value (usually this is 0) by writing an escape code, then the number of 'most used value'-s. Further it compresses a sequence of 3 or more the same bytes (of any value) by writing another escape code, then that byte value, then the number of these bytes.  

The compression is most effective when loading a snapshot. This is because when loading a snapshot, often large parts of the memory are zero which can be compressed effectively. Therefore loading a 48kb snapshot containing a 32kb game takes only little more as loading just the 32kb game itself: around 30 seconds (jsw even < 20 seconds!).

Limitations/TODO's
---

* It is only tested with 48K ZX Spectrum! Because thats all I have to test it with! (if you have it working on a 128K or any other ZX Spectrum let me know!...).

*When using TAP or TZX files:*

* It can only load games (or applications) that are in machine code, not BASIC. Any BASIC is ignored. For BASIC to work it would require ZQLoader to parse the BASIC and remove any LOAD/CLEAR/USR statements (because it does so itself) and still run the rest. So it must somehow recognise if BASIC is just a loader (that can be skipped) or there is more to it.  
It therefore ignores any BASIC when processing a TAP or TZX file, except trying to find the CLEAR and USR adresses in it.
**Of course with a snapshot it can still load any BASIC program.**

* It cannot load games that have any kind of copy protection. Or does any loading without BASIC. Eg *Horace and the Spiders* does some additional loading after Machine code started. ZXloader cannot possible know where this extra datablock needs to go. Same for headerless: ZQloader does not know where to put these. **Of course these games can be loaded without problem using a Z80 snapshot.**

* The file associations when set for Windows is not working very well. In Windows I'd like to have an explorer contect menu entry for TAP/Z80/TZX files `Load with ZQ-Loader`. This sometimes works, sometimes not, not sure. The installer was made with the 'Visual Studio Windows Installer Projects' extension. This tool is really wobbly imho.

Building
---

First get sources with:
```
git clone https://github.com/oxidaan/zqloader
```

You can use the file `zqloader.sln` with `zqloader.vcproj` to build the ZQloader executable with Visual studio 2022 in **Windows**.  
Or - at **Linux** - use the `CMakeList.txt` file to build it with CMake eg:
```
mkdir build
cd build
cmake ..
make
```
This will also try to build the z80 assembly code - but only if [sjasmplus](https://github.com/z00m128/sjasmplus) is present. For instructions see [sjasmplus](https://github.com/z00m128/sjasmplus/blob/master/INSTALL.md); I recommend what is described there under *CMAKE method for Linux / Unix / macOS / BSD* because ZQloader also uses CMake.
The assembler will also need the `BasicLib` to be present in the `examples` directory that comes with sjasmplus.   
It will also try to find QT to build the user interface. If this fails, it only builds the commandline version of the tool.  
If you can not build sjasmplus: no problem because its output files `zqloader.tap` and `zqloader.exp` and `snapshotregs.bin` are also here at github.

To assemble the z80 assembly code manually (without CMake) you can also use Visual studio code with the provided `tasks.json` at `.vscode` directory.
Or use a command like:
```
sjasmplus --fullpath     \
   -ipath/to/sjasmplus/examples/BasicLib \
   --exp=path/to/zqloader/z80/zqloader.exp \
   --lst --syntax=abf --color=on       \
   path/to/zqloader/z80/zqloader\z80\zqloader.z80asm  
```   
Installing
---
**Windows**  
A Windows installer is available [here](https://github.com/oxidaan/zqloader/releases). After installation it *should* add menu items to TAP/TZX/Z*) files `Load with ZQLoader`. If it does not try `Open with` -> `Choose another app`. At some point `Load with ZQ Loader` should be there in the explorer context menu.

**Linux**  
Use CMake (see above) then at the build directory:
```
sudo make install
```


Instructions
---
You need a *real* ZX-spectrum 48K. Connect the ZX-spectrums EAR input to your host computers sound output. At the host computer set sound to maximum.  
At Windows, make sure *Audio Enhancements* are switched off. 
Eg at `Settings -> System -> Sound -> Speakers -> Advanced`   
Switch off `Audio Enhancements` there.

Make sure no other sound is playing...

At the ZX Spectrum type:
```
LOAD ""
```

Then at the host type:
```
path/to/zqloader path/to/zqloader.tap path/to/turbofile
```
(At Windows there should be an explorer context menu `Load with ZQLoader`)

Eg:
```
zqloader z80/zqloader.tap c:\games\manic.z80`
```
You can play with options like, for example (see also [ZQloader commandline options](#ZQloader-commandline-options) ):
```
zqloader samplerate=48000 volume_left=-100 volume_right=-100 z80/zqloader.tap c:\games/rtype.z80
```

Tuning sound
---
There is a lot of discussion about what is the best setup for sound (eg see [here](https://retrocomputing.stackexchange.com/questions/773/loading-zx-spectrum-tape-audio-in-a-post-cassette-world)). Sometimes a stereo cable is recommended, sometimes mono. When the ZX-Spectrum and the host computer are sharing a common ground, a stereo cable is probably best. 
And when using stereo it is probably best to invert one sound output (left or right) eg with commandline parameters `volume_left=-100 volume_right=100` . This is not the default.  
Personally I was using stereo, but without inverting one channel. I used a 'composite-video to hdmi hardware box' that was powered through USB from my laptop. This way, the ZX Spectrum shared ground with the laptop. Later I used composite video directly. For some reason the ZX-Spectrum did not receive any sound at all - until I switched to a mono cable.  
_A good test is to try to play any sound at the host laptop (eg music) (play loud!). When all is good the ZX-Spectrum border should flash between red-cyan (after typing `LOAD ""`)_  
Moral of the story: try stereo/mono cables, try with inverted or not inverted sound channels - until it works.  
The user interface has a 'Tune' button that plays a leader tone forever. You can then adjust volume (and sample rate) until you get the red/cyan stripes.



ZQloader commandline options
---

Syntax:
1) `zqloader.exe [options] path/to/filename` 
2) `zqloader.exe [options] path/to/zqloader.tap path/to/turbofile`
3) `zqloader.exe [options] filename="path/to/zqloader.tap" turbofile="path/to/turbofile" option=value`
Arguments:
-  `path/to/filename`  
            First file: can be a .tap or .tzx file and will be loaded at normal speed into a real ZX spectrum. Only when the file here is 'zqloader.tap' it can load the second file:<br>
-  `path/to/turbofile`  
       Second file, also a .tap or .tzx or a .z80 (snapshot) file. A game for example. When given will be send to the ZX spectrum at turbo speed.

More options can be given with syntax:

`option=value`, or just `option value` or `option="some value" or `--option=value`:

* volume_left = value            
* volume_right = value    
A number between -100 and 100: sets volume for left or right sound (stereo) channel. Default 100 (max). A negative value eg -100 inverts this channel. When both are negative both channels are inverted.
* samplerate = value  
   Sample rate for audio. Default 0 meaning take device native sample rate. S/a miniaudio documentation.
* zero_tstates = value
* one_tstates = value  
     The number of TStates a zero / one pulse will take when using the ZQloader/turboloader. Not giving this (or 0) uses a default that worked for me. (118/293)
* bit_one_threshold = value      
    A time value in 50xTStates used at Z80 turboloader indicating the  time between edges when it is considered a 'one' - below this time it is considered a 'zero'. Related to 'one_tstates' above. Not giving this (or 0) uses a default that worked for me (4)
* bit_loop_max = value           
    A time value in 50xTstates used at Z80 turboloader indicating the maximum time between edges treated as valid 'one' value. Above this a timeout error will occur. Not giving this (or 0) uses a default that worked for me (12)
*  outputfile="path/to/filename.wav"  
When a wav file is given: write result to given WAV (audio) file instead of playing sound.
*  outputfile="path/to/filename.tzx"  
When a tzx file given: write result as tzx file instead of playing sound. *)
                           When a tzx file given: write result as tzx file instead of playing sound. **
*  wav or -w  
            Write a wav file as above with same as turbo (2nd) filename but with wav extension.
* tzx or -t  
            Write a tzx file as above with same as turbo (2nd) filename but with tzx extension.
*  overwrite or -o  
       When given allows overwriting above output file when already exists, else gives error in that case. Eg: `-wo` create wav file, overwrite previous one.
* key = yes/no/error  
  When done wait for key: yes=always, no=never or only when an error occurred (which is the default).
* --help
* --version  
   Show help or version.

*) tzx files is experimental and not fully tested. It uses `ID 19 - Generalized Data Block` a lot but I can't find a tool that can actually play it.   
