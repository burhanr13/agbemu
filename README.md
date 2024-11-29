# agbemu

An accurate Game Boy Advance Emulator. Supports all of the graphics and audio subsystems, passes many test roms including the AGS Aging Cartridge (used to test real GBAs by Nintendo), ARMWrestler, and JSMolka's gba-tests. From the mgba test suite it passes 2020/2020 timing tests, 936/936 timer count-up tests and 90/90 timer irq tests (very few emulators I know pass these), along with all memory, DMA, IO read, shifter, and carry tests. It runs all games I have tested without issue.

## Building

This project requires SDL2 as a dependency to build and run. 
To build use `make` or `make release` to build the release version 
or `make debug` for debug symbols.
I have tested on both Ubuntu and MacOS.

## Usage

You need a GBA bios binary to run the emulator. You can dump an official one or use an open source replacement. Pass the file path in the command line with `-b` or leave it out and it will use the
file `bios.bin` which should be in the current directory by default.

To run a game just run the executable with the path to the ROM as the last command line argument, or use no arguments to see other command line options.

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
| Save State | `9` |
| Load State | `0` |

## Credits

- [GBATEK](https://www.problemkaputt.de/gbatek.htm)
- [Tonc](https://www.coranac.com/tonc/text/toc.htm)
- [mGBA](https://github.com/mgba-emu/mgba) and its [test suite](https://github.com/mgba-emu/suite)
- fleroviux and zayd
- [Emulator Development Discord Server](https://discord.gg/dkmJAes)
