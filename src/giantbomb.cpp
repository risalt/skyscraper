/***************************************************************************
 *            giantbomb.cpp
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

#include <unistd.h>

#include <QList>
#include <QThread>
#include <QProcess>
#include <QJsonArray>
#include <QTextStream>
#include <QStringList>
#include <QListIterator>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTextDocumentFragment>

#include "giantbomb.h"
#include "platform.h"
#include "strtools.h"
#include "nametools.h"
#include "skyscraper.h"

constexpr int RETRIESMAX = 4;
constexpr int APICOOLDOWN = 21;

GiantBomb::GiantBomb(Settings *config,
           QSharedPointer<NetManager> manager,
           QString threadId)
  : AbstractScraper(config, manager), threadId(threadId)
{
  incrementalScraping = false;
  loadConfig("giantbomb.json");
  
  QPair<QString, QString> clientIdHeader;
  clientIdHeader.first = "Client-ID";
  clientIdHeader.second = config->user;

  QPair<QString, QString> tokenHeader;
  tokenHeader.first = "Authorization";
  tokenHeader.second = "Bearer " + config->password;

  headers.append(clientIdHeader);
  headers.append(tokenHeader);
  
  lastGameRequest = QDateTime::currentDateTimeUtc().addDays(-1);
  lastImageRequest = QDateTime::currentDateTimeUtc().addDays(-1);
  lastReleaseRequest = QDateTime::currentDateTimeUtc().addDays(-1);

  int totalEntries = 0;
  QString fileNameGames = config->giantBombDb + "/" + config->platform + "_games.json";
  QString fileNameReleases = config->giantBombDb + "/" + config->platform + "_releases.json";
  QString fileNameVideos = config->giantBombDb + "/" + "videos.json";
  QFile fileGames(fileNameGames);
  QFile fileReleases(fileNameReleases);
  QFile fileVideos(fileNameVideos);
  QString platformDb;
  // QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

  baseUrl = "https://www.giantbomb.com/api";
  urlPost = "api_key=" + config->apiKey + "&format=json";

  platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by GiantBomb or it hasn't yet been included in Skyscraper for this module...\033[0m\n");
    return;
  }

  QThread::sleep(1);
  // Download the full games and releases headers from the webservice:
  if(config->cacheGb) {
    if(threadId == "1") {
      QString games("games");
      refreshGbCache(games, &fileGames, &lastGameRequest);
    } else if (threadId == "2") {
      QString releases("releases");
      refreshGbCache(releases, &fileReleases, &lastReleaseRequest);
    } else if (threadId == "3" && !fileVideos.exists()) {
      QString videos("videos");
      refreshGbCache(videos, &fileVideos, &lastVideoRequest);
    }
    return;
  }
  
  // Read GAMES:
  // Read from config->giantBombDb + config->platform + "_games.xml"
  if(!fileGames.open(QIODevice::ReadOnly)){
    fileGames.close();
    printf("\nERROR: Database file %s cannot be accessed. ", fileNameGames.toStdString().c_str());
    printf("Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  }
  printf("INFO: Reading GiantBomb header game database...");
  fflush(stdout);
  platformDb = QString::fromUtf8(fileGames.readAll());
  fileGames.close();

  jsonDoc = QJsonDocument::fromJson(platformDb.toUtf8());
  if (jsonDoc.isNull()) {
    printf("\nERROR: The GiantBomb database file for the platform is missing, empty or corrupted. "
           "Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  } else {
    QJsonArray jsonGames = jsonDoc.array();
    while(!jsonGames.isEmpty()) {
      QJsonObject jsonGame = jsonGames.first().toObject();
      QString gameName = jsonGame["name"].toString();
      QStringList gameAliases = jsonGame["aliases"].toString().split(QRegularExpression("\\v+"), Qt::SkipEmptyParts);
      QString apiUrl = jsonGame["api_detail_url"].toString();
      if(!gameName.isEmpty()) {
        gameAliases.append(gameName);
      } else if(!gameAliases.isEmpty()) {
        gameName = gameAliases.first();
      }
      if(!gameAliases.isEmpty() && !apiUrl.isEmpty()) {
        totalEntries++;
        for(auto &gameAlias: gameAliases) {
          QString sanitizedGame = gameAlias.replace("&", " and ").remove("'");
          sanitizedGame = NameTools::convertToIntegerNumeral(sanitizedGame);
          QString sanitizedGameMain = sanitizedGame;
          sanitizedGame = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGame), -1, " ");
          // TODO: Danger false positives: sanitizedGameMain = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGameMain), -1, " ", true);
          sanitizedGame = StrTools::sanitizeName(sanitizedGame, true);
          sanitizedGameMain = StrTools::sanitizeName(sanitizedGameMain, true);
          if (!gbPlatformMap.contains(sanitizedGame)) {
            gbPlatformMap[sanitizedGame] = qMakePair(gameName, apiUrl + "?" + urlPost);
            if(config->verbosity >= 3) {
              printf("B1: %s\n", sanitizedGame.toStdString().c_str());
            }
          }
          if (!gbPlatformMap.contains(sanitizedGameMain)) {
            gbPlatformMap[sanitizedGameMain] = qMakePair(gameName, apiUrl + "?" + urlPost);
            if(config->verbosity >= 3) {
              printf("B2: %s\n", sanitizedGameMain.toStdString().c_str());
            }
          }
        }
      }
      jsonGames.removeFirst();
    }
    printf(" DONE.\nINFO: Read %d game header entries from GiantBomb local database.\n", totalEntries);
  }

  // Read RELEASES:
  // Read from config->giantBombDb + config->platform + "_releases.xml"
  printf("INFO: Reading GiantBomb header release database...");
  if(!fileReleases.open(QIODevice::ReadOnly)){
    fileReleases.close();
    printf("\nERROR: Database file %s cannot be accessed. ", fileNameReleases.toStdString().c_str());
    printf("Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  }
  platformDb = QString::fromUtf8(fileReleases.readAll());
  fileReleases.close();
  fflush(stdout);

  totalEntries = 0;  
  jsonRel = QJsonDocument::fromJson(platformDb.toUtf8());
  if (jsonRel.isNull()) {
    printf("\nERROR: The GiantBomb database file for the platform is missing, empty or corrupted. "
           "Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  } else {
    QJsonArray jsonReleases = jsonRel.array();
    while(!jsonReleases.isEmpty()) {
      QJsonObject jsonRelease = jsonReleases.first().toObject();
      QString gameName = jsonRelease["name"].toString();
      QString apiUrl = jsonRelease["game"].toObject()["api_detail_url"].toString();
      if(!gameName.isEmpty() && !apiUrl.isEmpty()) {
        totalEntries++;
        QString sanitizedGame = gameName.replace("&", " and ").remove("'");
        sanitizedGame = NameTools::convertToIntegerNumeral(sanitizedGame);
        QString sanitizedGameMain = sanitizedGame;
        sanitizedGame = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGame), -1, " ");
        // TODO: Danger false positives: sanitizedGameMain = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedGameMain), -1, " ", true);
        sanitizedGame = StrTools::sanitizeName(sanitizedGame, true);
        sanitizedGameMain = StrTools::sanitizeName(sanitizedGameMain, true);
        if (!gbPlatformMap.contains(sanitizedGame)) {
          gbPlatformMap[sanitizedGame] = qMakePair(gameName, apiUrl + "?" + urlPost);
          if(config->verbosity >= 3) {
            printf("C1: %s\n", sanitizedGame.toStdString().c_str());
          }
        }
        if (!gbPlatformMap.contains(sanitizedGameMain)) {
          gbPlatformMap[sanitizedGameMain] = qMakePair(gameName, apiUrl + "?" + urlPost);
          if(config->verbosity >= 3) {
            printf("C2: %s\n", sanitizedGameMain.toStdString().c_str());
          }
        }
      }
      jsonReleases.removeFirst();
    }
    printf(" DONE.\nINFO: Read %d release header entries from GiantBomb local database.\n", totalEntries);
  }

  // Read VIDEOS:
  // Read from config->giantBombDb + "_videos.xml"
  if(!fileVideos.open(QIODevice::ReadOnly)){
    fileVideos.close();
    printf("\nERROR: Database file %s cannot be accessed. ", fileNameVideos.toStdString().c_str());
    printf("Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  }
  platformDb = QString::fromUtf8(fileVideos.readAll());
  fileVideos.close();
  printf("INFO: Reading GiantBomb header videos database...");
  fflush(stdout);

  jsonVid = QJsonDocument::fromJson(platformDb.toUtf8());
  if (jsonVid.isNull()) {
    printf("\nERROR: The GiantBomb database file for the platform is missing, empty or corrupted. "
           "Please regenerate it re-executing this command adding the option '--cachegb'.\n");
    Skyscraper::removeLockAndExit(1);
  } else {
    printf(" DONE.\n");
  }

  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(TAGS);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(AGES);
  fetchOrder.append(SCREENSHOT);
  // SCREENSHOT -> images->image->original+image_tags (*Screenshot*, >01)
  fetchOrder.append(MARQUEE);
  // MARQUEE -> images->image->original (tags = *Media*)
  fetchOrder.append(COVER);
  // COVER -> images->image->original (tags = *Box Art*)
  fetchOrder.append(WHEEL);
  // WHEEL -> images->image->original+image_tags (*Screenshot*, 01)
  fetchOrder.append(VIDEO);
  // VIDEO -> videos->video->api_detail_url -> results->low_url (1800->800?)
}

bool GiantBomb::requestGb (const QString &url, QDateTime *lastRequest)
{
  int errors = 0;
  while(errors < RETRIESMAX) {
    int seconds = std::max(0, static_cast<int>(lastRequest->secsTo(QDateTime::currentDateTime())));
    if (seconds < APICOOLDOWN) {
      QThread::sleep(APICOOLDOWN - seconds);
    }
    netComm->request(url);
    q.exec();
    data = netComm->getData();
    *lastRequest = QDateTime::currentDateTime();
    // Debug:
    /*QTextStream out(stdout);
    out << url << Qt::endl << data << Qt::endl;*/

    jsonDoc = QJsonDocument::fromJson(data);
    if(jsonDoc.isEmpty()) {
      if(errors < RETRIESMAX) {
        errors++;
      }
      continue;
    }
    errors = 0;

    int queryStatus = jsonDoc.object()["status_code"].toInt();
    if(queryStatus == 1) {
      // No error
      return true;
    } else if(queryStatus == 107) {
      // API rate exceeded
      printf("\n\033[1;31mWARNING: GiantBomb API rate limit hit: Waiting for up to an hour before retrying...\033[0m\n");
      QProcess::execute("bash", { QDir::homePath() + "/.skyscraper/wait_gb.sh" });
      QThread::sleep(60);
      return requestGb(url, lastRequest);
    } else if(queryStatus == 101) {
      // Does not exist
      return false;
    } else {
      // Other errors
      printf("\033[1;31mERROR: The GiantBomb query failed with error %d: %s\033[0m\n",
             queryStatus, jsonDoc.object()["error"].toString().toStdString().c_str());
      return false;
    }
  }
  return false;
}

