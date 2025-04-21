/***************************************************************************
 *            scraperworker.cpp
 *
 *  Wed Jun 7 12:00:00 CEST 2017
 *  Copyright 2017 Lars Muldjord
 *  Copyright 2025 Risalt @ GitHub
 ****************************************************************************/
/*
 *  This file is part of skyscraper.
 *
 *  skyscraper is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  skyscraper is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with skyscraper; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#include <iostream>

#include <QTimer>

#include "scraperworker.h"
#include "strtools.h"
#include "nametools.h"
#include "settings.h"
#include "compositor.h"
#include "openretro.h"
#include "vggeek.h"
#include "exodos.h"
#include "gamebase.h"
#include "offlinetgdb.h"
#include "thegamesdb.h"
#include "launchbox.h"
#include "chiptune.h"
#include "vgmaps.h"
#include "spriters.h"
#include "customflags.h"
#include "worldofspectrum.h"
#include "screenscraper.h"
#include "igdb.h"
#include "rawg.h"
#include "giantbomb.h"
#include "gamefaqs.h"
#include "vgfacts.h"
#include "docsdb.h"
#include "everygame.h"
#include "mobygames.h"
#include "offlinemobygames.h"
#include "localscraper.h"
#include "importscraper.h"
#include "arcadedb.h"
#include "mamehistory.h"
#include "esgamelist.h"
#include "platform.h"
#include "skyscraper.h"


ScraperWorker::ScraperWorker(QSharedPointer<Queue> queue,
                             QSharedPointer<Cache> cache,
                             QSharedPointer<NetManager> manager,
                             Settings config,
                             QString threadId)
  : config(config), cache(cache), manager(manager), queue(queue), threadId(threadId)
{
  NameTool = new NameTools(threadId);
}

ScraperWorker::~ScraperWorker()
{
  delete NameTool;
}

// Instantiates the actual scraper and processes files from the list of files.
// Protected by mutex as there can be several scraper workers in parallel.
void ScraperWorker::run()
{
  if(!Platform::get().getScrapers(config.platform).contains(config.scraper) &&
     !config.generateLbDb && (!config.useChecksum || (config.useChecksum &&
       config.scraper != "mamehistory" && config.scraper != "arcadedb")))
  {
    printf("ERROR: The scraper selected does not support this platform. Exiting.\n");
    emit allDone(true);
    return;
  }

  if(config.scraper == "openretro") {
    scraper = new OpenRetro(&config, manager, threadId, NameTool);
  } else if(config.scraper == "vggeek") {
    scraper = new VGGeek(&config, manager, threadId, NameTool);
  } else if(config.scraper == "exodos") {
    scraper = new ExoDos(&config, manager, threadId, NameTool);
  } else if(config.scraper == "gamebase") {
    scraper = new GameBase(&config, manager, threadId, NameTool);
  } else if(config.scraper == "thegamesdb") {
    scraper = new TheGamesDb(&config, manager, threadId, NameTool);
  } else if(config.scraper == "offlinetgdb") {
    scraper = new OfflineTGDB(&config, manager, threadId, NameTool);
  } else if(config.scraper == "launchbox") {
    scraper = new LaunchBox(&config, manager, threadId, NameTool);
  } else if(config.scraper == "chiptune") {
    scraper = new Chiptune(&config, manager, threadId, NameTool);
  } else if(config.scraper == "vgmaps") {
    scraper = new VGMaps(&config, manager, threadId, NameTool);
  } else if(config.scraper == "spriters") {
    scraper = new Spriters(&config, manager, threadId, NameTool);
  } else if(config.scraper == "customflags") {
    scraper = new CustomFlags(&config, manager, threadId, NameTool);
  } else if(config.scraper == "arcadedb") {
    scraper = new ArcadeDB(&config, manager, threadId, NameTool);
  } else if(config.scraper == "mamehistory") {
    scraper = new MAMEHistory(&config, manager, threadId, NameTool);
  } else if(config.scraper == "screenscraper") {
    scraper = new ScreenScraper(&config, manager, threadId, NameTool);
  } else if(config.scraper == "igdb") {
    scraper = new Igdb(&config, manager, threadId, NameTool);
  } else if(config.scraper == "giantbomb") {
    scraper = new GiantBomb(&config, manager, threadId, NameTool);
  } else if(config.scraper == "gamefaqs") {
    scraper = new GameFaqs(&config, manager, threadId, NameTool);
  } else if(config.scraper == "vgfacts") {
    scraper = new VGFacts(&config, manager, threadId, NameTool);
  } else if(config.scraper == "docsdb") {
    scraper = new DocsDB(&config, manager, threadId, NameTool);
  } else if(config.scraper == "everygame") {
    scraper = new EveryGame(&config, manager, threadId, NameTool);
  } else if(config.scraper == "rawg") {
    scraper = new RawG(&config, manager, threadId, NameTool);
  } else if(config.scraper == "mobygames") {
    scraper = new MobyGames(&config, manager, threadId, NameTool);
  } else if(config.scraper == "offlinemobygames") {
    scraper = new OfflineMobyGames(&config, manager, threadId, NameTool);
  } else if(config.scraper == "worldofspectrum") {
    scraper = new WorldOfSpectrum(&config, manager, threadId, NameTool);
  } else if(config.scraper == "esgamelist") {
    scraper = new ESGameList(&config, manager, threadId, NameTool);
  } else if(config.scraper == "cache") {
    scraper = new LocalScraper(&config, manager, threadId, NameTool);
  } else if(config.scraper == "import") {
    scraper = new ImportScraper(&config, manager, threadId, NameTool);
  } else {
    scraper = new AbstractScraper(&config, manager, threadId, NameTool);
  }

  QString error = "\033[1;33m(T" + threadId + ")\033[0m ";
  if(limitReached(error)) {
    printf(error.toStdString().c_str()); fflush(stdout);
    delete scraper;
    emit allDone(true);
    return;
  }

  Compositor compositor(&config);
  if(!compositor.processXml()) {
    printf("Something went wrong when parsing artwork xml from '%s', please check the file for errors. Now exiting...\n",
           config.artworkConfig.toStdString().c_str());
    delete scraper;
    emit allDone(true);
    return;
  }

  while(queue->hasEntry()) {
    // takeEntry() also unlocks the mutex that was locked in hasEntry()
    QFileInfo info = queue->takeEntry();
    QString output = "\033[1;33m(T" + threadId + ")\033[0m ";
    QString lowMatch = "";
    QString debug = "";
    QString cacheId = cache->getQuickId(info);
    if(cacheId.isEmpty()) {
      cacheId = NameTools::getCacheId(info);
      cache->addQuickId(info, cacheId);
    }

    // Create the game entry we use for the rest of the process
    GameEntry game, game2;
    GameEntry cachedGame;

    // Create list for potential game entries that will come from the scraping source
    QList<GameEntry> gameEntries, gameEntries2;

    if(!config.rescan) {
      game2.found = false;
      game2.searchMatch = 0;
    }
    bool fromCache = false;
    bool potentialUpdates = false;
    bool hasMeaningfulOwnEntries = false;
    bool hasMeaningfulEntries = false;
    bool hasOwnEntries = false;
    QString action = "nothing";
    if(config.scraper == "cache") {
      action = "cache";
    } else {
      hasMeaningfulOwnEntries = cache->hasMeaningfulEntries(cacheId, config.scraper);
      hasMeaningfulEntries = hasMeaningfulOwnEntries || cache->hasMeaningfulEntries(cacheId);
      hasOwnEntries = hasMeaningfulOwnEntries || cache->hasEntries(cacheId, config.scraper);
      if(config.rescan) {
        if(hasOwnEntries) {
          cachedGame.cacheId = cacheId;
          cache->fillBlanks(cachedGame, config.scraper);
          action = "update";
        } else {
          action = "skip";
        }
      } else if(!config.refresh && hasOwnEntries && config.getMissingResources) {
        cachedGame.cacheId = cacheId;
        cache->fillBlanks(cachedGame, config.scraper);
        if(cachedGame.getCompleteness() < 99.0) {
            action = "update";
        }
      }
      if(action == "nothing") {
        if(config.onlyMissing && !hasMeaningfulEntries) {
          action = "refresh";
        } else if(config.onlyMissing && hasMeaningfulEntries && !(config.refresh && hasOwnEntries)) {
          action = "skip";
        } else if(!config.onlyMissing && !config.refresh) {
          if(hasOwnEntries) {
            action = "skip";
          } else {
            action = "refresh";
          }
        } else {
          action = "refresh";
        }
      }
    }
    if(config.verbosity >= 1) {
      printf("INFO: Action is %s\n", action.toStdString().c_str());
      printf("INFO: Action path is (oM,r,rsc,mR,%%,MOE,ME,OE): %d %d %d %d %d %d %d %d\n",
             config.onlyMissing, config.refresh, config.rescan, config.getMissingResources,
             cachedGame.getCompleteness(), hasMeaningfulOwnEntries, hasMeaningfulEntries,
             hasOwnEntries);
    }

    CanonicalData canonicalData;
    QFileInfo searchInfo = info;
    QString compareTitle, compareTitle2;
    // We delay the calculation of checksums until it is decided that it is a refresh or an update:
    if(action == "refresh" || action == "update") {
      if((config.rescan || config.useChecksum) && config.scraper != "screenscraper") {
        canonicalData = NameTool->getCanonicalData(info);
        if(config.scraper == "arcadedb" || config.scraper == "mamehistory") {
          if(canonicalData.mameid.isEmpty() && !config.rescan) {
            game.found = false;
            game.title = StrTools::stripBrackets(info.completeBaseName());
            game.platform = Platform::get().getAliases(config.platform).at(1);
            output.append("\033[1;33m---- Game '" + info.completeBaseName() +
                          "' not found in the checksum MAME database, skipping... :( ----\033[0m\n\n");
            game.resetMedia();
            emit entryReady(game, output, debug, lowMatch);
            if(forceEnd) {
              break;
            } else {
              continue;
            }
          } else if(!canonicalData.mameid.isEmpty()) {
            searchInfo = QFileInfo(canonicalData.mameid + ".zip");
            game.canonical = canonicalData;
          }
        } else {
          if(canonicalData.name.isEmpty() && !config.rescan) {
            game.found = false;
            game.title = StrTools::stripBrackets(info.completeBaseName());
            game.platform = Platform::get().getAliases(config.platform).at(1);
            output.append("\033[1;33m---- Game '" + info.completeBaseName() +
                          "' not found in the checksum database, skipping... :( ----\033[0m\n\n");
            game.resetMedia();
            emit entryReady(game, output, debug, lowMatch);
            if(forceEnd) {
              break;
            } else {
              continue;
            }
          } else if(!canonicalData.name.isEmpty()) {
            searchInfo = QFileInfo(canonicalData.name + "." + info.suffix());
            game.canonical = canonicalData;
          }
        }
      }
      compareTitle = scraper->getCompareTitle(searchInfo);
      compareTitle2 = scraper->getCompareTitle(info);
    }

    if(action == "nothing") {
      printf("\nERROR: Unforeseen scenario has happened in ScraperWorker::run: "
             "Action path is (oM,r,rsc,mR,%%,MOE,ME,OE): %d %d %d %d %d %d %d %d\n",
             config.onlyMissing, config.refresh, config.rescan, config.getMissingResources,
             cachedGame.getCompleteness(), hasMeaningfulOwnEntries, hasMeaningfulEntries,
             hasOwnEntries);
      break;
    } else if(action == "cache") {
      if(!cache->hasMeaningfulEntries(cacheId)) {
        cachedGame.emptyShell = true;
      }
      fromCache = true;
      cachedGame.cacheId = cacheId;
      cache->fillBlanks(cachedGame);
      if(cachedGame.title.isEmpty()) {
        cachedGame.title = StrTools::stripBrackets(info.completeBaseName());
      }
      if(cachedGame.platform.isEmpty()) {
        cachedGame.platform = Platform::get().getAliases(config.platform).at(1);
      }
      gameEntries.append(cachedGame);
    } else if(action == "update") {
      fromCache = true;
      if(cachedGame.title.isEmpty()) {
        cachedGame.title = compareTitle;
      }
      if(cachedGame.platform.isEmpty()) {
        cachedGame.platform = config.platform;
      }
      scraper->runPasses(gameEntries, searchInfo, info, output, debug, cachedGame.title);
      if(config.rescan) {
        if(compareTitle != compareTitle2) {
          scraper->runPasses(gameEntries2, info, info, output, debug, cachedGame.title);
        } else if(config.scraper == "screenscraper") {
          config.useChecksum = true;
          scraper->runPasses(gameEntries2, info, info, output, debug, cachedGame.title);
          config.useChecksum = false;
        }
        if(gameEntries.isEmpty() && !gameEntries2.isEmpty()) {
          gameEntries.swap(gameEntries2);
          searchInfo = info;
          compareTitle = compareTitle2;
        }
      }
      potentialUpdates = true;
    } else if(action == "refresh") {
      scraper->runPasses(gameEntries, searchInfo, info, output, debug);
      if(config.verbosity >= 5) {
        qDebug() << "Calculated gameEntries:" << gameEntries << "\nfor file:" << info;
      }
    } else if(action == "skip") {
      // Skip this file. 'onlymissing' has been set or the game had already been (fully) scraped.
      game.found = false;
      game.title = compareTitle;
      output.append("\033[1;33m---- Skipping game '" + info.completeBaseName() + "' ----\033[0m\n\n");
      game.resetMedia();
      emit entryReady(game, output, debug, lowMatch);
      if(forceEnd) {
        break;
      } else {
        continue;
      }
    }

    // No game name will be longer than this:
    int lowestDistance = 10000, lowestDistance2 = 10000;
    int stringSize = 0, stringSize2 = 0;
    if(gameEntries.isEmpty()) {
      if(config.brackets) {
        // fix for issue #47
        game.title = info.completeBaseName();
      } else {
        game.title = compareTitle;
      }
      game.found = false;
      game2.found = false;
    } else {
      game = getBestEntry(gameEntries, compareTitle, lowestDistance, stringSize);
      if(gameEntries2.isEmpty()) {
        game2.found = false;
      } else {
        game2 = getBestEntry(gameEntries2, compareTitle2, lowestDistance2, stringSize2);
      }
      if(config.interactive && !fromCache && !config.rescan) {
        game = getEntryFromUser(gameEntries, game, compareTitle, lowestDistance);
      }
      if(config.verbosity > 2) {
        qDebug() << "Selected entry:" << game << "for" << compareTitle <<
                    "with distance" << lowestDistance;
        if(!gameEntries2.isEmpty()) {
          qDebug() << "Selected entry (2):" << game2 << "for" << compareTitle2 <<
                      "with distance" << lowestDistance2;
        }
      }
    }
    // Fill it with additional needed data
    game.path = info.absoluteFilePath();
    game.baseName = info.completeBaseName();
    game.absoluteFilePath = info.absoluteFilePath();
    game.cacheId = cacheId;
    game.canonical = canonicalData;

    // Sort out brackets here prior to not found checks, in case user has 'skipped="true"' set
    game.sqrNotes = NameTools::getSqrNotes(game.title);
    game.parNotes = NameTools::getParNotes(game.title);
    game.sqrNotes.append(NameTools::getSqrNotes(info.completeBaseName()));
    game.parNotes.append(NameTools::getParNotes(info.completeBaseName()));
    game.sqrNotes = NameTools::getUniqueNotes(game.sqrNotes, '[');
    game.parNotes = NameTools::getUniqueNotes(game.parNotes, '(');

    if(game2.found) {
      game2.path = info.absoluteFilePath();
      game2.baseName = info.completeBaseName();
      game2.absoluteFilePath = info.absoluteFilePath();
      game2.cacheId = cacheId;
      game2.canonical = canonicalData;
      game2.sqrNotes = NameTools::getSqrNotes(game2.title);
      game2.parNotes = NameTools::getParNotes(game2.title);
      game2.sqrNotes.append(NameTools::getSqrNotes(info.completeBaseName()));
      game2.parNotes.append(NameTools::getParNotes(info.completeBaseName()));
      game2.sqrNotes = NameTools::getUniqueNotes(game2.sqrNotes, '[');
      game2.parNotes = NameTools::getUniqueNotes(game2.parNotes, '(');
    }

    if(cachedGame.emptyShell) {
      output.append("\033[1;33m---- Game '" + info.completeBaseName() +
                    "' not found in cache :( ----\033[0m\n\n");
    }

    if(!game.found && !game2.found) {
      if(!config.rescan) {
        scraper->addLastSearchToNegativeCache(info.absoluteFilePath());
      }
      output.append("\033[1;33m---- Game '" + info.completeBaseName() +
                    "' not found :( ----\033[0m\n\n");
      if(config.rescan) {
        if(cache->removeResources(cacheId, config.scraper)) {
          output.append("\033[1;34m---- WARNING: Game '" + info.completeBaseName() +
                        "' stored in cache does not exist in the scraper database!" +
                        " Deleting the cache records... ----\033[0m\n");
        }
      } else {
        if(!forceEnd)
          forceEnd = limitReached(output);
      }
      if(game.title.isEmpty()) {
        game.title = StrTools::stripBrackets(info.completeBaseName());
      }
      if(game.platform.isEmpty()) {
        game.platform = Platform::get().getAliases(config.platform).at(1);
      }
      game.resetMedia();
      emit entryReady(game, output, debug, lowMatch);
      if(forceEnd) {
        break;
      } else {
        continue;
      }
    } else if(!game.found && game2.found) {
      game = game2;
      compareTitle = compareTitle2;
      lowestDistance = lowestDistance2;
      stringSize = stringSize2;
    }

    game.searchMatch = getSearchMatch(game.title, compareTitle, lowestDistance, stringSize);
    if(game2.found) {
      game2.searchMatch = getSearchMatch(game2.title, compareTitle2, lowestDistance2, stringSize2);
    }
    if(config.verbosity > 2) {
      qDebug() << "Search match:" << game.searchMatch << "for" << compareTitle <<
                  "and" << game.title << "Threshold" << config.minMatch;
      if(game2.found) {
        qDebug() << "Search match (2):" << game2.searchMatch << "for" << compareTitle2 <<
                    "and" << game2.title << "Threshold" << config.minMatch;
      }
    }
    if(game.searchMatch < config.minMatch && game2.searchMatch < config.minMatch) {
      output.append("\033[1;33m---- File '" + info.completeBaseName() +
                    "' match (" + game.title + ") is too low :| ----\033[0m\n\n");
      lowMatch.append(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ";" + 
                      config.platform + ";" + config.scraper + ";" + game.title + ";" +
                      info.completeBaseName() + ";" + QString::number(game.searchMatch) + ";" + 
                      info.canonicalFilePath() + "\n");
      if(game2.found) {
        output.append("\033[1;33m---- File '" + info.completeBaseName() +
                      "' match (" + game2.title + ") is too low :| ----\033[0m\n\n");
        lowMatch.append(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ";" + 
                        config.platform + ";" + config.scraper + ";" + game2.title + ";" +
                        info.completeBaseName() + ";" + QString::number(game2.searchMatch) + ";" + 
                        info.canonicalFilePath() + "\n");
      }
      game.found = false;
      game2.found = false;
      if(!config.rescan) {
        scraper->addLastSearchToNegativeCache(info.absoluteFilePath(), game.title);
      }
      if(config.rescan) {
        if(cache->removeResources(cacheId, config.scraper)) {
          output.append("\033[1;34m---- WARNING: Game '" + info.completeBaseName() +
                        "' stored in cache does not exist in the scraper database!" +
                        " Deleting the cache records... ----\033[0m\n");
        }
      } else {
        if(!forceEnd)
          forceEnd = limitReached(output);
      }
      if(game.title.isEmpty()) {
        game.title = StrTools::stripBrackets(info.completeBaseName());
      }
      if(game.platform.isEmpty()) {
        game.platform = Platform::get().getAliases(config.platform).at(1);
      }
      game.resetMedia();
      emit entryReady(game, output, debug, lowMatch);
      if(forceEnd) {
        break;
      } else {
        continue;
      }
    } else if (game.searchMatch < config.minMatch && game2.searchMatch >= config.minMatch) {
      game = game2;
      compareTitle = compareTitle2;
    }

    output.append("\033[1;34m---- Game '" + info.completeBaseName() +
                  "' found! :) ----\033[0m\n");

    if(fromCache && config.rescan) {
      if(game.found && cachedGame.title == game.title && cachedGame.platform == game.platform) {
        game2.found = false;
      } else if(game2.found && cachedGame.title == game2.title && cachedGame.platform == game2.platform) {
        game = game2;
        compareTitle = compareTitle2;
      } else {
        fromCache = false;
        cachedGame.resetMedia();
        cache->removeResources(cacheId, config.scraper);
        if(game.searchMatch < game2.searchMatch) {
          game = game2;
          compareTitle = compareTitle2;
        }
        output.append("\033[1;34m---- WARNING: Game '" + info.completeBaseName() +
                      "' stored in cache (" + cachedGame.title + "/" + cachedGame.platform +
                      ") does not match the scraper database (" + game.title + "/" +
                      game.platform + ")!" + " Repeating the scraping process... ----\033[0m\n");
      }
    }

    QStringList sharedBlobs = {};
    if(config.scraper != "cache") {
      if(cache->hasEntriesOfType(cacheId, "video")) {
        sharedBlobs << "video";
      }
      if(cache->hasEntriesOfType(cacheId, "manual")) {
        sharedBlobs << "manual";
      }
      if(cache->hasEntriesOfType(cacheId, "cover")) {
        sharedBlobs << "cover";
      }
      if(cache->hasEntriesOfType(cacheId, "marquee")) {
        sharedBlobs << "marquee";
      }
      if(cache->hasEntriesOfType(cacheId, "texture")) {
        sharedBlobs << "texture";
      }
      if(cache->hasEntriesOfType(cacheId, "screenshot")) {
        sharedBlobs << "screenshot";
      }
      if(cache->hasEntriesOfType(cacheId, "wheel")) {
        sharedBlobs << "wheel";
      }
      if(cache->hasMeaningfulEntries(cacheId, config.scraper, true)) {
        sharedBlobs << "offlineonly";
      }
      if(!fromCache) {
        scraper->getGameData(game, sharedBlobs);
      }
      else if(config.getMissingResources) {
        scraper->getGameData(game, sharedBlobs, &cachedGame);
      }
    }

    if(!config.pretend && config.scraper == "cache") {
      // Process all artwork
      compositor.saveAll(game, info.completeBaseName());
      // Copy or symlink videos as requested
      if(config.videos &&
         !game.videoFormat.isEmpty() &&
         !game.videoFile.isEmpty() &&
         QFile::exists(game.videoFile)) {
        QString videoDst = config.videosFolder + "/" + info.completeBaseName() +
                           "." + game.videoFormat;
        if(!config.skipExistingVideos || !QFile::exists(videoDst)) {
          if(QFile::exists(videoDst)) {
            QFile::remove(videoDst);
          }
          if(config.symlink) {
            // Try to remove existing video destination file before linking
            if(!QFile::link(game.videoFile, videoDst)) {
              game.videoFormat = "";
            }
          } else {
            QFile videoFile(videoDst);
            if(videoFile.open(QIODevice::WriteOnly)) {
              videoFile.write(game.videoData);
              videoFile.close();
            } else {
              game.videoFormat = "";
            }
          }
        }
      }
      // Copy or symlink manuals as requested
      if(config.manuals &&
         !game.manualFormat.isEmpty() &&
         !game.manualFile.isEmpty() &&
         QFile::exists(game.manualFile)) {
        QString manualDst = config.manualsFolder + "/" + info.completeBaseName() +
                            "." + game.manualFormat;
        if(config.skipExistingManuals && QFile::exists(manualDst)) {
        } else {
          if(QFile::exists(manualDst)) {
            QFile::remove(manualDst);
          }
          if(config.symlink) {
            // Try to remove existing manual destination file before linking
            if(!QFile::link(game.manualFile, manualDst)) {
              game.manualFormat = "";
            }
          } else {
            QFile manualFile(manualDst);
            if(manualFile.open(QIODevice::WriteOnly)) {
              manualFile.write(game.manualData);
              manualFile.close();
            } else {
              game.manualFormat = "";
            }
          }
        }
      }
    }

    // Add all resources to the cache
    QString cacheOutput = "";
    if(config.scraper != "cache" && game.found && (!fromCache || potentialUpdates)) {
     // Very ugly hack because it's actually more than one database (3/3):
      if(config.scraper == "docsdb") {
        game.source = Skyscraper::docType;
      } else {
        game.source = config.scraper;
      }

      cache->addResources(game, config, cacheOutput);
    }

    // We're done saving the raw data at this point, so feel free to manipulate
    // game resources to better suit game list creation from here on out.

    // Strip any brackets from the title as they will be read when assembling gamelist
    game.title = StrTools::stripBrackets(game.title);

    // Move 'The' or ', The' depending on the config. This does not affect game list sorting.
    // 'The ' is always removed before sorting.
    game.title = NameTools::moveArticle(game.title, config.theInFront);

    // Don't unescape title since we already did that in getBestEntry()
    if(!game.videoFormat.isEmpty()) {
      game.videoFile = StrTools::xmlUnescape(config.videosFolder + "/" +
                       info.completeBaseName() + "." + game.videoFormat);
    }
    if(!game.manualFormat.isEmpty()) {
      game.manualFile = StrTools::xmlUnescape(config.manualsFolder + "/" +
                        info.completeBaseName() + "." + game.manualFormat);
    }
    game.chiptuneId = StrTools::xmlUnescape(game.chiptuneId);
    game.chiptunePath = StrTools::xmlUnescape(game.chiptunePath);
    game.guides = StrTools::xmlUnescape(game.guides);
    game.cheats = StrTools::xmlUnescape(game.cheats);
    game.reviews = StrTools::xmlUnescape(game.reviews);
    game.artbooks = StrTools::xmlUnescape(game.artbooks);
    game.vgmaps = StrTools::xmlUnescape(game.vgmaps);
    game.sprites = StrTools::xmlUnescape(game.sprites);
    game.description = StrTools::xmlUnescape(game.description);
    game.trivia = StrTools::xmlUnescape(game.trivia);
    game.releaseDate = StrTools::xmlUnescape(game.releaseDate);
    // Make sure we have the correct 'yyyymmdd' format of 'releaseDate'
    game.releaseDate = StrTools::conformReleaseDate(game.releaseDate);
    game.developer = StrTools::xmlUnescape(game.developer);
    game.publisher = StrTools::xmlUnescape(game.publisher);
    game.tags = StrTools::xmlUnescape(game.tags);
    game.tags = StrTools::conformTags(game.tags);
    game.franchises = StrTools::xmlUnescape(game.franchises);
    game.franchises = StrTools::conformTags(game.franchises);
    game.rating = StrTools::xmlUnescape(game.rating);
    game.players = StrTools::xmlUnescape(game.players);
    // Make sure we have the correct single digit format of 'players'
    game.players = StrTools::conformPlayers(game.players);
    game.ages = StrTools::xmlUnescape(game.ages);
    // Make sure we have the correct format of 'ages'
    game.ages = StrTools::conformAges(game.ages);

    output.append("Scraper:        " + config.scraper + "\n");
    if(config.scraper != "cache" && config.scraper != "import") {
      output.append("From cache:     " + QString((fromCache?"YES (refresh from source with '--cache refresh')":"NO")) + "\n");
      output.append("Search match:   " + QString::number(game.searchMatch) + " %\n");
      output.append("Compare title:  '\033[1;32m" + compareTitle + "\033[0m'\n");
      output.append("Result title:   '\033[1;32m" + game.title + "\033[0m' (" + game.titleSrc + ")\n");
    } else {
      output.append("Title:          '\033[1;32m" + game.title + "\033[0m' (" + game.titleSrc + ")\n");
    }
    if(!config.nameTemplate.isEmpty()) {
      game.title = StrTools::xmlUnescape(NameTools::getNameFromTemplate(game,
                                                                        config.nameTemplate));
    } else {
      game.title = StrTools::xmlUnescape(game.title);
      if(config.forceFilename) {
        game.title = StrTools::xmlUnescape(StrTools::stripBrackets(info.completeBaseName()));
      }
      if(config.brackets) {
        game.title.append(StrTools::xmlUnescape((game.parNotes != ""?" " + game.parNotes:QString("")) +
                                                (game.sqrNotes != ""?" " + game.sqrNotes:QString(""))));
      }
    }
    output.append("Platform:       '\033[1;32m" + game.platform + "\033[0m' (" + game.platformSrc + ")\n");
    output.append("Release Date:   '\033[1;32m");
    if(game.releaseDate.isEmpty()) {
      output.append("\033[0m' ()\n");
    } else {
      output.append(QDate::fromString(game.releaseDate, "yyyyMMdd").toString("yyyy-MM-dd") + "\033[0m' (" + game.releaseDateSrc + ")\n");
    }
    output.append("Id:             '\033[1;32m" + game.id + "\033[0m' (" + game.idSrc + ")\n");
    output.append("Developer:      '\033[1;32m" + game.developer + "\033[0m' (" + game.developerSrc + ")\n");
    output.append("Publisher:      '\033[1;32m" + game.publisher + "\033[0m' (" + game.publisherSrc + ")\n");
    output.append("Players:        '\033[1;32m" + game.players + "\033[0m' (" + game.playersSrc + ")\n");
    output.append("Ages:           '\033[1;32m" + game.ages + (game.ages.toInt() != 0?"+":"") + "\033[0m' (" + game.agesSrc + ")\n");
    output.append("Tags:           '\033[1;32m" + game.tags + "\033[0m' (" + game.tagsSrc + ")\n");
    output.append("Franchises:     '\033[1;32m" + game.franchises + "\033[0m' (" + game.franchisesSrc + ")\n");
    output.append("Rating (0-1):   '\033[1;32m" + game.rating + "\033[0m' (" + game.ratingSrc + ")\n");
    output.append("Guides:         '\033[1;32m" + game.guides + "\033[0m' (" + game.guidesSrc + ")\n");
    output.append("Cheats:         '\033[1;32m" + game.cheats + "\033[0m' (" + game.cheatsSrc + ")\n");
    output.append("Reviews:        '\033[1;32m" + game.reviews + "\033[0m' (" + game.reviewsSrc + ")\n");
    output.append("Artbooks:       '\033[1;32m" + game.artbooks + "\033[0m' (" + game.artbooksSrc + ")\n");
    output.append("Maps:           '\033[1;32m" + game.vgmaps + "\033[0m' (" + game.vgmapsSrc + ")\n");
    output.append("Sprites:        '\033[1;32m" + game.sprites + "\033[0m' (" + game.spritesSrc + ")\n");
    output.append("Cover:          " + QString(((game.coverData.isNull() && game.coverFile.isEmpty())?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((config.cacheCovers || config.scraper == "cache"?"":" (uncached)")) + " (" + game.coverSrc + ")\n");
    output.append("Screenshot:     " + QString(((game.screenshotData.isNull() && game.coverFile.isEmpty())?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((config.cacheScreenshots || config.scraper == "cache"?"":" (uncached)")) + " (" + game.screenshotSrc + ")\n");
    output.append("Wheel:          " + QString(((game.wheelData.isNull() && game.wheelFile.isEmpty())?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((config.cacheWheels || config.scraper == "cache"?"":" (uncached)")) + " (" + game.wheelSrc + ")\n");
    output.append("Marquee:        " + QString(((game.marqueeData.isNull() && game.marqueeFile.isEmpty())?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((config.cacheMarquees || config.scraper == "cache"?"":" (uncached)")) + " (" + game.marqueeSrc + ")\n");
    output.append("Texture:        " + QString(((game.textureData.isNull() && game.textureFile.isEmpty())? "\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((config.cacheTextures || config.scraper == "cache"?"":" (uncached)")) + " (" + game.textureSrc + ")\n");
    if(config.videos) {
      output.append("Video:          " + QString((game.videoFormat.isEmpty()?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((game.videoData.size() <= config.videoSizeLimit?"":" (size exceeded, uncached)")) + " (" + game.videoSrc + ")\n");
    }
    if(config.manuals) {
      output.append("Manual:         " + QString((game.manualFormat.isEmpty()?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + QString((game.manualData.size() <= config.manualSizeLimit?"":" (size exceeded, uncached)")) + " (" + game.manualSrc + ")\n");
    }
    if(config.chiptunes) {
      output.append("Chiptunes:      " + QString((game.chiptuneId.isEmpty()?"\033[1;31mNO":"\033[1;32mYES")) + "\033[0m" + " (" + game.chiptuneIdSrc + ")\n");
    }
    output.append("\nDescription: (" + game.descriptionSrc + ")\n'\033[1;32m" + game.description.left(config.maxLength) + "\033[0m'\n");
    output.append("\nTrivia: (" + game.triviaSrc + ")\n'\033[1;32m" + game.trivia.left(config.maxLength) + "\033[0m'\n");
    if(!cacheOutput.isEmpty()) {
      output.append("\n\033[1;33mCache output:\033[0m\n" + cacheOutput + "\n");
    }
    if(!forceEnd) {
      forceEnd = limitReached(output);
    }
    game.resetMedia();
    cachedGame.resetMedia();
    emit entryReady(game, output, debug, lowMatch);
    if(forceEnd) {
      break;
    }
  }

  delete scraper;
  emit allDone();
}

bool ScraperWorker::limitReached(QString &output)
{
  if(scraper->reqRemaining != -1 || scraper->reqRemainingKO != -1) { // -1 means there is no limit
    if(scraper->reqRemaining && scraper->reqRemainingKO) {
      output.append("\n\033[1;33m'" + config.scraper + "' requests remaining/remaining KO: " +
                    QString::number(scraper->reqRemaining) + "/" +
                    QString::number(scraper->reqRemainingKO) + "\033[0m\n");
    } else {
      output.append("\033[1;31mForcing thread " + threadId + " to stop...\033[0m\n");
      return true;
    }
  }
  return false;
}

// Calculate a percentage of match between two strings, taking into account their length
// and the Levenshtein-Damerau distance.
int ScraperWorker::getSearchMatch(const QString &title, const QString &compareTitle,
                                  const int lowestDistance, const int stringSize)
{
  double titleLength, compareLength;
  if(!stringSize) {
    titleLength = std::max(1.0, (double)title.length());
    compareLength = std::max(1.0, (double)compareTitle.length());
  } else {
    titleLength = compareLength = (double)stringSize;
  }

  int searchMatch = 0;
  if(titleLength > compareLength) {
    searchMatch = (int)(100.0 / titleLength * (titleLength - (double)lowestDistance));
  } else {
    searchMatch = (int)(100.0 / compareLength * (compareLength - (double)lowestDistance));
  }

  // RMA: Modified as the original was not acceptable, bringed too many false positives
  // Special case where result is actually correct, but gets low match because of the
  // prepending of, for instance, "Disney's [title]" where the fileName is just "[title]".
  // Accept these if searchMatch is 'similar enough' (above 50)
  if(searchMatch < config.minMatch) {
    int excludePreffix = title.toLower().indexOf(compareTitle.toLower(),
                                                 title.indexOf(' ') + 1);
    if(title.toLower().mid(excludePreffix) == compareTitle.toLower() &&
       searchMatch >= 60) {
      searchMatch = 90;
    } else {
      excludePreffix = compareTitle.toLower().indexOf(title.toLower(),
                                                      compareTitle.indexOf(' ') + 1);
      if(compareTitle.toLower().mid(excludePreffix) == title.toLower() &&
         searchMatch >= 60) {
        searchMatch = 90;
      }
    }
  }

  return searchMatch;
}

// Returns the entry from gameEntries with the lowest distance to compareTitle,
// and that lowest distance.
GameEntry ScraperWorker::getBestEntry(const QList<GameEntry> &gameEntries,
                                      QString compareTitle,
                                      int &lowestDistance, int &stringSize)
{
  GameEntry game;
  // If scraper isn't filename search based, always return first entry
  if(config.scraper == "cache"          || config.scraper == "import"         ||
     config.scraper == "esgamelist"     || config.scraper == "customflags"    ||
     config.scraper == "arcadedb"       || config.scraper == "mamehistory"    ||
     config.scraper == "exodos"         ||
     (config.scraper == "openretro"     && gameEntries.first().url.isEmpty()) ||
     (config.scraper == "screenscraper" && 
      (Platform::get().getFamily(config.platform) == "arcade" ||
       config.useChecksum))) {
    lowestDistance = 0;
    game = gameEntries.first();
    game.title = StrTools::xmlUnescape(game.title);
    return game;
  }

  QList<GameEntry> potentials;

  // Remove all brackets from name, since we pretty much NEVER want these
  compareTitle = compareTitle.left(compareTitle.indexOf("(")).simplified();
  compareTitle = compareTitle.left(compareTitle.indexOf("[")).simplified();
  int compareNumeral = NameTools::getNumeral(compareTitle);
  // Start by applying rules we are certain are needed. Add the ones that pass to potentials
  for(auto entry: gameEntries) {
    entry.title = StrTools::xmlUnescape(entry.title);
    if(config.verbosity >= 2) {
      qDebug() << "Comparison: " << compareTitle << ":" << entry.title;
    }

    // Remove all brackets from name, since we pretty much NEVER want these
    entry.title = entry.title.left(entry.title.indexOf("(")).simplified();
    entry.title = entry.title.left(entry.title.indexOf("[")).simplified();
    int entryNumeral = NameTools::getNumeral(entry.title);
    // If numerals don't match, skip.
    // Numeral defaults to 1, even for games without a numeral.
    if(compareNumeral != entryNumeral) {
      continue;
    }

    potentials.append(entry);
  }

  // If we have no potentials at all, return false
  if(potentials.isEmpty()) {
    game.found = false;
    return game;
  }

  int mostSimilar = 0;
  QString compareTitleLC = StrTools::simplifyLetters(NameTools::convertToIntegerNumeral(compareTitle).toLower());
  // Remove some typical keywords that are sometimes omitted (such as "Disney's"):
  compareTitleLC.remove("disney's").remove("disneys");
  QString compareTitleLCNoArticle = NameTools::removeArticle(compareTitleLC.simplified());
  QString compareTitleLCNoArticleNoEdition = NameTools::removeEdition(compareTitleLCNoArticle);
  // Run through the potentials and find the best match
  for(int a = 0; a < potentials.length(); ++a) {
    QString entryTitleLC = StrTools::simplifyLetters(NameTools::convertToIntegerNumeral(potentials.at(a).title).toLower());
    // Remove some typical keywords that are sometimes omitted (such as "Disney's"):
    entryTitleLC.remove("disney's").remove("disneys");
    QString entryTitleLCNoArticle = NameTools::removeArticle(entryTitleLC.simplified());
    QString entryTitleLCNoArticleNoEdition = NameTools::removeEdition(entryTitleLCNoArticle);

    // If we have a perfect hit, always use this result
    if(compareTitleLC == entryTitleLC) {
      lowestDistance = 0;
      game = potentials.at(a);
      return game;
    }

    // Check if game is exact match if "The" at either end is manipulated
    bool match = false;
    if(compareTitleLCNoArticle == entryTitleLCNoArticle) {
      match = true;
    }
    if(match) {
      lowestDistance = 0;
      game = potentials.at(a);
      return game;
    }

    // Check if game is exact match if the "xxx Edition" is removed
    match = false;
    if(compareTitleLCNoArticleNoEdition == entryTitleLCNoArticleNoEdition) {
      match = true;
    }
    if(match) {
      lowestDistance = 0;
      game = potentials.at(a);
      return game;
    }

    // Compare all words of compareTitle and entryTitle. If all words with a length of
    // more than 3 letters are found in the entry words, return match
    QStringList compareWords = compareTitleLCNoArticleNoEdition.split(" ");
    for(int b = 0; b < compareWords.size(); ++b) {
      if(compareWords.at(b).length() < 3)
        compareWords.removeAt(b);
    }
    QStringList entryWords = entryTitleLCNoArticleNoEdition.split(" ");
    for(int b = 0; b < entryWords.size(); ++b) {
      if(entryWords.at(b).length() < 3)
        entryWords.removeAt(b);
    }
    // Only perform check if there's 3 or more words in compareTitle
    if(compareWords.size() >= 3) {
      int wordsFound = 0;
      for(const auto &compareWord: std::as_const(compareWords)) {
        for(const auto &entryWord: std::as_const(entryWords)) {
          if(entryWord == compareWord) {
            wordsFound++;
            break;
          }
        }
      }
      if(wordsFound == compareWords.size()) {
        lowestDistance = 0;
        game = potentials.at(a);
        return game;
      }
    }
    // Only perform check if there's 3 or more words in entryTitle
    if(entryWords.size() >= 3) {
      int wordsFound = 0;
      for(const auto &entryWord: std::as_const(entryWords)) {
        for(const auto &compareWord: std::as_const(compareWords)) {
          if(compareWord == entryWord) {
            wordsFound++;
            break;
          }
        }
      }
      if(wordsFound == entryWords.size()) {
        lowestDistance = 0;
        game = potentials.at(a);
        return game;
      }
    }

    QString compareTitleSanitized =
          StrTools::simplifyLetters(
                StrTools::sanitizeName(
                      StrTools::xmlUnescape(compareTitleLCNoArticleNoEdition)));
    QString entryTitleSanitized =
          StrTools::simplifyLetters(
                StrTools::sanitizeName(
                      StrTools::xmlUnescape(entryTitleLCNoArticleNoEdition)));
    int currentDistance = StrTools::distanceBetweenStrings(
          compareTitleSanitized, entryTitleSanitized);
    int currentSize = std::max(compareTitleSanitized.size(),
                               entryTitleSanitized.size());
    if(currentDistance < lowestDistance) {
      lowestDistance = currentDistance;
      stringSize = currentSize;
      mostSimilar = a;
    }

    // If only one title has a subtitle (eg. has ":" or similar in name), remove subtitle from
    // the other if length differs more than 4 in order to have a better chance of a match.
    // Even if a match is realized, penalize it (+1) to allow other entries to be compared.
    if(!config.keepSubtitle) {
      bool entryHasSubtitle, compareHasSubtitle;
      int lengthDiff = abs(compareTitleLCNoArticleNoEdition.length() -
                           entryTitleLCNoArticleNoEdition.length());
      if(lengthDiff > 4) {
        QString compareTitleLCNoArticleNoEditionNoSub =
           NameTools::removeSubtitle(compareTitleLCNoArticleNoEdition, compareHasSubtitle);
        QString entryTitleLCNoArticleNoEditionNoSub =
           NameTools::removeSubtitle(entryTitleLCNoArticleNoEdition, entryHasSubtitle);
        if((entryHasSubtitle && !compareHasSubtitle) || (!entryHasSubtitle && compareHasSubtitle)) {
          QString compare =
                StrTools::simplifyLetters(
                      StrTools::sanitizeName(
                            StrTools::xmlUnescape(compareTitleLCNoArticleNoEditionNoSub)));
          QString entry =
                StrTools::simplifyLetters(
                      StrTools::sanitizeName(
                            StrTools::xmlUnescape(entryTitleLCNoArticleNoEditionNoSub)));
          currentDistance = 1 + StrTools::distanceBetweenStrings(compare, entry);
          currentSize = std::max(compare.size(), entry.size());
        }
      }
    }

    if(currentDistance < lowestDistance) {
      lowestDistance = currentDistance;
      stringSize = currentSize;
      mostSimilar = a;
    }
  }
  game = potentials.at(mostSimilar);
  return game;
}

GameEntry ScraperWorker::getEntryFromUser(const QList<GameEntry> &gameEntries,
                                          const GameEntry &suggestedGame,
                                          const QString &compareTitle,
                                          int &lowestDistance)
{
  GameEntry game;

  std::string entryStr = "";
  printf("Potential entries for '\033[1;32m%s\033[0m':\n", compareTitle.toStdString().c_str());
  bool suggestedShown = false;
  for(int a = 1; a <= gameEntries.length(); ++a) {
    QString suggested = "";
    if(gameEntries.at(a - 1).title == suggestedGame.title && !suggestedShown) {
      suggested = " <-- Skyscraper's choice";
      suggestedShown = true;
    }
    printf("\033[1;32m%d%s\033[0m: Title:    '\033[1;32m%s\033[0m'%s\n    platform: '\033[1;33m%s\033[0m'\n", a, QString((a <= 9?" ":"")).toStdString().c_str(), gameEntries.at(a - 1).title.toStdString().c_str(), suggested.toStdString().c_str(), gameEntries.at(a - 1).platform.toStdString().c_str());
  }
  printf("\033[1;32m-1\033[0m: \033[1;33mNONE OF THE ABOVE!\033[0m\n");
  printf("\033[1;34mPlease choose the preferred entry\033[0m (Or press enter to let Skyscraper choose):\033[0m "); fflush(stdout);
  getline(std::cin, entryStr);
  printf("\n");
  // Becomes 0 if input is not a number
  int chosenEntry = QString(entryStr.c_str()).toInt();

  if(entryStr != "") {
    if(chosenEntry == -1) {
      game.title = "Entries discarded by user";
      game.found = false;
      return game;
    } else if(chosenEntry != 0) {
      lowestDistance = 0;
      game = gameEntries.at(chosenEntry - 1);
      return game;
    }
  }

  return suggestedGame;
}

// --- Console colors ---
// Black        0;30     Dark Gray     1;30
// Red          0;31     Light Red     1;31
// Green        0;32     Light Green   1;32
// Brown/Orange 0;33     Yellow        1;33
// Blue         0;34     Light Blue    1;34
// Purple       0;35     Light Purple  1;35
// Cyan         0;36     Light Cyan    1;36
// Light Gray   0;37     White         1;37
