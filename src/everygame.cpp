/***************************************************************************
 *            everygame.cpp
 *
 *  Sun Aug 26 12:00:00 CEST 2018
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

#include <unistd.h>

#include <QList>
#include <QJsonArray>
#include <QTextStream>
#include <QStringList>
#include <QListIterator>

#include "everygame.h"
#include "platform.h"
#include "strtools.h"
#include "nametools.h"
#include "skyscraper.h"

constexpr int RETRIESMAX = 4;
constexpr int APICOOLDOWN = 21;

EveryGame::EveryGame(Settings *config,
                     QSharedPointer<NetManager> manager,
                     QString threadId,
                     NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;
  loadConfig("everygame.json", "code", "machine_types");

  baseUrl = "https://www.everygamegoing.com/";
  urlPost = "api/getAllItemIDs/index/machine_type_id/";

  platformId = getPlatformId(config->platform);
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by EveryGame or it hasn't yet been "
           "included in Skyscraper for this module...\033[0m\n");
    return;
  }

  QJsonArray jsonPlatforms;
  QFile platformsCache(config->dbPath + "/platforms.json");
  QString platformsUrl = "https://www.everygamegoing.com/api/getAllMachines/";
  if(!platformsCache.open(QIODevice::ReadOnly)) {
    printf("ERROR: Platforms file '%s' cannot be accessed. Please download it "
           "from '%s'. Exiting.\n",
           platformsCache.fileName().toStdString().c_str(),
           platformsUrl.toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  jsonDoc = QJsonDocument::fromJson(QString::fromUtf8(platformsCache.readAll()).toUtf8());
  platformsCache.close();
  if(jsonDoc.isNull()) {
    printf("\nERROR: The EveryGame platforms file is empty or corrupted. Please "
           "delete it and redownload from '%s' before re-executing this command.\n",
           platformsUrl.toStdString().c_str());
    reqRemaining = 0;
    return;
  } else {
    jsonPlatforms = jsonDoc.object()["machines"].toArray();
  }
  for(const auto &platform: std::as_const(jsonPlatforms)) {
    QString platformId = platform["machine_type_id"].toString();
    QString country = platform["country_group_name"].toString();
    if(!platformId.isEmpty() && !country.isEmpty()) {
      platformCountry[platformId] = country;
    }
  }

  QJsonArray jsonIndex;
  QFile listCache(config->dbPath + "/" + config->platform + ".json");
  if(!listCache.exists()) {
    if(!listCache.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
      printf("\nERROR: Cannot create the cache file %s. Exiting.\n",
             listCache.fileName().toStdString().c_str());
      reqRemaining = 0;
      return;
    }
    // Download the full list of games using the API for offline search:
    printf("INFO: Downloading EveryGame game list for the platform..."); fflush(stdout);
    QStringList machineList = platformId.split(',');
    for(const auto &machine: std::as_const(machineList)) {
      QString url = baseUrl + urlPost + machine;
      int errors = 0;
      QString queryStatus;
      while(errors <= RETRIESMAX) {
        netComm->request(url);
        q.exec();
        data = netComm->getData();
        // Debug:
        /*QTextStream out(stdout);
        out << url << Qt::endl << data << Qt::endl;*/
        jsonDoc = QJsonDocument::fromJson(data);
        if(jsonDoc.isEmpty()) {
          errors++;
          continue;
        }
        queryStatus = jsonDoc.object()["report"].toString();
        break;
      }
      if(queryStatus != "Success" || errors > RETRIESMAX) {
        printf("\n\033[1;31mERROR: EveryGame getAllItemIDs API error '%s'. Stopping...\033[0m\n",
               queryStatus.toStdString().c_str());
        reqRemaining = 0;
        return;
      }
      QJsonArray newItems = jsonDoc.object()["response"].toArray();
      for(auto item: std::as_const(newItems)) {
        QJsonObject newItem = item.toObject();
        newItem["platform"] = machine;
        jsonIndex.append(newItem);
      }
      printf("."); fflush(stdout);
    }
    listCache.write(QJsonDocument(jsonIndex).toJson(QJsonDocument::Indented));
    listCache.close();
  }

  if(!listCache.open(QIODevice::ReadOnly)) {
    printf("\nERROR: Database file %s cannot be accessed. Exiting.\n",
           listCache.fileName().toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  jsonDoc = QJsonDocument::fromJson(QString::fromUtf8(listCache.readAll()).toUtf8());
  listCache.close();
  if(jsonDoc.isNull()) {
    printf("\nERROR: The EveryGame cache file for the platform is empty or corrupted. "
           "Please delete it so that it can be regenerated and re-execute this command.\n");
    reqRemaining = 0;
    return;
  } else {
    jsonIndex = jsonDoc.array();
  }

  int totalEntries = 0;
  if(jsonIndex.isEmpty()) {
    printf("\n\033[1;31mERROR: No game could be found for the platform using the API. "
           "Exiting.\033[0m\n");
    reqRemaining = 0;
    return;
  } else {
    printf("INFO: Reading local cache file... "); fflush(stdout);
    while(!jsonIndex.isEmpty()) {
      QJsonObject jsonGame = jsonIndex.first().toObject();
      QString gameName = jsonGame["show_title"].toString();
      QString itemId = jsonGame["item_id"].toString();
      QString platform = jsonGame["platform"].toString();
      if(!gameName.isEmpty() && !itemId.isEmpty()) {
        totalEntries++;
        QStringList safeVariations, unsafeVariations;
        NameTools::generateSearchNames(gameName, safeVariations, unsafeVariations, true);
        const auto currentGame = qMakePair(gameName,
                                           qMakePair(itemId, platformCountry[platform]));
        for(const auto &name: std::as_const(safeVariations)) {
          if(!egPlatformMap.contains(name, currentGame)) {
            if(config->verbosity > 2) {
              printf("Adding full variation: %s -> %s\n",
                     gameName.toStdString().c_str(), name.toStdString().c_str());
            }
            egPlatformMap.insert(name, currentGame);
          }
        }
        for(const auto &name: std::as_const(unsafeVariations)) {
          if(!egPlatformMapTitle.contains(name, currentGame)) {
            if(config->verbosity > 2) {
              printf("Adding title variation: %s -> %s\n",
                     gameName.toStdString().c_str(), name.toStdString().c_str());
            }
            egPlatformMapTitle.insert(name, currentGame);
          }
        }
      }
      jsonIndex.removeFirst();
    }
    printf(" DONE.\nINFO: Read %d game entries from EveryGame database.\n", totalEntries);
  }

  developerPre.append("<td><b>Author(s)</b>:</td><td><a href = \"/lauthor/");
  developerPre.append("\">");
  developerPost = "</a>";
  agesPre.append("<td><b>Certificate</b>:</td>");
  agesPre.append("<td>");
  agesPost = "</td>";
  tagsPre.append("<span itemprop = \"genre\">");
  tagsPost = "</span>";
  ratingPre.append("Critic Score");
  ratingPost = "%";
  descriptionPre.append("clickThing( 'love', '/', '");
  descriptionPost = "'";
  coverPre.append("<h3>Cover Art</h3>");
  coverPre.append("<a onclick = \"popImage( '");
  coverPost = "' );\">";
  wheelPre.append("<h3>Screenshots</h3>");
  wheelPre.append("<a onclick = \"popImage( '");
  wheelPost = "' );\">";
  screenshotCounter = "";
  screenshotPre.append("<h3>Screenshots</h3>");
  screenshotPre.append("<a onclick = \"popImage( '");
  screenshotPre.append("<a onclick = \"popImage( '");
  screenshotPost = "' );\">";
  texturePre.append("<h3>Back Inlay</h3>");
  texturePre.append("<a onclick = \"popImage( '");
  texturePost = "' );\">";
  marqueePre.append("<h3>Media Scans</h3>");
  marqueePre.append("<a onclick = \"popImage( '");
  marqueePost = "' );\">";
  videoPre.append("<div class=\"video\">");
  videoPre.append("src=\"");
  videoPost = "\"";
  manualPre.append("\"#full-instructions\"");
  manualPre.append("<div class=\"tab-pane fade\" id=\"full-instructions\"");
  manualPre.append("<div id=\"full-instructions\"");
  manualPost = "<div class=\"tab-pane fade\"";
  guidesPre.append("\"#solutions\"");
  guidesPre.append("<div class=\"tab-pane fade\" id=\"solutions\"");
  guidesPre.append("<div id=\"solutions\"");
  guidesPost = "<div class=\"tab-pane fade\"";
  cheatsPre.append("\"#cheat-loaders\"");
  cheatsPre.append("<div class=\"tab-pane fade\" id=\"cheat-loaders\"");
  cheatsPre.append("<div id=\"cheat-loaders\"");
  cheatsPost = "<div class=\"tab-pane fade\"";
  reviewsPre.append("\"#critical-reaction\"");
  reviewsPre.append("<div id = \"critical-reaction\"");
  reviewsPre.append("<div id=\"critical-reaction\"");
  reviewsPost = "<div class=\"tab-pane fade\"";

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(RATING);
  fetchOrder.append(TAGS);
  fetchOrder.append(AGES);
  fetchOrder.append(COVER);
  fetchOrder.append(WHEEL);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(VIDEO);
  fetchOrder.append(MANUAL);
  fetchOrder.append(GUIDES);
  fetchOrder.append(CHEATS);
  fetchOrder.append(REVIEWS);
}

void EveryGame::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  if(config->verbosity >= 2) {
    qDebug() << "SearchName: " << searchName;
  }

  QList<GameEntry> gameEntriesRegion;
  QList<QPair<QString, QPair<QString, QString>>> matches;

  if(getSearchResultsOffline(matches, searchName, egPlatformMap, egPlatformMapTitle)) {
    for(const auto &gameDetail: std::as_const(matches)) {
      GameEntry game;
      game.title = gameDetail.first;
      game.id = gameDetail.second.first;
      game.platform = Platform::get().getAliases(config->platform).at(1);
      game.url = gameDetail.second.second;
      gameEntriesRegion.append(game);
    }
  }

  QStringList gfRegions;
  for(const auto &region: std::as_const(regionPrios)) {
    if(region == "eu" || region == "fr" || region == "de" || region == "it" ||
       region == "sp" || region == "se" || region == "nl" || region == "dk" ||
       region == "gr" || region == "no" || region == "ru") {
      gfRegions.append("Europe");
    } else if(region == "us" || region == "ca") {
      gfRegions.append("United States");
    } else if(region == "wor") {
      gfRegions.append("United States");
    } else if(region == "jp") {
      gfRegions.append("Japan");
    } else if(region == "kr") {
      gfRegions.append("Japan");
    } else if(region == "au"  || region == "uk" || region == "nz") {
      gfRegions.append("Europe");
    } else if(region == "asi" || region == "tw" || region == "cn") {
      gfRegions.append("Japan");
    } else if(region == "ame" || region == "br") {
      gfRegions.append("United States");
    } else {
      gfRegions.append("United States");
    }
  }
  gfRegions.append("");
  gfRegions.removeDuplicates();

  for(const auto &country: std::as_const(gfRegions)) {
    for(const auto &game: std::as_const(gameEntriesRegion)) {
      if(game.url == country) {
        gameEntries.append(game);
      }
    }
  }
}

