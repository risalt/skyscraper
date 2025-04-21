/***************************************************************************
 *            gameentry.cpp
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

#include "gameentry.h"
#include "skyscraper.h"

GameEntry::GameEntry()
{
}

GameEntry::GameEntry(const QByteArray & buffer)
{
  GameEntry game;
  QDataStream bRead(buffer);
  bRead >>
        id >>
        idSrc >>
        path >>
        canonical.name >>
        canonical.mameid >>
        canonical.size >>
        canonical.crc >>
        canonical.sha1 >>
        canonical.md5 >>
        title >>
        titleSrc >>
        platform >>
        platformSrc >>
        description >>
        descriptionSrc >>
        trivia >>
        triviaSrc >>
        releaseDate >>
        releaseDateSrc >>
        developer >>
        developerSrc >>
        publisher >>
        publisherSrc >>
        tags >>
        tagsSrc >>
        franchises >>
        franchisesSrc >>
        players >>
        playersSrc >>
        ages >>
        agesSrc >>
        rating >>
        ratingSrc >>
        completed >>
        favourite >>
        played >>
        timesPlayed >>
        lastPlayed >>
        firstPlayed >>
        timePlayed >>
        diskSize >>
        coverFile >>
        coverData >>
        coverSrc >>
        coverRegion >>
        screenshotFile >>
        screenshotData >>
        screenshotSrc >>
        screenshotRegion >>
        wheelFile >>
        wheelData >>
        wheelSrc >>
        wheelRegion >>
        marqueeFile >>
        marqueeData >>
        marqueeSrc >>
        marqueeRegion >>
        textureFile >>
        textureData >>
        textureSrc >>
        textureRegion >>
        videoFile >>
        videoData >>
        videoSrc >>
        manualFile >>
        manualData >>
        manualSrc >>
        guides >>
        guidesSrc >>
        cheats >>
        cheatsSrc >>
        reviews >>
        reviewsSrc >>
        artbooks >>
        artbooksSrc >>
        vgmaps >>
        vgmapsSrc >>
        sprites >>
        spritesSrc >>
        chiptuneId >>
        chiptuneIdSrc >>
        chiptunePath >>
        chiptunePathSrc >>
        searchMatch >>
        cacheId >>
        source >>
        url >>
        sqrNotes >>
        parNotes >>
        videoFormat >>
        manualFormat >>
        baseName >>
        absoluteFilePath >>
        found >>
        emptyShell >>
        miscData >>
        pSValuePairs;
}

QByteArray GameEntry::serialize() const
{
  QByteArray result;
  QDataStream bWrite(&result, QIODevice::WriteOnly);
  bWrite << 
         id <<
         idSrc <<
         path <<
         canonical.name <<
         canonical.mameid <<
         canonical.size <<
         canonical.crc <<
         canonical.sha1 <<
         canonical.md5 <<
         title <<
         titleSrc <<
         platform <<
         platformSrc <<
         description <<
         descriptionSrc <<
         trivia <<
         triviaSrc <<
         releaseDate <<
         releaseDateSrc <<
         developer <<
         developerSrc <<
         publisher <<
         publisherSrc <<
         tags <<
         tagsSrc <<
         franchises <<
         franchisesSrc <<
         players <<
         playersSrc <<
         ages <<
         agesSrc <<
         rating <<
         ratingSrc <<
         completed <<
         favourite <<
         played <<
         timesPlayed <<
         lastPlayed <<
         firstPlayed <<
         timePlayed <<
         diskSize <<
         coverFile <<
         coverData <<
         coverSrc <<
         coverRegion <<
         screenshotFile <<
         screenshotData <<
         screenshotSrc <<
         screenshotRegion <<
         wheelFile <<
         wheelData <<
         wheelSrc <<
         wheelRegion <<
         marqueeFile <<
         marqueeData <<
         marqueeSrc <<
         marqueeRegion <<
         textureFile <<
         textureData <<
         textureSrc <<
         textureRegion <<
         videoFile <<
         videoData <<
         videoSrc <<
         manualFile <<
         manualData <<
         manualSrc <<
         guides <<
         guidesSrc <<
         cheats <<
         cheatsSrc <<
         reviews <<
         reviewsSrc <<
         artbooks <<
         artbooksSrc <<
         vgmaps <<
         vgmapsSrc <<
         sprites <<
         spritesSrc <<
         chiptuneId <<
         chiptuneIdSrc <<
         chiptunePath <<
         chiptunePathSrc <<
         searchMatch <<
         cacheId <<
         source <<
         url <<
         sqrNotes <<
         parNotes <<
         videoFormat <<
         manualFormat <<
         baseName <<
         absoluteFilePath <<
         found <<
         emptyShell <<
         miscData <<
         pSValuePairs;
  return result;
}

void GameEntry::resetMedia()
{
  CanonicalData empty;
  canonical = empty;
  coverData.clear();
  screenshotData.clear();
  wheelData.clear();
  marqueeData.clear();
  textureData.clear();
  videoData.clear();
  manualData.clear();
}

int GameEntry::getCompleteness() const
{
  double completeness = 0.0;
  int noOfTypes = 17;
  if(Skyscraper::config.videos) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.manuals) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.guides) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.cheats) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.reviews) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.artbooks) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.maps) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.chiptunes) {
    noOfTypes += 1;
  }
  if(Skyscraper::config.sprites) {
    noOfTypes += 1;
  }
  // If we are scraping we can be more exact:
  if(Skyscraper::config.scraper == "arcadedb") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
    if(Skyscraper::config.manuals) noOfTypes++;
  } else if(Skyscraper::config.scraper == "mamehistory") {
    noOfTypes = 4;
  } else if(Skyscraper::config.scraper == "chiptune") {
    noOfTypes = 3;
    if(Skyscraper::config.chiptunes) noOfTypes++;
  } else if(Skyscraper::config.scraper == "vgmaps") {
    noOfTypes = 2;
    if(Skyscraper::config.maps) noOfTypes++;
  } else if(Skyscraper::config.scraper == "everygame") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
    if(Skyscraper::config.manuals) noOfTypes++;
    if(Skyscraper::config.guides) noOfTypes++;
    if(Skyscraper::config.cheats) noOfTypes++;
    if(Skyscraper::config.reviews) noOfTypes++;
  } else if(Skyscraper::config.scraper == "docsdb") {
    noOfTypes = 2;
    if(Skyscraper::config.guides) noOfTypes++;
    if(Skyscraper::config.cheats) noOfTypes++;
    if(Skyscraper::config.reviews) noOfTypes++;
    if(Skyscraper::config.artbooks) noOfTypes++;
  } else if(Skyscraper::config.scraper == "gamefaqs") {
    noOfTypes = 13;
    if(Skyscraper::config.guides) noOfTypes++;
  } else if(Skyscraper::config.scraper == "vgfacts") {
    noOfTypes = 10;
  } else if(Skyscraper::config.scraper == "rawg") {
    noOfTypes = 12;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "giantbomb") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "igdb") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "launchbox") {
    noOfTypes = 16;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "customflags") {
    noOfTypes = 7;
  } else if(Skyscraper::config.scraper == "mobygames") {
    noOfTypes = 16;
    if(Skyscraper::config.manuals) noOfTypes++;
  } else if(Skyscraper::config.scraper == "offlinemobygames") {
    noOfTypes = 16;
    if(Skyscraper::config.manuals) noOfTypes++;
  } else if(Skyscraper::config.scraper == "openretro") {
    noOfTypes = 15;
    if(Skyscraper::config.manuals) noOfTypes++;
  } else if(Skyscraper::config.scraper == "vggeek") {
    noOfTypes = 17;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "exodos") {
    noOfTypes = 17;
    if(Skyscraper::config.videos) noOfTypes++;
    if(Skyscraper::config.manuals) noOfTypes++;
    if(Skyscraper::config.chiptunes) noOfTypes++;
    if(Skyscraper::config.guides) noOfTypes++;
    if(Skyscraper::config.cheats) noOfTypes++;
    if(Skyscraper::config.artbooks) noOfTypes++;
    if(Skyscraper::config.maps) noOfTypes++;
  } else if(Skyscraper::config.scraper == "gamebase") {
    noOfTypes = 15;
    if(Skyscraper::config.manuals) noOfTypes++;
    if(Skyscraper::config.guides) noOfTypes++;
    if(Skyscraper::config.cheats) noOfTypes++;
    if(Skyscraper::config.artbooks) noOfTypes++;
    if(Skyscraper::config.maps) noOfTypes++;
    if(Skyscraper::config.chiptunes) noOfTypes++;
  } else if(Skyscraper::config.scraper == "spriters") {
    noOfTypes = 2;
    if(Skyscraper::config.sprites) noOfTypes++;
  } else if(Skyscraper::config.scraper == "screenscraper") {
    noOfTypes = 17;
    if(Skyscraper::config.videos) noOfTypes++;
    if(Skyscraper::config.manuals) noOfTypes++;
  } else if(Skyscraper::config.scraper == "thegamesdb") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "offlinetgdb") {
    noOfTypes = 15;
    if(Skyscraper::config.videos) noOfTypes++;
  } else if(Skyscraper::config.scraper == "worldofspectrum") {
    noOfTypes = 14;
    if(Skyscraper::config.videos) noOfTypes++;
    if(Skyscraper::config.manuals) noOfTypes++;
  }
  double valuePerType = 100.0 / (double)noOfTypes;
  if(!id.isEmpty()) {
    completeness += valuePerType;
  }
  if(!title.isEmpty()) {
    completeness += valuePerType;
  }
  if(!platform.isEmpty()) {
    completeness += valuePerType;
  }
  if((!coverData.isNull() || !coverFile.isEmpty()) && !emptyShell) {
    completeness += valuePerType;
  }
  if(!screenshotData.isNull() || !screenshotFile.isEmpty()) {
    completeness += valuePerType;
  }
  if(!wheelData.isNull() || !wheelFile.isEmpty()) {
    completeness += valuePerType;
  }
  if(!marqueeData.isNull() || !marqueeFile.isEmpty()) {
    completeness += valuePerType;
  }
  if(!textureData.isNull() || !textureFile.isEmpty()) {
    completeness += valuePerType;
  }
  if(!description.isEmpty()) {
    completeness += valuePerType;
  }
  if(!trivia.isEmpty()) {
    completeness += valuePerType;
  }
  if(!releaseDate.isEmpty()) {
    completeness += valuePerType;
  }
  if(!developer.isEmpty()) {
    completeness += valuePerType;
  }
  if(!publisher.isEmpty()) {
    completeness += valuePerType;
  }
  if(!tags.isEmpty()) {
    completeness += valuePerType;
  }
  if(!franchises.isEmpty()) {
    completeness += valuePerType;
  }
  if(!rating.isEmpty()) {
    completeness += valuePerType;
  }
  if(!players.isEmpty()) {
    completeness += valuePerType;
  }
  if(!ages.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.videos && !videoFormat.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.manuals && !manualFormat.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.guides && !guides.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.cheats && !cheats.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.reviews && !reviews.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.artbooks && !artbooks.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.maps && !vgmaps.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.sprites && !sprites.isEmpty()) {
    completeness += valuePerType;
  }
  if(Skyscraper::config.chiptunes && !chiptuneId.isEmpty() && !chiptunePath.isEmpty()) {
    completeness += valuePerType;
  }
  return (int)completeness;
}

QDataStream &operator>>(QDataStream &in, GameEntry &game)
{
  in >> game.id >>
        game.idSrc >>
        game.path >>
        game.canonical.name >>
        game.canonical.mameid >>
        game.canonical.size >>
        game.canonical.crc >>
        game.canonical.sha1 >>
        game.canonical.md5 >>
        game.title >>
        game.titleSrc >>
        game.platform >>
        game.platformSrc >>
        game.description >>
        game.descriptionSrc >>
        game.trivia >>
        game.triviaSrc >>
        game.releaseDate >>
        game.releaseDateSrc >>
        game.developer >>
        game.developerSrc >>
        game.publisher >>
        game.publisherSrc >>
        game.tags >>
        game.tagsSrc >>
        game.franchises >>
        game.franchisesSrc >>
        game.players >>
        game.playersSrc >>
        game.ages >>
        game.agesSrc >>
        game.rating >>
        game.ratingSrc >>
        game.completed >>
        game.favourite >>
        game.played >>
        game.timesPlayed >>
        game.lastPlayed >>
        game.firstPlayed >>
        game.timePlayed >>
        game.diskSize >>
        game.coverFile >>
        game.coverData >>
        game.coverSrc >>
        game.coverRegion >>
        game.screenshotFile >>
        game.screenshotData >>
        game.screenshotSrc >>
        game.screenshotRegion >>
        game.wheelFile >>
        game.wheelData >>
        game.wheelSrc >>
        game.wheelRegion >>
        game.marqueeFile >>
        game.marqueeData >>
        game.marqueeSrc >>
        game.marqueeRegion >>
        game.textureFile >>
        game.textureData >>
        game.textureSrc >>
        game.textureRegion >>
        game.videoFile >>
        game.videoData >>
        game.videoSrc >>
        game.manualFile >>
        game.manualData >>
        game.manualSrc >>
        game.guides >>
        game.guidesSrc >>
        game.cheats >>
        game.cheatsSrc >>
        game.reviews >>
        game.reviewsSrc >>
        game.artbooks >>
        game.artbooksSrc >>
        game.vgmaps >>
        game.vgmapsSrc >>
        game.sprites >>
        game.spritesSrc >>
        game.chiptuneId >>
        game.chiptuneIdSrc >>
        game.chiptunePath >>
        game.chiptunePathSrc >>
        game.searchMatch >>
        game.cacheId >>
        game.source >>
        game.url >>
        game.sqrNotes >>
        game.parNotes >>
        game.videoFormat >>
        game.manualFormat >>
        game.baseName >>
        game.absoluteFilePath >>
        game.found >>
        game.emptyShell >>
        game.miscData >>
        game.pSValuePairs;
  return in;
}

QDataStream &operator<<(QDataStream &out, const GameEntry &game)
{
  out << game.id <<
         game.idSrc <<
         game.path <<
         game.canonical.name <<
         game.canonical.mameid <<
         game.canonical.size <<
         game.canonical.crc <<
         game.canonical.sha1 <<
         game.canonical.md5 <<
         game.title <<
         game.titleSrc <<
         game.platform <<
         game.platformSrc <<
         game.description <<
         game.descriptionSrc <<
         game.trivia <<
         game.triviaSrc <<
         game.releaseDate <<
         game.releaseDateSrc <<
         game.developer <<
         game.developerSrc <<
         game.publisher <<
         game.publisherSrc <<
         game.tags <<
         game.tagsSrc <<
         game.franchises <<
         game.franchisesSrc <<
         game.players <<
         game.playersSrc <<
         game.ages <<
         game.agesSrc <<
         game.rating <<
         game.ratingSrc <<
         game.completed <<
         game.favourite <<
         game.played <<
         game.timesPlayed <<
         game.lastPlayed <<
         game.firstPlayed <<
         game.timePlayed <<
         game.diskSize <<
         game.coverFile <<
         game.coverData <<
         game.coverSrc <<
         game.coverRegion <<
         game.screenshotFile <<
         game.screenshotData <<
         game.screenshotSrc <<
         game.screenshotRegion <<
         game.wheelFile <<
         game.wheelData <<
         game.wheelSrc <<
         game.wheelRegion <<
         game.marqueeFile <<
         game.marqueeData <<
         game.marqueeSrc <<
         game.marqueeRegion <<
         game.textureFile <<
         game.textureData <<
         game.textureSrc <<
         game.textureRegion <<
         game.videoFile <<
         game.videoData <<
         game.videoSrc <<
         game.manualFile <<
         game.manualData <<
         game.manualSrc <<
         game.guides <<
         game.guidesSrc <<
         game.cheats <<
         game.cheatsSrc <<
         game.reviews <<
         game.reviewsSrc <<
         game.artbooks <<
         game.artbooksSrc <<
         game.vgmaps <<
         game.vgmapsSrc <<
         game.sprites <<
         game.spritesSrc <<
         game.chiptuneId <<
         game.chiptuneIdSrc <<
         game.chiptunePath <<
         game.chiptunePathSrc <<
         game.searchMatch <<
         game.cacheId <<
         game.source <<
         game.url <<
         game.sqrNotes <<
         game.parNotes <<
         game.videoFormat <<
         game.manualFormat <<
         game.baseName <<
         game.absoluteFilePath <<
         game.found <<
         game.emptyShell <<
         game.miscData <<
         game.pSValuePairs;
  return out;
}

QDebug operator<<(QDebug debug, const GameEntry &game) {
    QDebugStateSaver saver(debug);
    debug.nospace() <<
      "\nField 'id': " << game.id <<
      "\nField 'idSrc': " << game.idSrc <<
      "\nField 'path': " << game.path <<
      "\nField 'Canonical name': " <<  game.canonical.name <<
      "\nField 'Canonical MAME id': " <<  game.canonical.mameid <<
      "\nField 'Canonical size': " <<  game.canonical.size <<
      "\nField 'Canonical CRC': " <<  game.canonical.crc <<
      "\nField 'Canonical SHA1': " <<  game.canonical.sha1 <<
      "\nField 'Canonical MD5': " <<  game.canonical.md5 <<
      "\nField 'title': " << game.title <<
      "\nField 'titleSrc': " << game.titleSrc <<
      "\nField 'platform': " << game.platform <<
      "\nField 'platformSrc': " << game.platformSrc <<
      "\nField 'description': " << game.description <<
      "\nField 'descriptionSrc': " << game.descriptionSrc <<
      "\nField 'trivia': " << game.trivia <<
      "\nField 'triviaSrc': " << game.triviaSrc <<
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
      "\nField 'diskSize': " << game.diskSize <<
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
      "\nField 'guides': " << game.guides <<
      "\nField 'guidesSrc': " << game.guidesSrc <<
      "\nField 'cheats': " << game.cheats <<
      "\nField 'cheatsSrc': " << game.cheatsSrc <<
      "\nField 'reviews': " << game.reviews <<
      "\nField 'reviewsSrc': " << game.reviewsSrc <<
      "\nField 'artbooks': " << game.artbooks <<
      "\nField 'artbooksSrc': " << game.artbooksSrc <<
      "\nField 'vgmaps': " << game.vgmaps <<
      "\nField 'vgmapsSrc': " << game.vgmapsSrc <<
      "\nField 'sprites': " << game.sprites <<
      "\nField 'spritesSrc': " << game.spritesSrc <<
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
