# TZX waveform tests

Configure with `BUILD_TESTING=ON`, build, then run:

```
ctest --test-dir build -C Release --output-on-failure
```

Python 3 is required for the test driver. The fixture compiles the internal
writer sources directly rather than relying on private DLL symbols. Tests need
neither an audio device nor Fuse/libspectrum. The independent Python decoder
checks levels, processing-delay placement, pauses, long holds, data/alphabet
block boundaries, empty/invalid inputs and the fixed 3.5 MHz TZX timebase.
Sampled exports are compared with the actual WAV writer at 44.1, 48, 96 and
192 kHz, including seeded mixed edge/hold sequences.

## Optional Fuse integration reproduction

Create a synthetic 48K snapshot with no game data:

```
python tests/make_fuse_payload.py payload.z80
build/zqloader key=no samplerate=48000 outputfile=payload.wav z80/zqloader48.tap payload.z80
build/zqloader key=no samplerate=48000 outputfile=payload.tzx z80/zqloader48.tap payload.z80
```

Adjust the executable path for the chosen build layout/platform. In Fuse select
48K Spectrum and open each tape with automatic loading enabled. For the controlled
comparison, disable automatic tape start/stop detection (`--no-detect-loader`)
and loader acceleration (`--no-accelerate-loader`). The tested current Fuse can
otherwise falsely stop playback during BASIC startup, before turbo loading.
Successful
loading gives a black-and-white noise screen and a green border. The payload's
final loop is at 0x8005; automated tests should also check the restored register
signature (AF=0x0400, SP=0xff00, BC/DE/HL/IX/IY/I=0, IFF1 off) and green border,
not merely a visit to that address following a corrupted load.

Both exports explicitly use sampled timing here. Omit `samplerate` for exact
nominal TZX timing; do not assume that exact output matches a sampled WAV.

To exercise the observed 48 kHz boundary, add `zero_tstates=91 zero_max=3`
and compare `one_tstates=227` against `one_tstates=228` for each output format.
On the tested current Fuse build the first fails and the second succeeds.
These are observations for this payload/settings, not a universal speed limit.
Changing loader bit rate is distinct from changing audio sample rate.
