/***************************************************************************
 *            exodos.h
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

#ifndef EXODOS_H
#define EXODOS_H

#include <QMap>
#include <QList>
#include <QString>
#include <QStringList>
#include <QSqlDatabase>

#include "abstractscraper.h"

constexpr int EXNULL = 0;
constexpr int EXGAME = 1;
constexpr int EXTITLE = 3;
constexpr int EXRELEASEDATE = 4;
constexpr int EXNOTES = 5;
constexpr int EXMAXPLAYERS = 6;
constexpr int EXID = 7;
constexpr int EXCOMMUNITYSTARRATING = 8;
constexpr int EXPLATFORM = 9;
constexpr int EXRATING = 10;
constexpr int EXGENRE = 11;
constexpr int EXDEVELOPER = 12;
constexpr int EXPUBLISHER = 13;
constexpr int EXROOTFOLDER = 14;
constexpr int EXAPPLICATIONPATH = 15;
constexpr int EXMANUALPATH = 16;
constexpr int EXMUSICPATH = 17;
constexpr int EXSERIES = 18;
constexpr int EXLAUNCHBOX = 19;
constexpr int EXVIDEOPATH = 20;

class ExoDos : public AbstractScraper
{
  Q_OBJECT

public:
  ExoDos(Settings *config,
         QSharedPointer<NetManager> manager,
         QString threadId,
         NameTools *NameTool);
  ~ExoDos();

  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;

protected:
  void getSearchResults(QList<GameEntry> &gameEntries,
                        QString searchName, QString platform) override;
  void getReleaseDate(GameEntry &game) override;
  void getDeveloper(GameEntry &game) override;
  void getPublisher(GameEntry &game) override;
  void getPlayers(GameEntry &game) override;
  void getAges(GameEntry &game) override;
  void getDescription(GameEntry &game) override;
  void getTags(GameEntry &game) override;
  void getFranchises(GameEntry &game) override;
  void getRating(GameEntry &game) override;

  void getCover(GameEntry &game) override;
  void getScreenshot(GameEntry &game) override;
  void getWheel(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getVideo(GameEntry &game) override;
  void getManual(GameEntry &game) override;

  void getChiptune(GameEntry &game) override;
  void getGuides(GameEntry &game) override;
  void getCheats(GameEntry &game) override;
  void getArtbooks(GameEntry &game) override;
  void getVGMaps(GameEntry &game) override;

private:
  void loadMaps();
  void getExtras(const QString &path, const QString &type, QString &files);
  void getImage(int type, QString &fileName,
                        const QString &gameName, const QString &platform);
  void fillMediaAsset(const QString &basePath, QString &file, QString &format,
                      const QString &pattern = ".*");

  QSqlDatabase navidrome;
  QMap<QString, QString> pathToId;
  QMap<QString, GameEntry> exoDosDb;
  QMap<QString, QStringList> extrasMapping;
  QStringList assetPaths;

  QMap<QString, int> schema;
  QMap<int, QStringList> priorityImages;

};

#endif // EXODOS_H
