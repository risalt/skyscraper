/***************************************************************************
 *            arcadedb.cpp
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

#include "arcadedb.h"
#include "strtools.h"
#include "platform.h"

ArcadeDB::ArcadeDB(Settings *config,
                   QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  incrementalScraping = true;

  baseUrl = "http://adb.arcadeitalia.net";
  searchUrlPre = "http://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=";

  fetchOrder.append(PUBLISHER);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(COVER);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(VIDEO);
  fetchOrder.append(MANUAL);
}

void ArcadeDB::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString platform)
{
  if (Platform::get().getFamily(config->platform) != "arcade") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by ArcadeItalia (only Arcade platforms are supported)...\033[0m\n");
    return;
  }

  netComm->request(searchUrlPre + searchName);
  q.exec();
  data = netComm->getData();

  if(data.indexOf("{\"release\":1,\"result\":[]}") != -1) {
    return;
  }
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    return;
  }
  jsonObj = jsonDoc.object().value("result").toArray().first().toObject();

  if(jsonObj.value("title") == QJsonValue::Undefined) {
    return;
  }

  GameEntry game;

  internalName = searchName;
  game.title = jsonObj.value("title").toString();
  game.platform = Platform::get().getAliases(platform).at(1);
  gameEntries.append(game);
}

void ArcadeDB::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case DESCRIPTION:
      getDescription(game);
      break;
    case DEVELOPER:
      getDeveloper(game);
      break;
    case PUBLISHER:
      getPublisher(game);
      break;
    case PLAYERS:
      getPlayers(game);
      break;
    case RATING:
      getRating(game);
      break;
    case AGES:
      getAges(game);
      break;
    case TAGS:
      getTags(game);
      break;
    case FRANCHISES:
      getFranchises(game);
      break;
    case RELEASEDATE:
      getReleaseDate(game);
      break;
    case COVER:
      if(config->cacheCovers) {
        if ((!cache) || (cache && cache->coverData.isNull())) {
          getCover(game);
        }
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots) {
        if ((!cache) || (cache && cache->screenshotData.isNull())) {
          getScreenshot(game);
        }
      }
      break;
    case WHEEL:
      if(config->cacheWheels) {
        if ((!cache) || (cache && cache->wheelData.isNull())) {
          getWheel(game);
        }
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees) {
        if ((!cache) || (cache && cache->marqueeData.isNull())) {
          getMarquee(game);
        }
      }
      break;
    case TEXTURE:
      if (config->cacheTextures) {
        if ((!cache) || (cache && cache->textureData.isNull())) {
          getTexture(game);
        }
      }
      break;
    case VIDEO:
      if((config->videos) && (!sharedBlobs.contains("video"))) {
        if ((!cache) || (cache && cache->videoData == "")) {
          getVideo(game);
        }
      }
      break;
    case MANUAL:
      if((config->manuals) && (!sharedBlobs.contains("manual"))) {
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

void ArcadeDB::getReleaseDate(GameEntry &game)
{
  game.releaseDate = jsonObj.value("year").toString();
}

void ArcadeDB::getPlayers(GameEntry &game)
{
  game.players = QString::number(jsonObj.value("players").toInt());
}

void ArcadeDB::getTags(GameEntry &game)
{
  game.tags = jsonObj.value("genre").toString().replace(" / ", ", ");
}

void ArcadeDB::getPublisher(GameEntry &game)
{
  game.publisher = jsonObj.value("manufacturer").toString();
}

void ArcadeDB::getDescription(GameEntry &game)
{
  game.description = jsonObj.value("history").toString();
  if(game.description.contains("- CONTRIBUTE")) {
    game.description = game.description.left(game.description.indexOf("- CONTRIBUTE")).trimmed();
  }
}

void ArcadeDB::getCover(GameEntry &game)
{
  if(!jsonObj.contains("url_image_flyer") ||
     jsonObj.value("url_image_flyer").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_image_flyer").toString());
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.coverData = netComm->getData();
  }
}

void ArcadeDB::getScreenshot(GameEntry &game)
{
  if(!jsonObj.contains("url_image_ingame") ||
     jsonObj.value("url_image_ingame").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_image_ingame").toString());
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

void ArcadeDB::getWheel(GameEntry &game)
{
  if(!jsonObj.contains("url_image_title") ||
     jsonObj.value("url_image_title").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_image_title").toString());
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void ArcadeDB::getMarquee(GameEntry &game)
{
  if(!jsonObj.contains("url_image_marquee") ||
     jsonObj.value("url_image_marquee").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_image_marquee").toString());
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.marqueeData = netComm->getData();
  }
}

void ArcadeDB::getTexture(GameEntry &game)
{
  if(!jsonObj.contains("url_image_cabinet") ||
     jsonObj.value("url_image_cabinet").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_image_cabinet").toString());
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.textureData = netComm->getData();
  }
}

void ArcadeDB::getVideo(GameEntry &game)
{
  if(!jsonObj.contains("url_video_shortplay") ||
     jsonObj.value("url_video_shortplay").toString().isEmpty()) {
    return;
  }
  netComm->request(jsonObj.value("url_video_shortplay").toString());
  q.exec();
  game.videoData = netComm->getData();
  if(netComm->getError() == QNetworkReply::NoError &&
     game.videoData.length() > 4096) {
    game.videoFormat = "mp4";
  } else {
    game.videoData = "";
  }
}

void ArcadeDB::getManual(GameEntry &game)
{
  netComm->request("http://adb.arcadeitalia.net/download_file.php?tipo=mame_current&codice=" +
         internalName + "&entity=manual&oper=view&filler=" + internalName + ".pdf");
  q.exec();
  game.manualData = netComm->getData();
  if(netComm->getError() == QNetworkReply::NoError &&
     game.manualData.length() > 4096) {
    game.manualFormat = "pdf";
  } else {
    game.manualData = "";
  }
}

QList<QString> ArcadeDB::getSearchNames(const QFileInfo &info)
{
  QList<QString> searchNames;
  searchNames.append(info.baseName());
  return searchNames;
}
