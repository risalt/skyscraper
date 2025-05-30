/***************************************************************************
 *            platform.h
 *
 *  Sat Dec 23 10:00:00 CEST 2017
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

#ifndef PLATFORM_H
#define PLATFORM_H

#include <QObject>
#include <QStringList>
#include <QMap>

class Platform : public QObject
{
  Q_OBJECT
public:
    static Platform & get();

    void loadConfig(const QString &configPath);
    void clearConfigData();

    QStringList getPlatforms() const;
    QString getSortBy(QString platform) const;
    QString getFamily(QString platform) const;
    QStringList getScrapers(QString platform) const;
    QString getFormats(QString platform, QString extensions, QString addExtensions) const;
    QString getDefaultScraper() const;
    QStringList getAliases(QString platform) const;

private:
    QStringList platforms;
    QMap<QString, QString> platformToSortBy;
    QMap<QString, QString> platformToFamily;
    QMap<QString, QStringList> platformToScrapers;
    QMap<QString, QStringList> platformToFormats;
    QMap<QString, QStringList> platformToAliases;
};

#endif // PLATFORM_H
