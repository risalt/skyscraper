/***************************************************************************
 *            gameentry.h
 *
 *  Wed Jun 14 12:00:00 CEST 2017
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

#ifndef GAMEENTRY_H
#define GAMEENTRY_H

constexpr int DESCRIPTION = 0;
constexpr int DEVELOPER = 1;
constexpr int PUBLISHER = 2;
constexpr int PLAYERS = 3;
constexpr int TAGS = 4;
constexpr int RELEASEDATE = 5;
constexpr int COVER = 6;
constexpr int SCREENSHOT = 7;
constexpr int VIDEO = 8;
constexpr int RATING = 9;
constexpr int WHEEL = 10;
constexpr int MARQUEE = 11;
constexpr int AGES = 12;
constexpr int TITLE = 13;
constexpr int ID = 27;
constexpr int TEXTURE = 14;
constexpr int FRANCHISES = 15;
constexpr int MANUAL = 16;
constexpr int CHIPTUNE = 17;
constexpr int CUSTOMFLAGS = 18;
constexpr int GUIDES = 19;
constexpr int PLATFORM = 20;
constexpr int TRIVIA = 21;
constexpr int VGMAPS = 22;
constexpr int CHEATS = 23;
constexpr int REVIEWS = 24;
constexpr int ARTBOOKS = 25;
constexpr int SPRITES = 26;

#include <QImage>
#include <QByteArray>
#include <QDataStream>

struct CanonicalData {
  QString name = "";
  qint64 size = 0;
  QString crc = "";
  QString sha1 = "";
  QString md5 = "";
  QString file = "";
  QString mameid = "";
  QString platform = "";
};

class GameEntry {
public:
  GameEntry();
  GameEntry(const QByteArray & buffer);
  QByteArray serialize() const;
  void resetMedia();
  int getCompleteness() const;

  // The following block is serialized into quickid.xml/db.xml:
  QString id = "";
  QString idSrc = "";
  QString path = "";
  QString title = "";
  QString titleSrc = "";
  QString platform = "";
  QString platformSrc = "";
  QString description = "";
  QString descriptionSrc = "";
  QString trivia = "";
  QString triviaSrc = "";
  QString releaseDate = "";
  QString releaseDateSrc = "";
  QString developer = "";
  QString developerSrc = "";
  QString publisher = "";
  QString publisherSrc = "";
  QString tags = "";
  QString tagsSrc = "";
  QString franchises = "";
  QString franchisesSrc = "";
  QString players = "";
  QString playersSrc = "";
  QString ages = "";
  QString agesSrc = "";
  QString rating = "";
  QString ratingSrc = "";
  bool completed = false;
  bool favourite = false;
  bool played = false;
  unsigned int timesPlayed = 0;
  qint64 lastPlayed = 0;
  qint64 firstPlayed = 0;
  qint64 timePlayed = 0;
  qint64 diskSize = 0;
  QByteArray coverData = QByteArray();
  QString coverFile = "";
  QString coverSrc = "";
  QString coverRegion = "";
  QByteArray screenshotData = QByteArray();
  QString screenshotFile = "";
  QString screenshotSrc = "";
  QString screenshotRegion = "";
  QByteArray wheelData = QByteArray();
  QString wheelFile = "";
  QString wheelSrc = "";
  QString wheelRegion = "";
  QByteArray marqueeData = QByteArray();
  QString marqueeFile = "";
  QString marqueeSrc = "";
  QString marqueeRegion = "";
  QByteArray textureData = QByteArray();
  QString textureFile = "";
  QString textureSrc = "";
  QString textureRegion = "";
  QByteArray videoData = "";
  QString videoFile = "";
  QString videoSrc = "";
  QByteArray manualData = "";
  QString manualFile = "";
  QString manualSrc = "";
  QString guides = "";
  QString guidesSrc = "";
  QString cheats = "";
  QString cheatsSrc = "";
  QString reviews = "";
  QString reviewsSrc = "";
  QString artbooks = "";
  QString artbooksSrc = "";
  QString vgmaps = "";
  QString vgmapsSrc = "";
  QString sprites = "";
  QString spritesSrc = "";
  QString chiptuneId = "";
  QString chiptuneIdSrc = "";
  QString chiptunePath = "";
  QString chiptunePathSrc = "";
  CanonicalData canonical;

  int searchMatch = 0;
  QString cacheId = "";
  QString source = "";
  QString url = "";
  QString sqrNotes = "";
  QString parNotes = "";
  QString videoFormat = "";
  QString manualFormat = "";
  QString baseName = "";
  QString absoluteFilePath = "";
  bool found = true;
  bool emptyShell = false;

  QByteArray miscData = "";

  // EmulationStation specific metadata for preservation
  QString eSFavorite = "";
  QString eSHidden = "";
  QString eSPlayCount = "";
  QString eSLastPlayed = "";
  QString eSKidGame = "";
  QString eSSortName = "";

  // AttractMode specific metadata for preservation
  // #Name;Title;Emulator;CloneOf;Year;Manufacturer;Category;Players;Rotation;Control;Status;DisplayCount;DisplayType;AltRomname;AltTitle;Extra;Buttons
  QString aMCloneOf = "";
  QString aMRotation = "";
  QString aMControl = "";
  QString aMStatus = "";
  QString aMDisplayCount = "";
  QString aMDisplayType = "";
  QString aMAltRomName = "";
  QString aMAltTitle = "";
  QString aMExtra = "";
  QString aMButtons = "";

  // Pegasus specific metadata for preservation
  QList<QPair<QString, QString> > pSValuePairs;

};

QDataStream &operator<<(QDataStream &out, const GameEntry &game);
QDataStream &operator>>(QDataStream &in, GameEntry &game);
QDebug operator<<(QDebug dbg, const GameEntry &game);

#endif // GAMEENTRY_H