bool GiantBomb::refreshGbCache(QString &endpoint, QFile *file, QDateTime *lastRequest)
{
  printf("INFO: Downloading game %s from GiantBomb for the platform...\n", endpoint.toStdString().c_str());
  QString platformDb = "[\n";
  int totalEntries = 0;
  bool pendingResults = true;

  while (pendingResults) {
    QString url;
    if(endpoint == "videos") {
      url = baseUrl + "/" + endpoint + "/" + "?" + urlPost + "&filter=video_categories:7&offset=" + QString::number(totalEntries);
    } else {
      url = baseUrl + "/" + endpoint + "/" + "?" + urlPost + "&platforms=" + platformId + "&offset=" + QString::number(totalEntries);
    }
    
    if(requestGb(url, lastRequest)) {
      printf(".");
      QJsonArray jsonGames = jsonDoc.object()["results"].toArray();
      if (jsonGames.size()) {
        while(!jsonGames.isEmpty()) {
          totalEntries++;
          QJsonObject jsonGame = jsonGames.first().toObject();
          platformDb.append(QJsonDocument(jsonGame).toJson(QJsonDocument::Indented));
          platformDb.append(",\n");
          jsonGames.removeFirst();
        }
      } else {
        pendingResults = false;
      }
    } else {
      break;
    }
  }
  if (totalEntries) {
    platformDb.chop(2);
  }
  platformDb.append("]");
  reqRemaining = 0;

  printf("INFO: Writing %d %s header entries from GiantBomb database to local database.\n", totalEntries, endpoint.toStdString().c_str());
  if(!file->open(QIODevice::WriteOnly)){
    file->close();
    printf("\nERROR: Database file %s cannot be created or written to.\n", file->fileName().toStdString().c_str());
    return false; // Skyscraper::removeLockAndExit(1);
  } else {
    file->write(platformDb.toUtf8());
    file->close();
  }
  return true;
}

