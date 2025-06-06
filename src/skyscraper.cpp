/***************************************************************************
 *            skyscraper.cpp
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

#include <unistd.h>
#include <iostream>

#include <QThread>
#include <QSettings>
#include <QDirIterator>
#include <QTimer>
#include <QMutexLocker>
#include <QProcess>
#include <QDomDocument>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStorageInfo>
#include <QRandomGenerator>

#include "skyscraper.h"
#include "strtools.h"
#include "nametools.h"
#include "emulationstation.h"
#include "retrobat.h"
#include "attractmode.h"
#include "pegasus.h"
#include "xmlexport.h"
#include "koillection.h"
#include "gameentry.h"


Skyscraper::Skyscraper(const QCommandLineParser &parser, const QString &currentDir)
{
  qRegisterMetaType<GameEntry>("GameEntry");

  manager = QSharedPointer<NetManager>(new NetManager());

  printf("%s", StrTools::getVersionHeader().toStdString().c_str()); fflush(stdout);

  config.currentDir = currentDir;
  loadConfig(parser);
}

Skyscraper::~Skyscraper()
{
  delete frontend;
  removeLockAndExit(0);
}

void Skyscraper::setLock()
{
  // Deactivate buffering of stdout (to avoid sync issues between Qt and Std libraries printouts)
  setbuf(stdout, NULL);
  // Cache as a scraper is actually a read-write source, due to cache maintenance actions
  Skyscraper::lockFile.setFileName("cache/." + config.platform + ".lock");
  bool lockAcquired = false;
  bool keepTrying = true;
  printf("\nINFO: Locking the cache for the platform..."); fflush(stdout);
  while(keepTrying) {
    if(Skyscraper::lockFile.open(QIODevice::NewOnly)) {
      lockAcquired = true;
      keepTrying = false;
      printf(" Success!\n");
    }
    else {
      if(config.waitIfConcurrent) {
        sleep(5);
        printf("."); fflush(stdout);
      }
      else {
        keepTrying = false;
      }
    }
  }
  if(!lockAcquired) {
    printf(" ERROR: There is another instance scraping the same platform or frontend+platform). Exiting.\n");
    exit(3);
  }
}

void Skyscraper::removeLockAndExit(int exitCode)
{
  if(lockFile.isOpen()) {
    if(!lockFile.remove()) {
      printf("ERROR: Could not remove lockfile.\n");
    }
  }
  if(frontend) {
    delete frontend;
  }
  printf("%d\n", exitCode);
  emit finished();
  exit(exitCode);
}

void Skyscraper::run()
{
  printf("Platform:           '\033[1;32m%s\033[0m'\n", config.platform.toStdString().c_str());
  printf("Scraping module:    '\033[1;32m%s\033[0m'\n", config.scraper.toStdString().c_str());
  if(config.scraper == "cache") {
    printf("Frontend:           '\033[1;32m%s\033[0m'\n", config.frontend.toStdString().c_str());
    if(!config.frontendExtra.isEmpty()) {
      printf("Extra:              '\033[1;32m%s\033[0m'\n", config.frontendExtra.toStdString().c_str());
    }
  }
  printf("Input folder:       '\033[1;32m%s\033[0m'\n", config.inputFolder.toStdString().c_str());
  printf("Game list folder:   '\033[1;32m%s\033[0m'\n", config.gameListFolder.toStdString().c_str());
  printf("Covers folder:      '\033[1;32m%s\033[0m'\n", config.coversFolder.toStdString().c_str());
  printf("Screenshots folder: '\033[1;32m%s\033[0m'\n", config.screenshotsFolder.toStdString().c_str());
  printf("Wheels folder:      '\033[1;32m%s\033[0m'\n", config.wheelsFolder.toStdString().c_str());
  printf("Marquees folder:    '\033[1;32m%s\033[0m'\n", config.marqueesFolder.toStdString().c_str());
  printf("Textures folder:    '\033[1;32m%s\033[0m'\n", config.texturesFolder.toStdString().c_str());
  if(config.videos) {
    printf("Videos folder:      '\033[1;32m%s\033[0m'\n", config.videosFolder.toStdString().c_str());
  }
  if(config.manuals) {
    printf("Manuals folder:     '\033[1;32m%s\033[0m'\n", config.manualsFolder.toStdString().c_str());
  }
  printf("Cache folder:       '\033[1;32m%s\033[0m'\n", config.cacheFolder.toStdString().c_str());
  if(config.scraper == "import") {
    printf("Import folder:      '\033[1;32m%s\033[0m'\n", config.importFolder.toStdString().c_str());
  }

  printf("\n");

  if(config.hints) {
    showHint();
  }

  doPrescrapeJobs();

  doneThreads = 0;
  notFound = 0;
  found = 0;
  avgCompleteness = 0;
  avgSearchMatch = 0;

  if(!config.cacheFolder.isEmpty()) {
    cache = QSharedPointer<Cache>(new Cache(config.cacheFolder, config.scraper));
    NameTools::cache = cache;
    if(cache->createFolders()) {
      if(!cache->read() && config.scraper == "cache") {
        printf("No resources for this platform found in the resource cache. Please "
               "specify a scraping module with '-s' to gather some resources before "
               "trying to generate a game list. Check all available modules with "
               "'--help'. You can also run Skyscraper in simple mode by typing "
               "'Skyscraper' and follow the instructions on screen.\n\n");
        removeLockAndExit(1);
      }
    } else {
      printf("Couldn't create cache folders, please check folder permissions and try again...\n");
      removeLockAndExit(1);
    }
  }
  if(config.verbosity || config.cacheOptions == "show") {
    cache->showStats(config.cacheOptions == "show"?2:config.verbosity);
    if(config.cacheOptions == "show")
      removeLockAndExit(0);
  }
  if(config.cacheOptions.contains("purge:") ||
     config.cacheOptions.contains("vacuum") ||
     config.cacheOptions.contains("scrapingerrors")) {
    bool success = true;
    if(config.cacheOptions == "purge:all") {
      success = cache->purgeAll(config.unattend || config.unattendSkip);
    } else if(config.cacheOptions == "vacuum") {
      success = cache->vacuumResources(
         config.inputFolder,
         Platform::get().getFormats(config.platform, config.extensions, config.addExtensions),
         config.verbosity, config.unattend || config.unattendSkip);
    } else if(config.cacheOptions == "scrapingerrors") {
      success = cache->detectScrapingErrors(config);
    } else if(config.cacheOptions.contains("purge:m=") ||
              config.cacheOptions.contains("purge:t=")) {
      success = cache->purgeResources(config.cacheOptions);
    }
    if(success) {
      state = 1; // Ignore ctrl+c
      cache->write();
      state = 0;
    }
    removeLockAndExit(0);
  }
  if(config.cacheOptions.contains("report:")) {
    cache->assembleReport(config, Platform::get().getFormats(config.platform,
                                                       config.extensions,
                                                       config.addExtensions));
    removeLockAndExit(0);
  }
  if(config.cacheOptions == "validate") {
    cache->validate();
    state = 1; // Ignore ctrl+c
    cache->write();
    state = 0;
    removeLockAndExit(0);
  }
  if(config.cacheOptions == "ignorenegativecache") {
    config.ignoreNegCache = true;
  }
  if(config.cacheOptions.contains("merge:")) {
    QFileInfo mergeCacheInfo(config.cacheOptions.replace("merge:", ""));
    if(mergeCacheInfo.exists()) {
      Cache mergeCache(mergeCacheInfo.absoluteFilePath(), config.scraper);
      mergeCache.read();
      cache->merge(mergeCache, config.refresh, mergeCacheInfo.absoluteFilePath());
      state = 1; // Ignore ctrl+c
      cache->write();
      state = 0;
    } else {
      printf("Folder to merge from doesn't seem to exist, can't continue...\n");
    }
    removeLockAndExit(0);
  }
  cache->readPriorities();

  QDir inputDir(config.inputFolder, Platform::get().getFormats(config.platform, config.extensions, config.addExtensions), QDir::Name, QDir::Files);
  if(!config.generateLbDb && !inputDir.exists()) {
    printf("Input folder '\033[1;32m%s\033[0m' doesn't exist or can't be seen by "
           "current user. Please check path and permissions.\n",
           inputDir.absolutePath().toStdString().c_str());
    removeLockAndExit(1);
  }
  config.inputFolder = inputDir.absolutePath();

  QDir gameListDir(config.gameListFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(gameListDir);
  config.gameListFolder = gameListDir.absolutePath();

  QDir coversDir(config.coversFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(coversDir);
  config.coversFolder = coversDir.absolutePath();

  QDir screenshotsDir(config.screenshotsFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(screenshotsDir);
  config.screenshotsFolder = screenshotsDir.absolutePath();

  QDir wheelsDir(config.wheelsFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(wheelsDir);
  config.wheelsFolder = wheelsDir.absolutePath();

  QDir marqueesDir(config.marqueesFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(marqueesDir);
  config.marqueesFolder = marqueesDir.absolutePath();

  QDir texturesDir(config.texturesFolder);
  if(config.scraper == "cache" && !config.pretend)
    checkForFolder(texturesDir);
  config.texturesFolder = texturesDir.absolutePath();

  if(config.videos) {
    QDir videosDir(config.videosFolder);
    if(config.scraper == "cache" && !config.pretend)
      checkForFolder(videosDir);
    config.videosFolder = videosDir.absolutePath();
  }

  if(config.manuals) {
    QDir manualsDir(config.manualsFolder);
    if(config.scraper == "cache" && !config.pretend)
      checkForFolder(manualsDir);
    config.manualsFolder = manualsDir.absolutePath();
  }

  if(!config.importFolder.isEmpty()) {
    QDir importDir(config.importFolder);
    checkForFolder(importDir, false);
    config.importFolder = importDir.absolutePath();
  }

  gameListFileString = gameListDir.absolutePath() + "/" + frontend->getGameListFileName();

  QFile gameListFile(gameListFileString);

  // Create shared queue with files to process
  bool foundSliceStart = false;
  bool foundSliceEnd = false;
  QList<QFileInfo> infoList = sliceFiles(inputDir, foundSliceStart, foundSliceEnd);
  queue = QSharedPointer<Queue>(new Queue());
  if(!infoList.isEmpty()) {
    queue->append(infoList);
  }
  if(config.subdirs) {
    QDirIterator dirIt(config.inputFolder,
                       QDir::Dirs | QDir::NoDotAndDotDot,
                       QDirIterator::Subdirectories);
    QString exclude = "";
    while(dirIt.hasNext()) {
      QString subdir = dirIt.next();
      if(config.scraper != "cache" && QFileInfo::exists(subdir + "/.skyscraperignoretree")) {
        exclude = subdir;
      }
      if(!exclude.isEmpty() &&
         (subdir == exclude ||
          (subdir.left(exclude.length()) == exclude && subdir.mid(exclude.length(), 1) == "/"))) {
        continue;
      } else {
        exclude.clear();
      }
      if(config.scraper != "cache" && QFileInfo::exists(subdir + "/.skyscraperignore")) {
        continue;
      }
      inputDir.setPath(subdir);
      QList<QFileInfo> infoListDir = sliceFiles(inputDir, foundSliceStart, foundSliceEnd);
      if(!infoListDir.isEmpty()) {
        queue->append(infoListDir);
      }
      if(config.verbosity > 0) {
        printf("Adding files from subdir: '%s'\n", subdir.toStdString().c_str());
      }
    }
    if(config.verbosity > 0)
      printf("\n");
  }
  if(!config.excludePattern.isEmpty()) {
    queue->filterFiles(config.excludePattern);
  }
  if(!config.includePattern.isEmpty()) {
    queue->filterFiles(config.includePattern, true);
  }

  if(!cliFiles.isEmpty()) {
    queue->clear();
    for(const auto &cliFile: std::as_const(cliFiles)) {
      queue->append(QFileInfo(cliFile));
    }
  }

  // Remove files from excludeFrom, if any
  if(!config.excludeFrom.isEmpty()) {
    QFileInfo excludeFromInfo(config.excludeFrom);
    if(!excludeFromInfo.exists()) {
      excludeFromInfo.setFile(config.currentDir + "/" + config.excludeFrom);
    }
    if(excludeFromInfo.exists()) {
      QFile excludeFrom(excludeFromInfo.absoluteFilePath());
      if(excludeFrom.open(QIODevice::ReadOnly)) {
        QStringList excludes;
        while(!excludeFrom.atEnd()) {
          excludes.append(QString(excludeFrom.readLine().simplified()));
        }
        excludeFrom.close();
        if(!excludes.isEmpty()) {
          queue->removeFiles(excludes);
        }
      }
    } else {
      printf("File: '\033[1;32m%s\033[0m' does not exist.\n\nPlease verify the "
             "filename and try again...\n",
             excludeFromInfo.absoluteFilePath().toStdString().c_str());
      removeLockAndExit(1);
    }
  }

  state = 2; // Clear queue on ctrl+c
  if(config.cacheOptions.left(4) == "edit") {
    QString editCommand = "";
    QString editType = "";
    if(config.cacheOptions.contains(":") && config.cacheOptions.contains("=")) {
      config.cacheOptions.remove(0, config.cacheOptions.indexOf(":") + 1);
      if(config.cacheOptions.split("=").size() == 2) {
        editCommand = config.cacheOptions.split("=").at(0);
        editType = config.cacheOptions.split("=").at(1);
      }
    }
    cache->editResources(queue, editCommand, editType);
    printf("Done editing resources!\n");
    state = 1; // Ignore ctrl+c
    cache->write();
    state = 0;
    removeLockAndExit(0);
  }
  state = 0;

  if(!config.pretend && config.scraper == "cache" && config.gameListBackup) {
    QString gameListBackup = gameListFile.fileName() + "-" +
      QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    printf("Game list backup saved to '\033[1;33m%s\033[0m'\n", gameListBackup.toStdString().c_str());
    gameListFile.copy(gameListBackup);
  }

  if(!config.pretend && config.scraper == "cache" &&
     !config.unattend && !config.unattendSkip &&
     gameListFile.exists()) {
    std::string userInput = "";
    printf("\033[1;34m'\033[0m\033[1;33m%s\033[0m\033[1;34m' already exists, do you want to overwrite it\033[0m (y/N)? ",
           frontend->getGameListFileName().toStdString().c_str()); fflush(stdout);
    getline(std::cin, userInput);
    if(userInput == "y" || userInput == "Y") {
    } else {
      printf("User chose not to overwrite, now exiting...\n");
      removeLockAndExit(0);
    }
    printf("Checking if '\033[1;33m%s\033[0m' is writable?... ",
           frontend->getGameListFileName().toStdString().c_str()); fflush(stdout);

    if(gameListFile.open(QIODevice::Append)) {
      printf("\033[1;32mIt is! :)\033[0m\n");
      gameListFile.close();
    } else {
      printf("\033[1;31mIt isn't! :(\nPlease check path and permissions and try again.\033[0m\n");
      removeLockAndExit(1);
    }
    printf("\n");
  }
  if(config.pretend && config.scraper == "cache") {
    printf("Pretend set! Not changing any files, just showing output.\n\n");
  }

  QFile::remove(skippedFileString);

  if(gameListFile.exists()) {
    printf("Trying to parse and load existing game list metadata... "); fflush(stdout);
    fflush(stdout);
    if(frontend->loadOldGameList(gameListFileString)) {
      printf("\033[1;32mSuccess!\033[0m\n");
      if(!config.unattend && cliFiles.isEmpty()) {
        std::string userInput = "";
        if(gameListFile.exists() && frontend->canSkip()) {
          if(config.unattendSkip) {
            userInput = "y";
          } else {
            printf("\033[1;34mDo you want to skip already existing game list entries\033[0m (y/N)? "); fflush(stdout);
            getline(std::cin, userInput);
          }
          if((userInput == "y" || userInput == "Y") && frontend->canSkip()) {
            frontend->skipExisting(gameEntries, queue);
          }
        }
      }
    } else {
      printf("\033[1;33mNot found or unsupported!\033[0m\n");
    }
  }

  totalFiles = queue->length();

  if(config.romLimit != -1 && totalFiles > config.romLimit && !config.onlyMissing) {
    printf("\n\033[1;33mRestriction overrun!\033[0m This scraping module only allows "
           "for scraping up to %d roms at a time. You can either supply a few rom "
           "filenames on command line, or make use of the '--startat' and / or '--endat' "
           "command line options to adhere to this. Alternatively, you can use the "
           "--cache onlymissing option so that only games without any relevant resource "
           "in other scrapers are attempted. Please check '--help' for more info.\n\n"
           "Now quitting...\n",
           config.romLimit);
    removeLockAndExit(0);
  }
  printf("\n");
  if(totalFiles > 0) {
    printf("Starting scraping run on \033[1;32m%d\033[0m files using \033[1;32m%d\033[0m "
           "threads.\nSit back, relax and let me do the work! :)\n\n",
           totalFiles, config.threads);
  } else if(!config.generateLbDb) {
    printf("\nNo entries to scrape...\n\n");
  }

  timer.start();
  currentFile = 1;

  // Very ugly hack because it's actually more than one database (1/3):
  if(config.scraper == "docsdb" && !docTypeCurrent) {
    docType = config.docTypes.at(docTypeCurrent);
  }
  QList<QThread*> threadList;
  for(int curThread = 1; curThread <= config.threads; ++curThread) {
    QThread *thread = new QThread;
    ScraperWorker *worker = new ScraperWorker(queue, cache, manager, config, QString::number(curThread));
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &ScraperWorker::run);
    connect(worker, &ScraperWorker::entryReady, this, &Skyscraper::entryReady);
    connect(worker, &ScraperWorker::allDone, this, &Skyscraper::checkThreads);
    connect(thread, &QThread::finished, worker, &ScraperWorker::deleteLater);
    threadList.append(thread);
    // Do not start more threads if we have less files than allowed threads
    if(curThread == totalFiles && !config.cacheGb) {
      config.threads = curThread;
      break;
    }
  }
  // Ready, set, GO! Start all threads
  for(const auto thread: std::as_const(threadList)) {
    thread->start();
    state = 3;
  }
}

QList<QFileInfo> Skyscraper::sliceFiles(const QDir &inputDir, bool &foundSliceStart, bool &foundSliceEnd)
{
  QList<QFileInfo> infoList = inputDir.entryInfoList();
  if(config.scraper != "cache" && QFileInfo::exists(config.inputFolder + "/.skyscraperignore")) {
    infoList.clear();
  }
  if(foundSliceEnd) {
    infoList.clear();
  }
  if(!config.startAt.isEmpty() && !infoList.isEmpty() && !foundSliceStart) {
    QFileInfo startAt(config.startAt);
    if(!startAt.exists()) {
      startAt.setFile(config.currentDir + "/" + config.startAt);
    }
    if(!startAt.exists()) {
      startAt.setFile(inputDir.absolutePath() + "/" + config.startAt);
    }
    if(startAt.exists()) {
      while(!infoList.isEmpty() && infoList.first().fileName() != startAt.fileName()) {
        infoList.removeFirst();
      }
      if(!infoList.isEmpty()) {
        foundSliceStart = true;
      }
    }
  }
  if(!config.endAt.isEmpty() && !infoList.isEmpty()) {
    QList<QFileInfo> infoListCopy(infoList);
    QFileInfo endAt(config.endAt);
    if(!endAt.exists()) {
      endAt.setFile(config.currentDir + "/" + config.endAt);
    }
    if(!endAt.exists()) {
      endAt.setFile(inputDir.absolutePath() + "/" + config.endAt);
    }
    if(endAt.exists()) {
      while(!infoList.isEmpty() && infoList.last().fileName() != endAt.fileName()) {
        infoList.removeLast();
      }
      if(!infoList.isEmpty()) {
        foundSliceEnd = true;
      } else {
        infoList = infoListCopy;
      }
    }
  }
  return infoList;
}

void Skyscraper::checkForFolder(QDir &folder, bool create)
{
  if(!folder.exists()) {
    printf("Folder '%s' doesn't exist", folder.absolutePath().toStdString().c_str());
    if(create) {
      printf(", trying to create it... "); fflush(stdout);
      fflush(stdout);
      if(folder.mkpath(folder.absolutePath())) {
        printf("\033[1;32mSuccess!\033[0m\n");
      } else {
        printf("\033[1;32mFailed!\033[0m Please check path and permissions, now exiting...\n");
        removeLockAndExit(1);
      }
    } else {
      printf(", can't continue...\n");
      removeLockAndExit(1);
    }
  }
}

QString Skyscraper::secsToString(const int &secs)
{
  QString hours = QString::number(secs / 3600000 % 24);
  QString minutes = QString::number(secs / 60000 % 60);
  QString seconds = QString::number(secs / 1000 % 60);
  if(hours.length() == 1) {
    hours.prepend("0");
  }
  if(minutes.length() == 1) {
    minutes.prepend("0");
  }
  if(seconds.length() == 1) {
    seconds.prepend("0");
  }

  return hours + ":" + minutes + ":" + seconds;
}

void Skyscraper::entryReady(const GameEntry &entry, const QString &output, const QString &debug, const QString &lowMatch)
{
  QMutexLocker locker(&entryMutex);

  printf("\033[0;32m#%d/%d\033[0m %s\n", currentFile, totalFiles, output.toStdString().c_str());

  if(config.verbosity >= 3) {
    printf("\033[1;33mDebug output:\033[0m\n%s\n", debug.toStdString().c_str());
  }

  if(entry.found && !entry.emptyShell) {
    found++;
    avgCompleteness += entry.getCompleteness();
    avgSearchMatch += entry.searchMatch;
    gameEntries.append(entry);
  } else {
    notFound++;
    QFile skippedFile(skippedFileString);
    skippedFile.open(QIODevice::Append);
    skippedFile.write(entry.absoluteFilePath.toUtf8() + "\n");
    if(!lowMatch.isEmpty()) {
      skippedFile.write(lowMatch.toUtf8());
    }
    skippedFile.close();
    if(config.skipped) {
      gameEntries.append(entry);
    }
  }

  printf("\033[1;34m#%d/%d\033[0m, (\033[1;32m%d\033[0m/\033[1;33m%d\033[0m)\n", currentFile, totalFiles, found, notFound);
  int elapsed = timer.elapsed();
  int estTime = (elapsed / currentFile * totalFiles) - elapsed;
  if(estTime < 0)
    estTime = 0;
  printf("Elapsed time   : \033[1;33m%s\033[0m\n", secsToString(elapsed).toStdString().c_str());
  printf("Est. time left : \033[1;33m%s\033[0m\n\n", secsToString(estTime).toStdString().c_str());

  if(!config.onlyMissing &&
     currentFile == config.maxFails &&
     notFound == config.maxFails &&
     config.scraper != "import" && config.scraper != "cache") {
    printf("\033[1;31mOut of %d files we had %d misses. So either the scraping source "
           "is down or you are using a scraping source that doesn't support this platform. "
           "Please try another scraping module (check '--help').\n\nNow exiting...\033[0m\n",
           config.maxFails, config.maxFails);
    removeLockAndExit(1);
  }
  currentFile++;

  qint64 spaceLimit = 209715200;
  if(config.spaceCheck) {
    if(config.scraper == "cache" && !config.pretend &&
       QStorageInfo(QDir(config.screenshotsFolder)).bytesFree() < spaceLimit) {
      printf("\033[1;31mYou have very little disk space left on the Skyscraper media "
             "export drive, please free up some space and try again. Now aborting...\033[0m\n\n");
      printf("Note! You can disable this check by setting 'spaceCheck=\"false\"' in the '[main]' section of config.ini.\n\n");
      // By clearing the queue here we basically tell Skyscraper to stop and quit nicely
      config.pretend = true;
      queue->clearAll();
    } else if(QStorageInfo(QDir(config.cacheFolder)).bytesFree() < spaceLimit) {
      printf("\033[1;31mYou have very little disk space left on the Skyscraper resource "
             "cache drive, please free up some space and try again. Now aborting...\033[0m\n\n");
      printf("Note! You can disable this check by setting 'spaceCheck=\"false\"' in the '[main]' section of config.ini.\n\n");
      // By clearing the queue here we basically tell Skyscraper to stop and quit nicely
      config.pretend = true;
      queue->clearAll();
    }
  }
}

void Skyscraper::checkThreads(const bool &stopNow)
{
  QMutexLocker locker(&checkThreadMutex);

  doneThreads++;
  if(doneThreads != config.threads)
    return;

  if(!stopNow) {
    if(!config.pretend && config.scraper == "cache") {
      printf("\033[1;34m---- Game list generation run completed! YAY! ----\033[0m\n");
      if(!config.cacheFolder.isEmpty()) {
        state = 1; // Ignore ctrl+c
        cache->write(true);
        state = 0;
      }
      QString finalOutput;
      frontend->sortEntries(gameEntries);
      printf("Assembling game list..."); fflush(stdout);
      frontend->assembleList(finalOutput, gameEntries);
      printf(" \033[1;32mDone!\033[0m\n");
      if(!finalOutput.isEmpty()) {
        QFile gameListFile(gameListFileString);
        printf("Now writing '\033[1;33m%s\033[0m'... ", gameListFileString.toStdString().c_str()); fflush(stdout);
        if(gameListFile.open(QIODevice::WriteOnly)) {
          state = 1; // Ignore ctrl+c
          gameListFile.write(finalOutput.toUtf8());
          state = 0;
          gameListFile.close();
          printf("\033[1;32mSuccess!\033[0m\n\n");
        } else {
          printf("\033[1;31mCouldn't open file for writing!\nAll that work for nothing... :(\033[0m\n");
        }
      }
    } else {
      printf("\033[1;34m---- Resource gathering run completed! YAY! ----\033[0m\n");
      if(!config.cacheFolder.isEmpty()) {
        state = 1; // Ignore ctrl+c
        cache->write();
        state = 0;
      }
    }

    printf("\033[1;34m---- And here are some neat stats :) ----\033[0m\n");
    printf("Total completion time: \033[1;33m%s\033[0m\n\n", secsToString(timer.elapsed()).toStdString().c_str());
    if(found > 0) {
      printf("Average search match: \033[1;33m%d%%\033[0m\n",
             (int)((double)avgSearchMatch / (double)found));
      printf("Average entry completeness: \033[1;33m%d%%\033[0m\n\n",
             (int)((double)avgCompleteness / (double)found));
    }
    printf("\033[1;34mTotal number of games: %d\033[0m\n", totalFiles);
    printf("\033[1;32mSuccessfully processed games: %d\033[0m\n", found);
    printf("\033[1;33mSkipped games: %d\033[0m (Filenames saved to '\033[1;33m%s/%s\033[0m')\n\n",
           notFound, QDir::currentPath().toStdString().c_str(), skippedFileString.toStdString().c_str());
  }
  // Very ugly hack because it's actually more than one database (2/3):
  if(config.scraper == "docsdb") {
    docTypeCurrent++;
    if(docTypeCurrent < config.docTypes.size()) {
      docType = config.docTypes.at(docTypeCurrent);
      QTimer::singleShot(0, this, SLOT(run()));
    } else {
      // All done, now clean up and exit to terminal
      removeLockAndExit(0);
    }
  } else {
    // All done, now clean up and exit to terminal
    removeLockAndExit(0);
  }
}

void Skyscraper::loadConfig(const QCommandLineParser &parser)
{
/*
  // -----
  // Files that should ALWAYS be updated from distributed default files - DEPRECATED by RMA
  // -----

  copyFile("/usr/local/etc/skyscraper/config.ini.example", "config.ini.example", false);
  copyFile("/usr/local/etc/skyscraper/hints.xml", "hints.xml", false);
  copyFile("/usr/local/etc/skyscraper/artwork.xml.example1", "artwork.xml.example1", false);
  copyFile("/usr/local/etc/skyscraper/artwork.xml.example2", "artwork.xml.example2", false);
  copyFile("/usr/local/etc/skyscraper/artwork.xml.example3", "artwork.xml.example3", false);
  copyFile("/usr/local/etc/skyscraper/artwork.xml.example4", "artwork.xml.example4", false);
  copyFile("/usr/local/etc/skyscraper/mameMap.csv", "mameMap.csv", false);
  copyFile("/usr/local/etc/skyscraper/tgdb_developers.json", "tgdb_developers.json", false);
  copyFile("/usr/local/etc/skyscraper/tgdb_publishers.json", "tgdb_publishers.json", false);
  copyFile("/usr/local/etc/skyscraper/tgdb_genres.json", "tgdb_genres.json", false);
  copyFile("/usr/local/etc/skyscraper/resources/boxfront.png", "resources/boxfront.png", false);
  copyFile("/usr/local/etc/skyscraper/resources/boxside.png", "resources/boxside.png", false);
  copyFile("/usr/local/etc/skyscraper/resources/missingboxart.png", "resources/missingboxart.png", false);
  copyFile("/usr/local/etc/skyscraper/cache/priorities.xml.example", "cache/priorities.xml.example", false);
  copyFile("/usr/local/etc/skyscraper/import/definitions.dat.example1", "import/definitions.dat.example1", false);
  copyFile("/usr/local/etc/skyscraper/import/definitions.dat.example2", "import/definitions.dat.example2", false);

  // -----
  // Files that will only be copied if they don't already exist
  // -----

  // Make sure we have a default config.ini file based on the config.ini.example file
  // False means it won't overwrite if it exists
  copyFile("/usr/local/etc/skyscraper/platforms.json", "platforms.json", false); 
  copyFile("/usr/local/etc/skyscraper/mobygames.json", "mobygames.json", false);
  copyFile("/usr/local/etc/skyscraper/screenscraper.json", "screenscraper.json", false);
  copyFile("/usr/local/etc/skyscraper/spriters.json", "spriters.json", false);
  copyFile("/usr/local/etc/skyscraper/thegamesdb.json", "thegamesdb.json", false);
  copyFile("/usr/local/etc/skyscraper/tgdb_developers.json", "tgdb_developers.json", false);
  copyFile("/usr/local/etc/skyscraper/tgdb_publishers.json", "tgdb_publishers.json", false);
  copyFile("/usr/local/etc/skyscraper/launchbox.json", "launchbox.json", false);
  copyFile("/usr/local/etc/skyscraper/checksumcatalogs.json", "checksumcatalogs.json", false);
  copyFile("/usr/local/etc/skyscraper/categorypatterns.json", "categorypatterns.json", false);
  copyFile("/usr/local/etc/skyscraper/giantbomb.json", "giantbomb.json", false);
  copyFile("/usr/local/etc/skyscraper/rawg.json", "rawg.json", false);
  copyFile("/usr/local/etc/skyscraper/mamehistory.json", "mamehistory.json", false);
  copyFile("/usr/local/etc/skyscraper/everygame.json", "everygame.json", false);
  copyFile("/usr/local/etc/skyscraper/openretro.json", "openretro.json", false);
  copyFile("/usr/local/etc/skyscraper/vggeek.json", "vggeek.json", false);
  copyFile("/usr/local/etc/skyscraper/config.ini.example", "config.ini", false);
  copyFile("/usr/local/etc/skyscraper/artwork.xml", "artwork.xml", false);
  copyFile("/usr/local/etc/skyscraper/aliasMap.csv", "aliasMap.csv", false);
  copyFile("/usr/local/etc/skyscraper/genresMap.json", "genresMap.json", false);
  copyFile("/usr/local/etc/skyscraper/franchisesMap.json", "franchisesMap.json", false);
  copyFile("/usr/local/etc/skyscraper/resources/maskexample.png", "resources/maskexample.png", false);
  copyFile("/usr/local/etc/skyscraper/resources/frameexample.png", "resources/frameexample.png", false);
  copyFile("/usr/local/etc/skyscraper/resources/scanlines1.png", "resources/scanlines1.png", false);
  copyFile("/usr/local/etc/skyscraper/resources/scanlines2.png", "resources/scanlines2.png", false);
  // Copy one of the example definitions.dat files if none exists
  copyFile("/usr/local/etc/skyscraper/import/definitions.dat.example2", "import/definitions.dat", false);

  // -----
  // END updating files from distribution files
  // -----
*/

  //migrate(parser.isSet("c")?parser.value("c"):"config.ini");

  QSettings settings(parser.isSet("c")?parser.value("c"):"config.ini", QSettings::IniFormat);

  // Start by setting frontend, since we need it to set default for game list and so on
  settings.beginGroup("main");
  if(settings.contains("frontend")) {
    config.frontend = settings.value("frontend").toString();
  }
  settings.endGroup();
  if(parser.isSet("f") && (parser.value("f") == "emulationstation" ||
                           parser.value("f") == "retrobat" ||
                           parser.value("f") == "attractmode" ||
                           parser.value("f") == "pegasus" ||
                           parser.value("f") == "xmlexport" ||
                           parser.value("f") == "koillection")) {
    config.frontend = parser.value("f");
  } else if(parser.isSet("f")) {
    printf("Frontend: '\033[1;32m%s\033[0m' is not supported. Please select a valid one.\n",
           parser.value("f").toStdString().c_str());
    removeLockAndExit(1);
  }

  bool inputFolderSet = false;
  bool gameListFolderSet = false;
  bool mediaFolderSet = false;

  config.negCacheExpiration = QDateTime::currentMSecsSinceEpoch() / 1000
                               - config.negCacheDaysExpiration * 24 * 60 * 60;

  // Main config, overrides defaults
  settings.beginGroup("main");
  if(settings.contains("platform")) {
    config.platform = settings.value("platform").toString();
  }
  if(settings.contains("lang")) {
    config.lang = settings.value("lang").toString();
  }
  if(settings.contains("region")) {
    config.region = settings.value("region").toString();
  }
  if(settings.contains("langPrios")) {
    config.langPriosStr = settings.value("langPrios").toString();
  }
  if(settings.contains("regionPrios")) {
    config.regionPriosStr = settings.value("regionPrios").toString();
  }
  if(settings.contains("pretend")) {
    config.pretend = settings.value("pretend").toBool();
  }
  if(settings.contains("interactive")) {
    config.interactive = settings.value("interactive").toBool();
  }
  if(settings.contains("unattend")) {
    config.unattend = settings.value("unattend").toBool();
  }
  if(settings.contains("unattendSkip")) {
    config.unattendSkip = settings.value("unattendSkip").toBool();
    config.unattend = false;
  }
  if(settings.contains("forceFilename")) {
    config.forceFilename = settings.value("forceFilename").toBool();
  }
  if(settings.contains("verbosity")) {
    config.verbosity = settings.value("verbosity").toInt();
  }
  if(settings.contains("hints")) {
    config.hints = settings.value("hints").toBool();
  }
  if(settings.contains("subdirs")) {
    config.subdirs = settings.value("subdirs").toBool();
  }
  if(settings.contains("maxLength")) {
    config.maxLength = settings.value("maxLength").toInt();
  }
  if(settings.contains("threads")) {
    config.threads = settings.value("threads").toInt();
    config.threadsSet = true;
  }
  if(settings.contains("emulator")) {
    config.frontendExtra = settings.value("emulator").toString();
  }
  if(settings.contains("launch")) {
    config.frontendExtra = settings.value("launch").toString();
  }
  if(settings.contains("exodosPath")) {
    config.exodosPath = settings.value("exodosPath").toString();
  }
  if(settings.contains("gamebasePath")) {
    config.gamebasePath = settings.value("gamebasePath").toString();
  }
  if(settings.contains("guidesPath")) {
    config.guidesPath = settings.value("guidesPath").toString();
  }
  if(settings.contains("docsPath")) {
    config.docsPath = settings.value("docsPath").toString();
  }
  if(settings.contains("mapsPath")) {
    config.mapsPath = settings.value("mapsPath").toString();
  }
  if(settings.contains("spritesPath")) {
    config.spritesPath = settings.value("spritesPath").toString();
  }
  if(settings.contains("chiptunes")) {
    config.chiptunes = settings.value("chiptunes").toBool();
  }
  if(settings.contains("guides")) {
    config.guides = settings.value("guides").toBool();
  }
  if(settings.contains("cheats")) {
    config.cheats = settings.value("cheats").toBool();
  }
  if(settings.contains("reviews")) {
    config.reviews = settings.value("reviews").toBool();
  }
  if(settings.contains("artbooks")) {
    config.artbooks = settings.value("artbooks").toBool();
  }
  if(settings.contains("maps")) {
    config.maps = settings.value("maps").toBool();
  }
  if(settings.contains("sprites")) {
    config.sprites = settings.value("sprites").toBool();
  }
  if(settings.contains("manuals")) {
    config.manuals = settings.value("manuals").toBool();
  }
  if(settings.contains("manualSizeLimit")) {
    config.manualSizeLimit = settings.value("manualSizeLimit").toInt() * 1024 * 1024;
  }
  if(settings.contains("videos")) {
    config.videos = settings.value("videos").toBool();
  }
  if(settings.contains("videoSizeLimit")) {
    config.videoSizeLimit = settings.value("videoSizeLimit").toInt() * 1024 * 1024;
  }
  if(settings.contains("videoConvertCommand")) {
    config.videoConvertCommand = settings.value("videoConvertCommand").toString();
  }
  if(settings.contains("videoConvertExtension")) {
    config.videoConvertExtension = settings.value("videoConvertExtension").toString();
  }
  if(settings.contains("negCacheDaysExpiration")) {
    config.negCacheDaysExpiration = settings.value("negCacheDaysExpiration").toInt();
    config.negCacheExpiration = QDateTime::currentMSecsSinceEpoch() / 1000
                                 - config.negCacheDaysExpiration * 24 * 60 * 60;
  }
  if(settings.contains("symlink")) {
    config.symlink = settings.value("symlink").toBool();
  }
  if(settings.contains("symlinkImages")) {
    config.symlinkImages = settings.value("symlinkImages").toBool();
  }
  if(settings.contains("theInFront")) {
    config.theInFront = settings.value("theInFront").toBool();
  }
  if(settings.contains("skipped")) {
    config.skipped = settings.value("skipped").toBool();
  }
  if(settings.contains("skipExistingCovers")) {
    config.skipExistingCovers = settings.value("skipExistingCovers").toBool();
  }
  if(settings.contains("skipExistingMarquees")) {
    config.skipExistingMarquees = settings.value("skipExistingMarquees").toBool();
  }
  if(settings.contains("skipExistingScreenshots")) {
    config.skipExistingScreenshots = settings.value("skipExistingScreenshots").toBool();
  }
  if(settings.contains("skipExistingVideos")) {
    config.skipExistingVideos = settings.value("skipExistingVideos").toBool();
  }
  if(settings.contains("skipExistingManuals")) {
    config.skipExistingManuals = settings.value("skipExistingManuals").toBool();
  }
  if(settings.contains("skipExistingWheels")) {
    config.skipExistingWheels = settings.value("skipExistingWheels").toBool();
  }
  if(settings.contains("skipExistingTextures")) {
    config.skipExistingTextures = settings.value("skipExistingTextures").toBool();
  }
  if(settings.contains("maxFails") &&
     settings.value("maxFails").toInt() >= 1 &&
     settings.value("maxFails").toInt() <= 1000000) {
    config.maxFails = settings.value("maxFails").toInt();
  }
  if(settings.contains("brackets")) {
    config.brackets = settings.value("brackets").toBool();
  }
  if(settings.contains("relativePaths")) {
    config.relativePaths = settings.value("relativePaths").toBool();
  }
  if(settings.contains("addExtensions")) {
    config.addExtensions = settings.value("addExtensions").toString();
  }
  if(settings.contains("minMatch")) {
    config.minMatch = settings.value("minMatch").toInt();
    config.minMatchSet = true;
  }
  if(settings.contains("artworkXml")) {
    config.artworkConfig = settings.value("artworkXml").toString();
  }
  if(settings.contains("includeFiles")) { // This option is DEPRECATED, use includePattern
    config.includePattern = settings.value("includeFiles").toString();
  }
  if(settings.contains("includePattern")) {
    config.includePattern = settings.value("includePattern").toString();
  }
  if(settings.contains("excludeFiles")) { // This option is DEPRECATED, use excludePattern
    config.excludePattern = settings.value("excludeFiles").toString();
  }
  if(settings.contains("excludePattern")) {
    config.excludePattern = settings.value("excludePattern").toString();
  }
  if(settings.contains("includeFrom")) {
    config.includeFrom = settings.value("includeFrom").toString();
  }
  if(settings.contains("excludeFrom")) {
    config.excludeFrom = settings.value("excludeFrom").toString();
  }
  if(settings.contains("jpgQuality")) {
    config.jpgQuality = settings.value("jpgQuality").toInt();
  }
  if(settings.contains("getMissingResources")) {
    config.getMissingResources = settings.value("getMissingResources").toBool();
  }
  if(settings.contains("singleImagePerType")) {
    config.singleImagePerType = settings.value("singleImagePerType").toBool();
  }
  if(settings.contains("onlyMissing")) {
    config.onlyMissing = settings.value("onlyMissing").toBool();
  }
  if(settings.contains("cacheRefresh") && !config.rescan) {
    config.refresh = settings.value("cacheRefresh").toBool();
  }
  if(settings.contains("ignoreNegativeCache")) {
    config.ignoreNegCache = settings.value("ignoreNegativeCache").toBool();
  }
  if(settings.contains("cacheResize")) {
    config.cacheResize = settings.value("cacheResize").toBool();
  }
  if(settings.contains("cacheCovers")) {
    config.cacheCovers = settings.value("cacheCovers").toBool();
  }
  if(settings.contains("cacheTextures")) {
    config.cacheTextures = settings.value("cacheTextures").toBool();
  }
  if(settings.contains("cacheScreenshots")) {
    config.cacheScreenshots = settings.value("cacheScreenshots").toBool();
  }
  if(settings.contains("cropBlack")) {
    config.cropBlack = settings.value("cropBlack").toBool();
  }
  if(settings.contains("cacheWheels")) {
    config.cacheWheels = settings.value("cacheWheels").toBool();
  }
  if(settings.contains("cacheMarquees")) {
    config.cacheMarquees = settings.value("cacheMarquees").toBool();
  }
  if(settings.contains("scummIni")) {
    config.scummIni = settings.value("scummIni").toString();
  }
  // Check for command line platform here, since we need it for 'platform' config.ini entries
  // '_' is seen as a subcategory of the selected platform
  if(parser.isSet("p") && Platform::get().getPlatforms().contains(parser.value("p").split('_').first())) {
    config.platform = parser.value("p");
  } else {
    if(!(parser.isSet("flags") && parser.value("flags") == "help") &&
       !(parser.isSet("cache") && parser.value("cache") == "help") &&
       !parser.isSet("generatelbdb") && !parser.isSet("loadchecksum")) {
      printf("Please set a valid platform with '-p [platform]'\nCheck '--help' for a list of supported platforms.\n");
      removeLockAndExit(1);
    }
  }
  if(settings.contains("cacheFolder")) {
    QString cacheFolder = settings.value("cacheFolder").toString();
    config.cacheFolder = cacheFolder + (cacheFolder.right(1) == "/"?"":"/") + config.platform;
  }
  if(settings.contains("inputFolder")) {
    QString inputFolder = settings.value("inputFolder").toString();
    config.inputFolder = inputFolder + (inputFolder.right(1) == "/"?"":"/") + config.platform;
    inputFolderSet = true;
  }
  // gamelistFolder kept in addition to gameListFolder for backwards compatibility
  if(settings.contains("gamelistFolder")) {
    QString gameListFolder = settings.value("gamelistFolder").toString();
    config.gameListFolder = gameListFolder + (gameListFolder.right(1) == "/"?"":"/") + config.platform;
    gameListFolderSet = true;
  }
  if(settings.contains("gameListFolder")) {
    QString gameListFolder = settings.value("gameListFolder").toString();
    config.gameListFolder = gameListFolder + (gameListFolder.right(1) == "/"?"":"/") + config.platform;
    gameListFolderSet = true;
  }
  if(settings.contains("gameListBackup")) {
    config.gameListBackup = settings.value("gameListBackup").toBool();
  }
  if(settings.contains("mediaFolder")) {
    QString mediaFolder = settings.value("mediaFolder").toString();
    config.mediaFolder = mediaFolder + (mediaFolder.right(1) == "/"?"":"/") + config.platform;
    mediaFolderSet = true;
  }
  if(settings.contains("importFolder")) {
    config.importFolder = settings.value("importFolder").toString();
  }
  if(settings.contains("spaceCheck")) {
    config.spaceCheck = settings.value("spaceCheck").toBool();
  }
  if(settings.contains("fuzzySearch")) {
    config.fuzzySearch = settings.value("fuzzySearch").toInt();
  }
  if(settings.contains("useChecksum")) {
    config.useChecksum = settings.value("useChecksum").toBool();
  }
  if(settings.contains("waitIfConcurrent")) {
    config.waitIfConcurrent = settings.value("waitIfConcurrent").toBool();
  }
  if(settings.contains("keepSubtitle")) {
    config.keepSubtitle = settings.value("keepSubtitle").toBool();
  }
  if(settings.contains("nameTemplate")) {
    config.nameTemplate = settings.value("nameTemplate").toString();
  }
  settings.endGroup();

  // Platform specific configs, overrides main and defaults
  settings.beginGroup(config.platform);
  if(settings.contains("emulator")) {
    config.frontendExtra = settings.value("emulator").toString();
  }
  if(settings.contains("launch")) {
    config.frontendExtra = settings.value("launch").toString();
  }
  if(settings.contains("inputFolder")) {
    config.inputFolder = settings.value("inputFolder").toString();
    inputFolderSet = true;
  }
  // gamelistFolder kept in addition to gameListFolder for backwards compatibility
  if(settings.contains("gamelistFolder")) {
    config.gameListFolder = settings.value("gamelistFolder").toString();
    gameListFolderSet = true;
  }
  if(settings.contains("gameListFolder")) {
    config.gameListFolder = settings.value("gameListFolder").toString();
    gameListFolderSet = true;
  }
  if(settings.contains("mediaFolder")) {
    config.mediaFolder = settings.value("mediaFolder").toString();
    mediaFolderSet = true;
  }
  if(settings.contains("cacheFolder")) {
    config.cacheFolder = settings.value("cacheFolder").toString();
  }
  if(settings.contains("jpgQuality")) {
    config.jpgQuality = settings.value("jpgQuality").toInt();
  }
  if(settings.contains("getMissingResources")) {
    config.getMissingResources = settings.value("getMissingResources").toBool();
  }
  if(settings.contains("singleImagePerType")) {
    config.singleImagePerType = settings.value("singleImagePerType").toBool();
  }
  if(settings.contains("onlyMissing")) {
    config.onlyMissing = settings.value("onlyMissing").toBool();
  }
  if(settings.contains("cacheResize")) {
    config.cacheResize = settings.value("cacheResize").toBool();
  }
  if(settings.contains("cacheCovers")) {
    config.cacheCovers = settings.value("cacheCovers").toBool();
  }
  if(settings.contains("cacheTextures")) {
    config.cacheTextures = settings.value("cacheTextures").toBool();
  }
  if(settings.contains("cacheScreenshots")) {
    config.cacheScreenshots = settings.value("cacheScreenshots").toBool();
  }
  if(settings.contains("cropBlack")) {
    config.cropBlack = settings.value("cropBlack").toBool();
  }
  if(settings.contains("cacheWheels")) {
    config.cacheWheels = settings.value("cacheWheels").toBool();
  }
  if(settings.contains("cacheMarquees")) {
    config.cacheMarquees = settings.value("cacheMarquees").toBool();
  }
  if(settings.contains("importFolder")) {
    config.importFolder = settings.value("importFolder").toString();
  }
  if(settings.contains("skipped")) {
    config.skipped = settings.value("skipped").toBool();
  }
  if(settings.contains("brackets")) {
    config.brackets = settings.value("brackets").toBool();
  }
  if(settings.contains("subdirs")) {
    config.subdirs = settings.value("subdirs").toBool();
  }
  if(settings.contains("relativePaths")) {
    config.relativePaths = settings.value("relativePaths").toBool();
  }
  if(settings.contains("extensions")) {
    config.extensions = settings.value("extensions").toString();
  }
  if(settings.contains("addExtensions")) {
    config.addExtensions = settings.value("addExtensions").toString();
  }
  if(settings.contains("minMatch")) {
    config.minMatch = settings.value("minMatch").toInt();
    config.minMatchSet = true;
  }
  if(settings.contains("lang")) {
    config.lang = settings.value("lang").toString();
  }
  if(settings.contains("region")) {
    config.region = settings.value("region").toString();
  }
  if(settings.contains("langPrios")) {
    config.langPriosStr = settings.value("langPrios").toString();
  }
  if(settings.contains("regionPrios")) {
    config.regionPriosStr = settings.value("regionPrios").toString();
  }
  if(settings.contains("threads")) {
    config.threads = settings.value("threads").toInt();
    config.threadsSet = true;
  }
  if(settings.contains("videos")) {
    config.videos = settings.value("videos").toBool();
  }
  if(settings.contains("videoSizeLimit")) {
    config.videoSizeLimit = settings.value("videoSizeLimit").toInt() * 1000 * 1000;
  }
  if(settings.contains("manuals")) {
    config.manuals = settings.value("manuals").toBool();
  }
  if(settings.contains("manualSizeLimit")) {
    config.manualSizeLimit = settings.value("manualSizeLimit").toInt() * 1000 * 1000;
  }
  if(settings.contains("guides")) {
    config.guides = settings.value("guides").toBool();
  }
  if(settings.contains("cheats")) {
    config.cheats = settings.value("cheats").toBool();
  }
  if(settings.contains("reviews")) {
    config.reviews = settings.value("reviews").toBool();
  }
  if(settings.contains("artbooks")) {
    config.artbooks = settings.value("artbooks").toBool();
  }
  if(settings.contains("maps")) {
    config.maps = settings.value("maps").toBool();
  }
  if(settings.contains("sprites")) {
    config.sprites = settings.value("sprites").toBool();
  }
  if(settings.contains("chiptunes")) {
    config.chiptunes = settings.value("chiptunes").toBool();
  }
  if(settings.contains("negCacheDaysExpiration")) {
    config.negCacheDaysExpiration = settings.value("negCacheDaysExpiration").toInt();
    config.negCacheExpiration = QDateTime::currentMSecsSinceEpoch() / 1000
                                 - config.negCacheDaysExpiration * 24 * 60 * 60;
  }
  if(settings.contains("symlink")) {
    config.symlink = settings.value("symlink").toBool();
  }
  if(settings.contains("symlinkImages")) {
    config.symlinkImages = settings.value("symlinkImages").toBool();
  }
  if(settings.contains("theInFront")) {
    config.theInFront = settings.value("theInFront").toBool();
  }
  if(settings.contains("startAt")) {
    config.startAt = settings.value("startAt").toString();
  }
  if(settings.contains("endAt")) {
    config.endAt = settings.value("endAt").toString();
  }
  if(settings.contains("pretend")) {
    config.pretend = settings.value("pretend").toBool();
  }
  if(settings.contains("unattend")) {
    config.unattend = settings.value("unattend").toBool();
  }
  if(settings.contains("unattendSkip")) {
    config.unattendSkip = settings.value("unattendSkip").toBool();
    config.unattend = false;
  }
  if(settings.contains("interactive")) {
    config.interactive = settings.value("interactive").toBool();
  }
  if(settings.contains("forceFilename")) {
    config.forceFilename = settings.value("forceFilename").toBool();
  }
  if(settings.contains("fuzzySearch")) {
    config.fuzzySearch = settings.value("fuzzySearch").toInt();
  }
  if(settings.contains("useChecksum")) {
    config.useChecksum = settings.value("useChecksum").toBool();
  }
  if(settings.contains("keepSubtitle")) {
    config.keepSubtitle = settings.value("keepSubtitle").toBool();
  }
  if(settings.contains("verbosity")) {
    config.verbosity = settings.value("verbosity").toInt();
  }
  if(settings.contains("maxLength")) {
    config.maxLength = settings.value("maxLength").toInt();
  }
  if(settings.contains("artworkXml")) {
    config.artworkConfig = settings.value("artworkXml").toString();
  }
  if(settings.contains("includeFiles")) { // This option is DEPRECATED, use includePattern
    config.includePattern = settings.value("includeFiles").toString();
  }
  if(settings.contains("includePattern")) {
    config.includePattern = settings.value("includePattern").toString();
  }
  if(settings.contains("excludeFiles")) { // This option is DEPRECATED, use excludePattern
    config.excludePattern = settings.value("excludeFiles").toString();
  }
  if(settings.contains("excludePattern")) {
    config.excludePattern = settings.value("excludePattern").toString();
  }
  if(settings.contains("includeFrom")) {
    config.includeFrom = settings.value("includeFrom").toString();
  }
  if(settings.contains("excludeFrom")) {
    config.excludeFrom = settings.value("excludeFrom").toString();
  }
  if(settings.contains("nameTemplate")) {
    config.nameTemplate = settings.value("nameTemplate").toString();
  }
  settings.endGroup();

  // Check for command line scraping module here
  if(parser.isSet("s") && (parser.value("s") == "openretro" ||
                           parser.value("s") == "vggeek" ||
                           parser.value("s") == "thegamesdb" ||
                           parser.value("s") == "gamebase" ||
                           parser.value("s") == "exodos" ||
                           parser.value("s") == "offlinetgdb" ||
                           parser.value("s") == "arcadedb" ||
                           parser.value("s") == "mamehistory" ||
                           parser.value("s") == "worldofspectrum" ||
                           parser.value("s") == "igdb" ||
                           parser.value("s") == "giantbomb" ||
                           parser.value("s") == "mobygames" ||
                           parser.value("s") == "offlinemobygames" ||
                           parser.value("s") == "screenscraper" ||
                           parser.value("s") == "launchbox" ||
                           parser.value("s") == "gamefaqs" ||
                           parser.value("s") == "vgfacts" ||
                           parser.value("s") == "docsdb" ||
                           parser.value("s") == "everygame" ||
                           parser.value("s") == "rawg" ||
                           parser.value("s") == "chiptune" ||
                           parser.value("s") == "vgmaps" ||
                           parser.value("s") == "spriters" ||
                           parser.value("s") == "customflags" ||
                           parser.value("s") == "esgamelist" ||
                           parser.value("s") == "cache" ||
                           parser.value("s") == "import") &&
                           !parser.isSet("loadchecksum")) {
    config.scraper = parser.value("s");
    if(config.scraper == "customflags") {
      config.rescan = false;
    }
    setLock();
  } else if(parser.isSet("s") && parser.isSet("loadchecksum")) {
    printf("ERROR: Loading chechsum is scraper independent, hence no scraper can be indicated in this mode.\n\n");
    removeLockAndExit(1);
  } else if(parser.isSet("s")) {
    printf("ERROR: The scraper requested does not exist. Please check the list of available scrapers in 'Skyscraper --help'.\n\n");
    removeLockAndExit(1);
  }

  // Frontend specific configs, overrides main, platform, module and defaults
  settings.beginGroup(config.frontend);
  if(settings.contains("userCreds")) {
    config.userCreds = settings.value("userCreds").toString();
  }
  if(settings.contains("dbServer")) {
    config.dbServer = settings.value("dbServer").toString();
  }
  if(settings.contains("dbName")) {
    config.dbName = settings.value("dbName").toString();
  }
  if(settings.contains("dbUser")) {
    config.dbUser = settings.value("dbUser").toString();
  }
  if(settings.contains("dbPassword")) {
    config.dbPassword = settings.value("dbPassword").toString();
  }
  if(settings.contains("relativePaths")) {
    config.relativePaths = settings.value("relativePaths").toBool();
  }
  if(settings.contains("artworkXml")) {
    config.artworkConfig = settings.value("artworkXml").toString();
  }
  if(settings.contains("includeFiles")) { // This option is DEPRECATDE, use includePattern
    config.includePattern = settings.value("includeFiles").toString();
  }
  if(settings.contains("includePattern")) {
    config.includePattern = settings.value("includePattern").toString();
  }
  if(settings.contains("excludeFiles")) { // This option is DEPRECATED, use excludePattern
    config.excludePattern = settings.value("excludeFiles").toString();
  }
  if(settings.contains("excludePattern")) {
    config.excludePattern = settings.value("excludePattern").toString();
  }
  if(settings.contains("skipExistingCovers")) {
    config.skipExistingCovers = settings.value("skipExistingCovers").toBool();
  }
  if(settings.contains("skipExistingMarquees")) {
    config.skipExistingMarquees = settings.value("skipExistingMarquees").toBool();
  }
  if(settings.contains("skipExistingScreenshots")) {
    config.skipExistingScreenshots = settings.value("skipExistingScreenshots").toBool();
  }
  if(settings.contains("skipExistingVideos")) {
    config.skipExistingVideos = settings.value("skipExistingVideos").toBool();
  }
  if(settings.contains("skipExistingManuals")) {
    config.skipExistingManuals = settings.value("skipExistingManuals").toBool();
  }
  if(settings.contains("skipExistingWheels")) {
    config.skipExistingWheels = settings.value("skipExistingWheels").toBool();
  }
  if(settings.contains("skipExistingTextures")) {
    config.skipExistingTextures = settings.value("skipExistingTextures").toBool();
  }
  if(settings.contains("emulator")) {
    config.frontendExtra = settings.value("emulator").toString();
  }
  if(settings.contains("launch")) {
    config.frontendExtra = settings.value("launch").toString();
  }
  // gamelistFolder kept in addition to gameListFolder for backwards compatibility
  if(settings.contains("gamelistFolder")) {
    config.gameListFolder = settings.value("gamelistFolder").toString();
    gameListFolderSet = true;
  }
  if(settings.contains("gameListFolder")) {
    config.gameListFolder = settings.value("gameListFolder").toString();
    gameListFolderSet = true;
  }
  if(settings.contains("gameListBackup")) {
    config.gameListBackup = settings.value("gameListBackup").toBool();
  }
  if(settings.contains("mediaFolder")) {
    config.mediaFolder = settings.value("mediaFolder").toString();
    mediaFolderSet = true;
  }
  if(settings.contains("mediaFolderHidden") && (config.frontend == "emulationstation" || config.frontend == "retrobat")) {
    config.mediaFolderHidden = settings.value("mediaFolderHidden").toBool();
  }
  if(settings.contains("skipped")) {
    config.skipped = settings.value("skipped").toBool();
  }
  if(settings.contains("brackets")) {
    config.brackets = settings.value("brackets").toBool();
  }
  if(settings.contains("videos")) {
    config.videos = settings.value("videos").toBool();
  }
  if(settings.contains("manuals")) {
    config.manuals = settings.value("manuals").toBool();
  }
  if(settings.contains("exodosPath")) {
    config.exodosPath = settings.value("exodosPath").toString();
  }
  if(settings.contains("gamebasePath")) {
    config.gamebasePath = settings.value("gamebasePath").toString();
  }
  if(settings.contains("guidesPath")) {
    config.guidesPath = settings.value("guidesPath").toString();
  }
  if(settings.contains("guides")) {
    config.guides = settings.value("guides").toBool();
  }
  if(settings.contains("docsPath")) {
    config.docsPath = settings.value("docsPath").toString();
  }
  if(settings.contains("cheats")) {
    config.cheats = settings.value("cheats").toBool();
  }
  if(settings.contains("reviews")) {
    config.reviews = settings.value("reviews").toBool();
  }
  if(settings.contains("artbooks")) {
    config.artbooks = settings.value("artbooks").toBool();
  }
  if(settings.contains("mapsPath")) {
    config.mapsPath = settings.value("mapsPath").toString();
  }
  if(settings.contains("maps")) {
    config.maps = settings.value("maps").toBool();
  }
  if(settings.contains("spritesPath")) {
    config.spritesPath = settings.value("spritesPath").toString();
  }
  if(settings.contains("sprites")) {
    config.sprites = settings.value("sprites").toBool();
  }
  if(settings.contains("chiptunes")) {
    config.chiptunes = settings.value("chiptunes").toBool();
  }
  if(settings.contains("symlink")) {
    config.symlink = settings.value("symlink").toBool();
  }
  if(settings.contains("symlinkImages")) {
    config.symlinkImages = settings.value("symlinkImages").toBool();
  }
  if(settings.contains("theInFront")) {
    config.theInFront = settings.value("theInFront").toBool();
  }
  if(settings.contains("startAt")) {
    config.startAt = settings.value("startAt").toString();
  }
  if(settings.contains("endAt")) {
    config.endAt = settings.value("endAt").toString();
  }
  if(settings.contains("unattend")) {
    config.unattend = settings.value("unattend").toBool();
  }
  if(settings.contains("unattendSkip")) {
    config.unattendSkip = settings.value("unattendSkip").toBool();
    config.unattend = false;
  }
  if(settings.contains("forceFilename")) {
    config.forceFilename = settings.value("forceFilename").toBool();
  }
  if(settings.contains("verbosity")) {
    config.verbosity = settings.value("verbosity").toInt();
  }
  if(settings.contains("maxLength")) {
    config.maxLength = settings.value("maxLength").toInt();
  }
  if(settings.contains("cropBlack")) {
    config.cropBlack = settings.value("cropBlack").toBool();
  }
  settings.endGroup();

  // Scraping module specific configs, overrides main, platform and defaults
  settings.beginGroup(config.scraper);
  if(settings.contains("docTypes")) {
    config.docTypes = settings.value("docTypes").toString().split(" ");
  }
  if(settings.contains("dbPath")) {
    config.dbPath = settings.value("dbPath").toString();
  }
  if(settings.contains("dbServer")) {
    config.dbServer = settings.value("dbServer").toString();
  }
  if(settings.contains("dbName")) {
    config.dbName = settings.value("dbName").toString();
  }
  if(settings.contains("dbUser")) {
    config.dbUser = settings.value("dbUser").toString();
  }
  if(settings.contains("dbPassword")) {
    config.dbPassword = settings.value("dbPassword").toString();
  }
  if(settings.contains("userCreds")) {
    config.userCreds = settings.value("userCreds").toString();
  }
  if(settings.contains("threads")) {
    config.threads = settings.value("threads").toInt();
    config.threadsSet = true;
  }
  if(settings.contains("minMatch")) {
    config.minMatch = settings.value("minMatch").toInt();
    config.minMatchSet = true;
  }
  if(settings.contains("maxLength")) {
    config.maxLength = settings.value("maxLength").toInt();
  }
  if(settings.contains("interactive")) {
    config.interactive = settings.value("interactive").toBool();
  }
  if(settings.contains("unattend")) {
    config.unattend = settings.value("unattend").toBool();
  }
  if(settings.contains("unattendSkip")) {
    config.unattendSkip = settings.value("unattendSkip").toBool();
    config.unattend = false;
  }
  if(settings.contains("jpgQuality")) {
    config.jpgQuality = settings.value("jpgQuality").toInt();
  }
  if(settings.contains("brackets")) {
    config.brackets = settings.value("brackets").toBool();
  }
  if(settings.contains("maxFails") &&
     settings.value("maxFails").toInt() >= 1 &&
     settings.value("maxFails").toInt() <= 1000000) {
    config.maxFails = settings.value("maxFails").toInt();
  }
  if(settings.contains("verbosity")) {
    config.verbosity = settings.value("verbosity").toInt();
  }
  if(settings.contains("getMissingResources")) {
    config.getMissingResources = settings.value("getMissingResources").toBool();
  }
  if(settings.contains("singleImagePerType")) {
    config.singleImagePerType = settings.value("singleImagePerType").toBool();
  }
  if(settings.contains("onlyMissing")) {
    config.onlyMissing = settings.value("onlyMissing").toBool();
  }
  if(settings.contains("cacheRefresh") && !config.rescan) {
    config.refresh = settings.value("cacheRefresh").toBool();
  }
  if(settings.contains("ignoreNegativeCache")) {
    config.ignoreNegCache = settings.value("ignoreNegativeCache").toBool();
  }
  if(settings.contains("cacheResize")) {
    config.cacheResize = settings.value("cacheResize").toBool();
  }
  if(settings.contains("cacheCovers")) {
    config.cacheCovers = settings.value("cacheCovers").toBool();
  }
  if(settings.contains("cacheTextures")) {
    config.cacheTextures = settings.value("cacheTextures").toBool();
  }
  if(settings.contains("cacheScreenshots")) {
    config.cacheScreenshots = settings.value("cacheScreenshots").toBool();
  }
  if(settings.contains("cacheWheels")) {
    config.cacheWheels = settings.value("cacheWheels").toBool();
  }
  if(settings.contains("cacheMarquees")) {
    config.cacheMarquees = settings.value("cacheMarquees").toBool();
  }
  if(settings.contains("chiptunes")) {
    config.chiptunes = settings.value("chiptunes").toBool();
  }
  if(settings.contains("guides")) {
    config.guides = settings.value("guides").toBool();
  }
  if(settings.contains("cheats")) {
    config.cheats = settings.value("cheats").toBool();
  }
  if(settings.contains("reviews")) {
    config.reviews = settings.value("reviews").toBool();
  }
  if(settings.contains("artbooks")) {
    config.artbooks = settings.value("artbooks").toBool();
  }
  if(settings.contains("maps")) {
    config.maps = settings.value("maps").toBool();
  }
  if(settings.contains("sprites")) {
    config.sprites = settings.value("sprites").toBool();
  }
  if(settings.contains("manuals")) {
    config.manuals = settings.value("manuals").toBool();
  }
  if(settings.contains("manualSizeLimit")) {
    config.manualSizeLimit = settings.value("manualSizeLimit").toInt() * 1000 * 1000;
  }
  if(settings.contains("videos")) {
    config.videos = settings.value("videos").toBool();
  }
  if(settings.contains("videoSizeLimit")) {
    config.videoSizeLimit = settings.value("videoSizeLimit").toInt() * 1000 * 1000;
  }
  if(settings.contains("videoConvertCommand")) {
    config.videoConvertCommand = settings.value("videoConvertCommand").toString();
  }
  if(settings.contains("videoConvertExtension")) {
    config.videoConvertExtension = settings.value("videoConvertExtension").toString();
  }
  if(settings.contains("videoPreferNormalized")) {
    config.videoPreferNormalized = settings.value("videoPreferNormalized").toBool();
  }
  if(settings.contains("negCacheDaysExpiration")) {
    config.negCacheDaysExpiration = settings.value("negCacheDaysExpiration").toInt();
    config.negCacheExpiration = QDateTime::currentMSecsSinceEpoch() / 1000
                                 - config.negCacheDaysExpiration * 24 * 60 * 60;
  }
  if(settings.contains("fuzzySearch")) {
    config.fuzzySearch = settings.value("fuzzySearch").toInt();
  }
  if(settings.contains("useChecksum")) {
    config.useChecksum = settings.value("useChecksum").toBool();
  }
  if(settings.contains("keepSubtitle")) {
    config.keepSubtitle = settings.value("keepSubtitle").toBool();
  }
  settings.endGroup();

  // Command line configs, overrides main, platform, module and defaults
  if(parser.isSet("verbosity")) {
    config.verbosity = parser.value("verbosity").toInt();
  }
  if(parser.isSet("l") && parser.value("l").toInt() >= 0 && parser.value("l").toInt() <= 100000) {
    config.maxLength = parser.value("l").toInt();
  }
  if(parser.isSet("t") && parser.value("t").toInt() <= 8) {
    config.threads = parser.value("t").toInt();
    config.threadsSet = true;
  }
  if(parser.isSet("e")) {
    config.frontendExtra = parser.value("e");
  }
  if(parser.isSet("i")) {
    config.inputFolder = parser.value("i");
    inputFolderSet = true;
  }
  if(parser.isSet("g")) {
    config.gameListFolder = parser.value("g");
    gameListFolderSet = true;
  }
  if(parser.isSet("o")) {
    config.mediaFolder = parser.value("o");
    mediaFolderSet = true;
  }
  if(parser.isSet("a")) {
    config.artworkConfig = parser.value("a");
  }
  if(parser.isSet("m") && parser.value("m").toInt() >= 0 && parser.value("m").toInt() <= 100) {
    config.minMatch = parser.value("m").toInt();
    config.minMatchSet = true;
  }
  if(parser.isSet("u")) {
    config.userCreds = parser.value("u");
  }
  if(parser.isSet("d")) {
    config.cacheFolder = parser.value("d");
  } else {
    if(config.cacheFolder.isEmpty())
      config.cacheFolder = "cache/" + config.platform;
  }
  if(parser.isSet("doctypes")) {
    if(parser.value("doctypes").isEmpty()) {
      printf("Please indicate at least one document type.\n");
      removeLockAndExit(1);
    } else {
      config.docTypes = parser.value("doctypes").split(" ");
    }
  }
  if(parser.isSet("flags")) {
    if(parser.value("flags") == "help") {
      printf("Showing '\033[1;33m--flags\033[0m' help\n");
      printf("Use comma-separated flags (eg. '--flags FLAG1,FLAG2') to enable multiple flags.\nThe following is a list of valid flags and what they do:\n");

      printf("  \033[1;33martbooks\033[0m: Enables management of artbooks for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33mcheats\033[0m: Enables management of cheats for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33mchiptunes\033[0m: Enables management of chiptunes for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33mforcefilename\033[0m: Use filename as game name instead of the returned game title when generating a game list. Consider using 'nameTemplate' config.ini option instead.\n");
      printf("  \033[1;33mgetmissingresources\033[0m: If a game is already in the cache, has some missing multimedia resources for the scraper service, and refresh is not set, then check and download the missing multimedia resources if they are now available, and refresh the non-multimedia (text) resources.\n");
      printf("  \033[1;33mguides\033[0m: Enables management of guides for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33minteractive\033[0m: Always ask user to choose best returned result from the scraping modules.\n");
      printf("  \033[1;33mmanuals\033[0m: Enables scraping and caching of manuals for the scraping modules that support them. Beware, this takes up a lot of disk space!.\n");
      printf("  \033[1;33mmaps\033[0m: Enables management of game maps for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33mnobrackets\033[0m: Disables any [] and () tags in the frontend game titles. Consider using 'nameTemplate' config.ini option instead.\n");
      printf("  \033[1;33mnocovers\033[0m: Disable covers/boxart from being cached locally. Only do this if you do not plan to use the cover artwork in 'artwork.xml'\n");
      printf("  \033[1;33mnocropblack\033[0m: Disables cropping away black borders around screenshot resources when compositing the final gamelist artwork.\n");
      printf("  \033[1;33mnohints\033[0m: Disables the 'DID YOU KNOW:' hints when running Skyscraper.\n");
      printf("  \033[1;33mnomarquees\033[0m: Disable marquees from being cached locally. Only do this if you do not plan to use the marquee artwork in 'artwork.xml'\n");
      printf("  \033[1;33mnoresize\033[0m: Disable resizing of artwork when saving it to the resource cache. Normally they are resized to save space.\n");
      printf("  \033[1;33mnoscreenshots\033[0m: Disable screenshots/snaps from being cached locally. Only do this if you do not plan to use the screenshot artwork in 'artwork.xml'\n");
      printf("  \033[1;33mnosubdirs\033[0m: Do not include input folder subdirectories when scraping.\n");
      printf("  \033[1;33mnotextures\033[0m: Disable textures from being cached locally. Only do this if you do not plan to use the texture artwork in 'artwork.xml'\n");
      printf("  \033[1;33mnowheels\033[0m: Disable wheels from being cached locally. Only do this if you do not plan to use the wheel artwork in 'artwork.xml'\n");
      printf("  \033[1;33monlymissing\033[0m: Tells Skyscraper to skip when scraping all files which already have any data from any source in the cache.\n");
      printf("  \033[1;33mpretend\033[0m: Only relevant when generating a game list, populating the canonical database or validating the cache. It disables the game list generator and artwork compositor and only outputs the results of the potential game list generation to the terminal. Use it to check what and how the data will be combined from cached resources.\n");
      printf("  \033[1;33mrelative\033[0m: Forces all gamelist paths to be relative to rom location.\n");
      printf("  \033[1;33mremovesubtitle\033[0m: Remove subtitles/second names of the file name (default is to keep them).\n");
      printf("  \033[1;33mreviews\033[0m: Enables management of reviews for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33msingleimagepertype\033[0m: Retrieve only one picture per image type, that is, if a scraper already has a picture for a given type and game, no other scraper will attempt to retrieve it (same behaviour as for videos and manuals) (default is to allow multiple pictures across scrapers).\n");
      printf("  \033[1;33mskipchecksum\033[0m: When validating the cache, do not regenerate the MD5/CRC/SHA1 checksums.\n");
      printf("  \033[1;33mskipexistingcovers\033[0m: When generating gamelists, skip processing covers that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingmanuals\033[0m: When generating gamelists, skip copying manuals that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingmarquees\033[0m: When generating gamelists, skip processing marquees that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingscreenshots\033[0m: When generating gamelists, skip processing screenshots that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingtextures\033[0m: When generating gamelists, skip processing textures, covers, disc art that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingvideos\033[0m: When generating gamelists, skip copying videos that already exist in the media output folder.\n");
      printf("  \033[1;33mskipexistingwheels\033[0m: When generating gamelists, skip processing wheels that already exist in the media output folder.\n");
      printf("  \033[1;33mskipped\033[0m: When generating a gamelist, also include games that do not have any cached data.\n");
      printf("  \033[1;33msprites\033[0m: Enables management of game sprites for the scraping modules and frontends that support them.\n");
      printf("  \033[1;33msymlink\033[0m: Forces cached videos and manuals to be symlinked to game list destination to save space. WARNING! Deleting or moving files from your cache can invalidate the links! (default is not symlinking).\n");
      printf("  \033[1;33msymlinkimages\033[0m: Forces cached images to be symlinked to game list destination to save space. WARNING! Deleting or moving files from your cache can invalidate the links! (default is not symlinking).\n");
      printf("  \033[1;33mtheinfront\033[0m: Forces Skyscraper to always try and move 'The' to the beginning of the game title when generating gamelists. By default 'The' will be moved to the end of the game titles.\n");
      printf("  \033[1;33munattend\033[0m: Skip initial questions when scraping. It will then always overwrite existing gamelist and not skip existing entries.\n");
      printf("  \033[1;33munattendskip\033[0m: Skip initial questions when scraping. It will then always overwrite existing gamelist and always skip existing entries. Overrides 'unattend' flag.\n");
      printf("  \033[1;33musechecksum\033[0m: Activates checksum (MD5SUM/SHA1) search instead of filename based search for compatible scrapers.\n");
      printf("  \033[1;33mvideos\033[0m: Enables scraping and caching of videos for the scraping modules that support them. Beware, this takes up a lot of disk space!.\n");
      printf("  \033[1;33mwaitifconcurrent\033[0m: Stops execution of the scraping if another instance of Skyscraper is scraping the same platform.\n");
      printf("\n");
      removeLockAndExit(0);
    } else {
      const auto flags = parser.value("flags").split(",");
      for(const auto &flag: std::as_const(flags)) {
        if(flag == "usechecksum") {
          config.useChecksum = true;
        } else if(flag == "removesubtitle") {
          config.keepSubtitle = false;
        } else if(flag == "forcefilename") {
          config.forceFilename = true;
        } else if(flag == "interactive") {
          config.interactive = true;
        } else if(flag == "nobrackets") {
          config.brackets = false;
        } else if(flag == "getmissingresources") {
          config.getMissingResources = true;
        } else if(flag == "singleimagepertype") {
          config.singleImagePerType = true;
        } else if(flag == "nocovers") {
          config.cacheCovers = false;
        } else if(flag == "notextures") {
          config.cacheTextures = false;
        } else if(flag == "nocropblack") {
          config.cropBlack = false;
        } else if(flag == "nohints") {
          config.hints = false;
        } else if(flag == "waitifconcurrent") {
          config.waitIfConcurrent = true;
        } else if(flag == "nomarquees") {
          config.cacheMarquees = false;
        } else if(flag == "noresize") {
          config.cacheResize = false;
        } else if(flag == "noscreenshots") {
          config.cacheScreenshots = false;
        } else if(flag == "nosubdirs") {
          config.subdirs = false;
        } else if(flag == "nowheels") {
          config.cacheWheels = false;
        } else if(flag == "onlymissing") {
          config.onlyMissing = true;
        } else if(flag == "pretend") {
          config.pretend = true;
        } else if(flag == "relative") {
          config.relativePaths = true;
        } else if(flag == "skipchecksum") {
          config.skipChecksum = true;
        } else if(flag == "skipexistingcovers") {
          config.skipExistingCovers = true;
        } else if(flag == "skipexistingmarquees") {
          config.skipExistingMarquees = true;
        } else if(flag == "skipexistingscreenshots") {
          config.skipExistingScreenshots = true;
        } else if(flag == "skipexistingvideos") {
          config.skipExistingVideos = true;
        } else if(flag == "skipexistingmanuals") {
          config.skipExistingManuals = true;
        } else if(flag == "skipexistingwheels") {
          config.skipExistingWheels = true;
        } else if(flag == "skipexistingtextures") {
          config.skipExistingTextures = true;
        } else if(flag == "skipped") {
          config.skipped = true;
        } else if(flag == "symlink") {
          config.symlink = true;
        } else if(flag == "symlinkimages") {
          config.symlinkImages = true;
        } else if(flag == "theinfront") {
          config.theInFront = true;
        } else if(flag == "unattend") {
          config.unattend = true;
          config.unattendSkip = false;
        } else if(flag == "unattendskip") {
          config.unattendSkip = true;
          config.unattend = false;
        } else if(flag == "videos") {
          config.videos = true;
        } else if(flag == "manuals") {
          config.manuals = true;
        } else if(flag == "guides") {
          config.guides = true;
        } else if(flag == "cheats") {
          config.cheats = true;
        } else if(flag == "reviews") {
          config.reviews = true;
        } else if(flag == "artbooks") {
          config.artbooks = true;
        } else if(flag == "maps") {
          config.maps = true;
        } else if(flag == "sprites") {
          config.sprites = true;
        } else if(flag == "chiptunes") {
          config.chiptunes = true;
        } else {
          printf("Unknown flag '%s', please check '--flags help' for a list of valid "
                 "flags. Exiting...\n",
                 flag.toStdString().c_str());
          removeLockAndExit(1);
        }
      }
    }
  }
  if(config.useChecksum && config.scraper != "screenscraper") {
    if(config.scraper == "esgamelist"  || config.scraper == "import" ||
       config.scraper == "customflags" || config.scraper == "cache") {
      printf("WARNING: Checksum matching requested, but the scraper does not "
             "allow it. Exiting.\n");
      config.useChecksum = false;
      removeLockAndExit(1);
    } else if(Platform::get().getFamily(config.platform) == "arcade") {
      printf("WARNING: Checksum matching requested, but the platform uses "
             "MAME-compliant short names, rendering checksums unnecessary. "
             "Exiting.\n");
      config.useChecksum = false;
      removeLockAndExit(1);
    } else {
      printf("INFO: Checksum matching requested, but scraper is not compatible. "
             "Using hybrid filename matching using canonical checksum database.\n");
    }
  }
  if(parser.isSet("addext")) {
    config.addExtensions = parser.value("addext");
  }
  if(parser.isSet("generatelbdb")) {
    config.threads = 1;
    config.generateLbDb = true;
    if(config.scraper != "launchbox") {
      printf("Launchbox SQLite DB generation requested, but the scraper is not 'launchbox'. Exiting.\n");
      Skyscraper::removeLockAndExit(1);
    }
  }
  if(parser.isSet("loadchecksum")) {
    // When loading checksums, nothing else is needed:
    QString checksumFile = parser.value("loadchecksum");
    if(checksumFile == "LUTRISDB" || QFile::exists(checksumFile)) {
      printf("Loading canonical data from DAT/XML/DB file '%s'...\n",
             checksumFile.toStdString().c_str());
      NameTools *NameTool = new NameTools("main");
      int exitStatus = NameTool->importCanonicalData(checksumFile);
      delete NameTool;
      exit(exitStatus);
    } else {
      printf("ERROR: Checksum DAT/XML file does not exist. Please enter the full path. Exiting.\n");
      exit(1);
    }
  }
  if(parser.isSet("cachegb")) {
    config.cacheGb = true;
    config.threads = 3;
    config.threadsSet = true;
    if(config.scraper != "giantbomb") {
      printf("GiantBomb cache refresh requested, but the scraper is not 'giantbomb'. Exiting.\n");
      removeLockAndExit(1);
    }
    if(config.platform.isEmpty()) {
      printf("GiantBomb cache refresh requested, but no platform has been selected. Exiting.\n");
      removeLockAndExit(1);
    }
  }
  if(parser.isSet("refresh") && !config.rescan) {
    config.refresh = true;
  }
  if(parser.isSet("rescan") && config.scraper != "customflags") {
    config.rescan = true;
    config.refresh = false;
  }
  if(parser.isSet("fuzzysearch")) {
    config.fuzzySearch = parser.value("fuzzysearch").toInt();
  }
  if(parser.isSet("cache")) {
    config.cacheOptions = parser.value("cache");
    if(config.cacheOptions == "refresh" && !config.rescan) {
      config.refresh = true;
    } else if(config.cacheOptions == "rescan" && config.scraper != "customflags") {
      config.rescan = true;
      config.refresh = false;
    } else if(config.cacheOptions == "help") {
      printf("Showing '\033[1;33m--cache\033[0m' help\n");
      printf("  \033[1;33m--cache edit\033[0m: Let's you edit resources for the selected platform for all files or a range of files. Add a filename on command line to edit cached resources for just that one file, use '--includefrom' to edit files created with the '--cache report' option or use '--startat' and '--endat' to edit a range of roms.\n");
      printf("  \033[1;33m--cache edit:new=<TYPE>\033[0m: Let's you batch add resources of <TYPE> to the selected platform for all files or a range of files. Add a filename on command line to edit cached resources for just that one file, use '--includefrom' to edit files created with the '--cache report' option or use '--startat' and '--endat' to edit a range of roms.\n");
      printf("  \033[1;33m--cache ignorenegativecache\033[0m: Switches off the negative functionality (both for queries and updates). This is the default for offline scrapers.\n");
      printf("  \033[1;33m--cache merge:<PATH>\033[0m: Merges two resource caches together. It will merge the resource cache specified by <PATH> into the local resource cache by default. To merge into a non-default destination cache folder set it with '-d <PATH>'. Both should point to folders with the 'db.xml' inside.\n");
      printf("  \033[1;33m--cache purge:all\033[0m: Removes ALL cached resources for the selected platform.\n");
      printf("  \033[1;33m--cache purge:m=<MODULE>,t=<TYPE>\033[0m: Removes cached resources related to the selected module(m) and / or type(t). Either one can be left out in which case ALL resources from the selected module or ALL resources from the selected type will be removed.\n");
      printf("  \033[1;33m--cache refresh\033[0m: Forces a refresh of existing cached resources for any scraping module. Requires a scraping module set with '-s'. Same as '--refresh'. Incompatible with --rescan.\n");
      printf("  \033[1;33m--cache report:missing=<OPTION>\033[0m: Generates reports with all files that are missing the specified resources. Check '--cache report:missing=help' for more info.\n");
      printf("  \033[1;33m--cache rescan\033[0m: Executes the scraping process for entries with existing resources, and validates that the scraper provides the same information as in the cache database. Deletes/refreshes it otherwise. Requires a scraping module set with '-s'. Same as '--rescan'. Incompatible with (and overrides) --refresh.\n");
      printf("  \033[1;33m--cache scrapingerrors\033[0m: Compares your romset to the scraped titles and identify possible false positives.\n");
      printf("  \033[1;33m--cache show\033[0m: Prints a status of all cached resources for the selected platform.\n");
      printf("  \033[1;33m--cache vacuum\033[0m: Compares your romset to any cached resource and removes the resources that you no longer have roms for.\n");
      printf("  \033[1;33m--cache validate\033[0m: Checks the consistency of the cache for the selected platform.\n");
      printf("\n");
      removeLockAndExit(0);
    } else if(!(config.cacheOptions == "edit" ||
                config.cacheOptions.startsWith("edit:") ||
                config.cacheOptions.startsWith("merge:") ||
                config.cacheOptions.startsWith("purge:") ||
                config.cacheOptions.startsWith("report:") ||
                config.cacheOptions == "ignorenegativecache" ||
                config.cacheOptions == "scrapingerrors" ||
                config.cacheOptions == "show" ||
                config.cacheOptions == "vacuum" ||
                config.cacheOptions == "validate")) {
      printf("Unknown cache option '%s', please check '--cache help' for a list of valid "
             "flags. Exiting...\n",
             config.cacheOptions.toStdString().c_str());
      removeLockAndExit(1);
    }
  }
  if(parser.isSet("startat")) {
    config.startAt = parser.value("startat");
  }
  if(parser.isSet("endat")) {
    config.endAt = parser.value("endat");
  }
  if(parser.isSet("includefiles")) { // This option is DEPRECATDE, use includepattern
    config.includePattern = parser.value("includefiles");
  }
  if(parser.isSet("includepattern")) {
    config.includePattern = parser.value("includepattern");
  }
  if(parser.isSet("excludefiles")) {  // This option is DEPRECATED, use excludepattern
    config.excludePattern = parser.value("excludefiles");
  }
  if(parser.isSet("excludepattern")) {
    config.excludePattern = parser.value("excludepattern");
  }
  if(parser.isSet("includefrom")) {
    config.includeFrom = parser.value("includefrom");
  }
  if(parser.isSet("excludefrom")) {
    config.excludeFrom = parser.value("excludefrom");
  }
  if(parser.isSet("maxfails") &&
     parser.value("maxfails").toInt() >= 1 &&
     parser.value("maxfails").toInt() <= 200) {
    config.maxFails = parser.value("maxfails").toInt();
  }
  if(parser.isSet("region")) {
    config.region = parser.value("region");
  }
  if(parser.isSet("lang")) {
    config.lang = parser.value("lang");
  }

  // Choose default scraper for chosen platform if none has been set yet
  if(config.scraper.isEmpty() && !parser.isSet("loadchecksum")) {
    config.scraper = Platform::get().getDefaultScraper();
    setLock();
  }

  // If platform subfolder exists for import path, use it
  if(!config.importFolder.isEmpty()) {
    QDir importFolder(config.importFolder);
    if(importFolder.exists(config.platform)) {
      config.importFolder.append((config.importFolder.right(1) == "/"?"":"/") + config.platform);
    }
  }

  // Set minMatch to 0 for cache, arcadedb, mamehistory, import and esgamelist.
  // We know these results are always accurate
  if((config.scraper == "cache"  || config.scraper == "esgamelist"   ||
      config.scraper == "import" || config.scraper == "arcadedb" ||
      config.scraper == "mamehistory")) {
    config.minMatch = 0;
  }

  skippedFileString = "skipped-" + config.platform + "-" + config.scraper + ".txt";

  // Grab all requested files from cli, if any
  QStringList requestedFiles = parser.positionalArguments();

  // Add files from '--includefrom', if any
  if(!config.includeFrom.isEmpty()) {
    QFileInfo includeFromInfo(config.includeFrom);
    if(!includeFromInfo.exists()) {
      includeFromInfo.setFile(config.currentDir + "/" + config.includeFrom);
    }
    if(includeFromInfo.exists()) {
      QFile includeFrom(includeFromInfo.absoluteFilePath());
      if(includeFrom.open(QIODevice::ReadOnly)) {
        while(!includeFrom.atEnd()) {
          requestedFiles.append(QString(includeFrom.readLine().simplified()));
        }
        includeFrom.close();
      }
    } else {
      printf("File: '\033[1;32m%s\033[0m' does not exist.\n\nPlease verify the "
             "filename and try again...\n",
             includeFromInfo.absoluteFilePath().toStdString().c_str());
      removeLockAndExit(1);
    }
  }

  // Verify requested files and add the ones that exist
  for(const auto &requestedFile: std::as_const(requestedFiles)) {
    QFileInfo requestedFileInfo(requestedFile);
    if(!requestedFileInfo.exists()) {
      requestedFileInfo.setFile(config.currentDir + "/" + requestedFile);
    }
    if(!requestedFileInfo.exists()) {
      requestedFileInfo.setFile(config.inputFolder + "/" + requestedFile);
    }
    if(requestedFileInfo.exists()) {
      cliFiles.append(requestedFileInfo.absoluteFilePath());
      // Always set refresh or rescan and unattend true if user has supplied filenames on
      // command line. That way they are cached, but game list is not changed and user isn't
      // asked about skipping and overwriting.
      if(!config.rescan) {
        config.refresh = true;
      }
      if(!config.unattendSkip) {
        config.unattend = true;
      }
    } else {
      printf("Filename: '\033[1;32m%s\033[0m' requested either on command line or with "
             "'--includefrom' not found!\n\nPlease verify the filename and try again...\n",
             requestedFile.toStdString().c_str());
      removeLockAndExit(1);
    }
  }

  // Add query only if a single filename was passed on command line
  if(parser.isSet("query")) {
    if(cliFiles.length() == 1) {
      config.searchName = parser.value("query");
    } else {
      printf("'--query' requires a single rom filename to be added at the end of "
             "the command-line. You either forgot to set one, or more than one was "
             "provided. Now quitting...\n");
      removeLockAndExit(1);
    }
  }

  if(config.startAt != "" || config.endAt != "") {
    // config.refresh = true;
    if(!config.unattendSkip) {
      config.unattend = true;
    }
    // config.subdirs = false;
  }

  // If interactive is set, force 1 thread and always accept the chosen result
  if(config.interactive) {
    if(config.scraper == "cache" ||
       config.scraper == "import"  ||
       config.scraper == "arcadedb"  ||
       config.scraper == "esgamelist"  ||
       config.scraper == "mamehistory" ||
       config.scraper == "customflags") {
      config.interactive = false;
    } else {
      config.threads = 1;
      config.minMatch = 0;
      if(!config.rescan) {
        config.refresh = true;
      }
    }
  }

  if(config.scraper == "import") {
    // Always force the cache to be refreshed when using import scraper
    config.refresh = true;
    config.rescan = false;
    config.videos = true;
    config.manuals = true;
    config.guides = true;
    config.reviews = true;
    config.artbooks = true;
    config.cheats = true;
    config.maps = true;
    config.sprites = true;
    config.chiptunes = true;
    // minMatch set to 0 further up
  }

  if(config.rescan) {
    config.useChecksum = false;
  }

  if(!config.userCreds.isEmpty() && config.userCreds.contains(":")) {
    QStringList userCreds = config.userCreds.split(":");
    if(userCreds.length() == 2) {
      config.user = userCreds.at(0);
      config.password = userCreds.at(1);
    } else if(userCreds.length() == 3) {
      config.user = userCreds.at(0);
      config.password = userCreds.at(1);
      config.apiKey = userCreds.at(2);
    }
  }

  QFile artworkFile(config.artworkConfig);
  if(artworkFile.open(QIODevice::ReadOnly)) {
    config.artworkXml = artworkFile.readAll();
    artworkFile.close();
  } else {
    printf("Couldn't read artwork xml file '\033[1;32m%s\033[0m'. Please check file "
           "and permissions. Now exiting...\n",
           config.artworkConfig.toStdString().c_str());
    removeLockAndExit(1);
  }

  QDir resDir("./resources");
  QDirIterator resDirIt(resDir.absolutePath(),
                        QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
  while(resDirIt.hasNext()) {
    QString resFile = resDirIt.next();
    resFile = resFile.remove(0, resFile.indexOf("resources/") + 10); // Also cut off 'resources/'
    config.resources[resFile] = QImage("resources/" + resFile);
  }

  // Always create a frontend, in case it is needed
  if(config.frontend == "emulationstation") {
    frontend = new EmulationStation;
  } else if(config.frontend == "retrobat") {
    frontend = new RetroBat;
  } else if(config.frontend == "attractmode") {
    frontend = new AttractMode;
  } else if(config.frontend == "pegasus") {
    frontend = new Pegasus;
  } else if(config.frontend == "xmlexport") {
    frontend = new XmlExport;
  } else if(config.frontend == "koillection") {
    frontend = new Koillection(manager, &config);
  } else {
    printf("Frontend: '\033[1;32m%s\033[0m' is not supported. Please select a valid one.\n",
           config.frontend.toStdString().c_str());
    removeLockAndExit(1);
  }

  frontend->setConfig(&config);
  frontend->checkReqs();

  // Change config defaults if they aren't already set, find the rest in settings.h
  if(!inputFolderSet) {
    config.inputFolder = frontend->getInputFolder();
  }
  if(!gameListFolderSet) {
    config.gameListFolder = frontend->getGameListFolder();
  }
  if(!mediaFolderSet) {
    config.mediaFolder = config.gameListFolder + "/" + (config.mediaFolderHidden?".":"") + "media";
  }
  config.coversFolder = frontend->getCoversFolder();
  config.screenshotsFolder = frontend->getScreenshotsFolder();
  config.wheelsFolder = frontend->getWheelsFolder();
  config.marqueesFolder = frontend->getMarqueesFolder();
  config.texturesFolder = frontend->getTexturesFolder();
  config.videosFolder = frontend->getVideosFolder();
  config.manualsFolder = frontend->getManualsFolder();
}