void EveryGame::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  existingMedia = sharedBlobs;
  printf("Waiting to get game data..."); fflush(stdout);

  QString url = baseUrl + "api/getItemByItemID/index/item_id/" + game.id;
  int errors = 0;
  QString queryStatus;
  while(errors <= RETRIESMAX) {
    netComm->request(url);
    q.exec();
    data = netComm->getData();
    // Debug:
    /*QTextStream out(stdout);
    out << url << Qt::endl << data << Qt::endl;*/
    jsonDoc = QJsonDocument::fromJson(data);
    if(jsonDoc.isEmpty()) {
      errors++;
      continue;
    }
    queryStatus = jsonDoc.object()["report"].toString();
    break;
  }
  if(queryStatus != "Success" || errors > RETRIESMAX) {
    printf("\nERROR: EveryGame getItemByItemID API error '%s'. Skipping game...\n",
           queryStatus.toStdString().c_str());
    game.found = false;
    game.title = "";
    game.platform = "";
    game.url = "";
    return;
  }
  printf("."); fflush(stdout);
  jsonItem = jsonDoc.object()["response"].toObject();
  if(!jsonItem["machine_type_group_name"].toString().isEmpty()) {
    game.platform = jsonItem["machine_type_group_name"].toString();
  }

  game.url = baseUrl + "litem/" + jsonItem["item_title"].toString() + "/" + game.id;
  errors = 0;
  while(errors <= RETRIESMAX) {
    netComm->request(game.url);
    q.exec();
    game.miscData = netComm->getData();
    // Debug:
    /*QTextStream out(stdout);
    out << url << Qt::endl << data << Qt::endl;*/
    if(game.miscData.isEmpty() ||
       netComm->getError(config->verbosity) != QNetworkReply::NoError) {
      errors++;
      continue;
    }
    break;
  }
  if(errors > RETRIESMAX) {
    printf("\nERROR: EveryGame web retrieval error %s. Skipping game...\n",
           queryStatus.toStdString().c_str());
    game.found = false;
    game.title = "";
    game.platform = "";
    game.url = "";
    return;
  }
  printf(" DONE\n");

  data = game.miscData;
  AbstractScraper::getDescription(game);
  if(game.description.isEmpty()) {
    printf("WARNING: This game is a compilation. This feature is not yet supported"
           " by this scraper. Some fields may not be retrieved.\n");
    jsonThing = QJsonObject();
  } else {
    url = baseUrl + "api/getThingByThingID/index/thing_id/" + game.description;
    errors = 0;
    while(errors <= RETRIESMAX) {
      netComm->request(url);
      q.exec();
      data = netComm->getData();
      // Debug:
      /*QTextStream out(stdout);
      out << url << Qt::endl << data << Qt::endl;*/
      jsonDoc = QJsonDocument::fromJson(data);
      if(jsonDoc.isEmpty()) {
        errors++;
        continue;
      }
      queryStatus = jsonDoc.object()["report"].toString();
      break;
    }
    if(queryStatus != "Success" || errors > RETRIESMAX) {
      printf("\nERROR: EveryGame getThingByThingID API error '%s'. Skipping game...\n",
             queryStatus.toStdString().c_str());
      game.found = false;
      game.title = "";
      game.platform = "";
      game.url = "";
      return;
    }
    jsonThing = jsonDoc.object()["response"].toObject();
    game.description = "";
  }

  fetchGameResources(game, sharedBlobs, cache);
}

