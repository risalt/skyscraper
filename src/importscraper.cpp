/***************************************************************************
 *            importscraper.cpp
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
#include <QRegularExpression>

#include "importscraper.h"

ImportScraper::ImportScraper(Settings *config,
                             QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  fetchOrder.append(TITLE);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(VIDEO);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(AGES);
  fetchOrder.append(RATING);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(MANUAL);
  fetchOrder.append(GUIDES);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(TRIVIA);
  fetchOrder.append(VGMAPS);

  covers = QDir(config->importFolder + "/covers", "*.*",
                QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  screenshots = QDir(config->importFolder + "/screenshots", "*.*",
               QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  wheels = QDir(config->importFolder + "/wheels", "*.*",
                QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  marquees = QDir(config->importFolder + "/marquees", "*.*",
                  QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  textures = QDir(config->importFolder + "/textures", "*.*",
                  QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  videos = QDir(config->importFolder + "/videos", "*.*",
                QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  manuals = QDir(config->importFolder + "/manuals", "*.*",
                QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  textual = QDir(config->importFolder + "/textual", "*.*",
                 QDir::Name, QDir::Files | QDir::NoDotAndDotDot).entryInfoList();
  loadDefinitions();
}

void ImportScraper::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  // Always reset game title at this point, to avoid saving the dummy title in cache
  game.title = "";

  loadData();
  QByteArray dataOrig = data;

  fetchGameResources(game, sharedBlobs, cache);
}

void ImportScraper::runPasses(QList<GameEntry> &gameEntries, const QFileInfo &info, QString &, QString &)
{
  data = "";
  textualFile = "";
  screenshotFile = "";
  coverFile = "";
  wheelFile = "";
  marqueeFile = "";
  textureFile = "";
  videoFile = "";
  manualFile = "";
  GameEntry game;
  bool textualFound = checkType(info.completeBaseName(), textual, textualFile);
  bool screenshotFound = checkType(info.completeBaseName(), screenshots, screenshotFile);
  bool coverFound = checkType(info.completeBaseName(), covers, coverFile);
  bool wheelFound = checkType(info.completeBaseName(), wheels, wheelFile);
  bool marqueeFound = checkType(info.completeBaseName(), marquees, marqueeFile);
  bool textureFound = checkType(info.completeBaseName(), textures, textureFile);
  bool videoFound = checkType(info.completeBaseName(), videos, videoFile);
  bool manualFound = checkType(info.completeBaseName(), manuals, manualFile);
  if(textualFound || screenshotFound || coverFound || wheelFound || marqueeFound ||
     textureFound || videoFound || manualFound) {
    game.title = info.completeBaseName();
    game.platform = config->platform;
    gameEntries.append(game);
  }
}

QString ImportScraper::getCompareTitle(QFileInfo info)
{
  return info.completeBaseName();
}

void ImportScraper::getCover(GameEntry &game)
{
  if(!coverFile.isEmpty()) {
    QFile f(coverFile);
    if(f.open(QIODevice::ReadOnly)) {
      game.coverData = f.readAll();
      f.close();
    }
  }
}

void ImportScraper::getScreenshot(GameEntry &game)
{
  if(!screenshotFile.isEmpty()) {
    QFile f(screenshotFile);
    if(f.open(QIODevice::ReadOnly)) {
      game.screenshotData = f.readAll();
      f.close();
    }
  }
}

void ImportScraper::getWheel(GameEntry &game)
{
  if(!wheelFile.isEmpty()) {
    QFile f(wheelFile);
    if(f.open(QIODevice::ReadOnly)) {
      game.wheelData = f.readAll();
      f.close();
    }
  }
}

void ImportScraper::getMarquee(GameEntry &game)
{
  if(!marqueeFile.isEmpty()) {
    QFile f(marqueeFile);
    if(f.open(QIODevice::ReadOnly)) {
      game.marqueeData = f.readAll();
      f.close();
    }
  }
}

void ImportScraper::getTexture(GameEntry &game)
{
  if(!textureFile.isEmpty()) {
    QFile f(textureFile);
    if(f.open(QIODevice::ReadOnly)) {
      game.textureData = f.readAll();
      f.close();
    }
  }
}

void ImportScraper::getVideo(GameEntry &game)
{
  if(!videoFile.isEmpty()) {
    QFile f(videoFile);
    if(f.open(QIODevice::ReadOnly)) {
      QFileInfo i(videoFile);
      game.videoData = f.readAll();
      game.videoFormat = i.suffix();
      f.close();
    }
  }
}

void ImportScraper::getManual(GameEntry &game)
{
  if(!manualFile.isEmpty()) {
    QFile f(manualFile);
    if(f.open(QIODevice::ReadOnly)) {
      QFileInfo i(manualFile);
      game.manualData = f.readAll();
      game.manualFormat = i.suffix();
      f.close();
    }
  }
}

void ImportScraper::getTitle(GameEntry &game)
{
  if(titlePre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(titlePre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(titlePre)) {
    nomNom(nom);
  }
  game.title = data.left(data.indexOf(titlePost.toUtf8())).simplified();
}

void ImportScraper::getRating(GameEntry &game)
{
  if(ratingPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(ratingPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(ratingPre)) {
    nomNom(nom);
  }
  game.rating = data.left(data.indexOf(ratingPost.toUtf8()));

  // check for 0, 0.5, 1, 1.5, ... 5
  QRegularExpression re("^[0-5](\\.5)?$");
  QRegularExpressionMatch m = re.match(game.rating);
  if(m.hasMatch()) {
    double rating = game.rating.toDouble();
    if(rating <= 5.0) {
      game.rating = QString::number(rating / 5.0);
    } else {
      game.rating = "";
    }
    return;
  }

  // check for 0.0 ... 1.0
  // known limitation: to catch 0.5 here enter it as '.5'
  bool toDoubleOk = false;
  double rating = game.rating.toDouble(&toDoubleOk);
  if(toDoubleOk && rating >= 0.0 && rating <= 1.0) {
    game.rating = QString::number(rating);
  } else {
    game.rating = "";
  }
}

void ImportScraper::loadData()
{
  if(!textualFile.isEmpty()) {
    QFile f(textualFile);
    if(f.open(QIODevice::ReadOnly)) {
      data = f.readAll();
      f.close();
    }
  }
}

bool ImportScraper::loadDefinitions()
{
  // Check for textual resource file
  QFile defFile;
  if(QFile::exists(config->importFolder + "/definitions.dat")) {
    defFile.setFileName(config->importFolder + "/definitions.dat");
  } else {
    defFile.setFileName(config->importFolder + "/../definitions.dat");
  }
  if(defFile.open(QIODevice::ReadOnly)) {
    while(!defFile.atEnd()) {
      QString line(defFile.readLine());
      checkForTag(titlePre, titlePost, titleTag, line);
      checkForTag(publisherPre, publisherPost, publisherTag, line);
      checkForTag(developerPre, developerPost, developerTag, line);
      checkForTag(playersPre, playersPost, playersTag, line);
      checkForTag(agesPre, agesPost, agesTag, line);
      checkForTag(ratingPre, ratingPost, ratingTag, line);
      checkForTag(tagsPre, tagsPost, tagsTag, line);
      checkForTag(franchisesPre, franchisesPost, franchisesTag, line);
      checkForTag(releaseDatePre, releaseDatePost, releaseDateTag, line);
      checkForTag(descriptionPre, descriptionPost, descriptionTag, line);
      checkForTag(guidesPre, guidesPost, guidesTag, line);
      checkForTag(platformPre, platformPost, platformTag, line);
      checkForTag(triviaPre, triviaPost, triviaTag, line);
      checkForTag(vgmapsPre, vgmapsPost, vgmapsTag, line);
    }
    defFile.close();
    return true;
  }
  return false;
}

void ImportScraper::checkForTag(QStringList &pre, QString &post, QString &tag, QString &line)
{
  if(line.indexOf(tag) != -1 && line.indexOf(tag) != 0) {
    pre.append(line.left(line.indexOf(tag)));
    post = line.right(line.length() - (line.indexOf(tag) + tag.length()));
  }
}

bool ImportScraper::checkType(QString baseName, QList<QFileInfo> &infos, QString &inputFile)
{
  for(const auto &i: std::as_const(infos)) {
    if(i.completeBaseName() == baseName) {
      inputFile = i.absoluteFilePath();
      return true;
    }
  }
  return false;
}
