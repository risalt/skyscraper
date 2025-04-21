/***************************************************************************
 *            arcadedb.cpp
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

#include <QUrl>
#include <QJsonArray>

#include "arcadedb.h"
#include "strtools.h"
#include "platform.h"

ArcadeDB::ArcadeDB(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId,
                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  baseUrl = "http://adb.arcadeitalia.net";
  searchUrlPre = "http://adb.arcadeitalia.net/service_scraper.php?ajax=query_mame&lang=en&use_parent=1&game_name=";

  if(Platform::get().getFamily(config->platform) != "arcade" && !config->useChecksum &&
     (!config->extensions.contains("mame") && !config->addExtensions.contains("mame"))) {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by ArcadeItalia "
           "(only Arcade platforms are supported)...\033[0m\n");
    return;
  }

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(RATING);
  fetchOrder.append(FRANCHISES);
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
  netComm->request(searchUrlPre + QUrl::toPercentEncoding(searchName));
  q.exec();
  if(netComm->getError() == QNetworkReply::NoError) {
    searchError = false;
  } else {
    printf("Connection error. Is the API down?\n");
    searchError = true;
    return;
  }
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

  game.id = searchName.toLower();
  game.title = jsonObj.value("title").toString();
  game.platform = Platform::get().getAliases(platform).at(1);
  game.miscData = QJsonDocument(jsonObj).toJson(QJsonDocument::Compact);
  gameEntries.append(game);
}

void ArcadeDB::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  jsonObj = QJsonDocument::fromJson(game.miscData).object();

  fetchGameResources(game, sharedBlobs, cache);
}

void ArcadeDB::getReleaseDate(GameEntry &game)
{
  game.releaseDate = StrTools::conformReleaseDate(jsonObj.value("year").toString());
}

void ArcadeDB::getPlayers(GameEntry &game)
{
  game.players = StrTools::conformPlayers(QString::number(jsonObj.value("players").toInt()));
}

void ArcadeDB::getRating(GameEntry &game)
{
  if(jsonObj.value("rate").toInt() > 0) {
    game.rating = QString::number(jsonObj.value("rate").toInt()/100.0, 'f', 3);
  }
}

void ArcadeDB::getTags(GameEntry &game)
{
  game.tags = StrTools::conformTags(jsonObj.value("genre").toString().replace(" / ", ", "));
}

void ArcadeDB::getPublisher(GameEntry &game)
{
  game.publisher = jsonObj.value("manufacturer").toString();
}

void ArcadeDB::getFranchises(GameEntry &game)
{
  game.franchises = StrTools::conformTags(jsonObj.value("serie").toString());
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
    netComm->request("https://www.progettosnaps.net/videosnaps/mp4/" + game.id + ".mp4");
    q.exec();
    game.videoData = netComm->getData();
    if(netComm->getError() == QNetworkReply::NoError &&
       game.videoData.length() > 4096) {
      game.videoFormat = "mp4";
    } else {    
      game.videoData = "";
    }
  }
}

void ArcadeDB::getManual(GameEntry &game)
{
  netComm->request("http://adb.arcadeitalia.net/download_file.php?tipo=mame_current&codice=" +
         game.id + "&entity=manual&oper=view&filler=" + game.id + ".pdf");
  q.exec();
  game.manualData = netComm->getData();
  if(netComm->getError() == QNetworkReply::NoError &&
     game.manualData.length() > 4096) {
    game.manualFormat = "pdf";
  } else {
    netComm->request("https://www.progettosnaps.net/manuals/pdf/" + game.id + ".pdf");
    q.exec();
    game.manualData = netComm->getData();
    if(netComm->getError() == QNetworkReply::NoError &&
       game.manualData.length() > 4096) {
      game.manualFormat = "pdf";
    } else {    
      game.manualData = "";
    }
  }
}

QStringList ArcadeDB::getSearchNames(const QFileInfo &info)
{
  QStringList searchNames;
  if(info.suffix() == "mame" && info.isSymbolicLink()) {
    QFileInfo mameFile = info.symLinkTarget();
    searchNames.append(mameFile.baseName());
  } else {
    searchNames.append(info.baseName());
  }
  if(config->verbosity >= 2) {
    qDebug() << "Search Name:" << searchNames.first();
  }
  return searchNames;
}
