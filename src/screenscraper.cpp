/***************************************************************************
 *            screenscraper.cpp
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

#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>

#include "platform.h"
#include "queue.h"
#include "screenscraper.h"
#include "strtools.h"
#include "crc32.h"

constexpr int RETRIESMAX = 3;
constexpr int MINARTSIZE = 256;
constexpr int MINTEXTURESIZE = 16384;

ScreenScraper::ScreenScraper(Settings *config,
                             QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  incrementalScraping = true;
  loadConfig("screenscraper.json");
  
  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(1200); // 1.2 second request limit set a bit above 1.0 as requested by the good folks at ScreenScraper. Don't change!
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "http://www.screenscraper.fr";

  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(AGES);
  fetchOrder.append(RATING);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(VIDEO);
  fetchOrder.append(MANUAL);
}

void ScreenScraper::getSearchResults(QList<GameEntry> &gameEntries,
                                     QString searchName, QString)
{
  QString platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by ScreenScraper or it hasn't yet been included in Skyscraper for this module...\033[0m\n");
    return;
  }

  QString gameUrl = "https://www.screenscraper.fr/api2/jeuInfos.php?devid=muldjord&devpassword=" +
                    config->apiKey + "&softname=skyscraper" VERSION +
                    (config->user.isEmpty()?"":"&ssid=" + config->user) +
                    (config->password.isEmpty()?"":"&sspassword=" + config->password) +
                    (platformId.isEmpty()?"":"&systemeid=" + platformId) + "&output=json&" +
                    searchName;
//  printf(gameUrl.toStdString().c_str()); printf("\n"); printf("\n");

  for(int retries = 0; retries < RETRIESMAX; ++retries) {
    limiter.exec();
    printf("1");
    netComm->request(gameUrl);
    q.exec();
    data = netComm->getData();

    QByteArray headerData = data.left(1024); // Minor optimization with minimal more RAM usage
    // Do error checks on headerData. It's more stable than checking the potentially faulty JSON
    if(headerData.isEmpty()) {
      printf("\033[1;33mRetrying request...\033[0m\n\n");
      continue;
    } else if(headerData.contains("non trouvée")) {
      return;
    } else if(headerData.contains("API totalement fermé")) {
      printf("\033[1;31mThe ScreenScraper API is currently closed, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      return;
    } else if(headerData.contains("Le logiciel de scrape utilisé a été blacklisté")) {
      printf("\033[1;31mSkyscraper has apparently been blacklisted at ScreenScraper, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      return;
    } else if(headerData.contains("Votre quota de scrape est")) {
      printf("\033[1;31mYour daily ScreenScraper request limit has been reached, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      return;
    } else if(headerData.contains("API fermé pour les non membres") ||
              headerData.contains("API closed for non-registered members") ||
              headerData.contains("****T****h****e**** ****m****a****x****i****m****u****m**** ****t****h****r****e****a****d****s**** ****a****l****l****o****w****e****d**** ****t****o**** ****l****e****e****c****h****e****r**** ****u****s****e****r****s**** ****i****s**** ****a****l****r****e****a****d****y**** ****u****s****e****d****")) {
      printf("\033[1;31mThe screenscraper service is currently closed or too busy to handle requests from unregistered and inactive users. Sign up for an account at https://www.screenscraper.fr and contribute to gain more threads. Then use the credentials with Skyscraper using the '-u user:pass' command line option or by setting 'userCreds=\"user:pass\"' in '%s/.skyscraper/config.ini'.\033[0m\n\n", QDir::homePath().toStdString().c_str());
      if(retries == RETRIESMAX - 1) {
        reqRemaining = 0;
        return;
      } else {
        continue;
      }
    }

    // Fix faulty JSON that is sometimes received back from ScreenScraper
    data.replace("],\n\t\t}", "]\n\t\t}");
    //printf("\n%s\n",data.toStdString().c_str());

    // Now parse the JSON
    jsonObj = QJsonDocument::fromJson(data).object();
    //QJsonDocument doc;
    //doc.setArray(jsonObj[medias].toArray());
    //printf("1 %s\n",doc.toJson().toStdString().c_str());
    //exit(0);

    // Check if we got a valid JSON document back
    if(jsonObj.isEmpty()) {
      printf("\033[1;31mScreenScraper APIv2 returned invalid / empty Json. Their servers are probably down. Please try again later or use a different scraping module with '-s MODULE'. Check 'Skyscraper --help' for more information.\033[0m\n");
      data.replace(config->apiKey.toUtf8(), "****");
      data.replace(config->password.toUtf8(), "****");
      QFile jsonErrorFile("./screenscraper_error.json");
      if(jsonErrorFile.open(QIODevice::WriteOnly)) {
        if(data.length() > 64) {
          jsonErrorFile.write(data);
          printf("The erroneous answer was written to '%s/.skyscraper/screenscraper_error.json'. If this file contains game data, please consider filing a bug report at 'https://github.com/detain/skyscraper/issues' and attach that file.\n", QDir::homePath().toStdString().c_str());
        }
        jsonErrorFile.close();
      }
      break; // DON'T try again! If we don't get a valid JSON document, something is very wrong with the API
    }

    // Check if the request was successful
    if(jsonObj["header"].toObject()["success"].toString() != "true") {
      printf("Request returned a success state of '%s'. Error was:\n%s\n",
             jsonObj["header"].toObject()["success"].toString().toStdString().c_str(),
             jsonObj["header"].toObject()["error"].toString().toStdString().c_str());
      // Try again. We handle important errors above, so something weird is going on here
      continue;
    }

    // Check if user has exceeded daily request limit
    if(!jsonObj["response"].toObject()["ssuser"].toObject()["requeststoday"].toString().isEmpty() && !jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestsperday"].toString().isEmpty()) {
      reqRemaining = jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestsperday"].toString().toInt() - jsonObj["response"].toObject()["ssuser"].toObject()["requeststoday"].toString().toInt();
      if(reqRemaining <= 0) {
        printf("\033[1;31mYour daily ScreenScraper request limit has been reached, exiting nicely...\033[0m\n\n");
      }
    }

    // Check if we got a game entry back
    if(jsonObj["response"].toObject().contains("jeu")) {
      // Game found, stop retrying
      break;
    }
  }

  jsonObj = jsonObj["response"].toObject()["jeu"].toObject();

  GameEntry game;
  game.title = getJsonText(jsonObj["noms"].toArray(), REGION);

  // 'screenscraper' sometimes returns a faulty result with the following names. If we get either
  // result DON'T use it. It will provide faulty data for the cache
  if((game.title.toLower().contains("hack") && game.title.toLower().contains("link")) ||
     game.title.toLower() == "zzz" ||
     game.title.toLower().contains("notgame")) {
    return;
  }

  // If title still unset, no acceptable rom was found, so return with no results
  if(game.title.isNull()) {
    return;
  }

  game.url = gameUrl;
  game.platform = jsonObj["systeme"].toObject()["text"].toString();

  // Only check if platform is empty, it's always correct when using ScreenScraper
  if(!game.platform.isEmpty())
    gameEntries.append(game);
}

void ScreenScraper::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
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

void ScreenScraper::getReleaseDate(GameEntry &game)
{
  game.releaseDate = getJsonText(jsonObj["dates"].toArray(), REGION);
}

void ScreenScraper::getDeveloper(GameEntry &game)
{
  game.developer = jsonObj["developpeur"].toObject()["text"].toString();
}

void ScreenScraper::getPublisher(GameEntry &game)
{
  game.publisher = jsonObj["editeur"].toObject()["text"].toString();
}

void ScreenScraper::getDescription(GameEntry &game)
{
  game.description = getJsonText(jsonObj["synopsis"].toArray(), LANGUE);
}

void ScreenScraper::getPlayers(GameEntry &game)
{
  game.players = jsonObj["joueurs"].toObject()["text"].toString();
}

void ScreenScraper::getAges(GameEntry &game)
{
  QList<QString> ageBoards;
  ageBoards.append("PEGI");
  ageBoards.append("ESRB");
  ageBoards.append("SS");

  if(!jsonObj["classifications"].isArray())
    return;

  QJsonArray jsonAges = jsonObj["classifications"].toArray();

  for(const auto &ageBoard: ageBoards) {
    for(int a = 0; a < jsonAges.size(); ++a) {
      if(jsonAges.at(a).toObject()["type"].toString() == ageBoard) {
        game.ages = jsonAges.at(a).toObject()["text"].toString();
        return;
      }
    }
  }
}

void ScreenScraper::getRating(GameEntry &game)
{
  game.rating = jsonObj["note"].toObject()["text"].toString();
  bool toDoubleOk = false;
  double rating = game.rating.toDouble(&toDoubleOk);
  if(toDoubleOk) {
    game.rating = QString::number(rating / 20.0);
  } else {
    game.rating = "";
  }
}

void ScreenScraper::getTags(GameEntry &game)
{
  if(!jsonObj["genres"].isArray())
    return;

  QJsonArray jsonTags = jsonObj["genres"].toArray();

  for(int a = 0; a < jsonTags.size(); ++a) {
    QString tag = getJsonText(jsonTags.at(a).toObject()["noms"].toArray(), LANGUE);
    if(!tag.isEmpty()) {
      game.tags.append(tag + ", ");
    }
  }

  game.tags = game.tags.left(game.tags.length() - 2);
}

void ScreenScraper::getFranchises(GameEntry &game)
{
  if(!jsonObj["familles"].isArray())
    return;

  QJsonArray jsonFranchises = jsonObj["familles"].toArray();

  for(int a = 0; a < jsonFranchises.size(); ++a) {
    QString franchise = getJsonText(jsonFranchises.at(a).toObject()["noms"].toArray(), LANGUE);
    if(!franchise.isEmpty()) {
      game.franchises.append(franchise + ", ");
    }
  }

  game.franchises = game.franchises.left(game.franchises.length() - 2);
}

void ScreenScraper::getCover(GameEntry &game)
{
  QString url = "";
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     config->platform != "gameandwatch") {
    url = getJsonText(jsonObj["medias"].toArray(), REGION, QList<QString>({"flyer"}));
  } else {
    url = getJsonText(jsonObj["medias"].toArray(), REGION, QList<QString>({"box-2D", "box-2d", "flyer", "box-texture"}));
  }
  //printf("1 %s\n",url.toStdString().c_str());
  //QJsonDocument doc;
  //doc.setArray(jsonObj["medias"].toArray());
  //printf("1 %s\n",doc.toJson().toStdString().c_str());
  //exit(0);
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("2");
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         netComm->getData().size() >= MINARTSIZE &&
         image.loadFromData(netComm->getData())) {
        game.coverData = netComm->getData();
      } else {
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

void ScreenScraper::getScreenshot(GameEntry &game)
{
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, QList<QString>({"ss"}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("3");
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         netComm->getData().size() >= MINARTSIZE &&
         image.loadFromData(netComm->getData())) {
        game.screenshotData = netComm->getData();
      } else {
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

void ScreenScraper::getWheel(GameEntry &game)
{
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, QList<QString>({"sstitle"/*, "wheel", "wheel-hd"*/}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("4");
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         netComm->getData().size() >= MINARTSIZE &&
         image.loadFromData(netComm->getData())) {
        game.wheelData = netComm->getData();
      } else {
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

void ScreenScraper::getMarquee(GameEntry &game)
{
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, QList<QString>({"fanart-hd", "fanart"/*, "support-2D", "support-2d", "support-texture", "marquee", "screenmarquee"*/}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("5");
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         netComm->getData().size() >= MINARTSIZE &&
         image.loadFromData(netComm->getData())) {
        game.marqueeData = netComm->getData();
      } else {
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

void ScreenScraper::getTexture(GameEntry &game) {
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION,
      QList<QString>({"box-2D-back", "box-2d-back", "support-2D", "support-2d", "support-texture"}));
  if (!url.isEmpty()) {
    bool moveOn = false;
    for (int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("6");
      netComm->request(url);
      q.exec();
      QImage image;
      if (netComm->getError(config->verbosity) == QNetworkReply::NoError) {
        int imgSize = netComm->getData().size();
        if (imgSize > MINARTSIZE && image.loadFromData(netComm->getData())) {
          if (imgSize > MINTEXTURESIZE) {
            game.textureData = netComm->getData();
          }
          moveOn = true;
        }
      }
      if (moveOn) break;
    }
  }
}

void ScreenScraper::getVideo(GameEntry &game)
{
  QStringList types;
  if(config->videoPreferNormalized) {
    types.append("video-normalized");
  }
  types.append("video");
  QString url = getJsonText(jsonObj["medias"].toArray(), NONE, types);
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("7");
      netComm->request(url);
      q.exec();
      game.videoData = netComm->getData();
      // Make sure received data is actually a video file
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         contentType.contains("video/") && game.videoData.size() > 4096) {
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

void ScreenScraper::getManual(GameEntry &game)
{
  QStringList types;
  types.append("manuel");
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, types);
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("8");
      netComm->request(url);
      q.exec();
      game.manualData = netComm->getData();
      // Make sure received the data is not empty and has a type
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         !contentType.isEmpty() && game.manualData.size() > 4096) {
        game.manualFormat = contentType.mid(contentType.indexOf("/") + 1,
                                            contentType.length() - contentType.indexOf("/") + 1);
        if (game.manualFormat.length()>4) {
          game.manualFormat = "pdf";
        }
      } else {
        game.manualData = "";
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

QList<QString> ScreenScraper::getSearchNames(const QFileInfo &info)
{
  QList<QString> searchNames;

  bool unpack = config->unpack;

  if (unpack) {
    // For 7z (7z, zip) unpacked file reading
    if (info.suffix() == "7z" || info.suffix() == "zip")  {
      // Size limit for "unpack" is set to 800 megs to ensure the pi doesn't run out of memory
      if (info.size() < 819200000) {
        QProcess decProc;
        decProc.setReadChannel(QProcess::StandardOutput);
        decProc.start("7z", QStringList({"l", "-so", "-slt", "-ba", info.absoluteFilePath()}));
        if(decProc.waitForFinished(30000)) {
          if(decProc.exitStatus() != QProcess::NormalExit) {
            printf("Getting file list from compressed file failed, falling back...\n");
            unpack = false;
          } else {
            QList<QByteArray> output7z = decProc.readAllStandardOutput().split('\n');
            QString formats = Platform::get().getFormats(config->platform, config->extensions, config->addExtensions);
            QSharedPointer<Queue> queue = QSharedPointer<Queue>(new Queue());
            QList<QFileInfo> infoList{};
            for (int i = 0; i < output7z.size(); i++) {
              if (output7z.at(i).startsWith("Path = ")) {
                QFileInfo compressedFile(QString(output7z.at(i)).remove(0, 7));
                QString suffix(compressedFile.suffix());
                if (formats.contains(suffix) && suffix != "7z" && suffix != "zip") {
                  infoList.append(compressedFile);
                }
              }
            }
            queue->append(infoList);
            if(!config->excludePattern.isEmpty()) {
              queue->filterFiles(config->excludePattern);
            }
            if(!config->includePattern.isEmpty()) {
              queue->filterFiles(config->includePattern, true);
            }
            while(queue->hasEntry()) {
              QFileInfo info = queue->takeEntry();
              searchNames.append("romnom=" + QUrl::toPercentEncoding(info.completeBaseName(), "()"));
              if (info.completeBaseName() != StrTools::stripBrackets(info.completeBaseName())) {
                searchNames.append("romnom=" + QUrl::toPercentEncoding(StrTools::stripBrackets(info.completeBaseName()), "()"));
              }
            }
          }
        } else {
          printf("Getting file list from compressed file timed out or failed, falling back...\n");
          unpack = false;
        }
      } else {
        printf("Compressed file exceeds 800 meg size limit, falling back...\n");
        unpack = false;
      }
    } else {
      unpack = false;
    }
  }

  if(!unpack) {
    QList<QString> searchNamesRaw = AbstractScraper::getSearchNames(info);
    QListIterator<QString> listNames(searchNamesRaw);
    while (listNames.hasNext()) {
      // printf("%s\n", listNames.next().toStdString().c_str());
      searchNames.append("romnom=" + QUrl::toPercentEncoding(listNames.next(), "()"));
    }
    /*searchNames.append("romnom=" + QUrl::toPercentEncoding(info.completeBaseName(), "()"));
    if (info.completeBaseName() != StrTools::stripBrackets(info.completeBaseName())) {
      searchNames.append("romnom=" + QUrl::toPercentEncoding(StrTools::stripBrackets(info.completeBaseName()), "()"));
    }*/
  }
  
  return searchNames;
}

QString ScreenScraper::getJsonText(QJsonArray jsonArr, int attr, QList<QString> types)
{
  if(attr == NONE && !types.isEmpty()) {
    for(const auto &type: types) {
      for(const auto &jsonVal: jsonArr) {
        if(jsonVal.toObject()["type"].toString() == type) {
          if(jsonVal.toObject()["url"].isString()) {
            return jsonVal.toObject()["url"].toString();
          } else {
            return jsonVal.toObject()["text"].toString();
          }
        }
      }
    }
  } else if(attr == REGION) {
    // Not using the config->regionPrios since they might have changed due to region autodetection. So using temporary internal one instead.
    if(types.isEmpty()) {
      for(const auto &region: regionPrios) {
        for(const auto &jsonVal: jsonArr) {
          if(jsonVal.toObject()["region"].toString() == region ||
             jsonVal.toObject()["region"].toString().isEmpty()) {
            if(jsonVal.toObject()["url"].isString()) {
              return jsonVal.toObject()["url"].toString();
            } else {
              return jsonVal.toObject()["text"].toString();
            }
          }
        }
      }
    } else {
      for(const auto &type: types) {
        for(const auto &region: regionPrios) {
          for(const auto &jsonVal: jsonArr) {
            if((jsonVal.toObject()["region"].toString() == region ||
               jsonVal.toObject()["region"].toString().isEmpty()) &&
               jsonVal.toObject()["type"].toString() == type) {
              if(jsonVal.toObject()["url"].isString()) {
                return jsonVal.toObject()["url"].toString();
              } else {
                return jsonVal.toObject()["text"].toString();
              }
            }
          }
        }
      }
    }
  } else if(attr == LANGUE) {
    if(types.isEmpty()) {
      for(const auto &lang: config->langPrios) {
        for(const auto &jsonVal: jsonArr) {
          if(jsonVal.toObject()["langue"].toString() == lang ||
             jsonVal.toObject()["langue"].toString().isEmpty()) {
            if(jsonVal.toObject()["url"].isString()) {
              return jsonVal.toObject()["url"].toString();
            } else {
              return jsonVal.toObject()["text"].toString();
            }
          }
        }
      }
    } else {
      for(const auto &type: types) {
        for(const auto &lang: config->langPrios) {
          for(const auto &jsonVal: jsonArr) {
            if((jsonVal.toObject()["langue"].toString() == lang ||
               jsonVal.toObject()["langue"].toString().isEmpty()) &&
               jsonVal.toObject()["type"].toString() == type) {
              if(jsonVal.toObject()["url"].isString()) {
                return jsonVal.toObject()["url"].toString();
              } else {
                return jsonVal.toObject()["text"].toString();
              }
            }
          }
        }
      }
    }
  }
  return QString();
}

QString ScreenScraper::getPlatformId(const QString platform)
{
  auto it = platformToId.find(platform);
  if(it != platformToId.cend())
      return QString::number(it.value());

  return "na";
}

void ScreenScraper::loadConfig(const QString& configPath)
{
  platformToId.clear();

  QFile configFile(configPath);
  if (!configFile.open(QIODevice::ReadOnly))
    return;

  QByteArray saveData = configFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty())
    return;

  QJsonArray platformsArray = json["platforms"].toArray();
  for (int platformIndex = 0; platformIndex < platformsArray.size(); ++platformIndex) {
    QJsonObject platformObject = platformsArray[platformIndex].toObject();

    QString platformName = platformObject["name"].toString();
    int platformId = platformObject["id"].toInt(-1);

    if(platformId > 0)
      platformToId[platformName] = platformId;
  }
}
