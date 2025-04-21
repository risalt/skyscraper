/***************************************************************************
 *            docsdb.cpp
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

#include "docsdb.h"
#include "skyscraper.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

DocsDB::DocsDB(Settings *config,
               QSharedPointer<NetManager> manager,
               QString threadId,
               NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  // Extract mapping Game->(Platform+Link) from the csv file:
  printf("INFO: Reading the documentation db files... "); fflush(stdout);
  QString fileName;
  bool platformAgnostic = false;
  if(Platform::get().getFamily(config->platform) == "arcade") {
    fileName = config->dbPath + "/" + Skyscraper::docType + "db/arcade.csv";
  }
  else {
    fileName = config->dbPath + "/" + Skyscraper::docType + "db/" +
                       config->platform + ".csv";
    if(!QFile::exists(fileName)) {
      platformAgnostic = true;
      fileName = config->dbPath + "/" + Skyscraper::docType + "db/all.csv";
    }
  }
  QFile file(fileName);
  if(!file.open(QFile::ReadOnly | QFile::Text)) {
    printf("\nERROR: Cannot open the db file for the platform in '%s/%sdb/'.\n",
           config->dbPath.toStdString().c_str(),
           Skyscraper::docType.toStdString().c_str());
    reqRemaining = 0;
    return;
  }

  int numDocs = addToMaps(&file, nameToGame, nameToGameTitle);
  // This db is platform independent, we need to be more careful when matching:
  if(platformAgnostic) {
    nameToGameTitle.clear();
  }

  printf(" DONE.\nINFO: Read %d %s from the local database.\n",
         numDocs, Skyscraper::docType.toStdString().c_str());

  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(GUIDES);
  fetchOrder.append(REVIEWS);
  fetchOrder.append(ARTBOOKS);
  fetchOrder.append(CHEATS);
}

int DocsDB::addToMaps(QFile *file,
                      QMultiMap<QString, QPair<QString, QString>> &nameToGame,
                      QMultiMap<QString, QPair<QString, QString>> &nameToGameTitle)
{
  QTextStream textFile(file);
  QString row;
  QStringList rowFields;
  int numEntries = 0;
  while(textFile.readLineInto(&row)) {
    rowFields = row.split('"');
    if(rowFields.size() == 2) {
      numEntries++;
      QString url = config->dbPath + "/" + rowFields.at(0);
      const auto mapTitleUrl = qMakePair(rowFields.at(1), url);
      QStringList safeVariations, unsafeVariations;
      NameTools::generateSearchNames(rowFields.at(1), safeVariations, unsafeVariations, true);
      for(const auto &name: std::as_const(safeVariations)) {
        if(!nameToGame.contains(name, mapTitleUrl)) {
          if(config->verbosity > 2) {
            printf("Adding full variation: %s -> %s\n",
                   rowFields.at(1).toStdString().c_str(), name.toStdString().c_str());
          }
          nameToGame.insert(name, mapTitleUrl);
        }
      }
      for(const auto &name: std::as_const(unsafeVariations)) {
        if(!nameToGameTitle.contains(name, mapTitleUrl)) {
          if(config->verbosity > 2) {
            printf("Adding title variation: %s -> %s\n",
                   rowFields.at(1).toStdString().c_str(), name.toStdString().c_str());
          }
          nameToGameTitle.insert(name, mapTitleUrl);
        }
      }
    }
  }
  return numEntries;
}

void DocsDB::getSearchResults(QList<GameEntry> &gameEntries, QString searchName, QString)
{
  QList<QPair<QString, QString>> docUrls;

  if(getSearchResultsOffline(docUrls, searchName, nameToGame, nameToGameTitle)) {
    GameEntry game;
    game.platform = Platform::get().getAliases(config->platform).at(1);
    QStringList urlList;
    for(const auto& record: std::as_const(docUrls)) {
      game.title = record.first;
      if(!urlList.contains(record.second)) {
        urlList.append(record.second);
      }
    }
    if(!game.title.isEmpty() && !urlList.isEmpty()) {
      if(Skyscraper::docType.contains("cheats")) {
        game.cheats = urlList.join(";");
      } else if(Skyscraper::docType.contains("reviews")) {
        game.reviews = urlList.join(";");
      } else if(Skyscraper::docType.contains("guides")) {
        game.guides = urlList.join(";");
      } else if(Skyscraper::docType.contains("artbooks")) {
        game.artbooks = urlList.join(";");
      }
      gameEntries.append(game);
    }
  }
}

void DocsDB::getGameData(GameEntry &, QStringList &, GameEntry *)
{
}
