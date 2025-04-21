TEMPLATE = app
TARGET = Skyscraper
DEPENDPATH += .
INCLUDEPATH += .
CONFIG -= debug
CONFIG += release
win32:CONFIG += console
QT += core network xml sql widgets
# QTPLUGIN.platforms = qoffscreen
DEFINES *= QT_USE_QSTRINGBUILDER

QMAKE_CXXFLAGS += -std=c++17

unix:target.path=/usr/local/bin
unix:target.files=Skyscraper Skyscraper.app/Contents/MacOS/Skyscraper

unix:docs.path=/usr/local/etc/skyscraper/docs
unix.docs.files=README.md docs/ARTWORK.md docs/CACHE.md docs/IMPORT.md docs/CONFIGINI.md docs/REGIONS.md docs/FAQ.md docs/LANGUAGES.md docs/SCRAPINGMODULES.md docs/CLIHELP.md docs/FRONTENDS.md docs/PLATFORMS.md docs/USECASE.md hints.txt

unix:examples.path=/usr/local/etc/skyscraper
unix:examples.files=config.ini.example hints.xml artwork.xml artwork.xml.example1 artwork.xml.example2 artwork.xml.example3 artwork.xml.example4 aliasMap.csv mameMap.csv tgdb_developers.json tgdb_publishers.json tgdb_genres.json platforms.json screenscraper.json mobygames.json giantbomb.json spriters.json thegamesdb.json launchbox.json checksumcatalogs.json rawg.json mamehistory.json everygame.json openretro.json vggeek.json genresMap.json franchisesMap.json 

unix:cacheexamples.path=/usr/local/etc/skyscraper/cache
unix:cacheexamples.files=cache/priorities.xml.example

unix:impexamples.path=/usr/local/etc/skyscraper/import
unix:impexamples.files=import/definitions.dat.example1 import/definitions.dat.example2

unix:resexamples.path=/usr/local/etc/skyscraper/resources
unix:resexamples.files=resources/missingboxart.png resources/maskexample.png resources/frameexample.png resources/boxfront.png resources/boxside.png resources/scanlines1.png resources/scanlines2.png

unix:INSTALLS += target docs examples cacheexamples impexamples resexamples

include(./VERSION)
DEFINES+=VERSION=\\\"$$VERSION\\\"

HEADERS += src/skyscraper.h \
           src/netmanager.h \
           src/netcomm.h \
           src/xmlreader.h \
           src/settings.h \
           src/compositor.h \
           src/strtools.h \
           src/imgtools.h \
           src/esgamelist.h \
           src/scraperworker.h \
           src/cache.h \
           src/localscraper.h \
           src/importscraper.h \
           src/gameentry.h \
           src/abstractscraper.h \
           src/abstractfrontend.h \
           src/emulationstation.h \
           src/retrobat.h \
           src/attractmode.h \
           src/pegasus.h \
           src/xmlexport.h \
           src/koillection.h \
           src/openretro.h \
           src/thegamesdb.h \
           src/offlinetgdb.h \
           src/launchbox.h \
           src/chiptune.h \
           src/vgmaps.h \
           src/spriters.h \
           src/vgfacts.h \
           src/docsdb.h \
           src/mamehistory.h \
           src/everygame.h \
           src/vggeek.h \
           src/customflags.h \
           src/worldofspectrum.h \
           src/screenscraper.h \
           src/crc32.h \
           src/mobygames.h \
           src/exodos.h \
           src/gamebase.h \
           src/offlinemobygames.h \
           src/gamefaqs.h \
           src/rawg.h \
           src/igdb.h \
           src/giantbomb.h \
           src/arcadedb.h \
           src/platform.h \
           src/layer.h \
           src/fxshadow.h \
           src/fxblur.h \
           src/fxmask.h \
           src/fxframe.h \
           src/fxrounded.h \
           src/fxstroke.h \
           src/fxbrightness.h \
           src/fxcontrast.h \
           src/fxbalance.h \
           src/fxopacity.h \
           src/fxgamebox.h \
           src/fxhue.h \
           src/fxsaturation.h \
           src/fxcolorize.h \
           src/fxrotate.h \
           src/fxscanlines.h \
           src/nametools.h \
           src/queue.h

SOURCES += src/main.cpp \
           src/skyscraper.cpp \
           src/netmanager.cpp \
           src/netcomm.cpp \
           src/xmlreader.cpp \
           src/compositor.cpp \
           src/strtools.cpp \
           src/imgtools.cpp \
           src/esgamelist.cpp \
           src/scraperworker.cpp \
           src/cache.cpp \
           src/localscraper.cpp \
           src/importscraper.cpp \
           src/gameentry.cpp \
           src/abstractscraper.cpp \
           src/abstractfrontend.cpp \
           src/emulationstation.cpp \
           src/retrobat.cpp \
           src/attractmode.cpp \
           src/pegasus.cpp \
           src/xmlexport.cpp \
           src/koillection.cpp \
           src/openretro.cpp \
           src/thegamesdb.cpp \
           src/offlinetgdb.cpp \
           src/launchbox.cpp \
           src/chiptune.cpp \
           src/vgmaps.cpp \
           src/spriters.cpp \
           src/vgfacts.cpp \
           src/docsdb.cpp \
           src/mamehistory.cpp \
           src/everygame.cpp \
           src/vggeek.cpp \
           src/customflags.cpp \
           src/worldofspectrum.cpp \
           src/screenscraper.cpp \
           src/crc32.cpp \
           src/mobygames.cpp \
           src/gamebase.cpp \
           src/exodos.cpp \
           src/offlinemobygames.cpp \
           src/gamefaqs.cpp \ 
           src/rawg.cpp \
           src/igdb.cpp \
           src/giantbomb.cpp \
           src/arcadedb.cpp \
           src/platform.cpp \
           src/layer.cpp \
           src/fxshadow.cpp \
           src/fxblur.cpp \
           src/fxmask.cpp \
           src/fxframe.cpp \
           src/fxrounded.cpp \
           src/fxstroke.cpp \
           src/fxbrightness.cpp \
           src/fxcontrast.cpp \
           src/fxbalance.cpp \
           src/fxopacity.cpp \
           src/fxgamebox.cpp \
           src/fxhue.cpp \
           src/fxsaturation.cpp \
           src/fxcolorize.cpp \
           src/fxrotate.cpp \
           src/fxscanlines.cpp \
           src/nametools.cpp \
           src/queue.cpp
