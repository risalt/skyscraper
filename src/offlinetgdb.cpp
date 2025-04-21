/***************************************************************************
 *            offlinetgdb.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
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

#include <QSql>
#include <QSqlQuery>
#include <QSqlError>
#include <QMapIterator>
#include <QString>
#include <QStringListIterator>
#include <QTextDocumentFragment>

#include "offlinetgdb.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

OfflineTGDB::OfflineTGDB(Settings *config,
                         QSharedPointer<NetManager> manager,
                         QString threadId,
                         NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(500);
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://cdn.thegamesdb.net/images/original/";
  QString databaseUrl = "http://cdn.thegamesdb.net/tgdb_dump.zip";

  db = QSqlDatabase::addDatabase("QMYSQL", "tgdb" + threadId);
  db.setHostName(config->dbServer);
  db.setDatabaseName(config->dbName);
  db.setUserName(config->dbUser);
  db.setPassword(config->dbPassword);
  if(!db.open()) {
    printf("ERROR: Could not open the TGDB database.\n");
    printf("Please create it if necessary downloading the latest dump from '%s' and "
           "importing it into the MySQL database indicated in the configuration file.\n",
           databaseUrl.toStdString().c_str());
    qDebug() << db.lastError();
    reqRemaining = 0;
    return;
  }

  loadMaps();
  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by TheGamesDB or it hasn't "
           "yet been included in Skyscraper for this module...\033[0m\n");
    return;
  }

  QSqlQuery query(db);
  // Main name
  query.setForwardOnly(true);
  query.prepare("SELECT id, game_title, region_id FROM games"
                "  WHERE platform IN (" + platformId + ")");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games'.\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    gamesIds << query.value(0).toInt();
    QStringList safeVariations, unsafeVariations;
    QString decoded = QTextDocumentFragment::fromHtml(query.value(1).toString())
                      .toPlainText();
    NameTools::generateSearchNames(decoded, safeVariations, unsafeVariations, true);
    for(const auto &name: std::as_const(safeVariations)) {
      if(!searchNameToId.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding full variation: %s -> %s\n",
                 query.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        searchNameToId.insert(name,
                              qMakePair(query.value(0).toInt(),
                                        qMakePair(decoded, query.value(2).toInt())));
      }
    }
    for(const auto &name: std::as_const(unsafeVariations)) {
      if(!searchNameToIdTitle.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding title variation: %s -> %s\n",
                 query.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        searchNameToIdTitle.insert(name,
                                   qMakePair(query.value(0).toInt(),
                                             qMakePair(decoded, query.value(2).toInt())));
      }
    }
  }
  query.finish();

  // Alternative names:
  query.setForwardOnly(true);
  query.prepare("SELECT games_alts.games_id, games_alts.name, games.region_id"
                " FROM games_alts INNER JOIN games ON games_alts.games_id = games.id"
                " WHERE games.platform IN (" + platformId + ")");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games' (idx).\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    gamesIds << query.value(0).toInt();
    QStringList safeVariations, unsafeVariations;
    QString decoded = QTextDocumentFragment::fromHtml(query.value(1).toString())
                      .toPlainText();
    NameTools::generateSearchNames(decoded, safeVariations, unsafeVariations, true);
    for(const auto &name: std::as_const(safeVariations)) {
      if(!searchNameToId.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding full variation: %s -> %s\n",
                 query.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        searchNameToId.insert(name,
                              qMakePair(query.value(0).toInt(),
                                        qMakePair(decoded, query.value(2).toInt())));
      }
    }
    for(const auto &name: std::as_const(unsafeVariations)) {
      if(!searchNameToIdTitle.contains(name)) {
        if(config->verbosity > 2) {
          printf("Adding title variation: %s -> %s\n",
                 query.value(1).toString().toStdString().c_str(),
                 name.toStdString().c_str());
        }
        searchNameToIdTitle.insert(name,
                                   qMakePair(query.value(0).toInt(),
                                             qMakePair(decoded, query.value(2).toInt())));
      }
    }
  }
  query.finish();

  if(gamesIds.isEmpty()) {
    printf("WARNING: The TGDB database contains no records for this platform.\n");
    reqRemaining = 0;
    return;
  } else {
    printf("INFO: TGDB database has been parsed, %d games have been identified.\n",
           gamesIds.size());
  }

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
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

OfflineTGDB::~OfflineTGDB()
{
  if(db.isOpen()) {
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("tgdb" + threadId);
  }
}

void OfflineTGDB::getSearchResults(QList<GameEntry> &gameEntries,
                                  QString searchName, QString)
{
  QList<GameEntry> gameEntriesRegion;
  QList<QPair<int, QPair<QString, int>>> matches;
  similarIds = QStringList();

  if(getSearchResultsOffline(matches, searchName, searchNameToId, searchNameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      if(gamesIds.contains(databaseId.first)) {
        GameEntry game;
        game.id = QString::number(databaseId.first);
        game.title = databaseId.second.first;
        game.platform = Platform::get().getAliases(config->platform).at(1);
        game.url = regionMap[databaseId.second.second];
        gameEntriesRegion.append(game);
        similarIds << QString::number(databaseId.first);
      }
      else {
        printf("ERROR: Internal error: Database Id '%d' not found in the memory database.\n",
               databaseId.first);
      }
    }
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
        GameEntry gameCopy = game;
        gameCopy.url = similarIds.join(',');
        gameEntries.append(gameCopy);
      }
    }
  }
}

void OfflineTGDB::getGameData(GameEntry &game, QStringList &sharedBlobs,
                              GameEntry *cache = nullptr)
{
  similarIds = game.url.split(',');

  QSqlQuery query(db);
  query.setForwardOnly(true);
  query.prepare("SELECT * FROM games WHERE id=" + game.id);
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games' (cnt).\n");
    reqRemaining = 0;
    return;
  }
  if(query.next()) {
    queryData = query.record();
    if(config->verbosity > 1) {
      qDebug() << queryData;
    }
  } else {
    queryData = QSqlRecord();
    printf("ERROR: Internal error: Database Id '%d' not found in the MySQL database.\n",
           game.id.toInt());
  }
  
  fetchGameResources(game, sharedBlobs, cache);
}

void OfflineTGDB::getReleaseDate(GameEntry &game)
{
  if(!queryData.value("release_date").toString().isEmpty()) {
    game.releaseDate = StrTools::conformReleaseDate(queryData.value("release_date")
                       .toString());
  }
}

void OfflineTGDB::getDeveloper(GameEntry &game)
{
  QSqlQuery query(db);

  query.setForwardOnly(true);
  query.prepare("SELECT dev_id FROM games_devs WHERE games_id=" + game.id);
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games_devs'.\n");
    reqRemaining = 0;
    return;
  }
  if(query.next()) {
    game.developer = developerMap[query.value(0).toInt()];
  }
  query.finish();
}

void OfflineTGDB::getPublisher(GameEntry &game)
{
  QSqlQuery query(db);

  query.setForwardOnly(true);
  query.prepare("SELECT pub_id FROM games_pubs WHERE games_id=" + game.id);
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games_pubs'.\n");
    reqRemaining = 0;
    return;
  }
  if(query.next()) {
    game.publisher = publisherMap[query.value(0).toInt()];
  }
  query.finish();
}

void OfflineTGDB::getDescription(GameEntry &game)
{
  game.description = queryData.value("overview").toString();
}

void OfflineTGDB::getPlayers(GameEntry &game)
{
  int players = queryData.value("players").toInt();
  if(players != 0) {
    game.players = QString::number(players);
  }
}

void OfflineTGDB::getAges(GameEntry &game)
{
  if(!queryData.value("rating").toString().isEmpty()) {
    game.ages = StrTools::conformAges(queryData.value("rating").toString());
  }
}

void OfflineTGDB::getTags(GameEntry &game)
{
  QSqlQuery query(db);

  query.setForwardOnly(true);
  query.prepare("SELECT genres_id FROM games_genre WHERE games_id=" + game.id);
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games_genre'.\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    game.tags.append(genreMap[query.value(0).toInt()] + ", ");
  }
  query.finish();
  if(!game.tags.isEmpty()) {
    game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
  }
}

void OfflineTGDB::getCover(GameEntry &game)
{
  QSqlQuery query(db);

  query.prepare("SELECT games_id, filename FROM banners WHERE"
                " type='boxart' AND side='front' AND games_id IN (" + game.url + ")"
                " ORDER BY filename ASC");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'banners' (co).\n");
    reqRemaining = 0;
    return;
  }
  query.next();
  for(const auto &id: std::as_const(similarIds)) {
    bool found = false;
    while(query.isValid() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = baseUrl + query.value(1).toString();
        limiter.exec();
        netComm->request(url);
        q.exec();
        QImage image;
        if(netComm->getError() == QNetworkReply::NoError &&
           image.loadFromData(netComm->getData())) {
          game.coverData = netComm->getData();
          found = true;
        }
        break;
      }
      query.next();
    }
    if(found) {
      break;
    }
    query.first();
  }
  query.finish();
}

void OfflineTGDB::getScreenshot(GameEntry &game)
{
  QSqlQuery query(db);

  query.prepare("SELECT games_id, filename FROM banners WHERE"
                " type='screenshot' AND games_id IN (" + game.url + ")"
                " ORDER BY filename ASC");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'banners' (sc).\n");
    reqRemaining = 0;
    return;
  }
  query.next();
  for(const auto &id: std::as_const(similarIds)) {
    bool found = false;
    while(query.isValid() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = baseUrl + query.value(1).toString();
        limiter.exec();
        netComm->request(url);
        q.exec();
        QImage image;
        if(netComm->getError() == QNetworkReply::NoError &&
           image.loadFromData(netComm->getData())) {
          game.screenshotData = netComm->getData();
          found = true;
        }
        break;
      }
      query.next();
    }
    if(found) {
      break;
    }
    query.first();
  }
  query.finish();
}

void OfflineTGDB::getWheel(GameEntry &game)
{
  QSqlQuery query(db);

  query.prepare("SELECT games_id, filename FROM banners WHERE"
                " type='titlescreen' AND games_id IN (" + game.url + ")"
                " ORDER BY filename ASC");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'banners' (wh).\n");
    reqRemaining = 0;
    return;
  }
  query.first();
  for(const auto &id: std::as_const(similarIds)) {
    bool found = false;
    while(query.isValid() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = baseUrl + query.value(1).toString();
        limiter.exec();
        netComm->request(url);
        q.exec();
        QImage image;
        if(netComm->getError() == QNetworkReply::NoError &&
           image.loadFromData(netComm->getData())) {
          game.wheelData = netComm->getData();
          found = true;
        }
        break;
      }
      query.next();
    }
    if(found) {
      break;
    }
    query.first();
  }
  query.finish();
}

void OfflineTGDB::getMarquee(GameEntry &game)
{
  QSqlQuery query(db);

  query.prepare("SELECT games_id, filename FROM banners WHERE"
                " type='fanart' AND games_id IN (" + game.url + ")"
                " ORDER BY filename ASC");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'banners' (ma).\n");
    reqRemaining = 0;
    return;
  }
  bool found = false;
  query.next();
  for(const auto &id: std::as_const(similarIds)) {
    while(query.isValid() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = baseUrl + query.value(1).toString();
        limiter.exec();
        netComm->request(url);
        q.exec();
        QImage image;
        if(netComm->getError() == QNetworkReply::NoError &&
           image.loadFromData(netComm->getData())) {
          game.marqueeData = netComm->getData();
          found = true;
        }
        break;
      }
      query.next();
    }
    if(found) {
      break;
    }
    query.first();
  }
  query.finish();

  if(!found) {
    query.prepare("SELECT games_id, filename FROM banners WHERE"
                  " type='banner' AND subkey!='graphical' AND"
                  " games_id IN (" + game.url + ")"
                  " ORDER BY filename ASC");
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing data from table 'banners' (ma).\n");
      reqRemaining = 0;
      return;
    }
    query.next();
    for(const auto &id: std::as_const(similarIds)) {
      bool found = false;
      while(query.isValid() && !found) {
        if(query.value(0).toInt() == id.toInt()) {
          QString url = baseUrl + query.value(1).toString();
          limiter.exec();
          netComm->request(url);
          q.exec();
          QImage image;
          if(netComm->getError() == QNetworkReply::NoError &&
             image.loadFromData(netComm->getData())) {
            game.marqueeData = netComm->getData();
            found = true;
          }
          break;
        }
        query.next();
      }
      if(found) {
        break;
      }
      query.first();
    }
    query.finish();
  }
}

void OfflineTGDB::getTexture(GameEntry &game)
{
  QSqlQuery query(db);

  query.prepare("SELECT games_id, filename FROM banners WHERE"
                " type='boxart' AND side='back' AND games_id IN (" + game.url + ")"
                " ORDER BY filename ASC");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'banners' (te).\n");
    reqRemaining = 0;
    return;
  }
  query.next();
  for(const auto &id: std::as_const(similarIds)) {
    bool found = false;
    while(query.isValid() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = baseUrl + query.value(1).toString();
        limiter.exec();
        netComm->request(url);
        q.exec();
        QImage image;
        if(netComm->getError() == QNetworkReply::NoError &&
           image.loadFromData(netComm->getData())) {
          game.textureData = netComm->getData();
          found = true;
        }
        break;
      }
      query.next();
    }
    if(found) {
      break;
    }
    query.first();
  }
  query.finish();
}

void OfflineTGDB::getVideo(GameEntry &game)
{
  QString baseYt = "https://www.youtube.com/watch?v=";
  QSqlQuery query(db);

  query.prepare("SELECT id, youtube FROM games WHERE id IN (" + game.url + ")");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'games' (yt).\n");
    reqRemaining = 0;
    return;
  }
  for(const auto &id: std::as_const(similarIds)) {
    bool found = false;
    while(query.next() && !found) {
      if(query.value(0).toInt() == id.toInt()) {
        QString url = query.value(1).toString();
        if(!url.isEmpty()) {
          if(!url.startsWith("http")) {
            url = baseYt + url;
          }
          getOnlineVideo(url, game);
          if(!game.videoFormat.isEmpty()) {
            found = true;
          }
        }
        break;
      }
    }
    if(found) {
      break;
    } else {
      query.first();
    }
  }
  query.finish();
}

void OfflineTGDB::loadMaps()
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

  QSqlQuery query(db);

  query.setForwardOnly(true);
  query.prepare("SELECT id, genre FROM genres");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'genres'.\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    genreMap[query.value(0).toInt()] = query.value(1).toString();
  }
  query.finish();

  query.setForwardOnly(true);
  query.prepare("SELECT id, name FROM devs_list");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'devs_list'.\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    developerMap[query.value(0).toInt()] = query.value(1).toString();
  }
  query.finish();

  query.setForwardOnly(true);
  query.prepare("SELECT id, name FROM pubs_list");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table 'pubs_list'.\n");
    reqRemaining = 0;
    return;
  }
  while(query.next()) {
    publisherMap[query.value(0).toInt()] = query.value(1).toString();
  }
  query.finish();
}
