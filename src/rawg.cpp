/***************************************************************************
 *            rawg.cpp
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

#include "rawg.h"
#include "skyscraper.h"
#include "nametools.h"
#include "strtools.h"

RawG::RawG(Settings *config,
           QSharedPointer<NetManager> manager,
           QString threadId,
           NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  QString platformDb;

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(500); // Half a second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  loadConfig("rawg.json", "code", "id");
  platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    printf("\033[0;31mPlatform not supported by Rawg (see file rawg.json)...\033[0m\n");
    reqRemaining = 0;
    return;
  }

  baseUrl = "https://api.rawg.io/api/games";
  urlPost = "?key=" + config->userCreds;

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(AGES);
  fetchOrder.append(RATING);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(VIDEO);
}

void RawG::getSearchResults(QList<GameEntry> &gameEntries,
                            QString searchName, QString platform)
{
  QString searchUrl = baseUrl + urlPost + "&page_size=10&platforms=" +
                      platformId + "&search=" + searchName;
  limiter.exec();
  netComm->request(searchUrl);
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

  if(data.contains("API limit reached")) {
    printf("\033[1;31mThe RAWG monthly API limit has been reached. Try next month.\033[0m\n");
    searchError = true;
    reqRemaining = 0;
    return;
  }
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    searchError = true;
    return;
  }

  QJsonArray jsonGames = jsonDoc.object()["results"].toArray();
  while(!jsonGames.isEmpty()) {
    GameEntry game;
    QJsonObject jsonGame = jsonGames.first().toObject();
    game.platform = Platform::get().getAliases(platform).at(1);
    game.title = jsonGame["name"].toString();
    game.id = QString::number(jsonGame["id"].toInt());
    gameEntries.append(game);
    jsonGames.removeFirst();
  }
}

void RawG::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  printf("Waiting to get game data...\n");
  limiter.exec();
  netComm->request(baseUrl + "/" + game.id + urlPost);
  q.exec();
  data = netComm->getData();

  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    return;
  }

  jsonObj = jsonDoc.object();

  fetchGameResources(game, sharedBlobs, cache);
}

void RawG::getReleaseDate(GameEntry &game)
{
  QJsonArray jsonPlatforms = jsonObj["platforms"].toArray();
  while(!jsonPlatforms.isEmpty()) {
    QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
    QString gamePlatformId = QString::number(jsonPlatform["platform"].toObject()["id"].toInt());
    if(platformId.split(",").contains(gamePlatformId)) {
      game.releaseDate = jsonPlatform["released_at"].toString();
      break;
    }
    jsonPlatforms.removeFirst();
  }
  if(game.releaseDate.isEmpty()) {
    game.releaseDate = jsonObj["released"].toString();
  }
  game.releaseDate = StrTools::conformReleaseDate(game.releaseDate);
}

void RawG::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(int a = 0; a < jsonGenres.count(); ++a) {
    game.tags.append(jsonGenres.at(a).toObject()["name"].toString() + ", ");
  }
  game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
}

void RawG::getAges(GameEntry &game)
{
  QJsonObject jsonAges = jsonObj["esrb_rating"].toObject();
  if(!jsonAges["name"].toString().isEmpty()) {
    game.ages = StrTools::conformAges(jsonAges["name"].toString());
  }
}

void RawG::getPublisher(GameEntry &game)
{
  QJsonArray jsonReleases = jsonObj["publishers"].toArray();
  if(!jsonReleases.isEmpty()) {
    game.developer = jsonReleases.at(0).toObject()["name"].toString();
  }
}

void RawG::getDeveloper(GameEntry &game)
{
  QJsonArray jsonReleases = jsonObj["developers"].toArray();
  if(!jsonReleases.isEmpty()) {
    game.developer = jsonReleases.at(0).toObject()["name"].toString();
  }
}

void RawG::getDescription(GameEntry &game)
{
  game.description = jsonObj["description_raw"].toString();

  // Remove all html tags within description
  game.description = StrTools::stripHtmlTags(game.description);
}

void RawG::getRating(GameEntry &game)
{
  double rating = 0.0;
  QJsonArray jsonPlatforms = jsonObj["metacritic_platforms"].toArray();
  while(!jsonPlatforms.isEmpty()) {
    QJsonObject jsonPlatform = jsonPlatforms.first().toObject();
    QString gamePlatformId = QString::number(jsonPlatform["platform"].toObject()["platform"].toInt());
    if(platformId.split(",").contains(gamePlatformId)) {
      rating = (double)jsonPlatform["metascore"].toInt() / 100.0;
      break;
    }
    jsonPlatforms.removeFirst();
  }
  if(rating == 0.0) {
    QJsonValue jsonValue = jsonObj["metacritic"];
    if(jsonValue != QJsonValue::Undefined) {
      rating = (double)jsonValue.toInt() / 100.0;
    }
  }
  if(rating == 0.0) {
    QJsonValue jsonValue = jsonObj["rating"];
    if(jsonValue != QJsonValue::Undefined) {
      rating = jsonValue.toDouble() / 5.0;
    }
  }
  if(!(rating == 0.0)) {
    game.rating = QString::number(rating);
  }
}

void RawG::getMarquee(GameEntry &game)
{
  QString coverUrl = jsonObj["background_image"].toString();
  if(!coverUrl.isEmpty()) {
    printf("Waiting to get marquee data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}

void RawG::getScreenshot(GameEntry &game)
{
  printf("Waiting to get media data...\n");
  limiter.exec();
  netComm->request(baseUrl + "/" + game.id + "/screenshots" + urlPost);
  q.exec();
  data = netComm->getData();
  jsonScreens = QJsonDocument::fromJson(data).object();

  QJsonArray jsonScreenshots = jsonScreens["results"].toArray();
  while(!jsonScreenshots.isEmpty()) {
    QJsonObject jsonScreenshot = jsonScreenshots.first().toObject();
    QString url = jsonScreenshot["image"].toString();
    if(!jsonScreenshot["is_deleted"].toBool() && !url.isEmpty()) {
      printf("Waiting to get screenshot data...\n");
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError() == QNetworkReply::NoError &&
         image.loadFromData(netComm->getData())) {
        game.screenshotData = netComm->getData();
        return;
      }
    }
    jsonScreenshots.removeFirst();
  }
}

void RawG::getVideo(GameEntry &game)
{
  printf("Waiting to get video data...\n");
  limiter.exec();
  netComm->request(baseUrl + "/" + game.id + "/movies" + urlPost);
  q.exec();
  data = netComm->getData();
  jsonMedia = QJsonDocument::fromJson(data).object();

  QJsonArray jsonTrailers = jsonMedia["results"].toArray();
  while(!jsonTrailers.isEmpty()) {
    QJsonObject jsonTrailer = jsonTrailers.first().toObject();
    QString url = jsonTrailer["data"].toObject()["480"].toString();
    if(!url.isEmpty()) {
      printf("Waiting to get trailer data...\n");
      netComm->request(url);
      q.exec();
      game.videoData = netComm->getData();
      // Make sure received data is actually a video file
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         contentType.contains("video/") && game.videoData.size() > 4096) {
        game.videoFormat = contentType.mid(contentType.indexOf("/") + 1,
                                           contentType.length() - contentType.indexOf("/") + 1);
        return;
      } else {
        game.videoData = "";
      }
    }
    jsonTrailers.removeFirst();
  }
}
