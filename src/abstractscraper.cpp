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

AbstractScraper::AbstractScraper(Settings *config,
                                 QSharedPointer<NetManager> manager)
  : config(config)
{
  incrementalScraping = false;
  netComm = new NetComm(manager);
  connect(netComm, &NetComm::dataReady, &q, &QEventLoop::quit);
}

AbstractScraper::~AbstractScraper()
{
  delete netComm;
}

bool AbstractScraper::supportsIncrementalScraping()
{
  return incrementalScraping;
}

// Queries the scraping service with searchName and generates a skeleton
// game in gameEntries for each candidate returned.
void AbstractScraper::getSearchResults(QList<GameEntry> &gameEntries,
                                       QString searchName, QString platform)
{
  netComm->request(searchUrlPre + searchName + searchUrlPost);
  q.exec();
  data = netComm->getData();

  GameEntry game;

  while(data.indexOf(searchResultPre.toUtf8()) != -1) {
    nomNom(searchResultPre);

    // Digest until url
    for(const auto &nom: urlPre) {
      nomNom(nom);
    }
    game.url = baseUrl + "/" + data.left(data.indexOf(urlPost.toUtf8()));

    // Digest until title
    for(const auto &nom: titlePre) {
      nomNom(nom);
    }
    game.title = data.left(data.indexOf(titlePost.toUtf8()));

    // Digest until platform
    for(const auto &nom: platformPre) {
      nomNom(nom);
    }
    game.platform = data.left(data.indexOf(platformPost.toUtf8()));

    if(platformMatch(game.platform, platform)) {
      gameEntries.append(game);
    }
  }
}

