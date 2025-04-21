/***************************************************************************
 *            vgmaps.cpp
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

#include "vgmaps.h"
#include "skyscraper.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

VGMaps::VGMaps(Settings *config,
               QSharedPointer<NetManager> manager,
               QString threadId,
               NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  // Extract mapping Game->(Platform+Link) from the csv file:
  printf("INFO: Reading the videogame maps index file... "); fflush(stdout);
  QString vgmapsFileName = config->dbPath + "/vgmaps.csv";
  QFile vgmapsFile(vgmapsFileName);
  if(!vgmapsFile.open(QFile::ReadOnly | QFile::Text)) {
    printf("\nERROR: Cannot open file '%s'.\n", vgmapsFileName.toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  QTextStream vgmaps(&vgmapsFile);
  QStringList vgmapsRow;
  int entries = 0;
  while(StrTools::readCSVRow(vgmaps, &vgmapsRow)) {
    if(vgmapsRow.size() > 1) {
      QString gameName = StrTools::stripBrackets(vgmapsRow.first());
      if(!gameName.isEmpty()) {
        entries++;
        for(int pos=1; pos < vgmapsRow.size(); pos++) {
          if(vgmapsRow.at(pos).endsWith(".html")) {
            QString url = config->dbPath + "/" + vgmapsRow.at(pos);
            const auto mapTitleUrl = qMakePair(gameName, url);
            QStringList safeVariations, unsafeVariations;
            NameTools::generateSearchNames(gameName, safeVariations, unsafeVariations, true);
            for(const auto &name: std::as_const(safeVariations)) {
              if(!nameToGame.contains(name, mapTitleUrl)) {
                if(config->verbosity > 2) {
                  printf("Adding full variation: %s -> %s\n",
                         gameName.toStdString().c_str(), name.toStdString().c_str());
                }
                nameToGame.insert(name, mapTitleUrl);
              }
            }
            for(const auto &name: std::as_const(unsafeVariations)) {
              if(!nameToGameTitle.contains(name, mapTitleUrl)) {
                if(config->verbosity > 2) {
                  printf("Adding title variation: %s -> %s\n",
                         gameName.toStdString().c_str(), name.toStdString().c_str());
                }
                nameToGameTitle.insert(name, mapTitleUrl);
              }
            }
          }
        }
      }
    }
  }
  printf(" DONE.\nINFO: Read %d game header entries from videogame maps local database.\n", entries);

  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(VGMAPS);
}

void VGMaps::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<QString, QString>> gameUrls;

  if(getSearchResultsOffline(gameUrls, searchName, nameToGame, nameToGameTitle)) {
    GameEntry game;
    game.platform = Platform::get().getAliases(config->platform).at(1);
    QStringList urlList;
    for(const auto& record: std::as_const(gameUrls)) {
      game.title = record.first;
      if(!urlList.contains(record.second)) {
        urlList.append(record.second);
      }
    }
    if(!game.title.isEmpty() && !urlList.isEmpty()) {
      game.vgmaps = urlList.join(";");
      gameEntries.append(game);
    }
  }
}

void VGMaps::getGameData(GameEntry &, QStringList &, GameEntry *)
{
}
