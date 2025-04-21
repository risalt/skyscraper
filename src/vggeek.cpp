/***************************************************************************
 *            vggeek.cpp
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

#include "vggeek.h"
#include "nametools.h"
#include "strtools.h"
#include "platform.h"
#include "skyscraper.h"

#include <QFile>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>

VGGeek::VGGeek(Settings *config,
               QSharedPointer<NetManager> manager,
               QString threadId,
               NameTools *NameTool)
  : AbstractScraper(config, manager, threadId, NameTool)
{
  loadConfig("vggeek.json", "code", "id");

  platformId = getPlatformId(config->platform);
  if(Platform::get().getFamily(config->platform) == "arcade" &&
     platformId == "na") {
    platformId = getPlatformId("arcade");
  }
  if(platformId == "na") {
    reqRemaining = 0;
    printf("\033[0;31mPlatform not supported by VGGeek (see file vggeek.json)...\033[0m\n");
    return;
  }

  connect(&limitTimer, &QTimer::timeout, &limiter, &QEventLoop::quit);
  limitTimer.setInterval(5000); // Clearly excessive...
  limitTimer.setSingleShot(false);
  limitTimer.start();

  baseUrl = "https://videogamegeek.com";

  searchUrlPre = "https://videogamegeek.com/geeksearch.php?objecttype=videogame"
                 "&advsearch=1&action=search&q=";
  searchUrlPost = "&familyids[]=";

  urlPre.append("<td class='collection_thumbnail'>");
  urlPre.append("<a href=\"");
  urlPost = "\"";
  titlePre.append("class='primary' >");
  titlePost = "</a>";

  agesPre.append("<a href=\"/");
  agesPost = "\" target='_parent'>";
  franchisesPre.append("<b>Franchises</b>");
  franchisesPost = "</tr>";
  platformPre.append("#aaaaaa'>");
  platformPost = "</span>";
  descriptionPre.append("<!-- google_ad_section_start -->");
  descriptionPost = "<!-- google_ad_section_end -->";
  developerPre.append("<b>Developer</b>");
  developerPre.append("<a  href=\"");
  developerPre.append(">");
  developerPost = "</a>";
  coverPre.append("<image>");
  coverPost = "</image>";
  videoPre.append("<videos total");
  videoPre.append(">");
  videoPost = "</videos>";
  playersPre.append("<div id='results_maxplayers");
  playersPre.append(">");
  playersPost = "</div>";
  publisherPre.append("<b>Publisher</b>");
  publisherPre.append("<a  href=\"");
  publisherPre.append(">");
  publisherPost = "</a>";
  releaseDatePre.append("<div id='results_releasedate");
  releaseDatePre.append(">");
  releaseDatePost = "</div>";
  tagsPre.append("<b>Genre</b>");
  tagsPost = "</tr>";
  ratingPre.append("Average Rating: <img");
  ratingPre.append("<span property='v:average'>");
  ratingPost = "</span>";

  // Gameplay screen
  screenshotCaptions << "gameplay" << "example of play" << "battle";
  // Back cover
  textureCaptions << "back cover" << "back of case" << "box back" << "back box" << 
                     "back of box" << "case back" << "rear cover";
  // Title screen
  wheelCaptions << "title screen" << "intro" << "loading screen" << "start menu" <<
                   "menu screen" << "loading screen";
  // Artwork/fanart/media
  marqueeCaptions << "inlay" << "fanart" << "promotional image" << "media" <<
                     "cartridge";

  fetchOrder.append(ID);
  fetchOrder.append(TITLE);
  fetchOrder.append(PLATFORM);
  fetchOrder.append(AGES);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(DEVELOPER);
  fetchOrder.append(FRANCHISES);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(RATING);
  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(TAGS);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(MARQUEE);
  fetchOrder.append(TEXTURE);
  fetchOrder.append(WHEEL);
  fetchOrder.append(VIDEO);
}

void VGGeek::getSearchResults(QList<GameEntry> &gameEntries,
                              QString searchName, QString platformCode)
{
  if(searchName.contains("(") || searchName.contains("[")) {
    return;
  }

  QStringList platforms = platformId.split(',');
  for(const auto &platform: std::as_const(platforms)) {

    GameEntry game;
    QString url = searchUrlPre + searchName + searchUrlPost + platform;
    limiter.exec();
    netComm->request(url);
    q.exec();

    while(!netComm->getRedirUrl().isEmpty()) {
      game.url = netComm->getRedirUrl();
      netComm->request(game.url);
      q.exec();
    }

    if(netComm->getError() != QNetworkReply::NoError) {
      qDebug() << netComm->getError();
      printf("Connection error. Is the web down?\n");
      reqRemaining = 0;
      searchError = true;
      return;
    }

    data = netComm->getData();
    if(data.isEmpty() ||
       data.contains("Error: 500 Internal Server Error")) {
      printf("Unexpected reply. Is the service down?\n");
      reqRemaining = 0;
      searchError = true;
      return;
    }
    if(data.contains("Error: 404 Not Found")) {
      continue;
    }

    data = data.replace(QString(QChar(0x200B)).toUtf8(), "").simplified();
    while(data.indexOf(urlPre[0].toUtf8()) != -1) {
      // Digest until url
      for(const auto &nom: std::as_const(urlPre)) {
        nomNom(nom);
      }
      game.id = data.left(data.indexOf(urlPost.toUtf8()));
      game.url = baseUrl + game.id;
      game.id.replace("/videogame/", "");
      game.id.replace("/videogameexpansion/", "");
      game.id.replace("/videogamecompilation/", "");
      game.id = game.id.left(game.id.indexOf("/"));

      // Digest until title
      for(const auto &nom: std::as_const(titlePre)) {
        nomNom(nom);
      }
      game.title = data.left(data.indexOf(titlePost.toUtf8())).simplified();

      game.platform = Platform::get().getAliases(platformCode).at(1);

      gameEntries.append(game);
    }
  }
}

void VGGeek::getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr)
{
  QFile htmlDumpFile(config->dbPath + "/" + game.id + ".html");
  if(htmlDumpFile.exists() && htmlDumpFile.open(QIODevice::ReadOnly | QFile::Text)) {
    QTextStream htmlDump(&htmlDumpFile);
    game.miscData = htmlDump.readAll().toUtf8();
  } else {
    if(!game.url.isEmpty()) {
      limiter.exec();
      netComm->request(game.url);
      q.exec();
      game.miscData = netComm->getData();
      if(htmlDumpFile.open(QIODevice::WriteOnly | QFile::Text | QFile::Truncate)) {
        QTextStream htmlDump(&htmlDumpFile);
        htmlDump << game.miscData;
      }
    } else {
      game.found = false;
      return;
    }
  }
  htmlDumpFile.close();

  QList<GameEntry> sortedVersions = getVersions(game);
  QStringList gfRegions;
  for(const auto &region: std::as_const(regionPrios)) {
    if(region == "eu" || region == "fr" || region == "de" || region == "it" ||
       region == "sp" || region == "se" || region == "nl" || region == "dk" ||
       region == "gr" || region == "no" || region == "ru" || region == "au" ||
       region == "uk" || region == "nz") {
      gfRegions.append("Europe");
    } else if(region == "us" || region == "ca") {
      gfRegions.append("USA");
      gfRegions.append("Americas");
    } else if(region == "wor") {
      gfRegions.append("World");
    } else if(region == "jp") {
      gfRegions.append("Japan");
    } else if(region == "cn") {
      gfRegions.append("China");
    } else if(region == "asi" || region == "tw" || region == "kr") {
      gfRegions.append("Asia");
    } else if(region == "ame" || region == "br") {
      gfRegions.append("Americas");
    } else {
      gfRegions.append("World");
    }
  }
  gfRegions.append("World");
  gfRegions.removeDuplicates();
  versions.clear();
  for(const auto &country: std::as_const(gfRegions)) {
    for(const auto &version: std::as_const(sortedVersions)) {
      if(version.source == country) {
        versions.append(version);
      }
    }
  }
  if(config->verbosity > 4) {
    qDebug() << "Extracted versions: " << versions;
  }

  QFile xmlDumpFile(config->dbPath + "/" + game.id + ".xml");
  if(xmlDumpFile.exists() && xmlDumpFile.open(QIODevice::ReadOnly | QFile::Text)) {
    QTextStream xmlDump(&xmlDumpFile);
    xmlData = xmlDump.readAll().toUtf8();
  } else {
    QString url = "https://api.geekdo.com/xmlapi2/thing?id=" + game.id +
                  "&type=videogame&versions=1&videos=1&images=1&stats=1&pagesize=100";
    netComm->request(url);
    q.exec();
    xmlData = netComm->getData();
    if(xmlDumpFile.open(QIODevice::WriteOnly | QFile::Text | QFile::Truncate)) {
      QTextStream xmlDump(&xmlDumpFile);
      xmlDump << xmlData;
    }
  }
  xmlDumpFile.close();

  QByteArray jsonData;
  QFile jsonDumpFile(config->dbPath + "/" + game.id + ".json");
  if(jsonDumpFile.exists() && jsonDumpFile.open(QIODevice::ReadOnly | QFile::Text)) {
    QTextStream jsonDump(&jsonDumpFile);
    jsonData = jsonDump.readAll().toUtf8();
  } else {
    QString url = "https://api.geekdo.com/api/images?ajax=1&date=alltime&"
                  "gallery=all&nosession=1&objectid=" + game.id +
                  "&objecttype=thing&pageid=1&showcount=100&size=thumb&sort=hot";
    netComm->request(url);
    q.exec();
    jsonData = netComm->getData();
    if(jsonDumpFile.open(QIODevice::WriteOnly | QFile::Text | QFile::Truncate)) {
      QTextStream jsonDump(&jsonDumpFile);
      jsonDump << jsonData;
    }
  }
  jsonDumpFile.close();
  if(jsonData.contains("\"images\"")) {
    imagesArray = QJsonDocument::fromJson(jsonData).object()["images"].toArray();
  } else {
    imagesArray = QJsonArray();
  }

  fetchGameResources(game, sharedBlobs, cache);
}

QList<GameEntry> VGGeek::getVersions(GameEntry &game)
{
  data = game.miscData;
  QString versionPre = "<td id='edit_linkednameid'";
  QString versionPost = "<a href=\"/item/correction/videogameversion";
  QList<GameEntry> versions;

  while(data.indexOf(versionPre.toUtf8()) != -1) {
    GameEntry version;
    nomNom(versionPre);
    QByteArray versionData = data.left(data.indexOf(versionPost.toUtf8()));
    QByteArray remainingVersions = data;
    data = versionData;
    nomNom("Nick:&nbsp;");
    nomNom("<div");
    nomNom("<div");
    nomNom(">");
    version.parNotes = QString(data.left(data.indexOf("</div>"))).trimmed();
    nomNom("Rel Date:&nbsp;<span");
    nomNom(">");
    version.releaseDate = QString(data.left(data.indexOf("</span>"))).replace("-00", "");
    version.releaseDate = StrTools::conformReleaseDate(version.releaseDate);
    nomNom("Publisher:&nbsp;");
    version.publisher = QString(data.left(data.indexOf("</div>")));
    nomNom("<div id='results__version");
    nomNom(">");
    version.platform = QString(data.left(data.indexOf("</div>")));
    nomNom("Developer:&nbsp;");
    version.developer = QString(data.left(data.indexOf("</div>")));
    nomNom("Region:&nbsp;");
    version.sqrNotes = QString(data.left(data.indexOf("</div>")));
    nomNom("Rating:&nbsp;");
    version.ages = QString(data.left(data.indexOf("</div>")));
    QStringList ageRatings = version.ages.split(",&ensp;");
    for(const auto &ageRating: std::as_const(ageRatings)) {
      version.ages = StrTools::conformAges(ageRating.split(": ").last());
      if(!version.ages.isEmpty()) {
        break;
      }
    }
    nomNom("<a href=\"/videogameversion/");
    version.id = QString(data.left(data.indexOf("/")));
    data = remainingVersions;
    if(platformMatch(version.platform, config->platform)) {
      if(version.sqrNotes.isEmpty()) {
        if(version.parNotes.isEmpty()) {
          version.source = "World";
        } else {
          if(version.parNotes.contains("European") ||
             version.parNotes.contains("Australian") ||
             version.parNotes.contains("UK") ||
             version.parNotes.contains("German") ||
             version.parNotes.contains("French") ||
             version.parNotes.contains("Dutch") ||
             version.parNotes.contains("Ital") ||
             version.parNotes.contains("EU") ||
             version.parNotes.contains("Spanish") ||
             version.parNotes.contains("Russian") ||
             version.parNotes.contains("Scandinavian") ||
             version.parNotes.contains("SWE")) {
            version.source = "Europe";
          } else if(version.parNotes.contains("North American") ||
                    version.parNotes.contains("US") ||
                    version.parNotes.contains("NA")) {
            version.source = "USA";
          } else if(version.parNotes.contains("Japanese")) {
            version.source = "Japan";
          } else if(version.parNotes.contains("Korean") ||
                    version.parNotes.contains("Asian")) {
            version.source = "Asian";
          } else if(version.parNotes.contains("Chinese")) {
            version.source = "China";
          } else {
            version.source = "World";
          }
        }
      } else {
        if(version.sqrNotes.contains("Europe") ||
           version.sqrNotes.contains("Region 2")) {
          version.source = "Europe";
        } else if(version.sqrNotes.contains("North America") ||
                  version.sqrNotes.contains("Region 1")) {
          version.source = "USA";
        } else if(version.sqrNotes.contains("Asia") ||
                  version.sqrNotes.contains("Region 3")) {
          version.source = "Asia";
        } else if(version.sqrNotes.contains("China") ||
                  version.sqrNotes.contains("Region 6")) {
          version.source = "China";
        } else if(version.sqrNotes.contains("Japan")) {
          version.source = "Japan";
        } else if(version.sqrNotes.contains("Region 4")) {
          version.source = "Americas";
        } else {
          version.source = "World";
        }
      }
      versions.append(version);
    }
  }
  return versions;
}

void VGGeek::getDeveloper(GameEntry &game)
{
  for(const auto &version: std::as_const(versions)) {
    if(!version.developer.isEmpty()) {
      game.developer = version.developer;
      break;
    }
  }
  if(game.developer.isEmpty()) {
    data = game.miscData;
    AbstractScraper::getDeveloper(game);
    game.developer = game.developer.simplified();
  }
}

void VGGeek::getPublisher(GameEntry &game)
{
  for(const auto &version: std::as_const(versions)) {
    if(!version.publisher.isEmpty()) {
      game.publisher = version.publisher;
      break;
    }
  }
  if(game.publisher.isEmpty()) {
    data = game.miscData;
    AbstractScraper::getPublisher(game);
    game.publisher = game.publisher.simplified();
  }
}

void VGGeek::getPlayers(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getPlayers(game);
}

void VGGeek::getReleaseDate(GameEntry &game)
{
  for(const auto &version: std::as_const(versions)) {
    if(!version.releaseDate.isEmpty()) {
      game.releaseDate = version.releaseDate;
      break;
    }
  }
  if(game.releaseDate.isEmpty()) {
    data = game.miscData;
    data.replace("-00", "");
    AbstractScraper::getReleaseDate(game);
  }
}

void VGGeek::getTags(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getTags(game);
  QStringList firstPassPre = tagsPre;
  QString firstPassPost = tagsPost;
  QString firstPassTags = game.tags;
  data = game.miscData;
  tagsPre = QStringList("<b>Theme</b>");
  AbstractScraper::getTags(game);
  data = firstPassTags.toUtf8() + game.tags.toUtf8();
  game.tags.clear();
  tagsPre.clear();
  tagsPre.append("<a href=\"/");
  tagsPre.append(">");
  tagsPost = "</a>";
  while(data.indexOf(tagsPre[0].toUtf8()) != -1) {
    for(const auto &nom: std::as_const(tagsPre)) {
      nomNom(nom);
    }
    game.tags = game.tags + "," + data.left(data.indexOf(tagsPost.toUtf8()));
  }
  game.tags = StrTools::conformTags(game.tags);
  tagsPre = firstPassPre;
  tagsPost = firstPassPost;
}

void VGGeek::getFranchises(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getFranchises(game);
  QStringList firstPassPre = franchisesPre;
  QString firstPassPost = franchisesPost;
  QString firstPassFranchises = game.franchises;
  data = game.miscData;
  franchisesPre = QStringList("<b>Series</b>");
  AbstractScraper::getFranchises(game);
  data = firstPassFranchises.toUtf8() + game.franchises.toUtf8();
  game.franchises.clear();
  franchisesPre.clear();
  franchisesPre.append("<a href=\"/");
  franchisesPre.append(">");
  franchisesPost = "</a>";
  while(data.indexOf(franchisesPre[0].toUtf8()) != -1) {
    for(const auto &nom: std::as_const(franchisesPre)) {
      nomNom(nom);
    }
    game.franchises = game.franchises + "," + data.left(data.indexOf(franchisesPost.toUtf8()));
  }
  game.franchises = StrTools::conformTags(game.franchises);
  franchisesPre = firstPassPre;
  franchisesPost = firstPassPost;
}

void VGGeek::getDescription(GameEntry &game)
{
  data = game.miscData;
  AbstractScraper::getDescription(game);
  AbstractScraper::getDescription(game);
  QString description = game.description.trimmed();
  if(description.contains("This page does not exist.")) {
    description.clear();
  }
  AbstractScraper::getDescription(game);
  if(game.description.contains("This page does not exist.")) {
    game.description.clear();
  }
  if(!description.isEmpty()) {
    game.description = description + '\n' + game.description.trimmed();
  } else if(!game.description.isEmpty()) {
    game.description = game.description.trimmed();
  }
}

void VGGeek::getRating(GameEntry &game)
{
  bool toDoubleOk = false;
  data = game.miscData;
  AbstractScraper::getRating(game);
  double rating = game.rating.toDouble(&toDoubleOk);
  if(toDoubleOk && rating != 0.0) {
    game.rating = QString::number(rating / 2.0);
  } else {
    game.rating = "";
  }
}

void VGGeek::getAges(GameEntry &game)
{
  for(const auto &version: std::as_const(versions)) {
    if(!version.ages.isEmpty()) {
      game.ages = version.ages;
      break;
    }
  }
}

void VGGeek::getCover(GameEntry &game)
{
  data = xmlData;
  AbstractScraper::getCover(game);
}

void VGGeek::getScreenshot(GameEntry &game)
{
  QString url;
  bool found = false;
  for(const auto &image: std::as_const(imagesArray)) {
    for(const auto &caption: std::as_const(screenshotCaptions)) {
      if(image.toObject()["caption"].toString().toLower().contains(caption)) {
        url = image.toObject()["imageurl_lg"].toString();
        found = true;
        break;
      }
    }
    if(found) {
      break;
    }
  }
  if(!url.isEmpty()) {
    limiter.exec();
    netComm->request(url);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.screenshotData = netComm->getData();
    }
  }
}

void VGGeek::getMarquee(GameEntry &game)
{
  QString url;
  bool found = false;
  for(const auto &image: std::as_const(imagesArray)) {
    for(const auto &caption: std::as_const(marqueeCaptions)) {
      if(image.toObject()["caption"].toString().toLower().contains(caption)) {
        url = image.toObject()["imageurl_lg"].toString();
        found = true;
        break;
      }
    }
    if(found) {
      break;
    }
  }
  if(!url.isEmpty()) {
    limiter.exec();
    netComm->request(url);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.marqueeData = netComm->getData();
    }
  }
}

void VGGeek::getTexture(GameEntry &game)
{
  QString url;
  bool found = false;
  for(const auto &image: std::as_const(imagesArray)) {
    for(const auto &caption: std::as_const(textureCaptions)) {
      if(image.toObject()["caption"].toString().toLower().contains(caption)) {
        url = image.toObject()["imageurl_lg"].toString();
        found = true;
        break;
      }
    }
    if(found) {
      break;
    }
  }
  if(!url.isEmpty()) {
    limiter.exec();
    netComm->request(url);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.textureData = netComm->getData();
    }
  }
}

void VGGeek::getWheel(GameEntry &game)
{
  QString url;
  bool found = false;
  for(const auto &image: std::as_const(imagesArray)) {
    for(const auto &caption: std::as_const(wheelCaptions)) {
      if(image.toObject()["caption"].toString().toLower().contains(caption)) {
        url = image.toObject()["imageurl_lg"].toString();
        found = true;
        break;
      }
    }
    if(found) {
      break;
    }
  }
  if(!url.isEmpty()) {
    limiter.exec();
    netComm->request(url);
    q.exec();
    QImage image;
    if(netComm->getError() == QNetworkReply::NoError &&
       image.loadFromData(netComm->getData())) {
      game.wheelData = netComm->getData();
    }
  }
}

void VGGeek::getVideo(GameEntry &game)
{
  QString url;
  data = xmlData;
  
  for(const auto &nom: std::as_const(videoPre)) {
    if(!checkNom(nom)) {
      return;
    }
  }
  for(const auto &nom: std::as_const(videoPre)) {
    nomNom(nom);
  }
  QString videoSection = data.left(data.indexOf(videoPost.toUtf8())).replace("&amp;", "&");
  QStringList videoUrls = videoSection.split("/>", Qt::SkipEmptyParts);
  for(const auto &videoUrl: std::as_const(videoUrls)) {
    if(videoUrl.contains("category=\"session\"")) {
      url = videoUrl;
      url = url.remove(QRegularExpression("<video id.*link=\""));
      url = url.left(url.indexOf("\""));
    }
  }
  getOnlineVideo(url, game);
}
