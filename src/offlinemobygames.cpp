/***************************************************************************
 *            offlinemobygames.cpp
 *
 *  Fri Mar 30 12:00:00 CEST 2018
 *  Copyright 2018 Lars Muldjord
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

#include <QFile>
#include <QJsonArray>

#include "offlinemobygames.h"
#include "skyscraper.h"
#include "nametools.h"
#include "strtools.h"

#if QT_VERSION >= 0x050a00
#include <QRandomGenerator>
#endif

OfflineMobyGames::OfflineMobyGames(Settings *config,
                     QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  QString platformDb;
  incrementalScraping = true;
  
  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(10000); // 10 second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  loadConfig("mobygames.json");
  platformId = getPlatformId(config->platform);
  if (platformId == "na") {
    printf("\033[0;31mPlatform not supported by MobyGames (see file mobygames.json)...\033[0m\n");
    Skyscraper::removeLockAndExit(1);
  }

  QFile dbFile(config->mobygamesDb + "/" + config->platform + ".json");
  if(!dbFile.open(QIODevice::ReadOnly)){
    dbFile.close();
    printf("\nERROR: Database file %s cannot be accessed. ", dbFile.fileName().toStdString().c_str());
    printf("Please regenerate it using the program 'Moby Games Platform Scraper'.\n");
    Skyscraper::removeLockAndExit(1);
  }
  printf("INFO: Reading MobyGames header game database...");
  fflush(stdout);
  platformDb = QString::fromUtf8(dbFile.readAll());
  dbFile.close();

  jsonDoc = QJsonDocument::fromJson(platformDb.toUtf8());
  if (jsonDoc.isNull()) {
    printf("\nERROR: The MobyGames database file for the platform is missing, empty or corrupted. "
           "Please regenerate it using the program 'Moby Games Platform Scraper'.\n");
    Skyscraper::removeLockAndExit(1);
  } else {
    QJsonArray jsonGames = jsonDoc.array();
    // Load offline mobygames database
    for(const auto &jsonGameRef: jsonGames) {
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
      for(const auto &alternate: gameAlternates) {
        gameAliases << alternate.toObject()["title"].toString();
      }
      if(gameId) {
        for(auto &gameAlias: gameAliases) {
          QString sanitizedGame = gameAlias.replace("&", " and ").remove("'");
          sanitizedGame = NameTools::convertToIntegerNumeral(sanitizedGame);
          QString sanitizedGameMain = sanitizedGame;
          sanitizedGame = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGame), -1, " ");
          // TODO: Danger false positives: sanitizedGameMain = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGameMain), -1, " ", true);
          sanitizedGame = StrTools::sanitizeName(sanitizedGame, true);
          sanitizedGameMain = StrTools::sanitizeName(sanitizedGameMain, true);
          /*qDebug() << "New name/alias;" << gameId << ";" << gameAlias <<
                   ";" << sanitizedGame << ";" << sanitizedGameMain; */
          if(!nameToId.contains(sanitizedGame, gameId)) {
            nameToId.insert(sanitizedGame, gameId);
          }
          if(!nameToId.contains(sanitizedGameMain, gameId)) {
            nameToId.insert(sanitizedGameMain, gameId);
          }
        }
        idToGame[gameId] = jsonGame;
      }
    }
    printf(" DONE.\nINFO: Read %d game header entries from MobyGames local database.\n", idToGame.size());
  }

  baseUrl = "https://api.mobygames.com";
  searchUrlPre = "https://api.mobygames.com/v1/games";

  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(RELEASEDATE); // could be offline
  fetchOrder.append(TAGS); // could be offline
  fetchOrder.append(PLAYERS);
  fetchOrder.append(DESCRIPTION); // could be offline
  fetchOrder.append(AGES);
  fetchOrder.append(RATING); // could be offline
  fetchOrder.append(COVER); // sometimes could be offline
  fetchOrder.append(SCREENSHOT); // sometimes could be offline
  fetchOrder.append(TEXTURE);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MANUAL);
}

