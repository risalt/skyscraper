/***************************************************************************
 *            offlinemobygames.cpp
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
#include <QRandomGenerator>

#include "offlinemobygames.h"
#include "skyscraper.h"
#include "nametools.h"
#include "strtools.h"

OfflineMobyGames::OfflineMobyGames(Settings *config,
                                   QSharedPointer<NetManager> manager,
                                   QString threadId,
                                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  QString platformDb;
  offlineScraper = true;

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(10000); // 10 second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  loadConfig("mobygames.json", "platform_name", "platform_id");

  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    printf("\033[0;31mPlatform not supported by MobyGames (see file mobygames.json)...\033[0m\n");
    reqRemaining = 0;
    return;
  }

  QFile dbFile(config->dbPath + "/" + config->platform + ".json");
  if(!dbFile.open(QIODevice::ReadOnly)){
    dbFile.close();
    printf("\nERROR: Database file %s cannot be accessed. ", dbFile.fileName().toStdString().c_str());
    printf("Please regenerate it using the program 'Moby Games Platform Scraper'.\n");
    reqRemaining = 0;
    return;
  }
  printf("INFO: Reading MobyGames header game database..."); fflush(stdout);
  fflush(stdout);
  platformDb = QString::fromUtf8(dbFile.readAll());
  dbFile.close();

  jsonDoc = QJsonDocument::fromJson(platformDb.toUtf8());
  if(jsonDoc.isNull()) {
    printf("\nERROR: The MobyGames database file for the platform is missing, empty or corrupted. "
           "Please regenerate it using the program 'Moby Games Platform Scraper'.\n");
    reqRemaining = 0;
    return;
  } else {
    QJsonArray jsonGames = jsonDoc.array();
    // Load offline mobygames database
    for(const auto &jsonGameRef: std::as_const(jsonGames)) {
      int gameId = 0;
      QJsonObject jsonGame;
      if(jsonGameRef.isObject()) {
        jsonGame = jsonGameRef.toObject();
      } else {
        continue;
      }
      if(jsonGame["game_id"].isDouble()) {
        gameId = jsonGame["game_id"].toInt();
      }
      QStringList gameAliases = { jsonGame["title"].toString() };
      QJsonArray gameAlternates = jsonGame["alternate_titles"].toArray();
      for(const auto &alternate: std::as_const(gameAlternates)) {
        gameAliases << alternate.toObject()["title"].toString();
      }
      gameAliases.removeDuplicates();
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
    printf(" DONE.\nINFO: Read %d game header entries from MobyGames local database.\n", idToGame.size());
  }

  baseUrl = "https://api.mobygames.com";
  searchUrlPre = "https://api.mobygames.com/v1/games";

  // Can be offline:
  fetchOrder.append(ID);
  // Can be offline:
  fetchOrder.append(TITLE);
  // Can be offline:
  fetchOrder.append(PLATFORM);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  // Can be offline:
  fetchOrder.append(RELEASEDATE);
  // Can be offline:
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  // Can be offline:
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(AGES);
  // Can be offline:
  fetchOrder.append(RATING);
  // Sometimes can be offline:
  fetchOrder.append(COVER);
  // Sometimes can be offline:
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MANUAL);
}

void OfflineMobyGames::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<int, QString>> matches = {};

  if(getSearchResultsOffline(matches, searchName, nameToId, nameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      if(idToGame.contains(databaseId.first)) {
        GameEntry game;
        QJsonObject jsonGame = idToGame.value(databaseId.first);
        game.id = QString::number(databaseId.first);
        game.title = databaseId.second;
        game.miscData = QJsonDocument(jsonGame).toJson(QJsonDocument::Compact);

        QJsonArray jsonPlatforms = jsonGame["platforms"].toArray();
        while(!jsonPlatforms.isEmpty()) {
          QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
          QString gamePlatformId = QString::number(jsonPlatform["platform_id"].toInt());
          if(platformId.split(",").contains(gamePlatformId)) {
            game.url = searchUrlPre + "/" + game.id + "/platforms/" +
                       gamePlatformId + "?api_key=" + config->userCreds;
            game.platform = jsonPlatform["platform_name"].toString();
            gameEntries.append(game);
          }
          jsonPlatforms.removeFirst();
        }
      } else {
        printf("ERROR: Internal error: Database Id '%d' not found in the in-memory database.\n",
               databaseId.first);
      }
    }
  }
}

void OfflineMobyGames::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if(sharedBlobs.contains("offlineonly") || config->userCreds.isEmpty()) {
    printf("Offline mode activated: reduced set of data will be retrieved "
           "from disk database.\n");
    offlineOnly = true;
  } else {
    printf("Waiting to get game data...\n");
    offlineOnly = false;
    limiter.exec();
    netComm->request(game.url);
    q.exec();
    data = netComm->getData();
    printf("g %s\n", game.url.toStdString().c_str());

    jsonDoc = QJsonDocument::fromJson(data);
    if(jsonDoc.isEmpty()) {
      return;
    }
  }

  jsonObj = QJsonDocument::fromJson(game.miscData).object();

  if(offlineOnly) {
    jsonDoc = QJsonDocument();
    jsonMedia = QJsonDocument();
    jsonScreens = QJsonDocument();
  } else {
    printf("Waiting to get media data...\n");
    limiter.exec();
    netComm->request(game.url.left(game.url.indexOf("?api_key=")) + "/covers" +
                     "?api_key=" + config->userCreds);
    q.exec();
    data = netComm->getData();
    jsonMedia = QJsonDocument::fromJson(data);

    printf("Waiting to get screenshot data...\n");
    limiter.exec();
    netComm->request(game.url.left(game.url.indexOf("?api_key=")) + "/screenshots" +
                     "?api_key=" + config->userCreds);
    q.exec();
    data = netComm->getData();
    jsonScreens = QJsonDocument::fromJson(data);
  }

  fetchGameResources(game, sharedBlobs, cache);
}

void OfflineMobyGames::getReleaseDate(GameEntry &game)
{
  // game.releaseDate = jsonDoc.object()["first_release_date"].toString();
  QJsonArray jsonPlatforms = jsonObj["platforms"].toArray();
  while(!jsonPlatforms.isEmpty()) {
    QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
    QString gamePlatformId = QString::number(jsonPlatform["platform_id"].toInt());
    if(platformId.split(",").contains(gamePlatformId)) {
      game.releaseDate = StrTools::conformReleaseDate(jsonPlatform["first_release_date"].toString());
      break;
    }
    jsonPlatforms.removeFirst();
  }
}

void OfflineMobyGames::getPlayers(GameEntry &game)
{
  if(!offlineOnly) {
    QJsonArray jsonAttribs = jsonDoc.object()["attributes"].toArray();
    for(int a = 0; a < jsonAttribs.count(); ++a) {
      QString players = jsonAttribs.at(a).toObject()["attribute_category_name"].toString();
      if(players == "Number of Players Supported" || players == "Number of Offline Players") {
        game.players = StrTools::conformPlayers(players);
      }
    }
  }
}

void OfflineMobyGames::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(int a = 0; a < jsonGenres.count(); ++a) {
    game.tags.append(jsonGenres.at(a).toObject()["genre_name"].toString() + ", ");
  }
  game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
}

void OfflineMobyGames::getAges(GameEntry &game)
{
  if(!offlineOnly) {
    QJsonArray jsonAges = jsonDoc.object()["ratings"].toArray();
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "PEGI Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "ELSPA Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "ESRB Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "USK Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "OFLC (Australia) Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "SELL Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "BBFC Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "OFLC (New Zealand) Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    for(int a = 0; a < jsonAges.count(); ++a) {
      if(jsonAges.at(a).toObject()["rating_system_name"].toString() == "VRC Rating") {
        game.ages = jsonAges.at(a).toObject()["rating_name"].toString();
        break;
      }
    }
    game.ages = StrTools::conformAges(game.ages);
  }
}

void OfflineMobyGames::getPublisher(GameEntry &game)
{
  if(!offlineOnly) {
    QJsonArray jsonReleases = jsonDoc.object()["releases"].toArray();
    for(int a = 0; a < jsonReleases.count(); ++a) {
      QJsonArray jsonCompanies = jsonReleases.at(a).toObject()["companies"].toArray();
      for(int b = 0; b < jsonCompanies.count(); ++b) {
        if(jsonCompanies.at(b).toObject()["role"].toString() == "Published by") {
          game.publisher = jsonCompanies.at(b).toObject()["company_name"].toString();
          return;
        }
      }
    }
  }
}

void OfflineMobyGames::getDeveloper(GameEntry &game)
{
  if(!offlineOnly) {
    QJsonArray jsonReleases = jsonDoc.object()["releases"].toArray();
    for(int a = 0; a < jsonReleases.count(); ++a) {
      QJsonArray jsonCompanies = jsonReleases.at(a).toObject()["companies"].toArray();
      for(int b = 0; b < jsonCompanies.count(); ++b) {
        if(jsonCompanies.at(b).toObject()["role"].toString() == "Developed by") {
          game.developer = jsonCompanies.at(b).toObject()["company_name"].toString();
          return;
        }
      }
    }
  }
}

void OfflineMobyGames::getDescription(GameEntry &game)
{
  game.description = jsonObj["description"].toString();

  // Remove all html tags within description
  game.description = StrTools::stripHtmlTags(game.description);
}

void OfflineMobyGames::getRating(GameEntry &game)
{
  QJsonValue jsonValue = jsonObj["moby_score"];
  if(jsonValue != QJsonValue::Undefined) {
    double rating = jsonValue.toDouble();
    if(rating != 0.0) {
      game.rating = QString::number(rating / 10.0);
    }
  }
}

void OfflineMobyGames::getCover(GameEntry &game)
{
  QString coverUrl = "";

  if(offlineOnly || jsonMedia.isEmpty()) {
    QJsonObject cover = jsonObj["sample_cover"].toObject();
    if(!cover.isEmpty()) {
      QJsonArray jsonPlatforms = cover["platforms"].toArray();
      while(!jsonPlatforms.isEmpty()) {
        QString jsonPlatform = jsonPlatforms.first().toString();
        if(platformMatch(jsonPlatform, config->platform)) {
          coverUrl = cover["image"].toString();
          break;
        }
        jsonPlatforms.removeFirst();
      }
    }
  } else {
    bool foundFrontCover= false;
    for(const auto &region: std::as_const(regionPrios)) {
      QJsonArray jsonCoverGroups = jsonMedia.object()["cover_groups"].toArray();
      while(!jsonCoverGroups.isEmpty()) {
        bool foundRegion = false;
        QJsonArray jsonCountries = jsonCoverGroups.first().toObject()["countries"].toArray();
        while(!jsonCountries.isEmpty()) {
          if(getRegionShort(jsonCountries.first().toString().simplified()) == region) {
            foundRegion = true;
            break;
          }
          jsonCountries.removeFirst();
        }
        if(!foundRegion) {
          jsonCoverGroups.removeFirst();
          continue;
        }
        QJsonArray jsonCovers = jsonCoverGroups.first().toObject()["covers"].toArray();
        while(!jsonCovers.isEmpty()) {
          QJsonObject jsonCover = jsonCovers.first().toObject();
          if(jsonCover["scan_of"].toString().toLower().simplified().contains("front cover")) {
            coverUrl = jsonCover["image"].toString();
            foundFrontCover= true;
            break;
          }
          jsonCovers.removeFirst();
        }
        if(foundFrontCover) {
          break;
        }
        jsonCoverGroups.removeFirst();
      }
      if(foundFrontCover) {
        break;
      }
    }
  }

  // For some reason the links are http but they are always redirected to https
  coverUrl.replace("http://", "https://");

  if(!coverUrl.isEmpty()) {
    printf("Retrieving cover data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
    }
  }
}

void OfflineMobyGames::getManual(GameEntry &game)
{
  if(offlineOnly || jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: std::as_const(regionPrios)) {
    QJsonArray jsonCoverGroups = jsonMedia.object()["cover_groups"].toArray();
    while(!jsonCoverGroups.isEmpty()) {
      bool foundRegion = false;
      QJsonArray jsonCountries = jsonCoverGroups.first().toObject()["countries"].toArray();
      while(!jsonCountries.isEmpty()) {
        if(getRegionShort(jsonCountries.first().toString().simplified()) == region) {
          foundRegion = true;
          break;
        }
        jsonCountries.removeFirst();
      }
      if(!foundRegion) {
        jsonCoverGroups.removeFirst();
        continue;
      }
      QJsonArray jsonCovers = jsonCoverGroups.first().toObject()["covers"].toArray();
      while(!jsonCovers.isEmpty()) {
        QJsonObject jsonCover = jsonCovers.first().toObject();
        if(jsonCover["scan_of"].toString().toLower().simplified().contains("manual")) {
          coverUrl = jsonCover["image"].toString();
          foundFrontCover= true;
          break;
        }
        jsonCovers.removeFirst();
      }
      if(foundFrontCover) {
        break;
      }
      jsonCoverGroups.removeFirst();
    }
    if(foundFrontCover) {
      break;
    }
  }

  // For some reason the links are http but they are always redirected to https
  coverUrl.replace("http://", "https://");

  if(!coverUrl.isEmpty()) {
    printf("Retrieving manual data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.manualData = netComm->getData();
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         !contentType.isEmpty() && game.manualData.size() > 4096) {
        game.manualFormat = contentType.mid(contentType.indexOf("/") + 1,
                                            contentType.length() - contentType.indexOf("/") + 1);
        if(game.manualFormat.length()>4) {
          game.manualFormat = "webp";
        }
      } else {
        game.manualData = "";
      }
    }
  }
}

void OfflineMobyGames::getMarquee(GameEntry &game)
{
  if(offlineOnly || jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: std::as_const(regionPrios)) {
    QJsonArray jsonCoverGroups = jsonMedia.object()["cover_groups"].toArray();
    while(!jsonCoverGroups.isEmpty()) {
      bool foundRegion = false;
      QJsonArray jsonCountries = jsonCoverGroups.first().toObject()["countries"].toArray();
      while(!jsonCountries.isEmpty()) {
        if(getRegionShort(jsonCountries.first().toString().simplified()) == region) {
          foundRegion = true;
          break;
        }
        jsonCountries.removeFirst();
      }
      if(!foundRegion) {
        jsonCoverGroups.removeFirst();
        continue;
      }
      QJsonArray jsonCovers = jsonCoverGroups.first().toObject()["covers"].toArray();
      while(!jsonCovers.isEmpty()) {
        QJsonObject jsonCover = jsonCovers.first().toObject();
        if(jsonCover["scan_of"].toString().toLower().simplified().contains("inside cover")) {
          coverUrl = jsonCover["image"].toString();
          foundFrontCover= true;
          break;
        }
        jsonCovers.removeFirst();
      }
      if(foundFrontCover) {
        break;
      }
      jsonCoverGroups.removeFirst();
    }
    if(foundFrontCover) {
      break;
    }
  }

  // For some reason the links are http but they are always redirected to https
  coverUrl.replace("http://", "https://");

  if(!coverUrl.isEmpty()) {
    printf("Retrieving marquee data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}

void OfflineMobyGames::getTexture(GameEntry &game)
{
  if(offlineOnly || jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: std::as_const(regionPrios)) {
    QJsonArray jsonCoverGroups = jsonMedia.object()["cover_groups"].toArray();
    while(!jsonCoverGroups.isEmpty()) {
      bool foundRegion = false;
      QJsonArray jsonCountries = jsonCoverGroups.first().toObject()["countries"].toArray();
      while(!jsonCountries.isEmpty()) {
        if(getRegionShort(jsonCountries.first().toString().simplified()) == region) {
          foundRegion = true;
          break;
        }
        jsonCountries.removeFirst();
      }
      if(!foundRegion) {
        jsonCoverGroups.removeFirst();
        continue;
      }
      QJsonArray jsonCovers = jsonCoverGroups.first().toObject()["covers"].toArray();
      while(!jsonCovers.isEmpty()) {
        QJsonObject jsonCover = jsonCovers.first().toObject();
        if(jsonCover["scan_of"].toString().toLower().simplified().contains("back cover")) {
          coverUrl = jsonCover["image"].toString();
          foundFrontCover= true;
          break;
        }
        jsonCovers.removeFirst();
      }
      if(foundFrontCover) {
        break;
      }
      jsonCoverGroups.removeFirst();
    }
    if(foundFrontCover) {
      break;
    }
  }

  if(coverUrl.isEmpty()) {
    for(const auto &region: std::as_const(regionPrios)) {
      QJsonArray jsonCoverGroups = jsonMedia.object()["cover_groups"].toArray();
      while(!jsonCoverGroups.isEmpty()) {
        bool foundRegion = false;
        QJsonArray jsonCountries = jsonCoverGroups.first().toObject()["countries"].toArray();
        while(!jsonCountries.isEmpty()) {
          if(getRegionShort(jsonCountries.first().toString().simplified()) == region) {
            foundRegion = true;
            break;
          }
          jsonCountries.removeFirst();
        }
        if(!foundRegion) {
          jsonCoverGroups.removeFirst();
          continue;
        }
        QJsonArray jsonCovers = jsonCoverGroups.first().toObject()["covers"].toArray();
        while(!jsonCovers.isEmpty()) {
          QJsonObject jsonCover = jsonCovers.first().toObject();
          if(jsonCover["scan_of"].toString().toLower().simplified().contains("media")) {
            coverUrl = jsonCover["image"].toString();
            foundFrontCover= true;
            break;
          }
          jsonCovers.removeFirst();
        }
        if(foundFrontCover) {
          break;
        }
        jsonCoverGroups.removeFirst();
      }
      if(foundFrontCover) {
        break;
      }
    }
  }

  // For some reason the links are http but they are always redirected to https
  coverUrl.replace("http://", "https://");

  if(!coverUrl.isEmpty()) {
    printf("Retrieving back texture data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
    }
  }
}

void OfflineMobyGames::getWheel(GameEntry &game)
{
  if(offlineOnly || jsonScreens.isEmpty()) {
    return;
  }

  QJsonArray jsonScreenshots = jsonScreens.object()["screenshots"].toArray();

  if(jsonScreenshots.isEmpty()) {
    return;
  }
  int chosen = 0;
  printf("Retrieving wheel data...\n");
  netComm->request(jsonScreenshots.at(chosen).toObject()["image"].toString().replace("http://", "https://"));
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void OfflineMobyGames::getScreenshot(GameEntry &game)
{
  QJsonArray jsonScreenshots;

  if(offlineOnly || jsonScreens.isEmpty()) {
    jsonScreenshots = jsonObj["sample_screenshots"].toArray();
  } else {
    jsonScreenshots = jsonScreens.object()["screenshots"].toArray();
  }

  int chosen = 0;
  if(jsonScreenshots.isEmpty()) {
    return;
  } else if(jsonScreenshots.count() > 2) {
    // First 2 are almost always not ingame, so skip those if we have 3 or more
    chosen = (QRandomGenerator::global()->generate() % (jsonScreenshots.count() - 2)) + 2;
  }
  printf("Retrieving screenshot data...\n");
  netComm->request(jsonScreenshots.at(chosen).toObject()["image"].toString().replace("http://", "https://"));
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

QString OfflineMobyGames::getRegionShort(const QString &region)
{
  if(region == "Germany") {
    return "de";
  } else if(region == "Australia") {
    return "au";
  } else if(region == "Brazil") {
    return "br";
  } else if(region == "Bulgaria") {
    return "bg";
  } else if(region == "Canada") {
    return "ca";
  } else if(region == "Chile") {
    return "cl";
  } else if(region == "China") {
    return "cn";
  } else if(region == "South Korea") {
    return "kr";
  } else if(region == "Denmark") {
    return "dk";
  } else if(region == "Spain") {
    return "sp";
  } else if(region == "Finland") {
    return "fi";
  } else if(region == "France") {
    return "fr";
  } else if(region == "Greece") {
    return "gr";
  } else if(region == "Hungary") {
    return "hu";
  } else if(region == "Israel") {
    return "il";
  } else if(region == "Italy") {
    return "it";
  } else if(region == "Japan") {
    return "jp";
  } else if(region == "Worldwide") {
    return "wor";
  } else if(region == "Norway") {
    return "no";
  } else if(region == "New Zealand") {
    return "nz";
  } else if(region == "Netherlands") {
    return "nl";
  } else if(region == "Poland") {
    return "pl";
  } else if(region == "Portugal") {
    return "pt";
  } else if(region == "Czech Republic") {
    return "cz";
  } else if(region == "United Kingdom") {
    return "uk";
  } else if(region == "Russia") {
    return "ru";
  } else if(region == "Slovakia") {
    return "sk";
  } else if(region == "Sweden") {
    return "se";
  } else if(region == "Taiwan") {
    return "tw";
  } else if(region == "Turkey") {
    return "tr";
  } else if(region == "United States") {
    return "us";
  }
  return "na";
}
