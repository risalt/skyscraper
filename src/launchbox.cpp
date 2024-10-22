/***************************************************************************
 *            launchbox.cpp
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

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QJsonArray>
#include <QMapIterator>
#include <QXmlStreamReader>
#include <QStringListIterator>

#include "launchbox.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"
#include "skyscraper.h"

constexpr int RETRIESMAX = 4;


LaunchBox::LaunchBox(Settings *config,
                     QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  incrementalScraping = true;  

  baseUrl = "https://images.launchbox-app.com/";
  QString databaseUrl = "https://gamesdb.launchbox-app.com/Metadata.zip";

  loadMaps();

  bool skipGame = false;
  bool loadedOk = false;
  int syntaxError = 0;
  int currentEntity = LBNULL;
  int currentTag = LBNULL;
  QString currentFileName = "";
  QString currentName = "";

  if (Platform::get().getFamily(config->platform) == "arcade") {
    QXmlStreamReader mame;
    QFile mameFile (config->launchBoxMameDb);
    if (mameFile.open(QIODevice::ReadOnly)) {
      mame.setDevice(&mameFile);
      if (mame.readNext() && mame.isStartDocument() &&
          mame.readNextStartElement() &&
          schema.value(mame.name().toString()) == LBLAUNCHBOX) {
        while (!mame.atEnd() && !mame.hasError() && 
              (mame.readNextStartElement())) {
          currentTag = schema.value(mame.name().toString());
          switch (currentEntity) {
            case LBMAMEFILE:
              //printf("STARTOFENTITY\n");
              if (!currentFileName.isEmpty() && !currentName.isEmpty()) {
                if (mameNameToLongName.contains(currentFileName)) {
                  if (mameNameToLongName[currentFileName] != currentName) {
                    printf("ERROR: Detected duplicated short name: %s ('%s' and '%s')",
                           currentFileName.toStdString().c_str(),
                           mameNameToLongName[currentFileName].toStdString().c_str(),
                           currentName.toStdString().c_str());
                  }
                }
                else {
                  mameNameToLongName[currentFileName] = currentName;
                }
              }
              else {
                syntaxError++;printf("A");
              }
            break;
            case LBNULL:
            break;
            default:
              syntaxError++;printf("U");
          }
          currentName = "";
          currentFileName = "";
          currentEntity = LBNULL;
          switch (currentTag) {
            case LBMAMEFILE:
              currentEntity = currentTag;
              while (!mame.atEnd() && !mame.hasError() && 
                    (mame.readNextStartElement())) {
                //printf(".. %s: \n", mame.name().toString().toStdString().c_str());
                currentTag = schema.value(mame.name().toString());
                switch (currentTag) {
                  case LBFILENAME:
                    currentFileName = mame.readElementText();
                  break;
                  case LBNAME:
                    currentName = mame.readElementText();
                  break;
                  default:
                    mame.skipCurrentElement();
                }
                //printf("N %s.\n", currentName.remove(QChar('\n')).left(60).toStdString().c_str());
                //printf("F %s.\n", currentFileName.remove(QChar('\n')).left(60).toStdString().c_str());
              }
            break;
            default:
              mame.skipCurrentElement();
          }
        }
        loadedOk = mame.hasError();
      }
      else {
        mame.raiseError("ERROR: Incorrect XML document or not a LaunchBox DB.\n");
      }
    }
    else {
      printf("ERROR: Database file '%s' cannot be found or is corrupted.\n",
             config->launchBoxMameDb.toStdString().c_str());
      printf("INFO: Please download it from '%s'\n", databaseUrl.toStdString().c_str());
      Skyscraper::removeLockAndExit(1);
    }
    mame.clear();
    mameFile.close();
    if (syntaxError) {
      printf("WARNING: The MAME database contains %d syntax error(s). The associated records have been ignored.\n", syntaxError);
    }
    if (!loadedOk) {
      printf("WARNING: The parser has reported errors.\n");
    }
    printf("INFO: MAME database has been parsed, %d equivalences have been loaded into memory.\n",
           mameNameToLongName.count());
  }
    
  skipGame = false;
  loadedOk = false;
  syntaxError = 0;
  currentEntity = LBNULL;
  currentTag = LBNULL;
  currentFileName = "";
  int currentId = LBNULL;
  QString currentText = "";
  QString currentType = "";
  QString currentRegion = "";
  GameEntry currentGame;
  
  QXmlStreamReader reader;
  QFile xmlFile (config->launchBoxDb);
  if (xmlFile.open(QIODevice::ReadOnly)) {
    reader.setDevice(&xmlFile);
    if (reader.readNext() && reader.isStartDocument() &&
        reader.readNextStartElement() &&
        schema.value(reader.name().toString()) == LBLAUNCHBOX) {
      while (!reader.atEnd() && !reader.hasError() && 
            (reader.readNextStartElement())) {
        //printf(". %s\n", reader.name().toString().toStdString().c_str());
        currentTag = schema.value(reader.name().toString());
        switch (currentEntity) {
          case LBGAMEIMAGE:
            if (currentId and !currentType.isEmpty() and !currentFileName.isEmpty()) {
              // Save or update currentGame in launchBoxDb
              QMapIterator<int, QStringList> prios(priorityImages);
              while (prios.hasNext()) {
                prios.next();
                if (prios.value().contains(currentType)) {
                  GameEntry * existingGame = nullptr;
                  if (!launchBoxDb.contains(currentId)) {
                    launchBoxDb[currentId] = currentGame;
                  }
                  existingGame = &launchBoxDb[currentId];
                  bool keepExisting = false;
                  switch (prios.key()) {
                    case COVER:
                      if (!existingGame->coverFile.isEmpty()) {
                        if (existingGame->coverSrc == currentType) {
                          if (currentRegion.isEmpty() || 
                              (priorityRegions.indexOf(existingGame->coverRegion) >=
                              priorityRegions.indexOf(currentRegion))) {
                            keepExisting = true;
                          }
                        }
                        else if (prios.value().indexOf(existingGame->coverSrc) >
                            prios.value().indexOf(currentType)) {
                          keepExisting = true;
                        }
                      }
                      if (!keepExisting) {
                        existingGame->coverFile = currentFileName;
                        existingGame->coverSrc = currentType;
                        existingGame->coverRegion = currentRegion;
                      }
                    break;
                    case SCREENSHOT:
                      if (!existingGame->screenshotFile.isEmpty()) {
                        if (existingGame->screenshotSrc == currentType) {
                          if (currentRegion.isEmpty() || 
                              (priorityRegions.indexOf(existingGame->screenshotRegion) >=
                              priorityRegions.indexOf(currentRegion))) {
                            keepExisting = true;
                          }
                        }
                        else if (prios.value().indexOf(existingGame->screenshotSrc) >
                            prios.value().indexOf(currentType)) {
                          keepExisting = true;
                        }
                      }
                      if (!keepExisting) {
                        existingGame->screenshotFile = currentFileName;
                        existingGame->screenshotSrc = currentType;
                        existingGame->screenshotRegion = currentRegion;
                      }
                    break;
                    case WHEEL:
                      if (!existingGame->wheelFile.isEmpty()) {
                        if (existingGame->wheelSrc == currentType) {
                          if (currentRegion.isEmpty() || 
                              (priorityRegions.indexOf(existingGame->wheelRegion) >=
                              priorityRegions.indexOf(currentRegion))) {
                            keepExisting = true;
                          }
                        }
                        else if (prios.value().indexOf(existingGame->wheelSrc) >
                            prios.value().indexOf(currentType)) {
                          keepExisting = true;
                        }
                      }
                      if (!keepExisting) {
                        existingGame->wheelFile = currentFileName;
                        existingGame->wheelSrc = currentType;
                        existingGame->wheelRegion = currentRegion;
                      }
                    break;
                    case MARQUEE:
                      if (!existingGame->marqueeFile.isEmpty()) {
                        if (existingGame->marqueeSrc == currentType) {
                          if (currentRegion.isEmpty() || 
                              (priorityRegions.indexOf(existingGame->marqueeRegion) >=
                              priorityRegions.indexOf(currentRegion))) {
                            keepExisting = true;
                          }
                        }
                        else if (prios.value().indexOf(existingGame->marqueeSrc) >
                            prios.value().indexOf(currentType)) {
                          keepExisting = true;
                        }
                      }
                      if (!keepExisting) {
                        existingGame->marqueeFile = currentFileName;
                        existingGame->marqueeSrc = currentType;
                        existingGame->marqueeRegion = currentRegion;
                      }
                    break;
                    case TEXTURE:
                      if (!existingGame->textureFile.isEmpty()) {
                        if (existingGame->textureSrc == currentType) {
                          if (currentRegion.isEmpty() || 
                              (priorityRegions.indexOf(existingGame->textureRegion) >=
                              priorityRegions.indexOf(currentRegion))) {
                            keepExisting = true;
                          }
                        }
                        else if (prios.value().indexOf(existingGame->textureSrc) >
                            prios.value().indexOf(currentType)) {
                          keepExisting = true;
                        }
                      }
                      if (!keepExisting) {
                        existingGame->textureFile = currentFileName;
                        existingGame->textureSrc = currentType;
                        existingGame->textureRegion = currentRegion;
                      }
                    break;
                    default:
                      ;
                  }
                  break;
                }
              }
            }
            else {
              // Apparently there are entries without DatabaseID... Let's ignore them.
              syntaxError++;
              //printf("%lld|%s|%s\n", reader.lineNumber(),
              //       currentGame.title.toStdString().c_str(), currentFileName.toStdString().c_str());
            }
          break;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
          case LBGAME:
            if (currentId) {
              GameEntry * existingGame = nullptr;
              if (!skipGame) {
                if (launchBoxDb.contains(currentId)) {
                  existingGame = &launchBoxDb[currentId];
                  if (!launchBoxDb[currentId].platform.isEmpty()) {
                    printf("ERROR: Detected duplicated Game Id: %d (%s and %s)... \n",
                           currentId, launchBoxDb.value(currentId).title.toStdString().c_str(),
                           currentGame.title.toStdString().c_str());
                    if (launchBoxDb.value(currentId).getCompleteness() >= currentGame.getCompleteness()) {
                      skipGame = true;
                    }
                  }
                }
                if (!skipGame) {
                  if (existingGame) {
                    existingGame->title = currentGame.title;
                    existingGame->platform = currentGame.platform;
                    existingGame->description = currentGame.description;
                    existingGame->releaseDate = currentGame.releaseDate;
                    existingGame->developer = currentGame.developer;
                    existingGame->publisher = currentGame.publisher;
                    existingGame->tags = currentGame.tags;
                    existingGame->players = currentGame.players;
                    existingGame->ages = currentGame.ages;
                    existingGame->rating = currentGame.rating;
                    existingGame->videoFile = currentGame.videoFile;
                  }
                  else {                  
                    launchBoxDb[currentId] = currentGame;
                  }
                }
              }
            }
            else {
              // Apparently there are entries without DatabaseID... Let's ignore them.
              syntaxError++;
              //printf("%lld|%s|%s\n", reader.lineNumber(),
              //       currentGame.title.toStdString().c_str(), currentFileName.toStdString().c_str());
            }
          case LBGAMEALTERNATENAME:
            if (currentId && !currentGame.title.isEmpty()) {
              if (!skipGame) {
                QString searchString = currentGame.title.toLower();
                // printf("A: %s\n", searchString.toStdString().c_str());
                if (!searchNameToId.contains(searchString, currentId)) {
                  searchNameToId.insert(searchString, currentId);
                }
                searchString = StrTools::sanitizeName(searchString);
                // printf("B: %s\n", searchString.toStdString().c_str());
                if (!searchNameToId.contains(searchString, currentId)) {
                  searchNameToId.insert(searchString, currentId);
                }
                searchString = StrTools::sanitizeName(currentGame.title, true);
                // printf("C: %s\n", searchString.toStdString().c_str());
                if (!searchNameToId.contains(searchString, currentId)) {
                  searchNameToId.insert(searchString, currentId);
                }
                searchString = currentGame.title;
                searchString = searchString.replace("&", " and ").remove("'");
                searchString = NameTools::convertToIntegerNumeral(searchString);
                searchString = NameTools::getUrlQueryName(NameTools::removeArticle(searchString), -1, " ");
                searchString = StrTools::sanitizeName(searchString, true);
                // printf("D: %s\n", searchString.toStdString().c_str());
                if (!searchNameToId.contains(searchString, currentId)) {
                  searchNameToId.insert(searchString, currentId);
                }
              }
            }
            else if (currentEntity == LBGAMEALTERNATENAME) {
              syntaxError++;printf("B");
            }
#pragma GCC diagnostic pop
          break;
          case LBNULL:
          break;
          default:
            syntaxError++;printf("C");
        }
        // currentGame.resetMedia();
        currentGame = GameEntry();
        currentText = "";
        currentType = "";
        currentRegion = "";
        currentFileName = "";
        currentId = LBNULL;
        currentEntity = LBNULL;
        skipGame = false;
        switch (currentTag) {
          case LBGAME:
          case LBGAMEALTERNATENAME:
          case LBGAMEIMAGE:
            currentEntity = currentTag;
            while (!reader.atEnd() && !reader.hasError() && 
                  (reader.readNextStartElement())) {
              //printf(".. %s: ", reader.name().toString().toStdString().c_str());
              currentTag = schema.value(reader.name().toString());
              switch (currentTag) {
                case LBRELEASEYEAR:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("D");
                  }
                  else if (!currentText.isEmpty()) {
                    if (currentText.length() == 4 && currentGame.releaseDate.isEmpty()) {
                      currentGame.releaseDate = StrTools::conformReleaseDate(currentText);
                    }
                  }
                break;
                case LBNAME:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("E");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.title = currentText;
                  }
                  else {
                    skipGame = true;
                  }
                break;
                case LBRELEASEDATE:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("F");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.releaseDate = StrTools::conformReleaseDate(currentText.left(10));
                  }
                break;
                case LBOVERVIEW:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("G");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.description = currentText;
                  }
                break;
                case LBMAXPLAYERS:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("H");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.players = StrTools::conformPlayers(currentText);
                  }
                break;
                case LBVIDEOURL:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("I");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.videoFile = currentText;
                  }
                break;
                case LBDATABASEID:
                  currentText = reader.readElementText();
                  if (currentEntity) {
                    currentId = currentText.toInt();
                  }
                  if (!currentEntity || !currentId) {
                    skipGame = true;
                    syntaxError++;printf("J");
                  }
                break;
                case LBCOMMUNITYRATING:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("K");
                  }
                  else if (!currentText.isEmpty()) {
                    float rating = currentText.toFloat();
                    if (rating > 0.0) {
                      currentGame.rating = QString::number(rating/5.0, 'f', 3);
                    }
                  }
                break;
                case LBPLATFORM:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("L");
                  } 
                  else if (!currentText.isEmpty() &&
                          platformNameToFinalCode.contains(currentText)) {
                    currentGame.platform = currentText;
                    if (platformNameToFinalCode[currentText] != config->platform  &&
                        platformNameToFinalCode[currentText] != platformCodeToFinalCode.value(config->platform)) {
                      skipGame = true;
                    }
                  }
                  else {
                    skipGame = true;
                  }
                break;
                case LBESRB:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("M");
                  }
                  else if (!currentText.isEmpty()) {
                    if (!currentText.contains("Not Rated", Qt::CaseInsensitive) ||
                        !currentText.contains("Rating Pending", Qt::CaseInsensitive)) {
                      currentGame.ages = StrTools::conformAges(currentText);
                    }
                  }
                break;
                case LBGENRES:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("N");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.tags = StrTools::conformTags(currentText.replace(';', ','));
                  }
                break;
                case LBDEVELOPER:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("O");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.developer = currentText;
                  }
                break;
                case LBPUBLISHER:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAME) {
                    syntaxError++;printf("P");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.publisher = currentText;
                  }
                break;
                case LBALTERNATENAME:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAMEALTERNATENAME) {
                    syntaxError++;printf("Q");
                  }
                  else if (!currentText.isEmpty()) {
                    currentGame.title = currentText;
                  }
                break;
                case LBREGION:
                  currentText = reader.readElementText();
                  if (!currentEntity) {
                    syntaxError++;printf("R");
                  }
                  else if (!currentText.isEmpty() && (currentEntity == LBGAMEIMAGE)) {
                    currentRegion = currentText;
                  }
                break;
                case LBFILENAME:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAMEIMAGE) {
                    syntaxError++;printf("S");
                  }
                  else if (!currentText.isEmpty()) {
                    currentFileName = currentText;
                  }
                break;
                case LBTYPE:
                  currentText = reader.readElementText();
                  if (currentEntity != LBGAMEIMAGE) {
                    syntaxError++;printf("T");
                  }
                  else if (!currentText.isEmpty()) {
                    currentType = currentText;
                  }
                break;
                default:
                  reader.skipCurrentElement();
                  //printf("..2\n");
                  currentText = "";
              }
              //printf("%s.\n", currentText.remove(QChar('\n')).left(60).toStdString().c_str());
            }
          break;
          default:
            reader.skipCurrentElement();
            //printf(".1\n");
        }
      }
      loadedOk = reader.hasError();
      // printf("ERROR: XML Parser has reported an error: %s\n",
      //        reader.errorString().toStdString().c_str());
    }
    else {
      reader.raiseError("ERROR: Incorrect XML document or not a LaunchBox DB.\n");
    }
  }
  else {
    printf("ERROR: Database file '%s' cannot be found or is corrupted.\n",
           config->launchBoxDb.toStdString().c_str());
    printf("INFO: Please download it from '%s'\n", databaseUrl.toStdString().c_str());
    Skyscraper::removeLockAndExit(1);
  }
  reader.clear();
  xmlFile.close();
  
  if (syntaxError) {
    printf("WARNING: The database contains %d syntax error(s). The associated records have been ignored.\n", syntaxError);
  }
  if (!loadedOk) {
    printf("WARNING: The parser has reported errors.\n");
  }

  // Delete all entries in launchBoxDb with empty title and platform.
  for (auto item = launchBoxDb.begin(); item != launchBoxDb.end();) {
    if (item.value().title.isEmpty() || item.value().platform.isEmpty()) {
      item = launchBoxDb.erase(item);
    }
    else {
      ++item;
    }
  }
  // Delete all entries in searchNameToId with an id that does not exist in launchBoxDb.
  for (auto item = searchNameToId.begin(); item != searchNameToId.end();) {
    if (!launchBoxDb.contains(item.value())) {
      item = searchNameToId.erase(item);
    }
    else {
      ++item;
    }
  }
  
  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(100);
  limitTimer.setSingleShot(false);
  limitTimer.start();

  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(AGES);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(RATING);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(VIDEO);
  
  printf("INFO: LaunchBox database has been parsed, %d games have been loaded into memory.\n",
         launchBoxDb.count());

  // Print list of game names (including alternates):
  if(config->verbosity >= 3) {
    foreach (const QString &key, searchNameToId.uniqueKeys()) {
      foreach (int slot_id, searchNameToId.values(key)) {
        qDebug()<<key<<slot_id;
      }
    }
  }
}

LaunchBox::~LaunchBox()
{
}

QList<QString> LaunchBox::getSearchNames(const QFileInfo &info)
{
  QList<QString> searchNames = {};
  QString file = info.completeBaseName();
  
  if (Platform::get().getFamily(config->platform) == "arcade") {
    // Convert filename from short mame-style to regular one
    if (mameNameToLongName.contains(file)) {
      printf("INFO: Short Mame-style name '%s' converted to '%s'.\n",
             file.toStdString().c_str(),
             mameNameToLongName.value(file).toStdString().c_str());
      searchNames << mameNameToLongName.value(file);
      return searchNames;
    }
    else if (!config->aliasMap[file].isEmpty()) {
      printf("INFO: Alias found converting '%s' to '%s'.\n", 
             file.toStdString().c_str(),
             config->aliasMap[file].toStdString().c_str());
      searchNames << config->aliasMap[file];
      return searchNames;
    }
    else {
      return AbstractScraper::getSearchNames(info);
    }
  }
  else if (!config->aliasMap[file].isEmpty()) {
    printf("INFO: Alias found converting '%s' to '%s'.\n", 
           file.toStdString().c_str(),
           config->aliasMap[file].toStdString().c_str());
    searchNames << config->aliasMap[file];
    return searchNames;
  }

  searchNames << file;
  return searchNames;
  // return AbstractScraper::getSearchNames(info);
}

void LaunchBox::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString)
{
  
  QList<int> matches = {};
  QList<int> match = {};
  QListIterator<int> matchIterator(matches);

  QString sanitizedName = searchName.toLower();
  if(config->verbosity >= 2) {
    printf("1: %s\n", sanitizedName.toStdString().c_str());
    if (searchNameToId.contains(sanitizedName)) {
      printf("Found!\n");
    }
  }
  if (searchNameToId.contains(sanitizedName)) {
    match = searchNameToId.values(sanitizedName);
    matchIterator = QListIterator<int> (match);
    while (matchIterator.hasNext()) {
      int databaseId = matchIterator.next();
      if (!matches.contains(databaseId)) {
        matches << databaseId;
      }
    }
  }
  sanitizedName = StrTools::sanitizeName(sanitizedName);
  if(config->verbosity >= 2) {
    printf("2: %s\n", sanitizedName.toStdString().c_str());
    if (searchNameToId.contains(sanitizedName)) {
      printf("Found!\n");
    }
  }
  if (searchNameToId.contains(sanitizedName)) {
    match = searchNameToId.values(sanitizedName);
    matchIterator = QListIterator<int> (match);
    while (matchIterator.hasNext()) {
      int databaseId = matchIterator.next();
      if (!matches.contains(databaseId)) {
        matches << databaseId;
      }
    }
  }
  sanitizedName = StrTools::sanitizeName(sanitizedName, true);
  if(config->verbosity >= 2) {
    printf("3: %s\n", sanitizedName.toStdString().c_str());
    if (searchNameToId.contains(sanitizedName)) {
      printf("Found!\n");
    }
  }
  if (searchNameToId.contains(sanitizedName)) {
    match = searchNameToId.values(sanitizedName);
    matchIterator = QListIterator<int> (match);
    while (matchIterator.hasNext()) {
      int databaseId = matchIterator.next();
      if (!matches.contains(databaseId)) {
        matches << databaseId;
      }
    }
  }
  sanitizedName = searchName;
  sanitizedName = sanitizedName.replace("&", " and ").remove("'");
  sanitizedName = NameTools::convertToIntegerNumeral(sanitizedName);
  sanitizedName = NameTools::getUrlQueryName(NameTools::removeArticle(sanitizedName), -1, " ");
  sanitizedName = StrTools::sanitizeName(sanitizedName, true);
  if(config->verbosity >= 2) {
    printf("4: %s\n", sanitizedName.toStdString().c_str());
    if (searchNameToId.contains(sanitizedName)) {
      printf("Found!\n");
    }
  }
  if (searchNameToId.contains(sanitizedName)) {
    match = searchNameToId.values(sanitizedName);
    matchIterator = QListIterator<int> (match);
    while (matchIterator.hasNext()) {
      int databaseId = matchIterator.next();
      if (!matches.contains(databaseId)) {
        matches << databaseId;
      }
    }
  }

  // If not matches found, try using a fuzzy matcher based on the Damerau/Levenshtein text distance: 
  if(matches.isEmpty()) {
    if(config->fuzzySearch && sanitizedName.size() >= 6) {
      int maxDistance = config->fuzzySearch;
      if((sanitizedName.size() <= 10) || (config->fuzzySearch < 0)) {
        maxDistance = 1;
      }
      QListIterator<QString> keysIterator(searchNameToId.keys());
      while (keysIterator.hasNext()) {
        QString name = keysIterator.next();
        if(StrTools::onlyNumbers(name) == StrTools::onlyNumbers(sanitizedName)) {
          int distance = StrTools::distanceBetweenStrings(sanitizedName, name);
          if(distance <= maxDistance) {
            printf("FuzzySearch: Found %s = %s (distance %d)!\n", sanitizedName.toStdString().c_str(),
                   name.toStdString().c_str(), distance);
            match = searchNameToId.values(name);
            matchIterator = QListIterator<int> (match);
            while (matchIterator.hasNext()) {
              int databaseId = matchIterator.next();
              if (!matches.contains(databaseId)) {
                matches << databaseId;
              }
            }
          }
        }
      }
    }
  }
  
  printf("INFO: %d match(es) found.\n", matches.size()); 
  matchIterator = QListIterator<int> (matches);
  while (matchIterator.hasNext()) {
    GameEntry game;
    int databaseId = matchIterator.next();
    if (launchBoxDb.contains(databaseId)) {
      game.id = QString::number(databaseId);
      game.title = launchBoxDb[databaseId].title;
      game.platform = launchBoxDb[databaseId].platform;
      gameEntries.append(game);
    }
    else {
      printf("ERROR: Internal error: Database Id '%d' not found in the in-memory database.\n",
             databaseId);
    }
  }
}

void LaunchBox::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  if (!launchBoxDb.contains(game.id.toInt())) {
    printf("ERROR: Internal error: Database Id '%s' not found in the in-memory database.\n",
           game.id.toStdString().c_str());
    return;
  }
  /*printf("Scraping game '%s' with id '%s', for platform '%s'.\n",
      game.title.toStdString().c_str(),
      game.id.toStdString().c_str(),
      game.platform.toStdString().c_str());*/

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case DESCRIPTION:
      getDescription(game);
      break;
    case DEVELOPER:
      getDeveloper(game);
      break;
    case PUBLISHER:
      getPublisher(game);
      break;
    case PLAYERS:
      getPlayers(game);
      break;
    case AGES:
      getAges(game);
      break;
    case RATING:
      getRating(game);
      break;
    case TAGS:
      getTags(game);
      break;
    case FRANCHISES:
      getFranchises(game);
      break;
    case RELEASEDATE:
      getReleaseDate(game);
      break;
    case COVER:
      if(config->cacheCovers) {
        if ((!cache) || (cache && cache->coverData.isNull())) {
          getCover(game);
        }
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots) {
        if ((!cache) || (cache && cache->screenshotData.isNull())) {
          getScreenshot(game);
        }
      }
      break;
    case WHEEL:
      if(config->cacheWheels) {
        if ((!cache) || (cache && cache->wheelData.isNull())) {
          getWheel(game);
        }
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees) {
        if ((!cache) || (cache && cache->marqueeData.isNull())) {
          getMarquee(game);
        }
      }
      break;
    case TEXTURE:
      if (config->cacheTextures) {
        if ((!cache) || (cache && cache->textureData.isNull())) {
          getTexture(game);
        }
      }
      break;
    case VIDEO:
      if((config->videos) && (!sharedBlobs.contains("video"))) {
        if ((!cache) || (cache && cache->videoData == "")) {
          getVideo(game);
        }
      }
      break;
    case MANUAL:
      if((config->manuals) && (!sharedBlobs.contains("manual"))) {
        if ((!cache) || (cache && cache->manualData == "")) {
          getManual(game);
        }
      }
      break;
    default:
      ;
    }
  }
}

