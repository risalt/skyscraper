/***************************************************************************
 *            mobygames.cpp
 *
 *  Fri Mar 30 12:00:00 CEST 2018
 *  Copyright 2018 Lars Muldjord
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

#include "mobygames.h"
#include "skyscraper.h"
#include "strtools.h"

#include <QJsonArray>
#include <QRandomGenerator>

MobyGames::MobyGames(Settings *config,
                     QSharedPointer<NetManager> manager,
                     QString threadId,
                     NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  if(config->userCreds.isEmpty()) {
    printf("ERROR: Missing Mobygames API Key: please add one to the configuration file.\n");
    reqRemaining = 0;
    return;
  }

  loadConfig("mobygames.json", "platform_name", "platform_id");

  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by MobyGames (see file mobygames.json)...\033[0m\n");
    return;
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(10000); // 10 second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://api.mobygames.com";

  searchUrlPre = "https://api.mobygames.com/v1/games";

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(AGES);
  fetchOrder.append(RATING);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MANUAL);
}

void MobyGames::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  if(searchName.contains("(") || searchName.contains("[")) {
    return;
  }

  for(const auto &onePlatformId: platformId.split(",")) {
    printf("Waiting as advised by MobyGames api restrictions...\n");
    limiter.exec();
    QString searchUrl = searchUrlPre + "?api_key=" + config->userCreds +
                        "&title=" + StrTools::simplifyLetters(searchName) +
                        "&platform=" + onePlatformId;
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
    printf("s %s\n", searchUrl.toStdString().c_str());

    jsonDoc = QJsonDocument::fromJson(data);
    if(jsonDoc.isEmpty()) {
      searchError = true;
      return;
    }

    if(jsonDoc.object()["code"].toInt() == 429) {
      printf("\033[1;31mToo many requests! This is probably because some other "
             "Skyscraper user is currently using the 'mobygames' module. Please "
             "wait a while and try again.\n\nNow quitting...\033[0m\n");
      reqRemaining = 0;
      searchError = true;
      return;
    }

    QJsonArray jsonGames = jsonDoc.object()["games"].toArray();

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
        if(platformId.split(",").contains(gamePlatformId)) {
          game.url = searchUrlPre + "/" + game.id + "/platforms/" +
                     gamePlatformId + "?api_key=" + config->userCreds;
          game.platform = jsonPlatform["platform_name"].toString();
          gameEntries.append(game);
          const QJsonArray variants = jsonGame["alternate_titles"].toArray();
          for(const auto &variant: std::as_const(variants)) {
            GameEntry variantGame = game;
            variantGame.title = variant.toObject()["title"].toString();
            if(!variantGame.title.isEmpty() && variantGame.title != game.title) {
              gameEntries.append(variantGame);
            }
          }
        }
        jsonPlatforms.removeFirst();
      }
      jsonGames.removeFirst();
    }

    if(!gameEntries.isEmpty()) {
      break;
    }
  }
}

void MobyGames::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
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

  jsonObj = QJsonDocument::fromJson(game.miscData).object();

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

  fetchGameResources(game, sharedBlobs, cache);
}

void MobyGames::getReleaseDate(GameEntry &game)
{
  game.releaseDate = StrTools::conformReleaseDate(jsonDoc.object()["first_release_date"].toString());
}

void MobyGames::getPlayers(GameEntry &game)
{
  QJsonArray jsonAttribs = jsonDoc.object()["attributes"].toArray();
  for(int a = 0; a < jsonAttribs.count(); ++a) {
    if(jsonAttribs.at(a).toObject()["attribute_category_name"].toString() == "Number of Players Supported" ||
       jsonAttribs.at(a).toObject()["attribute_category_name"].toString() == "Number of Offline Players") {
      game.players = StrTools::conformPlayers(jsonAttribs.at(a).toObject()["attribute_name"].toString());
    }
  }
}

void MobyGames::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(int a = 0; a < jsonGenres.count(); ++a) {
    game.tags.append(jsonGenres.at(a).toObject()["genre_name"].toString() + ", ");
  }
  game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
}

void MobyGames::getAges(GameEntry &game)
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
  game.ages = StrTools::conformAges(game.ages);
}

void MobyGames::getPublisher(GameEntry &game)
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

void MobyGames::getDeveloper(GameEntry &game)
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

void MobyGames::getDescription(GameEntry &game)
{
  game.description = jsonObj["description"].toString();

  // Remove all html tags within description
  game.description = StrTools::stripHtmlTags(game.description);
}

void MobyGames::getRating(GameEntry &game)
{
  QJsonValue jsonValue = jsonObj["moby_score"];
  if(jsonValue != QJsonValue::Undefined) {
    double rating = jsonValue.toDouble();
    if(rating != 0.0) {
      game.rating = QString::number(rating / 10.0);
    }
  }
}

void MobyGames::getCover(GameEntry &game)
{
  if(jsonMedia.isEmpty()) {
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
    printf("Waiting to get cover data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
    }
  }
}

void MobyGames::getManual(GameEntry &game)
{
  if(jsonMedia.isEmpty()) {
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
    printf("Waiting to get manual data...\n");
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

void MobyGames::getMarquee(GameEntry &game)
{
  if(jsonMedia.isEmpty()) {
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

void MobyGames::getTexture(GameEntry &game)
{
  if(jsonMedia.isEmpty()) {
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
    printf("Waiting to get back texture data...\n");
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
    }
  }
}

void MobyGames::getWheel(GameEntry &game)
{
  if(jsonScreens.isEmpty()) {
    return;
  }

  QJsonArray jsonScreenshots = jsonScreens.object()["screenshots"].toArray();

  if(jsonScreenshots.count() < 1) {
    return;
  }
  int chosen = 0;
  printf("Waiting to get wheel data...\n");
  netComm->request(jsonScreenshots.at(chosen).toObject()["image"].toString().replace("http://", "https://"));
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void MobyGames::getScreenshot(GameEntry &game)
{
  if(jsonScreens.isEmpty()) {
    return;
  }

  QJsonArray jsonScreenshots = jsonScreens.object()["screenshots"].toArray();

  int chosen = 1;
  if(jsonScreenshots.count() < 2) {
    return;
  } else if(jsonScreenshots.count() > 2) {
    // First 2 are almost always not ingame, so skip those if we have 3 or more
    chosen = (QRandomGenerator::global()->generate() % (jsonScreenshots.count() - 2)) + 2;
  }
  printf("Waiting to get screenshot data...\n");
  netComm->request(jsonScreenshots.at(chosen).toObject()["image"].toString().replace("http://", "https://"));
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

QString MobyGames::getRegionShort(const QString &region)
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
