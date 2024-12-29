/***************************************************************************
 *            mamehistory.cpp
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

#include <QFile>
#include <QList>
#include <QXmlStreamReader>

#include "mamehistory.h"
#include "strtools.h"
#include "platform.h"

MAMEHistory::MAMEHistory(Settings *config,
                   QSharedPointer<NetManager> manager,
                   QString threadId)
  : AbstractScraper(config, manager, threadId)
{
  offlineScraper = true;

  if(Platform::get().getFamily(config->platform) != "arcade" && !config->useChecksum &&
     (!config->extensions.contains("mame") && !config->addExtensions.contains("mame"))) {
    printf("\033[0;31mPlatform not supported by Arcade History "
           "(only Arcade platforms are supported)...\033[0m\n");
    reqRemaining = 0;
    return;
  }

  loadConfig("mamehistory.json", "code", "platform");

  printf("INFO: Reading data from MAME history file... "); fflush(stdout);

  QXmlStreamReader reader;
  QFile xmlFile("history.xml");
  if(xmlFile.open(QIODevice::ReadOnly)) {
    reader.setDevice(&xmlFile);
    GameEntry item;
    QList<GameEntry> listSoftware;
    if(reader.readNext() && reader.isStartDocument()) {
      while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
        if(reader.name().toString() == "entry") {
          listSoftware.clear();
          while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
            if(reader.name().toString() == "software" || reader.name().toString() == "systems") {
              while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
                if(reader.name().toString() == "item" || reader.name().toString() == "system") {
                  item = GameEntry();
                  if(reader.attributes().hasAttribute("list")) {
                    item.platform = getPlatformId(reader.attributes().value("list").toString());
                    QString mamePlatform = item.platform.split(';').at(0);
                    if(mamePlatform == "na") {
                      printf("WARN: Missing platform media in 'mamehistory.json': %s\n",
                             reader.attributes().value("list").toString().toStdString().c_str());
                      mamePlatform = "";
                    }
                    if(!mamePlatform.isEmpty() && mamePlatform != config->platform) {
                      item.platform = "";
                    } else if(item.platform.contains(';')) {
                      item.platform = item.platform.split(';').at(1);
                    } else {
                      item.platform = reader.attributes().value("list").toString();
                    }
                  } else {
                    item.platform = Platform::get().getAliases(config->platform).at(1);
                  }
                  if(reader.attributes().hasAttribute("name")) {
                    item.id = reader.attributes().value("name").toString().toLower();
                    item.title = config->mameMap[item.id];
                  }
                  listSoftware.append(item);
                }
                reader.skipCurrentElement();
              }
            }
            else if(reader.name().toString() == "text") {
              QString description = reader.readElementText();
              description = description.right(description.size() - description.indexOf('\n', 2));
              for(const auto &software: std::as_const(listSoftware)) {
                item.id = software.id;
                item.title = software.title;
                item.platform = software.platform;
                item.description = "Category: " + item.platform + description;
                if(!item.title.isEmpty() && !item.platform.isEmpty() &&
                   !item.id.isEmpty() && !item.description.isEmpty()) {
                  historyMap[item.id] = item;
                  if(config->verbosity > 3) {
                    qDebug() << item.id << item.title << item.platform << item.description;
                  }
                  item = GameEntry();
                }
              }
            }
            else {
              reader.skipCurrentElement();
            }
          }
        }
      }
    }
    printf("DONE\n");
  } else {
    printf("ERROR: Cannot open MAME history XML file. Please (re-)download "
           "it from 'https://www.arcade-history.com/dats/history272.zip' "
           "and unzip it in the Skyscraper home directory.\n");
    reqRemaining = 0;
    return;
  }

  printf("INFO: MAME History database has been parsed, %d games have been loaded into memory.\n",
         historyMap.count());

  fetchOrder.append(DESCRIPTION);
}

void MAMEHistory::getSearchResults(QList<GameEntry> &gameEntries,
                                QString searchName, QString)
{
  if(historyMap.contains(searchName)) {
    gameEntries.append(historyMap[searchName]);
  }
}

void MAMEHistory::getGameData(GameEntry &, QStringList &, GameEntry *)
{
}

void MAMEHistory::getDescription(GameEntry &)
{
}

QStringList MAMEHistory::getSearchNames(const QFileInfo &info)
{
  QStringList searchNames;
  if(info.suffix() == "mame" && info.isSymbolicLink()) {
    QFileInfo mameFile = info.symLinkTarget();
    searchNames.append(mameFile.baseName());
  } else {
    searchNames.append(info.baseName());
  }
  if(config->verbosity >= 2) {
    qDebug() << "Search Name:" << searchNames.first();
  }
  return searchNames;
}
