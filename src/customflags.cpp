/***************************************************************************
 *            customflags.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
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

#include "customflags.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"
#include "skyscraper.h"

#include <QDateTime>


CustomFlags::CustomFlags(Settings *config,
                         QSharedPointer<NetManager> manager,
                         QString threadId,
                         NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  fetchOrder.append(CUSTOMFLAGS);
}

QStringList CustomFlags::getSearchNames(const QFileInfo &info)
{
  QStringList searchNames = { info.filePath() };
  return searchNames;
}

void CustomFlags::getSearchResults(QList<GameEntry> &gameEntries,
                                   QString searchName, QString)
{
  GameEntry game;
  game.path = searchName;
  gameEntries.append(game);
}

void CustomFlags::getGameData(GameEntry & game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  fetchGameResources(game, sharedBlobs, cache);
}

void CustomFlags::getCustomFlags(GameEntry & game)
{
  NameTools::cache->fillBlanks(game, "generic");
  // Refresh is not enough, as it will not delete a resource that cannot be scraped anymore:
  NameTools::cache->removeResources(game.cacheId, config->scraper);
  game.canonical = CanonicalData();
  if(!game.diskSize) {
    // Calculating game size
    game.diskSize = NameTool->calculateGameSize(game.absoluteFilePath);
  }
  game.completed = QFileInfo::exists(game.path + ".completed") ? true : false;
  game.favourite = QFileInfo::exists(game.path + ".favourite") ? true : false;
  QFile inputFile(game.path + ".played");
  game.played = inputFile.exists() ? true : false;
  if(game.played) {
    if(inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&inputFile);
      while(!in.atEnd()) {
        QString line = in.readLine();
        QStringList startEndPlayTime = line.split(";", Qt::SkipEmptyParts);
        if(!startEndPlayTime.isEmpty()) {
          qint64 firstEpoch = startEndPlayTime.at(0).toLongLong();
          if(firstEpoch) {
            if(!game.firstPlayed) {
              game.firstPlayed = firstEpoch;
            }
            game.timesPlayed++;
            game.lastPlayed = firstEpoch;
            if(startEndPlayTime.size() == 2 && startEndPlayTime.at(1).toLongLong()) {
              qint64 timePlayed = startEndPlayTime.at(1).toLongLong() - firstEpoch;
              if(timePlayed>0) {
                game.timePlayed += timePlayed;
              }
            }
          }
        }
      }
      inputFile.close();
    }
  }
}