void GiantBomb::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  if(config->cacheGb) {
    return;
  }

  if(config->verbosity >= 2) {
    qDebug() << "SearchName: " << searchName;
  }  

  GameEntry game;
  
  QPair <QString, QString> gameDetail = gbPlatformMap.value(searchName);
  if(!gameDetail.second.isEmpty()) {
    game.title = gameDetail.first;
    game.url = gameDetail.second;
    game.platform = Platform::get().getAliases(config->platform).at(1);
    gameEntries.append(game);
  } else {
    if(config->fuzzySearch && searchName.size() >= 6) {
      int maxDistance = config->fuzzySearch;
      if((searchName.size() <= 10) || (config->fuzzySearch < 0)) {
        maxDistance = 1;
      }
      QListIterator<QString> keysIterator(gbPlatformMap.keys());
      while (keysIterator.hasNext()) {
        QString name = keysIterator.next();
        if(StrTools::onlyNumbers(name) == StrTools::onlyNumbers(searchName)) {
          int distance = StrTools::distanceBetweenStrings(searchName, name);
          if(distance <= maxDistance) {
            printf("FuzzySearch: Found %s = %s (distance %d)!\n",
                   searchName.toStdString().c_str(),
                   name.toStdString().c_str(), distance);
            gameDetail = gbPlatformMap.value(name);
            if(!gameDetail.second.isEmpty()) {
              game.title = gameDetail.first;
              game.url = gameDetail.second;
              game.platform = Platform::get().getAliases(config->platform).at(1);
              gameEntries.append(game);
            }
          }
        }
      }
    }
  }
  if(config->verbosity >= 2) {
    qDebug() << gameEntries;
  }
}

