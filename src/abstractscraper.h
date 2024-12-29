/***************************************************************************
 *            abstractscraper.h
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

#ifndef ABSTRACTSCRAPER_H
#define ABSTRACTSCRAPER_H

#include "netcomm.h"
#include "netmanager.h"
#include "gameentry.h"
#include "settings.h"

#include <QImage>
#include <QString>
#include <QEventLoop>
#include <QFileInfo>
#include <QSettings>
#include <QMultiMap>
#include <QStringList>
#include <QSqlDatabase>

class AbstractScraper : public QObject
{
  Q_OBJECT

public:
  AbstractScraper(Settings *config,
                  QSharedPointer<NetManager> manager,
                  QString threadId);
  virtual ~AbstractScraper();

  // Fill in the game skeleton with the data from the scraper service.
  virtual void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr);

  // Redirects the scraper to fetch each resource valid for the scraper.
  void fetchGameResources(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache = nullptr);

  // Applies alias/scummvm/amiga/mame filename to name conversion.
  QString applyNameMappings(const QString &fileName);

  // Applies alias/scummvm/amiga/mame filename to name conversion.
  // Applies getUrlQuery conversion to the outcome, including space to
  // the special character needed by the scraping service.
  // Adds three variations: without subtitles, converting the first numeral
  // to arabic or roman format, and both without subtitles and converting
  // the first numeral.
  // Returns the full list of up to four possibilities, that will be checked
  // in order against the scraping service until a match is found.
  virtual QStringList getSearchNames(const QFileInfo &info);

  // Generates the name for the game until a scraper provides an official one.
  // This is used to calculate the search names and calculate the best entry and
  // the distance to the official name.
  virtual QString getCompareTitle(QFileInfo info);

  // Tries to identify the region of the game from the filename. Calls getSearchNames and with
  // the outcome, executes the getSearchResults in sequence until a search name is successful.
  virtual void runPasses(QList<GameEntry> &gameEntries, const QFileInfo &searchInfo,
                         const QFileInfo &originalInfo, QString &output, QString &debug,
                         QString pastTitle = "");

  // Adds the last active term searched to the negative cache,
  // as the results were not successful.
  void addLastSearchToNegativeCache(const QString &file = "");

  int reqRemaining = -1;
  int reqRemainingKO = -1;

protected:
  // Queries the scraping service with searchName and generates a skeleton
  // game in gameEntries for each candidate returned.
  virtual void getSearchResults(QList<GameEntry> &gameEntries, QString searchName,
                                QString platform);

  // Executes the search in the generic multimaps that store the games database access
  // information for the offline scrapers (the ones for which the database ids are fully
  // accessible). Needs to be executed as part of the scraper overriden getSearchResults.
  template <typename T> bool getSearchResultsOffline(QList<T> &gameIds, const QString &searchName,
                                                     const QMultiMap<QString, T> &fullTitles,
                                                     const QMultiMap<QString, T> &mainTitles);

  void loadConfig(const QString &configPath, const QString &code, const QString &name);

  virtual void getTitle(GameEntry &game);
  virtual void getPlatform(GameEntry &game);
  virtual void getDescription(GameEntry &game);
  virtual void getDeveloper(GameEntry &game);
  virtual void getPublisher(GameEntry &game);
  virtual void getPlayers(GameEntry &game);
  virtual void getAges(GameEntry &game);
  virtual void getTags(GameEntry &game);
  virtual void getFranchises(GameEntry &game);
  virtual void getRating(GameEntry &game);
  virtual void getReleaseDate(GameEntry &game);
  virtual void getCover(GameEntry &game);
  virtual void getScreenshot(GameEntry &game);
  virtual void getWheel(GameEntry &game);
  virtual void getMarquee(GameEntry &game);
  virtual void getTexture(GameEntry &game);
  virtual void getVideo(GameEntry &game);
  virtual void getManual(GameEntry &game);
  virtual void getGuides(GameEntry &game);
  virtual void getCheats(GameEntry &game);
  virtual void getReviews(GameEntry &game);
  virtual void getArtbooks(GameEntry &game);
  virtual void getVGMaps(GameEntry &game);
  virtual void getTrivia(GameEntry &game);
  virtual void getChiptune(GameEntry &game);
  virtual void getCustomFlags(GameEntry &game);

  // Consume text in data until finding nom.
  virtual void nomNom(const QString nom, bool including = true);

  // Detects if found is a valid name for platform platform.
  virtual bool platformMatch(QString found, QString platform);

  // Returns the scraping service id for the platform.
  QString getPlatformId(const QString &platform);

  // Detects if nom is present in the non-consumed part of data.
  bool checkNom(const QString nom);

  void getOnlineVideo(QString videoUrl, GameEntry &game);

  Settings *config;
  bool offlineScraper;
  bool searchError = false;
  QList<int> fetchOrder;
  QMap<QString, QString> platformToId;
  QString threadId;

  QByteArray data;
  QString baseUrl;
  QString searchUrlPre;
  QString searchUrlPost;
  QString searchResultPre;
  QStringList urlPre;
  QString urlPost;
  QStringList titlePre;
  QString titlePost;
  QStringList platformPre;
  QString platformPost;
  QStringList descriptionPre;
  QString descriptionPost;
  QStringList developerPre;
  QString developerPost;
  QStringList publisherPre;
  QString publisherPost;
  QStringList playersPre;
  QString playersPost;
  QStringList agesPre;
  QString agesPost;
  QStringList tagsPre;
  QString tagsPost;
  QStringList franchisesPre;
  QString franchisesPost;
  QStringList ratingPre;
  QString ratingPost;
  QStringList releaseDatePre;
  QString releaseDatePost;
  QStringList coverPre;
  QString coverPost;
  QStringList screenshotPre;
  QString screenshotPost;
  QString screenshotCounter;
  QStringList wheelPre;
  QString wheelPost;
  QStringList marqueePre;
  QString marqueePost;
  QStringList texturePre;
  QString texturePost;
  QStringList videoPre;
  QString videoPost;
  QStringList manualPre;
  QString manualPost;
  QStringList guidesPre;
  QString guidesPost;
  QStringList cheatsPre;
  QString cheatsPost;
  QStringList reviewsPre;
  QString reviewsPost;
  QStringList artbooksPre;
  QString artbooksPost;
  QStringList vgmapsPre;
  QString vgmapsPost;
  QStringList triviaPre;
  QString triviaPost;

  // This is used when file names have a region in them. The original regionPrios is in Settings
  QStringList regionPrios;

  NetComm *netComm;
  QEventLoop q; // Event loop for use when waiting for data from NetComm.

private:
  bool negDbReady = false;
  QSqlDatabase negDb;
  QString dbNegCache = "negativecache.db";
  QString lastSearchName;

};

#endif // ABSTRACTSCRAPER_H