void LaunchBox::getReleaseDate(GameEntry &game)
{
  game.releaseDate = launchBoxDb.value(game.id.toInt()).releaseDate;
}

void LaunchBox::getDeveloper(GameEntry &game)
{
  game.developer = launchBoxDb.value(game.id.toInt()).developer;
}

void LaunchBox::getPublisher(GameEntry &game)
{
  game.publisher = launchBoxDb.value(game.id.toInt()).publisher;
}

void LaunchBox::getDescription(GameEntry &game)
{
  game.description = launchBoxDb.value(game.id.toInt()).description;
}

void LaunchBox::getPlayers(GameEntry &game)
{
  game.players = launchBoxDb.value(game.id.toInt()).players;
}

void LaunchBox::getAges(GameEntry &game)
{
  game.ages = launchBoxDb.value(game.id.toInt()).ages;
}

void LaunchBox::getRating(GameEntry &game)
{
  game.rating = launchBoxDb.value(game.id.toInt()).rating;
}

void LaunchBox::getTags(GameEntry &game)
{
  game.tags = launchBoxDb.value(game.id.toInt()).tags;
}

void LaunchBox::getCover(GameEntry &game)
{
  limiter.exec();
  netComm->request(baseUrl + launchBoxDb.value(game.id.toInt()).coverFile);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.coverData = netComm->getData();
  }
}