void EveryGame::getReleaseDate(GameEntry &game)
{
  game.releaseDate = StrTools::conformReleaseDate(jsonItem["release_date"].toString());
}

void EveryGame::getTags(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getTags(game);
  if(game.tags == "Unknown Genre Type") {
    game.tags = "";
  } else {
    game.tags = StrTools::conformTags(game.tags.replace(' ', ','));
  }
}

void EveryGame::getAges(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getAges(game);
}

void EveryGame::getPublisher(GameEntry &game)
{
  game.publisher = jsonItem["publisher_info"].toString();
}

void EveryGame::getDeveloper(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getDeveloper(game);
}

void EveryGame::getRating(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getRating(game);
  if(!game.rating.isEmpty()) {
    game.rating = QString::number(game.rating.toDouble() / 20.0);
  }
}

void EveryGame::getDescription(GameEntry &game)
{
  if(!jsonThing.isEmpty()) {
    game.description = jsonThing["thing_descrption"].toString();
  }
}

void EveryGame::getCover(GameEntry &game)
{
  if(jsonItem["covs"].toInt() > 0) {
    data = game.miscData;
    AbstractScraper::getCover(game);
  }
}

void EveryGame::getMarquee(GameEntry &game)
{
  if(jsonItem["media_scans"].toInt() > 0) {
    data = game.miscData;
    AbstractScraper::getMarquee(game);
  }
}

