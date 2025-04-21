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

#ifndef GAMEBASE_H
#define GAMEBASE_H

#include <QList>
#include <QStringList>
#include <QPair>
#include <QMultiMap>
#include <QSqlRecord>
#include <QSqlDatabase>

#include "abstractscraper.h"

class GameBase : public AbstractScraper {
  Q_OBJECT

public:
  GameBase(Settings *config,
           QSharedPointer<NetManager> manager,
           QString threadId,
           NameTools *NameTool);
  ~GameBase();

  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;

protected:
  void getSearchResults(QList<GameEntry> &gameEntries, QString searchName,
                        QString platform) override;
  void getReleaseDate(GameEntry &game) override;
  void getDescription(GameEntry &game) override;
  void getTags(GameEntry &game) override;
  void getPlayers(GameEntry &game) override;
  void getAges(GameEntry &game) override;
  void getPublisher(GameEntry &game) override;
  void getDeveloper(GameEntry &game) override;
  void getRating(GameEntry &game) override;
  void getCover(GameEntry &game) override;
  void getScreenshot(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getManual(GameEntry &game) override;
  void getGuides(GameEntry &game) override;
  void getCheats(GameEntry &game) override;
  void getVGMaps(GameEntry &game) override;
  void getArtbooks(GameEntry &game) override;
  void getChiptune(GameEntry &game) override;

private:
  void loadMaps();
  QString getFile(QString &fileName, const QString &type, const QString &dataName);
  QString loadImageData(const QString &subFolder, const QString &fileName);
  QString caseFixedPath(const QString &path);

  QSqlDatabase db;
  QSqlDatabase navidrome;
  QSqlRecord gameRecord;
  QList<QPair<QString, QString>> gameExtras;
  QMap<QString, QString> caseInsensitivePaths;
  QMap<QString, QList<QString>> extrasMapping;
  QString platformName;
  QMultiMap<QString, QPair<int, QString>> searchNameToId;
  QMultiMap<QString, QPair<int, QString>> searchNameToIdTitle;

};

#endif // GAMEBASE_H
