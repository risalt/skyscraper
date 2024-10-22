/***************************************************************************
 *            pegasus.cpp
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

#include "pegasus.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

#include <iostream>
#include <QDir>
#include <QDate>
#include <QFileInfo>
#include <QRegularExpression>

Pegasus::Pegasus()
{
}

bool Pegasus::loadOldGameList(const QString &)
{
  return false;
}

QString Pegasus::makeAbsolute(const QString &filePath, const QString &inputFolder)
{
  QString returnPath = filePath;

  if(returnPath.left(1) == ".") {
    returnPath.remove(0, 1);
    returnPath.prepend(inputFolder);
  }
  return returnPath;
}


bool Pegasus::skipExisting(QList<GameEntry> &, QSharedPointer<Queue>) 
{
  return false;
}

void Pegasus::preserveFromOld(GameEntry &)
{
  return;
}

QString Pegasus::toPegasusFormat(const QString &key, const QString &value)
{
  QString pegasusFormat = value;
  
  QRegularExpressionMatch match;
  match = QRegularExpression("\\n[\\t ]*\\n").match(pegasusFormat);
  for(const auto &capture: match.capturedTexts()) {
    pegasusFormat.replace(capture, "###NEWLINE###" + tab + ".###NEWLINE###" + tab);
  }
  pegasusFormat.replace("\n", "\n" + tab);
  pegasusFormat.replace("###NEWLINE###", "\n");
  pegasusFormat.prepend(key + ": ");
  pegasusFormat = pegasusFormat.trimmed();

  return pegasusFormat;
}

void Pegasus::assembleList(QString &finalOutput, QList<GameEntry> &gameEntries)
{
  if(!gameEntries.isEmpty()) {
    finalOutput.append("collection: " + Platform::get().getAliases(config->platform).at(1) + "\n");
    finalOutput.append("shortname: " + config->platform + "\n");
    if (Platform::get().getSortBy(config->platform).isEmpty()) {
        finalOutput.append("sortby: 0000\n");
    }
    else {
        finalOutput.append("sortby: " + Platform::get().getSortBy(config->platform) + "\n");
    }
    // finalOutput.append("extensions: " + fromPreservedHeader("extensions", extensions) + "\n");
    if(config->frontendExtra.isEmpty()) {
      finalOutput.append("command: /opt/retropie/supplementary/runcommand/runcommand.sh 0 _SYS_ " +
                         config->platform + " \"{file.path}\"\n");
    } else {
      //finalOutput.append("command: " + config->frontendExtra.replace(":","") + "\n");
      finalOutput.append("command: " + config->frontendExtra + "\n");
    }
    finalOutput.append("\n");
  }
  int dots = 0;
  // Always make dotMod at least 1 or it will give "floating point exception" when modulo
  int dotMod = gameEntries.length() * 0.1 + 1;
  if(dotMod == 0)
    dotMod = 1;
  for(auto &entry: gameEntries) {
    if(dots % dotMod == 0) {
      printf(".");
      fflush(stdout);
    }
    dots++;

    preserveFromOld(entry);

    if(config->relativePaths) {
      entry.path.replace(config->inputFolder, ".");
    }
    QString sanitizedName = entry.title;
    if (sanitizedName.isEmpty()) {
      sanitizedName = "N/A";
    }
    finalOutput.append(toPegasusFormat("game", sanitizedName) + "\n");
    finalOutput.append(toPegasusFormat("file", entry.path) + "\n");
    // The replace here IS supposed to be 'inputFolder' and not 'mediaFolder' because we only want the path to be relative if '-o' hasn't been set. So this will only make it relative if the path is equal to inputFolder which is what we want.
    if(!entry.rating.isEmpty()) {
      finalOutput.append(toPegasusFormat("rating", QString::number((int)(entry.rating.toDouble() * 100)) + "%") + "\n");
    }
    if(!entry.description.isEmpty()) {
      finalOutput.append(toPegasusFormat("description", entry.description.left(config->maxLength)) + "\n");
    }
    if(!entry.releaseDate.isEmpty()) {
      finalOutput.append(toPegasusFormat("release", QDate::fromString(entry.releaseDate, "yyyyMMdd").toString("yyyy-MM-dd")) + "\n");
    }
    if(!entry.developer.isEmpty()) {
      finalOutput.append(toPegasusFormat("developer", entry.developer) + "\n");
    }
    if(!entry.publisher.isEmpty()) {
      finalOutput.append(toPegasusFormat("publisher", entry.publisher) + "\n");
    }
    if(!entry.tags.isEmpty()) {
      finalOutput.append(toPegasusFormat("genre", entry.tags) + "\n");
    }
    if(!entry.players.isEmpty()) {
      finalOutput.append(toPegasusFormat("players", entry.players) + "\n");
    }
    if(!entry.screenshotFile.isEmpty()) {
      finalOutput.append(toPegasusFormat("assets.screenshot", (config->relativePaths?entry.screenshotFile.replace(config->inputFolder, "."):entry.screenshotFile)) + "\n");
    }
    if(entry.coverSrc.isEmpty() && !entry.marqueeFile.isEmpty()) {
      finalOutput.append(toPegasusFormat("assets.boxFront", (config->relativePaths?entry.marqueeFile.replace(config->inputFolder, "."):entry.marqueeFile)) + "\n");
    } else {
      finalOutput.append(toPegasusFormat("assets.boxFront", (config->relativePaths?entry.coverFile.replace(config->inputFolder, "."):entry.coverFile)) + "\n");
    }
    if(!entry.marqueeFile.isEmpty()) {
      finalOutput.append(toPegasusFormat("assets.marquee", (config->relativePaths?entry.marqueeFile.replace(config->inputFolder, "."):entry.marqueeFile)) + "\n");
    }
    if(!entry.textureFile.isEmpty()) {
      finalOutput.append(toPegasusFormat("assets.cartridge", (config->relativePaths?entry.textureFile.replace(config->inputFolder, "."):entry.textureFile)) + "\n");
    }
    if(!entry.wheelFile.isEmpty()) {
      finalOutput.append(toPegasusFormat("assets.wheel", (config->relativePaths?entry.wheelFile.replace(config->inputFolder, "."):entry.wheelFile)) + "\n");
    }
    if(!entry.videoFormat.isEmpty() && config->videos) {
      finalOutput.append(toPegasusFormat("assets.video", (config->relativePaths?entry.videoFile.replace(config->inputFolder, "."):entry.videoFile)) + "\n");
    }
    if(!entry.manualFormat.isEmpty() && config->manuals) {
      finalOutput.append(toPegasusFormat("assets.manual", (config->relativePaths?entry.manualFile.replace(config->inputFolder, "."):entry.manualFile)) + "\n");
    }
    if(!entry.pSValuePairs.isEmpty()) {
      for(const auto &pair: entry.pSValuePairs) {
        finalOutput.append(toPegasusFormat(pair.first, pair.second) + "\n");
      }
    }
    finalOutput.append("\n\n");
  }
}

bool Pegasus::canSkip()
{
  return true;
}

QString Pegasus::getGameListFileName()
{
  return QString("metadata.pegasus.txt");
}

QString Pegasus::getInputFolder()
{
  return QString(QDir::homePath() + "/RetroPie/roms/" + config->platform);
}

QString Pegasus::getGameListFolder()
{
  return config->inputFolder;
}

QString Pegasus::getCoversFolder()
{
  return config->mediaFolder + "/covers";
}

QString Pegasus::getScreenshotsFolder()
{
  return config->mediaFolder + "/screenshots";
}

QString Pegasus::getWheelsFolder()
{
  return config->mediaFolder + "/wheels";
}

QString Pegasus::getMarqueesFolder()
{
  return config->mediaFolder + "/marquees";
}

QString Pegasus::getTexturesFolder()
{
  return config->mediaFolder + "/textures";
}

QString Pegasus::getVideosFolder()
{
  return config->mediaFolder + "/videos";
}

QString Pegasus::getManualsFolder()
{
  return config->mediaFolder + "/manuals";
}
