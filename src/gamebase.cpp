/***************************************************************************
 *            gamebase.cpp
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

#include <QSql>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

#include "gamebase.h"
#include "platform.h"
#include "strtools.h"

GameBase::GameBase(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId,
                   NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  baseUrl = config->gamebasePath + "/" + config->platform + "/";

  printf("INFO: Loading GameBase database... "); fflush(stdout);
  QString dbPath = baseUrl + config->platform + ".sqlite";
  db = QSqlDatabase::addDatabase("QSQLITE", "gamebase" + threadId);
  db.setDatabaseName(dbPath);
  db.setConnectOptions("QSQLITE_OPEN_READONLY");
  if(!db.open()) {
    printf("ERROR: Could not open the database %s.\n",
           dbPath.toStdString().c_str());
    printf("Please download and regenerate it as needed.\n");
    qDebug() << db.lastError();
    reqRemaining = 0;
    return;
  }

  QSqlQuery query(db);

  // Get platform name
  query.setForwardOnly(true);
  query.prepare("SELECT DatabaseName FROM Config LIMIT 1");
  if(!query.exec()) {
    qDebug() << query.lastError();
  } else {
    while(query.next()) {
      platformName = query.value(0).toString();
    }
  }
  query.finish();
  if(platformName.isEmpty()) {
    platformName = Platform::get().getAliases(config->platform).at(1);
  }

  // Games to searchNametoId/Title
  query.setForwardOnly(true);
  query.prepare("SELECT GA_Id, name FROM Games");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table Games.\n");
    reqRemaining = 0;
    return;
  }
  int gameCount = 0;
  while(query.next()) {
    int gameId = query.value(0).toInt();
    QString gameName = query.value(1).toString();

    if(gameId && !gameName.isEmpty()) {
      gameCount++;
      QStringList safeVariations, unsafeVariations;
      NameTools::generateSearchNames(gameName, safeVariations, unsafeVariations, true);
      for(const auto &name: std::as_const(safeVariations)) {
        const auto idTitle = qMakePair(gameId, gameName);
        if(!searchNameToId.contains(name, idTitle)) {
          if(config->verbosity > 3) {
            printf("Adding full variation: %s -> %s\n",
                   gameName.toStdString().c_str(), name.toStdString().c_str());
          }
          searchNameToId.insert(name, idTitle);
        }
      }
      for(const auto &name: std::as_const(unsafeVariations)) {
        const auto idTitle = qMakePair(gameId, gameName);
        if(!searchNameToIdTitle.contains(name, idTitle)) {
          if(config->verbosity > 3) {
            printf("Adding title variation: %s -> %s\n",
                   gameName.toStdString().c_str(), name.toStdString().c_str());
          }
          searchNameToIdTitle.insert(name, idTitle);
        }
      }
    }
  }
  query.finish();

  printf("Database has been parsed, %d games have been loaded into memory.\n",
         gameCount);

  printf("INFO: Opening the videogame music database... "); fflush(stdout);
  navidrome = QSqlDatabase::addDatabase("QSQLITE", "navidrome" + threadId);
  navidrome.setDatabaseName(config->dbServer);
  navidrome.setConnectOptions("QSQLITE_OPEN_READONLY");
  if(!navidrome.open()) {
    printf("ERROR: Connection to Navidrome database has failed.\n");
    qDebug() << navidrome.lastError();
    reqRemaining = 0;
    return;
  } else {
    printf("DONE.\n");
  }

  printf("INFO: Loading assets file to correct file case sensitivity errors... ");
  fflush(stdout);
  QString assetsFileName = config->gamebasePath + "/assets.txt";
  QFile assetsFile(assetsFileName);
  if(assetsFile.exists() && assetsFile.open(QIODevice::ReadOnly | QFile::Text)) {
    {
      QTextStream assetsList(&assetsFile);
      while(!assetsList.atEnd()) {
        QString asset = assetsList.readLine();
        if(asset.startsWith(baseUrl)) {
          caseInsensitivePaths.insert(asset.toLower(), asset);
        }
      }
      printf("DONE. %d filenames have been loaded into memory.\n",
             caseInsensitivePaths.size());
    }
    assetsFile.close();
  } else {
    printf("ERROR: Could not load the assets file. Please generate "
           "the file running the command '%s/generate_links.sh'.\n",
            config->gamebasePath.toStdString().c_str());
    reqRemaining = 0;
    return;
  }

  loadMaps();

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
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(MANUAL);
  fetchOrder.append(GUIDES);
  fetchOrder.append(CHEATS);
  fetchOrder.append(VGMAPS);
  fetchOrder.append(ARTBOOKS);
  fetchOrder.append(CHIPTUNE);
}

GameBase::~GameBase(){
  if(db.isOpen()) {
    db.close();
    QString oldDatabase = db.connectionName();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(oldDatabase);
  }
  if(navidrome.isOpen()) {
    navidrome.close();
    QString oldDatabase = navidrome.connectionName();
    navidrome = QSqlDatabase();
    QSqlDatabase::removeDatabase(oldDatabase);
  }
}

void GameBase::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  // TODO: Medium: Implement CRC search using canonical data
  QList<QPair<int, QString>> matches;

  if(getSearchResultsOffline(matches, searchName, searchNameToId, searchNameToIdTitle)) {
    for(const auto &databaseId: std::as_const(matches)) {
      GameEntry game;
      game.id = QString::number(databaseId.first);
      game.title = databaseId.second;
      game.platform = platformName;
      gameEntries.append(game);
    }
  }
}

void GameBase::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache)
{
  gameRecord.clear();
  gameExtras.clear();
  QSqlQuery query(db);

  // Get game details
  query.setForwardOnly(true);
  query.prepare("SELECT * FROM Games WHERE GA_Id = :id");
  query.bindValue(":id", game.id.toInt());
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing id %d from table Games.\n",
           game.id.toInt());
    return;
  }
  while(query.next()) {
    gameRecord = query.record();
  }
  query.finish();

  // Get game extras
  query.setForwardOnly(true);
  query.prepare("SELECT Name, Path FROM Extras "
                "WHERE GA_Id = :id ORDER BY DisplayOrder ASC");
  query.bindValue(":id", game.id.toInt());
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing game id %d from table Extras.\n",
           game.id.toInt());
  } else {
    while(query.next()) {
      gameExtras.append(qMakePair(query.value(0).toString(),
                                  query.value(1).toString().replace("\\", "/")));
    }
  }
  query.finish();

  fetchGameResources(game, sharedBlobs, cache);
}

void GameBase::getReleaseDate(GameEntry &game)
{
  if(gameRecord.contains("YE_Id")) {
    int id = gameRecord.value("YE_Id").toInt();
    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare("SELECT Year FROM Years WHERE YE_Id = :id");
    query.bindValue(":id", id);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing id %d from table Years.\n", id);
    } else {
      while(query.next()) {
        game.releaseDate = StrTools::conformReleaseDate(
                             QString::number(query.value(0).toInt()));
      }
    }
    query.finish();
  }
}

void GameBase::getDescription(GameEntry &game)
{
  if(gameRecord.contains("MemoText")) {
    QString text = gameRecord.value("MemoText").toString();
    if(text.size() > 80) {
      game.description = text;
    }
  }
}

void GameBase::getTags(GameEntry &game)
{
  int parentGenre = 0;
  if(gameRecord.contains("GE_Id")) {
    int id = gameRecord.value("GE_Id").toInt();
    if(id > 1) {
      QSqlQuery query(db);
      query.setForwardOnly(true);
      query.prepare("SELECT PG_Id, Genre FROM Genres WHERE GE_Id = :id");
      query.bindValue(":id", id);
      if(!query.exec()) {
        qDebug() << query.lastError();
        printf("ERROR: Error while accessing id %d from table Genres.\n", id);
      } else {
        while(query.next()) {
          parentGenre = query.value(0).toInt();
          game.tags = query.value(1).toString();
          if(game.tags.contains("[") || game.tags.contains("ncategorized")) {
            game.tags.clear();
          }
        }
      }
      query.finish();
    }
  }

  if(parentGenre > 1) {
    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare("SELECT ParentGenre FROM PGenres WHERE PG_Id = :id");
    query.bindValue(":id", parentGenre);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing id %d from table PGenres.\n", parentGenre);
    } else {
      while(query.next()) {
        QString genre = query.value(0).toString();
        if(!genre.contains("[") && !genre.contains("ncategorized")) {
          if(game.tags.isEmpty()) {
            game.tags = genre;
          } else {
            game.tags = genre + "," + game.tags;
          }
        }
      }
    }
    query.finish();
  }

  game.tags = StrTools::conformTags(game.tags);
}

void GameBase::getPlayers(GameEntry &game)
{
  if(gameRecord.contains("PlayersFrom") && gameRecord.contains("PlayersTo")) {
    int from = gameRecord.value("PlayersFrom").toInt();
    int to = gameRecord.value("PlayersTo").toInt();
    if(from > 0 && to > 0) {
      if(from != to) {
        game.players = QString::number(from) + "-" + QString::number(to);
      } else {
        game.players = QString::number(from);
      }
      game.players = StrTools::conformPlayers(game.players);
    }
  }
}

void GameBase::getAges(GameEntry &game)
{
  if(gameRecord.contains("Adult")) {
    int adult = gameRecord.value("Adult").toInt();
    if(adult > 0) {
      game.ages = StrTools::conformAges("Mature");
    }
  }
}

void GameBase::getPublisher(GameEntry &game)
{
  if(gameRecord.contains("PU_Id")) {
    int id = gameRecord.value("PU_Id").toInt();
    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare("SELECT Publisher FROM Publishers WHERE PU_Id = :id");
    query.bindValue(":id", id);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing id %d from table Publishers.\n", id);
    } else {
      while(query.next()) {
        game.publisher = query.value(0).toString();
        if(game.publisher.contains("(") || game.publisher.contains("nknown")) {
          game.publisher.clear();
        }
      }
    }
    query.finish();
  }
}

void GameBase::getDeveloper(GameEntry &game)
{
  if(gameRecord.contains("DE_Id")) {
    int id = gameRecord.value("DE_Id").toInt();
    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare("SELECT Developer FROM Developers WHERE DE_Id = :id");
    query.bindValue(":id", id);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing id %d from table Developers.\n", id);
    } else {
      while(query.next()) {
        game.developer = query.value(0).toString();
        if(game.developer.contains("(") || game.developer.contains("nknown")) {
          game.developer.clear();
        }
      }
    }
    query.finish();
  }

  if(game.developer.isEmpty() && gameRecord.contains("PR_Id")) {
    int id = gameRecord.value("PR_Id").toInt();
    QSqlQuery query(db);
    query.setForwardOnly(true);
    query.prepare("SELECT Programmer FROM Programmers WHERE PR_Id = :id");
    query.bindValue(":id", id);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Error while accessing id %d from table Programmers.\n", id);
    } else {
      while(query.next()) {
        game.developer = query.value(0).toString();
        if(game.developer.contains("(") || game.developer.contains("nknown")) {
          game.developer.clear();
        }
      }
    }
    query.finish();
  }
}

void GameBase::getRating(GameEntry &game)
{
  if(gameRecord.contains("Classic")) {
    int classic = gameRecord.value("Classic").toInt();
    if(classic > 0) {
      game.rating = "1.0";
      return;
    }
  }
  if(gameRecord.contains("Rating")) {
    int rating = gameRecord.value("Rating").toInt();
    if(rating > 0) {
      game.rating = QString::number(rating / 5.0f);
    }
  }
  if(gameRecord.contains("ReviewRating")) {
    int rating = gameRecord.value("ReviewRating").toInt();
    if(rating > 0) {
      game.rating = QString::number(rating / 5.0f);
    }
  }
}

void GameBase::getScreenshot(GameEntry &game)
{
  if(gameRecord.contains("ScrnshotFilename")) {
    QString fileName = gameRecord.value("ScrnshotFilename").toString();
    fileName.replace("\\", "/");
    game.screenshotFile = loadImageData("Screenshots", fileName);
  }
}

void GameBase::getCover(GameEntry &game)
{
  getFile(game.coverFile, "image", "cover");
}

void GameBase::getMarquee(GameEntry &game)
{
  getFile(game.marqueeFile, "image", "marquee");
}

void GameBase::getTexture(GameEntry &game)
{
  getFile(game.textureFile, "image", "texture");
}

void GameBase::getManual(GameEntry &game)
{
  game.manualFormat = getFile(game.manualFile, "file", "manual");
}

void GameBase::getGuides(GameEntry &game)
{
  QString dummy;
  game.guides = getFile(dummy, "link", "guides");
}

void GameBase::getCheats(GameEntry &game)
{
  QString dummy;
  game.cheats = getFile(dummy, "link", "cheats");
}

void GameBase::getVGMaps(GameEntry &game)
{
  QString dummy;
  game.vgmaps = getFile(dummy, "link", "vgmaps");
}

void GameBase::getArtbooks(GameEntry &game)
{
  QString dummy;
  game.artbooks = getFile(dummy, "link", "artbooks");
}

void GameBase::getChiptune(GameEntry &game)
{
  if(gameRecord.contains("SidFilename")) {
    QString track = gameRecord.value("SidFilename").toString();
    if(!track.isEmpty()) {
      track.replace("\\", "/");
      QString fullTrack = baseUrl + "Music/" + track;
      if(!QFileInfo(fullTrack).exists()) {
        fullTrack = caseFixedPath(fullTrack);
        if(fullTrack.isEmpty()) {
          qWarning() << "Track does not exist: "
                     << baseUrl + "Music/" + gameRecord.value("SidFilename").toString();
          return;
        } else {
          track = fullTrack;
          track.remove(0, config->gamebasePath.size() + 1);
        }
      }
      QSqlQuery query(navidrome);
      query.setForwardOnly(true);
      query.prepare("SELECT id FROM media_file WHERE path = :path");
      query.bindValue(":path", track);
      if(!query.exec()) {
        qDebug() << query.lastError();
      } else {
        while(query.next()) {
          game.chiptunePath = fullTrack;
          game.chiptuneId = query.value(0).toString();
        }
        if(game.chiptuneId.isEmpty()) {
          query.finish();
          query.prepare("SELECT id FROM playlist WHERE path = :path");
          query.bindValue(":path", fullTrack);
          if(!query.exec()) {
            qDebug() << query.lastError();
          } else {
            while(query.next()) {
              game.chiptunePath = fullTrack;
              game.chiptuneId = query.value(0).toString();
            }
          }
        }
      }
      query.finish();
    }
  }
}

// For type:image: Returns nothing, data is filled with the binary data of the image
//          file:  Returns file extension, data is filled with the binary data of the file
//          link:  Returns a blank space-separated list of urls, data is not used
QString GameBase::getFile(QString &fileName, const QString &type, const QString &dataName)
{
  QString returnValue;
  for(const auto &pattern: std::as_const(extrasMapping[dataName])) {
    for(const auto &extra: std::as_const(gameExtras)) {
      if(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption)
           .match(extra.first).hasMatch()) {
        if(type == "image") {
          fileName = loadImageData("Extras", extra.second);
        } else if(type == "file") {
          fileName = loadImageData("Extras", extra.second);
          returnValue = QFileInfo(extra.second).suffix().toLower();
        } else if(type == "link") {
          QString fullPath;
          if(extra.second.startsWith("http", Qt::CaseInsensitive)) {
            fullPath = extra.second;
          } else {
            fullPath = baseUrl + "Extras/" + extra.second;
            if(!QFileInfo(fullPath).exists()) {
              fullPath = caseFixedPath(fullPath);
              if(fullPath.isEmpty()) {
                qWarning() << "File does not exist: " << baseUrl + "Extras/" + extra.second;
              }
            }
          }
          if(!fullPath.isEmpty() && !returnValue.contains(fullPath)) {
            returnValue = returnValue + ";" + fullPath;
          }
        }
        if(!fileName.isEmpty()) {
          break;
        }
      }
    }
    if(!fileName.isEmpty()) {
      break;
    }
  }
  return returnValue;
}

QString GameBase::loadImageData(const QString &subFolder, const QString &dosFileName)
{
  QString fileName = dosFileName;
  if(!fileName.isEmpty()) {
    fileName = baseUrl + subFolder + "/" + fileName;
    if(!QFileInfo::exists(fileName)) {
      QString caseFixed = caseFixedPath(fileName);
      if(caseFixed.isEmpty() || !QFileInfo::exists(caseFixed)) {
        qWarning() << "File cannot be found: " << fileName;
        fileName = "";
      } else {
        fileName = caseFixed;
      }
    }
  }
  return fileName;
}

// GameBases are typically created in a Windows environment and are plagued with
// case sensitivity errors in the filenames. This solution is inefficient but
// necessary:
QString GameBase::caseFixedPath(const QString &path)
{
  return caseInsensitivePaths.value(path.toLower());
}

void GameBase::loadMaps()
{
  extrasMapping.clear();

  QFile patternsFile("categorypatterns.json");
  if(!patternsFile.open(QIODevice::ReadOnly)) {
    qDebug("ERROR: Could not open patterns file. External resources will be missed.\n");
    return;
  }

  QByteArray saveData = patternsFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty()) {
    qDebug("ERROR: Patterns file is empty or corrupted. External resources will be missed.\n");
    return;
  }

  QJsonArray categoryPatterns = json["categoryPatterns"].toArray();
  for(const auto categoryPattern: std::as_const(categoryPatterns)) {
    QString category = categoryPattern.toObject()["category"].toString();
    QJsonArray patterns = categoryPattern.toObject()["patterns"].toArray();
    for(const auto pattern: std::as_const(patterns)) {
      extrasMapping[category].append(pattern.toString());
    }
  }
}
