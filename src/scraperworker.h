/***************************************************************************
 *            scraperworker.h
 *
 *  Wed Jun 7 12:00:00 CEST 2017
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

#ifndef SCRAPERWORKER_H
#define SCRAPERWORKER_H

#include "abstractscraper.h"
#include "settings.h"
#include "cache.h"
#include "queue.h"
#include "nametools.h"
#include "netmanager.h"

class ScraperWorker : public QObject
{
  Q_OBJECT

public:
  ScraperWorker(QSharedPointer<Queue> queue,
                QSharedPointer<Cache> cache,
                QSharedPointer<NetManager> manager,
                Settings config,
                QString threadId);
  ~ScraperWorker();

  // Instantiates the actual scraper and processes files from the list of files.
  // Protected by mutex as there can be several scraper workers in parallel.
  void run();

  // Calculate a percentage of match between two strings, taking into account their length
  // and the Levenshtein-Damerau distance.
  int getSearchMatch(const QString &title, const QString &compareTitle,
                     const int lowestDistance, const int stringSize);

  // Returns the entry from gameEntries with the lowest distance to compareTitle,
  // and that lowest distance.
  GameEntry getBestEntry(const QList<GameEntry> &gameEntries, QString compareTitle,
                         int &lowestDistance, int &stringSize);

  bool forceEnd = false;

signals:
  void allDone(const bool &stopNow = false);
  void entryReady(const GameEntry &entry, const QString &output,
                  const QString &debug, const QString &lowMatch);

private:
  AbstractScraper *scraper;

  Settings config;
  NameTools *NameTool;

  QSharedPointer<Cache> cache;
  QSharedPointer<NetManager> manager;
  QSharedPointer<Queue> queue;

  QString threadId;

  GameEntry getEntryFromUser(const QList<GameEntry> &gameEntries, const GameEntry &suggestedGame,
                             const QString &compareTitle, int &lowestDistance);

  bool limitReached(QString &output);
};

#endif // SCRAPERWORKER_H
