/***************************************************************************
 *            igdb.cpp
 *
 *  Sun Aug 26 12:00:00 CEST 2018
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

#include <QJsonArray>
#include <QTextStream>

#include "igdb.h"
#include "platform.h"
#include "strtools.h"
#include "nametools.h"

Igdb::Igdb(Settings *config, QSharedPointer<NetManager> manager, QString threadId)
  : AbstractScraper(config, manager, threadId)
{
  QPair<QString, QString> clientIdHeader;
  clientIdHeader.first = "Client-ID";
  clientIdHeader.second = config->user;

  QPair<QString, QString> tokenHeader;
  tokenHeader.first = "Authorization";
  tokenHeader.second = "Bearer " + config->igdbToken;

  headers.append(clientIdHeader);
  headers.append(tokenHeader);

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  // 1.2 second request limit set a bit above 0.25 as requested by the folks at IGDB
  // (4 requests per second=1.0 seconds delay for 4 threads). Don't change! It will
  // break the module stability.
  limitTimer.setInterval(1200);
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://api.igdb.com/v4";

  searchUrlPre = "https://api.igdb.com/v4";

  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(RATING);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(TAGS);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(AGES);
  fetchOrder.append(SCREENSHOT);
  // Deprecated: fetchOrder.append(TEXTURE); if several screenshots
  fetchOrder.append(COVER);
  fetchOrder.append(MARQUEE);
  // Deprecated: fetchOrder.append(WHEEL); if several artworks (i.e. marquee)
  // Fetch one minute videos from youtube https://www.youtube.com/watch?v=%s
  fetchOrder.append(VIDEO);
}

void Igdb::getSearchResults(QList<GameEntry> &gameEntries,
                            QString searchName, QString platform)
{
  // Request list of games but don't allow re-releases ("game.version_parent = null")
  limiter.exec();
  // netComm->request(baseUrl + "/search/", "fields game.name,game.platforms.name; "
  // "search \"" + searchName + "\"; where game != null & game.version_parent = null;", headers);
  // netComm->request(baseUrl + "/search/", "fields game.name,game.id,game.alternative_names.name,"
  // "game.platforms.name; where name ~ \"" + searchName + "\" & game != null &"
  // " game.version_parent = null;", headers);
  // Try search based on partial match:
  netComm->request(baseUrl + "/games",
                   "fields name,id,alternative_names.name,platforms.name;"
                   " search \"" + searchName + "\";",
                   headers);
  q.exec();
  if(netComm->getError() != QNetworkReply::NoError &&
     netComm->getError() <= QNetworkReply::ProxyAuthenticationRequiredError) {
    printf("Connection error. Is the API down?\n");
    searchError = true;
    return;
  } else {
    searchError = false;
  }
  data = netComm->getData();
  if(config->verbosity > 3) {
    qDebug() << data;
  }

  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    searchError = true;
    return;
  }

  if(jsonDoc.object()["message"].toString().contains("Too Many Requests")) {
    printf("\033[1;31mThe IGDB requests per second limit has been exceeded, can't continue!\033[0m\n");
    reqRemaining = 0;
    searchError = true;
    return;
  }
  if(jsonDoc.object()["message"].toString().contains("Authorization Failure")) {
    printf("\033[1;31mThe IGDB authentication has failed, can't continue! Try removing file 'igdbToken.dat'.\033[0m\n");
    reqRemaining = 0;
    searchError = true;
    return;
  }

  QJsonArray jsonGames = jsonDoc.array();

  for(const auto &jsonGame: std::as_const(jsonGames)) {
    GameEntry game;

    game.title = jsonGame.toObject()["name"].toString();
    game.id = QString::number(jsonGame.toObject()["id"].toInt());

    QJsonArray jsonPlatforms = jsonGame.toObject()["platforms"].toArray();
    for(const auto &jsonPlatform: std::as_const(jsonPlatforms)) {
      //game.id.append(";" + QString::number(jsonPlatform.toObject()["id"].toInt()));
      game.platform = jsonPlatform.toObject()["name"].toString();
      if(platformMatch(game.platform, platform)) {
        game.id.append(";" + QString::number(jsonPlatform.toObject()["id"].toInt()));
        gameEntries.append(game);
        const QJsonArray variants = jsonGame.toObject()["alternative_names"].toArray();
        for(const auto &variant: std::as_const(variants)) {
          GameEntry variantGame = game;
          variantGame.title = variant.toObject()["name"].toString();
          if(!variantGame.title.isEmpty() && variantGame.title != game.title) {
            gameEntries.append(variantGame);
          }
        }
        break;
      }
    }
  }
}

void Igdb::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  limiter.exec();
  netComm->request(baseUrl + "/games/",
                   "fields age_ratings.rating,age_ratings.category,total_rating,cover.url,"
                   "game_modes.slug,genres.name,franchises.name,screenshots.url,"
                   "artworks.url,videos.video_id,storyline,summary,release_dates.date,"
                   "release_dates.region,release_dates.platform,involved_companies.company.name"
                   ",involved_companies.developer,involved_companies.publisher; where id = " +
                   game.id.split(";").first() + ";",
                   headers);
  q.exec();
  data = netComm->getData();

  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    return;
  }

  jsonObj = jsonDoc.array().first().toObject();

  fetchGameResources(game, sharedBlobs, cache);
}

void Igdb::getReleaseDate(GameEntry &game)
{
  QJsonArray jsonDates = jsonObj["release_dates"].toArray();
  bool regionMatch = false;
  for(const auto &region: std::as_const(regionPrios)) {
    for(const auto &jsonDate: std::as_const(jsonDates)) {
      int regionEnum = jsonDate.toObject()["region"].toInt();
      QString curRegion = "";
      if(regionEnum == 1)
        curRegion = "eu";
      else if(regionEnum == 2)
        curRegion = "us";
      else if(regionEnum == 3)
        curRegion = "au";
      else if(regionEnum == 4)
        curRegion = "nz";
      else if(regionEnum == 5)
        curRegion = "jp";
      else if(regionEnum == 6)
        curRegion = "cn";
      else if(regionEnum == 7)
        curRegion = "asi";
      else if(regionEnum == 8)
        curRegion = "wor";
      if(QString::number(jsonDate.toObject()["platform"].toInt()) ==
         game.id.split(";").last() &&
         region == curRegion) {
        game.releaseDate = QDateTime::fromMSecsSinceEpoch((qint64)jsonDate.toObject()["date"].toInt() * 1000).toString("yyyyMMdd");
        regionMatch = true;
        break;
      }
    }
    if(regionMatch)
      break;
  }
  game.releaseDate = StrTools::conformReleaseDate(game.releaseDate);
}

void Igdb::getPlayers(GameEntry &game)
{
  // This is a bit of a hack. The unique identifiers are as follows:
  // 1 = Single Player
  // 2 = Multiplayer
  // 3 = Cooperative
  // 4 = Split screen
  // 5 = MMO
  // So basically if != 1 it's at least 2 players. That's all we can gather from this
  game.players = "1";
  QJsonArray jsonPlayers = jsonObj["game_modes"].toArray();
  for(const auto &jsonPlayer: std::as_const(jsonPlayers)) {
    if(jsonPlayer.toObject()["id"].toInt() != 1) {
      game.players = "2";
      break;
    }
  }
}

void Igdb::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(const auto &jsonGenre: std::as_const(jsonGenres)) {
    game.tags.append(jsonGenre.toObject()["name"].toString() + ", ");
  }
  game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
}

void Igdb::getFranchises(GameEntry &game)
{
  QJsonArray jsonFranchises = jsonObj["franchises"].toArray();
  for(const auto &jsonFranchise: std::as_const(jsonFranchises)) {
    game.franchises.append(jsonFranchise.toObject()["name"].toString() + ", ");
  }
  game.franchises = StrTools::conformTags(game.franchises.left(game.franchises.length() - 2));
}

void Igdb::getAges(GameEntry &game)
{
  int agesEnum = jsonObj["age_ratings"].toArray().first().toObject()["rating"].toInt();
  if(agesEnum == 1) {
    game.ages = "3";
  } else if(agesEnum == 2) {
    game.ages = "7";
  } else if(agesEnum == 3) {
    game.ages = "12";
  } else if(agesEnum == 4) {
    game.ages = "16";
  } else if(agesEnum == 5) {
    game.ages = "18";
  } else if(agesEnum == 6) {
    // Rating pending
  } else if(agesEnum == 7) {
    game.ages = "EC";
  } else if(agesEnum == 8) {
    game.ages = "E";
  } else if(agesEnum == 9) {
    game.ages = "E10";
  } else if(agesEnum == 10) {
    game.ages = "T";
  } else if(agesEnum == 11) {
    game.ages = "M";
  } else if(agesEnum == 12) {
    game.ages = "AO";
  }
  game.ages = StrTools::conformAges(game.ages);
}

void Igdb::getPublisher(GameEntry &game)
{
  QJsonArray jsonCompanies = jsonObj["involved_companies"].toArray();
  for(const auto &jsonCompany: std::as_const(jsonCompanies)) {
    if(jsonCompany.toObject()["publisher"].toBool() == true) {
      game.publisher = jsonCompany.toObject()["company"].toObject()["name"].toString();
      return;
    }
  }
}

void Igdb::getDeveloper(GameEntry &game)
{
  QJsonArray jsonCompanies = jsonObj["involved_companies"].toArray();
  for(const auto &jsonCompany: std::as_const(jsonCompanies)) {
    if(jsonCompany.toObject()["developer"].toBool() == true) {
      game.developer = jsonCompany.toObject()["company"].toObject()["name"].toString();
      return;
    }
  }
}

void Igdb::getDescription(GameEntry &game)
{
  game.description = "";
  QJsonValue jsonValue = jsonObj["summary"];
  if(jsonValue != QJsonValue::Undefined) {
    game.description = StrTools::stripHtmlTags(jsonValue.toString()) + "\n";
  }
  jsonValue = jsonObj["storyline"];
  if(jsonValue != QJsonValue::Undefined && !jsonValue.toString().isEmpty()) {
    game.description = game.description + "\nPossible spoilers ahead!\n" + StrTools::stripHtmlTags(jsonValue.toString());
  }
}

void Igdb::getRating(GameEntry &game)
{
  QJsonValue jsonValue = jsonObj["total_rating"];
  if(jsonValue != QJsonValue::Undefined) {
    double rating = jsonValue.toDouble();
    if(rating != 0.0) {
      game.rating = QString::number(rating / 100.0);
    }
  }
}

void Igdb::getCover(GameEntry &game)
{
  QJsonValue jsonValue = jsonObj["cover"].toObject()["url"];
  if(jsonValue == QJsonValue::Undefined) {
    return;
  }
  QString coverUrl = "https:" + StrTools::stripHtmlTags(jsonValue.toString());
  coverUrl.replace("t_thumb", "t_original");

  netComm->request(coverUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.coverData = netComm->getData();
  }
}

void Igdb::getScreenshot(GameEntry &game)
{
  QJsonArray jsonScreenshots = jsonObj["screenshots"].toArray();
  int pos = 0;
  QString screenshotUrl;
  for(const auto &jsonScreenshot: std::as_const(jsonScreenshots)) {
    // Change 1 to 2 to allow second screenshot to be stored as Wheel
    if(pos < 1) {
      screenshotUrl = "https:" + jsonScreenshot.toObject()["url"].toString();
      screenshotUrl.replace("t_thumb", "t_original");
    }
    else {
      return;
    }
    netComm->request(screenshotUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      if(pos == 0) {
        game.screenshotData = netComm->getData();
      }
      else {
        game.wheelData = netComm->getData();
      }
    }
    pos++;
  }
}

void Igdb::getMarquee(GameEntry &game)
{
  QJsonArray jsonMarquees = jsonObj["artworks"].toArray();
  int pos = 0;
  QString marqueeUrl;
  for(const auto &jsonMarquee: std::as_const(jsonMarquees)) {
    // Change 1 to 2 to allow second artwork to be stored as Texture
    if(pos < 1) {
      marqueeUrl = "https:" + jsonMarquee.toObject()["url"].toString();
      marqueeUrl.replace("t_thumb", "t_original");
    }
    else {
      return;
    }
    netComm->request(marqueeUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      if(pos == 0) {
        game.marqueeData = netComm->getData();
      }
      else {
        game.textureData = netComm->getData();
      }
    }
    pos++;
  }
}

void Igdb::getVideo(GameEntry &game)
{
  QJsonArray jsonVideos = jsonObj["videos"].toArray();
  int pos = 0;
  QString videoUrl;
  for(const auto &jsonVideo: std::as_const(jsonVideos)) {
    if(pos < 1) {
      videoUrl = jsonVideo.toObject()["video_id"].toString();
      if(videoUrl.isEmpty()) {
        return;
      }
      videoUrl = "https://www.youtube.com/watch?v=" + videoUrl;
    }
    else {
      return;
    }
    getOnlineVideo(videoUrl, game);
    pos++;
  }
}
