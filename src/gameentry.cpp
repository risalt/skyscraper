/***************************************************************************
 *            gameentry.cpp
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

#include "gameentry.h"
#include "skyscraper.h"

GameEntry::GameEntry()
{
}

int GameEntry::getCompleteness() const
{
  double completeness = 100.0;
  int noOfTypes = 16;
  if(Skyscraper::videos) {
    noOfTypes += 1;
  }
  if(Skyscraper::manuals) {
    noOfTypes += 1;
  }
  if(Skyscraper::chiptunes) {
    noOfTypes += 1;
  }
  double valuePerType = completeness / (double)noOfTypes;
  if(title.isEmpty()) {
    completeness -= valuePerType;
  }
  if(platform.isEmpty()) {
    completeness -= valuePerType;
  }
  if(coverData.isNull() || emptyShell) {
    completeness -= valuePerType;
  }
  if(screenshotData.isNull()) {
    completeness -= valuePerType;
  }
  if(wheelData.isNull()) {
    completeness -= valuePerType;
  }
  if(marqueeData.isNull()) {
    completeness -= valuePerType;
  }
  if(textureData.isNull()) {
    completeness -= valuePerType;
  }
  if(description.isEmpty()) {
    completeness -= valuePerType;
  }
  if(releaseDate.isEmpty()) {
    completeness -= valuePerType;
  }
  if(developer.isEmpty()) {
    completeness -= valuePerType;
  }
  if(publisher.isEmpty()) {
    completeness -= valuePerType;
  }
  if(tags.isEmpty()) {
    completeness -= valuePerType;
  }
  if(franchises.isEmpty()) {
    completeness -= valuePerType;
  }
  if(rating.isEmpty()) {
    completeness -= valuePerType;
  }
  if(players.isEmpty()) {
    completeness -= valuePerType;
  }
  if(ages.isEmpty()) {
    completeness -= valuePerType;
  }
  if(Skyscraper::videos && videoFormat.isEmpty()) {
    completeness -= valuePerType;
  }
  if(Skyscraper::manuals && manualFormat.isEmpty()) {
    completeness -= valuePerType;
  }
  if(Skyscraper::chiptunes && ( chiptuneId.isEmpty() || chiptunePath.isEmpty())) {
    completeness -= valuePerType;
  }
  return (int)completeness;
}

void GameEntry::resetMedia()
{
  coverData = QByteArray();
  screenshotData = QByteArray();
  wheelData = QByteArray();
  marqueeData = QByteArray();
  textureData = QByteArray();
  videoData = "";
  manualData = "";
}

QDebug operator<<(QDebug debug, const GameEntry &game) {
    QDebugStateSaver saver(debug);
    debug.nospace() << 
      "\nField 'id': " << game.id <<
      "\nField 'path': " << game.path <<
      "\nField 'title': " << game.title <<
      "\nField 'titleSrc': " << game.titleSrc <<
      "\nField 'platform': " << game.platform <<
      "\nField 'platformSrc': " << game.platformSrc <<
      "\nField 'description': " << game.description <<
      "\nField 'descriptionSrc': " << game.descriptionSrc <<
      "\nField 'releaseDate': " << game.releaseDate <<
      "\nField 'releaseDateSrc': " << game.releaseDateSrc <<
      "\nField 'developer': " << game.developer <<
      "\nField 'developerSrc': " << game.developerSrc <<
      "\nField 'publisher': " << game.publisher <<
      "\nField 'publisherSrc': " << game.publisherSrc <<
      "\nField 'tags': " << game.tags <<
      "\nField 'tagsSrc': " << game.tagsSrc <<
      "\nField 'franchises': " << game.franchises <<
      "\nField 'franchisesSrc': " << game.franchisesSrc <<
      "\nField 'players': " << game.players <<
      "\nField 'playersSrc': " << game.playersSrc <<
      "\nField 'ages': " << game.ages <<
      "\nField 'agesSrc': " << game.agesSrc <<
      "\nField 'rating': " << game.rating <<
      "\nField 'ratingSrc': " << game.ratingSrc <<
      "\nField 'completed': " << game.completed <<
      "\nField 'favourite': " << game.favourite <<
      "\nField 'played': " << game.played <<
      "\nField 'timesPlayed': " << game.timesPlayed <<
      "\nField 'lastPlayed': " << game.lastPlayed <<
      "\nField 'firstPlayed': " << game.firstPlayed <<
      "\nField 'timePlayed': " << game.timePlayed <<
      "\nField 'coverFile': " << game.coverFile <<
      "\nField 'coverData': " << "Data:" << game.coverData.size() <<
      "\nField 'coverSrc': " << game.coverSrc <<
      "\nField 'coverRegion': " << game.coverRegion <<
      "\nField 'screenshotFile': " << game.screenshotFile <<
      "\nField 'screenshotData': " << "Data:" << game.screenshotData.size() <<
      "\nField 'screenshotSrc': " << game.screenshotSrc <<
      "\nField 'screenshotRegion': " << game.screenshotRegion <<
      "\nField 'wheelFile': " << game.wheelFile <<
      "\nField 'wheelData': " << "Data:" << game.wheelData.size() <<
      "\nField 'wheelSrc': " << game.wheelSrc <<
      "\nField 'wheelRegion': " << game.wheelRegion <<
      "\nField 'marqueeFile': " << game.marqueeFile <<
      "\nField 'marqueeData': " << "Data:" << game.marqueeData.size() <<
      "\nField 'marqueeSrc': " << game.marqueeSrc <<
      "\nField 'marqueeRegion': " << game.marqueeRegion <<
      "\nField 'textureFile': " << game.textureFile <<
      "\nField 'textureData': " << "Data:" << game.textureData.size() <<
      "\nField 'textureSrc': " << game.textureSrc <<
      "\nField 'textureRegion': " << game.textureRegion <<
      "\nField 'videoFile': " << game.videoFile <<
      "\nField 'videoData': " << "Data:" << game.videoData.size() <<
      "\nField 'videoSrc': " << game.videoSrc <<
      "\nField 'manualFile': " << game.manualFile <<
      "\nField 'manualData': " << "Data:" << game.manualData.size() <<
      "\nField 'manualSrc': " << game.manualSrc <<
      "\nField 'chiptuneId': " << game.chiptuneId <<
      "\nField 'chiptuneIdSrc': " << game.chiptuneIdSrc <<
      "\nField 'chiptunePath': " << game.chiptunePath <<
      "\nField 'chiptunePathSrc': " << game.chiptunePathSrc <<
      "\nField 'searchMatch': " << game.searchMatch <<
      "\nField 'cacheId': " << game.cacheId <<
      "\nField 'source': " << game.source <<
      "\nField 'url': " << game.url <<
      "\nField 'sqrNotes': " << game.sqrNotes <<
      "\nField 'parNotes': " << game.parNotes <<
      "\nField 'videoFormat': " << game.videoFormat <<
      "\nField 'manualFormat': " << game.manualFormat <<
      "\nField 'baseName': " << game.baseName <<
      "\nField 'absoluteFilePath': " << game.absoluteFilePath <<
      "\nField 'found': " << game.found <<
      "\nField 'emptyShell': " << game.emptyShell <<
      "\nField 'miscData': " << QString(game.miscData) <<
      "\nField 'pSValuePairs': " << game.pSValuePairs;

    return debug;
}