void Skyscraper::copyFile(const QString &distro, const QString &current, bool overwrite)
{
  if(QFileInfo::exists(distro)) {
    if(QFileInfo::exists(current)) {
      if(overwrite) {
        QFile::remove(current);
        QFile::copy(distro, current);
      }
    } else {
      QFile::copy(distro, current);
    }
  }
}

void Skyscraper::showHint()
{
  QFile hintsFile("hints.xml");
  QDomDocument hintsXml;
  if(!hintsFile.open(QIODevice::ReadOnly)) {
    return;
  }
  if(!hintsXml.setContent(&hintsFile)) {
    return;
  }
  hintsFile.close();
  QDomNodeList hintNodes = hintsXml.elementsByTagName("hint");
  printf("\033[1;33mDID YOU KNOW:\033[0m %s\n\n",
         hintsXml.elementsByTagName("hint").at(QRandomGenerator::global()->generate() %
         hintNodes.length()).toElement().text().toStdString().c_str());
}

void Skyscraper::doPrescrapeJobs()
{
  loadAliasMap();
  loadMameMap();
  loadWhdLoadMap();

  setRegionPrios();
  setLangPrios();

  NetComm netComm(manager);
  QEventLoop q; // Event loop for use when waiting for data from NetComm.
  connect(&netComm, &NetComm::dataReady, &q, &QEventLoop::quit);

  if(config.platform == "amiga" &&
     config.scraper != "cache" && config.scraper != "import" && config.scraper != "esgamelist") {
    printf("Fetching 'whdload_db.xml', just a sec..."); fflush(stdout);
    netComm.request("https://raw.githubusercontent.com/HoraceAndTheSpider/Amiberry-XML-Builder/master/whdload_db.xml");
    q.exec();
    QByteArray data = netComm.getData();
    QDomDocument tempDoc;
    QFile whdLoadFile("whdload_db.xml");
    if(data.size() > 1000000 &&
       tempDoc.setContent(data) &&
       whdLoadFile.open(QIODevice::WriteOnly)) {
      whdLoadFile.write(data);
      whdLoadFile.close();
      printf("\033[1;32m Success!\033[0m\n\n");
    } else {
      printf("\033[1;31m Failed!\033[0m\n\n");
    }
  }

  if(config.scraper == "arcadedb" && config.threads != 1) {
    printf("\033[1;33mForcing 1 thread to accomodate limits in the ArcadeDB API\033[0m\n\n");
    config.threads = 1; // Don't change! This limit was set by request from ArcadeDB
  } else if(config.scraper == "openretro" && config.threads != 1) {
    printf("\033[1;33mForcing 1 thread to accomodate limits in the OpenRetro API\033[0m\n\n");
    config.threads = 1; // Don't change! This limit was set by request from OpenRetro
  } else if(config.scraper == "giantbomb") {
    if(config.threads != 1 && !config.cacheGb) {
      printf("\033[1;33mForcing 1 thread to accomodate limits in the GiantBomb API\033[0m\n\n");
      config.threads = 1; // Don't change! This limit was set by request from GiantBomb
    }
    if(config.user.isEmpty() || config.password.isEmpty() || config.apiKey.isEmpty()) {
      printf("The GiantBomb scraping module requires user credentials and an API token to "
             "work. Get one here: 'https://www.giantbomb.com/api/'\n");
      removeLockAndExit(1);
    }
  } else if(config.scraper == "igdb") {
    if(config.threads > 4) {
      printf("\033[1;33mAdjusting to 4 threads to accomodate limits in the IGDB API\033[0m\n\n");
      printf("\033[1;32mThis module is powered by IGDB.com\033[0m\n");
      config.threads = 4; // Don't change! This limit was set by request from IGDB
    }
    if(config.user.isEmpty() || config.password.isEmpty()) {
      printf("The IGDB scraping module requires free user credentials to work. Read more about that here: "
             "'https://github.com/detain/skyscraper/blob/master/docs/SCRAPINGMODULES.md#igdb'\n");
      removeLockAndExit(1);
    }
    printf("Fetching IGDB authentication token status, just a sec...\n");
    QFile tokenFile("igdbToken.dat");
    QByteArray tokenData = "";
    if(tokenFile.exists() && tokenFile.open(QIODevice::ReadOnly)) {
      tokenData = tokenFile.readAll().trimmed();
      tokenFile.close();
    }
    if(tokenData.split(';').length() != 3) {
      tokenData = "user;token;0";
    }
    bool updateToken = false;
    if(config.user != tokenData.split(';').at(0)) {
      updateToken = true;
    }
    qlonglong tokenLife = tokenData.split(';').at(2).toLongLong() - (QDateTime::currentMSecsSinceEpoch() / 1000);
    if(tokenLife < 60 * 60 * 24 * 2) { // 2 days, should be plenty for a scraping run
      updateToken = true;
    }
    config.igdbToken = tokenData.split(';').at(1);
    if(updateToken) {
      netComm.request("https://id.twitch.tv/oauth2/token"
                      "?client_id=" + config.user +
                      "&client_secret=" + config.password +
                      "&grant_type=client_credentials", "");
      q.exec();
      QJsonObject jsonObj = QJsonDocument::fromJson(netComm.getData()).object();
      if(jsonObj.contains("access_token") &&
         jsonObj.contains("expires_in") &&
         jsonObj.contains("token_type")) {
        config.igdbToken = jsonObj["access_token"].toString();
        printf("Token '%s' acquired, ready to scrape!\n", config.igdbToken.toStdString().c_str());
        tokenLife = (QDateTime::currentMSecsSinceEpoch() / 1000) + jsonObj["expires_in"].toInt();
        if(tokenFile.open(QIODevice::WriteOnly)) {
          tokenFile.write(config.user.toUtf8() + ";" + config.igdbToken.toUtf8() + ";" + QByteArray::number(tokenLife));
          tokenFile.close();
        }
      } else {
        printf("\033[1;33mReceived invalid IGDB server response. This can be caused "
               "by server issues or maybe you entered your credentials incorrectly in "
               "the Skyscraper configuration. Read more about that here: "
               "'https://github.com/detain/skyscraper/blob/master/docs/SCRAPINGMODULES.md#igdb'\033[0m\n");
        removeLockAndExit(1);
      }
    } else {
      printf("Cached token '%s' still valid, ready to scrape!\n", config.igdbToken.toStdString().c_str());
    }
    printf("\n");
  } else if(config.scraper.contains("mobygames") && config.threads != 1) {
    printf("\033[1;33mForcing 1 thread to accomodate limits in MobyGames scraping module. "
           "Also be aware that MobyGames has a request limit of 360 requests per hour for "
           "the entire Skyscraper user base. So if someone else is currently using it, it "
           "will quit.\033[0m\n\n");
    config.threads = 1; // Don't change! This limit was set by request from Mobygames
    config.romLimit = 35; // Don't change! This limit was set by request from Mobygames
  } else if(config.scraper == "screenscraper") {
    if(config.apiKey.isEmpty()) {
      printf("\033[1;33mThe Screenscraper service requires an API key, which is missing in "
             "the third field of userCreds. Please complete it.\033[0m\n");
    }
    if(config.user.isEmpty() || config.password.isEmpty()) {
      if(config.threads > 1) {
        printf("\033[1;33mForcing 1 threads as this is the anonymous limit in the ScreenScraper "
               "scraping module. Sign up for an account at https://www.screenscraper.fr and support "
               "them to gain more threads. Then use the credentials with Skyscraper using the "
               "'-u user:password' command line option or by setting 'userCreds=\"user:password\"' "
               "in '%s/config.ini'.\033[0m\n\n",
               QDir::currentPath().toStdString().c_str());
        config.threads = 1; // Don't change! This limit was set by request from ScreenScraper
      }
    } else {
      printf("Fetching limits for user '\033[1;33m%s\033[0m', just a sec...\n", config.user.toStdString().c_str());
      netComm.request("https://www.screenscraper.fr/api2/ssuserInfos.php?devid=muldjord&devpassword=" +
                      config.apiKey + "&softname=skyscraper" VERSION "&output=json&ssid=" +
                      config.user + "&sspassword=" + config.password);
      q.exec();
      QJsonObject jsonObj = QJsonDocument::fromJson(netComm.getData()).object();
      if(jsonObj.isEmpty()) {
        if(netComm.getData().contains("Erreur de login")) {
          printf("\033[0;31mScreenScraper login error! Please verify that you've entered "
                 "your credentials correctly in '%s/config.ini'. It needs to "
                 "look EXACTLY like this, but with your USER and PASS:\033[0m\n\033[1;33m"
                 "[screenscraper]\nuserCreds=\"USER:PASS\"\033[0m\033[0;31m\n"
                 "Continuing with unregistered user, forcing 1 thread...\033[0m\n\n",
                 QDir::currentPath().toStdString().c_str());
        } else {
          printf("\033[1;33mReceived invalid / empty ScreenScraper server response, maybe "
                 "their server is busy / overloaded. Forcing 1 thread...\033[0m\n\n");
        }
        config.threads = 1; // Don't change! This limit was set by request from ScreenScraper
      } else {
        int allowedThreads = jsonObj["response"].toObject()["ssuser"].toObject()["maxthreads"].toString().toInt();
        if(allowedThreads != 0) {
          if(config.threadsSet && config.threads <= allowedThreads) {
            printf("User is allowed %d threads, but user has set it manually, so ignoring.\n\n", allowedThreads);
          } else {
            config.threads = (allowedThreads <= 8?allowedThreads:8);
            printf("Setting threads to \033[1;32m%d\033[0m as allowed for the supplied user credentials.\n\n", config.threads);
          }
        }
        int maxRequestsDay = jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestsperday"].toString().toInt();
        int currentRequests = jsonObj["response"].toObject()["ssuser"].toObject()["requeststoday"].toString().toInt();
        int maxKORequestsDay = jsonObj["response"].toObject()["ssuser"].toObject()["maxrequestskoperday"].toString().toInt();
        int currentKORequests = jsonObj["response"].toObject()["ssuser"].toObject()["requestskotoday"].toString().toInt();
        if((maxRequestsDay <= currentRequests) || (maxKORequestsDay <= currentKORequests)) {
          printf("\033[1;33mThe daily user limits at ScreenScraper have been reached. Please wait until the next day.\033[0m\n\n");
          removeLockAndExit(1);
        }
      }
    }
  }
}

