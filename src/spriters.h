/***************************************************************************
 *            spriters.h
 *
 *  Wed Jun 18 12:00:00 CEST 2017
 *  Copyright 2017 Lars Muldjord
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

#ifndef SPRITERS_H
#define SPRITERS_H

#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QEventLoop>

#include "abstractscraper.h"

class Spriters : public AbstractScraper
{
  Q_OBJECT

public:
  Spriters(Settings *config,
           QSharedPointer<NetManager> manager,
           QString threadId,
           NameTools *NameTool);
  void getGameData(GameEntry &game, QStringList &sharedBlobs, GameEntry *cache) override;

protected:
  void getSearchResults(QList<GameEntry> &gameEntries,
                        QString searchName, QString platform) override;
  void getSprites(GameEntry &game) override;

private:
  QTimer limitTimer;
  QEventLoop limiter;

  QString platformId;

  QMultiMap<QString, QPair<QString, QString>> nameToGameSame;
  QMultiMap<QString, QPair<QString, QString>> nameToGameTitleSame;
  QMultiMap<QString, QPair<QString, QString>> nameToGameOther;
  QMultiMap<QString, QPair<QString, QString>> nameToGameTitleOther;

};

#endif // SPRITERS_H
