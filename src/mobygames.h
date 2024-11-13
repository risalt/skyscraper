/***************************************************************************
 *            mobygames.h
 *
 *  Fri Mar 30 12:00:00 CEST 2018
 *  Copyright 2018 Lars Muldjord
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

#ifndef MOBYGAMES_H
#define MOBYGAMES_H

#include <QJsonDocument>
#include <QJsonObject>

#include "abstractscraper.h"

class MobyGames : public AbstractScraper
{
  Q_OBJECT

public:
  MobyGames(Settings *config, QSharedPointer<NetManager> manager);
  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;

protected:
  void getSearchResults(QList<GameEntry> &gameEntries,
                        QString searchName, QString platform) override;
  void getReleaseDate(GameEntry &game) override;
  void getPlayers(GameEntry &game) override;
  void getTags(GameEntry &game) override;
  void getDeveloper(GameEntry &game) override;
  void getPublisher(GameEntry &game) override;
  void getDescription(GameEntry &game) override;
  void getAges(GameEntry &game) override;
  void getRating(GameEntry &game) override;
  void getCover(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getWheel(GameEntry &game) override;
  void getScreenshot(GameEntry &game) override;
  void getManual(GameEntry &game) override;

private:
  QString getRegionShort(const QString &region);

  QTimer limitTimer;
  QEventLoop limiter;

  QString platformId;
  QJsonDocument jsonDoc;
  QJsonDocument jsonMedia;
  QJsonDocument jsonScreens;
  QJsonObject jsonObj;

};

#endif // MOBYGAMES_H