void GiantBomb::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }
  printf("Waiting to get game data...");
  fflush(stdout);
  if(requestGb(game.url, &lastGameRequest)) {
    printf(" DONE.\n");
    jsonObj = jsonDoc.object()["results"].toObject();
  } else {
    printf(" ERROR, skipping game.\n");
    game.title = "";
    game.platform = "";
    game.url = "";
    return;
  }

  // Now retrieve specific trailers of the game for the platform (offline reduced database):
  QList<QJsonObject> matchingVideos;
  QString gameGbId = jsonObj["guid"].toString();
  // qDebug() << gameGbId;
  if(gameGbId.isEmpty()) {
    printf("ERROR: Game %s does not have an associated Id.\n", game.title.toStdString().c_str());
  } else {
    QJsonArray jsonVideos = jsonVid.array();
    while(!jsonVideos.isEmpty()) {
      jsonObjVid = jsonVideos.first().toObject();
      for(const auto &association: jsonObjVid["associations"].toArray()) {
        if (association.toObject()["guid"].toString() == gameGbId) {
          if (jsonObjVid["length_seconds"].toInt(1000) < 180) {
            matchingVideos.append(jsonObjVid);
          }
        }
      }
      jsonVideos.removeFirst();
    }
  }
  if(matchingVideos.size() == 0) {
    jsonObjVid = QJsonObject();
  } else if (matchingVideos.size() == 1) {
    jsonObjVid = matchingVideos.first();
  } else {
    // Filter by apropriate length:
    jsonObjVid = matchingVideos.first();
    for(const auto &video: matchingVideos) {
      int videoLength = video["length_seconds"].toInt(0);
      if (videoLength > 25 && videoLength < 100) {
        jsonObjVid = video;
        break;
      }
    }
  }

  // Now retrieve specific releases of the game for the platform (offline reduced database):
  // https://www.giantbomb.com/api/releases/?api_key=xxx&filter=game:yyy&platforms=zzz&sort=release_date:asc
  QList<QJsonObject> matchingReleases;
  gameGbId = QString::number(jsonObj["id"].toInt());
  // qDebug() << gameGbId;
  if(gameGbId.isEmpty()) {
    printf("ERROR: Game %s does not have an associated Id.\n", game.title.toStdString().c_str());
  } else {
    QJsonArray jsonRels = jsonRel.array();
    while(!jsonRels.isEmpty()) {
      jsonObjRel = jsonRels.first().toObject();
      if(gameGbId == QString::number(jsonObjRel["game"].toObject()["id"].toInt())) {
        matchingReleases.append(jsonObjRel);
      }
      jsonRels.removeFirst();
    }
  }
  // qDebug() << matchingReleases.size();
  if(matchingReleases.size() == 0) {
    jsonObjRel = QJsonObject();
  } else if(matchingReleases.size() == 1) {
    jsonObjRel = matchingReleases.first();
  } else {
    // Prioritize by region
    bool foundRel = false;
    QStringList europe = {"eu", "wor", "fr", "de", "it", "sp", "se", "nl", "dk"};
    QStringList americas = {"us", "br", "ca"};
    QStringList australia = {"au"};
    QStringList asia = {"jp", "kr", "tw", "cn", "asi"};
    for(const auto &region: regionPrios) {
      for(const auto &relRegion: matchingReleases) {
        QString jsonRegion = relRegion["region"].toObject()["name"].toString();
        if(jsonRegion == "United States") {
          if(americas.contains(region)) {
            jsonObjRel = relRegion;
            foundRel = true;
          }
        } else if (jsonRegion == "United Kingdom") {
          if(europe.contains(region)) {
            jsonObjRel = relRegion;
            foundRel = true;
          }
        } else if (jsonRegion == "Japan") {
          if(asia.contains(region)) {
            jsonObjRel = relRegion;
            foundRel = true;
          }
        } else if (jsonRegion == "Australia") {
          if(australia.contains(region)) {
            jsonObjRel = relRegion;
            foundRel = true;
          }
        }
        if(foundRel) {
          break;
        }
      }
      if(foundRel) {
        break;
      }
    }
    if(!foundRel) {
      jsonObjRel = matchingReleases.first();
    }
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
    case AGES:
      getAges(game);
      break;
    case RATING:
      getRating(game);
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
      if(config->cacheCovers && (!sharedBlobs.contains("cover"))) {
        if ((!cache) || (cache && cache->coverData.isNull())) {
          getCover(game);
        }
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots && (!sharedBlobs.contains("screenshot"))) {
        if ((!cache) || (cache && cache->screenshotData.isNull())) {
          getScreenshot(game);
        }
      }
      break;
    case WHEEL:
      if(config->cacheWheels && (!sharedBlobs.contains("wheel"))) {
        if ((!cache) || (cache && cache->wheelData.isNull())) {
          getWheel(game);
        }
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees && (!sharedBlobs.contains("marquee"))) {
        if ((!cache) || (cache && cache->marqueeData.isNull())) {
          getMarquee(game);
        }
      }
      break;
    case TEXTURE:
      if (config->cacheTextures && (!sharedBlobs.contains("texture"))) {
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
  // qDebug() << game;
}

void GiantBomb::getReleaseDate(GameEntry &game)
{
  QString basicDate = jsonObj["original_release_date"].toString();
  QJsonValue jsonValue = jsonObj["original_release_date"];
  if(jsonValue != QJsonValue::Undefined && !jsonValue.toString().isEmpty()) {
    game.releaseDate = jsonValue.toString().left(10);
  }
  if(game.releaseDate.isEmpty()) {
    jsonValue = jsonObj["expected_release_year"];
    if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
      game.releaseDate = QString::number(jsonValue.toInt());
      jsonValue = jsonObj["expected_release_quarter"];
      if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
        game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt() * 3) + "-01";
      } else {
        jsonValue = jsonObj["expected_release_month"];
        if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
          game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt());
          jsonValue = jsonObj["expected_release_day"];
          if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
            game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt());
          } else {
            game.releaseDate = game.releaseDate + "-01";
          }
        } else {
          game.releaseDate = game.releaseDate + "-01-01";
        }
      }
    }
  }
  if(game.releaseDate.isEmpty()) {
    basicDate = jsonObjRel["release_date"].toString();
    jsonValue = jsonObjRel["release_date"];
    if(jsonValue != QJsonValue::Undefined && !jsonValue.toString().isEmpty()) {
      game.releaseDate = jsonValue.toString().left(10);
    }
    if(game.releaseDate.isEmpty()) {
      jsonValue = jsonObjRel["expected_release_year"];
      if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
        game.releaseDate = QString::number(jsonValue.toInt());
        jsonValue = jsonObjRel["expected_release_quarter"];
        if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
          game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt() * 3) + "-01";
        } else {
          jsonValue = jsonObjRel["expected_release_month"];
          if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
            game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt());
            jsonValue = jsonObjRel["expected_release_day"];
            if(jsonValue != QJsonValue::Undefined && jsonValue.toInt(0)) {
              game.releaseDate = game.releaseDate + "-" + QString::number(jsonValue.toInt());
            } else {
              game.releaseDate = game.releaseDate + "-01";
            }
          } else {
            game.releaseDate = game.releaseDate + "-01-01";
          }
        }
      }
    }
  }
}

