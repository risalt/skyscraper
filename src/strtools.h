/***************************************************************************
 *            strtools.h
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

#ifndef STRTOOLS_H
#define STRTOOLS_H

#include <QString>
#include <QObject>
#include <QTextStream>

class StrTools : public QObject
{

public:
  static QString xmlUnescape(QString str);
  static QString xmlEscape(QString str);
  static QString uriEscape(QString str);
  static QString altUriEscape(const QString &str, const QString &spaceChar);
  static QString agesLabel(const QString &str);
  static QString conformPlayers(const QString &str);
  static QString conformAges(QString str);
  static QString conformReleaseDate(QString str);
  static QString conformTags(const QString &str);
  static QString getVersionHeader();
  static QString stripBrackets(const QString &str);
  static QString stripHtmlTags(QString str);
  static QString getMd5Sum(const QByteArray &data);
  static QString sanitizeName(const QString &str, bool removeBrackets=false);
  static int distanceBetweenStrings(const QString &first, const QString &second,
                                    bool simplify = false);
  static QString onlyNumbers(const QString &str);
  static bool readCSVRow(QTextStream &in, QStringList *row);
  static QString simplifyLetters(const QString &str);

};

#endif // STRTOOLS_H
