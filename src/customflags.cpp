/***************************************************************************
 *            customflags.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
 *  Copyright 2017 Lars Muldjord
 *  muldjordlars@gmail.com
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


CustomFlags::CustomFlags(Settings *config, QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  fetchOrder.append(CUSTOMFLAGS);
}

CustomFlags::~CustomFlags()
{
}

QList<QString> CustomFlags::getSearchNames(const QFileInfo &info)
{
  QList<QString> searchNames = { info.filePath() };
  return searchNames;
}

void CustomFlags::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString)
{
  GameEntry game;
  game.path = searchName;
  game.completed = QFileInfo::exists(game.path + ".completed") ? true : false;
  game.favourite = QFileInfo::exists(game.path + ".favourite") ? true : false;
  QFile inputFile(game.path + ".played");
  game.played = inputFile.exists() ? true : false;
  if (game.played) {
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&inputFile);
      while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList startEndPlayTime = line.split(";", Qt::SkipEmptyParts);
        if (startEndPlayTime.size() > 0) {
          unsigned int firstEpoch = startEndPlayTime.at(0).toUInt();
          if (firstEpoch) {
            if (!game.firstPlayed) {
              game.firstPlayed = firstEpoch;
            }
            game.timesPlayed++;
            game.lastPlayed = firstEpoch;
            if (startEndPlayTime.size() == 2 && startEndPlayTime.at(1).toUInt()) {
              unsigned int timePlayed = startEndPlayTime.at(1).toUInt() - firstEpoch;
              if (timePlayed>0) {
                game.timePlayed += timePlayed;
              }
            }
          }
        }
      }
      inputFile.close();
    }
  }
  gameEntries.append(game);
}

void CustomFlags::getGameData(GameEntry & game, QStringList &, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case CUSTOMFLAGS:
      getCustomFlags(game);
      break;
    default:
      ;
    }
  }
}

void CustomFlags::getCustomFlags(GameEntry & game)
{
  game.completed = QFileInfo::exists(game.path + ".completed") ? true : false;
  game.favourite = QFileInfo::exists(game.path + ".favourite") ? true : false;
  QFile inputFile(game.path + ".played");
  game.played = inputFile.exists() ? true : false;
  if (game.played) {
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream in(&inputFile);
      while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList startEndPlayTime = line.split(";", Qt::SkipEmptyParts);
        if (startEndPlayTime.size() > 0) {
          unsigned int firstEpoch = startEndPlayTime.at(0).toUInt();
          if (firstEpoch) {
            if (!game.firstPlayed) {
              game.firstPlayed = firstEpoch;
            }
            game.timesPlayed++;
            game.lastPlayed = firstEpoch;
            if (startEndPlayTime.size() == 2 && startEndPlayTime.at(1).toUInt()) {
              unsigned int timePlayed = startEndPlayTime.at(1).toUInt() - firstEpoch;
              if (timePlayed>0) {
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

