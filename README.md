# agbemu

A somewhat accurate Game Boy Advance Emulator. Supports all of the graphics and audio subsystems, passes many test roms including AGS Aging Cartridge, ARMWrestler, JSMolka's gba-tests, and a sizable portion of the mGBA test suite including all of the memory and DMA tests and most timing tests. It runs all games I have tested without any issue.

## Building

This project requires SDL2 as a dependency to build and run. 
To build use `make` or `make debug` to build
a debug version or `make release` for the optimized release version.
I have tested on both Ubuntu and MacOS.

## Usage

You need a GBA bios binary to run the emulator. You can dump an official one or use an open source replacement. Name the file `bios.bin` and keep it in the same directory as the executable.

To run a game just run the executable with the path to the ROM as the only command line argument, or use no arguments to see other command line options.

The keyboard controls are as follows:

| GBA | Key |
| --- | --- |
| `A` | `Z` |
| `B` | `X` |
| `L` | `A` |
| `R` | `S` |
| `Dpad` | `Arrow keys` |
| `Start` | `Return` |
| `Select` | `RShift` |

You can also connect a controller prior to starting the emulator.

Hotkeys are as follows:

| Control | Key |
| ------- | --- |
| Pause/Unpause | `P` |
| Mute/Unmute | `M` |
| Reset | `R` |
| Toggle color filter | `F` |
| Toggle speedup | `Tab` |

## Credits

- [GBATEK](https://www.problemkaputt.de/gbatek.htm)
- [Tonc](https://www.coranac.com/tonc/text/toc.htm)
- [mGBA](https://github.com/mgba-emu/mgba)
- [EmuDev Discord Server](https://discord.gg/dkmJAes)
