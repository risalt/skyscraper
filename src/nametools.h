/***************************************************************************
 *            nametools.h
 *
 *  Tue Feb 20 12:00:00 CEST 2018
 *  Copyright 2018 Lars Muldjord
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

#ifndef NAMETOOLS_H
#define NAMETOOLS_H

#include "gameentry.h"
#include "cache.h"

#include <QObject>
#include <QFileInfo>
#include <QStringList>
#include <QSqlDatabase>
#include <QAtomicInteger>
#include <QSharedPointer>

class NameTools : public QObject
{
public:
  NameTools(QString threadId);
  ~NameTools();
  static QString getScummName(const QString &baseName, const QString &scummIni);
  static QString getNameWithSpaces(const QString &baseName);
  static QString getUrlQueryName(const QString &baseName,
                                 const int words = -1,
                                 const QString &spaceChar = "+",
                                 const bool onlyMainTitle = false);
  static bool hasIntegerNumeral(const QString &baseName);
  static bool hasRomanNumeral(const QString &baseName);
  static QString convertToIntegerNumeral(const QString &baseName);
  static QString convertToRomanNumeral(const QString &baseName);
  static int getNumeral(const QString &baseName);
  static QString getSqrNotes(QString baseName);
  static QString getParNotes(QString baseName);
  static QString getUniqueNotes(const QString &notes, QChar delim);
  static QString getCacheId(const QFileInfo &info);
  static QString getNameFromTemplate(const GameEntry &game, const QString &nameTemplate);
  static QString removeArticle(const QString &baseName, const QString &spaceChar = " ");
  static QString moveArticle(const QString &baseName, const bool &toFront);
  static QString removeEdition(const QString &newName);
  static QString removeSubtitle(const QString &baseName, bool &hasSubtitle);
  static void generateSearchNames(const QString &baseName,
                                  QStringList &safeTransformations,
                                  QStringList &unsafeTransformations,
                                  bool offlineUsage);
  static CanonicalData calculateChecksums(const QFileInfo &info);

  CanonicalData getCanonicalData(const QFileInfo &info, bool onlyChecksums = false);
  bool searchCanonicalData(CanonicalData &canonical);
  bool importCanonicalData(const QString &file);
  QString searchLutrisData(const QString &filePath);
  qint64 dirSize(const QString &dirPath);
  qint64 calculateGameSize(const QString &filePath);
  qint64 recursiveCalculateGameSize(const QString &filePath);

  inline static QSharedPointer<Cache> cache;

private:
  void loadConfig(const QString &configPath,
                  const QString &code, const QString &name);

  QSqlDatabase db;
  QSqlDatabase lutrisdb;
  QString threadId;
  QString dbName = "canonicaldata.db";
  QString dbLutris = "/share/Games/pcsteam/linux/.local/share/lutris/pga.db";
  QString configLutris = "/share/Games/pcsteam/linux/.config/lutris/games/";
  QStringList platformCatalogs;

};

#endif // NAMETOOLS_H