// Fill in the game skeleton with the data from the scraper service.
void AbstractScraper::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache)
{
  if (cache && !incrementalScraping) {
    printf("\033[1;31m This scraper does not support incremental scraping. Internal error!\033[0m\n\n");
    return;
  }

  netComm->request(game.url);
  q.exec();
  data = netComm->getData();
  //printf("URL IS: '%s'\n", game.url.toStdString().c_str());
  //printf("DATA IS:\n'%s'\n", data.data());

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
      getCover(game);
      break;
    case SCREENSHOT:
      getScreenshot(game);
      break;
    case WHEEL:
      getWheel(game);
      break;
    case MARQUEE:
      getMarquee(game);
      break;
    case TEXTURE:
      getTexture(game);
      break;
    case VIDEO:
      if((config->videos) && (!sharedBlobs.contains("video"))) {
        getVideo(game);
      }
      break;
    case MANUAL:
      if((config->manuals) && (!sharedBlobs.contains("manual"))) {
        getManual(game);
      }
      break;
    case CHIPTUNE:
      if(config->chiptunes) {
        getChiptune(game);
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

void AbstractScraper::getDescription(GameEntry &game)
{
  if(descriptionPre.isEmpty()) {
    return;
  }
  for(const auto &nom: descriptionPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: descriptionPre) {
    nomNom(nom);
  }

  game.description = data.left(data.indexOf(descriptionPost.toUtf8())).replace("&lt;", "<").replace("&gt;", ">");
  game.description = game.description.replace("\\n", "\n");

  // Remove all html tags within description
  game.description = StrTools::stripHtmlTags(game.description);
}

void AbstractScraper::getDeveloper(GameEntry &game)
{
  for(const auto &nom: developerPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: developerPre) {
    nomNom(nom);
  }
  game.developer = data.left(data.indexOf(developerPost.toUtf8()));
}

void AbstractScraper::getPublisher(GameEntry &game)
{
  if(publisherPre.isEmpty()) {
    return;
  }
  for(const auto &nom: publisherPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: publisherPre) {
    nomNom(nom);
  }
  game.publisher = data.left(data.indexOf(publisherPost.toUtf8()));
}

void AbstractScraper::getPlayers(GameEntry &game)
{
  if(playersPre.isEmpty()) {
    return;
  }
  for(const auto &nom: playersPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: playersPre) {
    nomNom(nom);
  }
  game.players = data.left(data.indexOf(playersPost.toUtf8()));
}

void AbstractScraper::getAges(GameEntry &game)
{
  if(agesPre.isEmpty()) {
    return;
  }
  for(const auto &nom: agesPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: agesPre) {
    nomNom(nom);
  }
  game.ages = data.left(data.indexOf(agesPost.toUtf8()));
}

void AbstractScraper::getTags(GameEntry &game)
{
  if(tagsPre.isEmpty()) {
    return;
  }
  for(const auto &nom: tagsPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: tagsPre) {
    nomNom(nom);
  }
  game.tags = data.left(data.indexOf(tagsPost.toUtf8()));
}

void AbstractScraper::getFranchises(GameEntry &game)
{
  if(franchisesPre.isEmpty()) {
    return;
  }
  for(const auto &nom: franchisesPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: franchisesPre) {
    nomNom(nom);
  }
  game.franchises = data.left(data.indexOf(franchisesPost.toUtf8()));
}

void AbstractScraper::getRating(GameEntry &game)
{
  if(ratingPre.isEmpty()) {
    return;
  }
  for(const auto &nom: ratingPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: ratingPre) {
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
  for(const auto &nom: releaseDatePre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: releaseDatePre) {
    nomNom(nom);
  }
  game.releaseDate = data.left(data.indexOf(releaseDatePost.toUtf8())).simplified();
}

void AbstractScraper::getCover(GameEntry &game)
{
  if(coverPre.isEmpty()) {
    return;
  }
  for(const auto &nom: coverPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: coverPre) {
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
      for(const auto &nom: screenshotPre) {
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
  for(const auto &nom: wheelPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: wheelPre) {
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
  for(const auto &nom: marqueePre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: marqueePre) {
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
  if (texturePre.isEmpty()) {
    return;
  }

  for (const auto &nom : texturePre) {
    if (!checkNom(nom)) {
      return;
    }
  }

  for (const auto &nom : texturePre) {
    nomNom(nom);
  }

  QString textureUrl =
      data.left(data.indexOf(texturePost.toUtf8())).replace("&amp;", "&");
  if (textureUrl.left(4) != "http") {
    textureUrl.prepend(baseUrl + (textureUrl.left(1) == "/" ? "" : "/"));
  }
  netComm->request(textureUrl);
  q.exec();
  QImage image;
  if (netComm->getError() == QNetworkReply::NoError &&
      image.loadFromData(netComm->getData())) {
    game.textureData = netComm->getData();
  }
}

void AbstractScraper::getVideo(GameEntry &game)
{
  if(videoPre.isEmpty()) {
    return;
  }
  for(const auto &nom: videoPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: videoPre) {
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
  for(const auto &nom: manualPre) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: manualPre) {
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

void AbstractScraper::getCustomFlags(GameEntry &)
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
// Applies getUrlQuery conversion to the outcome, including space to
// the special character needed by the scraping service.
// Adds three variations: without subtitles, converting the first numeral
// to arabic or roman format, and both without subtitles and converting
// the first numeral.
// Returns the full list of up to four possibilities, that will be checked
// in order against the scraping service until a match is found.
QList<QString> AbstractScraper::getSearchNames(const QFileInfo &info)
{
  QString baseName = info.completeBaseName();

  if(config->scraper != "import") {
    if(!config->aliasMap[baseName].isEmpty()) {
      baseName = config->aliasMap[baseName];
    } else if(Platform::get().getFamily(config->platform) == "amiga") {
      QString nameWithSpaces = config->whdLoadMap[baseName].first;
      if(nameWithSpaces.isEmpty()) {
        baseName = NameTools::getNameWithSpaces(baseName);
      } else {
        baseName = nameWithSpaces;
      }
    } else if(config->platform == "scummvm") {
      baseName = NameTools::getScummName(baseName, config->scummIni);
    } else if(Platform::get().getFamily(config->platform) == "arcade" &&
              !config->mameMap[baseName.toLower()].isEmpty()) {
      QString original = baseName;
      baseName = config->mameMap[baseName.toLower()];
      printf("INFO: 1 Short Mame-style name '%s' converted to '%s'.\n",
             original.toStdString().c_str(),
             baseName.toStdString().c_str());
    }
  }

  QList<QString> searchNames;

  if(baseName.isEmpty())
    return searchNames;

  QString separator = "+";
  if (config->scraper == "igdb" || config->scraper == "screenscraper") {
    separator = " ";
  }

  searchNames.append(NameTools::getUrlQueryName(baseName, -1, separator));

  bool hasSubtitle;
  if(!config->keepSubtitle) {
    QString noSubtitle = NameTools::removeSubtitle(baseName, hasSubtitle);
    if(hasSubtitle) {
      searchNames.append(NameTools::getUrlQueryName(noSubtitle, -1, separator));
    }
  }

  if(NameTools::hasRomanNumeral(baseName) || NameTools::hasIntegerNumeral(baseName)) {
    if(NameTools::hasRomanNumeral(baseName)) {
      baseName = NameTools::convertToIntegerNumeral(baseName);
    } else if(NameTools::hasIntegerNumeral(baseName)) {
      baseName = NameTools::convertToRomanNumeral(baseName);
    }
    searchNames.append(NameTools::getUrlQueryName(baseName, -1, separator));

    if(!config->keepSubtitle) {
      QString noSubtitle = NameTools::removeSubtitle(baseName, hasSubtitle);
      if(hasSubtitle) {
        searchNames.append(NameTools::getUrlQueryName(noSubtitle, -1, separator));
      }
    }
  }

  return searchNames;
}

// Generates the name for the game until a scraper provides an official one.
// This is used to calculate the search names and calculate the best entry and
// the distance to the official name.
QString AbstractScraper::getCompareTitle(QFileInfo info)
{
  QString baseName = info.completeBaseName();

  if(config->scraper != "import") {
    if(!config->aliasMap[baseName].isEmpty()) {
      baseName = config->aliasMap[baseName];
    } else if(Platform::get().getFamily(config->platform) == "amiga") {
      QString nameWithSpaces = config->whdLoadMap[baseName].first;
      if(nameWithSpaces.isEmpty()) {
        baseName = NameTools::getNameWithSpaces(baseName);
      } else {
        baseName = nameWithSpaces;
      }
    } else if(config->platform == "scummvm") {
      baseName = NameTools::getScummName(baseName, config->scummIni);
    } else if(Platform::get().getFamily(config->platform) == "arcade" &&
              !config->mameMap[baseName].isEmpty()) {
      baseName = config->mameMap[baseName];
    }
  }

  // Now create actual compareTitle
  baseName = baseName.replace("_", " ").left(baseName.indexOf("(")).left(baseName.indexOf("[")).simplified();

  // Always move ", The" to the beginning of the name
  // Three digit articles in English, German, French, Spanish:
  baseName = NameTools::moveArticle(baseName, true);

  // Remove "vX.XXX" versioning string if one is found
  QRegularExpressionMatch match;
  match = QRegularExpression(" v[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4}|[IVX]{1,5})$").match(baseName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    baseName = baseName.left(match.capturedStart(0)).simplified();
  }
    
  return baseName;
}

// Tries to identify the region of the game from the filename. Calls getSearchNames and with
// the outcome, executes the getSearchResults in sequence until a search name is successful.
void AbstractScraper::runPasses(QList<GameEntry> &gameEntries, const QFileInfo &info, QString &output, QString &debug)
{
  // Reset region priorities to original list from Settings
  regionPrios = config->regionPrios;
  // Autodetect region and append to region priorities
  if((info.fileName().indexOf("(") != -1 || info.fileName().indexOf("[") != -1 ) && config->region.isEmpty()) {
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

  QList<QString> searchNames;
  if(config->searchName.isEmpty()) {
    searchNames = getSearchNames(info);
  } else {
    // Add the string provided by "--query"
    searchNames.append(config->searchName);
  }

  if(searchNames.isEmpty()) {
    return;
  }
  for(int pass = 1; pass <= searchNames.size(); ++pass) {
    output.append("\033[1;35mPass " + QString::number(pass) + "\033[0m ");
    getSearchResults(gameEntries, searchNames.at(pass - 1), config->platform);
    debug.append("Tried with: '" + searchNames.at(pass - 1) + "'\n");
    debug.append("Platform: " + config->platform + "\n");
    if(!gameEntries.isEmpty()) {
      break;
    }
  }
}

// Detects if found is a valid name for platform platform.
bool AbstractScraper::platformMatch(QString found, QString platform) {
  for(const auto &p: Platform::get().getAliases(platform)) {
    if(found.toLower() == p.toLower()) {
      return true;
    }
  }
  return false;
}

// Returns the scraping service id for the platform.
QString AbstractScraper::getPlatformId(const QString)
{
  return "na";
}

void AbstractScraper::getOnlineVideo(QString videoUrl, GameEntry &game)
{
  game.videoData = "";
  if (!videoUrl.isEmpty()) {
    QString tempFile;
    {
      QTemporaryFile video;
      video.setAutoRemove(false);
      video.open();
      tempFile = video.fileName();
    }
    if (!tempFile.isEmpty()) {
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
      if (video.open(QIODevice::ReadOnly)) {
        game.videoData = video.readAll();
        if (game.videoData.size() > 4096) {
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
