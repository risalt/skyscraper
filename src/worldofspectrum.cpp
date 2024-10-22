/***************************************************************************
 *            worldofspectrum.cpp
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

#include <QProcess>
#include <QByteArray>
#include <QTemporaryFile>

#include "worldofspectrum.h"
#include "platform.h"
#include "strtools.h"
#include "nametools.h"

WorldOfSpectrum::WorldOfSpectrum(Settings *config,
                                 QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  baseUrl = "https://worldofspectrum.net";

  searchResultPre = "<article class=\"item tease tease-item\">";
  urlPre.append("Full title");
  urlPre.append("<a href=\"");
  urlPost = "\" ";
  titlePre.append("title=\"Get direct link to this entry\">");
  titlePost = "</a>";
  platformPre.append("<dt>Machine type</dt>\n<dd>");
  platformPost = "</dd>";

  releaseDatePre.append("<table class=\"item-releases\">");
  releaseDatePre.append("Original release");
  releaseDatePre.append("<td>");
  releaseDatePost = "</td>";
  publisherPre.append("<dt>Publisher(s)</dt>");
  publisherPre.append("<dd>");
  publisherPre.append("<a href=\"");
  publisherPre.append("\">");
  publisherPost = "</a>";
  developerPre.append("<dt>Author(s)</dt>");
  developerPre.append("<dd>");
  developerPre.append("<a href=\"");
  developerPre.append("\">");
  developerPost = "</a>";
  playersPre.append("<dt>Number of players</dt>");
  playersPre.append("<dd>");
  playersPost = "</dd>";
  tagsPre.append("<dt>Genre</dt>");
  tagsPre.append("<dd>");
  tagsPost = "</dd>";
  /* screenshotPre.append("<div class=\"item-picture\" style=\"background-image: url('");
  screenshotPost = "');"; */
  coverPre.append("<table class=\"item-downloads\">");
  coverPre.append("<tbody>");
  coverPost = "</tbody>";
  /* descriptionPre.append("Additional info");
  descriptionPre.append("<FONT");
  descriptionPre.append("\">");
  descriptionPost = "</FONT>"; */

  fetchOrder.append(TAGS);
  fetchOrder.append(PUBLISHER);
  // fetchOrder.append(DEVELOPER);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(WHEEL);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(MANUAL);
  fetchOrder.append(VIDEO);
}

void WorldOfSpectrum::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString platform)
{
  // searchName = searchName.replace("the+", "");
  // Match English, German, French, Spanish articles:
  // Exceptions: "Las Vegas", "Die Hard"
  searchName = NameTools::removeArticle(searchName, "+");
  QString url = "https://worldofspectrum.net/infoseek/";
  netComm->request(url + "?regexp=" + searchName + "&model=spectrum&loadpics=3");
  // qDebug() << url + "?regexp=" + searchName + "&model=spectrum&loadpics=3";
  q.exec();
  data = netComm->getData();
  GameEntry game;

  while(data.indexOf(searchResultPre.toUtf8()) != -1) {
    nomNom(searchResultPre);

    // Digest until url
    for(const auto &nom: urlPre) {
      nomNom(nom);
    }
    game.url = data.left(data.indexOf(urlPost.toUtf8()));

    // Digest until title
    for(const auto &nom: titlePre) {
      nomNom(nom);
    }
    game.title = data.left(data.indexOf(titlePost.toUtf8()));

    // Always move ", The" to the beginning of the name
    // Three digit articles in English, German, French, Spanish:
    game.title = NameTools::moveArticle(game.title, true);

    // Digest until url
    for(const auto &nom: platformPre) {
      nomNom(nom);
    }
    QString platformGame = data.left(data.indexOf(platformPost.toUtf8()));

    game.platform = Platform::get().getAliases(platform).at(1);

    if(platformMatch(platformGame, platform)) {
      gameEntries.append(game);
    }
  }
}

void WorldOfSpectrum::getCover(GameEntry &game)
{
  for(const auto &nom: coverPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: coverPre) {
    nomNom(nom);
  }
  QString downloads = data.left(data.indexOf(screenshotPost.toUtf8()));

  while(data.indexOf("<tr>") != -1) {
    nomNom("<td>"); nomNom("<td>");
    nomNom("<a href=\"");
    QString url = data.left(data.indexOf("\" "));
    nomNom("<td>"); nomNom("<td>");
    QString type = data.left(data.indexOf("</td>")).toLower();
    if(type.contains("instructions") && url.endsWith(".pdf", Qt::CaseInsensitive)) {
      type = "manual";
    } else if(type.contains("instructions") && url.endsWith(".txt", Qt::CaseInsensitive)) {
      type = "description";
    } else if(type.contains("poster") || 
              type.contains("advertisement") ||
              type.contains("sketches") ||
              type.contains("photo") ||
              type.contains("sprite") ||
              type.contains("game additional material") ||
              type.contains("artwork")) {
      type = "media";
    }
    if(!resources.contains(type)) {
      resources[type] = url;
    }
  }
  
  QString url = resources["inlay - front"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.coverData = netComm->getData();
    }
  }
}

void WorldOfSpectrum::getTexture(GameEntry &game)
{
  QString url = resources["inlay - back"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
    }
  }
}