void EveryGame::getWheel(GameEntry &game)
{
  // We are optimistic in case of compilations...
  if(jsonThing.isEmpty() || jsonThing["ills"].toString().toInt() > 0) {
    data = game.miscData;
    AbstractScraper::getWheel(game);
  }
}

void EveryGame::getScreenshot(GameEntry &game)
{
  // We are optimistic in case of compilations...
  int counter = 3;
  if(!jsonThing.isEmpty()) {
    counter = jsonThing["ills"].toString().toInt();
  }
  if(counter < 2) {
    return;
  } else if(counter > 2) {
    screenshotPre.append(screenshotPre.last());
  }
  data = game.miscData;
  AbstractScraper::getScreenshot(game);
  if(counter > 2) {
    screenshotPre.removeLast();
  }
}

void EveryGame::getTexture(GameEntry &game)
{
  QString temp;
  if(jsonItem["inlal_back"].toInt() == 0 && jsonItem["inlal_front"].toInt() > 0) {
    temp = texturePre.takeFirst();
    texturePre.prepend("<h3>Front Inlay</h3>");
  } else if(jsonItem["inlal_back"].toInt() == 0 && jsonItem["inlal_front"].toInt() == 0) {
    return;
  }
  data = game.miscData;
  AbstractScraper::getTexture(game);
  if(!temp.isEmpty()) {
    texturePre.removeFirst();
    texturePre.prepend(temp);
  }
}