void GiantBomb::getPlayers(GameEntry &game)
{
  // Use release endpoint to retrieve player information
  // releases->release->maximum_players
  if(jsonObjRel["maximum_players"].isUndefined() || jsonObjRel["maximum_players"].isNull()) {
    if(!jsonObjRel["minimum_players"].isUndefined() && !jsonObjRel["minimum_players"].isNull()) {
      game.players = QString::number(jsonObjRel["minimum_players"].toInt(0));
    }
  } else {
    game.players = QString::number(jsonObjRel["maximum_players"].toInt(0));
  }
}

void GiantBomb::getTags(GameEntry &game)
{
  QJsonArray jsonGenres = jsonObj["genres"].toArray();
  for(const auto &jsonGenre: jsonGenres) {
    game.tags.append(jsonGenre.toObject()["name"].toString() + ", ");
  }
  game.tags.chop(2);
}

void GiantBomb::getFranchises(GameEntry &game)
{
  QJsonArray jsonFranchises = jsonObj["franchises"].toArray();
  for(const auto &jsonFranchise: jsonFranchises) {
    game.franchises.append(jsonFranchise.toObject()["name"].toString() + ", ");
  }
  game.franchises.chop(2);
}

void GiantBomb::getAges(GameEntry &game)
{
  // releases->game_rating->name
  game.ages = StrTools::conformAges(jsonObjRel["game_rating"].toObject()["name"].toString());
  if(game.ages.isEmpty()) {
    QJsonObject jsonRatings = jsonObj["original_game_rating"].toArray().first().toObject();
    game.ages = StrTools::conformAges(jsonRatings["game_rating"].toObject()["name"].toString());
  }
}

