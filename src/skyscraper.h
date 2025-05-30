/***************************************************************************
 *            skyscraper.h
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

#ifndef SKYSCRAPER_H
#define SKYSCRAPER_H

#include "netcomm.h"
#include "netmanager.h"
#include "scraperworker.h"
#include "cache.h"
#include "abstractfrontend.h"
#include "settings.h"
#include "platform.h"

#include <QObject>
#include <QList>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>

#include <QCommandLineParser>

class Skyscraper : public QObject
{
  Q_OBJECT

public:
  Skyscraper(const QCommandLineParser &parser, const QString &currentDir);
  ~Skyscraper();
  void setLock();
  void removeLockAndExit(const int exitCode = 0);
  QSharedPointer<Queue> queue;
  QSharedPointer<NetManager> manager;
  int state = 0;
  inline static Settings config;
  inline static QString docType;

public slots:
  void run();

signals:
  void finished();

private slots:
  void entryReady(const GameEntry &entry, const QString &output,
                  const QString &debug, const QString &lowMatch);
  void checkThreads(const bool &stopNow = false);

private:
  inline static QFile lockFile;
  inline static int docTypeCurrent = 0;
  void loadConfig(const QCommandLineParser &parser);
  void copyFile(const QString &distro, const QString &current, bool overwrite = true);
  QString secsToString(const int &seconds);
  QList<QFileInfo> sliceFiles(const QDir &inputDir, bool &foundSliceStart, bool &foundsliceEnd);
  void checkForFolder(QDir &folder, bool create = true);
  void showHint();
  void doPrescrapeJobs();
  void loadAliasMap();
  void loadMameMap();
  void loadWhdLoadMap();
  void setRegionPrios();
  void setLangPrios();
  //void migrate(QString filename);

  AbstractFrontend *frontend = nullptr;

  QSharedPointer<Cache> cache;

  QList<GameEntry> gameEntries;
  QStringList cliFiles;
  QMutex entryMutex;
  QMutex checkThreadMutex;
  QElapsedTimer timer;
  QString gameListFileString;
  QString skippedFileString;
  int doneThreads;
  int notFound;
  int found;
  int avgSearchMatch;
  int avgCompleteness;
  int currentFile;
  int totalFiles;
};

#endif // SKYSCRAPER_H
