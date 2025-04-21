/***************************************************************************
 *            worldofspectrum.h
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

#ifndef WORLDOFSPECTRUM_H
#define WORLDOFSPECTRUM_H

#include <QMap>

#include "abstractscraper.h"

class WorldOfSpectrum : public AbstractScraper
{
  Q_OBJECT

public:
  WorldOfSpectrum(Settings *config,
                  QSharedPointer<NetManager> manager,
                  QString threadId,
                  NameTools *NameTool);

protected:
  void getSearchResults(QList<GameEntry> &gameEntries,
                        QString searchName, QString platform) override;
  void getDescription(GameEntry &game) override;
  void getCover(GameEntry &game) override;
  void getScreenshot(GameEntry &game) override;
  void getMarquee(GameEntry &game) override;
  void getTexture(GameEntry &game) override;
  void getWheel(GameEntry &game) override;
  void getManual(GameEntry &game) override;
  void getVideo(GameEntry &game) override;
  void getReleaseDate(GameEntry &game) override;
  void getTags(GameEntry &game) override;

private:
  QMap <QString, QString> resources;

};

#endif // WORLDOFSPECTRUM_H
