/***************************************************************************
 *            vgfacts.cpp
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
#include <QDate>
#include <QJsonArray>
#include <QDirIterator>

#include "vgfacts.h"
#include "skyscraper.h"
#include "nametools.h"
#include "strtools.h"

#if QT_VERSION >= 0x050a00
#include <QRandomGenerator>
#endif

VGFacts::VGFacts(Settings *config, QSharedPointer<NetManager> manager, QString threadId)
  : AbstractScraper(config, manager, threadId)
{
  QString triviaDb;
  offlineScraper = true;
  
  baseUrl = "https://www.vgfacts.com";

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(1000); // 1 second request limit
  limitTimer.setSingleShot(false);
  limitTimer.start();

  QFile dbFile(config->vgfactsDb + "/" + "trivia.json");
  if(!dbFile.open(QIODevice::ReadOnly)){
    dbFile.close();
    printf("\nERROR: Database file %s cannot be accessed. ", dbFile.fileName().toStdString().c_str());
    printf("Please regenerate it using the program 'VGFacts Scraper'.\n");
    reqRemaining = 0;
    return;
  }
  printf("INFO: Reading VGFacts game database..."); fflush(stdout);
  fflush(stdout);
  triviaDb = QString::fromUtf8(dbFile.readAll());
  dbFile.close();

  QJsonDocument jsonDoc = QJsonDocument::fromJson(triviaDb.toUtf8());
  if(jsonDoc.isNull()) {
    printf("\nERROR: The VGFacts database file is missing, empty or corrupted. "
           "Please regenerate it using the program 'VGFacts Scraper'.\n");
    reqRemaining = 0;
    return;
  } else {
    QJsonObject jsonGames = jsonDoc.object();
    // Load offline vgfacts database
    for(const auto &jsonGameRef: std::as_const(jsonGames)) {
      QString gameId;
      QJsonObject jsonGame;
      if(jsonGameRef.isObject()) {
        jsonGame = jsonGameRef.toObject();
      } else {
        continue;
      }
      if(jsonGame["gameFaqsId"].isString()) {
        gameId = jsonGame["gameFaqsId"].toString();
      }
      QString gameName = jsonGame["titleDetail"].toObject()["title"].toString();
      QStringList platforms = jsonGame["titleDetail"].toObject()["platforms"].toString().split(";");
      bool platformFound = false;
      for(const auto &platform: std::as_const(platforms)) {
        if(platformMatch(platform, config->platform)) {
          QJsonObject changePlatform = jsonGame.take("titleDetail").toObject();
          changePlatform["platforms"] = platform;
          jsonGame.insert("titleDetail", changePlatform);
          platformFound = true;
          break;
        }
      }
      if(platformFound && !gameId.isEmpty()) {
        QStringList safeVariations, unsafeVariations;
        NameTools::generateSearchNames(gameName, safeVariations, unsafeVariations, true);
        for(const auto &name: std::as_const(safeVariations)) {
          const auto idTitle = qMakePair(gameId, gameName);
          if(!nameToId.contains(name, idTitle)) {
            if(config->verbosity > 2) {
              printf("Adding full variation: %s -> %s\n",
                     gameName.toStdString().c_str(), name.toStdString().c_str());
            }
            nameToId.insert(name, idTitle);
          }
        }
        for(const auto &name: std::as_const(unsafeVariations)) {
          const auto idTitle = qMakePair(gameId, gameName);
          if(!nameToIdTitle.contains(name, idTitle)) {
            if(config->verbosity > 2) {
              printf("Adding title variation: %s -> %s\n",
                     gameName.toStdString().c_str(), name.toStdString().c_str());
            }
            nameToIdTitle.insert(name, idTitle);
          }
        }
        idToGame[gameId] = jsonGame;
      }
    }
    printf(" DONE.\nINFO: Read %d game header entries from GameFaqs local database.\n", idToGame.size());
  }

  fetchOrder.append(DEVELOPER);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(TRIVIA);
  fetchOrder.append(COVER);
  fetchOrder.append(MARQUEE);
}

void VGFacts::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  QList<QPair<QString, QString>> matches = {};

  if(getSearchResultsOffline(matches, searchName, nameToId, nameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      if(idToGame.contains(databaseId.first)) {
        GameEntry game;
        QJsonObject jsonGame = idToGame.value(databaseId.first);
        game.id = databaseId.first;
        game.title = databaseId.second;
        game.platform = jsonGame["titleDetail"].toObject()["platforms"].toString();
        game.miscData = QJsonDocument(jsonGame).toJson(QJsonDocument::Compact);
        gameEntries.append(game);
      } else {
        printf("ERROR: Internal error: Database Id '%s' not found in the in-memory database.\n",
               databaseId.first.toStdString().c_str());
      }
    }
  }
  // qDebug() << "Detected:" << gameEntries;
}

void VGFacts::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  jsonObj = QJsonDocument::fromJson(game.miscData).object();

  fetchGameResources(game, sharedBlobs, cache);
}

void VGFacts::getReleaseDate(GameEntry &game)
{
  game.releaseDate = jsonObj["titleDetail"].toObject()["releaseDate"].toString();
  if(!game.releaseDate.contains("TBA")) {
    game.releaseDate = StrTools::conformReleaseDate(game.releaseDate);
  } else {
    game.releaseDate = "";
  }
}

void VGFacts::getTags(GameEntry &game)
{
  game.tags = jsonObj["titleDetail"].toObject()["genres"].toString().replace(";", ", ");
  game.tags = StrTools::conformTags(game.tags);
}

void VGFacts::getDeveloper(GameEntry &game)
{
  game.developer = jsonObj["titleDetail"].toObject()["developers"].toString().split(";").first();
}

void VGFacts::getTrivia(GameEntry &game)
{
  QJsonArray trivias = jsonObj["trivia"].toArray();
  for(const auto &trivia: std::as_const(trivias)) {
    game.trivia.append("\n\n" + trivia.toString().trimmed());
  }
}

void VGFacts::getFranchises(GameEntry &game)
{
  game.franchises = jsonObj["titleDetail"].toObject()["franchises"].toString().replace(";", ", ");
  if(game.franchises.isEmpty()) {
    game.franchises = jsonObj["titleDetail"].toObject()["collections"].toString().replace(";", ", ");
  }
  if(!game.franchises.isEmpty()) {
    game.franchises = StrTools::conformTags(game.franchises);
  }
}

void VGFacts::getCover(GameEntry &game)
{
  QString imageUrl = jsonObj["titleDetail"].toObject()["cover"].toString();
  if(!imageUrl.isEmpty()) {
    if(config->verbosity > 1) {
      qDebug() << "Cover:";
    }
    limiter.exec();
    netComm->request(baseUrl + imageUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
    }
  }
}

void VGFacts::getMarquee(GameEntry &game)
{
  QString imageUrl = jsonObj["titleDetail"].toObject()["artwork"].toString();
  if(!imageUrl.isEmpty()) {
    if(config->verbosity > 1) {
      qDebug() << "Marquee:";
    }
    limiter.exec();
    netComm->request(baseUrl + imageUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}
