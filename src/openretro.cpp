/***************************************************************************
 *            openretro.cpp
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

#include "openretro.h"
#include "nametools.h"
#include "strtools.h"
#include "platform.h"
#include "skyscraper.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

OpenRetro::OpenRetro(Settings *config,
                     QSharedPointer<NetManager> manager,
                     QString threadId,
                     NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  loadConfig("openretro.json", "platform_name", "platform_id");

  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by OpenRetro, see file "
           "'openretro.json'.\033[0m\n");
    return;
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(500); // OpenRetro seems to get tired after a few minutes of high-speed scraping...
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://openretro.org";

  searchUrlPre = "https://openretro.org";
  searchUrlPost = "&disabled=1&unpublished=1";

  searchResultPre = "<div style='margin-bottom: 4px;'>";
  urlPre.append("<a href=\"/");
  urlPost = "\" target='_parent'>";
  titlePre.append("</div>");
  titlePost = " <span";
  platformPre.append("#aaaaaa'>");
  platformPost = "</span>";
  marqueePre.append(">banner_sha1</td>");
  marqueePre.append("<div><a href=\"");
  marqueePost = "\">";
  // Check getDescription function for special case __long_description scrape
  // For description
  descriptionPre.append(">description</td>");
  descriptionPre.append("<td style='color: black;'><div>");
  // For __long_description
  descriptionPre.append("__long_description</td>");
  descriptionPre.append("<td style='color: black;'><div>");
  descriptionPost = "</div></td>";
  developerPre.append("black;'>developer</td>");
  developerPre.append("<td style='color: black;'><div>");
  developerPost = "</div></td>";
  coverPre.append(">front_sha1</td>");
  coverPre.append("<div><a href=\"");
  coverPost = "\">";
  wheelPre.append(">title_sha1</td>");
  wheelPre.append("<div><a href=\"");
  wheelPost = "\">";
  texturePre.append(">__back_sha1</td>");
  texturePre.append("<div><a href=\"");
  texturePost = "\">";
  playersPre.append(">players</td>");
  playersPre.append("<td style='color: black;'><div>");
  playersPost = "</div></td>";
  publisherPre.append(">publisher</td>");
  publisherPre.append("<td style='color: black;'><div>");
  publisherPost = "</div></td>";
  screenshotCounter = "<td style='width: 180px; color: black;'>screen";
  screenshotPre.append("<td style='width: 180px; color: black;'>screen");
  screenshotPre.append("<td style='color: black;'><div><a href=\"");
  screenshotPost = "\">";
  releaseDatePre.append(">year</td>");
  releaseDatePre.append("<td style='color: black;'><div>");
  releaseDatePost = "</div></td>";
  tagsPre.append(">tags</td>");
  ratingPre.append("<div class='game-review'>");
  ratingPre.append("<span class='score'>");
  ratingPre.append("<div class='score'>Score: ");
  ratingPost = "</";
  manualPre.append("var doc_sha1 = \"");
  manualPre.append("var page_size = ");
  manualPost = ";";

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(COVER);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(TAGS);
  fetchOrder.append(WHEEL);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(RATING);
  fetchOrder.append(MANUAL);
}

void OpenRetro::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString platform)
{
  bool hasWhdlUuid = searchName.left(6) == "/game/";
  QString lookupReq = QString(searchUrlPre);
  QString finalSearchName = StrTools::simplifyLetters(searchName);
  if(hasWhdlUuid) {
    lookupReq = lookupReq + searchName;
  } else {
    if(restrictSearch && finalSearchName.split(" ").size() > 3) {
      QStringList splitSearchName = finalSearchName.split(" ");
      finalSearchName = "";
      int i = 0;
      int numWords = 0;
      while(i < splitSearchName.size() && numWords < 3) {
        QString word = StrTools::sanitizeName(splitSearchName.at(i), true);
        if(!word.isEmpty()) {
          numWords++;
          finalSearchName.append(word + " ");
        }
        i++;
      }
      finalSearchName = finalSearchName.simplified();
    }
    lookupReq = lookupReq + "/browse/" + platformId + "?q=" + finalSearchName + searchUrlPost;
  }
  limiter.exec();
  netComm->request(lookupReq);
  q.exec();

  GameEntry game;

  while(!netComm->getRedirUrl().isEmpty()) {
    game.url = netComm->getRedirUrl();
    limiter.exec();
    netComm->request(game.url);
    q.exec();
  }

  if(netComm->getError() == QNetworkReply::NoError) {
    searchError = false;
  } else {
    printf("Connection error. Is the API down?\n");
    searchError = true;
    return;
  }
  data = netComm->getData();
  if(data.isEmpty()) {
    searchError = true;
    return;
  }

  if(data.contains("Error: 500 Internal Server Error") ||
     data.contains("Error: 404 Not Found")) {
    searchError = true;
    return;
  }

  if(data.contains("Sorry, max three search terms")) {
    if(finalSearchName.split(" ").size() > 3) {
      restrictSearch = true;
      getSearchResults(gameEntries, searchName, platform);
    }
    return;
  }

  if(hasWhdlUuid) {
    QByteArray tempData = data;
    nomNom("<td style='width: 180px; color: black;'>game_name</td>");
    nomNom("<td style='color: black;'><div>");
    // Remove AGA, we already add this automatically in StrTools::addSqrBrackets
    game.title = data.left(data.indexOf("</div></td>")).replace("[AGA]", "").
      replace("[CD32]", "").
      replace("[CDTV]", "").simplified();
    data = tempData;
    game.platform = platform;
    // Check if title is empty. Some games exist but have no data, not even a name. We don't want those results
    if(!game.title.isEmpty())
      gameEntries.append(game);
  } else {
    while(data.indexOf(searchResultPre.toUtf8()) != -1) {
      nomNom(searchResultPre);

      // Digest until url
      for(const auto &nom: std::as_const(urlPre)) {
        nomNom(nom);
      }
      game.url = baseUrl + "/" + data.left(data.indexOf(urlPost.toUtf8())) + "/edit";

      // Digest until title
      for(const auto &nom: std::as_const(titlePre)) {
        nomNom(nom);
      }
      // Remove AGA, we already add this automatically in StrTools::addSqrBrackets
      game.title = data.left(data.indexOf(titlePost.toUtf8())).replace("[AGA]", "").simplified();

      // Digest until platform (not needed anymore, as we filter by platform during the search)
      for(const auto &nom: std::as_const(platformPre)) {
        nomNom(nom);
      }
      game.platform = data.left(data.indexOf(platformPost.toUtf8())).replace("&nbsp;", " ");

      gameEntries.append(game);
    }
  }
}

void OpenRetro::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if(!game.url.isEmpty()) {
    limiter.exec();
    netComm->request(game.url);
    q.exec();
    data = netComm->getData();
  }
  game.id = game.url;
  game.id.chop(5);
  game.id = game.id.mid(game.id.lastIndexOf('/') + 1);
  // Remove all the variants so we don't choose between their screenshots
  data = data.left(data.indexOf("</table></div><div id='"));

  fetchGameResources(game, sharedBlobs, cache);
}

void OpenRetro::getDescription(GameEntry &game)
{
  if(descriptionPre.isEmpty()) {
    return;
  }
  QByteArray tempData = data;

  if(data.indexOf(descriptionPre.at(0).toUtf8()) != -1) {
    // If description
    nomNom(descriptionPre.at(0));
    nomNom(descriptionPre.at(1));
    game.description = data.left(data.indexOf(descriptionPost.toUtf8()));
    // Revert data back to pre-description
    data = tempData;
  }
  if(data.indexOf(descriptionPre.at(2).toUtf8()) != -1) {
    // If __long_description
    nomNom(descriptionPre.at(2));
    nomNom(descriptionPre.at(3));
    game.description += '\n';
    game.description += data.left(data.indexOf(descriptionPost.toUtf8()));
    // Revert data back to pre-description
    data = tempData;
  } else {
    return;
  }

  // Remove all html tags within description
  game.description.replace("&lt;", "<").replace("&gt;", ">");
  game.description = StrTools::stripHtmlTags(game.description);
}

void OpenRetro::getTags(GameEntry &game)
{
  for(const auto &nom: std::as_const(tagsPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(tagsPre)) {
    nomNom(nom);
  }
  QString tags = "";
  QString tagBegin = "<a href=\"/browse/";
  while(data.indexOf(tagBegin.toUtf8()) != -1) {
    nomNom(tagBegin);
    nomNom("\">");
    tags.append(data.left(data.indexOf("</a>")) + ", ");
  }
  if(!tags.isEmpty()) {
    tags.chop(2); // Remove last ", "
  }

  game.tags = StrTools::conformTags(tags);
}

void OpenRetro::getManual(GameEntry &game)
{
  limiter.exec();
  netComm->request(game.url + "/docs");
  q.exec();
  data = netComm->getData();

  if(data.isEmpty() ||
     data.contains("Error: 500 Internal Server Error") ||
     data.contains("Error: 404 Not Found")) {
    return;
  }

  QString docId, pageSize;
  if(checkNom(manualPre.at(0))) {
    nomNom(manualPre.at(0));
    docId = data.left(data.indexOf("\""));
    if(checkNom(manualPre.at(1))) {
      nomNom(manualPre.at(1));
      pageSize = data.left(data.indexOf(manualPost.toUtf8()));
    } else {
      printf("ERROR: (2) OpenRetro docs interface may have changed!\n");
    }
  } else {
    printf("ERROR: (1) OpenRetro docs interface may have changed!\n");
  }
  if(!pageSize.isEmpty() && !docId.isEmpty()) {
    QString manualUrl = baseUrl + "/image/" + docId + "?s=" + pageSize + "&f=pdf";
    limiter.exec();
    netComm->request(manualUrl);
    q.exec();
    if(netComm->getError() == QNetworkReply::NoError) {
      game.manualData = netComm->getData();
      game.manualFormat = "pdf";
    }
  }
}

void OpenRetro::getRating(GameEntry &game)
{
  game.url.chop(5); // remove trailing '/edit'
  limiter.exec();
  netComm->request(game.url);
  q.exec();
  data = netComm->getData();

  bool ratingDecimal = true;

  if(checkNom(ratingPre.at(0))) {
    // "0 ... 100%" or "n/d" (from mags/ext. websites)
    nomNom(ratingPre.at(0));
    nomNom(ratingPre.at(1));
    ratingDecimal = false;
  } else if(checkNom(ratingPre.at(2))) {
    // 1.0 ... 10.0 (from Openretro)
    nomNom(ratingPre.at(2));
  } else {
    game.rating = "";
    return;
  }

  bool toDoubleOk = false;
  game.rating = data.left(data.indexOf(ratingPost.toUtf8()));

  if(!ratingDecimal) {
    if(game.rating.endsWith("%")) {
      game.rating.chop(1);
      game.rating = QString::number(game.rating.toDouble()/100.0);
    } else if(game.rating.contains("/")) {
      QStringList parts = game.rating.split("/");
      double num = parts.value(0).toDouble(&toDoubleOk);
      if(toDoubleOk) {
        double den = parts.value(1).toDouble(&toDoubleOk);
        if(toDoubleOk && den > 0.0) {
          game.rating = QString::number(num/den);
          return;
        }
      }
      game.rating = "";
    } else {
      game.rating = "";
    }
  }

  double rating = game.rating.toDouble(&toDoubleOk);
  if(toDoubleOk) {
    game.rating = QString::number(rating / (ratingDecimal ? 10.0 : 1.0));
  } else {
    game.rating = "";
  }
}

void OpenRetro::getScreenshot(GameEntry &game)
{
  if(screenshotPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(screenshotPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(screenshotPre)) {
    nomNom(nom);
  }
  QString screenshotUrl = data.left(data.indexOf(screenshotPost.toUtf8())).replace("&amp;", "&");
  if(screenshotUrl.left(4) != "http") {
    screenshotUrl.prepend(baseUrl + (screenshotUrl.left(1) == "/"?"":"/"));
  }
  limiter.exec();
  netComm->request(screenshotUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

QStringList OpenRetro::getSearchNames(const QFileInfo &info)
{
  QStringList searchNames = AbstractScraper::getSearchNames(info);

  if(Platform::get().getFamily(config->platform) == "amiga") {
    // Pass 1 is uuid from whdload_db.xml
    QString baseName = info.completeBaseName();
    if(!config->whdLoadMap[baseName].second.isEmpty()) {
      searchNames.prepend("/game/" + config->whdLoadMap[baseName].second);
    }
  }

  return searchNames;
}
