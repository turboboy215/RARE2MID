# RARE2MID
RARE (GB/GBC) to MIDI converter

This tool converts music from Game Boy and Game Boy Color games using RARE's sound engine to MIDI format.

It works with ROM images. Due to complications regarding how songs are stored in some games, a special .cfg configuration file needs to be used with each game specifying where to look for the song data in the game. Configuration files for every game are also included with the program. Note that for almost all games, there are "empty" tracks (usually at least the first track). This is normal.

Examples:
* RARE2MID "Donkey Kong Land 2 (UE) [S][!].gb" "Donkey Kong Land 2 (UE) [S][!].cfg"
* RARE2MID "Battletoads (U) [!].gb" "Battletoads (U) [!].cfg"
* RARE2MID "Perfect Dark (U) (M5) [C][!].gbc" "Perfect Dark (U) (M5) [C][!].cfg"

This tool was based on my own reverse-engineering of the sound engine, partially based on disassembly of Donkey Kong Land 2's sound code. Since the set of commands widely varies between many games, all versions of the driver have been accounted for.

As usual, a "prototype" program, RARE2TXT, is also included; however, this is not as compatible as the actual MIDI converter program.

Supported games:
  * The Amazing Spider-Man
  * Battletoads
  * Battletoads & Double Dragon: The Ultimate Team
  * Battletoads in Ragnarok's World
  * Beetlejuice
  * Conker's Pocket Tales
  * Croc: Legend of the Gobbos
  * Donkey Kong Country
  * Donkey Kong Land
  * Donkey Kong Land 2
  * Donkey Kong Land III
  * Killer Instinct
  * Mickey's Racing Adventure
  * Mickey's Speedway USA
  * Monster Max
  * Perfect Dark
  * Sneaky Snakes
  * Super R.C. Pro-Am
  * Wizards & Warriors Chapter X: The Fortress of Fear
  * WWF Superstars

## To do:
  * Support for other versions of the sound engine (e.g. NES)
  * GBS file support
