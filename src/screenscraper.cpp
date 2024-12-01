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

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

#include "platform.h"
#include "queue.h"
#include "screenscraper.h"
#include "strtools.h"
#include "nametools.h"

constexpr int RETRIESMAX = 3;
constexpr int MINARTSIZE = 256;
constexpr int MINTEXTURESIZE = 16384;

ScreenScraper::ScreenScraper(Settings *config,
                             QSharedPointer<NetManager> manager,
                             QString threadId)
  : AbstractScraper(config, manager, threadId)
{
  loadConfig("screenscraper.json", "name", "id");

  platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by ScreenScraper or it hasn't "
           "yet been included in Skyscraper for this module...\033[0m\n");
    return;
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  // 1.2 second request limit set a bit above 1.0 as requested by the folks at ScreenScraper. Don't change!
  limitTimer.setInterval(1200);
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
  QString gameUrl = "https://www.screenscraper.fr/api2/jeuInfos.php?devid=muldjord&devpassword=" +
                    config->apiKey + "&softname=skyscraper" VERSION +
                    (config->user.isEmpty()?"":"&ssid=" + config->user) +
                    (config->password.isEmpty()?"":"&sspassword=" + config->password) +
                    (platformId.isEmpty()?"":"&systemeid=" + platformId) + "&output=json&" +
                    searchName;

  searchError = false;
  for(int retries = 0; retries < RETRIESMAX; ++retries) {
    limiter.exec();
    printf("1"); fflush(stdout);
    netComm->request(gameUrl);
    q.exec();
    data = netComm->getData();

    QByteArray headerData = data.left(1024); // Minor optimization with minimal more RAM usage
    // Do error checks on headerData. It's more stable than checking the potentially faulty JSON
    if(config->verbosity > 4) {
      qDebug() << headerData;
    }
    if(headerData.isEmpty()) {
      printf("\033[1;33mRetrying request...\033[0m\n\n");
      continue;
    } else if(headerData.contains("non trouvée")) {
      return;
    } else if(headerData.contains("API totalement fermé")) {
      printf("\033[1;31mThe ScreenScraper API is currently closed, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      searchError = true;
      return;
    } else if(headerData.contains("Le logiciel de scrape utilisé a été blacklisté")) {
      printf("\033[1;31mSkyscraper has apparently been blacklisted at ScreenScraper, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      searchError = true;
      return;
    } else if(headerData.contains("Votre quota de scrape est")) {
      printf("\033[1;31mYour daily ScreenScraper request limit has been reached, exiting nicely...\033[0m\n\n");
      reqRemaining = 0;
      searchError = true;
      return;
    } else if(headerData.contains("Faite du tri dans vos fichiers roms et repassez demain")) {
      printf("\033[1;31mYour daily ScreenScraper failed request limit has been reached, exiting nicely...\033[0m\n\n");
      reqRemainingKO = 0;
      searchError = true;
      return;
    } else if(headerData.contains("API fermé pour les non membres") ||
              headerData.contains("API closed for non-registered members") ||
              headerData.contains("****T****h****e**** ****m****a****x****i****m****u****m**** "
                                  "****t****h****r****e****a****d****s**** ****a****l****l****o"
                                  "****w****e****d**** ****t****o**** ****l****e****e****c****h"
                                  "****e****r**** ****u****s****e****r****s**** ****i****s**** "
                                  "****a****l****r****e****a****d****y**** ****u****s****e****d****")) {
      printf("\033[1;31mThe screenscraper service is currently closed or too busy to handle "
             "requests from unregistered and inactive users. Sign up for an account at "
             "https://www.screenscraper.fr and contribute to gain more threads. Then use the "
             "credentials with Skyscraper using the '-u user:pass' command line option or by "
             "setting 'userCreds=\"user:pass\"' in '%s/.skyscraper/config.ini'.\033[0m\n\n",
             QDir::homePath().toStdString().c_str());
      if(retries == RETRIESMAX - 1) {
        reqRemaining = 0;
        searchError = true;
        return;
      } else {
        continue;
      }
    }

    if(config->verbosity > 5) {
      qDebug() << data;
    }
    // Fix faulty JSON that is sometimes received back from ScreenScraper
    data.replace("],\n\t\t}", "]\n\t\t}");
    //printf("\n%s\n",data.toStdString().c_str());
    int openDelims = data.count('{');
    int closeDelims = data.count('}');
    if(openDelims != closeDelims && QJsonDocument::fromJson(data).isNull()) {
      printf("\nERROR: Detected delimiter unbalance! Trying to correct... "); fflush(stdout);
      if(openDelims < closeDelims) {
        int delimsDetected = 0;
        int pos = 0;
        for(auto i = data.rbegin(); i != data.rend(); ++i) {
          pos++;
          if(*i == '}') {
            delimsDetected++;
            if(delimsDetected == (closeDelims - openDelims)) {
              break;
            }
          }
        }
        data.chop(pos);
      } else {
        data.append(QString("}").repeated(openDelims - closeDelims).toUtf8());
      }
      if(QJsonDocument::fromJson(data).isNull()) {
        printf("Unsuccessful...\n");
      } else {
        printf("Success!\n");
      }
      if(config->verbosity > 5) {
        qDebug() << data;
      }
    }

    // Now parse the JSON
    jsonObj = QJsonDocument::fromJson(data).object();
    //QJsonDocument doc;
    //doc.setArray(jsonObj[medias].toArray());
    //printf("1 %s\n",doc.toJson().toStdString().c_str());
    //exit(0);

    // Check if we got a valid JSON document back
    if(jsonObj.isEmpty()) {
      printf("\033[1;31mScreenScraper APIv2 returned invalid / empty Json. Their servers are "
             "probably down. Please try again later or use a different scraping module with "
             "'-s MODULE'. Check 'Skyscraper --help' for more information.\033[0m\n");
      /*data.replace(config->apiKey.toUtf8(), "****");
      data.replace(config->password.toUtf8(), "****");*/
      QFile jsonErrorFile("./screenscraper_error.json");
      if(jsonErrorFile.open(QIODevice::WriteOnly)) {
        if(data.length() > 64) {
          jsonErrorFile.write(data);
          printf("The erroneous answer was written to '%s/.skyscraper/screenscraper_error.json'. "
                 "If this file contains game data, please consider filing a bug report at "
                 "'https://github.com/detain/skyscraper/issues' and attach that file.\n",
                 QDir::homePath().toStdString().c_str());
        }
        jsonErrorFile.close();
      }
      // DON'T try again! If we don't get a valid JSON document, something is very wrong with the API
      searchError = true;
      break;
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
    int maxRequestsDay = jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestsperday"].toString().toInt();
    int currentRequests = jsonObj["response"].toObject()["ssuser"].toObject()["requeststoday"].toString().toInt();
    int maxKORequestsDay = jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestskoperday"].toString().toInt();
    int currentKORequests = jsonObj["response"].toObject()["ssuser"].toObject()["requestskotoday"].toString().toInt();
    if((maxRequestsDay <= currentRequests) || (maxKORequestsDay <= currentKORequests)) {
      printf("\033[1;33mThe daily user limits at ScreenScraper have been reached, exiting nicely."
             "\nPlease wait until the next day.\033[0m\n\n");
      reqRemaining = 0;
    } else {
      reqRemaining = std::max(0, maxRequestsDay-currentRequests);
      reqRemainingKO = std::max(0, maxKORequestsDay-currentKORequests);
    }

    // Check if we got a game entry back
    if(jsonObj["response"].toObject().contains("jeu")) {
      // Game found, stop retrying
      break;
    }
  }
  if(netComm->getError() != QNetworkReply::NoError &&
     netComm->getError() <= QNetworkReply::ProxyAuthenticationRequiredError) {
    printf("Connection error. Is the API down?\n");
    searchError = true;
    return;
  } else {
    searchError = false;
  }

  jsonObj = jsonObj["response"].toObject()["jeu"].toObject();

  GameEntry game;
  game.title = getJsonText(jsonObj["noms"].toArray(), REGION);

  // 'screenscraper' sometimes returns a faulty result with the following names. If we get either
  // result DON'T use it. It will provide faulty data for the cache
  if((game.title.toLower().contains("hack") && game.title.toLower().contains("link")) ||
     game.title.toLower() == "zzz" ||
     game.title.toLower().contains("notgame")) {
    searchError = true;
    return;
  }

  // If title still unset, no acceptable rom was found, so return with no results
  if(game.title.isNull()) {
    return;
  }

  game.url = gameUrl;
  game.platform = jsonObj["systeme"].toObject()["text"].toString();

  if(!canonical.md5.isEmpty() || !canonical.sha1.isEmpty()) {
    canonical.name = game.title;
    game.canonical = canonical;
  }

  // Only check if platform is empty, it's always correct when using ScreenScraper
  if(!game.platform.isEmpty()) {
    gameEntries.append(game);
  }

  // Add the rest of the alternative names just in case:
  const auto noms = jsonObj["noms"].toArray();
  for(const auto &nom: std::as_const(noms)) {
    GameEntry alternative = game;
    alternative.title = nom.toObject()["text"].toString();
    if(!alternative.title.isEmpty() && alternative.title != game.title) {
      gameEntries.append(alternative);
    }
  }

}

void ScreenScraper::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  fetchGameResources(game, sharedBlobs, cache);
}

void ScreenScraper::getReleaseDate(GameEntry &game)
{
  game.releaseDate = StrTools::conformReleaseDate(getJsonText(jsonObj["dates"].toArray(), REGION));
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
  game.players = StrTools::conformPlayers(jsonObj["joueurs"].toObject()["text"].toString());
}

void ScreenScraper::getAges(GameEntry &game)
{
  QStringList ageBoards;
  ageBoards.append("PEGI");
  ageBoards.append("ESRB");
  ageBoards.append("SS");

  if(!jsonObj["classifications"].isArray())
    return;

  QJsonArray jsonAges = jsonObj["classifications"].toArray();

  for(const auto &ageBoard: std::as_const(ageBoards)) {
    for(int a = 0; a < jsonAges.size(); ++a) {
      if(jsonAges.at(a).toObject()["type"].toString() == ageBoard) {
        game.ages = StrTools::conformAges(jsonAges.at(a).toObject()["text"].toString());
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

  game.tags = StrTools::conformTags(game.tags.left(game.tags.length() - 2));
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

  game.franchises = StrTools::conformTags(game.franchises.left(game.franchises.length() - 2));
}

void ScreenScraper::getCover(GameEntry &game)
{
  QString url = "";
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     config->platform != "gameandwatch") {
    url = getJsonText(jsonObj["medias"].toArray(), REGION, QStringList({"flyer"}));
  } else {
    url = getJsonText(jsonObj["medias"].toArray(), REGION, QStringList({"box-2D", "box-2d", "flyer", "box-texture"}));
  }
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("2"); fflush(stdout);
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
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, QStringList({"ss"}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("3"); fflush(stdout);
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
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION, QStringList({"sstitle"/*, "wheel", "wheel-hd"*/}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("4"); fflush(stdout);
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
  QString url = getJsonText(jsonObj["medias"].toArray(), REGION,
                            QStringList({"fanart-hd", "fanart"/*, "support-2D", "support-2d",
                                         "support-texture", "marquee", "screenmarquee"*/}));
  if(!url.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("5"); fflush(stdout);
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
      QStringList({"box-2D-back", "box-2d-back", "support-2D", "support-2d", "support-texture"}));
  if(!url.isEmpty()) {
    bool moveOn = false;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      printf("6"); fflush(stdout);
      netComm->request(url);
      q.exec();
      QImage image;
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError) {
        int imgSize = netComm->getData().size();
        if(imgSize > MINARTSIZE && image.loadFromData(netComm->getData())) {
          if(imgSize > MINTEXTURESIZE) {
            game.textureData = netComm->getData();
          }
          moveOn = true;
        }
      }
      if(moveOn) break;
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
      printf("7"); fflush(stdout);
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
      printf("8"); fflush(stdout);
      netComm->request(url);
      q.exec();
      game.manualData = netComm->getData();
      // Make sure received the data is not empty and has a type
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         !contentType.isEmpty() && game.manualData.size() > 4096) {
        game.manualFormat = contentType.mid(contentType.indexOf("/") + 1,
                                            contentType.length() - contentType.indexOf("/") + 1);
        if(game.manualFormat.length()>4) {
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

QStringList ScreenScraper::getSearchNames(const QFileInfo &info)
{
  QStringList searchNames;

  if(config->useChecksum) {
    canonical = NameTools::getCanonicalData(info, true);
    if(!canonical.md5.isEmpty() || !canonical.sha1.isEmpty()) {
      searchNames.append("crc=" + canonical.crc +
                         "&md5=" + canonical.md5 +
                         "&sha1=" + canonical.sha1 +
                         "&romtaille=" + QString::number(canonical.size));
    }
  } else if(Platform::get().getFamily(config->platform) == "arcade") {
    searchNames.append("romnom=" + QUrl::toPercentEncoding(info.baseName(), "()"));
  } else {
    QStringList searchNamesRaw = AbstractScraper::getSearchNames(info);
    if(info.isSymbolicLink() && info.suffix() == "mame") {
      QFileInfo mameFile = info.symLinkTarget();
      searchNamesRaw.prepend(mameFile.baseName());
    }
    QListIterator<QString> listNames(searchNamesRaw);
    while(listNames.hasNext()) {
      // printf("%s\n", listNames.next().toStdString().c_str());
      searchNames.append("romnom=" + QUrl::toPercentEncoding(listNames.next(), "()"));
    }
  }

  return searchNames;
}

QString ScreenScraper::getJsonText(QJsonArray jsonArr, int attr, QStringList types)
{
  if(attr == NONE && !types.isEmpty()) {
    for(const auto &type: std::as_const(types)) {
      for(const auto &jsonVal: std::as_const(jsonArr)) {
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
    // Not using the config->regionPrios since they might have changed due to region autodetection.
    // So using temporary internal one instead.
    if(types.isEmpty()) {
      for(const auto &region: std::as_const(regionPrios)) {
        for(const auto &jsonVal: std::as_const(jsonArr)) {
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
      for(const auto &type: std::as_const(types)) {
        for(const auto &region: std::as_const(regionPrios)) {
          for(const auto &jsonVal: std::as_const(jsonArr)) {
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
      for(const auto &lang: std::as_const(config->langPrios)) {
        for(const auto &jsonVal: std::as_const(jsonArr)) {
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
      for(const auto &type: std::as_const(types)) {
        for(const auto &lang: std::as_const(config->langPrios)) {
          for(const auto &jsonVal: std::as_const(jsonArr)) {
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
