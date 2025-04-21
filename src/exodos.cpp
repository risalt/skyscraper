/***************************************************************************
 *            exodos.cpp
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
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QJsonArray>
#include <QMapIterator>
#include <QXmlStreamReader>
#include <QStringListIterator>
#include <QRegularExpression>
#include <QJsonObject>
#include <QJsonDocument>

#include "exodos.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"
#include "skyscraper.h"

ExoDos::ExoDos(Settings *config,
                     QSharedPointer<NetManager> manager,
                     QString threadId,
                     NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  offlineScraper = true;

  baseUrl = config->exodosPath + "/";

  if(config->platform != "pc") {
    reqRemaining = 0;
    printf("\033[0;31mPC is the only platform supported by eXoDOS...\033[0m\n");
    return;
  }

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
  QString assetsFileName = baseUrl + "assets.txt";
  QFile assetsFile(assetsFileName);
  if(assetsFile.exists() && assetsFile.open(QIODevice::ReadOnly | QFile::Text)) {
    {
      QTextStream assetsList(&assetsFile);
      while(!assetsList.atEnd()) {
        QString asset = assetsList.readLine();
        if(asset.startsWith(baseUrl) &&
           !asset.endsWith(".bsh") && !asset.endsWith(".conf") &&
           !asset.endsWith(".bat") && !asset.endsWith(".command")) {
          assetPaths.append(asset);
        }
      }
      printf("DONE. %d filenames have been loaded into memory.\n",
             assetPaths.size());
    }
    assetsFile.close();
  } else {
    printf("ERROR: Could not load the assets file. Please generate "
           "the file running the command 'find %s -type f > %s/assets.txt'.\n",
            config->gamebasePath.toStdString().c_str(),
            config->gamebasePath.toStdString().c_str());
    reqRemaining = 0;
    return;
  }

  loadMaps();

  bool skipGame = false;
  bool loadedOk = false;
  int syntaxError = 0;
  int currentEntity = EXNULL;
  int currentTag = EXNULL;
  QString currentFileName;
  QString currentId;
  QString currentText;
  QString currentType;
  QString currentRegion;
  GameEntry currentGame;

  printf("INFO: Loading eXoDOS database... "); fflush(stdout);
  QXmlStreamReader reader;
  QFile xmlFile (baseUrl + "xml/all/MS-DOS.xml");
  if(xmlFile.open(QIODevice::ReadOnly)) {
    reader.setDevice(&xmlFile);
    if(reader.readNext() && reader.isStartDocument() &&
       reader.readNextStartElement() &&
       schema.value(reader.name().toString()) == EXLAUNCHBOX) {

      while(!reader.atEnd() && !reader.hasError() &&
            (reader.readNextStartElement())) {
        //printf(". %s\n", reader.name().toString().toStdString().c_str());
        currentTag = schema.value(reader.name().toString());
        switch (currentEntity) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
          case EXGAME:
            if(!currentId.isEmpty()) {
              if(!skipGame) {
                if(exoDosDb.contains(currentId)) {
                  skipGame = true;
                  printf("ERROR: Detected duplicated Game Id: %s \n",
                         currentId.toStdString().c_str());
                }
                if(!skipGame) {
                  exoDosDb[currentId] = currentGame;
                  pathToId[currentGame.baseName] = currentId;
                }
              }
            }
            else {
              // Apparently there are entries without ID... Let's ignore them.
              syntaxError++;
              //printf("%lld|%s|%s\n", reader.lineNumber(),
              //        currentGame.title.toStdString().c_str(), currentFileName.toStdString().c_str());
            }
#pragma GCC diagnostic pop
          break;
          case EXNULL:
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
        currentId = EXNULL;
        currentEntity = EXNULL;
        skipGame = false;
        switch (currentTag) {
          case EXGAME:
            currentEntity = currentTag;
            while(!reader.atEnd() && !reader.hasError() &&
                  (reader.readNextStartElement())) {
              //printf(".. %s: ", reader.name().toString().toStdString().c_str());
              currentTag = schema.value(reader.name().toString());
              switch (currentTag) {
                case EXTITLE:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("E"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.title = currentText;
                  }
                  else {
                    skipGame = true;
                  }
                break;
                case EXRELEASEDATE:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("F"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.releaseDate = StrTools::conformReleaseDate(currentText.left(10));
                  }
                break;
                case EXNOTES:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("G"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.description = currentText;
                  }
                break;
                case EXMAXPLAYERS:
                  currentText = reader.readElementText();
                  if(!currentText.isEmpty() && currentText != "1") {
                    currentText.prepend("1-");
                  }
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("H"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.players = StrTools::conformPlayers(currentText);
                  }
                break;
                case EXID:
                  currentText = reader.readElementText();
                  if(currentEntity) {
                    currentId = currentText;
                    currentGame.id = currentText;
                  }
                  if(!currentEntity || currentId.isEmpty()) {
                    skipGame = true;
                    syntaxError++; printf("J"); fflush(stdout);
                  }
                break;
                case EXCOMMUNITYSTARRATING:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("K"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    float rating = currentText.toFloat();
                    if(rating > 0.0) {
                      currentGame.rating = QString::number(rating/5.0, 'f', 3);
                    }
                  }
                break;
                case EXPLATFORM:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("L"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.platform = currentText;
                  }
                  else {
                    skipGame = true;
                  }
                break;
                case EXRATING:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("M"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    if(!currentText.contains("Not Rated", Qt::CaseInsensitive) ||
                        !currentText.contains("Rating Pending", Qt::CaseInsensitive)) {
                      currentGame.ages = StrTools::conformAges(currentText);
                    }
                  }
                break;
                case EXGENRE:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("N"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentText.replace('/', ',').replace(';', ',');
                    currentGame.tags = StrTools::conformTags(currentText);
                  }
                break;
                case EXSERIES:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("N"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentText.replace('/', ',').replace(';', ',');
                    currentGame.franchises = StrTools::conformTags(currentText);
                  }
                break;
                case EXDEVELOPER:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("O"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.developer = currentText;
                  }
                break;
                case EXPUBLISHER:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("P"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.publisher = currentText;
                  }
                break;
                case EXROOTFOLDER:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("Q"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.path = currentText.replace("\\", "/");
                  }
                break;
                case EXAPPLICATIONPATH:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("R"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    QFileInfo baseName(currentText.replace("\\", "/"));
                    currentGame.baseName = baseName.completeBaseName();
                  }
                break;
                case EXMANUALPATH:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("S"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.manualFile = currentText.replace("\\", "/");
                  }
                break;
                case EXMUSICPATH:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("T"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.chiptunePath = currentText.replace("\\", "/");
                  }
                break;
                case EXVIDEOPATH:
                  currentText = reader.readElementText();
                  if(currentEntity != EXGAME) {
                    syntaxError++; printf("U"); fflush(stdout);
                  }
                  else if(!currentText.isEmpty()) {
                    currentGame.videoFile = currentText.replace("\\", "/");
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
      //printf("ERROR: XML Parser has reported an error: %s\n",
      //       reader.errorString().toStdString().c_str());
    }
    else {
      reader.raiseError("ERROR: Incorrect XML document or not a ExoDos DB.\n");
    }
  }
  else {
    printf("ERROR: Database file '%s/xml/all/MS-DOS.xml' cannot be found or is corrupted.\n",
           config->dbPath.toStdString().c_str());
    reqRemaining = 0;
    return;
  }
  reader.clear();
  xmlFile.close();

  if(syntaxError) {
    printf("WARNING: The database contains %d syntax error(s). "
           "The associated records have been ignored.\n", syntaxError);
  }
  if(!loadedOk) {
    printf("WARNING: The parser has reported errors.\n");
  }

  printf("INFO: ExoDos database has been parsed, %d games have been loaded into memory.\n",
         exoDosDb.count());

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(FRANCHISES);
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
  fetchOrder.append(MANUAL);
  fetchOrder.append(CHIPTUNE);
  fetchOrder.append(GUIDES);
  fetchOrder.append(CHEATS);
  fetchOrder.append(VGMAPS);
  fetchOrder.append(ARTBOOKS);

  // Print list of game names (including alternates):
  if(config->verbosity >= 3) {
    const auto keys = exoDosDb.keys();
    for(const auto &key: std::as_const(keys)) {
      exoDosDb.value(key).title;
      qDebug() << key << exoDosDb.value(key).title;
    }
  }
}

ExoDos::~ExoDos()
{
  if(navidrome.isOpen()) {
    navidrome.close();
    QString oldDatabase = navidrome.connectionName();
    navidrome = QSqlDatabase();
    QSqlDatabase::removeDatabase(oldDatabase);
  }
}

void ExoDos::getSearchResults(QList<GameEntry> &gameEntries,
                              QString searchName, QString)
{
  QString id = pathToId.value(searchName);
  if(!id.isEmpty()) {
    GameEntry exoGame = exoDosDb.value(id);
    if(exoGame.id.isEmpty()) {
      printf("ERROR: Internal error: Database Id '%s' not found in the in-memory database.\n", 
             id.toStdString().c_str());
    } else {
      gameEntries = { exoGame };
    }
  }
}

void ExoDos::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  QString cacheId = game.cacheId;
  game = exoDosDb.value(game.id);
  game.cacheId = cacheId;
  if(game.id.isEmpty()) {
    printf("ERROR: Internal error: Database Id not found in the in-memory database.\n");
  } else {
    fetchGameResources(game, sharedBlobs, cache);
  }
}

void ExoDos::getReleaseDate(GameEntry &)
{
}

void ExoDos::getDeveloper(GameEntry &)
{
}

void ExoDos::getPublisher(GameEntry &)
{
}

void ExoDos::getDescription(GameEntry &)
{
}

void ExoDos::getPlayers(GameEntry &)
{
}

void ExoDos::getAges(GameEntry &)
{
}

void ExoDos::getRating(GameEntry &)
{
}

void ExoDos::getTags(GameEntry &)
{
}

void ExoDos::getFranchises(GameEntry &)
{
}

void ExoDos::getChiptune(GameEntry &game)
{
  if(game.chiptunePath.isEmpty()) {
    QString alternative = baseUrl + "Music/" + game.platform + "/" + game.baseName;
    fillMediaAsset(alternative, game.chiptunePath, game.chiptuneId);
    game.chiptuneId = "";
    if(!game.chiptunePath.isEmpty()) {
      game.chiptunePath.remove(0, baseUrl.size());
    }
  }

  if(!game.chiptunePath.isEmpty()) {
    game.chiptunePath.prepend(baseUrl);
    if(!QFileInfo(game.chiptunePath).exists()) {
      qWarning() << "Track does not exist: " << game.chiptunePath;
      game.chiptunePath = "";
      return;
    }
    QString track = game.chiptunePath;
    track.remove(0, baseUrl.size() + 6);
    QSqlQuery query(navidrome);
    query.setForwardOnly(true);
    query.prepare("SELECT id FROM media_file WHERE path = :path");
    query.bindValue(":path", track);
    if(!query.exec()) {
      qDebug() << query.lastError();
    } else {
      while(query.next()) {
        game.chiptuneId = query.value(0).toString();
      }
      if(game.chiptuneId.isEmpty()) {
        query.finish();
        query.prepare("SELECT id FROM playlist WHERE path = :path");
        query.bindValue(":path", game.chiptunePath);
        if(!query.exec()) {
          qDebug() << query.lastError();
        } else {
          while(query.next()) {
            game.chiptuneId = query.value(0).toString();
          }
        }
      }
    }
    query.finish();
    if(game.chiptuneId.isEmpty()) {
      qWarning() << "Track is not registered in the Navidrome server: " << game.chiptunePath;
      game.chiptunePath = "";
    }
  }
}

void ExoDos::getGuides(GameEntry &game)
{
  getExtras(game.path, "guides", game.guides);
}

void ExoDos::getCheats(GameEntry &game)
{
  getExtras(game.path, "cheats", game.cheats);
}

void ExoDos::getArtbooks(GameEntry &game)
{
  getExtras(game.path, "artbooks", game.artbooks);
}

void ExoDos::getVGMaps(GameEntry &game)
{
  getExtras(game.path, "vgmaps", game.vgmaps);
}

void ExoDos::getCover(GameEntry &game)
{
  getImage(COVER, game.coverFile, game.title, game.platform);
}

void ExoDos::getScreenshot(GameEntry &game)
{
  getImage(SCREENSHOT, game.screenshotFile, game.title, game.platform);
}

void ExoDos::getWheel(GameEntry &game)
{
  getImage(WHEEL, game.wheelFile, game.title, game.platform);
}

void ExoDos::getMarquee(GameEntry &game)
{
  getImage(MARQUEE, game.marqueeFile, game.title, game.platform);
}

void ExoDos::getTexture(GameEntry &game)
{
  getImage(TEXTURE, game.textureFile, game.title, game.platform);
}

void ExoDos::getVideo(GameEntry &game)
{
  QString alternative = baseUrl + "Videos/" + game.platform + "/" + game.baseName;
  fillMediaAsset(alternative, game.videoFile, game.videoFormat);
  if(game.videoFile.isEmpty()) {
    alternative = baseUrl + game.path + "/Extras/";
    fillMediaAsset(alternative, game.videoFile, game.videoFormat, ".*[.]mp4$");
  }
}

void ExoDos::getManual(GameEntry &game)
{
  QString alternative = baseUrl + "Manuals/" + game.platform + "/" + game.baseName;
  fillMediaAsset(alternative, game.manualFile, game.manualFormat);
}

void ExoDos::getExtras(const QString &path, const QString &type, QString &files)
{
  QString assetRegExp = baseUrl + path + "/Extras/";
  assetRegExp = "^" + QRegularExpression::escape(assetRegExp) + ".*";
  QStringList gameExtras = assetPaths.filter(QRegularExpression(assetRegExp));
  for(const auto &pattern: std::as_const(extrasMapping[type])) {
    for(const auto &extra: std::as_const(gameExtras)) {
      QFileInfo file(extra);
      if(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption)
           .match(file.completeBaseName()).hasMatch()) {
        if(!files.contains(extra)) {
          files = files + extra + ";";
        }
      }
    }
  }

  if(!files.isEmpty()) {
    files.chop(1);
  }
}

void ExoDos::getImage(int type, QString &fileName,
                      const QString &gameName, const QString &platform)
{
  QString baseName = gameName + "-0";
  baseName.replace("'", "_").replace(";", "_").replace(":", "_").replace("?", "_");
  for(const auto &type: std::as_const(priorityImages[type])) {
    QString format;
    QString alternative = baseUrl + "Images/" + platform + "/" + type + "/" + baseName;
    fillMediaAsset(alternative, fileName, format);
    if(!fileName.isEmpty()) {
      if(format == "png" || format == "jpg" || format == "gif") {
        break;
      } else {
        fileName = "";
      }
    }
  }
}

void ExoDos::fillMediaAsset(const QString &basePath, QString &file, QString &format,
                            const QString &pattern)
{
  if(!file.isEmpty() && !file.startsWith("/")) {
    file.prepend(baseUrl);
  }
  if(file.isEmpty() || !assetPaths.contains(file)) {
    QString regExp = "^" + QRegularExpression::escape(basePath) + pattern;
    int pos = assetPaths.indexOf(QRegularExpression(regExp));
    if(pos < 0) {
      file = "";
      format = "";
    } else {
      file = assetPaths[pos];
    }
  }
  if(!file.isEmpty()) {
    QFileInfo mediaFile(file);
    format = mediaFile.suffix().toLower();
  }
}

void ExoDos::loadMaps()
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

  schema["LaunchBox"] = EXLAUNCHBOX;
  schema["Game"] = EXGAME;
  schema["Title"] = EXTITLE;
  schema["ReleaseDate"] = EXRELEASEDATE;
  schema["Notes"] = EXNOTES;
  schema["MaxPlayers"] = EXMAXPLAYERS;
  schema["ID"] = EXID;
  schema["CommunityStarRating"] = EXCOMMUNITYSTARRATING;
  schema["Platform"] = EXPLATFORM;
  schema["Rating"] = EXRATING;
  schema["Genre"] = EXGENRE;
  schema["Developer"] = EXDEVELOPER;
  schema["Publisher"] = EXPUBLISHER;
  schema["RootFolder"] = EXROOTFOLDER;
  schema["ApplicationPath"] = EXAPPLICATIONPATH;
  schema["ManualPath"] = EXMANUALPATH;
  schema["MusicPath"] = EXMUSICPATH;
  schema["Series"] = EXSERIES;

  // Reverse Priority (from more prioritary to less prioritary)!

  priorityImages[COVER] = QStringList (
      { "Box - Front", "Box - Front - Reconstructed", "Fanart - Box - Front", "Advertisement Flyer - Front" });
  priorityImages[SCREENSHOT] = QStringList (
      { "Screenshot - Gameplay", "Screenshot - Game Select" });
  priorityImages[WHEEL] = QStringList (
      { "Screenshot - Game Title", "Screenshot - High Scores" });
  priorityImages[MARQUEE] = QStringList (
      { "Fanart - Background", "Advertisement Flyer - Back", "Advertisement Flyer - Front", "Disc","Cart - Front", "Fanart - Disc", "Fanart - Cart - Front" });
  priorityImages[TEXTURE] = QStringList (
      { "Box - Back", "Box - Back - Reconstructed", "Fanart - Box - Back" });
}
