/***************************************************************************
 *            vggeek.h
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

#ifndef VGGEEK_H
#define VGGEEK_H

#include <QMap>
#include <QList>
#include <QTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QStringList>

#include "abstractscraper.h"

class VGGeek : public AbstractScraper
{
  Q_OBJECT

public:
  VGGeek(Settings *config,
         QSharedPointer<NetManager> manager,
         QString threadId,
         NameTools *NameTool);
  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;

protected:
  void getSearchResults(QList<GameEntry> &gameEntries, QString searchName,
                        QString platform) override;
  void getDescription(GameEntry &game) override;
  void getTags(GameEntry &game) override;
  void getRating(GameEntry &game) override;
  void getPlayers(GameEntry &game) override;
  void getAges(GameEntry &game) override;
  void getDeveloper(GameEntry &game) override;
  void getFranchises(GameEntry &game) override;
  void getPublisher(GameEntry &game) override;
  void getReleaseDate(GameEntry &game) override;
  void getCover(GameEntry &game) override; // box
  void getScreenshot(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getWheel(GameEntry &game) override;
  void getVideo(GameEntry &game) override;

private:
  QList<GameEntry> getVersions(GameEntry &game);

  QTimer limitTimer;
  QEventLoop limiter;

  QString platformId;
  QList<GameEntry> versions;
  QByteArray xmlData;
  QJsonArray imagesArray;

  QStringList screenshotCaptions;
  QStringList textureCaptions;
  QStringList wheelCaptions;
  QStringList marqueeCaptions;

};

#endif // VGGEEK_H
