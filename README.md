# 🏢Skyscraper

I have modified the very powerful scraper developed mainly by [Muldjord](https://github.com/muldjord/skyscraper) and later on enhanced by [Detain](https://github.com/detain/skyscraper) and lately by [Gemba](https://github.com/Gemba/skyscraper/). I forked the Detain version (two years ago) after it had reached a somewhat stale state, and I started adding many features to cater to my needs. Some changes were implemented (more elegantly) by Gemba, but my fork had already diverged too much so I just cherry-picked from his fork what I felt I needed instead of rebasing to his fork and starting from there. The codebase of my version has significantly increased since then.

The main changes have been:

## New Frontends
* XMLExport: Generic XML export of the information that I use to import data into [GCStar](https://gitlab.com/GCstar/GCstar) and its web frontend [GCWeb](http://jonas.tuxfamily.org/wiki/gcweb).
* Koillection: Frontend to support the wonderful collection tool by [Benjamin Jonard](https://github.com/benjaminjonard/koillection). A hybrid frontend mixing API calls to the Koillection server and SQL connections to the underlying MySQL/MariaDB database. Beware that I also slightly "customized" the public Koillection branch so that it could interoperate in my landscape.

## New Scrapers
* GiantBomb: A hybrid frontend that downloads part of the information from [GiantBomb](https://www.giantbomb.com) in an offline cache to improve throughtput while respecting the strict limits implemented by their admins.
* Chiptune: A specialized scraper that matches my soundtrack collection stored in my [Navidrome](https://github.com/navidrome/navidrome) SQLite database to my game collection.
* OfflineMobyGames: A hybrid frontend that uses an offline cache to improve throughtput while respecting the strict limits implemented by their admins. Improved to maximize offline available data when no API key is defined (MobyGames has closed the door to limited, free API scraping).
* OfflineTGDB: A hybrid frontend that uses a local database to improve throughput while respecting the strict limits implemented by their admins.
* Customflags: A specialized scraper that detects special files in my collection to deduce the concepts: Game Size, Completed, Favourite, Played, First Played, Last Played, Total Times Played, Time Played.
* Launchbox: A very effective scraper based on the fantastic and accessible [LaunchBox](https://gamesdb.launchbox-app.com/) database.
* Rawg: An online frontend that uses the [Rawg.io](https://rawg.io/) service.
* VGFacts: A hybrid specialized scraper that uses a JSON version of the [VGFacts](https://www.vgfacts.com/) website to retrieve videogames trivia.
* VGMaps: A specialized offline scraper that uses a dump of the [VGMaps](https://www.vgmaps.com/) website to include graphical maps of videogames.
* GameFAQs: An offline scraper that uses a JSON+guides dump of the [GameFAQs](https://gamefaqs.gamespot.com/) website to include videogame guides and walkthroughs in the collection. GameFAQs does not approve of external scraping, so the dumps need to be obtained from somewhere else.
* MAMEHistory: An offline scraper that uses the XML dump of the [Arcade History](https://www.arcade-history.com/) website to include descriptions and overviews for MAME short-names.
* DocsDB: An offline scraper that uses a simple CSV file and folder structure to refer to local documents to be loaded as guides, reviews, cheats or artbook resources.
* EveryGame: An online scraper that extracts a broad range of resources from a mix of API and web scraping of the [EveryGameGoing](https://www.everygamegoing.com/) site.
* VGGeek: An online scraper that extracts text and image data from the [VideoGameGeek](https://videogamegeek.com/) site.
* Spriters: A specialized hybrid scraper that uses a Python script and an offline index to retrieve sprites and maps from the web.
* GameBase: An offline scraper that uses community-generated platform-specific bundles of Access databases plus a collection of multimedia and document resources. The bundles can be found scattered in the [Internet Archive](https://www.archive.org/) as well as specific websites such as [BU22](https://www.bu22.com/). I got the idea for this new scraper and the script to convert Access databases to SQLite from the [Gemba repo](https://github.com/Gemba/skyscraper/).
* eXoDOS: An offline, PC-DOS-specialized scraper that integrates the curated PC-DOS gaming package from [eXoDOS](https://www.retro-exo.com/exodos.html).

## Fix Broken Scrapers
* Notably OpenRetro, WorldOfSpectrum and TGDB, but also fixes and improvements to IGDB, MobyGames, ArcadeDB and, of course the King: Screenscraper.

And of course:
* The checksum tecnique has been re-implemented as an on-demand function that can be used in all scrapers now: ScreenScraper uses pure checksum search, all the other scrapers use an external database (canonical) to determine the proper search names for the games, instead of relying solely on the filename. The canonical database can be generated using Skyscraper itself, feeding it the TOSEC, Redump and No-Intro dat files, use an online Lutris database, or the MAME xml hash files. Checksums are stored in the cache database as cross-scraper resources, to avoid rehashing the same file multiple times.
* Implement a negative match database functionality that caches negative searches per platform, online scraper and optionally by file, with a configurable expiration date. It also stores low matches, to review the accuracy of the matching algorythm.
* Updated configuration files to support new platforms (plenty) and error cleansing of the json configuration files. All the scraper code/platform ids mappings are now in the configuration JSON files.
* Support additional resources (database ids, usage data, user flags, game size, music, manuals, trivia, maps, guides, cheats, reviews, artbooks, sprites) and the logic to scrape them and add them to the frontends.
* Some resources have an special logic when fed into the frontends: instead of a source priority rule, a smart selection/collation is used with: descriptions (longest), ids, sprites, cheats, trivia, maps, guides, reviews, artbooks (collate) and rating (average).
* Allow scraping of missing resources for already scraped games (in case the database has been updated with additional information) (getMissingResources).
* Allow scraping only one image per resource type across scrapers (similar behaviour as with videos and manuals) (singleImagePerType).
* Validation of the cache now tries to fix (and cleanses if it cannot) the textual resource contents of the database.
* --startat and --endat now are compatible with recursive directories.
* Take advantage of MAME short names for the compatible scrapers.
* Fallback to "arcade" configuration files for Arcade-type platforms if no platform specific configuration exists.
* Improve data conformance checks and corrections to ensure homogeneized resources across scrapers.
* Alternative name support for the compatible scrapers (most of them).
* Implement Latin characters simplification as a search variant in most scrapers.
* Massive refactorings, errors, typos and style fixes. Improvements to thread-safety and database access, command-line parameter processing is now more robust.
* File-based locking mechanism and database temp file when writing to avoid/minimize corrupted cache databases.
* Rescan feature to recheck the cache against the scraper databases (specially needed after changing the title matching logic).
* Incremental scraping, for when new resources need to be added to already scraped games.
* Improve the API keys and password management.
* Generation of dummy thumbnails with text when no real thumbnail is available in the cache database.
* Optional mapping of tags and franchises so that the words used across scrapers can point to the desired standardized word or even be deleted.
* Improve special characters management through Unicode.
* Some changes to minimize the false positives and false negatives from the scrapers.
* New cache tools to manage big databases such as mine (several tens of thousands of entries) and spot inconsistencies and false positive matches.
* Implement a command-line option (--list) to print valid sets of scrapers and platforms, typically to be used in scripts.
* Offline scrapers with multimedia data (GameBase and eXoDOS) no longer copy resources into the cache, but just symlink them.
* Add option to not copy image resources when generating the frontend output, but just symlink from the cache (same as symlink option for videos and manuals). Does not allow manipulation of the images when generating the output, obviously (symlinkImages).
* Differentiate between "platform" and "collection" in the frontend (platform comes from the scraper and is not necessarily normalized, collection is the platform code and text from the configuration files).
* Fix logic in Netcomm to determine if HTTP requests should be DELETE, POST, GET or potentially other operations.
* Clean old Qt code not reasonably needed anymore, preparing for Qt 6, and activate QStringBuilder to improve string management performance (alledgely).
* Copyright updates on the relevant files to acknowledge the contributors.
* ...Changes that I need that probably no other one out there will need :)
* Breaking changes: The unpack functionality has been removed (although the checksum generation does unpack the files if needed). The checksum matching logic has been heavily modified. Some parameters have been migrated to flags, or have had the name unified across scrapers/frontends.
 

## Original Readme:




Original project by [Lars Muldjord](https://github.com/muldjord/skyscraper)

A powerful and versatile yet easy to use game scraper written in C++ for use with multiple frontends running on a Linux system (macOS and Windows too, but not officially supported). It scrapes and caches various game resources from various scraping sources, including media such as screenshot, cover and video. It then gives you the option to generate a game list and artwork for the chosen frontend by combining all of the cached resources.

All Skyscraper features are [well-documented](docs/) and there's also a [F.A.Q](docs/FAQ.md).

## 🎮Platforms Supported (set with '-p'):
Check the full list of platforms [here](docs/PLATFORMS.md).

## 🕹Frontends Supported (set with '-f'):
* EmulationStation
* AttractMode
* Pegasus

## 📚Supported scraping modules (set with '-s')
Skyscraper supports a variety of different scraping sources called *scraping modules*. Use these to gather game data into the Skyscraper resource cache. Check the full list of scraping modules [here](docs/SCRAPINGMODULES.md) and read more about the resource cache [here](docs/CACHE.md).

## 🧑‍💻Code contributions
I welcome any contributions, although I would like to keep things backwards compatible.

## How to install Skyscraper
Follow the steps below to install the latest version of Skyscraper. Lines beginning with `$` signifies a command you need run in a terminal on the machine you wish to install it on.

NOTE! If you are using the RetroPie distribution, you have the option to install Skyscraper directly from the RetroPie-Setup script (*you need to update the script before installing it!*). Read more about all of that [here](https://retropie.org.uk/docs/Scraper/#lars-muldjords-skyscraper). If not, read on.

### Install prerequisites
#### Linux
Skyscraper needs Qt5.3 or later to compile. For a Retropie, Ubuntu or other Debian derived distro, you can install Qt5 using the following commands:
```
$ sudo apt update
$ sudo apt install build-essential qt5-default
```
You might be asked for your sudo password. On RetroPie the default password is `raspberry`. To install Qt5 on other Linux distributions, please refer to their documentation.

NOTE! From Ubuntu 21.04 and forward the `qt5-default` metapackage no longer exists. You will instead have to do `sudo apt install build-essential qtbase5-dev qt5-qmake qtbase5-dev-tools` which installs the same as the above command.

#### macOS
Skyscraper works perfectly on macOS as well but is not officially supported as I don't own a Mac. But with the help of HoraceAndTheSpider and abritinthebay here's the commands needed to install the Qt5 and other prerequisites:
```
$ /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
$ brew install gnu-tar --with-default-names
$ brew install wget
$ brew install qt5
$ brew link qt5 --force
```
If that went well, proceed to the default installation instructions below. It should work and give you a working installation of Skyscraper.

NOTE 1! User 'davidmgrantham' reports that the `--with-default-names` above might be deprecated for some macOS installations. If you remove it, you also need to download the `update_skyscraper.sh` and edit the `tar` commands to be `gtar` before running it.

NOTE 2! User 'trvrplk' reports numerous issues on MacOS 11.2+. Check [here](https://github.com/detain/skyscraper/issues/301) for potential fixes.

### Download, compile and install
When you've installed the prerequisites as described above, you can install Skyscraper by typing in the following commands:
```
$ cd
$ mkdir skysource
$ cd skysource
$ wget -q -O - https://raw.githubusercontent.com/detain/skyscraper/master/update_skyscraper.sh | bash
```
The last command will download and run the latest update script from Github. During the installation you might be asked for your sudo password. On RetroPie the default password is `raspberry`.

When the script has completed you are ready to run Skyscraper!

### Updating Skyscraper
From Skyscraper 2.3.2 and newer you can update to the latest version simply by running the following commands:
```
$ cd
$ cd skysource
$ ./update_skyscraper.sh
```
You might be asked for your sudo password during the update. On RetroPie the default password is `raspberry`. If your version is older than 2.3.2 (check with `--help`) you need to follow the [installation instructions](#download-compile-and-install) instead.

### How to uninstall Skyscraper
If you've installed Skyscraper using the instructions in this readme, you can uninstall it using the following commands:
```
$ cd
$ cd skysource
$ sudo make uninstall
$ cd
$ rm -Rf skysource
$ rm -Rf .skyscraper
```
You might be asked for your sudo password during the processs. On RetroPie the default password is `raspberry`.

### Windows
Windows is not officially supported at this time! But I do semi-regularly compile and release an unsupported Windows 64-bit version that works just fine. Be sure to read the included README from the downloaded file before using it! And just to be clear: You are on your own if you use this version - please don't ask me questions about it. Get the Windows version [here](https://github.com/detain/skyscraper/releases/download/3.6.12/Skyscraper_3.6.12_unsupported_win_version.zip).

## How to use Skyscraper
IMPORTANT!!! In order for Skyscraper to work properly, it is necessary to quit your frontend before running it! If you're running EmulationStation, you can quit it by pressing F4.

Remember, you can completely customize the artwork Skyscraper exports. Check out the documentation [here](docs/ARTWORK.md). If you just want to use the default (pretty cool looking) artwork Skyscraper provides, read on.

### A simple use case
For first-time users I recommend reading the short and to-the-point [use case](docs/USECASE.md). Please read it and get back here when you got the gist of it.

### A quick run-down of Skyscraper
Skyscraper is a command line tool, and has many, many options for you to fiddle around with. I recommend taking a look at all of them to familiarize yourself with the possibilites:
```
$ Skyscraper --help
$ Skyscraper --flags help
$ Skyscraper --cache help
```
This will give you a description of everything Skyscraper can do if you feel adventurous! For a thorough description of all available options, check [here](docs/CLIHELP.md).

The most important ones are probably:
* `-p <PLATFORM>`
* `-s <SCRAPING MODULE>`
* `-u <USER:PASS>` or `-u <KEY>`
* `--cache refresh`
* `--flags videos`

If you have your roms in a non-default location (default for RetroPie users is `/home/<USER>/RetroPie/roms/<PLATFORM>`) or wish to export the game list or artwork to non-default locations, you will also need these:
* `-i <PATH>`
* `-g <PATH>`
* `-o <PATH>`

For almost any command line option, consider setting them in the `/home/USER/.skyscraper/config.ini` file as described [here](docs/CONFIGINI.md). This will make the options permanent so you don't need to type them in all the time.

#### Gathering data for a subset of roms
Skyscraper offers several ways of gathering data for a subset of roms. If you just want to scrape the roms that have no data in the cache whatsoever, you can do so with the `--onlymissing` command-line option. You can also check out the `--startat FILENAME` and `--endat FILENAME` options. If you just want to gather data for a couple of roms you can simply add the filename(s) to the end of the command-line (eg. `Skyscraper -p amiga -s openretro "/path/to/rom name 1.lha" "/path/to/rom name 2.lha"`). And probably the most advanced (and quite handy) way to gather data for a subset of roms is to make use of the `--cache report:missing=RESOURCE` option. This can generate a report containing the filenames that are missing a certain resource. You can then feed the report back into Skyscraper with the `--fromfile REPORTFILE` afterwards. Skyscraper will then only scrape the files contained in the report.

### config.ini
A lesser known, but extremely useful, feature of Skyscraper is to add your desired config variables to `/home/USER/.skyscraper/config.ini`. Any options set in this file will be used by default by Skyscraper. So if you always use, for example, `-i <SOME FOLDER>` on command line, you can set the matching option `inputFolder="<SOME FOLDER>"` in the config.

For a full description of all availabe config options, check [here](docs/CONFIGINI.md).

### Resource cache
One of Skyscraper's most powerful features is the resource cache. It's important to understand how this works in order to use Skyscraper to its full potential. Read more about it [here](docs/CACHE.md).

### Custom data
I addition to allowing scraping from locally cached resources, Skyscraper also allows you to import your own data into the resource cache with the `-s import` scraping module. You can also edit existing resources in the cache or add individual `user` resources with the `--cache edit` command. Lastly, you also have the option of importing existing EmulationStation game list data into the Skyscraper resource cache if you need it. You can do this with the `-s esgamelist` scraping module.

To read more about any of the features described above, please check out all of the documentation [here](docs/).

### Artwork look and effects
Check the full artwork documentation [here](docs/ARTWORK.md)

## Skyscraper JSON Enhancement by torresflo

The main goal of this fork is to allow users to easily configurate and add platforms without having the needs to edit the code source directly.

Currently, the only scraper supported by this feature is Screenscraper. This feature is achieved by adding new config files:
- [platforms.json](platforms.json) describes now the supported platforms by Skyscraper.
- [screenscraper.json](screenscraper.json). describes the required information on the Screenscraper website for each compatible platform.
- [mobygames.json](mobygames.json): Maps the local platform name to the platform ID of mobygames.com webpage/API.

These 2 files are pretty straigthforward to edit if you want to add a new platform (as an example I added myself the platforms OpenBOR and Solarus). These files are copied in the folder `/usr/local/etc/skyscraper/` at the first run of the program if you want to edit them after an installation.  

### Installation

You can easilly install Skyscraper Enhanced by adding the script in the retropie-setup folder ([here](retropie-setup/skyscraperenhanced.sh)) in the scriptmodules folder of your retropie setup installation.

This way, you can install and run the program through the retropie setup menu like the original Skyscraper. Note that the same config and installation folders of the original Skyscraper will be used instead of having everything duplicated in specific folders.


## Release notes

#### Version x.x.x (Features under consideration, all unimplemented)
* Add the option to scrape from cached data purely originating from certain scraping modules
* Consider making aliasMap the global baseName instead of just the search term base name. This will fix missing brackets in alias names being ignored later on
* Make all artwork types custom meaning that their type can be whatever the user wants. The sources will create ones with known types such as 'screenshot' and 'cover', but the user can import other types they define themselves, such as 'cabinet' or whatever else. Internally artwork is then handled by a list of artwork instead of 4 hardcoded types in GameEntry
* Introduce the ability to use <BASENAME> in the 'file="somefolder/<BASENAME>.png"' in artwork.xml which would then look for a file in 'resources' with the game basename.png allowing users to use their own custom artwork data as a workaround to adding new types to import (Thank you to 'jueank' for suggesting this)
* Add the option of scraping custom platforms by configuring them in the config with an alias to an already existing platform. Example: scrape 'pcenginecd' could be scraped as 'pcengine' in case you have those files in a 'roms/pcenginecd' folder instead of the pcengine folder. Check here: https://github.com/detain/skyscraper/issues/136
* Create a testmode for the artwork compositor that let's you quickly render an example to see if you got everything set up right in the artwork xml
* Allow 'region' to be a list similar to 'regionPrios'. When using 'region' it should simply keep the default priority list and add those from 'region' to the top. 'regionPrios' should still overwrite it entirely. Naming change probably a good idea, for instance rename 'region' to 'regionsPrefer' or something. 'regionPrios' should probably also be changed to 'regionsOverride'. (Thank you to 'corezon' for suggesting this).
* Implement a less rigid filename match for the 'import' module to allow for close match filenames
* Add support for grouping multi-disk games so they only have one entry in the gamelists. See issues/232 (Thank you to 'igno2k' for suggesting this)

#### Version 3.7.7-2
* Added script to quickly generate the mapMap.csv
* Added script to quickly generate the platforms.json from an existing platform.cpp file
* Moved the list of platforms and thier associated scrapers, file masks, and aliases to be loaded from a JSON file instead of hardcoded in CPP from [torresflo/skyscraper-enhanced](https://github.com/torresflo/skyscraper-enhanced)
* Added 'texture' image type to be stored in textures dir from [InfiRandia/skyscraper](https://github.com/InfiRandia/skyscraper)
* Updated MAME map to 0.245
* Added 'actionmax', 'naomi2', 'ps3', 'ps4', 'ps5', and 'xbox' platforms
* Added '*.chd' to 'naomi' and 'naomi2'
* Added '*.nca' to 'switch'
* Fixed the 'pc88' and 'x1' ScreenScraper IDs
* Added automatic generation of compiled builds on new tag

#### Version 3.7.7 (28th June 2022)
* Added '*.m3u' to 'segacd' platform (Thank you to user 'bmn001' for suggesting it)

#### Version 3.7.6 (12th June 2022)
* Added 'pico8' platform (Thank you to user 'zerojay' for suggesting it)

#### Version 3.7.5 (31st January 2022)
* Now allows ':' in Pegasus command

#### Version 3.7.4 (23rd January 2022)
* '--includefrom' and '--excludefrom' now works with relative paths (Thank you to use 'sleve_mcdichael' for reporting this
* Skipped file entries now conforms to same format as report files to allow using them with '--excludefrom' and '--includefrom' (Thank you to user 'TomFury' for suggesting this)

#### Version 3.7.3 (23rd January 2022)
* Added 'switch' platform (Thank you to user 'Redemp' for providing info)

#### Version 3.7.2 (20th January 2022)
* Skyscraper now only ignore files when using the '.skyscraperignore[tree]' files when scraping for new data. They will NOT be ignored when generating game lists

#### Version 3.7.1 (19th January 2022)
* Skyscraper will now ignore an entire tree of subfolders where a file called '.skyscraperignoretree' is found (Thank you to user 'sromeroi' for suggesting this)

#### Version 3.7.0 (17th January 2022)
* Moved '--fromfile' option to '--includefrom'. '--fromfile' still works, but is considered deprecated
* Moved '--includefiles' option to '--includepattern'. '--includefiles' still works, but is considered deprecated
* Moved '--excludefiles' option to '--excludepattern'. '--excludefiles' still works, but is considered deprecated
* Added '--excludefrom' option similar to '--includefrom' only the opposite (Thank you to user 'TomFury' for suggesting this)
* Skyscraper will now ignore any subfolders within the input folder where a file called '.skyscraperignore' is found (Thank you to user 'sromeroi' for suggesting this)
* Added platform 'easyrpg', only usable using the 'screenscraper' scraping module (Thank you to user 'zerojay' for suggesting this)

#### Version 3.6.16 (9th November 2021)
* Added platform 'moto' (Thank you to user 'Yserra' for suggesting it)

#### Version 3.6.15 (25th August 2021)
* Added 'chd' extension to 'atomiswave' platform (Thank you to user 'smeegoan' for reporting this)
* Fixed bug that caused 'T000000' to be added multiple times when skipping entries in ES gamelists (Thank you to user 'sleve_mcdichael' for reporting this)

#### Version 3.6.14 (5th August 2021)
* Added 'windows 3.x' as alias to Mobygames scraping module (Thank you to user 'ecto' for reporting this)

#### Version 3.6.13 (2nd June 2021)
* Added 'mediaFolderHidden' EmulationStation specific config option that will set the media folder to 'PLATFORM/.media' when set to true. This can speed up EmulationStation initial loading when using slow storage such as a network file system (Thank you to user 'XenuIsWatching' for suggesting this)

#### Version 3.6.12 (15th May 2021)
* Removed 'Simple Mode' as it was broken and deprecated. Use RetroPie script instead
* Removed A LOT of deprecated (and hidden) CLI options

#### Version 3.6.11 (6th May 2021)
* '--fromfile' now accepts both relative and absolute path to filename (Thank you to user 'sleve_mcdichael' for reporting this)
* Added platforms 'atarijaguarcd', 'pcenginecd' and 'channelf' (Thank you to user 'XenuIsWatching' for providing info)

#### Version 3.6.10 (12th April 2021)
* Added 'naomi' as platform
* Added 'msx2' as platform
* Added 'atomiswave' as platform

#### Version 3.6.9 (1st April 2021)
* Probably fixed `--flags unpack` which broke with 3.6.8 due to deprecated Qt function calls
* Probably fixed video conversion which broke with 3.6.8 due to deprecated Qt function calls

#### Version 3.6.8 (31st March 2021)
* Added 'cacheRefresh' config.ini option and added it to default config.ini for 'esgamelist' module (Thank you to user 'penx' for suggesting this)
* 'ScreenScraper APIv2 returned invalid / empty Json' message now no longer includes message to attach the error json file unless it's more than 64 bytes long
* Added 'ca' to region priority list (Thank you to user 'joaoluizcarvalho' for pointing this out)
* Added 'de', 'fr' and 'es' to default language priority list
* Added 'players' resource to '--cache report:missing' CLI option (Thank you to user 'peligwe' for reporting this)
* Added 'theInFront' config.ini option and 'theinfront' CLI flag (Thank you to user 'sleve_mcdichael' for suggesting this)
* 'unattend' and 'unattendSkip' can now be used in a scraping module section in config.ini (Thank you to user 'sleve_mcdichael' for reporting this)
* Added Qt version pre-processor checks for deprecated Qt methods to avoid compilation warning on newer Qt installations

#### Version 3.6.7 (27th January 2021)
* Added '*.rvz' extension for 'wii' and 'gc' platforms (Thank you to user 'tcamargo' for reporting this)
* Added '*.chd' extension for '3do' platform (Thank you to user 'Roudaku' for reporting this)
* Added 'openbor' platform. Exists in RetroPie as experimental platform (as requested by user 'hahnmt')
* 'neogeo' platform now uses 'box-2D' as cover instead of 'flyer' when scraping with the ScreenScraper module (Thank you to user 'retro81' for suggesting this)

#### Version 3.6.6 (21st December 2020)
* The 'esgamelist' module now looks for the 'gamelist.xml' file in the path set with '-g' instead of the path set with '-i' (Thank you to user 'c0d3h4x0r' for reporting this and helping me debug it)

#### Version 3.6.5 (18th December 2020)
* Now uses one shared networking instance, instead of one per thread
* 'daphne' platform entries now added as '<game>' entries instead of '<folder>' entries (Thank you to user 'clyde' for helping figure this out)
* Added 'nocropblack' cli flag and 'cropBlack="BOOL"' config.ini option that allows user to disable cropping of black borders around screenshots when compositing the final gamelist artwork (Thank you to user 'WindyWinston' for suggesting this)

#### Version 3.6.1 (27th October 2020)
* Added 'amigacd32' as a separate platform just because (Thank you to user 'HoraceAndTheSpider' for resetting The Matrix)

#### Version 3.6.0 (25th October 2020)
* Changed 'SecsSinceEpich' to 'MSecsSinceEpich' to support pre-5.8 Qt versions (Thank you to user 'Hazza4569' for reporting this)

#### Older releases
Release notes for older releases can be found [here](OLDERRELEASES.md).
