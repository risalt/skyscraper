/***************************************************************************
 *            gamefaqs.cpp
 *
 *  Fri Mar 30 12:00:00 CEST 2018
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

#include <QFile>
#include <QJsonArray>
#include <QDirIterator>

#include "gamefaqs.h"
#include "skyscraper.h"
#include "nametools.h"
#include "strtools.h"

GameFaqs::GameFaqs(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId,
                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  QString platformDb;
  offlineScraper = true;
  offlineOnly = true;

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(10000); // 10 second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  QFile dbFile(config->dbPath + "/" + config->platform + ".json");
  if(!dbFile.open(QIODevice::ReadOnly)){
    dbFile.close();
    printf("\nERROR: Database file %s cannot be accessed. ", dbFile.fileName().toStdString().c_str());
    printf("Please regenerate it using the program 'GameFaqs Scaper'.\n");
    reqRemaining = 0;
    return;
  }
  printf("INFO: Reading GameFaqs game database..."); fflush(stdout);
  fflush(stdout);
  platformDb = QString::fromUtf8(dbFile.readAll());
  dbFile.close();

  jsonDoc = QJsonDocument::fromJson(platformDb.toUtf8());
  if(jsonDoc.isNull()) {
    printf("\nERROR: The GameFaqs database file for the platform is missing, empty or corrupted. "
           "Please regenerate it using the program 'GameFaqs Scaper'.\n");
    reqRemaining = 0;
    return;
  } else {
    QJsonObject jsonGames = jsonDoc.object();
    // Load offline gamefaqs database
    for(const auto &jsonGameRef: std::as_const(jsonGames)) {
      int gameId = 0;
      QJsonObject jsonGame;
      if(jsonGameRef.isObject()) {
        jsonGame = jsonGameRef.toObject();
      } else {
        continue;
      }
      if(jsonGame["gameFaqsId"].isDouble()) {
        gameId = jsonGame["gameFaqsId"].toInt();
      }
      QStringList gameAliases = { jsonGame["titleDetail"].toObject()["title"].toString() };
      QJsonArray gameAlternates = jsonGame["releaseData"].toArray();
      for(const auto &alternate: std::as_const(gameAlternates)) {
        gameAliases << alternate.toObject()["title"].toString();
      }
      QString fixSplit = jsonGame["titleDetail"].toObject()["alsoKnownAs"].toString();
      if(fixSplit.startsWith(", ")) {
        fixSplit.remove(0, 2);
      }
      if(fixSplit.endsWith(',')) {
        fixSplit.chop(1);
      }
      QStringList gameKnownAs = fixSplit.simplified().split("• ", Qt::SkipEmptyParts);
      for(const auto &alternate: std::as_const(gameKnownAs)) {
        gameAliases << alternate;
      }
      if(gameId) {
        for(const auto &gameAlias: std::as_const(gameAliases)) {
          QStringList safeVariations, unsafeVariations;
          NameTools::generateSearchNames(gameAlias, safeVariations, unsafeVariations, true);
          for(const auto &name: std::as_const(safeVariations)) {
            const auto idTitle = qMakePair(gameId, gameAlias);
            if(!nameToId.contains(name, idTitle)) {
              if(config->verbosity > 2) {
                printf("Adding full variation: %s -> %s\n",
                       gameAlias.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToId.insert(name, idTitle);
            }
          }
          for(const auto &name: std::as_const(unsafeVariations)) {
            const auto idTitle = qMakePair(gameId, gameAlias);
            if(!nameToIdTitle.contains(name, idTitle)) {
              if(config->verbosity > 2) {
                printf("Adding title variation: %s -> %s\n",
                       gameAlias.toStdString().c_str(), name.toStdString().c_str());
              }
              nameToIdTitle.insert(name, idTitle);
            }
          }
        }
        idToGame[gameId] = jsonGame;
      }
    }
    printf(" DONE.\nINFO: Read %d game header entries from GameFaqs local database.\n", idToGame.size());
  }

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(AGES);
  fetchOrder.append(RATING);
  fetchOrder.append(GUIDES);
  fetchOrder.append(TRIVIA);
}

void GameFaqs::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<int, QString>> matches = {};

  if(getSearchResultsOffline(matches, searchName, nameToId, nameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      if(idToGame.contains(databaseId.first)) {
        GameEntry game;
        QJsonObject jsonGame = idToGame.value(databaseId.first);
        game.id = QString::number(databaseId.first);
        game.url = jsonGame["gameUrl"].toString().replace("/data", "/media");
        game.title = databaseId.second;
        game.platform = jsonGame["titleDetail"].toObject()["platform"].toString();
        game.miscData = QJsonDocument(jsonGame).toJson(QJsonDocument::Compact);
	gameEntries.append(game);
      } else {
        printf("ERROR: Internal error: Database Id '%d' not found in the in-memory database.\n",
               databaseId.first);
      }
    }
  }
  // qDebug() << "Detected:" << gameEntries;
}

void GameFaqs::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  // Delete "offlineOnly || " when the media retrieval logic is implemented
  if(offlineOnly || sharedBlobs.contains("offlineonly")) {
    printf("Offline mode activated: no media will be retrieved from online scraper.\n");
    offlineOnly = true;
  } else {
    offlineOnly = false;
    printf("Waiting to get media data...\n");
    limiter.exec();
    netComm->request(game.url);
    q.exec();
    data = netComm->getData();
    printf("%s\n", game.url.toStdString().c_str());
    // TODO: LOW: Pending implementation of media url extraction + getCover + getVideo...
    // Not worth it unless the anti-scraping protections are dropped.
    // If ever implemented, also update GameEntries::getCompleteness and GameFaqs().
    // Also, protect all online getters with "if(!offlineOnly) {...}.
  }

  jsonObj = QJsonDocument::fromJson(game.miscData).object();
  jsonReleases = jsonObj["releaseData"].toArray();
  gfRegions.clear();
  for(const auto &region: std::as_const(regionPrios)) {
    if(region == "eu" || region == "fr" || region == "de" || region == "it" ||
       region == "sp" || region == "se" || region == "nl" || region == "dk") {
      gfRegions.append("EU");
    } else if(region == "us" || region == "ca") {
      gfRegions.append("US");
    } else if(region == "wor") {
      gfRegions.append("WW");
    } else if(region == "jp") {
      gfRegions.append("JP");
    } else if(region == "kr") {
      gfRegions.append("KO");
    } else if(region == "au") {
      gfRegions.append("AU");
    } else if(region == "asi" || region == "tw" || region == "cn") {
      gfRegions.append("AS");
    }
  }
  gfRegions.removeDuplicates();

  fetchGameResources(game, sharedBlobs, cache);
}

void GameFaqs::getReleaseDate(GameEntry &game)
{
  bool regionFound = false;
  for(const auto &region: std::as_const(gfRegions)) {
    for(const auto &release: std::as_const(jsonReleases)) {
      if(region == release.toObject()["region"].toString()) {
        game.releaseDate = StrTools::conformReleaseDate(release.toObject()["releaseDate"].toString());
        if(!game.releaseDate.isEmpty()) {
          regionFound = true;
          break;
        }
      }
      if(regionFound) {
        break;
      }
    }
  }
  if(game.releaseDate.isEmpty()) {
    game.releaseDate = StrTools::conformReleaseDate(jsonObj["titleDetail"].toObject()["release"].toString());
  }
}

void GameFaqs::getPlayers(GameEntry &game)
{
  game.players = StrTools::conformPlayers(
                  jsonObj["titleDetail"].toObject()["localPlayers"].toString());
}

void GameFaqs::getTags(GameEntry &game)
{
  game.tags = jsonObj["titleDetail"].toObject()["genre"].toString().replace(" > ", ", ");
  game.tags = StrTools::conformTags(game.tags);
}

void GameFaqs::getAges(GameEntry &game)
{
  bool regionFound = false;
  for(const auto &region: std::as_const(gfRegions)) {
    for(const auto &release: std::as_const(jsonReleases)) {
      if(region == release.toObject()["region"].toString()) {
        game.ages = StrTools::conformAges(release.toObject()["rating"].toString());
        if(!game.ages.isEmpty()) {
          regionFound = true;
          break;
        }
      }
      if(regionFound) {
        break;
      }
    }
  }
  if(game.ages.isEmpty()) {
    game.ages = jsonObj["titleDetail"].toObject()["esrbDescriptors"].toString();
    if(!game.ages.isEmpty()) {
      game.ages = StrTools::conformAges(game.ages);
    }
  }
}

void GameFaqs::getPublisher(GameEntry &game)
{
  bool regionFound = false;
  for(const auto &region: std::as_const(gfRegions)) {
    for(const auto &release: std::as_const(jsonReleases)) {
      if(region == release.toObject()["region"].toString()) {
        game.publisher = release.toObject()["publisher"].toString();
        if(!game.publisher.isEmpty()) {
          regionFound = true;
          break;
        }
      }
      if(regionFound) {
        break;
      }
    }
  }
  if(game.publisher.isEmpty()) {
    game.publisher = jsonObj["titleDetail"].toObject()["publisher"].toString();
  }
}

void GameFaqs::getDeveloper(GameEntry &game)
{
  game.developer = jsonObj["titleDetail"].toObject()["developer"].toString();
}

void GameFaqs::getDescription(GameEntry &game)
{
  game.description = jsonObj["description"].toString();
}

void GameFaqs::getRating(GameEntry &game)
{
  QJsonValue jsonValue = jsonObj["metacriticScore"];
  if(jsonValue != QJsonValue::Undefined) {
    double rating = jsonValue.toDouble();
    if(rating != 0.0) {
      game.rating = QString::number(rating / 100.0);
    }
  }
}

void GameFaqs::getGuides(GameEntry &game)
{
  QString guidePath = game.url;
  guidePath.replace("https://gamefaqs.gamespot.com", config->guidesPath);
  guidePath.chop(5);
  guidePath.append("faqs/");
  QDirIterator guideFiles(guidePath, {"*.txt.gz"}, QDir::Files);
  while(guideFiles.hasNext()) {
    QString newGuide = guideFiles.next();
    newGuide.chop(3);
    game.guides +=  newGuide + ";";
  }
  if(!game.guides.isEmpty()) {
    game.guides.chop(1);
  }
}

void GameFaqs::getTrivia(GameEntry &game)
{
  game.trivia = jsonObj["trivia"].toString();
}

void GameFaqs::getFranchises(GameEntry &game)
{
  game.franchises = jsonObj["titleDetail"].toObject()["franchises"].toString();
  game.franchises = StrTools::conformTags(game.franchises);
}
