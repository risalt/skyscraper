### What's supported and why
As Skyscraper was built to be used with RetroPie, the list of supported platforms is largely dictated by their naming scheme and [list of platforms](https://retropie.org.uk/docs/Supported-Systems). Skyscraper will only rarely add platforms that aren't already on that list.

### Skyscraper currently supports the following platforms (set with '-p PLATFORM'):
* 3DO
* Amiga (OCS, ECS, AGA, CD32, CDTV)
* Amstrad CPC
* Apple 2
* Arcade
* AstroCade
* Atari800
* Atari2600
* Atari5200
* Atari7800
* Atari Jaguar
* Atari Jaguar CD
* Atari Lynx
* Atari ST
* AtomisWave
* Coco (Tandy Colour Computer)
* ColecoVision
* Commodore 16/64/128
* Commodore Vic-20
* Daphne
* Dragon 32/64
* Dreamcast
* EasyRPG (RPG Maker 2000/2003 games)
* Emerson Arcadia 2001
* Fairchild Channel F
* Famicom Disk System
* FB Neo (formerly FB Alpha)
* Game Boy
* Game Boy Advance
* Game Boy Color
* GameCube
* Game & Watch
* Intellivision
* Mame
* Megadrive / Genesis
* MSX (MSX, MSX 2, MSX 2+, MSX Laserdisc)
* Naomi
* NeoGeo
* NeoGeo CD
* NeoGeo Pocket
* NeoGeo Pocket Color
* Nintendo 64
* Nintendo DS
* Nintendo Entertainment System
* Nintendo Switch
* Oric (Oric-1, Oric Atmos)
* PC
* PC-8800
* PC-9800
* PC-Engine / TurboGrafx-16
* PC-Engine CD / TurboGrafx-16 CD
* PC-Engine Supergrafx
* PC-FX
* Pico-8 (Virtual console)
* Playstation
* Playstation 2
* Playstation Portable
* Pokémon Mini
* Ports (For games ported to RaspBerry Pi)
* ScummVM (looks for PC or Amiga matches)
* Sega 32x
* Sega CD
* Sega Game Gear
* Sega Master System
* Sega Saturn
* Sega SG-1000
* Sharp X1
* Sharp X68000
* Steam (through Moonlight streaming)
* Super Nintendo
* TI-99/4A
* Thomson MO/TO
* TRS-80
* Vectrex
* Videopac (Oddysey 2)
* Virtual Boy
* Wii
* Wii U
* WonderSwan
* WonderSwan Color
* Z-Machine
* ZX81
* ZX Spectrum

Thanks to the PR from torresflo @ GitHub it is possible to add new platforms by
editing the `platforms.json` file.

Take this example:
```json
        {
            "name": "atari2600",
            "scrapers": [
                "screenscraper"
            ],
            "formats": [
                "*.a26",
                "*.bin",
                "*.gz",
                "*.rom"
            ],
            "aliases": [
                "atari 2600"
            ]
        },
```

- `name`: reflects the platform/folder name, usually provided with `-p` on the
  command line.
- `scrapers`: set of possible scraper sites. Denotes the default scrapers if not
  overridden by `-s` command line flag.
- `formats`: set of ROM file extensions which will be included in scraping if
  not a ROM file is provided via command line.
- `aliases`: set of aliases for this platform. Make sure that the platform names
  from `screenscraper.json` and/or `mobygames.json` are listed here, thus
  Skyscraper will map it to the right platform ID for the scraper.

**Note**: If you need a specific folder name for a platform (on your setup or
due to an EmulationStation theme) use a symbolic link (see `megadrive` and
`genesis` for example on RetroPie) instead of adding a new platform in this JSON
file.