void OfflineMobyGames::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<int> matches = {};
  QList<int> match = {};
  QListIterator<int> matchIterator(matches);
  if (nameToId.contains(searchName)) {
    match = nameToId.values(searchName);
    matchIterator = QListIterator<int> (match);
    while (matchIterator.hasNext()) {
      int databaseId = matchIterator.next();
      if (!matches.contains(databaseId)) {
        matches << databaseId;
      }
    }
  }
  
  // If not matches found, try using a fuzzy matcher based on the Damerau/Levenshtein text distance: 
  if(matches.isEmpty()) {
    if(config->fuzzySearch && searchName.size() >= 6) {
      int maxDistance = config->fuzzySearch;
      if((searchName.size() <= 10) || (config->fuzzySearch < 0)) {
        maxDistance = 1;
      }
      QListIterator<QString> keysIterator(nameToId.keys());
      while (keysIterator.hasNext()) {
        QString name = keysIterator.next();
        if(StrTools::onlyNumbers(name) == StrTools::onlyNumbers(searchName)) {
          int distance = StrTools::distanceBetweenStrings(searchName, name);
          if(distance <= maxDistance) {
            printf("FuzzySearch: Found %s = %s (distance %d)!\n",
                   searchName.toStdString().c_str(),
                   name.toStdString().c_str(), distance);
            match = nameToId.values(name);
            matchIterator = QListIterator<int> (match);
            while (matchIterator.hasNext()) {
              int databaseId = matchIterator.next();
              if (!matches.contains(databaseId)) {
                matches << databaseId;
              }
            }
          }
        }
      }
    }
  }

  // qDebug() << "Matches;" << matches;
  QJsonArray jsonGames;
  matchIterator = QListIterator<int> (matches);
  while (matchIterator.hasNext()) {
    int databaseId = matchIterator.next();
    if (idToGame.contains(databaseId)) {
      jsonGames.append(idToGame[databaseId]);
    }
    else {
      printf("ERROR: Internal error: Database Id '%d' not found in the in-memory database.\n",
             databaseId);
    }
  }

  while(!jsonGames.isEmpty()) {
    GameEntry game;
    QJsonObject jsonGame = jsonGames.first().toObject();
    game.id = QString::number(jsonGame["game_id"].toInt());
    game.title = jsonGame["title"].toString();
    game.miscData = QJsonDocument(jsonGame).toJson(QJsonDocument::Compact);

    QJsonArray jsonPlatforms = jsonGame["platforms"].toArray();
    while(!jsonPlatforms.isEmpty()) {
      QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
      QString gamePlatformId = QString::number(jsonPlatform["platform_id"].toInt());
      if (gamePlatformId == platformId ) {
        game.url = searchUrlPre + "/" + game.id + "/platforms/" +
                   QString::number(jsonPlatform["platform_id"].toInt()) +
                   "?api_key=" + config->userCreds;
        game.platform = jsonPlatform["platform_name"].toString();
        gameEntries.append(game);
      }
      jsonPlatforms.removeFirst();
    }
    jsonGames.removeFirst();
  }
  // qDebug() << "Detected:" << gameEntries;
}

void OfflineMobyGames::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  if(sharedBlobs.contains("offlineonly")) {
    printf("Offline mode activated: reduced set of data will be retrieved from disk database.\n");
  } else {
    printf("Waiting to get game data...\n");
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

  if(sharedBlobs.contains("offlineonly")) {
  } else {
    printf("Waiting to get media data...\n");
    limiter.exec();
    netComm->request(game.url.left(game.url.indexOf("?api_key=")) + "/covers" +
                     game.url.mid(game.url.indexOf("?api_key="), game.url.length() - game.url.indexOf("?api_key=")));
    q.exec();
    data = netComm->getData();
    jsonMedia = QJsonDocument::fromJson(data);

    printf("Waiting to get screenshot data...\n");
    limiter.exec();
    netComm->request(game.url.left(game.url.indexOf("?api_key=")) + "/screenshots" +
                     game.url.mid(game.url.indexOf("?api_key="), game.url.length() - game.url.indexOf("?api_key=")));
    q.exec();
    data = netComm->getData();
    jsonScreens = QJsonDocument::fromJson(data);
  }

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case DESCRIPTION:
      getDescription(game);
      break;
    case DEVELOPER:
      if(!sharedBlobs.contains("offlineonly")) {
        getDeveloper(game);
      }
      break;
    case PUBLISHER:
      if(!sharedBlobs.contains("offlineonly")) {
        getPublisher(game);
      }
      break;
    case PLAYERS:
      if(!sharedBlobs.contains("offlineonly")) {
        getPlayers(game);
      }
      break;
    case AGES:
      if(!sharedBlobs.contains("offlineonly")) {
        getAges(game);
      }
      break;
    case RATING:
      getRating(game);
      break;
    case TAGS:
      getTags(game);
      break;
    case FRANCHISES:
      if(!sharedBlobs.contains("offlineonly")) {
        getFranchises(game);
      }
      break;
    case RELEASEDATE:
      getReleaseDate(game);
      break;
    case COVER:
      if(config->cacheCovers && !sharedBlobs.contains("offlineonly")) {
        if ((!cache) || (cache && cache->coverData.isNull())) {
          if(!sharedBlobs.contains("offlineonly")) {
            getCover(game);
          }
        }
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots && !sharedBlobs.contains("offlineonly")) {
        if ((!cache) || (cache && cache->screenshotData.isNull())) {
          if(!sharedBlobs.contains("offlineonly")) {
            getScreenshot(game);
          }
        }
      }
      break;
    case WHEEL:
      if(config->cacheWheels && !sharedBlobs.contains("offlineonly")) {
        if ((!cache) || (cache && cache->wheelData.isNull())) {
          getWheel(game);
        }
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees && !sharedBlobs.contains("offlineonly")) {
        if ((!cache) || (cache && cache->marqueeData.isNull())) {
          getMarquee(game);
        }
      }
      break;
    case TEXTURE:
      if (config->cacheTextures && !sharedBlobs.contains("offlineonly")) {
        if ((!cache) || (cache && cache->textureData.isNull())) {
          getTexture(game);
        }
      }
      break;
    case VIDEO:
      if((config->videos) && !sharedBlobs.contains("offlineonly") && (!sharedBlobs.contains("video"))) {
        if ((!cache) || (cache && cache->videoData == "")) {
          getVideo(game);
        }
      }
      break;
    case MANUAL:
      if((config->manuals) && !sharedBlobs.contains("offlineonly") && (!sharedBlobs.contains("manual"))) {
        if ((!cache) || (cache && cache->manualData == "")) {
          getManual(game);
        }
      }
      break;
    default:
      ;
    }
  }
}

