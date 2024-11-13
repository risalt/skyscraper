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


TheGamesDb::TheGamesDb(Settings *config,
                       QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  loadMaps();

  baseUrl = "https://api.thegamesdb.net/v1";
  searchUrlPre = "https://api.thegamesdb.net/v1/Games/ByGameName?apikey=";

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
                                  QString searchName, QString platform)
{
  netComm->request(searchUrlPre + config->userCreds + "&name="+ searchName);
  q.exec();
  data = netComm->getData();
  // printf(data);
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    return;
  }

  reqRemaining = std::max(0, jsonDoc.object()["remaining_monthly_allowance"].toInt());
  if(reqRemaining <= 0)
    printf("\033[1;31mYou've reached TheGamesdDb's request limit for this month.\033[0m\n");

  if(jsonDoc.object()["status"].toString() != "Success") {
    return;
  }
  if(jsonDoc.object()["data"].toObject()["count"].toInt() < 1) {
    return;
  }

  QJsonArray jsonGames = jsonDoc.object()["data"].toObject()["games"].toArray();

  while(!jsonGames.isEmpty()) {
    QJsonObject jsonGame = jsonGames.first().toObject();

    GameEntry game;
    // https://api.thegamesdb.net/v1/Games/ByGameID?id=88&apikey=XXX&fields=game_title,players,release_date,developer,publisher,genres,overview,rating,platform
    game.id = QString::number(jsonGame["id"].toInt());
    game.url = "https://api.thegamesdb.net/v1/Games/ByGameID?id=" + game.id + "&apikey=" + config->userCreds + "&fields=game_title,players,release_date,developers,publishers,genres,overview,rating,youtube";
    game.title = jsonGame["game_title"].toString();
    // Remove anything at the end with a parentheses. 'thegamesdb' has a habit of adding
    // for instance '(1993)' to the name.
    game.title = game.title.left(game.title.indexOf("(")).simplified();
    game.platform = platformToId[QString::number(jsonGame["platform"].toInt())];
    if(platformMatch(game.platform, platform)) {
      gameEntries.append(game);
    }
    jsonGames.removeFirst();
  }
}

void TheGamesDb::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  netComm->request(game.url);
  q.exec();
  data = netComm->getData();
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    printf("No returned json data, is 'thegamesdb' down?\n");
    reqRemaining = 0;
  }

  reqRemaining = std::max(0, jsonDoc.object()["remaining_monthly_allowance"].toInt());

  if(jsonDoc.object()["data"].toObject()["count"].toInt() < 1) {
    printf("No returned json game document, is 'thegamesdb' down?\n");
    reqRemaining = 0;
  }

  jsonObj = jsonDoc.object()["data"].toObject()["games"].toArray().first().toObject();

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
  QString request = "https://cdn.thegamesdb.net/images/original/boxart/front/" + game.id + "-1";
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
  }
}

void TheGamesDb::getScreenshot(GameEntry &game)
{
  QString request = "https://cdn.thegamesdb.net/images/original/screenshots/" + game.id + "-1";
  netComm->request(request + ".jpg");
  q.exec();
  if(netComm->getError() != QNetworkReply::NoError) {
    netComm->request(request + ".png");
    q.exec();
  }
  if(netComm->getError() != QNetworkReply::NoError) {
    request = "https://cdn.thegamesdb.net/images/original/screenshot/" + game.id + "-1";
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
  }
}

void TheGamesDb::getWheel(GameEntry &game)
{
  QString request = "https://cdn.thegamesdb.net/images/original/titlescreen/" + game.id + "-1";
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
  }
}

void TheGamesDb::getMarquee(GameEntry &game)
{
  QString request = "https://cdn.thegamesdb.net/images/original/fanart/" + game.id + "-1";
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
  }
}

void TheGamesDb::getTexture(GameEntry &game)
{
  QString request = "https://cdn.thegamesdb.net/images/original/boxart/back/" + game.id + "-1";
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
  }/* else {
    netComm->request("https://cdn.thegamesdb.net/images/original/clearlogo/" + game.id + ".png");
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
    }
  }*/
}

void TheGamesDb::getVideo(GameEntry &game)
{
  getOnlineVideo(jsonObj["youtube"].toString(), game);
}

void TheGamesDb::loadMaps()
{
  loadConfig("thegamesdb.json", "id", "name");

  genreMap[1] = "Action";
  genreMap[2] = "Adventure";
  genreMap[3] = "Construction and Management Simulation";
  genreMap[4] = "Role-Playing";
  genreMap[5] = "Puzzle";
  genreMap[6] = "Strategy";
  genreMap[7] = "Racing";
  genreMap[8] = "Shooter";
  genreMap[9] = "Life Simulation";
  genreMap[10] = "Fighting";
  genreMap[11] = "Sports";
  genreMap[12] = "Sandbox";
  genreMap[13] = "Flight Simulator";
  genreMap[14] = "MMO";
  genreMap[15] = "Platform";
  genreMap[16] = "Stealth";
  genreMap[17] = "Music";
  genreMap[18] = "Horror";
  genreMap[19] = "Vehicle Simulation";

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
