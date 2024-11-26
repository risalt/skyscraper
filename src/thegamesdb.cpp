/***************************************************************************
 *            thegamesdb.cpp
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

#include <QJsonArray>

#include "thegamesdb.h"
#include "strtools.h"
#include "nametools.h"


TheGamesDb::TheGamesDb(Settings *config,
                       QSharedPointer<NetManager> manager,
                       QString threadId)
  : AbstractScraper(config, manager, threadId)
{
  loadMaps();
  platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by TheGamesDB or it hasn't "
           "yet been included in Skyscraper for this module...\033[0m\n");
    return;
  }

  baseUrl = "https://api.thegamesdb.net/v1";
  searchUrlPre = "https://api.thegamesdb.net/v1.1/Games/ByGameName?apikey=";
  searchUrlPost = "&fields=players,publishers,developers,genres,overview,last_updated,"
                  "rating,release_date,region_id,platform,coop,youtube,os,processor,"
                  "ram,hdd,video,sound,alternates,uids,hashes,serials"
                  "&include=platform"
                  "&filter[platform]=";

  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(AGES);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(VIDEO);
}

void TheGamesDb::getSearchResults(QList<GameEntry> &gameEntries,
                                  QString searchName, QString)
{
  netComm->request(searchUrlPre + config->userCreds + "&name=" +
                   NameTools::moveArticle(searchName, true) +
                   searchUrlPost + platformId);
  q.exec();
  if(netComm->getError() == QNetworkReply::NoError) {
    searchError = false;
  } else {
    if(netComm->getError() == QNetworkReply::ServiceUnavailableError ||
       netComm->getError() == QNetworkReply::ContentAccessDenied) {
      printf("\033[1;31mYou've reached TheGamesdDb's request limit for this month.\033[0m\n");
      reqRemaining = 0;
    } else {
      printf("Connection error. Is the API down?\n");
    }
    searchError = true;
    return;
  }
  data = netComm->getData();
  if(config->verbosity > 3) {
    qDebug() << data;
  }
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    printf("No returned json data, is 'thegamesdb' down?\n");
    searchError = true;
    return;
  }

  reqRemaining = std::max(0, jsonDoc.object()["remaining_monthly_allowance"].toInt());
  if(reqRemaining <= 0)
    printf("\033[1;31mYou've reached TheGamesdDb's request limit for this month.\033[0m\n");

  if(jsonDoc.object()["status"].toString() != "Success") {
    searchError = true;
    return;
  }
  if(jsonDoc.object()["data"].toObject()["count"].toInt() < 1) {
    return;
  }
  QJsonArray jsonGames = jsonDoc.object()["data"].toObject()["games"].toArray();

  QList<GameEntry> gameEntriesRegion;
  while(!jsonGames.isEmpty()) {
    GameEntry game;
    QJsonObject jsonGame = jsonGames.first().toObject();
    game.id = QString::number(jsonGame["id"].toInt());
    game.title = jsonGame["game_title"].toString();
    // Remove anything at the end with a parentheses. 'thegamesdb' has a habit of
    // adding for instance '(1993)' to the name.
    game.title = game.title.left(game.title.indexOf("(")).simplified();
    game.platform = jsonDoc.object()["include"].toObject()["platform"].
                     toObject()["data"].toObject()[QString::number(jsonGame["platform"].toInt())].
                     toObject()["name"].toString();
    game.miscData = QJsonDocument(jsonGame).toJson(QJsonDocument::Compact);
    game.url = regionMap[jsonGame["region_id"].toInt()];
    gameEntriesRegion.append(game);
    for(const auto &alternate: jsonGame["alternates"].toArray()) {
      game.title = alternate.toString();
      game.title = game.title.left(game.title.indexOf("(")).simplified();
      game.url = regionMap[jsonGame["region_id"].toInt()];
      gameEntriesRegion.append(game);
    }
    jsonGames.removeFirst();
  }

  QStringList gfRegions;
  for(const auto &region: std::as_const(regionPrios)) {
    if(region == "eu" || region == "fr" || region == "de" || region == "it" ||
       region == "sp" || region == "se" || region == "nl" || region == "dk" ||
       region == "gr" || region == "no" || region == "ru") {
      gfRegions.append("Europe");
    } else if(region == "us" || region == "ca") {
      gfRegions.append("USA");
      gfRegions.append("Americas");
    } else if(region == "wor") {
      gfRegions.append("World");
    } else if(region == "jp") {
      gfRegions.append("Japan");
    } else if(region == "kr") {
      gfRegions.append("Korea");
    } else if(region == "au"  || region == "uk" || region == "nz") {
      gfRegions.append("UK");
    } else if(region == "asi" || region == "tw" || region == "cn") {
      gfRegions.append("China");
    } else if(region == "ame" || region == "br") {
      gfRegions.append("Americas");
    } else {
      gfRegions.append("World");
    }
  }
  gfRegions.append("Others");
  gfRegions.removeDuplicates();
  for(const auto &country: std::as_const(gfRegions)) {
    for(const auto &game: std::as_const(gameEntriesRegion)) {
      if(game.url == country) {
        nameIds.insert(game.title, game.id);
        if(!jsonObj["youtube"].toString().isEmpty()) {
          nameVideos.insert(game.title, jsonObj["youtube"].toString());
        }
        gameEntries.append(game);
      }
    }
  }
}

void TheGamesDb::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  jsonDoc = QJsonDocument::fromJson(game.miscData);
  if(jsonDoc.isEmpty()) {
    printf("No returned json data, is 'thegamesdb' down?\n");
    reqRemaining = 0;
    return;
  }
  jsonObj = jsonDoc.object();

  fetchGameResources(game, sharedBlobs, cache);
}

void TheGamesDb::getReleaseDate(GameEntry &game)
{
  if(jsonObj["release_date"] != QJsonValue::Undefined)
    game.releaseDate = StrTools::conformReleaseDate(jsonObj["release_date"].toString());
}

void TheGamesDb::getDeveloper(GameEntry &game)
{
  QJsonArray developers = jsonObj["developers"].toArray();
  if(developers.count() != 0)
    game.developer = developerMap[developers.first().toInt()];
}

void TheGamesDb::getPublisher(GameEntry &game)
{
  QJsonArray publishers = jsonObj["publishers"].toArray();
  if(publishers.count() != 0)
    game.publisher = publisherMap[publishers.first().toInt()];
}

void TheGamesDb::getDescription(GameEntry &game)
{
  game.description = jsonObj["overview"].toString();
}

void TheGamesDb::getPlayers(GameEntry &game)
{
  int players = jsonObj["players"].toInt();
  if(players != 0)
    game.players = QString::number(players);
}

void TheGamesDb::getAges(GameEntry &game)
{
  if(jsonObj["rating"] != QJsonValue::Undefined)
    game.ages = StrTools::conformAges(jsonObj["rating"].toString());
}

void TheGamesDb::getTags(GameEntry &game)
{
  QJsonArray genres = jsonObj["genres"].toArray();
  if(genres.count() != 0) {
    while(!genres.isEmpty()) {
      game.tags.append(genreMap[genres.first().toInt()] + ", ");
      genres.removeFirst();
    }
    game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
  }
}

void TheGamesDb::getCover(GameEntry &game)
{
  for(const auto &id: nameIds.values(game.title)) {
    QString request = "https://cdn.thegamesdb.net/images/original/boxart/front/" + id + "-1";
    netComm->request(request + ".jpg");
    q.exec();
    if(netComm->getError() != QNetworkReply::NoError) {
      netComm->request(request + ".png");
      q.exec();
    }
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
      break;
    }
  }
}

void TheGamesDb::getScreenshot(GameEntry &game)
{
  for(const auto &id: nameIds.values(game.title)) {
    QString request = "https://cdn.thegamesdb.net/images/original/screenshots/" + id + "-1";
    netComm->request(request + ".jpg");
    q.exec();
    if(netComm->getError() != QNetworkReply::NoError) {
      netComm->request(request + ".png");
      q.exec();
    }
    if(netComm->getError() != QNetworkReply::NoError) {
      request = "https://cdn.thegamesdb.net/images/original/screenshot/" + id + "-1";
      netComm->request(request + ".jpg");
      q.exec();
      if(netComm->getError() != QNetworkReply::NoError) {
        netComm->request(request + ".png");
        q.exec();
      }
    }
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.screenshotData = netComm->getData();
      break;
    }
  }
}

void TheGamesDb::getWheel(GameEntry &game)
{
  for(const auto &id: nameIds.values(game.title)) {
    QString request = "https://cdn.thegamesdb.net/images/original/titlescreen/" + id + "-1";
    netComm->request(request + ".jpg");
    q.exec();
    if(netComm->getError() != QNetworkReply::NoError) {
      netComm->request(request + ".png");
      q.exec();
    }
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.wheelData = netComm->getData();
      break;
    }
  }
}

void TheGamesDb::getMarquee(GameEntry &game)
{
  for(const auto &id: nameIds.values(game.title)) {
    QString request = "https://cdn.thegamesdb.net/images/original/fanart/" + id + "-1";
    netComm->request(request + ".jpg");
    q.exec();
    if(netComm->getError() != QNetworkReply::NoError) {
      netComm->request(request + ".png");
      q.exec();
    }
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
      break;
    }
  }
}

void TheGamesDb::getTexture(GameEntry &game)
{
  for(const auto &id: nameIds.values(game.title)) {
    QString request = "https://cdn.thegamesdb.net/images/original/boxart/back/" + id + "-1";
    netComm->request(request + ".jpg");
    q.exec();
    if(netComm->getError() != QNetworkReply::NoError) {
      netComm->request(request + ".png");
      q.exec();
    }
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
      break;
    }
  }
}

void TheGamesDb::getVideo(GameEntry &game)
{
  for(const auto &id: nameVideos.values(game.title)) {
    getOnlineVideo(id, game);
    if(!game.videoFormat.isEmpty()) {
      break;
    }
  }
}

void TheGamesDb::loadMaps()
{
  loadConfig("thegamesdb.json", "code", "id");

  regionMap[0] = "World";    // Not region specific
  regionMap[1] = "USA";      // NTSC
  regionMap[2] = "Americas"; // NTSC-U - Americas
  regionMap[3] = "China";    // NTSC-C
  regionMap[4] = "Japan";    // NTSC-J - Japan and Asia
  regionMap[5] = "Korea";    // NTSC-K
  regionMap[6] = "Europe";   // PAL
  regionMap[7] = "UK";       // UK, Ireland and Australia
  regionMap[8] = "Europe";   // Europe
  regionMap[9] = "Others";   // Others/Hacks

  {
    QFile jsonFile("tgdb_genres.json");
    if(jsonFile.open(QIODevice::ReadOnly)) {
      QJsonObject jsonGenres = QJsonDocument::fromJson(jsonFile.readAll()).object()["data"].toObject()["genres"].toObject();
      for(QJsonObject::iterator it = jsonGenres.begin(); it != jsonGenres.end(); ++it) {
        genreMap[it.value().toObject()["id"].toInt()] = it.value().toObject()["name"].toString();
      }
      jsonFile.close();
    }
  }
  {
    QFile jsonFile("tgdb_developers.json");
    if(jsonFile.open(QIODevice::ReadOnly)) {
      QJsonObject jsonDevs = QJsonDocument::fromJson(jsonFile.readAll()).object()["data"].toObject()["developers"].toObject();
      for(QJsonObject::iterator it = jsonDevs.begin(); it != jsonDevs.end(); ++it) {
        developerMap[it.value().toObject()["id"].toInt()] = it.value().toObject()["name"].toString();
      }
      jsonFile.close();
    }
  }
  {
    QFile jsonFile("tgdb_publishers.json");
    if(jsonFile.open(QIODevice::ReadOnly)) {
      QJsonObject jsonPubs = QJsonDocument::fromJson(jsonFile.readAll()).object()["data"].toObject()["publishers"].toObject();
      for(QJsonObject::iterator it = jsonPubs.begin(); it != jsonPubs.end(); ++it) {
        publisherMap[it.value().toObject()["id"].toInt()] = it.value().toObject()["name"].toString();
      }
      jsonFile.close();
    }
  }
}