void LaunchBox::getScreenshot(GameEntry &game)
{
  limiter.exec();
  netComm->request(baseUrl + launchBoxDb.value(game.id.toInt()).screenshotFile);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

void LaunchBox::getWheel(GameEntry &game)
{
  limiter.exec();
  netComm->request(baseUrl + launchBoxDb.value(game.id.toInt()).wheelFile);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void LaunchBox::getMarquee(GameEntry &game)
{
  limiter.exec();
  netComm->request(baseUrl + launchBoxDb.value(game.id.toInt()).marqueeFile);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.marqueeData = netComm->getData();
  }
}

void LaunchBox::getTexture(GameEntry &game)
{
  limiter.exec();
  netComm->request(baseUrl + launchBoxDb.value(game.id.toInt()).textureFile);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.textureData = netComm->getData();
  }
}

void LaunchBox::getVideo(GameEntry &game)
{
  QString videoUrl = launchBoxDb.value(game.id.toInt()).videoFile;
  //printf("Downloading video '%s'.\n", videoUrl.toStdString().c_str());
  if(videoUrl.contains("youtube") || videoUrl.contains("youtu.be")) {
    getOnlineVideo(videoUrl, game);
  }
  else if(!videoUrl.isEmpty()) {
    bool moveOn = true;
    for(int retries = 0; retries < RETRIESMAX; ++retries) {
      limiter.exec();
      netComm->request(videoUrl);
      q.exec();
      game.videoData = netComm->getData();
      // Make sure received data is actually a video file
      QByteArray contentType = netComm->getContentType();
      if(netComm->getError(config->verbosity) == QNetworkReply::NoError &&
         contentType.contains("video/") &&
         game.videoData.size() > 4096) {
        game.videoFormat = contentType.mid(contentType.indexOf("/") + 1,
                                           contentType.length() - contentType.indexOf("/") + 1);
      } else {
        game.videoData = "";
        moveOn = false;
      }
      if(moveOn)
        break;
    }
  }
}

