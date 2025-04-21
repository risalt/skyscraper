/***************************************************************************
 *            chiptune.cpp
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


Chiptune::Chiptune(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId,
                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  bool naviOk = true;

  offlineScraper = true;
  baseUrl = config->dbPath + "/";

  // Extract table media_file from database selecting by artist=config->platform
  // and dump fields album (normalized), album_id and path: soundtrackList [album]{album_id, path}
  QSqlDatabase navidrome = QSqlDatabase::addDatabase("QSQLITE", "navidrome" + threadId);
  navidrome.setDatabaseName(config->dbServer);
  navidrome.setConnectOptions("QSQLITE_OPEN_READONLY");
  printf("INFO: Reading the videogame music database... "); fflush(stdout);
  if(!navidrome.open()) {
    printf("ERROR: Connection to Navidrome database has failed.\n");
    qDebug() << navidrome.lastError();
    naviOk = false;
  }
  QString queryString = "select path, album, album_id from media_file where artist='" +
                        config->platform + "'";
  QSqlQuery naviQuery(queryString, navidrome);
  while(naviOk && naviQuery.next()) {
    QStringList safeVariations, unsafeVariations;
    NameTools::generateSearchNames(naviQuery.value(1).toString(), safeVariations, unsafeVariations, true);
    for(const auto &name: std::as_const(safeVariations)) {
      if(!soundtrackList.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding full variation: %s -> %s\n",
                 naviQuery.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        soundtrackList.insert(name, qMakePair(naviQuery.value(2).toString(),
                                              qMakePair(naviQuery.value(0).toString(),
                                                        naviQuery.value(1).toString())));
      }
    }
    for(const auto &name: std::as_const(unsafeVariations)) {
      if(!soundtrackListTitle.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding title variation: %s -> %s\n",
                 naviQuery.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        soundtrackListTitle.insert(name, qMakePair(naviQuery.value(2).toString(),
                                                   qMakePair(naviQuery.value(0).toString(),
                                                             naviQuery.value(1).toString())));
      }
    }
  }
  naviQuery.finish();
  navidrome.close();
  if(soundtrackList.isEmpty()) {
    printf("WARNING: The Navidrome database contains no records for this platform.\n");
    naviOk = false;
  }
  else {
    printf("DONE\n");
  }

  if(naviOk){
    printf("INFO: Navidrome database has been parsed, %d games have been loaded into memory.\n\n",
           soundtrackList.count());
  }
  else {
    reqRemaining = 0;
    return;
  }

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(CHIPTUNE);
}

Chiptune::~Chiptune()
{
  QSqlDatabase::removeDatabase("navidrome" + threadId);
}

void Chiptune::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<QString, QPair<QString, QString>>> gameIds;

  if(getSearchResultsOffline(gameIds, searchName, soundtrackList, soundtrackListTitle)) {
    for(const auto &record: std::as_const(gameIds)) {
      GameEntry game;
      game.title = record.second.second;
      game.chiptuneId = record.first;
      game.id = game.chiptuneId;
      game.chiptunePath = baseUrl + record.second.first;
      game.platform = Platform::get().getAliases(config->platform).at(1);
      gameEntries.append(game);
    }
  }
}

void Chiptune::getGameData(GameEntry &, QStringList &, GameEntry *)
{
}
