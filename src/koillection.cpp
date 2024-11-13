/***************************************************************************
 *            koillection.cpp
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

#include "koillection.h"
#include "strtools.h"
#include "nametools.h"
#include "platform.h"

#include <QDir>
#include <QMap>
#include <QUuid>
#include <QSql>
#include <QSqlError>
#include <QSqlQuery>
#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonArray>

Koillection::Koillection(QSharedPointer<NetManager> manager)
{
  this->config = config;
  db = QSqlDatabase::addDatabase("QMYSQL");
  netComm = new NetComm(manager);
  connect(netComm, &NetComm::dataReady, &q, &QEventLoop::quit);
}

Koillection::~Koillection()
{
  db.close();
  delete netComm;
}

bool Koillection::loadOldGameList(const QString &)
{
  return false;
}

bool Koillection::skipExisting(QList<GameEntry> &, QSharedPointer<Queue>)
{
  return false;
}

void Koillection::assembleList(QString &finalOutput, QList<GameEntry> &gameEntries)
{
  finalOutput = QString();

  db.setHostName(config->dbServer);
  db.setDatabaseName(config->koiDb);
  db.setUserName(config->dbUser);
  db.setPassword(config->dbPassword);
  if(!db.open()) {
    printf("ERROR: Connection to Koillection database has failed.\n");
  }

  netComm->request("http://www.localnet.priv:88/api/authentication_token",
                   "{\"username\": \"" + config->user + "\",\"password\": \"" + config->password + "\"}",
                   headers);
  q.exec();
  jsonObj = QJsonDocument::fromJson(netComm->getData()).object();
  if(jsonObj.contains("token")) {
    koiToken = jsonObj["token"].toString();
    headers.append(QPair<QString, QString>("Authorization", "Bearer " + koiToken));
    headersPatch.append(QPair<QString, QString>("Authorization", "Bearer " + koiToken));
  }
  else {
    printf("ERROR: Connection to Koillection API has failed.\n");
  }

  // Search collections for the current platform:
  QString collectionId;
  QString queryString = "select id from koi_collection where platform='" + config->platform + "'";
  QSqlQuery query(queryString);
  if(query.next()) {
    collectionId = query.value(0).toString();
  }
  else {
    printf("ERROR: The platform %s is not yet created as a collection. Create it first.\n", config->platform.toStdString().c_str());
  }
  query.finish();
  QMap<QString, QString> platformsList;
  queryString = "select id, platform from koi_tag where platform is not null";
  if(query.exec(queryString)) {
    while(query.next()) {
      platformsList[query.value(1).toString()] = query.value(0).toString();
    }
  }
  if(platformsList.isEmpty()) {
    printf("ERROR: The Platforms category cannot be extracted, verify the Koillection configuration.");
  }
  query.finish();

  QString categoryCatalog;
  queryString = "select id from koi_tag_category where label='Catalog Status'";
  query.exec(queryString);
  if(query.next()) {
    categoryCatalog = query.value(0).toString();
  }
  else {
    printf("ERROR: The Catalog Category cannot be found, verify the Koillection database.\n");
  }
  query.finish();
  QMap<QString, QString> catalogTagList;
  if(!categoryCatalog.isEmpty()) {
    queryString = "select id, label from koi_tag where category_id='" + categoryCatalog + "'";
    if(query.exec(queryString)) {
      while(query.next()) {
        catalogTagList[query.value(1).toString()] = query.value(0).toString();
      }
    }
    if(catalogTagList.isEmpty()) {
      printf("ERROR: The Catalog Category tags cannot be extracted, verify the Koillection configuration.");
    }
    query.finish();
  }

  QString categoryYears;
  queryString = "select id from koi_tag_category where label='Year'";
  query.exec(queryString);
  if(query.next()) {
    categoryYears = query.value(0).toString();
  }
  else {
    printf("ERROR: The Year tags cannot be found, verify the Koillection database.\n");
  }
  query.finish();
  QMap<QString, QString> yearsList;
  if(!categoryYears.isEmpty()) {
    queryString = "select id, label from koi_tag where category_id='" + categoryYears + "'";
    if(query.exec(queryString)) {
      while(query.next()) {
        yearsList[query.value(1).toString()] = query.value(0).toString();
      }
    }
    if(yearsList.isEmpty()) {
      printf("ERROR: The Years category cannot be extracted, verify the Koillection configuration.");
    }
    query.finish();
  }

  QString categoryGenres;
  queryString = "select id from koi_tag_category where label='Genre'";
  query.exec(queryString);
  if(query.next()) {
    categoryGenres = query.value(0).toString();
  }
  else {
    printf("ERROR: The Genres category cannot be found, verify the Koillection database.\n");
  }
  query.finish();
  QMap<QString, QString> genresList;
  if(!categoryGenres.isEmpty()) {
    queryString = "select id, label from koi_tag where category_id='" + categoryGenres + "'";
    if(query.exec(queryString)) {
      while(query.next()) {
        genresList[query.value(1).toString()] = query.value(0).toString();
      }
    }
    if(genresList.isEmpty()) {
      printf("ERROR: The Genres tags cannot be extracted, verify the Koillection configuration.");
    }
    query.finish();
  }

  QString categoryFranchises;
  queryString = "select id from koi_tag_category where label='Franchise'";
  query.exec(queryString);
  if(query.next()) {
    categoryFranchises = query.value(0).toString();
  }
  else {
    printf("ERROR: The Franchises category cannot be found, verify the Koillection database.\n");
  }
  query.finish();
  QMap<QString, QString> franchisesList;
  if(!categoryFranchises.isEmpty()) {
    queryString = "select id, label from koi_tag where category_id='" + categoryFranchises + "'";
    if(query.exec(queryString)) {
      while(query.next()) {
        franchisesList[query.value(1).toString()] = query.value(0).toString();
      }
    }
    if(franchisesList.isEmpty()) {
      printf("ERROR: The Franchises tags cannot be extracted, verify the Koillection configuration.");
    }
    query.finish();
  }

  QString videogamesTemplate;
  queryString = "select id from koi_template where name='Videogames'";
  query.exec(queryString);
  if(query.next()) {
    videogamesTemplate = query.value(0).toString();
  }
  else {
    printf("ERROR: The Videogames template cannot be found, verify the Koillection configuration.\n");
  }
  query.finish();
  QMap<QString, QMap<QString, QString>> templateDefinition;
  if(!videogamesTemplate.isEmpty()) {
    queryString = "select name, owner_id, type, position, choice_list_id from koi_field where template_id='" + videogamesTemplate + "'";
    query.exec(queryString);
    while(query.next()) {
      QMap<QString, QString> fieldDefinition;
      fieldDefinition["owner_id"] = query.value(1).toString();
      fieldDefinition["type"] = query.value(2).toString();
      fieldDefinition["position"] = query.value(3).toString();
      fieldDefinition["choice_list_id"] = query.value(4).toString();
      templateDefinition[query.value(0).toString()] = fieldDefinition;
    }
    if(templateDefinition.isEmpty()) {
      printf("ERROR: The Videogames template cannot be extracted, verify the Koillection configuration.\n");
    }
    query.finish();
  }

  if(collectionId.isEmpty() || koiToken.isEmpty() || categoryYears.isEmpty() ||
      categoryGenres.isEmpty() || videogamesTemplate.isEmpty() || templateDefinition.isEmpty() ||
      genresList.isEmpty() || platformsList.isEmpty() || yearsList.isEmpty() ||
      categoryFranchises.isEmpty() || franchisesList.isEmpty() ||
      categoryCatalog.isEmpty() || catalogTagList.isEmpty()) {
    return;
  }

  int dots = 0;
  // Always make dotMod at least 1 or it will give "floating point exception" when modulo
  int dotMod = gameEntries.length() * 0.1 + 1;
  if(dotMod == 0)
    dotMod = 1;

  for(const auto &entry: std::as_const(gameEntries)) {
    if(dots % dotMod == 0) {
      printf(".");
      fflush(stdout);
    }
    dots++;

    QString entryPath = entry.path;
    if(config->relativePaths) {
      entryPath.replace(config->inputFolder, ".");
    }

    // Prepare the contents fields:
    QString name = entry.title;
    if(name.isEmpty()) {
      name = "N/A";
    }
    QString fullName = QFileInfo(entryPath).completeBaseName();
    if(Platform::get().getFamily(config->platform) == "arcade" &&
        !config->mameMap[fullName.toLower()].isEmpty()) {
      fullName = config->mameMap[fullName.toLower()];
    }
    QString platformCode = config->platform;
    QString platform = "[\"" + Platform::get().getAliases(config->platform).at(1) + "\"]";
    QString gameFile = entryPath;
    gameFile = StrTools::uriEscape(gameFile).replace("/share/Games/", "/uploads/games/");
    QString launch = "retro://" + config->platform + StrTools::uriEscape(entryPath);

    bool anyData = false;
    float rating = 0.0;
    if(!entry.rating.isEmpty()) {
      rating = entry.rating.toFloat() * 10.0;
      anyData = true;
    }
    /* QString youtubeSearch = "https://www.youtube.com/results?search_query=gameplay";
    QString searchString = platform + " " + name;
    youtubeSearch = youtubeSearch + StrTools::uriEscape(searchString) + "&sp=EgIQAQ%253D%253D"; */
    QString addedOn = "";
    if(QFileInfo(entryPath).exists()) {
      addedOn = QFileInfo(entryPath).lastModified().date().toString("yyyy/MM/dd");
    }
    QString releasedOn = "";
    if(!entry.releaseDate.isEmpty()) {
      releasedOn = QDate::fromString(entry.releaseDate, "yyyyMMdd").toString("yyyy/MM/dd");
      anyData = true;
    }
    QString description = "";
    if(!entry.description.isEmpty()) {
      description = entry.description.trimmed();
      /*while(description.contains("\n\n")) {
        description.replace("\n\n", "\n");
      }*/
      anyData = true;
    }
    QString trivia = "";
    if(!entry.trivia.isEmpty()) {
      trivia = entry.trivia.trimmed();
      while(trivia.contains("\n\n")) {
        trivia.replace("\n\n", "</li><li class=\"trivia\">");
      }
      trivia.prepend("<ul class=\"trivia\"><li class=\"trivia\">");
      trivia.append("</li></ul><br>");
      anyData = true;
    }
    QString developer = "";
    if(!entry.developer.isEmpty()) {
      developer = entry.developer;
      anyData = true;
    }
    QString editor = "";
    if(!entry.publisher.isEmpty()) {
      editor = entry.publisher;
      anyData = true;
    }
    QString maxPlayers = "";
    if(!entry.players.isEmpty()) {
      maxPlayers = entry.players;
      anyData = true;
    }
    QString ageRating = "";
    if(!entry.ages.isEmpty()) {
      ageRating = StrTools::agesLabel(entry.ages);
      anyData = true;
    }
    QStringList genres = {};
    if(!entry.tags.isEmpty()) {
      genres = entry.tags.split(", ");
      anyData = true;
    }
    QStringList franchises = {};
    if(!entry.franchises.isEmpty()) {
      franchises = entry.franchises.split(", ");
      anyData = true;
    }
    QStringList guides = {};
    if(!entry.guides.isEmpty()) {
      guides = entry.guides.split(" ");
      guides.sort();
    }
    QStringList vgmaps = {};
    if(!entry.vgmaps.isEmpty()) {
      vgmaps = entry.vgmaps.split(" ");
    }
    QDateTime dateEpoch;
    QString firstPlayed = "";
    if(entry.firstPlayed) {
      dateEpoch.setSecsSinceEpoch(entry.firstPlayed);
      firstPlayed = dateEpoch.toString(Qt::ISODate);
    }
    QString lastPlayed = "";
    if(entry.lastPlayed) {
      dateEpoch.setSecsSinceEpoch(entry.lastPlayed);
      lastPlayed = dateEpoch.toString(Qt::ISODate);
    }
    QString timePlayed = "";
    if(entry.timePlayed) {
      qint64 minutesPlayed = entry.timePlayed / 60;
      qint64 hoursPlayed = (minutesPlayed > 60) ? minutesPlayed / 60 : 0;
      minutesPlayed -= hoursPlayed * 60;
      timePlayed = QString::number(hoursPlayed) + " hours, " + QString::number(minutesPlayed) + " minutes";
    }
    QString timesPlayed = "";
    if(entry.timesPlayed) {
      timesPlayed = QString::number(entry.timesPlayed);
    }
    // Prepare the picture fields:
    // mainpicture/boxpic
    bool anyImage = false;
    QString mainPicture = "";
    if(entry.coverFile.isEmpty()) {
      mainPicture = "/opt/pegasus/roms/default/missingboxart.png";
    } else {
      // The replace here IS supposed to be 'inputFolder' and not 'mediaFolder' because we only want the path
      // to be relative if '-o' hasn't been set. So this will only make it relative if the path is equal to
      // inputFolder which is what we want.
      mainPicture = config->relativePaths?QString(entry.coverFile).replace(config->inputFolder, "."):entry.coverFile;
      if(!entry.coverSrc.isEmpty()) {
        anyImage = true;
      }
    }
    // redundant mainPicture = config->relativePaths?QString(mainPicture).replace(config->inputFolder, "."):mainPicture;
    mainPicture.replace("/opt/pegasus/roms", "/uploads/pictures");
    mainPicture = StrTools::uriEscape(mainPicture);
    // artwork
    QString artwork = "";
    if(!entry.marqueeFile.isEmpty()) {
      artwork = config->relativePaths?QString(entry.marqueeFile).replace(config->inputFolder, "."):entry.marqueeFile;
      artwork.replace("/opt/pegasus/roms", "/uploads/pictures");
      artwork = StrTools::uriEscape(artwork);
      anyImage = true;
    }
    // screenshot1
    QString screenshot1 = "";
    if(!entry.screenshotFile.isEmpty()) {
      screenshot1 = config->relativePaths?QString(entry.screenshotFile).replace(config->inputFolder, "."):entry.screenshotFile;
      screenshot1.replace("/opt/pegasus/roms", "/uploads/pictures");
      screenshot1 = StrTools::uriEscape(screenshot1);
      anyImage = true;
    }
    // screenshot2
    QString screenshot2 = "";
    if(!entry.wheelFile.isEmpty()) {
      screenshot2 = config->relativePaths?QString(entry.wheelFile).replace(config->inputFolder, "."):entry.wheelFile;
      screenshot2.replace("/opt/pegasus/roms", "/uploads/pictures");
      screenshot2 = StrTools::uriEscape(screenshot2);
      anyImage = true;
    }
    // backpic
    QString backpic = "";
    if(!entry.textureFile.isEmpty()) {
      backpic = config->relativePaths?QString(entry.textureFile).replace(config->inputFolder, "."):entry.textureFile;
      backpic.replace("/opt/pegasus/roms", "/uploads/pictures");
      backpic = StrTools::uriEscape(backpic);
      anyImage = true;
    }
    // video
    QString video = "";
    if(!entry.videoFile.isEmpty()) {
      video = config->relativePaths?QString(entry.videoFile).replace(config->inputFolder, "."):entry.videoFile;
      video.replace("/opt/pegasus/roms", "/uploads/pictures");
      video = StrTools::uriEscape(video);
    }
    // Soundtrack
    QString audioFile = "", audioUrl = "";
    if(!entry.chiptuneId.isEmpty() && !entry.chiptunePath.isEmpty()) {
      audioUrl = "http://media.localnet.priv:4533/share/" + entry.chiptuneId;
      audioFile = "retro://music" + StrTools::uriEscape(entry.chiptunePath);
    }
    // manual
    QString manual = "";
    if(!entry.manualFile.isEmpty()) {
      manual = config->relativePaths?QString(entry.manualFile).replace(config->inputFolder, "."):entry.manualFile;
      manual.replace("/opt/pegasus/roms", "/uploads/pictures");
      manual = StrTools::uriEscape(manual);
    }

    // Assign tags to the game:
    QStringList tagList = {};

    // Year
    if(!releasedOn.isEmpty()) {
      if(yearsList.contains(releasedOn.left(4))) {
        tagList << yearsList[releasedOn.left(4)];
      }
      else {
        printf("\nERROR: Year tag for date '%s' does not exist.", releasedOn.toStdString().c_str());
      }
    }
    // Platform
    tagList << platformsList[platformCode];
    // Genres
    QString genreId = "";
    if(!genres.isEmpty()) {
      for(const auto &genre: std::as_const(genres)) {
        if(!genre.isEmpty()) {
          QString sanitizedGenre = genre;
          sanitizedGenre.replace('"', "'");
          sanitizedGenre.replace("\\", "-");
          if(!genresList.contains(sanitizedGenre)) {
            printf("\nINFO: Creating new genre tag '%s'... ", genre.toStdString().c_str());
            // JSON request contents must never contain the double quote (") or backslash (\) characters
            QString request = "{\"label\": \"" + sanitizedGenre + "\", " +
                        "\"category\": \"/api/tag_categories/" + categoryGenres + "\", " +
                        "\"visibility\": \"public\" }";
            netComm->request("http://www.localnet.priv:88/api/tags", request, headers);
            q.exec();
            jsonObj = QJsonDocument::fromJson(netComm->getData()).object();
            if(jsonObj.contains("id")) {
              genreId = jsonObj["id"].toString();
              genresList[sanitizedGenre] = genreId;
              printf(" Done.");
            }
            else {
              printf("ERROR: Creation of genre tag has failed.");
            }
          }
          tagList << genresList[sanitizedGenre];
        }
      }
    }
    // Franchises
    QString franchiseId = "";
    if(!franchises.isEmpty()) {
      for(const auto &franchise: std::as_const(franchises)) {
        if(!franchise.isEmpty()) {
          QString sanitizedFranchise = franchise;
          sanitizedFranchise.replace('"', "'");
          sanitizedFranchise.replace("\\", "-");
          if(!franchisesList.contains(sanitizedFranchise)) {
            printf("\nINFO: Creating new franchise tag '%s'... ", franchise.toStdString().c_str());
            // JSON request contents must never contain the double quote (") or backslash (\) characters
            QString request = "{\"label\": \"" + sanitizedFranchise + "\", " +
                        "\"category\": \"/api/tag_categories/" + categoryFranchises + "\", " +
                        "\"visibility\": \"public\" }";
            netComm->request("http://www.localnet.priv:88/api/tags", request, headers);
            q.exec();
            jsonObj = QJsonDocument::fromJson(netComm->getData()).object();
            if(jsonObj.contains("id")) {
              franchiseId = jsonObj["id"].toString();
              franchisesList[sanitizedFranchise] = franchiseId;
              printf(" Done.");
            }
            else {
              printf("ERROR: Creation of franchise tag has failed.");
            }
          }
          tagList << franchisesList[sanitizedFranchise];
        }
      }
    }
    // Catalog
    QString catalogTag = "";
    if(!audioUrl.isEmpty()) {
      tagList << catalogTagList.value("With Soundtrack");
    }
    if(!video.isEmpty()) {
      tagList << catalogTagList.value("With Video");
    }
    if(!manual.isEmpty()) {
      tagList << catalogTagList.value("With Handbook");
    }
    if(!guides.isEmpty()) {
      tagList << catalogTagList.value("With Guides");
    }
    if(!vgmaps.isEmpty()) {
      tagList << catalogTagList.value("With Guides");
    }
    if(!anyImage) {
      tagList << catalogTagList.value("Missing Images");
    }
    if(!anyData) {
      tagList << catalogTagList.value("Missing Data");
    }
    if(entry.favourite) {
      tagList << catalogTagList.value("Favourite");
    }
    if(entry.completed) {
      tagList << catalogTagList.value("Completed");
    }
    if(entry.played) {
      tagList << catalogTagList.value("Played");
    }
    tagList.removeDuplicates();

    // Find if game is already in the database:
    QString itemId = "";
    query.prepare("select id, ext_idx from koi_item where ext_idx = :gameFile");
    query.bindValue(":gameFile", entryPath);
    if(query.exec()) {
      while(query.next()) {
        if(entryPath == query.value(1).toString()) {
          itemId = query.value(0).toString();
        }
      }
    }
    query.finish();

    //If it does not exist, create game in the database:
    if(itemId.isEmpty()) {
      // JSON request contents must never contain the double quote (") or backslash (\) characters
      QString sanitizedName = name;
      sanitizedName.replace('"', "'");
      sanitizedName.replace("\\", "-");
      printf("\nINFO: Creating new item in database: %s / %s... ", platformCode.toStdString().c_str(), name.toStdString().c_str());
      QString request = "{\"name\": \"" + sanitizedName + "\", \"quantity\": 1, " +
                        "\"collection\": \"/api/collections/" + collectionId + "\", " +
                        "\"tags\": [ \"/api/tags/" + tagList.join("\", \"/api/tags/") + "\" ], " +
                        "\"visibility\": \"public\" }";
      netComm->request("http://www.localnet.priv:88/api/items", request, headers);
      q.exec();
      jsonObj = QJsonDocument::fromJson(netComm->getData()).object();
      if(jsonObj.contains("id")) {
        itemId = jsonObj["id"].toString();
      }
      else {
        // Very probably a missing tag
        printf("ERROR: Creation of item has failed!");
      }
    }
    else {
      bool mustUpdate = false;
      QStringList tagsForItem = {};
      query.exec("select tag_id from koi_item_tag WHERE item_id='" + itemId + "'");
      while(query.next()) {
        tagsForItem << query.value(0).toString();
      }
      if(tagsForItem.count() != tagList.count()) {
        mustUpdate = true;
      }
      else {
        for(const auto &tag: std::as_const(tagList)) {
          if(!tagsForItem.contains(tag)) {
            mustUpdate = true;
          }
        }
      }
      if(mustUpdate) {
        printf("\nINFO: Tags for the existing item %s are not updated, fixing...", itemId.toStdString().c_str());
        QString request = "{\"tags\": [ \"/api/tags/" + tagList.join("\", \"/api/tags/") + "\" ]}";
        netComm->request("http://www.localnet.priv:88/api/items/" + itemId, request, headersPatch, "PATCH");
        q.exec();
        jsonObj = QJsonDocument::fromJson(netComm->getData()).object();
        if(jsonObj.contains("id")) {
          itemId = jsonObj["id"].toString();
        }
        else {
          // Very probably a missing tag
          printf("ERROR: Update of the tags has failed, please review that all tags are created.");
        }
      }
      printf("\nINFO: Updating existing item in database: %s ", fullName.toStdString().c_str());
    }
    if(itemId.isEmpty()) {
      continue;
    }

    QMap<QString, QPair<QString, QPair<QString, QString>>> datumsItemValues;
    query.exec("select id, label, type, value, image, position from koi_datum where item_id='" + itemId + "'");
    while(query.next()) {
      if(query.value(2).toString() == "image") {
        datumsItemValues[query.value(1).toString()] = qMakePair(query.value(0).toString(), qMakePair(query.value(4).toString(), query.value(5).toString()));
      }
      else {
        datumsItemValues[query.value(1).toString()] = qMakePair(query.value(0).toString(), qMakePair(query.value(3).toString(), query.value(5).toString()));
      }
    }

    // boxpic, name and platform on the Item Header, not (only) in the Item Datums:
    query.prepare("update koi_item set name = :name, collection_id = :collection_id, image = :image, ext_idx = :ext_idx where id = :id");
    query.bindValue(":image", mainPicture);
    query.bindValue(":name", fullName);
    query.bindValue(":collection_id", collectionId);
    query.bindValue(":ext_idx", entryPath);
    query.bindValue(":id", itemId);
    if(!query.exec()) {
      qDebug() << query.lastError();
      printf("ERROR: Failed database update of the item header.\n");
    }
    query.finish();

    QStringList noUpdateFields = {};
    QStringList imageFields = {"Box Back", "Artwork/Media", "Title Screen", "Gameplay Screen"};
    QStringList videoFields = {"Gameplay Video", "Soundtrack Player"};
    QStringList choiceFields = {"Platform"};
    QList<QPair <QString, QString>> datumsItem = {};
    if(!genres.isEmpty()) {
      datumsItem << QPair<QString, QString>("Genres", "[\"" + genres.join("\",\"") + "\"]");
    }
    else {
      datumsItem << QPair<QString, QString>("Genres", "");
    }
    if(!franchises.isEmpty()) {
      datumsItem << QPair<QString, QString>("Franchises", "[\"" + franchises.join("\",\"") + "\"]");
    }
    else {
      datumsItem << QPair<QString, QString>("Franchises", "");
    }
    if(!guides.isEmpty()) {
      QString guidesLinks;
      int pos = 1;
      for(const auto &link: std::as_const(guides)) {
        guidesLinks += "<a href=\"" + link + "\" target=\"_blank\">" + QString::number(pos) + "</a> ";
        pos++;
      }
      datumsItem << QPair<QString, QString>("Guides", guidesLinks.replace(config->guidesPath, "/uploads/guides"));
    }
    else {
      datumsItem << QPair<QString, QString>("Guides", "");
    }
    if(!vgmaps.isEmpty()) {
      QString vgmapsLinks;
      for(const auto &link: std::as_const(vgmaps)) {
        QFileInfo map(link);
        QString mapPlatform = map.absolutePath();
        map.setFile(mapPlatform);
        mapPlatform = map.baseName();
        vgmapsLinks += "<a href=\"" + link + "\" target=\"_blank\">" + mapPlatform + "</a> ";
      }
      datumsItem << QPair<QString, QString>("Maps", vgmapsLinks.replace(config->vgmapsPath, "/uploads/vgmaps"));
    }
    else {
      datumsItem << QPair<QString, QString>("Maps", "");
    }
    datumsItem << QPair<QString, QString>("Players", maxPlayers);
    datumsItem << QPair<QString, QString>("Age Rating", ageRating);
    datumsItem << QPair<QString, QString>("Editor", editor);
    datumsItem << QPair<QString, QString>("Developer", developer);
    datumsItem << QPair<QString, QString>("Description", description);
    datumsItem << QPair<QString, QString>("Trivia", trivia);
    datumsItem << QPair<QString, QString>("Released on", releasedOn);
    datumsItem << QPair<QString, QString>("Added on", addedOn);
    if(rating > 0.0) {
      datumsItem << QPair<QString, QString>("Rating", QString::number(rating, 'f', 1));
    }
    else {
      datumsItem << QPair<QString, QString>("Rating", "");
    }
    datumsItem << QPair<QString, QString>("Manual", manual);
    datumsItem << QPair<QString, QString>("Launch", launch);
    datumsItem << QPair<QString, QString>("Game File", gameFile);
    datumsItem << QPair<QString, QString>("Soundtrack", audioFile);
    datumsItem << QPair<QString, QString>("Name", name);
    datumsItem << QPair<QString, QString>("Platform", platform);
    datumsItem << QPair<QString, QString>("Favourite", entry.favourite ? "1" : "0");
    datumsItem << QPair<QString, QString>("Played", entry.played ? "1" : "0");
    datumsItem << QPair<QString, QString>("Completed", entry.completed ? "1" : "0");
    datumsItem << QPair<QString, QString>("First Played", firstPlayed);
    datumsItem << QPair<QString, QString>("Last Played", lastPlayed);
    datumsItem << QPair<QString, QString>("Times Played", timesPlayed);
    datumsItem << QPair<QString, QString>("Total Time Played", timePlayed);
    datumsItem << QPair<QString, QString>("Box Back", backpic);
    datumsItem << QPair<QString, QString>("Artwork/Media", artwork);
    datumsItem << QPair<QString, QString>("Gameplay Screen", screenshot1);
    datumsItem << QPair<QString, QString>("Title Screen", screenshot2);
    datumsItem << QPair<QString, QString>("Gameplay Video", video);
    datumsItem << QPair<QString, QString>("Soundtrack Player", audioUrl);

    bool doRollback = false;
    bool postTransaction = true;
    bool postStatement = false;
    if(!db.transaction()) {
      printf("ERROR: The database does not support transactions. Please review the configuration. Moving forward with atomic inserts.\n");
      postTransaction = false;
    }

    QMapIterator<QString, QPair<QString, QPair<QString, QString>>> iterator(datumsItemValues);
    while(iterator.hasNext()) {
      iterator.next();
      bool notInTemplate = true;
      for(const auto &datum: std::as_const(datumsItem)) {
        if(notInTemplate && datum.first == iterator.key()) {
          notInTemplate = false;
          break;
        }
      }
      if(notInTemplate) {
        // Delete obsoleted fields
        query.prepare("delete from koi_datum where id = :id");
        query.bindValue(":id", iterator.value().first);
        if(!query.exec()) {
          printf("ERROR: Could not delete obsolete field '%s' from the database.\n", iterator.key().toStdString().c_str());
          doRollback = true;
          qDebug() << query.lastError();
        }
        query.finish();
      }
    }

    for(const auto &datumItem: std::as_const(datumsItem)) {
      if(datumsItemValues.contains(datumItem.first) &&
          templateDefinition.contains(datumItem.first)) {
        // Existing field in the DB
        if((datumsItemValues[datumItem.first].second.first != datumItem.second ||
            datumItem.second.isEmpty()) &&
            !noUpdateFields.contains(datumItem.first)) {
          // Value has to be updated
          if(datumItem.second.isEmpty()) {
            query.prepare("delete from koi_datum where id = :id");
          }
          else {
            if(imageFields.contains(datumItem.first)) {
              query.prepare("update koi_datum set image = :value, position = :position where id = :id");
            }
            else if(videoFields.contains(datumItem.first)) {
              query.prepare("update koi_datum set video = :value, position = :position where id = :id");
            }
            else {
              query.prepare("update koi_datum set value = :value, position = :position where id = :id");
            }
            query.bindValue(":value", datumItem.second);
            query.bindValue(":position", templateDefinition[datumItem.first]["position"]);
          }
          query.bindValue(":id", datumsItemValues[datumItem.first].first);
          postStatement = true;
        }
        else if(datumsItemValues[datumItem.first].second.second !=
                 templateDefinition[datumItem.first]["position"]) {
          // Update position of the field if necessary
          query.prepare("update koi_datum set position = :position where id = :id");
          query.bindValue(":id", datumsItemValues[datumItem.first].first);
          query.bindValue(":position", templateDefinition[datumItem.first]["position"]);
          postStatement = true;
        }
      }
      else if(templateDefinition.contains(datumItem.first) && !datumItem.second.isEmpty()) {
        // Missing field to be added to the DB
        if(!datumItem.second.isEmpty() && imageFields.contains(datumItem.first)) {
          query.prepare("insert into koi_datum (id, item_id, owner_id, type, label, image, position)"
                                       "values (:id, :item_id, :owner_id, :type, :label, :value, :position)");
        }
        else if(!datumItem.second.isEmpty() && videoFields.contains(datumItem.first)) {
          query.prepare("insert into koi_datum (id, item_id, owner_id, type, label, video, position)"
                                       "values (:id, :item_id, :owner_id, :type, :label, :value, :position)");
        }
        else if(choiceFields.contains(datumItem.first)) {
          query.prepare("insert into koi_datum (id, item_id, owner_id, type, label, value, position, choice_list_id)"
                                       "values (:id, :item_id, :owner_id, :type, :label, :value, :position, :choice_list_id)");
          query.bindValue(":choice_list_id", templateDefinition[datumItem.first]["choice_list_id"]);
        }
        else {
          query.prepare("insert into koi_datum (id, item_id, owner_id, type, label, value, position)"
                                       "values (:id, :item_id, :owner_id, :type, :label, :value, :position)");
        }
        query.bindValue(":id", QUuid::createUuid().toString(QUuid::WithoutBraces));
        query.bindValue(":item_id", itemId);
        query.bindValue(":owner_id", templateDefinition[datumItem.first]["owner_id"]);
        query.bindValue(":type", templateDefinition[datumItem.first]["type"]);
        query.bindValue(":label", datumItem.first);
        query.bindValue(":value", datumItem.second);
        query.bindValue(":position", templateDefinition[datumItem.first]["position"]);
        postStatement = true;
      }
      if(postStatement) {
        if(!query.exec()) {
          printf("ERROR: Could not create/update field '%s' in the database.\n", datumItem.first.toStdString().c_str());
          doRollback = true;
          qDebug() << query.lastError();
        }
        query.finish();
      }
      postStatement = false;
    }

    datumsItemValues.clear();
    datumsItem.clear();
    if(doRollback && postTransaction) {
      db.rollback();
    }
    else if(postTransaction && !db.commit()) {
      printf("ERROR: Database commit failed. Trying rollback.\n");
      db.rollback();
    }
  }
  // Delete entries that do not exist anymore:
  printf("\nSearching for obsolete game entries...\n");
  query.exec("select id, ext_idx from koi_item where collection_id = '" + collectionId + "'");
  while(query.next()) {
    bool presentInCache = false;
    QString fileToCheck = query.value(1).toString();
    QFileInfo gamePath(fileToCheck);
    for(auto &entryCheck: gameEntries) {
      if(config->relativePaths) {
        entryCheck.path.replace(config->inputFolder, ".");
      }
      if(entryCheck.path == fileToCheck) {
        presentInCache = true;
        break;
      }
    }
    if(!gamePath.isFile() || !presentInCache) {
      printf("INFO: Item '%s' corresponding to a non-existant file will be deleted.\n", query.value(0).toString().toStdString().c_str());
      QString request = "";
      netComm->request("http://www.localnet.priv:88/api/items/" + query.value(0).toString(), request, headers, "DELETE");
      q.exec();
      // Not sure how to check for the result of the delete operation
    }
  }
}

bool Koillection::canSkip()
{
  return true;
}

QString Koillection::getGameListFileName()
{
  return QString("null");
}

QString Koillection::getInputFolder()
{
  return QString(QDir::homePath() + "/RetroPie/roms/" + config->platform);
}

QString Koillection::getGameListFolder()
{
  return config->inputFolder;
}

QString Koillection::getCoversFolder()
{
  return config->mediaFolder + "/covers";
}

QString Koillection::getScreenshotsFolder()
{
  return config->mediaFolder + "/screenshots";
}

QString Koillection::getWheelsFolder()
{
  return config->mediaFolder + "/wheels";
}

QString Koillection::getMarqueesFolder()
{
  return config->mediaFolder + "/marquees";
}

QString Koillection::getTexturesFolder() {
  return config->mediaFolder + "/textures";
}

QString Koillection::getVideosFolder()
{
  return config->mediaFolder + "/videos";
}

QString Koillection::getManualsFolder()
{
  return config->mediaFolder + "/manuals";
}