void LaunchBox::loadMaps()
{
  platformCodeToFinalCode["snes-music"] = "snes";
  platformCodeToFinalCode["pcengine-music"] = "pcengine";
  platformCodeToFinalCode["genesis-music"] = "genesis";
  platformCodeToFinalCode["lcdhandheld"] = "gameandwatch";
  platformCodeToFinalCode["cave"] = "arcade";
  platformCodeToFinalCode["cps1"] = "arcade";
  platformCodeToFinalCode["cps2"] = "arcade";
  platformCodeToFinalCode["cps3"] = "arcade";
  platformCodeToFinalCode["daphne"] = "arcade";
  platformCodeToFinalCode["dataeast"] = "arcade";
  platformCodeToFinalCode["igs"] = "arcade";
  platformCodeToFinalCode["irem"] = "arcade";
  platformCodeToFinalCode["jaleco"] = "arcade";
  platformCodeToFinalCode["kaneko"] = "arcade";
  platformCodeToFinalCode["konami"] = "arcade";
  platformCodeToFinalCode["midway"] = "arcade";
  platformCodeToFinalCode["nichibutsu"] = "arcade";
  platformCodeToFinalCode["nmk"] = "arcade";
  platformCodeToFinalCode["psikyo"] = "arcade";
  platformCodeToFinalCode["seta"] = "arcade";
  platformCodeToFinalCode["toaplan"] = "arcade";

  platformNameToFinalCode["3DO Interactive Multiplayer"] = "3do";
  platformNameToFinalCode["Acorn Archimedes"] = "acorn";
  platformNameToFinalCode["Acorn Electron"] = "acornelectron";
  platformNameToFinalCode["Amstrad CPC"] = "amstradcpc";
  platformNameToFinalCode["Amstrad GX4000"] = "amstradgx4000";
  platformNameToFinalCode["Apple II"] = "apple2";
  platformNameToFinalCode["Apple IIGS"] = "apple2gs";
  platformNameToFinalCode["Apple Mac OS"] = "macintosh";
  platformNameToFinalCode["Arcade"] = "arcade";
  platformNameToFinalCode["Atari 2600"] = "atari2600";
  platformNameToFinalCode["Atari 5200"] = "atari5200";
  platformNameToFinalCode["Atari 7800"] = "atari7800";
  platformNameToFinalCode["Atari 800"] = "atari800";
  platformNameToFinalCode["Atari Jaguar"] = "atarijaguar";
//  platformNameToFinalCode["Atari Jaguar CD"] = "atarijaguarcd";
  platformNameToFinalCode["Atari Lynx"] = "atarilynx";
  platformNameToFinalCode["Atari ST"] = "atarist";
  platformNameToFinalCode["Bally Astrocade"] = "astrocade";
  platformNameToFinalCode["BBC Microcomputer System"] = "bbcmicro";
  platformNameToFinalCode["Casio Loopy"] = "loopy";
  platformNameToFinalCode["Casio PV-1000"] = "pv1000";
  platformNameToFinalCode["ColecoVision"] = "coleco";
  platformNameToFinalCode["Commodore 128"] = "c64";
  platformNameToFinalCode["Commodore 64"] = "c64";
  platformNameToFinalCode["Commodore Amiga"] = "amiga";
  platformNameToFinalCode["Commodore Amiga CD32"] = "amigacd32";
  platformNameToFinalCode["Commodore CDTV"] = "amigacdtv";
  platformNameToFinalCode["Dragon 32/64"] = "dragon32";
  platformNameToFinalCode["Emerson Arcadia 2001"] = "arcadia";
  platformNameToFinalCode["Epoch Super Cassette Vision"] = "epochscv";
  platformNameToFinalCode["Fairchild Channel F"] = "channelf";
  platformNameToFinalCode["Fujitsu FM Towns Marty"] = "fmtowns";
  platformNameToFinalCode["Fujitsu FM-7"] = "fm7";
  platformNameToFinalCode["GamePark GP32"] = "gp32";
  platformNameToFinalCode["GCE Vectrex"] = "vectrex";
  platformNameToFinalCode["Linux"] = "linux";
  platformNameToFinalCode["Magnavox Odyssey 2"] = "videopac";
  platformNameToFinalCode["Mattel Intellivision"] = "intellivision";
  platformNameToFinalCode["Mega Duck"] = "megaduck";
  platformNameToFinalCode["Microsoft MSX"] = "msx";
  platformNameToFinalCode["Microsoft MSX2"] = "msx2";
  platformNameToFinalCode["Microsoft MSX2+"] = "msx2";
  platformNameToFinalCode["Microsoft Xbox"] = "xbox";
  platformNameToFinalCode["Microsoft Xbox 360"] = "xbox360";
  platformNameToFinalCode["MS-DOS"] = "pc";
  platformNameToFinalCode["Namco System 22"] = "namco";
  platformNameToFinalCode["NEC PC-8801"] = "pc88";
  platformNameToFinalCode["NEC PC-9801"] = "pc98";
  platformNameToFinalCode["NEC PC-FX"] = "pcfx";
  platformNameToFinalCode["NEC TurboGrafx-16"] = "pcengine";
  platformNameToFinalCode["NEC TurboGrafx-CD"] = "pcenginecd";
  platformNameToFinalCode["Nintendo 3DS"] = "3ds";
  platformNameToFinalCode["Nintendo 64"] = "n64";
  platformNameToFinalCode["Nintendo DS"] = "nds";
  platformNameToFinalCode["Nintendo Entertainment System"] = "nes";
  platformNameToFinalCode["Nintendo Famicom Disk System"] = "fds";
  platformNameToFinalCode["Nintendo Game &amp; Watch"] = "gameandwatch";
  platformNameToFinalCode["Nintendo Game Boy"] = "gb";
  platformNameToFinalCode["Nintendo Game Boy Advance"] = "gba";
  platformNameToFinalCode["Nintendo Game Boy Color"] = "gbc";
  platformNameToFinalCode["Nintendo GameCube"] = "gc";
  platformNameToFinalCode["Nintendo Pokemon Mini"] = "pokemini";
  platformNameToFinalCode["Nintendo Satellaview"] = "satellaview";
  platformNameToFinalCode["Nintendo Switch"] = "switch";
  platformNameToFinalCode["Nintendo Virtual Boy"] = "virtualboy";
  platformNameToFinalCode["Nintendo Wii"] = "wii";
  platformNameToFinalCode["Nintendo Wii U"] = "wiiu";
//  platformNameToFinalCode["Nokia N-Gage"] = "ngage";
  platformNameToFinalCode["OpenBOR"] = "openbor";
  platformNameToFinalCode["Oric Atmos"] = "oric";
  platformNameToFinalCode["PC Engine SuperGrafx"] = "supergrafx";
  platformNameToFinalCode["Philips CD-i"] = "cdi";
  platformNameToFinalCode["Philips Videopac+"] = "videopac";
  platformNameToFinalCode["SAM Coupé"] = "samcoupe";
  platformNameToFinalCode["Sammy Atomiswave"] = "atomiswave";
  platformNameToFinalCode["ScummVM"] = "scummvm";
  platformNameToFinalCode["Sega 32X"] = "sega32x";
  platformNameToFinalCode["Sega CD"] = "megacd";
  platformNameToFinalCode["Sega CD 32X"] = "megacd";
  platformNameToFinalCode["Sega Dreamcast"] = "dreamcast";
  platformNameToFinalCode["Sega Game Gear"] = "gamegear";
  platformNameToFinalCode["Sega Genesis"] = "genesis";
  platformNameToFinalCode["Sega Master System"] = "mastersystem";
  platformNameToFinalCode["Sega Model 1"] = "segasystemx";
  platformNameToFinalCode["Sega Model 2"] = "segasystemx";
  platformNameToFinalCode["Sega Model 3"] = "segasystemx";
  platformNameToFinalCode["Sega Naomi"] = "naomi";
  platformNameToFinalCode["Sega Naomi 2"] = "naomi2";
  platformNameToFinalCode["Sega Saturn"] = "saturn";
  platformNameToFinalCode["Sega SG-1000"] = "sg-1000";
  platformNameToFinalCode["Sega System 16"] = "segasystemx";
  platformNameToFinalCode["Sega System 32"] = "segasystemx";
  platformNameToFinalCode["Sharp X1"] = "x1";
  platformNameToFinalCode["Sharp X68000"] = "x68000";
  platformNameToFinalCode["Sinclair ZX Spectrum"] = "zxspectrum";
  platformNameToFinalCode["Sinclair ZX-81"] = "zx81";
  platformNameToFinalCode["SNK Neo Geo AES"] = "neogeo";
  platformNameToFinalCode["SNK Neo Geo CD"] = "neogeocd";
  platformNameToFinalCode["SNK Neo Geo MVS"] = "neogeo";
  platformNameToFinalCode["SNK Neo Geo Pocket"] = "ngp";
  platformNameToFinalCode["SNK Neo Geo Pocket Color"] = "ngpc";
  platformNameToFinalCode["Sony Playstation"] = "psx";
  platformNameToFinalCode["Sony Playstation 2"] = "ps2";
  platformNameToFinalCode["Sony Playstation 3"] = "ps3";
//  platformNameToFinalCode["Sony Playstation 4"] = "ps4";
//  platformNameToFinalCode["Sony Playstation 5"] = "ps5";
  platformNameToFinalCode["Sony Playstation Vita"] = "vita";
  platformNameToFinalCode["Sony PSP"] = "psp";
  platformNameToFinalCode["Sony PSP Minis"] = "psp";
  platformNameToFinalCode["Super Nintendo Entertainment System"] = "snes";
  platformNameToFinalCode["Taito Type X"] = "taito";
  platformNameToFinalCode["Tandy TRS-80"] = "trs80";
  platformNameToFinalCode["Texas Instruments TI 99/4A"] = "ti99";
  platformNameToFinalCode["VTech CreatiVision"] = "creativision";
  platformNameToFinalCode["Watara Supervision"] = "watara";
  platformNameToFinalCode["Windows"] = "windows";
  platformNameToFinalCode["Windows 3.X"] = "pc";
  platformNameToFinalCode["WonderSwan"] = "wonderswan";
  platformNameToFinalCode["WonderSwan Color"] = "wonderswancolor";
  platformNameToFinalCode["WoW Action Max"] = "actionmax";
  platformNameToFinalCode["ZiNc"] = "zmachine";

  schema["LaunchBox"] = LBLAUNCHBOX;
  schema["Game"] = LBGAME;
  schema["ReleaseYear"] = LBRELEASEYEAR;
  schema["Name"] = LBNAME;
  schema["ReleaseDate"] = LBRELEASEDATE;
  schema["Overview"] = LBOVERVIEW;
  schema["MaxPlayers"] = LBMAXPLAYERS;
  schema["VideoURL"] = LBVIDEOURL;
  schema["DatabaseID"] = LBDATABASEID;
  schema["CommunityRating"] = LBCOMMUNITYRATING;
  schema["Platform"] = LBPLATFORM;
  schema["ESRB"] = LBESRB;
  schema["Genres"] = LBGENRES;
  schema["Developer"] = LBDEVELOPER;
  schema["Publisher"] = LBPUBLISHER;
  schema["GameAlternateName"] = LBGAMEALTERNATENAME;
  schema["AlternateName"] = LBALTERNATENAME;
  schema["Region"] = LBREGION;
  schema["GameImage"] = LBGAMEIMAGE;
  schema["Type"] = LBTYPE;
  schema["FileName"] = LBFILENAME;
  schema["MameFile"] = LBMAMEFILE;

  // Reverse Priority (from less prioritary to more prioritary)!

  priorityRegions = QStringList ({
      "Russia", "Brazil", "China", "Hong Kong", "Asia",
      "Korea", "Greece", "Finland", "Norway", "Sweden",
      "The Netherlands", "Italy", "Germany", "France",
      "South America", "Canada", "Spain", "United Kingdom",
      "Australia", "Oceania", "United States", "North America",
      "Europe", "Japan", "World" });
    
  priorityImages[COVER] = QStringList (
      { "Advertisement Flyer - Front", "Fanart - Box - Front", "Box - Front - Reconstructed", "Box - Front" });
  priorityImages[SCREENSHOT] = QStringList (
      { "Screenshot - Game Select", "Screenshot - Gameplay" });
  priorityImages[WHEEL] = QStringList (
      { "Screenshot - High Scores", "Screenshot - Game Title" });
  priorityImages[MARQUEE] = QStringList (
      { "Fanart - Background Image" });
  priorityImages[TEXTURE] = QStringList (
      { "Fanart - Cart - Front", "Fanart - Disc", "Cart - Front", "Disc", "Fanart - Box - Back", "Box - Back - Reconstructed", "Box - Back"});

}
