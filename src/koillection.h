/***************************************************************************
 *            koillection.h
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

#ifndef KOILLECTION_H
#define KOILLECTION_H

#include <QSqlDatabase>
#include <QEventLoop>
#include <QJsonObject>
#include "netcomm.h"
#include "netmanager.h"
#include "abstractfrontend.h"

class Koillection : public AbstractFrontend
{
  Q_OBJECT

public:
  Koillection(QSharedPointer<NetManager> manager, Settings *config);
  ~Koillection();
  void assembleList(QString &finalOutput, QList<GameEntry> &gameEntries) override;
  bool skipExisting(QList<GameEntry> &gameEntries, QSharedPointer<Queue> queue) override;
  bool canSkip() override;
  bool loadOldGameList(const QString &gameListFileString) override;
  QString getGameListFileName() override;
  QString getInputFolder() override;
  QString getGameListFolder() override;
  QString getCoversFolder() override;
  QString getScreenshotsFolder() override;
  QString getWheelsFolder() override;
  QString getMarqueesFolder() override;
  QString getTexturesFolder() override;
  QString getVideosFolder() override;
  QString getManualsFolder() override;

private:
  bool fullMode = true;
  QByteArray data;
  QString koiToken;
  QString collectionId;
  QString baseUrl;
  QSqlDatabase db;
  NetComm *netComm;
  QEventLoop q;
  QJsonObject jsonObj;
  QList<QPair<QString, QString>> headers;
  QList<QPair<QString, QString>> headersPatch;
};

#endif // KOILLECTION_H