void GiantBomb::getPublisher(GameEntry &game)
{
  // Use release endpoint to retrieve publisher information
  // releases->release->publishers->company->name
  // NOTE: Not worth it.
  QJsonObject jsonPublisher = jsonObj["publishers"].toArray().first().toObject();
  game.publisher = jsonPublisher["name"].toString();
}

void GiantBomb::getDeveloper(GameEntry &game)
{
  // Use release endpoint to retrieve developer information
  // releases->release->developers->company->name
  // NOTE: Not worth it.
  QJsonObject jsonDeveloper = jsonObj["developers"].toArray().first().toObject();
  game.developer = jsonDeveloper["name"].toString();
}

void GiantBomb::getDescription(GameEntry &game)
{
  game.description = "";
  QJsonValue jsonValue = jsonObj["deck"];
  if(jsonValue != QJsonValue::Undefined && jsonValue.toString().length() > 4) {
    game.description = StrTools::stripHtmlTags(jsonValue.toString());
  }
  jsonValue = jsonObj["description"];
  if(jsonValue != QJsonValue::Undefined && jsonValue.toString().length() > 4) {
    if(!game.description.isEmpty()) {
      game.description.append('\n').append('\n').append('\n');
    }
    game.description.append(QTextDocumentFragment::fromHtml(
                            jsonValue.toString()).toPlainText().replace(QChar(0xFFFC), '\n'));
    /* game.description.append(StrTools::stripHtmlTags(
                            jsonValue.toString().
                            replace("<p>", "\n", Qt::CaseInsensitive).
                            replace("</p>", "\n", Qt::CaseInsensitive).
                            replace("<br>", "\n", Qt::CaseInsensitive)));*/
  }
}

/*void GiantBomb::getRating(GameEntry &game)
{
  // Use reviews and user_reviews endpoints to retrieve and average score information
  // reviews->review->score (1-5) + user_reviews->user_review->score (1-5)
  // NOTE: Too few reviews were found to be useful (<1000).
}*/

