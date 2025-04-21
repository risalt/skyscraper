/***************************************************************************
 *            launchbox.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
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
#include <QString>
#include <QJsonArray>
#include <QMapIterator>
#include <QXmlStreamReader>
#include <QStringListIterator>
#include <QSql>
#include <QSqlError>
#include <QSqlQuery>

#include "launchbox.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"
#include "skyscraper.h"

constexpr int RETRIESMAX = 4;

LaunchBox::LaunchBox(Settings *config,
                     QSharedPointer<NetManager> manager,
                     QString threadId,
                     NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  baseUrl = "https://images.launchbox-app.com/";
  QString databaseUrl = "https://gamesdb.launchbox-app.com/Metadata.zip";

  loadMaps();
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "launchbox" + threadId);

  if(config->generateLbDb) {
  
    bool skipGame = false;
    bool loadedOk = false;
    int syntaxError = 0;
    int currentEntity = LBNULL;
    int currentTag = LBNULL;
    QString currentFileName = "";
    int currentId = LBNULL;
    QString currentText = "";
    QString currentType = "";
    QString currentRegion = "";
    GameEntry currentGame;

    QXmlStreamReader reader;
    QFile xmlFile (config->dbPath);
    if(xmlFile.open(QIODevice::ReadOnly)) {
      reader.setDevice(&xmlFile);
      if(reader.readNext() && reader.isStartDocument() &&
         reader.readNextStartElement() &&
         schema.value(reader.name().toString()) == LBLAUNCHBOX) {

        printf("INFO: Preparing database... "); fflush(stdout);
        QFile::remove(config->dbPath + ".db");
        if(QFile::exists(config->dbPath + ".db")) {
          printf("ERROR: Old database could not be removed. Exiting.\n");
          reqRemaining = 0;
          return;
        }
        db.setDatabaseName(config->dbPath + ".db");
        if(!db.open()) {
          printf("ERROR: Could not create the database %s%s.\n",
                 config->dbPath.toStdString().c_str(), ".db");
          qDebug() << db.lastError();
          reqRemaining = 0;
          return;
        }

        while(!reader.atEnd() && !reader.hasError() &&
              (reader.readNextStartElement())) {
          //printf(". %s\n", reader.name().toString().toStdString().c_str());
          currentTag = schema.value(reader.name().toString());
          switch (currentEntity) {
            case LBGAMEIMAGE:
              if(currentId && !currentType.isEmpty() && !currentFileName.isEmpty()) {
                // Save or update currentGame in launchBoxDb
                QMapIterator<int, QStringList> prios(priorityImages);
                while(prios.hasNext()) {
                  prios.next();
                  if(prios.value().contains(currentType)) {
                    GameEntry * existingGame = nullptr;
                    if(!launchBoxDb.contains(currentId)) {
                      launchBoxDb[currentId] = currentGame;
                    }
                    existingGame = &launchBoxDb[currentId];
                    bool keepExisting = false;
                    switch (prios.key()) {
                      case COVER:
                        if(!existingGame->coverFile.isEmpty()) {
                          if(existingGame->coverSrc == currentType) {
                            if(currentRegion.isEmpty() ||
                                (priorityRegions.indexOf(existingGame->coverRegion) >=
                                priorityRegions.indexOf(currentRegion))) {
                              keepExisting = true;
                            }
                          }
                          else if(prios.value().indexOf(existingGame->coverSrc) >
                              prios.value().indexOf(currentType)) {
                            keepExisting = true;
                          }
                        }
                        if(!keepExisting) {
                          existingGame->coverFile = currentFileName;
                          existingGame->coverSrc = currentType;
                          existingGame->coverRegion = currentRegion;
                        }
                      break;
                      case SCREENSHOT:
                        if(!existingGame->screenshotFile.isEmpty()) {
                          if(existingGame->screenshotSrc == currentType) {
                            if(currentRegion.isEmpty() ||
                                (priorityRegions.indexOf(existingGame->screenshotRegion) >=
                                priorityRegions.indexOf(currentRegion))) {
                              keepExisting = true;
                            }
                          }
                          else if(prios.value().indexOf(existingGame->screenshotSrc) >
                              prios.value().indexOf(currentType)) {
                            keepExisting = true;
                          }
                        }
                        if(!keepExisting) {
                          existingGame->screenshotFile = currentFileName;
                          existingGame->screenshotSrc = currentType;
                          existingGame->screenshotRegion = currentRegion;
                        }
                      break;
                      case WHEEL:
                        if(!existingGame->wheelFile.isEmpty()) {
                          if(existingGame->wheelSrc == currentType) {
                            if(currentRegion.isEmpty() ||
                                (priorityRegions.indexOf(existingGame->wheelRegion) >=
                                priorityRegions.indexOf(currentRegion))) {
                              keepExisting = true;
                            }
                          }
                          else if(prios.value().indexOf(existingGame->wheelSrc) >
                              prios.value().indexOf(currentType)) {
                            keepExisting = true;
                          }
                        }
                        if(!keepExisting) {
                          existingGame->wheelFile = currentFileName;
                          existingGame->wheelSrc = currentType;
                          existingGame->wheelRegion = currentRegion;
                        }
                      break;
                      case MARQUEE:
                        if(!existingGame->marqueeFile.isEmpty()) {
                          if(existingGame->marqueeSrc == currentType) {
                            if(currentRegion.isEmpty() ||
                                (priorityRegions.indexOf(existingGame->marqueeRegion) >=
                                priorityRegions.indexOf(currentRegion))) {
                              keepExisting = true;
                            }
                          }
                          else if(prios.value().indexOf(existingGame->marqueeSrc) >
                              prios.value().indexOf(currentType)) {
                            keepExisting = true;
                          }
                        }
                        if(!keepExisting) {
                          existingGame->marqueeFile = currentFileName;
                          existingGame->marqueeSrc = currentType;
                          existingGame->marqueeRegion = currentRegion;
                        }
                      break;
                      case TEXTURE:
                        if(!existingGame->textureFile.isEmpty()) {
                          if(existingGame->textureSrc == currentType) {
                            if(currentRegion.isEmpty() ||
                                (priorityRegions.indexOf(existingGame->textureRegion) >=
                                priorityRegions.indexOf(currentRegion))) {
                              keepExisting = true;
                            }
                          }
                          else if(prios.value().indexOf(existingGame->textureSrc) >
                              prios.value().indexOf(currentType)) {
                            keepExisting = true;
                          }
                        }
                        if(!keepExisting) {
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
              if(currentId) {
                GameEntry * existingGame = nullptr;
                if(!skipGame) {
                  if(launchBoxDb.contains(currentId)) {
                    existingGame = &launchBoxDb[currentId];
                    if(!launchBoxDb[currentId].platform.isEmpty()) {
                      printf("ERROR: Detected duplicated Game Id: %d (%s and %s)... \n",
                             currentId, launchBoxDb.value(currentId).title.toStdString().c_str(),
                             currentGame.title.toStdString().c_str());
                      if(launchBoxDb.value(currentId).getCompleteness() >= currentGame.getCompleteness()) {
                        skipGame = true;
                      }
                    }
                  }
                  if(!skipGame) {
                    if(existingGame) {
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
              if(currentId && !currentGame.title.isEmpty()) {
                if(!skipGame) {
                  QStringList safeVariations, unsafeVariations;
                  NameTools::generateSearchNames(currentGame.title, safeVariations, unsafeVariations, true);
                  for(const auto &name: std::as_const(safeVariations)) {
                    const auto idTitle = qMakePair(currentId, currentGame.title);
                    if(!searchNameToId.contains(name, idTitle)) {
                      if(config->verbosity > 3) {
                        printf("Adding full variation: %s -> %s\n",
                               currentGame.title.toStdString().c_str(), name.toStdString().c_str());
                      }
                      searchNameToId.insert(name, idTitle);
                    }
                  }
                  for(const auto &name: std::as_const(unsafeVariations)) {
                    const auto idTitle = qMakePair(currentId, currentGame.title);
                    if(!searchNameToIdTitle.contains(name, idTitle)) {
                      if(config->verbosity > 3) {
                        printf("Adding title variation: %s -> %s\n",
                               currentGame.title.toStdString().c_str(), name.toStdString().c_str());
                      }
                      searchNameToIdTitle.insert(name, idTitle);
                    }
                  }
                }
              }
              else if(currentEntity == LBGAMEALTERNATENAME) {
                syntaxError++; printf("B"); fflush(stdout);
              }
#pragma GCC diagnostic pop
            break;
            case LBNULL:
            break;
            default:
              syntaxError++; printf("C"); fflush(stdout);
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
              while(!reader.atEnd() && !reader.hasError() &&
                    (reader.readNextStartElement())) {
                //printf(".. %s: ", reader.name().toString().toStdString().c_str());
                currentTag = schema.value(reader.name().toString());
                switch (currentTag) {
                  case LBRELEASEYEAR:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("D"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      if(currentText.length() == 4 && currentGame.releaseDate.isEmpty()) {
                        currentGame.releaseDate = StrTools::conformReleaseDate(currentText);
                      }
                    }
                  break;
                  case LBNAME:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("E"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.title = currentText;
                    }
                    else {
                      skipGame = true;
                    }
                  break;
                  case LBRELEASEDATE:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("F"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.releaseDate = StrTools::conformReleaseDate(currentText.left(10));
                    }
                  break;
                  case LBOVERVIEW:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("G"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.description = currentText;
                    }
                  break;
                  case LBMAXPLAYERS:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("H"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.players = StrTools::conformPlayers(currentText);
                    }
                  break;
                  case LBVIDEOURL:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("I"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.videoFile = currentText;
                    }
                  break;
                  case LBDATABASEID:
                    currentText = reader.readElementText();
                    if(currentEntity) {
                      currentId = currentText.toInt();
                    }
                    if(!currentEntity || !currentId) {
                      skipGame = true;
                      syntaxError++; printf("J"); fflush(stdout);
                    }
                  break;
                  case LBCOMMUNITYRATING:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("K"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      float rating = currentText.toFloat();
                      if(rating > 0.0) {
                        currentGame.rating = QString::number(rating/5.0, 'f', 3);
                      }
                    }
                  break;
                  case LBPLATFORM:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("L"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.platform = currentText;
                    }
                    else {
                      skipGame = true;
                    }
                  break;
                  case LBESRB:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("M"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      if(!currentText.contains("Not Rated", Qt::CaseInsensitive) ||
                          !currentText.contains("Rating Pending", Qt::CaseInsensitive)) {
                        currentGame.ages = StrTools::conformAges(currentText);
                      }
                    }
                  break;
                  case LBGENRES:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("N"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.tags = StrTools::conformTags(currentText.replace(';', ','));
                    }
                  break;
                  case LBDEVELOPER:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("O"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.developer = currentText;
                    }
                  break;
                  case LBPUBLISHER:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAME) {
                      syntaxError++; printf("P"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.publisher = currentText;
                    }
                  break;
                  case LBALTERNATENAME:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAMEALTERNATENAME) {
                      syntaxError++; printf("Q"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentGame.title = currentText;
                    }
                  break;
                  case LBREGION:
                    currentText = reader.readElementText();
                    if(!currentEntity) {
                      syntaxError++; printf("R"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty() && (currentEntity == LBGAMEIMAGE)) {
                      currentRegion = currentText;
                    }
                  break;
                  case LBFILENAME:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAMEIMAGE) {
                      syntaxError++; printf("S"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
                      currentFileName = currentText;
                    }
                  break;
                  case LBTYPE:
                    currentText = reader.readElementText();
                    if(currentEntity != LBGAMEIMAGE) {
                      syntaxError++; printf("T"); fflush(stdout);
                    }
                    else if(!currentText.isEmpty()) {
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
             config->dbPath.toStdString().c_str());
      printf("INFO: Please download it from '%s'\n", databaseUrl.toStdString().c_str());
      reqRemaining = 0;
      return;
    }
    reader.clear();
    xmlFile.close();

    if(syntaxError) {
      printf("WARNING: The database contains %d syntax error(s). The associated records have been ignored.\n", syntaxError);
    }
    if(!loadedOk) {
      printf("WARNING: The parser has reported errors.\n");
    }

    // Delete all entries in launchBoxDb with empty title and platform.
    for(auto item = launchBoxDb.begin(); item != launchBoxDb.end();) {
      if(item.value().title.isEmpty() || item.value().platform.isEmpty()) {
        item = launchBoxDb.erase(item);
      }
      else {
        ++item;
      }
    }
    // Delete all entries in searchNameToId with an id that does not exist in launchBoxDb.
    for(auto item = searchNameToId.begin(); item != searchNameToId.end();) {
      if(!launchBoxDb.contains(item.value().first)) {
        item = searchNameToId.erase(item);
      }
      else {
        ++item;
      }
    }
    for(auto item = searchNameToIdTitle.begin(); item != searchNameToIdTitle.end();) {
      if(!launchBoxDb.contains(item.value().first)) {
        item = searchNameToIdTitle.erase(item);
      }
      else {
        ++item;
      }
    }

    printf("INFO: LaunchBox database has been parsed, %d games have been loaded into memory.\n",
           launchBoxDb.count());

    printf("INFO: Creating SQLite database. Exporting the XML file to the database...\n");
    bool errorDb = true;
    bool useTransaction = true;
    QSqlQuery query(db);
    // Database structure creation
    query.prepare("CREATE TABLE searchNameToId (name TEXT, platform TEXT, id INTEGER, title TEXT)");
    if(query.exec()) {
      query.finish();
      query.prepare("CREATE TABLE searchNameToIdTitle (name TEXT, platform TEXT, id INTEGER, title TEXT)");
      if(query.exec()) {
        query.finish();
        query.prepare("CREATE TABLE launchBoxDb (id INTEGER, platform TEXT, game BLOB)");
        if(query.exec()) {
          query.finish();
          errorDb = false;
        }
      }
    }
    if(errorDb) {
      qDebug() << query.lastError();
      printf("ERROR: Error while creating the database structure.\n");
    }
    if(!errorDb && !db.transaction()) {
      printf("WARNING: Reduced performance as database does not support transactions.\n");
      qDebug() << db.lastError();
      useTransaction = false;
    }

    // searchNameToId
    if(!errorDb) {
      query.prepare("INSERT INTO searchNameToId VALUES (?, ?, ?, ?)");
      QVariantList names, platforms, ids, titles;
      const auto keys = searchNameToId.uniqueKeys();
      for(const auto &key: std::as_const(keys)) {
        const auto values = searchNameToId.values(key);
        for(const auto &id: std::as_const(values)) {
          names << key;
          platforms << launchBoxDb[id.first].platform;
          ids << id.first;
          titles << id.second;
        }
      }
      query.addBindValue(names);
      query.addBindValue(platforms);
      query.addBindValue(ids);
      query.addBindValue(titles);
      if(!query.execBatch()) {
        qDebug() << query.lastError();
        printf("ERROR: Error while adding data into table searchNameToId.\n");
        errorDb = true;
      }
    }
    // searchNameToIdTitle
    if(!errorDb) {
      query.prepare("INSERT INTO searchNameToIdTitle VALUES (?, ?, ?, ?)");
      QVariantList names, platforms, ids, titles;
      const auto keys = searchNameToIdTitle.uniqueKeys();
      for(const auto &key: std::as_const(keys)) {
        const auto values = searchNameToIdTitle.values(key);
        for(const auto &id: std::as_const(values)) {
          names << key;
          platforms << launchBoxDb[id.first].platform;
          ids << id.first;
          titles << id.second;
        }
      }
      query.addBindValue(names);
      query.addBindValue(platforms);
      query.addBindValue(ids);
      query.addBindValue(titles);
      if(!query.execBatch()) {
        qDebug() << query.lastError();
        printf("ERROR: Error while adding data into table searchNameToIdTitle.\n");
        errorDb = true;
      }
    }
    // launchBoxDb
    if(!errorDb) {
      query.prepare("INSERT INTO launchBoxDb VALUES (?, ?, ?)");
      QVariantList ids, platforms, games;
      const auto keys = launchBoxDb.keys();
      for(const auto &id: std::as_const(keys)) {
        ids << id;
        platforms << launchBoxDb[id].platform;
        games << launchBoxDb[id].serialize();
      }
      query.addBindValue(ids);
      query.addBindValue(platforms);
      query.addBindValue(games);
      if(!query.execBatch()) {
        qDebug() << query.lastError();
        printf("ERROR: Error while adding data into table searchNameToIdTitle.\n");
        errorDb = true;
      }
    }

    if(errorDb || (useTransaction && !db.commit())) {
      qDebug() << db.lastError();
      db.rollback();
      db.close();
      QFile::remove(config->dbPath + ".db");
      printf("ERROR: Error while creating the database. Removing the "
             "in-progress file to avoid corruption while scraping.\n");
    } else {
      db.close();
      printf("INFO: Export completed successfully. Now exiting.\n");
    }
    reqRemaining = 0;
    return;

  } else {
  
    QString platform = getPlatformId(config->platform);
    if(Platform::get().getFamily(config->platform) == "arcade" &&
       platform == "na") {
      platform = getPlatformId("arcade");
    }
    if(platform == "na") {
      reqRemaining = 0;
      printf("\033[0;31mPlatform not supported by Launchbox (see file launchbox.json)...\033[0m\n");
      return;
    }
  
    db.setDatabaseName(config->dbPath + ".db");
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if(!db.open()) {
      printf("ERROR: Could not open the database %s%s.\n",
             config->dbPath.toStdString().c_str(), ".db");
      printf("Please regenerate it by executing 'Skyscraper --generatelbdb -s launchbox'.\n");
      qDebug() << db.lastError();
      reqRemaining = 0;
      return;
    }
    QSqlQuery query(db);
    // launchBoxDb
    query.setForwardOnly(true);
    query.prepare("SELECT id, game FROM launchBoxDb WHERE platform='" + platform + "'");
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing data from table launchBoxDb.\n");
      reqRemaining = 0;
      return;
    }
    while(query.next()) {
      launchBoxDb[query.value(0).toInt()] = GameEntry(query.value(1).toByteArray());
    }
    query.finish();
    // searchNameToId
    query.setForwardOnly(true);
    query.prepare("SELECT name, id, title FROM searchNameToId WHERE platform='" +
                  platform + "'");
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing data from table searchNameToId.\n");
      reqRemaining = 0;
      return;
    }
    while(query.next()) {
      searchNameToId.insert(query.value(0).toString(),
                            qMakePair(query.value(1).toInt(), query.value(2).toString()));
    }
    query.finish();
    // searchNameToIdTitle
    query.setForwardOnly(true);
    query.prepare("SELECT name, id, title FROM searchNameToIdTitle WHERE platform='" +
                  platform + "'");
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing data from table searchNameToIdTitle.\n");
      reqRemaining = 0;
      return;
    }
    while(query.next()) {
      searchNameToIdTitle.insert(query.value(0).toString(),
                                 qMakePair(query.value(1).toInt(), query.value(2).toString()));
    }
    query.finish();
    db.close();

    printf("INFO: LaunchBox database has been parsed, %d games have been loaded into memory.\n",
           launchBoxDb.count());
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(100);
  limitTimer.setSingleShot(false);
  limitTimer.start();

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
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

  // Print list of game names (including alternates):
  if(config->verbosity >= 3) {
      const auto keys = searchNameToId.uniqueKeys();
      for(const auto &key: std::as_const(keys)) {
        const auto values = searchNameToId.values(key);
        for(const auto &id: std::as_const(values)) {
        qDebug() << key << id.first << id.second;
      }
    }
  }
}

LaunchBox::~LaunchBox()
{
  QSqlDatabase::removeDatabase("launchbox" + threadId);
}

void LaunchBox::getSearchResults(QList<GameEntry> &gameEntries,
                                 QString searchName, QString)
{
  QList<QPair<int, QString>> matches;

  if(getSearchResultsOffline(matches, searchName, searchNameToId, searchNameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      if(launchBoxDb.contains(databaseId.first)) {
        GameEntry game;
        game.id = QString::number(databaseId.first);
        game.title = databaseId.second;
        game.platform = launchBoxDb[databaseId.first].platform;
        gameEntries.append(game);
      }
      else {
        printf("ERROR: Internal error: Database Id '%d' not found in the in-memory database.\n",
               databaseId.first);
      }
    }
  }
}

void LaunchBox::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  if(!launchBoxDb.contains(game.id.toInt())) {
    printf("ERROR: Internal error: Database Id '%s' not found in the in-memory database.\n",
           game.id.toStdString().c_str());
    return;
  }

  fetchGameResources(game, sharedBlobs, cache);
}

void LaunchBox::getReleaseDate(GameEntry &game)
{
  game.releaseDate = StrTools::conformReleaseDate(launchBoxDb.value(game.id.toInt()).releaseDate);
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
  game.players = StrTools::conformPlayers(launchBoxDb.value(game.id.toInt()).players);
}

void LaunchBox::getAges(GameEntry &game)
{
  game.ages = StrTools::conformAges(launchBoxDb.value(game.id.toInt()).ages);
}

void LaunchBox::getRating(GameEntry &game)
{
  game.rating = launchBoxDb.value(game.id.toInt()).rating;
}

void LaunchBox::getTags(GameEntry &game)
{
  game.tags = StrTools::conformTags(launchBoxDb.value(game.id.toInt()).tags);
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
  loadConfig("launchbox.json", "code", "query");

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
      { "Fanart - Background Image", "Fanart - Cart - Front", "Fanart - Disc", "Cart - Front", "Disc"});
  priorityImages[TEXTURE] = QStringList (
      { "Fanart - Box - Back", "Box - Back - Reconstructed", "Box - Back"});

}
