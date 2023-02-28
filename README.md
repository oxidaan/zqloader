ZQloader
====

This is a turbo loader used to load machine code games into a **real** ZX Spectrum at high speed.  

This loader is capable of loading a 48K game in about *25-30 seconds*. This time includes the time of loading the loader itself (which uses tradtional ROM loader/speed) plus a splash screen.  

The idea is that when using a computer to generate the loading-sounds a much higher speed can be achived as compared to the old tapes. After all the accuracy when generating loading sounds from a computer is much higher than those old wobbly tape recorders.   

The loader is coded into a basic `REM` statement to have it loaded in one step, thus avoiding the need to load an additional machine code block (which would take extra time). At the ZX Spectrum all you have to type is `LOAD ""`.

To speed up even more data (can be) compressed before loading.

The project code is portable and should be able to run under both Windows and Linux. (For Linux I briefly tried with WSL2/TODO)

Obviously this project has lots in common with [otla](https://github.com/sweetlilmre/otla). Actually I was at 2/3 of developping this when I found out about otla....
But otla seems barely maintained and uses CppBuilder - I think. Unlike Otla this project can also be compiled for Linux. And unlike otla it uses compression to speed up even more.

Project
----
There are two parts: 

The Z80 assembled ZQloader.
--
This is the turbo loader running at the ZX spectrum, written in Z80 assemby. It is stored entirely into a BASIC `REM` statement, so it can be loaded in just one step, and all you have to type is `LOAD ""`.  

Once loaded it copies itself to upper memory regions because lower RAM is [contended](https://en.wikipedia.org/wiki/Contended_memory) and not quick/stable enough for loading algortihms. Then it starts waiting for incoming turbo blocks.
More about the [Z80 assembled ZQloader]


zqloader.exe coded in C++ (17) 
---
Runs on the host computer. It uses [miniaudio](https://github.com/mackron/miniaudio) to generated loading sounds.  
First it uploads the ZQloader machine code (described above) itself to the ZX spectrum. It uses traditional ROM speed for that.

After that it will read a TAP or TZX file. It will recognize a basic loader, taking the start address from that (as in `RANDOMIZE USR XXXX`) (also read `CLEAR` value, if any). Then it will load 1 or more code blocks, which can be a spash screen or machine code, compress them, and send these at turbo speed into the ZX spectrum. 
More about the [C++ ZQloader...]


Compression
---
It uses a simple RLE compression algorithm that only reduces size with 20-40% thereabout. This is not that much (eg [ZX0](https://github.com/einar-saukas/ZX0) should do much better). But essential is it can be decompressed at the ZX spectrum at he same memory block - so decompressed data will overwrite compressed data during decompression. ZX0 can do this also but seems to always need to have some minimal extra space at the end (see `delta` at [ZX0 readme](https://github.com/einar-saukas/ZX0#readme)). **TODO** I really want to try and use ZX0 at some point.  
The RLE compression algortitm compresses the most used byte value (usually this is 0) by writing an escape code, then the number of 'most used value'-s. Further it compresses a sequence of 3 or more the same bytes (of any value) by writing another escape code, then that byte value, then the number of these bytes.

Limitations/TODO's
---

*Most of these limitations can be worked around by just loading a Z80 snapshot.*

* It can only load games (or applications) that are in machine code, not BASIC. Any BASIC is ignored. This is because the loader itself is coded into a BASIC rem statement - and would get lost if any loaded BASIC over writes it. Yes, it is copied to upper memory regions as well, but this might als be overwritten as described here ****.  
*TODO* a possible solution might be to have our c++ program recognize this situation and then copy the Z80 ZQloader code to the (lower 3rd) of the screen - then BASIC can be overwritten. Would spoil 1/3 of the splash screen however.

* It cannot load games that have any kind of copy protection. Or does any loading without BASIC. Eg *Horace and the Spiders* does some additional loading after Machine code started. ZXloader cannot possible know where this extra datablock needs to go. Same for headerless: ZQloader does not know where to put these.  
*TODO* a possible solution is to have the option to provide this information manually.

* It can not load games that overwrite the BASIC memory area. Eg *Galaxians* loads splash screen, basic and machine code at once as one big block. This would overwrite our loader however.  
*TODO* a possible solution might be to have our c++ program recognize this situation and then uses a Z80 ZQloader that only uses upper RAM area, not at the BASIC rem statement.

* A program that does a CLEAR below XXXX is currently to big to load - would overwrite our loader at the REM statement.   
*TODO* like described above the loader might use the (lower 3rd) of the screen.

* Want to add code to load .SNA snapshots.

* Not all TZX blocks are correctly recognized.

Building
---

You can use the file `zqloader.sln` with `zqloader.vcproj` to build the zqloader executable with Visual studio 2022 in Windows. Or - at Linux - use the `CMakeList.txt` file to build it with cmake eg:
```
mkdir buikd
cd build
cmake ..
make
```
To assemble the z80 code use [sjasmplus](https://github.com/z00m128/sjasmplus). You can use Visual studio code with the provided `tasks.json` at `.vscode` directory.
Or use a command like:
```
sjasmplus --fullpath     \
   -ipath/to/sjasmplus/examples/TapLib \
   -ipath/to/sjasmplus/examples/BasicLib \
   --exp=path/to/zqloader/z80/zqloader.exp \
   --lst --syntax=abf --color=on       \
   path/to/zqloader/z80/zqloader\z80\zqloader.z80asm  
```   

Instructions
---
You need a real ZX-spectrum. Connect the ZX-spectrums EAR input to your host computers sound output. At the host computer set sound to maximum.