void Skyscraper::loadAliasMap()
{
  if(!QFileInfo::exists("aliasMap.csv")) {
    return;
  }
  printf("aliasMap.csv found. Parsing contents.\n");
  QFile aliasMapFile("aliasMap.csv");
  if(aliasMapFile.open(QIODevice::ReadOnly)) {
    while(!aliasMapFile.atEnd()) {
      QByteArray line = aliasMapFile.readLine();
      if(line.left(1) == "#")
        continue;
      QList<QByteArray> pair = line.split(';');
      if(pair.size() != 2)
        continue;
      QString baseName = pair.at(0);
      QString aliasName = pair.at(1);
      baseName = baseName.replace("\"", "").simplified();
      aliasName = aliasName.replace("\"", "").simplified();
      config.aliasMap[baseName] = aliasName;
    }
    aliasMapFile.close();
  }
}

void Skyscraper::loadMameMap()
{
  if(config.scraper != "import" &&
     (Platform::get().getFamily(config.platform) == "arcade" ||
      config.extensions.contains("mame") || config.addExtensions.contains("mame") ||
      (config.useChecksum && config.scraper == "mamehistory"))) {
    QFile mameMapFile("mameMap.csv");
    if(mameMapFile.open(QIODevice::ReadOnly)) {
      while(!mameMapFile.atEnd()) {
        QList<QByteArray> pair = mameMapFile.readLine().replace("&amp;", "&").split(';');
        if(pair.size() != 2)
          continue;
        QString mameName = pair.at(0);
        QString realName = pair.at(1);
        mameName = mameName.replace("\"", "").simplified();
        realName = realName.replace("\"", "").simplified();
        config.mameMap[mameName] = realName;
      }
      mameMapFile.close();
    }
  }
}


