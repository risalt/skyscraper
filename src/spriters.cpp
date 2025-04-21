/***************************************************************************
 *            spriters.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
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

#include <QDir>
#include <QFile>
#include <QString>
#include <QProcess>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "spriters.h"
#include "skyscraper.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

Spriters::Spriters(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId,
                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  loadConfig("spriters.json", "code", "id");

  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by Spriters-Resource (see file spriters.json)...\033[0m\n");
    return;
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(10000);
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://www.spriters-resource.com";

  // Extract mapping Game->(Platform+Link) from the csv file:
  printf("INFO: Reading the videogame sprites index file... "); fflush(stdout);
  QString indexFileName = config->dbPath + "/index.json";
  QFile indexFile(indexFileName);
  if(!indexFile.open(QFile::ReadOnly | QFile::Text)) {
    printf("\nERROR: Cannot open file '%s'.\n", indexFileName.toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  QByteArray index = indexFile.readAll();
  QJsonDocument jsonDoc(QJsonDocument::fromJson(index));
  if(jsonDoc.isNull() || jsonDoc.isEmpty()) {
    printf("\nERROR: No entries found in '%s'.\n", indexFileName.toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  int entries = 0;
  QJsonArray mappings = jsonDoc.array();
  for(const auto &mapping: std::as_const(mappings)) {
    QString link = mapping.toObject()["link"].toString();
    QString title = mapping.toObject()["title"].toString();
    if(!link.isEmpty() && !title.isEmpty()) {
      QString platform = link.mid(1, link.indexOf("/", 1) - 1);
      if(!platform.isEmpty()) {
        entries++;
        const auto mapTitleUrl = qMakePair(title, link);
        if(platform == platformId) {
          QStringList safeVariations, unsafeVariations;
          NameTools::generateSearchNames(title, safeVariations, unsafeVariations, true);
          for(const auto &name: std::as_const(safeVariations)) {
            if(!nameToGameSame.contains(name, mapTitleUrl)) {
              if(config->verbosity > 2) {
                printf("Adding full variation: %s -> %s\n",
                       title.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToGameSame.insert(name, mapTitleUrl);
            }
          }
          for(const auto &name: std::as_const(unsafeVariations)) {
            if(!nameToGameTitleSame.contains(name, mapTitleUrl)) {
              if(config->verbosity > 2) {
                printf("Adding title variation: %s -> %s\n",
                       title.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToGameTitleSame.insert(name, mapTitleUrl);
            }
          }
        } else {
          QStringList safeVariations, unsafeVariations;
          NameTools::generateSearchNames(title, safeVariations, unsafeVariations, true);
          for(const auto &name: std::as_const(safeVariations)) {
            if(!nameToGameOther.contains(name, mapTitleUrl)) {
              if(config->verbosity > 2) {
                printf("Adding full variation: %s -> %s\n",
                       title.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToGameOther.insert(name, mapTitleUrl);
            }
          }
          /*for(const auto &name: std::as_const(unsafeVariations)) {
            if(!nameToGameTitleOther.contains(name, mapTitleUrl)) {
              if(config->verbosity > 2) {
                printf("Adding title variation: %s -> %s\n",
                       title.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToGameTitleOther.insert(name, mapTitleUrl);
            }
          }*/
        }
      }
    }
  }
  printf("DONE.\nINFO: Read %d game entries from Spriters-Resource local database.\n", entries);

  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(SPRITES);
}

void Spriters::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<QString, QString>> gameUrls;

  if(getSearchResultsOffline(gameUrls, searchName, nameToGameSame, nameToGameTitleSame)) {
    GameEntry game;
    game.platform = Platform::get().getAliases(config->platform).at(1);
    QStringList urlList;
    for(const auto& record: std::as_const(gameUrls)) {
      game.title = record.first;
      game.url = record.second;
    }
    gameEntries.append(game);
  }
  if(gameEntries.isEmpty()) {
    if(getSearchResultsOffline(gameUrls, searchName, nameToGameOther, nameToGameTitleOther)) {
      GameEntry game;
      game.platform = Platform::get().getAliases(config->platform).at(1);
      QStringList urlList;
      for(const auto& record: std::as_const(gameUrls)) {
        game.title = record.first;
        game.url = record.second;
      }
      gameEntries.append(game);
    }
  }
}

void Spriters::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache)
{
  fetchGameResources(game, sharedBlobs, cache);
}

void Spriters::getSprites(GameEntry &game)
{
  QDir spriteDir(config->dbPath + game.url);
  if(!spriteDir.exists()) {
    limiter.exec();
    QProcess::execute("python3",
                      { QDir::currentPath() +
                        "/spriters-downloader/spriters-resource-downloader.py",
                        "-v", baseUrl + game.url });
  }
  if(spriteDir.exists()) {
    game.sprites = config->dbPath + game.url;
  }
}