void OfflineMobyGames::getReleaseDate(GameEntry &game)
{
  // game.releaseDate = jsonDoc.object()["first_release_date"].toString();
  QJsonArray jsonPlatforms = jsonObj["platforms"].toArray();
  while(!jsonPlatforms.isEmpty()) {
    QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
    QString gamePlatformId = QString::number(jsonPlatform["platform_id"].toInt());
    if (gamePlatformId == platformId ) {
      game.releaseDate = jsonPlatform["first_release_date"].toString();
      break;
    }
    jsonPlatforms.removeFirst();
  }
}

void OfflineMobyGames::getPlayers(GameEntry &game)
{
  QJsonArray jsonAttribs = jsonDoc.object()["attributes"].toArray();
  for(int a = 0; a < jsonAttribs.count(); ++a) {
    QString players = jsonAttribs.at(a).toObject()["attribute_category_name"].toString();
    if(players == "Number of Players Supported" || players == "Number of Offline Players") {
      game.players = players;
    }
  }
}

void OfflineMobyGames::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(int a = 0; a < jsonGenres.count(); ++a) {
    game.tags.append(jsonGenres.at(a).toObject()["genre_name"].toString() + ", ");
  }
  game.tags = game.tags.left(game.tags.length() - 2);
}

void OfflineMobyGames::getAges(GameEntry &game)
{
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
}

void OfflineMobyGames::getPublisher(GameEntry &game)
{
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

void OfflineMobyGames::getDeveloper(GameEntry &game)
{
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
  if(jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: regionPrios) {
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
  if(jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: regionPrios) {
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
        if (game.manualFormat.length()>4) {
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
  if(jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: regionPrios) {
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
  if(jsonMedia.isEmpty()) {
    return;
  }

  QString coverUrl = "";
  bool foundFrontCover= false;

  for(const auto &region: regionPrios) {
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

  if (coverUrl.isEmpty()) {
    for(const auto &region: regionPrios) {
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
  if(jsonScreens.isEmpty()) {
    return;
  }

  QJsonArray jsonScreenshots = jsonScreens.object()["screenshots"].toArray();

  if(jsonScreenshots.count() < 1) {
    return;
  }
  int chosen = 1;
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
  if(jsonScreens.isEmpty()) {
    return;
  }

  QJsonArray jsonScreenshots = jsonScreens.object()["screenshots"].toArray();

  if(jsonScreenshots.count() < 2) {
    return;
  }
  int chosen = 2;
  if(jsonScreenshots.count() >= 3) {
    // First 2 are almost always not ingame, so skip those if we have 3 or more
#if QT_VERSION >= 0x050a00
    chosen = (QRandomGenerator::global()->generate() % jsonScreenshots.count() - 3) + 3;
#else
    chosen = (qrand() % jsonScreenshots.count() - 3) + 3;
#endif
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

QString OfflineMobyGames::getPlatformId(const QString platform)
{
  auto it = platformToId.find(platform);
  if(it != platformToId.cend())
      return QString::number(it.value());

  return "na";
}

void OfflineMobyGames::loadConfig(const QString& configPath)
{
  platformToId.clear();

  QFile configFile(configPath);
  if (!configFile.open(QIODevice::ReadOnly))
    return;

  QByteArray saveData = configFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty())
    return;

  QJsonArray platformsArray = json["platforms"].toArray();
  for (int platformIndex = 0; platformIndex < platformsArray.size(); ++platformIndex) {
    QJsonObject platformObject = platformsArray[platformIndex].toObject();

    QString platformName = platformObject["platform_name"].toString().toLower();
    int platformId = platformObject["platform_id"].toInt(-1);

    if(platformId > 0)
      platformToId[platformName] = platformId;
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

QList<QString> OfflineMobyGames::getSearchNames(const QFileInfo &info)
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
      noSubtitle = NameTools::getUrlQueryName(noSubtitle, -1, " ");
      noSubtitle = StrTools::sanitizeName(noSubtitle, true);
      searchNames.append(noSubtitle);
    }
  } */
  // printf("D: %s\n", sanitizedName.toStdString().c_str());
  // qDebug() << "Search Names;" << name << ";" << searchNames;
  return searchNames;
}