void EveryGame::getVideo(GameEntry &game)
{
  if(videoPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(videoPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(videoPre)) {
    nomNom(nom);
  }
  QString videoUrl = data.left(data.indexOf(videoPost.toUtf8())).replace("&amp;", "&");
  getOnlineVideo(videoUrl, game);
}

void EveryGame::getManual(GameEntry &game)
{
  data = game.miscData;
  int startPos = data.indexOf(manualPre.at(0).toUtf8());
  if(startPos < 0) {
    return;
  }
  startPos = data.indexOf(manualPre.at(0).toUtf8());
  if(startPos < 0) {
    startPos = data.indexOf(manualPre.at(1).toUtf8());
  }
  if(startPos > 0) {
    int endPos = data.indexOf(manualPost.toUtf8(), startPos + 1);
    if(endPos > 0) {
      QByteArray htmlManual = data.mid(startPos, endPos - startPos);
      game.manualData = "<!DOCTYPE html><html><head><title>Instructions for " +
                        game.title.toUtf8() + "</title></head><body>\n" +
                        htmlManual + "\n</body></html>\n";
      game.manualData.replace("\"/txt/",
                              "\"https://www.everygamegoing.com/txt/");
      game.manualFormat = "html";
    }
  }
}

void EveryGame::getGuides(GameEntry &game)
{
  data = game.miscData;
  int startPos = data.indexOf(guidesPre.at(0).toUtf8());
  if(startPos < 0) {
    return;
  }
  startPos = data.indexOf(guidesPre.at(1).toUtf8());
  if(startPos < 0) {
    startPos = data.indexOf(guidesPre.at(2).toUtf8());
  }
  if(startPos > 0) {
    int endPos = data.indexOf(guidesPost.toUtf8(), startPos + 1);
    if(endPos > 0) {
      QString htmlGuide = data.mid(startPos, endPos - startPos);
      htmlGuide = "<!DOCTYPE html><html><head><title>Solution for " + game.title +
                  "</title></head><body>\n" + htmlGuide + "\n</body></html>\n";
      htmlGuide.replace("\"/litem/", "\"https://www.everygamegoing.com/litem/");
      htmlGuide.replace("\"%UP_PATH%", "\"https://www.everygamegoing.com/");
      game.guides = config->docsPath + "/everygamegoing.com/" + "guide_" +
                    config->platform + "_" + jsonItem["item_title"].toString() +
                    ".html";
      QFile guideFile(game.guides);
      if(!guideFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        printf("\nERROR: Cannot create the guide file %s. Skipping.\n",
               guideFile.fileName().toStdString().c_str());
        game.guides = "";
      } else {
        guideFile.write(htmlGuide.toUtf8());
        guideFile.close();
      }
    }
  }
}

void EveryGame::getCheats(GameEntry &game)
{
  data = game.miscData;
  int startPos = data.indexOf(cheatsPre.at(0).toUtf8());
  if(startPos < 0) {
    return;
  }
  startPos = data.indexOf(cheatsPre.at(1).toUtf8());
  if(startPos < 0) {
    startPos = data.indexOf(cheatsPre.at(2).toUtf8());
  }
  if(startPos > 0) {
    int endPos = data.indexOf(cheatsPost.toUtf8(), startPos + 1);
    if(endPos > 0) {
      QString htmlCheats = data.mid(startPos, endPos - startPos);
      htmlCheats = "<!DOCTYPE html><html><head><title>Cheats for " + game.title +
                   "</title></head><body>\n" + htmlCheats + "\n</body></html>\n";
      htmlCheats.replace("\"/litem/", "\"https://www.everygamegoing.com/litem/");
      game.cheats = config->docsPath + "/everygamegoing.com/" + "cheats_" +
                    config->platform + "_" + jsonItem["item_title"].toString() +
                    ".html";
      QFile cheatsFile(game.cheats);
      if(!cheatsFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        printf("\nERROR: Cannot create the cheats file %s. Skipping.\n",
               cheatsFile.fileName().toStdString().c_str());
        game.cheats = "";
      } else {
        cheatsFile.write(htmlCheats.toUtf8());
        cheatsFile.close();
      }
    }
  }
}

void EveryGame::getReviews(GameEntry &game)
{
  data = game.miscData;
  int startPos = data.indexOf(reviewsPre.at(0).toUtf8());
  if(startPos < 0) {
    return;
  }
  startPos = data.indexOf(reviewsPre.at(1).toUtf8());
  if(startPos < 0) {
    startPos = data.indexOf(reviewsPre.at(2).toUtf8());
  }
  if(startPos > 0) {
    int endPos = data.indexOf(reviewsPost.toUtf8(), startPos + 1);
    if(endPos > 0) {
      QString htmlReview = data.mid(startPos, endPos - startPos);
      htmlReview = "<!DOCTYPE html><html><head><title>Reviews for " + game.title +
                   "</title></head><body>\n" + htmlReview + "\n</body></html>\n";
      htmlReview.replace("\"/larticle/", "\"https://www.everygamegoing.com/larticle/");
      game.reviews = config->docsPath + "/everygamegoing.com/" + "reviews_" +
                     config->platform + "_" + jsonItem["item_title"].toString() +
                     ".html";
      QFile reviewsFile(game.reviews);
      if(!reviewsFile.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) {
        printf("\nERROR: Cannot create the reviews file %s. Skipping.\n",
               reviewsFile.fileName().toStdString().c_str());
        game.reviews = "";
      } else {
        reviewsFile.write(htmlReview.toUtf8());
        reviewsFile.close();
      }
    }
  }
}
