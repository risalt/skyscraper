/***************************************************************************
 *            launchbox.h
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

#ifndef LAUNCHBOX_H
#define LAUNCHBOX_H

#include <QMap>
#include <QList>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <QEventLoop>
#include <QJsonObject>
#include <QJsonDocument>

#include "abstractscraper.h"
#include "gameentry.h"

constexpr int LBNULL = 0;
constexpr int LBGAME = 1;
constexpr int LBRELEASEYEAR = 2;
constexpr int LBNAME = 3;
constexpr int LBRELEASEDATE = 4;
constexpr int LBOVERVIEW = 5;
constexpr int LBMAXPLAYERS = 6;
constexpr int LBVIDEOURL = 7;
constexpr int LBDATABASEID = 8;
constexpr int LBCOMMUNITYRATING = 9;
constexpr int LBPLATFORM = 10;
constexpr int LBESRB = 11;
constexpr int LBGENRES = 12;
constexpr int LBDEVELOPER = 13;
constexpr int LBPUBLISHER = 14;
constexpr int LBGAMEALTERNATENAME = 15;
constexpr int LBALTERNATENAME = 16;
constexpr int LBREGION = 17;
constexpr int LBGAMEIMAGE = 18;
constexpr int LBFILENAME = 19;
constexpr int LBTYPE = 20;
constexpr int LBLAUNCHBOX = 21;
constexpr int LBMAMEFILE = 22;


class LaunchBox : public AbstractScraper
{
  Q_OBJECT

public:
  LaunchBox(Settings *config, QSharedPointer<NetManager> manager);
  ~LaunchBox();
  virtual QList<QString> getSearchNames(const QFileInfo &info) override;
  
private:
  QTimer limitTimer;
  QEventLoop limiter;

  void getSearchResults(QList<GameEntry> &gameEntries,
                        QString searchName, QString platform) override;
  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;
  void getReleaseDate(GameEntry &game) override;
  void getDeveloper(GameEntry &game) override;
  void getPublisher(GameEntry &game) override;
  void getPlayers(GameEntry &game) override;
  void getAges(GameEntry &game) override;
  void getDescription(GameEntry &game) override;
  void getTags(GameEntry &game) override;
  void getRating(GameEntry &game) override;

  void getCover(GameEntry &game) override;
  void getScreenshot(GameEntry &game) override;
  void getWheel(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getVideo(GameEntry &game) override;

  QJsonDocument jsonDoc;
  QJsonObject jsonObj;
  QMap<int, GameEntry> launchBoxDb;
  QMultiMap<QString, int> searchNameToId;
  
  void loadMaps();
  QMap<QString, QString> platformCodeToFinalCode;
  QMap<QString, QString> platformNameToFinalCode;
  QMap<QString, int> schema;
  QMap<int, QStringList> priorityImages;
  QStringList priorityRegions;
  QMap<QString, QString> mameNameToLongName;

};

#endif // LAUNCHBOX_H
