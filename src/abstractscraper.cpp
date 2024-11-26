/***************************************************************************
 *            abstractscraper.cpp
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

#include "abstractscraper.h"
#include "platform.h"
#include "nametools.h"
#include "strtools.h"

#include <unistd.h>
#include <QRegularExpression>
#include <QDomDocument>
#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSql>
#include <QSqlError>
#include <QSqlQuery>

AbstractScraper::AbstractScraper(Settings *config,
                                 QSharedPointer<NetManager> manager,
                                 QString threadId)
  : config(config), threadId(threadId)
{
  offlineScraper = false;
  if(threadId != "cache") {
    netComm = new NetComm(manager);
    connect(netComm, &NetComm::dataReady, &q, &QEventLoop::quit);
    negDb = QSqlDatabase::addDatabase("QSQLITE", "negcache" + threadId);
    negDb.setDatabaseName(dbNegCache);
    if(!negDb.open()) {
      printf("ERROR: Could not open/create the negative cache database %s. "
             "Proceeding without it.\n",
             dbNegCache.toStdString().c_str());
      qDebug() << negDb.lastError();
    } else {
      QSqlQuery query(negDb);
      query.setForwardOnly(true);
      query.prepare("SELECT COUNT(*) FROM negativecache WHERE platform='"
                    + config->platform + "' AND scraper='" + config->scraper
                    + "' AND date>" + QString::number(config->negCacheExpiration));
      if(!query.exec()) {
        // Create the table:
        query.finish();
        query.prepare("CREATE TABLE negativecache ("
                       "platform TEXT not NULL, scraper TEXT not NULL, file TEXT not NULL, "
                       "failedname TEXT not NULL, date INTEGER not NULL)");
        if(!query.exec()) {
          qDebug() << query.lastError();
          printf("ERROR: Error creating the negative database table.\n");
        } else {
          query.finish();
          query.prepare("CREATE INDEX easyaccess ON negativecache(platform, scraper, file)");
          if(!query.exec()) {
            qDebug() << query.lastError();
            printf("ERROR: Error creating the negative database index.\n");
          } else {
            query.finish();
            negDbReady = true;
          }
        }
      } else {
        query.next();
        printf("INFO: %d entries have been read from the negative cache.\n",
               query.value(0).toInt());
        negDbReady = true;
      }
    }
  }
}

AbstractScraper::~AbstractScraper()
{
  if(threadId != "cache") {
    delete netComm;
    negDb.close();
  }
}

// Queries the scraping service with searchName and generates a skeleton
// game in gameEntries for each candidate returned.
void AbstractScraper::getSearchResults(QList<GameEntry> &gameEntries,
                                       QString searchName, QString platform)
{
  netComm->request(searchUrlPre + searchName + searchUrlPost);
  q.exec();
  if(netComm->getError() == QNetworkReply::NoError) {
    searchError = false;
  } else {
    printf("Connection error. Is the API down?\n");
    searchError = true;
    return;
  }
  data = netComm->getData();

  GameEntry game;

  while(data.indexOf(searchResultPre.toUtf8()) != -1) {
    nomNom(searchResultPre);

    // Digest until url
    for(const auto &nom: std::as_const(urlPre)) {
      nomNom(nom);
    }
    game.url = baseUrl + "/" + data.left(data.indexOf(urlPost.toUtf8()));

    // Digest until title
    for(const auto &nom: std::as_const(titlePre)) {
      nomNom(nom);
    }
    game.title = data.left(data.indexOf(titlePost.toUtf8()));

    // Digest until platform
    for(const auto &nom: std::as_const(platformPre)) {
      nomNom(nom);
    }
    game.platform = data.left(data.indexOf(platformPost.toUtf8()));

    if(platformMatch(game.platform, platform)) {
      gameEntries.append(game);
    }
  }
}

// Redirects the scraper to fetch each resource valid for the scraper.
void AbstractScraper::fetchGameResources(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache)
{
  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case TITLE:
      if((!cache) || (cache && cache->title.isEmpty())) {
        getTitle(game);
      }
      break;
    case PLATFORM:
      if((!cache) || (cache && cache->platform.isEmpty())) {
        getPlatform(game);
      }
      break;
    case DESCRIPTION:
      if((!cache) || (cache && cache->description.isEmpty())) {
        getDescription(game);
      }
      break;
    case TRIVIA:
      if((!cache) || (cache && cache->trivia.isEmpty())) {
        getTrivia(game);
      }
      break;
    case DEVELOPER:
      if((!cache) || (cache && cache->developer.isEmpty())) {
        getDeveloper(game);
      }
      break;
    case PUBLISHER:
      if((!cache) || (cache && cache->publisher.isEmpty())) {
        getPublisher(game);
      }
      break;
    case PLAYERS:
      if((!cache) || (cache && cache->players.isEmpty())) {
        getPlayers(game);
      }
      break;
    case AGES:
      if((!cache) || (cache && cache->ages.isEmpty())) {
        getAges(game);
      }
      break;
    case RATING:
      if((!cache) || (cache && cache->rating.isEmpty())) {
        getRating(game);
      }
      break;
    case TAGS:
      if((!cache) || (cache && cache->tags.isEmpty())) {
        getTags(game);
      }
      break;
    case FRANCHISES:
      if((!cache) || (cache && cache->franchises.isEmpty())) {
        getFranchises(game);
      }
      break;
    case RELEASEDATE:
      if((!cache) || (cache && cache->releaseDate.isEmpty())) {
        getReleaseDate(game);
      }
      break;
    case COVER:
      if(config->cacheCovers) {
        if((!cache) || (cache && cache->coverData.isNull())) {
          getCover(game);
        }
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots) {
        if((!cache) || (cache && cache->screenshotData.isNull())) {
          getScreenshot(game);
        }
      }
      break;
    case WHEEL:
      if(config->cacheWheels) {
        if((!cache) || (cache && cache->wheelData.isNull())) {
          getWheel(game);
        }
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees) {
        if((!cache) || (cache && cache->marqueeData.isNull())) {
          getMarquee(game);
        }
      }
      break;
    case TEXTURE:
      if(config->cacheTextures) {
        if((!cache) || (cache && cache->textureData.isNull())) {
          getTexture(game);
        }
      }
      break;
    case VIDEO:
      if((config->videos) && (!sharedBlobs.contains("video"))) {
        if((!cache) || (cache && cache->videoData.isEmpty())) {
          getVideo(game);
        }
      }
      break;
    case MANUAL:
      if((config->manuals) && (!sharedBlobs.contains("manual"))) {
        if((!cache) || (cache && cache->manualData.isEmpty())) {
          getManual(game);
        }
      }
      break;
    case CHIPTUNE:
      if(config->chiptunes) {
        if((!cache) || (cache && (cache->chiptuneId.isEmpty() || cache->chiptunePath.isEmpty()))) {
          getChiptune(game);
        }
      }
      break;
    case GUIDES:
      if(config->guides) {
        if((!cache) || (cache && cache->guides.isEmpty())) {
          getGuides(game);
        }
      }
      break;
    case VGMAPS:
      if(config->vgmaps) {
        if((!cache) || (cache && cache->vgmaps.isEmpty())) {
          getVGMaps(game);
        }
      }
      break;
    case CUSTOMFLAGS:
      getCustomFlags(game);
      break;
    default:
      ;
    }
  }
}

// Fill in the game skeleton with the data from the scraper service.
void AbstractScraper::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache)
{
  netComm->request(game.url);
  q.exec();
  data = netComm->getData();
  //printf("URL IS: '%s'\n", game.url.toStdString().c_str());
  //printf("DATA IS:\n'%s'\n", data.data());

  fetchGameResources(game, sharedBlobs, cache);
}

void AbstractScraper::getDescription(GameEntry &game)
{
  if(descriptionPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(descriptionPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(descriptionPre)) {
    nomNom(nom);
  }

  game.description = data.left(data.indexOf(descriptionPost.toUtf8())).replace("&lt;", "<").replace("&gt;", ">");
  game.description = game.description.replace("\\n", "\n");

  // Remove all html tags within description
  game.description = StrTools::stripHtmlTags(game.description);
}

void AbstractScraper::getDeveloper(GameEntry &game)
{
  for(const auto &nom: std::as_const(developerPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(developerPre)) {
    nomNom(nom);
  }
  game.developer = data.left(data.indexOf(developerPost.toUtf8()));
}

void AbstractScraper::getPublisher(GameEntry &game)
{
  if(publisherPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(publisherPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(publisherPre)) {
    nomNom(nom);
  }
  game.publisher = data.left(data.indexOf(publisherPost.toUtf8()));
}

void AbstractScraper::getPlayers(GameEntry &game)
{
  if(playersPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(playersPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(playersPre)) {
    nomNom(nom);
  }
  game.players = StrTools::conformPlayers(data.left(data.indexOf(playersPost.toUtf8())));
}

void AbstractScraper::getAges(GameEntry &game)
{
  if(agesPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(agesPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(agesPre)) {
    nomNom(nom);
  }
  game.ages = StrTools::conformAges(data.left(data.indexOf(agesPost.toUtf8())));
}

void AbstractScraper::getTags(GameEntry &game)
{
  if(tagsPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(tagsPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(tagsPre)) {
    nomNom(nom);
  }
  game.tags = StrTools::conformTags(data.left(data.indexOf(tagsPost.toUtf8())));
}

void AbstractScraper::getFranchises(GameEntry &game)
{
  if(franchisesPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(franchisesPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(franchisesPre)) {
    nomNom(nom);
  }
  game.franchises = StrTools::conformTags(data.left(data.indexOf(franchisesPost.toUtf8())));
}

void AbstractScraper::getRating(GameEntry &game)
{
  if(ratingPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(ratingPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(ratingPre)) {
    nomNom(nom);
  }
  game.rating = data.left(data.indexOf(ratingPost.toUtf8()));
  bool toDoubleOk = false;
  double rating = game.rating.toDouble(&toDoubleOk);
  if(toDoubleOk) {
    game.rating = QString::number(rating / 5.0);
  } else {
    game.rating = "";
  }
}

void AbstractScraper::getReleaseDate(GameEntry &game)
{
  if(releaseDatePre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(releaseDatePre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(releaseDatePre)) {
    nomNom(nom);
  }
  game.releaseDate = StrTools::conformReleaseDate(data.left(data.indexOf(releaseDatePost.toUtf8())).simplified());
}

void AbstractScraper::getCover(GameEntry &game)
{
  if(coverPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(coverPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(coverPre)) {
    nomNom(nom);
  }
  QString coverUrl = data.left(data.indexOf(coverPost.toUtf8())).replace("&amp;", "&");
  if(coverUrl.left(4) != "http") {
    coverUrl.prepend(baseUrl + (coverUrl.left(1) == "/"?"":"/"));
  }
  netComm->request(coverUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.coverData = netComm->getData();
  }
}

void AbstractScraper::getScreenshot(GameEntry &game)
{
  if(screenshotPre.isEmpty()) {
    return;
  }
  // Check that we have enough screenshots
  int screens = data.count(screenshotCounter.toUtf8());
  if(screens >= 1) {
    for(int a = 0; a < screens - (screens / 2); a++) {
      for(const auto &nom: std::as_const(screenshotPre)) {
        nomNom(nom);
      }
    }
    QString screenshotUrl = data.left(data.indexOf(screenshotPost.toUtf8())).replace("&amp;", "&");
    if(screenshotUrl.left(4) != "http") {
      screenshotUrl.prepend(baseUrl + (screenshotUrl.left(1) == "/"?"":"/"));
    }
    netComm->request(screenshotUrl);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.screenshotData = netComm->getData();
    }
  }
}

void AbstractScraper::getWheel(GameEntry &game)
{
  if(wheelPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(wheelPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(wheelPre)) {
    nomNom(nom);
  }
  QString wheelUrl = data.left(data.indexOf(wheelPost.toUtf8())).replace("&amp;", "&");
  if(wheelUrl.left(4) != "http") {
    wheelUrl.prepend(baseUrl + (wheelUrl.left(1) == "/"?"":"/"));
  }
  netComm->request(wheelUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void AbstractScraper::getMarquee(GameEntry &game)
{
  if(marqueePre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(marqueePre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(marqueePre)) {
    nomNom(nom);
  }
  QString marqueeUrl = data.left(data.indexOf(marqueePost.toUtf8())).replace("&amp;", "&");
  if(marqueeUrl.left(4) != "http") {
    marqueeUrl.prepend(baseUrl + (marqueeUrl.left(1) == "/"?"":"/"));
  }
  netComm->request(marqueeUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.marqueeData = netComm->getData();
  }
}

void AbstractScraper::getTexture(GameEntry &game) {
  if(texturePre.isEmpty()) {
    return;
  }

  for(const auto &nom: std::as_const(texturePre)) {
    if(!checkNom(nom)) {
      return;
    }
  }

  for(const auto &nom: std::as_const(texturePre)) {
    nomNom(nom);
  }

  QString textureUrl =
      data.left(data.indexOf(texturePost.toUtf8())).replace("&amp;", "&");
  if(textureUrl.left(4) != "http") {
    textureUrl.prepend(baseUrl + (textureUrl.left(1) == "/" ? "" : "/"));
  }
  netComm->request(textureUrl);
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
      image.loadFromData(netComm->getData())) {
    game.textureData = netComm->getData();
  }
}

void AbstractScraper::getVideo(GameEntry &game)
{
  if(videoPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(videoPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(videoPre)) {
    nomNom(nom);
  }
  QString videoUrl = data.left(data.indexOf(videoPost.toUtf8())).replace("&amp;", "&");
  if(videoUrl.left(4) != "http") {
    videoUrl.prepend(baseUrl + (videoUrl.left(1) == "/"?"":"/"));
  }
  netComm->request(videoUrl);
  q.exec();
  if(netComm->getError() == QNetworkReply::NoError) {
    game.videoData = netComm->getData();
    game.videoFormat = videoUrl.right(3);
  }
}

void AbstractScraper::getManual(GameEntry &game)
{
  if(manualPre.isEmpty()) {
    return;
  }
  for(const auto &nom: std::as_const(manualPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(manualPre)) {
    nomNom(nom);
  }
  QString manualUrl = data.left(data.indexOf(manualPost.toUtf8())).replace("&amp;", "&");
  if(manualUrl.left(4) != "http") {
    manualUrl.prepend(baseUrl + (manualUrl.left(1) == "/"?"":"/"));
  }
  netComm->request(manualUrl);
  q.exec();
  if(netComm->getError() == QNetworkReply::NoError) {
    game.manualData = netComm->getData();
    game.manualFormat = manualUrl.right(3);
  }
}

void AbstractScraper::getChiptune(GameEntry &)
{
}

void AbstractScraper::getGuides(GameEntry &)
{
}

void AbstractScraper::getVGMaps(GameEntry &)
{
}

void AbstractScraper::getTrivia(GameEntry &)
{
}

void AbstractScraper::getCustomFlags(GameEntry &)
{
}

void AbstractScraper::getTitle(GameEntry &)
{
}

void AbstractScraper::getPlatform(GameEntry &)
{
}

// Consume text in data until finding nom.
void AbstractScraper::nomNom(const QString nom, bool including)
{
  data.remove(0, data.indexOf(nom.toUtf8()) + (including?nom.length():0));
}

// Detects if nom is present in the non-consumed part of data.
bool AbstractScraper::checkNom(const QString nom)
{
  if(data.indexOf(nom.toUtf8()) != -1) {
    return true;
  }
  return false;
}

// Applies alias/scummvm/amiga/mame filename to name conversion.
QString AbstractScraper::applyNameMappings(const QString &fileName)
{
  QString baseName = fileName;
  
  if(config->scraper != "import") {
    if(!config->aliasMap[baseName].isEmpty()) {
      QString original = baseName;
      baseName = config->aliasMap[baseName];
      printf("INFO: Alias found converting '%s' to '%s'.\n",
             original.toStdString().c_str(),
             baseName.toStdString().c_str());
    }
    if(Platform::get().getFamily(config->platform) == "amiga") {
      QString nameWithSpaces = config->whdLoadMap[baseName].first;
      if(nameWithSpaces.isEmpty()) {
        baseName = NameTools::getNameWithSpaces(baseName);
      } else {
        baseName = nameWithSpaces;
      }
    }
    if(config->platform == "scummvm") {
      baseName = NameTools::getScummName(baseName, config->scummIni);
    }
    if(Platform::get().getFamily(config->platform) == "arcade" &&
              !config->mameMap[baseName.toLower()].isEmpty()) {
      QString original = baseName;
      baseName = config->mameMap[baseName.toLower()];
      // The following could fix some too long MAME mappings, but as of now
      // they will be treated as subtitles.
      // baseName = baseName.left(baseName.indexOf(" / ", 2)).simplified();
      printf("INFO: 1 Short Mame-style name '%s' converted to '%s'.\n",
             original.toStdString().c_str(),
             baseName.toStdString().c_str());
    }
    // Repeat just in case
    if(!config->aliasMap[baseName].isEmpty()) {
      QString original = baseName;
      baseName = config->aliasMap[baseName];
      printf("INFO: Alias found converting '%s' to '%s'.\n",
             original.toStdString().c_str(),
             baseName.toStdString().c_str());
    }
  }
  
  return baseName;
}

// Applies alias/scummvm/amiga/mame filename to name conversion.
// Applies getUrlQuery conversion to the outcome, including space to
// the special character needed by the scraping service.
// Adds three variations: without subtitles, converting the first numeral
// to arabic or roman format, and both without subtitles and converting
// the first numeral.
// Returns the full list of up to four possibilities, that will be checked
// in order against the scraping service until a match is found.
QStringList AbstractScraper::getSearchNames(const QFileInfo &info)
{
  QString baseName = applyNameMappings(info.completeBaseName());

  QStringList searchNames;
  if(offlineScraper) {
    searchNames << baseName;
  } else {
    NameTools::generateSearchNames(baseName, searchNames, searchNames, false);
  }
  if(config->verbosity >= 2) {
    qDebug() << "Search Names for:" << baseName << ":" << searchNames;
  }

  return searchNames;
}

// Executes the search in the generic multimaps that store the games database access
// information for the offline scrapers (the ones for which the database ids are fully
// accessible). Needs to be executed as part of the scraper overriden getSearchResults.
template <typename T> bool AbstractScraper::getSearchResultsOffline(
                                               QList<T> &gameIds, const QString &searchName,
                                               QMultiMap<QString, T> &fullTitles,
                                               QMultiMap<QString, T> &mainTitles)
{
  QList<T> match = {};
  QListIterator<T> matchIterator(match);

  int pos = 0;
  QStringList safeNameVariations, unsafeNameVariations;
  NameTools::generateSearchNames(searchName, safeNameVariations, unsafeNameVariations, true);
  for(const auto &name: std::as_const(safeNameVariations)) {
    if(config->verbosity >= 1) {
      printf("%d: %s ", pos, name.toStdString().c_str());
    }
    if(fullTitles.contains(name)) {
      if(config->verbosity >= 1) {
        printf("Found!\n");
        pos++;
      }
      match = fullTitles.values(name);
      matchIterator = QListIterator<T> (match);
      while(matchIterator.hasNext()) {
        T gameId = matchIterator.next();
        if(!gameIds.contains(gameId)) {
          gameIds << gameId;
        }
      }
    }
  }

  // If not matches found, try using a fuzzy matcher based on the Damerau/Levenshtein text distance:
  if(gameIds.isEmpty()) {
    QString sanitizedName = safeNameVariations.last();
    if(config->fuzzySearch && sanitizedName.size() >= 6) {
      int maxDistance = config->fuzzySearch;
      if((sanitizedName.size() <= 10) || (config->fuzzySearch < 0)) {
        maxDistance = 1;
      }
      QListIterator<QString> keysIterator(fullTitles.keys());
      while(keysIterator.hasNext()) {
        QString name = keysIterator.next();
        if(StrTools::onlyNumbers(name) == StrTools::onlyNumbers(sanitizedName)) {
          int distance = StrTools::distanceBetweenStrings(sanitizedName, name, false);
          if(distance <= maxDistance) {
            printf("LB FuzzySearch: Found %s = %s (distance %d)!\n", sanitizedName.toStdString().c_str(),
                   name.toStdString().c_str(), distance);
            match = fullTitles.values(name);
            matchIterator = QListIterator<T> (match);
            while(matchIterator.hasNext()) {
              T gameId = matchIterator.next();
              if(!gameIds.contains(gameId)) {
                gameIds << gameId;
              }
            }
          }
        }
      }
    }
  }

  if(gameIds.isEmpty()) {
    pos = 0;
    if(unsafeNameVariations.isEmpty()) {
      printf("\nNo match. Trying lossy name transformations (1)...\n");
      for(const auto &name: std::as_const(safeNameVariations)) {
        if(config->verbosity >= 1) {
          printf("%d: %s ", pos, name.toStdString().c_str());
        }
        if(mainTitles.contains(name)) {
          if(config->verbosity >= 1) {
            printf("Found!\n");
            pos++;
          }
          match = mainTitles.values(name);
          matchIterator = QListIterator<T> (match);
          while(matchIterator.hasNext()) {
            T gameId = matchIterator.next();
            if(!gameIds.contains(gameId)) {
              gameIds << gameId;
            }
          }
        }
      }
    } else {
      printf("\nNo match. Trying lossy name transformations (2)...\n");
      for(const auto &name: std::as_const(unsafeNameVariations)) {
        if(config->verbosity >= 1) {
          printf("%d: %s ", pos, name.toStdString().c_str());
        }
        if(fullTitles.contains(name)) {
          if(config->verbosity >= 1) {
            printf("Found!\n");
            pos++;
          }
          match = fullTitles.values(name);
          matchIterator = QListIterator<T> (match);
          while(matchIterator.hasNext()) {
            T gameId = matchIterator.next();
            if(!gameIds.contains(gameId)) {
              gameIds << gameId;
            }
          }
        }
      }
    }
  }

  printf("INFO: %d match(es) found.\n", gameIds.size());
  return !gameIds.isEmpty();
}

// C++ is not smart/inefficient enough to do on-the-fly template instantiation...

template bool AbstractScraper::getSearchResultsOffline(
                 QList<int> &gameIds, const QString &searchName,
                 QMultiMap<QString, int> &fullTitles,
                 QMultiMap<QString, int> &mainTitles);

template bool AbstractScraper::getSearchResultsOffline(
                 QList<QPair<int, QString>> &gameIds, const QString &searchName,
                 QMultiMap<QString, QPair<int, QString>> &fullTitles,
                 QMultiMap<QString, QPair<int, QString>> &mainTitles);

template bool AbstractScraper::getSearchResultsOffline(
                 QList<QPair<QString, QString>> &gameIds, const QString &searchName,
                 QMultiMap<QString, QPair<QString, QString>> &fullTitles,
                 QMultiMap<QString, QPair<QString, QString>> &mainTitles);

template bool AbstractScraper::getSearchResultsOffline(
                 QList<QPair<QString, QPair<QString, QString>>> &gameIds, const QString &searchName,
                 QMultiMap<QString, QPair<QString, QPair<QString, QString>>> &fullTitles,
                 QMultiMap<QString, QPair<QString, QPair<QString, QString>>> &mainTitles);

//

// Generates the name for the game until a scraper provides an official one.
// This is used to calculate the search names and calculate the best entry and
// the distance to the official name.
QString AbstractScraper::getCompareTitle(QFileInfo info)
{
  QString baseName = applyNameMappings(info.completeBaseName());

  // Now create actual compareTitle
  baseName = baseName.replace("_", " ").left(baseName.indexOf("(")).left(baseName.indexOf("[")).simplified();

  // Always move ", The" to the beginning of the name
  // Three digit articles in English, German, French, Spanish:
  baseName = NameTools::moveArticle(baseName, true);

  // Remove "vX.XXX" versioning string if one is found
  QRegularExpressionMatch match;
  match = QRegularExpression(" v[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4})$",
                             QRegularExpression::CaseInsensitiveOption).match(baseName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    baseName = baseName.left(match.capturedStart(0)).simplified();
  }

  // Remove " rev.X" instances
  match = QRegularExpression(" rev[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4}|[IVX]{1,5})$",
                             QRegularExpression::CaseInsensitiveOption).match(baseName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    baseName = baseName.left(match.capturedStart(0)).simplified();
  }

  return baseName;
}

// Tries to identify the region of the game from the filename. Calls getSearchNames and with
// the outcome, executes the getSearchResults in sequence until a search name is successful.
void AbstractScraper::runPasses(QList<GameEntry> &gameEntries, const QFileInfo &info,
                                const QFileInfo &originalInfo, QString &output,
                                QString &debug, QString pastTitle)
{
  // Reset region priorities to original list from Settings
  regionPrios = config->regionPrios;
  // Autodetect region and append to region priorities
  if((info.fileName().indexOf("(") != -1 || info.fileName().indexOf("[") != -1) && config->region.isEmpty()) {
    QString regionString = info.fileName().toLower();
    if(regionString.contains("(europe)") ||
       regionString.contains("[europe]") ||
       regionString.contains("(e)") ||
       regionString.contains("[e]") ||
       regionString.contains("(eu)") ||
       regionString.contains("[eu]") ||
       regionString.contains("(eur)") ||
       regionString.contains("[eur]")) {
      regionPrios.prepend("eu");
    }
    if(regionString.contains("(usa)") ||
       regionString.contains("[usa]") ||
       regionString.contains("(us)") ||
       regionString.contains("[us]") ||
       regionString.contains("(u)") ||
       regionString.contains("[u]")) {
      regionPrios.prepend("us");
    }
    if(regionString.contains("(world)") ||
       regionString.contains("[world]")) {
      regionPrios.prepend("wor");
    }
    if(regionString.contains("(japan)") ||
      regionString.contains("[japan]") ||
      regionString.contains("(ja)") ||
      regionString.contains("[ja]") ||
      regionString.contains("(jap)") ||
      regionString.contains("[jap]") ||
      regionString.contains("(j)") ||
      regionString.contains("[j]")) {
      regionPrios.prepend("jp");
    }
    if(regionString.contains("brazil")) {
      regionPrios.prepend("br");
    }
    if(regionString.contains("korea")) {
      regionPrios.prepend("kr");
    }
    if(regionString.contains("taiwan")) {
      regionPrios.prepend("tw");
    }
    if(regionString.contains("france")) {
      regionPrios.prepend("fr");
    }
    if(regionString.contains("germany")) {
      regionPrios.prepend("de");
    }
    if(regionString.contains("italy")) {
      regionPrios.prepend("it");
    }
    if(regionString.contains("spain")) {
      regionPrios.prepend("sp");
    }
    if(regionString.contains("china")) {
      regionPrios.prepend("cn");
    }
    if(regionString.contains("australia")) {
      regionPrios.prepend("au");
    }
    if(regionString.contains("sweden")) {
      regionPrios.prepend("se");
    }
    if(regionString.contains("canada")) {
      regionPrios.prepend("ca");
    }
    if(regionString.contains("netherlands")) {
      regionPrios.prepend("nl");
    }
    if(regionString.contains("denmark")) {
      regionPrios.prepend("dk");
    }
    if(regionString.contains("asia")) {
      regionPrios.prepend("asi");
    }
  }

  QStringList searchNames;
  if(config->searchName.isEmpty()) {
    if(pastTitle.isEmpty() || config->rescan) {
      searchNames = getSearchNames(info);
    } else {
      searchNames.append(pastTitle);
    }
  } else {
    // Add the string provided by "--query"
    searchNames.append(config->searchName);
  }

  if(searchNames.isEmpty()) {
    return;
  } else {
    // Apply negative cache suppresions:
    if(negDbReady && !offlineScraper) {
      QMutableListIterator<QString> it(searchNames);
      while(it.hasNext()) {
        QString current = it.next();
        QSqlQuery query(negDb);
        query.setForwardOnly(true);
        query.prepare("SELECT COUNT(*) FROM negativecache WHERE platform=:platform"
                      " AND scraper=:scraper AND (file=:fileempty OR file=:file)"
                      " AND failedname=:failedname AND date>:date");
        query.bindValue(":platform", config->platform);
        query.bindValue(":scraper", config->scraper);
        query.bindValue(":fileempty", "");
        query.bindValue(":file", originalInfo.absoluteFilePath());
        query.bindValue(":failedname", current);
        query.bindValue(":date", config->negCacheExpiration);
        if(!query.exec()) {
          qDebug() << query.lastError();
          printf("ERROR: Error accessing the negative database table.\n");
        } else {
          query.next();
          if(query.value(0).toInt() > 0) {
            printf("INFO: Negative cache hit, skipping '%s'\n",
                   current.toStdString().c_str());
            it.remove();
          }
        }
        query.finish();
      }
    }
  }
  if(searchNames.isEmpty()) {
    return;
  }

  for(int pass = 1; pass <= searchNames.size(); ++pass) {
    lastSearchName = searchNames.at(pass - 1);
    output.append("\033[1;35mPass " + QString::number(pass) + "\033[0m ");
    getSearchResults(gameEntries, lastSearchName, config->platform);
    debug.append("Tried with: '" + lastSearchName + "'\n");
    debug.append("Platform: " + config->platform + "\n");
    // Some online scrapers' search engines are broken and always returns results,
    // no matter the query... Adding them as exceptions here we try all the search names,
    // even if more than one returns results.
    // This trick of accumulating all the the passes even if a positive result was found
    // CANNOT be used with: ScreenScraper, ESGameList, CustomFlags, ArcadeDB, ImportScraper.
    if(!gameEntries.isEmpty() &&
       config->scraper != "worldofspectrum") { //&& config->scraper != "rawg") {
      break;
    } else {
      // Add to negative cache as universal "no-match" (except if there was an error
      // querying the scraper):
      addLastSearchToNegativeCache();
    }
  }
}

// Adds the last active search term used to the negative cache. If file is not empty,
// the negative cache is conditional to the indicated file. Otherwise it is universal.
void AbstractScraper::addLastSearchToNegativeCache(const QString &file)
{
  if(negDbReady && !offlineScraper && !searchError) {
    QSqlQuery query(negDb);
    query.prepare("INSERT INTO negativecache (platform, scraper, file, failedname, date)"
                  " VALUES (:platform, :scraper, :file, :failedname, :date)");
    query.bindValue(":platform", config->platform);
    query.bindValue(":scraper", config->scraper);
    query.bindValue(":file", file);
    query.bindValue(":failedname", lastSearchName);
    query.bindValue(":date", QDateTime::currentMSecsSinceEpoch() / 1000);
    if(!query.exec()) {
      printf("ERROR: Could not add failed match to negative cache database.\n");
      qDebug() << query.lastError();
    }
    query.finish();
  }
}

// Detects if found is a valid name for platform platform.
bool AbstractScraper::platformMatch(QString found, QString platform) {
  const auto platforms = Platform::get().getAliases(platform);
  for(const auto &p: std::as_const(platforms)) {
    if(found.toLower() == p.toLower()) {
      return true;
    }
  }
  return false;
}

// Returns the scraping service id for the platform.
QString AbstractScraper::getPlatformId(const QString &platform)
{
  auto it = platformToId.find(platform);
  if(it != platformToId.cend()) {
    return it.value();
  } else {
    return "na";
  }
}

void AbstractScraper::getOnlineVideo(QString videoUrl, GameEntry &game)
{
  game.videoData = "";
  if(!videoUrl.isEmpty()) {
    QString tempFile;
    {
      QTemporaryFile video;
      video.setAutoRemove(false);
      video.open();
      tempFile = video.fileName();
    }
    if(!tempFile.isEmpty()) {
      QProcess youtubeDL;
      QString command("yt-dlp");
      QStringList commandArgs;
      commandArgs << "--no-playlist"
                  << "-q"
                  << "--verbose"
                  << "--force-overwrites"
                  << "-f" << "mp4"
                  << "-o" << tempFile
                  << "--max-filesize" << QString::number(config->videoSizeLimit)
                  << "--download-sections" << "*-0:40--0:10"
                  << videoUrl;
      youtubeDL.start(command, commandArgs, QIODevice::ReadOnly);
      youtubeDL.waitForFinished(60000);
      // printf("%s\n", youtubeDL.readAllStandardError().toStdString().c_str());
      youtubeDL.close();
      QFile video(tempFile);
      if(video.open(QIODevice::ReadOnly)) {
        game.videoData = video.readAll();
        if(game.videoData.size() > 4096) {
          game.videoFormat = "mp4";
        }
        video.remove();
      }
      else {
        printf("ERROR: Error %d accessing the temporary file.\n", video.error());
        return;
      }
    }
    else {
      printf("ERROR: Error when creating a temporary file to download a video.\n");
    }
  }
}

void AbstractScraper::loadConfig(const QString &configPath,
                                 const QString &code, const QString &name)
{
  platformToId.clear();

  QFile configFile(configPath);
  if(!configFile.open(QIODevice::ReadOnly))
    return;

  QByteArray saveData = configFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty())
    return;

  QJsonArray platformsArray = json["platforms"].toArray();
  for(int platformIndex = 0; platformIndex < platformsArray.size(); ++platformIndex) {
    QJsonObject platformObject = platformsArray[platformIndex].toObject();

    QString platformName;
    if(platformObject[name].isDouble()) {
      platformName = QString::number(platformObject[name].toInt(-1));
    } else if(platformObject[name].isString()) {
      platformName = platformObject[name].toString();
    }

    QString platformIdn;
    if(platformObject[code].isDouble()) {
      platformIdn = QString::number(platformObject[code].toInt(-1));
    } else if(platformObject[code].isString()) {
      platformIdn = platformObject[code].toString();
    }

    if(!platformIdn.isEmpty() && platformIdn != "-1" &&
       !platformName.isEmpty() && platformName != "-1") {
      platformToId[platformIdn] = platformName;
    }
  }
}
