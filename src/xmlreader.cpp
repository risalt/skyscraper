/***************************************************************************
 *            xmlreader.cpp
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
#include <QFile>
#include <QFileInfo>

#include "xmlreader.h"
#include "nametools.h"
#include "strtools.h"

XmlReader::XmlReader()
{
}

XmlReader::~XmlReader()
{
}

bool XmlReader::setFile(QString filename)
{
  bool result = false;

  QFile f(filename);
  if(f.open(QIODevice::ReadOnly)) {
#if QT_VERSION < 0x060800
        if (setContent(f.readAll(), false)) {
#else
        if (QDomDocument::ParseResult p = QDomDocument::setContent(f.readAll());
            p) {
#endif
      result = true;
    }
    f.close();
  }
  return result;
}

QList<GameEntry> XmlReader::getEntries(QString inputFolder)
{
  QList<GameEntry> gameEntries;

  QDomNodeList gameNodes = elementsByTagName("game");
  QDomNodeList pathNodes = elementsByTagName("folder");

  addEntries(gameNodes, gameEntries, inputFolder);
  addEntries(pathNodes, gameEntries, inputFolder);

  return gameEntries;
}

void XmlReader::addEntries(const QDomNodeList &nodes, QList<GameEntry> &gameEntries,
                           const QString &inputFolder)
{
  for(int a = 0; a < nodes.length(); ++a) {
    GameEntry entry;
    entry.path = makeAbsolute(nodes.at(a).firstChildElement("path").text(), inputFolder);
    // Do NOT get sqr and par notes here. They are not used by skipExisting
    entry.title = nodes.at(a).firstChildElement("name").text();
    entry.coverFile = makeAbsolute(nodes.at(a).firstChildElement("cover").text(), inputFolder);
    entry.screenshotFile = makeAbsolute(nodes.at(a).firstChildElement("image").text(), inputFolder);
    entry.marqueeFile = makeAbsolute(nodes.at(a).firstChildElement("marquee").text(), inputFolder);
    entry.textureFile = makeAbsolute(nodes.at(a).firstChildElement("texture").text(), inputFolder);
    entry.videoFile = makeAbsolute(nodes.at(a).firstChildElement("video").text(), inputFolder);
    if(!entry.videoFile.isEmpty()) {
      entry.videoFormat = "fromxml";
    }
    entry.manualFile = makeAbsolute(nodes.at(a).firstChildElement("manual").text(), inputFolder);
    if(!entry.manualFile.isEmpty()) {
      entry.manualFormat = "fromxml";
    }
    entry.description = nodes.at(a).firstChildElement("desc").text();
    entry.trivia = nodes.at(a).firstChildElement("trivia").text();
    entry.releaseDate = nodes.at(a).firstChildElement("releasedate").text();
    entry.developer = nodes.at(a).firstChildElement("developer").text();
    entry.publisher = nodes.at(a).firstChildElement("publisher").text();
    entry.tags = nodes.at(a).firstChildElement("genre").text();
    entry.franchises = nodes.at(a).firstChildElement("franchise").text();
    entry.rating = nodes.at(a).firstChildElement("rating").text();
    entry.players = nodes.at(a).firstChildElement("players").text();
    entry.guides = nodes.at(a).firstChildElement("guides").text();
    entry.cheats = nodes.at(a).firstChildElement("cheats").text();
    entry.reviews = nodes.at(a).firstChildElement("reviews").text();
    entry.artbooks = nodes.at(a).firstChildElement("artbooks").text();
    entry.vgmaps = nodes.at(a).firstChildElement("vgmaps").text();
    entry.sprites = nodes.at(a).firstChildElement("sprites").text();
    entry.chiptuneId = nodes.at(a).firstChildElement("chiptuneid").text();
    entry.chiptunePath = nodes.at(a).firstChildElement("chiptunepath").text();
    entry.eSFavorite = nodes.at(a).firstChildElement("favorite").text();
    entry.eSHidden = nodes.at(a).firstChildElement("hidden").text();
    entry.eSPlayCount = nodes.at(a).firstChildElement("playcount").text();
    entry.eSLastPlayed = nodes.at(a).firstChildElement("lastplayed").text();
    entry.eSKidGame = nodes.at(a).firstChildElement("kidgame").text();
    entry.eSSortName = nodes.at(a).firstChildElement("sortname").text();
    gameEntries.append(entry);
  }
}

QString XmlReader::makeAbsolute(QString filePath, const QString &inputFolder)
{
  if(filePath.left(1) == ".") {
    filePath.remove(0, 1);
    filePath.prepend(inputFolder);
  }
  return filePath;
}
