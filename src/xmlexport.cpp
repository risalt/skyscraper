/***************************************************************************
 *            xmlexport.cpp
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

#include "xmlexport.h"
#include "xmlreader.h"
#include "strtools.h"
#include "platform.h"

#include <cmath>

#include <QDir>
#include <QDate>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>

XmlExport::XmlExport()
{
}

bool XmlExport::loadOldGameList(const QString &)
{
  // TODO: LOW Implement incremental file generation
  return false;
}

bool XmlExport::skipExisting(QList<GameEntry> &, QSharedPointer<Queue>)
{
  return false;
}

void XmlExport::assembleList(QString &finalOutput, QList<GameEntry> &gameEntries)
{
  int dots = 0;
  // Always make dotMod at least 1 or it will give "floating point exception" when modulo
  int dotMod = gameEntries.length() * 0.1 + 1;
  if(dotMod == 0)
    dotMod = 1;
  finalOutput.append("<?xml version=\"1.0\"?>\n<gameList>\n");
  for(const auto &entry: std::as_const(gameEntries)) {
    if(dots % dotMod == 0) {
      printf(".");
      fflush(stdout);
    }
    dots++;

    QString entryType = "game";

    QString entryPath = entry.path;
    if(config->relativePaths) {
      entryPath.replace(config->inputFolder, ".");
    }

    finalOutput.append("  <" + entryType + ">\n");
    finalOutput.append("    <executable>retro://" + config->platform + StrTools::uriEscape(entryPath) + "</executable>\n");
    finalOutput.append("    <romfile>" + StrTools::uriEscape(entryPath) + "</romfile>\n");
    if(entry.title.isEmpty()) {
      finalOutput.append("    <name>" + StrTools::xmlEscape("N/A") + "</name>\n");
    }
    else {
      finalOutput.append("    <name>" + StrTools::xmlEscape(entry.title) + "</name>\n");
    }
    finalOutput.append("    <platform>" + StrTools::xmlEscape(Platform::get().getAliases(config->platform).at(1)) + "</platform>\n");
    if(!QFileInfo(entryPath).exists()) {
      finalOutput.append("    <added />\n");
    } else {
      finalOutput.append("    <added>" + StrTools::xmlEscape(QFileInfo(entryPath).lastModified().date().toString("dd/MM/yyyy")) + "</added>\n");
    }
    QString thumbnail;
    // boxpic
    if(entry.coverFile.isEmpty()) {
      thumbnail = (config->relativePaths?getCoversFolder().replace(config->inputFolder, "."):getCoversFolder()) + "/" + QFileInfo(entryPath).completeBaseName() + ".png";
      if(!QFileInfo(thumbnail).exists()) {
        thumbnail = "/opt/pegasus/roms/default/missingboxart.png";
      }
    } else {
      // The replace here IS supposed to be 'inputFolder' and not 'mediaFolder' because we only want the path to be relative if '-o' hasn't been set. So this will only make it relative if the path is equal to inputFolder which is what we want.
      thumbnail = entry.coverFile;
    }
    finalOutput.append("    <boxpic>" + (config->relativePaths?StrTools::uriEscape(thumbnail).replace(config->inputFolder, "."):StrTools::uriEscape(thumbnail)) + "</boxpic>\n");
    // artwork
    if(entry.marqueeFile.isEmpty()) {
      finalOutput.append("    <artwork>/opt/pegasus/roms/default/missingboxart.png</artwork>\n");
    } else {
      finalOutput.append("    <artwork>" + (config->relativePaths?StrTools::uriEscape(entry.marqueeFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.marqueeFile)) + "</artwork>\n");
    }
    // screenshot1
    if(entry.screenshotFile.isEmpty()) {
      finalOutput.append("    <screenshot1>/opt/pegasus/roms/default/missingboxart.png</screenshot1>\n");
    } else {
      finalOutput.append("    <screenshot1>" + (config->relativePaths?StrTools::uriEscape(entry.screenshotFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.screenshotFile)) + "</screenshot1>\n");
    }
    // screenshot2
    if(entry.wheelFile.isEmpty()) {
      finalOutput.append("    <screenshot2>/opt/pegasus/roms/default/missingboxart.png</screenshot2>\n");
    } else {
      finalOutput.append("    <screenshot2>" + (config->relativePaths?StrTools::uriEscape(entry.wheelFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.wheelFile)) + "</screenshot2>\n");
    }
    // backpic
    if(entry.textureFile.isEmpty()) {
      finalOutput.append("    <backpic>/opt/pegasus/roms/default/missingboxart.png</backpic>\n");
    } else {
      finalOutput.append("    <backpic>" + (config->relativePaths?StrTools::uriEscape(entry.textureFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.textureFile)) + "</backpic>\n");
    }
    // video
    if(!entry.videoFile.isEmpty()) {
      finalOutput.append("    <video>" + (config->relativePaths?StrTools::uriEscape(entry.videoFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.videoFile)) + "</video>\n");
    }
    // manual
    if(!entry.manualFile.isEmpty()) {
      finalOutput.append("    <manual>" + (config->relativePaths?StrTools::uriEscape(entry.manualFile).replace(config->inputFolder, "."):StrTools::uriEscape(entry.manualFile)) + "</manual>\n");
    }
    // chiptune
    if(entry.chiptuneId.isEmpty()) {
      finalOutput.append("    <audiourl />\n");
    } else {
      finalOutput.append("    <audiourl>http://media.localnet.priv:4533/share/" + StrTools::xmlEscape(entry.chiptuneId) + "</audiourl>\n");
    }
    if(entry.chiptunePath.isEmpty()) {
      finalOutput.append("    <audiofile />\n");
    } else {
      finalOutput.append("    <audiofile>retro://music" + StrTools::xmlEscape(entry.chiptunePath) + "</audiofile>\n");
    }
    if(entry.guides.isEmpty()) {
      finalOutput.append("    <guides />\n");
    } else {
      QStringList guides = {};
      if(!entry.guides.isEmpty()) {
        guides = entry.guides.split(" ");
        guides.sort();
      }
      QString guidesLinks;
      if(!guides.isEmpty()) {
        int pos = 1;
        for(const auto &link: std::as_const(guides)) {
          guidesLinks += "<a href=\"" + link + "\" target=\"_blank\">" + QString::number(pos) + "</a> ";
          pos++;
        }
        guidesLinks.replace(config->guidesPath, "/uploads/guides");
      }
      finalOutput.append("    <guides>" + StrTools::xmlEscape(guidesLinks) + "</guides>\n");
    }
    if(entry.vgmaps.isEmpty()) {
      finalOutput.append("    <vgmaps />\n");
    } else {
      QStringList vgmaps = {};
      if(!entry.vgmaps.isEmpty()) {
        vgmaps = entry.vgmaps.split(" ");
      }
      QString vgmapsLinks;
      if(!vgmaps.isEmpty()) {
        for(const auto &link: std::as_const(vgmaps)) {
          QFileInfo map(link);
          QString mapPlatform = map.absolutePath();
          map.setFile(mapPlatform);
          mapPlatform = map.baseName();
          vgmapsLinks += "<a href=\"" + link + "\" target=\"_blank\">" + mapPlatform + "</a> ";
        }
        vgmapsLinks.replace(config->vgmapsPath, "/uploads/vgmaps");
      }
      finalOutput.append("    <vgmaps>" + StrTools::xmlEscape(vgmapsLinks) + "</vgmaps>\n");
    }
    if(entry.rating.isEmpty()) {
      finalOutput.append("    <rating />\n");
    } else {
      QString rating;
      finalOutput.append("    <rating>" + StrTools::xmlEscape(rating.setNum(entry.rating.toFloat() * 10.0)) + "</rating>\n");
    }
    if(entry.description.isEmpty()) {
      finalOutput.append("    <description />\n");
    } else {
      finalOutput.append("    <description>" + StrTools::xmlEscape(entry.description.trimmed()) + "</description>\n");
    }
    if(entry.trivia.isEmpty()) {
      finalOutput.append("    <trivia />\n");
    } else {
      finalOutput.append("    <trivia>" + StrTools::xmlEscape(entry.trivia.trimmed()) + "</trivia>\n");
    }
    if(entry.releaseDate.isEmpty() || entry.releaseDate == "19700101" || entry.releaseDate == "19600101") {
      finalOutput.append("    <released />\n");
    } else {
      finalOutput.append("    <released>" + StrTools::xmlEscape(QDate::fromString(entry.releaseDate, "yyyyMMdd").toString("dd/MM/yyyy")) + "</released>\n");
    }
    if(entry.ages.isEmpty()) {
      finalOutput.append("    <ages />\n");
    } else {
      finalOutput.append("    <ages>" + StrTools::xmlEscape(StrTools::agesLabel(entry.ages)) + "</ages>\n");
    }
    if(entry.developer.isEmpty()) {
      finalOutput.append("    <developer />\n");
    } else {
      finalOutput.append("    <developer>" + StrTools::xmlEscape(entry.developer) + "</developer>\n");
    }
    if(entry.publisher.isEmpty()) {
      finalOutput.append("    <editor />\n");
    } else {
      finalOutput.append("    <editor>" + StrTools::xmlEscape(entry.publisher) + "</editor>\n");
    }
    if(entry.tags.isEmpty()) {
      finalOutput.append("    <genre />\n");
    } else {
      finalOutput.append("    <genre>" + StrTools::xmlEscape(entry.tags) + "</genre>\n");
    }
    if(entry.franchises.isEmpty()) {
      finalOutput.append("    <franchise />\n");
    } else {
      finalOutput.append("    <franchise>" + StrTools::xmlEscape(entry.franchises) + "</franchise>\n");
    }
    if(entry.players.isEmpty()) {
      finalOutput.append("    <players />\n");
    } else {
      finalOutput.append("    <players>" + StrTools::xmlEscape(entry.players) + "</players>\n");
    }
    if(entry.favourite) {
      finalOutput.append("    <favorite>1</favorite>\n");
    } else {
      finalOutput.append("    <favorite>0</favorite>\n");
    }
    if(entry.played) {
      finalOutput.append("    <played>1</played>\n");
    } else {
      finalOutput.append("    <played>0</played>\n");
    }
    if(entry.completed) {
      finalOutput.append("    <completed>1</completed>\n");
    } else {
      finalOutput.append("    <completed>0</completed>\n");
    }
    if(!entry.timesPlayed) {
      finalOutput.append("    <timesPlayed>0</timesPlayed>\n");
    } else {
      finalOutput.append("    <timesPlayed>" + QString::number(entry.timesPlayed)  + "</timesPlayed>\n");
    }
    if(!entry.diskSize) {
      finalOutput.append("    <diskSize>0</diskSize>\n");
    } else {
      QStringList units = { "Bytes", "KiB", "MiB", "GiB", "TiB" };
      int pow = std::floor((entry.diskSize ? std::log(entry.diskSize) : 0) / log(1024));
      pow = std::min(pow, units.size() - 1);
      qint64 bytes = 100 * entry.diskSize / std::pow(1024, pow);
      double bytesdec = double(bytes) / 100.0;
      finalOutput.append("    <diskSize>" + QString::number(bytesdec) + ' ' + units.at(pow)  + "</diskSize>\n");
    }
    if(!entry.timePlayed) {
      finalOutput.append("    <timePlayed>0</timePlayed>\n");
    } else {
      qint64 minutesPlayed = entry.timePlayed / 60;
      qint64 hoursPlayed = (minutesPlayed > 60) ? minutesPlayed / 60 : 0;
      minutesPlayed -= hoursPlayed * 60;
      QString timePlayed = QString::number(hoursPlayed) + " hours, " + QString::number(minutesPlayed) + " minutes";
      finalOutput.append("    <timePlayed>" + timePlayed + "</timePlayed>\n");
    }
    if(!entry.firstPlayed) {
      finalOutput.append("    <firstPlayed></firstPlayed>\n");
    } else {
      QDateTime dateEpoch;
      dateEpoch.setSecsSinceEpoch(entry.firstPlayed);
      finalOutput.append("    <firstPlayed>" + dateEpoch.toString(Qt::ISODate) + "</firstPlayed>\n");
    }
    if(!entry.lastPlayed) {
      finalOutput.append("    <lastPlayed></lastPlayed>\n");
    } else {
      QDateTime dateEpoch;
      dateEpoch.setSecsSinceEpoch(entry.lastPlayed);
      finalOutput.append("    <lastPlayed>" + dateEpoch.toString(Qt::ISODate) + "</lastPlayed>\n");
    }
    finalOutput.append("  </" + entryType + ">\n");
  }
  finalOutput.append("</gameList>");
}

bool XmlExport::canSkip()
{
  return false;
}

QString XmlExport::getGameListFileName()
{
  return QString("gamelist.xml");
}

QString XmlExport::getInputFolder()
{
  return QString(QDir::homePath() + "/RetroPie/roms/" + config->platform);
}

QString XmlExport::getGameListFolder()
{
  return config->inputFolder;
}

QString XmlExport::getCoversFolder()
{
  return config->mediaFolder + "/covers";
}

QString XmlExport::getScreenshotsFolder()
{
  return config->mediaFolder + "/screenshots";
}

QString XmlExport::getWheelsFolder()
{
  return config->mediaFolder + "/wheels";
}

QString XmlExport::getMarqueesFolder()
{
  return config->mediaFolder + "/marquees";
}

QString XmlExport::getTexturesFolder() {
  return config->mediaFolder + "/textures";
}

QString XmlExport::getVideosFolder()
{
  return config->mediaFolder + "/videos";
}

QString XmlExport::getManualsFolder()
{
  return config->mediaFolder + "/manuals";
}
