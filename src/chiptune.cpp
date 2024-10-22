/***************************************************************************
 *            chiptune.cpp
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

#include <QSql>
#include <QString>
#include <QSqlError>
#include <QSqlQuery>
#include <QListIterator>
#include <QXmlStreamReader>

#include "chiptune.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"
#include "skyscraper.h"


Chiptune::Chiptune(Settings *config, QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  bool naviOk = true;

  // Extract table media_file from database selecting by artist=config->platform
  // and dump fields album (normalized), album_id and path: soundtrackList [album]{album_id, path}
  QSqlDatabase navidrome = QSqlDatabase::addDatabase("QSQLITE");
  navidrome.setDatabaseName(config->navidromeDb);
  navidrome.setConnectOptions("QSQLITE_OPEN_READONLY");
  printf("INFO: Reading the videogame music database... ");
  if (!navidrome.open()) {
    printf("ERROR: Connection to Navidrome database has failed.\n");
    qDebug() << navidrome.lastError();
    naviOk = false;
  }
  QString queryString = "select path, album, album_id from media_file where artist='" + config->platform + "'";
  QSqlQuery naviQuery(queryString, navidrome);
  while (naviOk && naviQuery.next()) {
    // printf("A: %s\n", naviQuery.value(1).toString().toStdString().c_str());
    QString sanitizedAlbum = naviQuery.value(1).toString().replace("&", " and ").remove("'");
    sanitizedAlbum = NameTools::convertToIntegerNumeral(sanitizedAlbum);
    QString sanitizedAlbumMain = sanitizedAlbum;
    sanitizedAlbum = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedAlbum), -1, " ");
    // TODO: Danger false positives: sanitizedAlbumMain = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedAlbumMain), -1, " ", true);
    sanitizedAlbum = StrTools::sanitizeName(sanitizedAlbum, true);
    sanitizedAlbumMain = StrTools::sanitizeName(sanitizedAlbumMain, true);
    soundtrackList[sanitizedAlbum] = qMakePair(naviQuery.value(2).toString(),
                                      qMakePair(naviQuery.value(0).toString(), naviQuery.value(1).toString()));
    if (sanitizedAlbum != sanitizedAlbumMain) {
      soundtrackList[sanitizedAlbumMain] = qMakePair(naviQuery.value(2).toString(),
                                            qMakePair(naviQuery.value(0).toString(), naviQuery.value(1).toString()));
    }
    // printf("B: %s\n", sanitizedAlbum.toStdString().c_str());
  }
  naviQuery.finish();
  navidrome.close();
  if (soundtrackList.isEmpty()) {
    printf("WARNING: The Navidrome database contains no records for this platform.\n");
    naviOk = false;
  }
  else {
    printf("OK.\n");
  }

  fetchOrder.append(CHIPTUNE);
  
  if (naviOk){
    printf("INFO: Navidrome database has been parsed, %d games have been loaded into memory.\n\n",
           soundtrackList.count());
  }
  else {
    Skyscraper::removeLockAndExit(1);
  }
}

Chiptune::~Chiptune()
{
}

QList<QString> Chiptune::getSearchNames(const QFileInfo &info)
{
  QString name = info.completeBaseName();
  QString original = name;
  if (Platform::get().getFamily(config->platform) == "arcade") {
    // Convert filename from short mame-style to regular one
    name = name.toLower();
    if (config->mameMap.contains(name)) {
      name = config->mameMap.value(name);
      printf("INFO: 1 Short Mame-style name '%s' converted to '%s'.\n",
             original.toStdString().c_str(),
             name.toStdString().c_str());
    }
  }

  QString sanitizedName = name.replace("&", " and ").remove("'");
  sanitizedName = NameTools::convertToIntegerNumeral(sanitizedName);
  original = sanitizedName;
  sanitizedName = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedName), -1, " ");
  sanitizedName = StrTools::sanitizeName(sanitizedName, true);

  QList<QString> searchNames = { sanitizedName };

  // Searching for subtitles generates many false positives. Deprecated.
  /* if(original.contains(":") || original.contains(" - ")) {
    QString noSubtitle = original.left(original.indexOf(":")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf(" - ")).simplified();
    // Only add if longer than 3. We don't want to search for "the" for instance
    if(noSubtitle.length() > 3) {
      noSubtitle = NameTools::getUrlQueryName(NameTools::removeArticle(noSubtitle), -1, " ");
      noSubtitle = StrTools::sanitizeName(noSubtitle, true);
      searchNames.append(noSubtitle);
    }
  } */
  // printf("D: %s\n", sanitizedName.toStdString().c_str());
  return searchNames;
}

void Chiptune::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString)
{
  GameEntry game;

  QPair<QString, QPair<QString, QString>> record = soundtrackList.value(searchName);
  // printf("F: %s\n", searchName.toStdString().c_str());
  if(!record.first.isEmpty()) {
    game.title = record.second.second;
    game.chiptuneId = record.first;
    game.chiptunePath = record.second.first;
    // printf("G: %s\n", game.chiptuneId.toStdString().c_str());
    // printf("H: %s\n", game.chiptunePath.toStdString().c_str());
    game.platform = Platform::get().getAliases(config->platform).at(1);
    gameEntries.append(game);
  } else {
    if(config->fuzzySearch && searchName.size() >= 6) {
      int maxDistance = config->fuzzySearch;
      if((searchName.size() <= 10) || (config->fuzzySearch < 0)) {
        maxDistance = 1;
      }
      QListIterator<QString> keysIterator(soundtrackList.keys());
      while (keysIterator.hasNext()) {
        QString name = keysIterator.next();
        if(StrTools::onlyNumbers(name) == StrTools::onlyNumbers(searchName)) {
          int distance = StrTools::distanceBetweenStrings(searchName, name);
          if(distance <= maxDistance) {
            printf("FuzzySearch: Found %s = %s (distance %d)!\n",
                   searchName.toStdString().c_str(),
                   name.toStdString().c_str(), distance);
            record = soundtrackList.value(name);
            if(!record.first.isEmpty()) {
              game.title = record.second.second;
              game.chiptuneId = record.first;
              game.chiptunePath = record.second.first;
              // printf("G: %s\n", game.chiptuneId.toStdString().c_str());
              // printf("H: %s\n", game.chiptunePath.toStdString().c_str());
              game.platform = Platform::get().getAliases(config->platform).at(1);
              gameEntries.append(game);
            }
          }
        }
      }
    }
  }
}

void Chiptune::getGameData(GameEntry &game, QStringList &, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case CHIPTUNE:
      if(config->chiptunes) {
        getChiptune(game);
      }
      break;
    default:
      ;
    }
  }
}

void Chiptune::getChiptune(GameEntry &)
{
}