void GiantBomb::getCover(GameEntry &game)
{
  // COVER -> images->image->original (tags = *Box Art*)
  QJsonValue jsonValue = jsonObj["image"].toObject()["original_url"];
  if(jsonValue == QJsonValue::Undefined) {
    return;
  }
  QString coverUrl = jsonValue.toString();
  
  if(!coverUrl.isEmpty()) {
    netComm->request(coverUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
    }
  }
}

void GiantBomb::getMarquee(GameEntry &game)
{
  // MARQUEE -> images->image->original (tags = *Media*)
  QString imageUrl;
  QJsonArray jsonArray = jsonObj["images"].toArray();
  if(!jsonArray.isEmpty()) {
    for(const auto &jsonImage: jsonArray) {
      if(jsonImage.toObject()["tags"].toString().contains("Media", Qt::CaseInsensitive) ||
         (jsonImage.toObject()["tags"].toString().contains("Art", Qt::CaseInsensitive) &&
          !jsonImage.toObject()["tags"].toString().contains("Box Art", Qt::CaseInsensitive))) {
        imageUrl = jsonImage.toObject()["original"].toString();
        if(imageUrl.isEmpty()) {
          imageUrl = jsonImage.toObject()["original_url"].toString();
        }
        if(!imageUrl.isEmpty()) {
          break;
        }
      }
    }
  }
  
  // Deep dive into the image endpoint:
  if(imageUrl.isEmpty()) {
    jsonArray = jsonObj["image_tags"].toArray();
    for(const auto &jsonTags: jsonArray) {
      if(jsonTags.toObject()["name"].toString().contains("Media", Qt::CaseInsensitive) ||
         (jsonTags.toObject()["name"].toString().contains("Art", Qt::CaseInsensitive) &&
          !jsonTags.toObject()["name"].toString().contains("Box Art", Qt::CaseInsensitive))) {

        QString endPointUrl = jsonTags.toObject()["api_detail_url"].toString() + "&" + urlPost;
        printf("Waiting to get image data...");
        fflush(stdout);
        if(!requestGb(endPointUrl, &lastImageRequest)) {
          printf(" ERROR, skipping marquee image.\n");
          return;
        }
        printf(" DONE.\n");
        QJsonArray jsonMarquee = jsonDoc.object()["results"].toArray();

        if(!jsonMarquee.isEmpty()) {
          imageUrl = jsonMarquee.first().toObject()["original_url"].toString();
        }
        break;
      }
    }
  }
  
  if(!imageUrl.isEmpty()) {
    // qDebug() << "Marquee: " << imageUrl;
    netComm->request(imageUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}

void GiantBomb::getWheel(GameEntry &game)
{
  // WHEEL -> images->image->original+image_tags (*Screenshot*, 1)
  QString imageUrl;
  QJsonArray jsonArray = jsonObj["images"].toArray();
  if(!jsonArray.isEmpty()) {
    for(const auto &jsonImage: jsonArray) {
      QString imageTag = jsonImage.toObject()["tags"].toString();
      if(imageTag.contains("Screenshot", Qt::CaseInsensitive) &&
         (QRegularExpression("[-_]0*1[.]").match(imageUrl).hasMatch())) {
        imageUrl = jsonImage.toObject()["original"].toString();
        if(imageUrl.isEmpty()) {
          imageUrl = jsonImage.toObject()["original_url"].toString();
        }
        if(!imageUrl.isEmpty()) {
          break;
        }
      }
    }
  }

  // Deep dive into the image endpoint:
  if(imageUrl.isEmpty()) {
    jsonArray = jsonObj["image_tags"].toArray();
    for(const auto &jsonTags: jsonArray) {
      if(jsonTags.toObject()["name"].toString() == "Screenshots") {

        if(jsonImages.isEmpty()) {
          QString endPointUrl = jsonTags.toObject()["api_detail_url"].toString() + "&" + urlPost;
          printf("Waiting to get image data...");
          fflush(stdout);
          if(requestGb(endPointUrl, &lastImageRequest)) {
            printf(" DONE.\n");
            jsonImages = jsonDoc.object()["results"].toArray();
          } else {
            printf(" ERROR, skipping screenshot image.\n");
            return;
          }
        }

        for(const auto &jsonImage: jsonImages) {
          imageUrl = jsonImage.toObject()["original_url"].toString();
          if(QRegularExpression("[-_]0*1[.]").match(imageUrl).hasMatch()) {
            break;
          }
        }
        break;
      }
    }
  }

  if(!imageUrl.isEmpty()) {
    // qDebug() << "Wheel: " << imageUrl;
    netComm->request(imageUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.wheelData = netComm->getData();
    }
  }
}

void GiantBomb::getScreenshot(GameEntry &game)
{
  // SCREENSHOT -> images->image->original+image_tags (*Screenshot*, >1)
  QString imageUrl;
  QJsonArray jsonArray = jsonObj["images"].toArray();
  if(!jsonArray.isEmpty()) {
    for(const auto &jsonImage: jsonArray) {
      QString imageTag = jsonImage.toObject()["tags"].toString();
      if(imageTag.contains("Screenshot", Qt::CaseInsensitive) &&
         (!QRegularExpression("[-_]0*1[.]").match(imageUrl).hasMatch())) {
        imageUrl = jsonImage.toObject()["original"].toString();
        if(imageUrl.isEmpty()) {
          imageUrl = jsonImage.toObject()["original_url"].toString();
        }
        if(!imageUrl.isEmpty()) {
          break;
        }
      }
    }
  }
    
  // Deep dive into the image endpoint:
  if(imageUrl.isEmpty()) {
    jsonArray = jsonObj["image_tags"].toArray();
    for(const auto &jsonTags: jsonArray) {
      if(jsonTags.toObject()["name"].toString() == "Screenshots") {

        if(jsonImages.isEmpty()) {
          QString endPointUrl = jsonTags.toObject()["api_detail_url"].toString() + "&" + urlPost;
          printf("Waiting to get image data...");
          fflush(stdout);
          if(requestGb(endPointUrl, &lastImageRequest)) {
            printf(" DONE.\n");
            jsonImages = jsonDoc.object()["results"].toArray();
          } else {
            printf(" ERROR, skipping screenshot image.\n");
            return;
          }
        }

        for(const auto &jsonImage: jsonImages) {
          imageUrl = jsonImage.toObject()["original_url"].toString();
          if(!QRegularExpression("[-_]0*1[.]").match(imageUrl).hasMatch()) {
            break;
          }
        }
        break;
      }
    }
  }

  if(!imageUrl.isEmpty()) {
    // qDebug() << "Screenshot: " << imageUrl;
    netComm->request(imageUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.screenshotData = netComm->getData();
    }
  }
}

void GiantBomb::getVideo(GameEntry &game)
{
  // VIDEO -> videos->video->api_detail_url -> results->low_url (1800->800?) (with web login)
  QString url;
  if(!jsonObjVid.isEmpty()) {
    url = jsonObjVid["low_url"].toString();
    if (url.isEmpty()) {
      url = jsonObjVid["high_url"].toString();
    }
  }

  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      netComm->request(url + "?" + urlPost.chopped(12));
      q.exec();
      game.videoData = netComm->getData();
      // Make sure received data is actually a video file
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         contentType.contains("video/") &&
         game.videoData.size() > 4096) {
        game.videoFormat = contentType.mid(contentType.indexOf("/") + 1,
                                           contentType.length() - contentType.indexOf("/") + 1);
      } else {
        game.videoData = "";
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

QList<QString> GiantBomb::getSearchNames(const QFileInfo &info)
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
      noSubtitle = NameTools::getUrlQueryName(NameTools::removeArticle(noSubtitle), -1, " ");
      noSubtitle = StrTools::sanitizeName(noSubtitle, true);
      searchNames.append(noSubtitle);
    }
  } */
  // printf("D: %s\n", sanitizedName.toStdString().c_str());
  return searchNames;
}

QString GiantBomb::getPlatformId(const QString platform)
{
  auto it = platformToId.find(platform);
  if(it != platformToId.cend())
      return QString::number(it.value());

  return "na";
}

void GiantBomb::loadConfig(const QString& configPath)
{
  platformToId.clear();

  QFile configFile(configPath);
  if (!configFile.open(QIODevice::ReadOnly)) {
    return;
  }

  QByteArray saveData = configFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty()) {
    return;
  }

  QJsonArray platformsArray = json["platforms"].toArray();
  for (int platformIndex = 0; platformIndex < platformsArray.size(); ++platformIndex) {
    QJsonObject platformObject = platformsArray[platformIndex].toObject();

    QString platformName = platformObject["name"].toString();
    int platformId = platformObject["id"].toInt(-1);

    if(platformId > 0)
      platformToId[platformName] = platformId;
  }
}