void Skyscraper::loadWhdLoadMap()
{
  if(config.platform == "amiga") {
    QDomDocument doc;

    QFile whdLoadFile;
    if(QFileInfo::exists("whdload_db.xml"))
      whdLoadFile.setFileName("whdload_db.xml");
    else if(QFileInfo::exists("/usr/local/etc/skyscraper/whdload_db.xml"))
      whdLoadFile.setFileName("/usr/local/etc/skyscraper/whdload_db.xml");
    else
      return;

    if(whdLoadFile.open(QIODevice::ReadOnly)) {
      QByteArray rawXml = whdLoadFile.readAll();
      whdLoadFile.close();
      if(doc.setContent(rawXml)) {
        QDomNodeList gameNodes = doc.elementsByTagName("game");
        for(int a = 0; a < gameNodes.length(); ++a) {
          QDomNode gameNode = gameNodes.at(a);
          QPair<QString, QString> gamePair;
          gamePair.first = gameNode.firstChildElement("name").text();
          gamePair.second = gameNode.firstChildElement("variant_uuid").text();
          config.whdLoadMap[gameNode.toElement().attribute("filename")] = gamePair;
        }
      }
    }
  }
}

void Skyscraper::setRegionPrios()
{
  // Load single custom region
  if(!config.region.isEmpty()) {
    config.regionPrios.append(config.region);
  }

  // Load custom region prioritizations
  if(!config.regionPriosStr.isEmpty()) {
    const auto regions = config.regionPriosStr.split(",");
    for(const auto &region: std::as_const(regions)) {
      config.regionPrios.append(region.trimmed());
    }
  } else {
    config.regionPrios.append("eu");
    config.regionPrios.append("us");
    config.regionPrios.append("ss");
    config.regionPrios.append("uk");
    config.regionPrios.append("wor");
    config.regionPrios.append("jp");
    config.regionPrios.append("au");
    config.regionPrios.append("ame");
    config.regionPrios.append("de");
    config.regionPrios.append("cus");
    config.regionPrios.append("cn");
    config.regionPrios.append("kr");
    config.regionPrios.append("asi");
    config.regionPrios.append("br");
    config.regionPrios.append("sp");
    config.regionPrios.append("fr");
    config.regionPrios.append("gr");
    config.regionPrios.append("it");
    config.regionPrios.append("no");
    config.regionPrios.append("dk");
    config.regionPrios.append("nz");
    config.regionPrios.append("nl");
    config.regionPrios.append("pl");
    config.regionPrios.append("ru");
    config.regionPrios.append("se");
    config.regionPrios.append("tw");
    config.regionPrios.append("ca");
  }
}

void Skyscraper::setLangPrios()
{
  // Load single custom lang
  if(!config.lang.isEmpty()) {
    config.langPrios.append(config.lang);
  }

  // Load custom lang prioritizations
  if(!config.langPriosStr.isEmpty()) {
    const auto langs = config.langPriosStr.split(",");
    for(const auto &lang: std::as_const(langs)) {
      config.langPrios.append(lang.trimmed());
    }
  } else {
    config.langPrios.append("en");
    config.langPrios.append("de");
    config.langPrios.append("fr");
    config.langPrios.append("es");
  }
}

// --- Console colors ---
// Black        0;30     Dark Gray     1;30
// Red          0;31     Light Red     1;31
// Green        0;32     Light Green   1;32
// Brown/Orange 0;33     Yellow        1;33
// Blue         0;34     Light Blue    1;34
// Purple       0;35     Light Purple  1;35
// Cyan         0;36     Light Cyan    1;36
// Light Gray   0;37     White         1;37