void WorldOfSpectrum::getMarquee(GameEntry &game)
{
  QString url = resources["media"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}

void WorldOfSpectrum::getWheel(GameEntry &game)
{
  QString url = resources["title screen"];
  if(url.isEmpty()) {
    url = resources["loading screen"];
  }
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();

    QImage image;
    if(netComm->getError() == QNetworkReply::NoError) {
      if(!url.endsWith(".gif", Qt::CaseInsensitive) &&
         !url.endsWith(".jpg", Qt::CaseInsensitive) &&
         !url.endsWith(".png", Qt::CaseInsensitive) &&
         !url.endsWith(".jpeg", Qt::CaseInsensitive)) {
        // Convert legacy image format to something modern:
        QString extension = url.split('.').last();
        QString tempFile;
        {
          QTemporaryFile imageFile;
          imageFile.setAutoRemove(false);
          imageFile.setFileTemplate("convertsk_XXXXXXX." + extension);
          imageFile.open();
          imageFile.write(netComm->getData());
          tempFile = imageFile.fileName();
        }
        if(!tempFile.isEmpty()) {
          QProcess converter;
          QString command("recoil2png");
          QStringList commandArgs = { tempFile };
          converter.start(command, commandArgs, QIODevice::ReadOnly);
          converter.waitForFinished(10000);
          converter.close();
          tempFile.chop(extension.size());
          QFile imageFile(tempFile + "png");
          if(imageFile.open(QIODevice::ReadOnly)) {
            game.wheelData = imageFile.readAll();
            if(!image.loadFromData(game.wheelData)) {
              game.wheelData = "";
              printf("ERROR: Conversion has generated a corrupt image\n");
            }
          } else {
            printf("ERROR: Error %d accessing the temporary file.\n", imageFile.error());
          }
          imageFile.remove();
          QFile::remove(tempFile + extension);
        } else {
          printf("ERROR: Error when creating a temporary file to download a video.\n");
        }
      } else if(image.loadFromData(netComm->getData())) {
        game.wheelData = netComm->getData();
      }
    }
  }
}

void WorldOfSpectrum::getScreenshot(GameEntry &game)
{
  QString url = resources["running screen"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();

    QImage image;
    if(netComm->getError() == QNetworkReply::NoError) {
      if(!url.endsWith(".gif", Qt::CaseInsensitive) &&
         !url.endsWith(".jpg", Qt::CaseInsensitive) &&
         !url.endsWith(".png", Qt::CaseInsensitive) &&
         !url.endsWith(".jpeg", Qt::CaseInsensitive)) {
        // Convert legacy image format to something modern:
        QString extension = url.split('.').last();
        QString tempFile;
        {
          QTemporaryFile imageFile;
          imageFile.setAutoRemove(false);
          imageFile.setFileTemplate("convertsk_XXXXXXX." + extension);
          imageFile.open();
          imageFile.write(netComm->getData());
          tempFile = imageFile.fileName();
        }
        if(!tempFile.isEmpty()) {
          QProcess converter;
          QString command("recoil2png");
          QStringList commandArgs = { tempFile };
          converter.start(command, commandArgs, QIODevice::ReadOnly);
          converter.waitForFinished(10000);
          converter.close();
          tempFile.chop(extension.size());
          QFile imageFile(tempFile + "png");
          if(imageFile.open(QIODevice::ReadOnly)) {
            game.screenshotData = imageFile.readAll();
            if(!image.loadFromData(game.screenshotData)) {
              game.screenshotData = "";
              printf("ERROR: Conversion has generated a corrupt image\n");
            }
          } else {
            printf("ERROR: Error %d accessing the temporary file.\n", imageFile.error());
          }
          imageFile.remove();
        } else {
          printf("ERROR: Error when creating a temporary file to download a video.\n");
        }
      } else if(image.loadFromData(netComm->getData())) {
        game.screenshotData = netComm->getData();
      }
    }
  }
}

void WorldOfSpectrum::getManual(GameEntry &game)
{
  QString url = resources["manual"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    game.manualData = netComm->getData();
    QByteArray contentType = netComm->getContentType();
    if(netComm->getError() == QNetworkReply::NoError &&
       !contentType.isEmpty() && game.manualData.size() > 4096) {
      game.manualFormat = contentType.mid(contentType.indexOf("/") + 1,
                                          contentType.length() - contentType.indexOf("/") + 1);
      if (game.manualFormat.length()>4) {
        game.manualFormat = "pdf";
      }
    } else {
        game.manualData = "";
    }
  }
}

void WorldOfSpectrum::getVideo(GameEntry &game)
{
  QString url = resources["video"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    game.videoData = netComm->getData();
    QByteArray contentType = netComm->getContentType();
    if(netComm->getError() == QNetworkReply::NoError &&
       !contentType.contains("video/") && game.videoData.size() > 4096) {
      game.videoFormat = contentType.mid(contentType.indexOf("/") + 1,
                                         contentType.length() - contentType.indexOf("/") + 1);
    } else {
      game.videoData = "";
    }
  }
}

void WorldOfSpectrum::getDescription(GameEntry &game)
{
  QString url = resources["description"];
  if(!url.isEmpty()) {
    if(url.indexOf("http") != -1) {
      netComm->request(url);
    } else {
      netComm->request(baseUrl + (url.left(1) == "/"?"":"/") + url);
    }
    q.exec();
    if(netComm->getError() == QNetworkReply::NoError) {
      game.description = netComm->getData();
    }
  }
}

void WorldOfSpectrum::getReleaseDate(GameEntry &game)
{
  for(const auto &nom: releaseDatePre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: releaseDatePre) {
    nomNom(nom);
  }
  game.releaseDate = data.left(data.indexOf(releaseDatePost.toUtf8())).simplified();

  bool isInt = true;
  game.releaseDate.toInt(&isInt);
  if(!isInt) {
    game.releaseDate = "";
  }
}

void WorldOfSpectrum::getTags(GameEntry &game)
{
  if(tagsPre.isEmpty()) {
    return;
  }
  for(const auto &nom: tagsPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: tagsPre) {
    nomNom(nom);
  }
  game.tags = data.left(data.indexOf(tagsPost.toUtf8()));
  if(game.tags.contains(": ")) {
    game.tags.remove(0, game.tags.indexOf(": ") + 2);
  }
}
