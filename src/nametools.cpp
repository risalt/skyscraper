/***************************************************************************
 *            nametools.cpp
 *
 *  Tue Feb 20 12:00:00 CEST 2018
 *  Copyright 2018 Lars Muldjord
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

#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QRegularExpression>
#include <QCryptographicHash>

#include "nametools.h"
#include "strtools.h"
#include "skyscraper.h"

QString NameTools::getScummName(const QString &baseName, const QString &scummIni)
{
  // Set to global for RetroPie
  QString scummIniStr = "/opt/retropie/configs/scummvm/scummvm.ini";

  // If local exists, use that one instead
  if(QFileInfo::exists(QDir::homePath() + "/.scummvmrc")) {
    scummIniStr = QDir::homePath() + "/.scummvmrc";
  }

  // If set in config, use that one instead
  if(!scummIni.isEmpty()) {
    scummIniStr = scummIni;
  }


  QFile scummIniFile(scummIniStr);
  if(scummIniFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    int state = 0;
    while(!scummIniFile.atEnd()) {
      QByteArray line = scummIniFile.readLine();
      if(line.contains("[")) {
        state = 0; // Always reset if traversing into a new game
      }
      if(state == 0 && line.contains("[" + baseName.toUtf8() + "]")) {
        state = 1;
      }
      if(state == 1 && line.contains("description=")) {
        return line.split('=').last();
      }
    }
    scummIniFile.close();
  }
  return baseName;
}

QString NameTools::getNameWithSpaces(const QString &baseName)
{
  // Only perform if name contains no spaces
  if(baseName.indexOf(" ") != -1) {
    return baseName;
  }

  QString withSpaces = "";
  QChar previous;
  for(int a = 0; a < baseName.length(); ++a) {
    QChar current = baseName.at(a);
    if(current == '_' ||
       (a > 4 && baseName.mid(a, 4) == "Demo") ||
       baseName.mid(a, 5) == "-WHDL") {
      break;
    }
    if(a > 0) {
      if(current.isDigit() && (!previous.isDigit() && previous != 'x')) {
        withSpaces.append(" ");
      } else if(current == '&') {
        withSpaces.append(" ");
      } else if(current == 'D') {
        if(baseName.mid(a, 6) == "Deluxe") {
          withSpaces.append(" ");
        } else if(previous != '3' && previous != '4') {
          withSpaces.append(" ");
        }
      } else if(current.isUpper()) {
        if(previous.isLetter() && !previous.isUpper()) {
          withSpaces.append(" ");
        } else if(previous == '&') {
          withSpaces.append(" ");
        } else if(previous == 'D') {
          withSpaces.append(" ");
        } else if(previous == 'C') {
          withSpaces.append(" ");
        } else if(previous == 'O') {
          withSpaces.append(" ");
        } else if(previous.isDigit()) {
          withSpaces.append(" ");
        }
      }
    }
    withSpaces.append(current);
    previous = current;
  }
  //printf("withSpaces: '%s'\n", withSpaces.toStdString().c_str());
  return withSpaces;
}

QString NameTools::getUrlQueryName(const QString &baseName, const int words, const QString &spaceChar, const bool onlyMainTitle)
{
  QString newName = baseName;
  // Remove everything in brackets
  newName = newName.left(newName.indexOf("(", 2)).simplified();
  newName = newName.left(newName.indexOf("[", 2)).simplified();
  // The following is mostly, if not only, used when getting the name from mameMap
  newName = newName.left(newName.indexOf(" / ", 2)).simplified();
  if (onlyMainTitle) {
    bool dummy;
    newName = NameTools::removeSubtitle(newName, dummy);
  }

  QRegularExpressionMatch match;
  // Remove " rev.X" instances
  match = QRegularExpression(" rev[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4}|[IVX]{1,5})$").match(newName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    newName = newName.left(match.capturedStart(0)).simplified();
  }
  // Remove versioning instances
  match = QRegularExpression(" v[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4}|[IVX]{1,5})$").match(newName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    newName = newName.left(match.capturedStart(0)).simplified();
  }

  // If we have the first game in a series, remove the ' I' for more search results
  if(newName.right(2) == " I") {
    newName = newName.left(newName.length() - 2);
  }

  newName = newName.toLower();

  newName = removeEdition(newName);

  // Always remove 'the' from beginning or end if equal to or longer than 10 chars.
  // If it's shorter the 'the' is of more significance and shouldn't be removed.
  // Consider articles in English, German, French, Spanish
  // Exceptions: "Las Vegas", "Die Hard"
  if(newName.length() >= 10) {
    newName = removeArticle(newName);
  }
  
  // Use space as separator character as a signal that the symbols need to be respected:
  if (spaceChar != " ") {
    newName = newName.replace("_", " ");
    newName = newName.replace(" - ", " ");
    newName = newName.replace(",", " ");
    newName = newName.replace("&", "%26");
    newName = newName.replace("+", "");
    // A few game names have faulty "s's". Fix them to "s'"
    newName = newName.replace("s's", "s'");
    newName = newName.replace("'", "%27");
    // Finally change all spaces to requested char. Default is '+' since that's what most search engines seem to understand
    newName = newName.simplified().replace(" ", spaceChar);
  }

  // Implement special cases here (belongs to the exceptions mapping file, not here)
  /* if(newName == "ik") {
    newName = "international+karate";
  }
  if(newName == "arkanoid+revenge+of+doh") {
    newName = "arkanoid%3A+revenge+of+doh";
  }
  if(newName == "lemmings+3") {
    newName = "all+new+world+of+lemmings";
  } */

  if(words != -1) {
    QList<QString> wordList = newName.split(spaceChar);
    if(wordList.size() > words) {
      newName.clear();
      for(int a = 0; a < words; ++a) {
        newName.append(wordList.at(a) + spaceChar);
      }
      newName = newName.left(newName.length() - spaceChar.length());
    }
  }
  
  // qDebug() << "Simplified from " << baseName << " to " << newName;
  return newName;
}

bool NameTools::hasIntegerNumeral(const QString &baseName)
{
  if(QRegularExpression(" [0-9]{1,2}([,: ]+|$)").match(baseName).hasMatch())
    return true;
  return false;
}

bool NameTools::hasRomanNumeral(const QString &baseName)
{
  if(QRegularExpression(" [IVX]{1,5}([,: ]+|$)").match(baseName).hasMatch())
    return true;
  return false;
}

QString NameTools::convertToRomanNumeral(const QString &baseName)
{
  QRegularExpressionMatch match;
  QString newName = baseName;

  match = QRegularExpression(" [0-9]{1,2}([,: ]+|$)").match(baseName);
  // Match is either " 2" or " 2: blah blah blah"
  if(match.hasMatch()) {
    QString integer = match.captured(0);
    if(integer.contains(":")) {
      integer = integer.left(integer.indexOf(":")).simplified();
    } else if(integer.contains(",")) {
      integer = integer.left(integer.indexOf(",")).simplified();
    } else {
      integer = integer.simplified();
    }
    if(integer == "1") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "I"));
    } else if(integer == "2") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "II"));
    } else if(integer == "3") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "III"));
    } else if(integer == "4") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "IV"));
    } else if(integer == "5") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "V"));
    } else if(integer == "6") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "VI"));
    } else if(integer == "7") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "VII"));
    } else if(integer == "8") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "VIII"));
    } else if(integer == "9") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "IX"));
    } else if(integer == "10") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "X"));
    } else if(integer == "11") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XI"));
    } else if(integer == "12") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XII"));
    } else if(integer == "13") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XIII"));
    } else if(integer == "14") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XIV"));
    } else if(integer == "15") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XV"));
    } else if(integer == "16") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XVI"));
    } else if(integer == "17") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XVII"));
    } else if(integer == "18") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XVIII"));
    } else if(integer == "19") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XIX"));
    } else if(integer == "20") {
      return newName.replace(match.captured(0), match.captured(0).replace(integer, "XX"));
    }
  }
  return newName;
}

QString NameTools::convertToIntegerNumeral(const QString &baseName)
{
  QRegularExpressionMatch match;
  QString newName = baseName;

  match = QRegularExpression(" [IVX]{1,5}([,: ]+|$)").match(baseName);
  // Match is either " X" or " X: blah blah blah"
  if(match.hasMatch()) {
    QString roman = match.captured(0);
    if(roman.contains(":")) {
      roman = roman.left(roman.indexOf(":")).simplified();
    } else if(roman.contains(",")) {
      roman = roman.left(roman.indexOf(",")).simplified();
    } else {
      roman = roman.simplified();
    }
    if(roman == "I") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "1"));
    } else if(roman == "II") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "2"));
    } else if(roman == "III") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "3"));
    } else if(roman == "IV") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "4"));
    } else if(roman == "V") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "5"));
    } else if(roman == "VI") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "6"));
    } else if(roman == "VII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "7"));
    } else if(roman == "VIII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "8"));
    } else if(roman == "IX") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "9"));
    } else if(roman == "X") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "10"));
    } else if(roman == "XI") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "11"));
    } else if(roman == "XII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "12"));
    } else if(roman == "XIII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "13"));
    } else if(roman == "XIV") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "14"));
    } else if(roman == "XV") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "15"));
    } else if(roman == "XVI") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "16"));
    } else if(roman == "XVII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "17"));
    } else if(roman == "XVIII") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "18"));
    } else if(roman == "XIX") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "19"));
    } else if(roman == "XX") {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "20"));
    }
  }
  return newName;
}

int NameTools::getNumeral(const QString &baseName)
{
  int numeral = StrTools::onlyNumbers(convertToIntegerNumeral(baseName)).toUInt();
  if(numeral) {
    return numeral;
  } else {
    return 1;
  }
}

QString NameTools::getSqrNotes(QString baseName)
{
  QString sqrNotes = "";

  // Get square notes
  while(baseName.contains("[") && baseName.contains("]") &&
        baseName.indexOf("[") < baseName.indexOf("]")) {
    if(baseName.indexOf("[") != -1 && baseName.indexOf("]") != -1) {
      sqrNotes.append(baseName.mid(baseName.indexOf("["),
                              baseName.indexOf("]") - baseName.indexOf("[") + 1));
    }
    baseName.remove(baseName.indexOf("["),
               baseName.indexOf("]") - baseName.indexOf("[") + 1);
  }

  // Look for '_tag_' or '[tag]' with the last char optional
  if(QRegularExpression("[_[]{1}(Aga|AGA)[_\\]]{0,1}").match(baseName).hasMatch())
    sqrNotes.append("[AGA]");
  if(QRegularExpression("[_[]{1}(Cd32|cd32|CD32)[_\\]]{0,1}").match(baseName).hasMatch())
    sqrNotes.append("[CD32]");
  if(QRegularExpression("[_[]{1}(Cdtv|cdtv|CDTV)[_\\]]{0,1}").match(baseName).hasMatch())
    sqrNotes.append("[CDTV]");
  if(QRegularExpression("[_[]{1}(Ntsc|ntsc|NTSC)[_\\]]{0,1}").match(baseName).hasMatch())
    sqrNotes.append("[NTSC]");
  if(QRegularExpression("(Demo|demo|DEMO)[_\\]]{1}").match(baseName).hasMatch())
    sqrNotes.append("[Demo]");
  // Don't add PAL detection as it will also match with "_Palace" and such
  sqrNotes = sqrNotes.simplified();

  return sqrNotes;
}

QString NameTools::getParNotes(QString baseName)
{
  QString parNotes = "";

  // Get parentheses notes
  while(baseName.contains("(") && baseName.contains(")") &&
        baseName.indexOf("(") < baseName.indexOf(")")) {
    if(baseName.indexOf("(") != -1 && baseName.indexOf(")") != -1) {
      parNotes.append(baseName.mid(baseName.indexOf("("),
                              baseName.indexOf(")") - baseName.indexOf("(") + 1));
    }
    baseName.remove(baseName.indexOf("("),
               baseName.indexOf(")") - baseName.indexOf("(") + 1);
  }

  QRegularExpressionMatch match;

  // Add "nDisk" detection
  match = QRegularExpression("[0-9]{1,2}[ ]{0,1}Disk").match(baseName);
  if(match.hasMatch()) {
    parNotes.append("(" + match.captured(0).left(match.captured(0).indexOf("Disk")).trimmed() + " Disk)");
  }
  // Add "CD" detection that DON'T match CD32 and CDTV
  if(QRegularExpression("[_[]{1}CD(?!32|TV)").match(baseName).hasMatch())
    parNotes.append("(CD)");
  // Look for language and add it
  match = QRegularExpression("[_[]{1}(De|It|Pl|Fr|Es|Fi|Dk|Gr|Cz){1,10}[_\\]]{0,1}").match(baseName);
  if(match.hasMatch()) {
    parNotes.append("(" +
                    match.captured(0).replace("_", "").
                    replace("[", "").
                    replace("]", "") +
                    ")");
  }

  parNotes = parNotes.simplified();

  return parNotes;
}

QString NameTools::getUniqueNotes(const QString &notes, QChar delim)
{
#if QT_VERSION >= 0x050e00
  QList<QString> notesList = notes.split(delim, Qt::SkipEmptyParts);
#else
  QList<QString> notesList = notes.split(delim, QString::SkipEmptyParts);
#endif
  QList<QString> uniqueList;
  for(const auto &note: notesList) {
    bool found = false;
    for(const auto &unique: uniqueList) {
      if(note.toLower() == unique.toLower()) {
        found = true;
      }
    }
    if(!found) {
      uniqueList.append(delim + note);
    }
  }
  QString uniqueNotes;
  for(const auto &note: uniqueList) {
    uniqueNotes.append(note);
  }
  return uniqueNotes;
}

QString NameTools::getCacheId(const QFileInfo &info)
{
  QCryptographicHash cacheId(QCryptographicHash::Sha1);

  // Use checksum of filename if file is a script or an "unstable" compressed filetype
  bool cacheIdFromData = true;
  if(info.suffix() == "uae"  || info.suffix() == "cue" ||
     info.suffix() == "sh"   || info.suffix() == "svm" ||
     info.suffix() == "conf" || info.suffix() == "mds" ||
     info.suffix() == "zip"  || info.suffix() == "7z"  ||
     info.suffix() == "gdi"  || info.suffix() == "ml"  ||
     info.suffix() == "bat"  || info.suffix() == "au3" ||
     info.suffix() == "po"   || info.suffix() == "dsk" ||
     info.suffix() == "nib"  || info.suffix() == "scummvm") {
    cacheIdFromData = false;
  }
  // If file is larger than 50 MBs, use filename checksum for cache id for optimization reasons
  if(info.size() > 52428800) {
    cacheIdFromData = false;
  }
  // If file is empty always do checksum on filename
  if(info.size() == 0) {
    cacheIdFromData = false;
  }
  if(cacheIdFromData) {
    QFile romFile(info.absoluteFilePath());
    if(romFile.open(QIODevice::ReadOnly)) {
      while(!romFile.atEnd()) {
        cacheId.addData(romFile.read(1024));
      }
      romFile.close();
    } else {
      printf("Couldn't calculate cache id of rom file '%s', please check permissions and try again, now exiting...\n", info.fileName().toStdString().c_str());
      Skyscraper::removeLockAndExit(1);
    }
  } else {
    cacheId.addData(info.fileName().toUtf8());
  }

  return cacheId.result().toHex();
}

QString NameTools::getNameFromTemplate(const GameEntry &game, const QString &nameTemplate)
{
  QList<QString> templateGroups = nameTemplate.split(";");
  QString finalName;
  for(auto &templateGroup: templateGroups) {
    bool include = false;
    if(templateGroup.contains("%t") && !game.title.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%f") && !game.baseName.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%b") && !game.parNotes.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%B") && !game.sqrNotes.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%a") && !game.ages.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%d") && !game.developer.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%p") && !game.publisher.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%P") && !game.players.isEmpty()) {
      include = true;
    }
    if(templateGroup.contains("%D") && !game.releaseDate.isEmpty()) {
      include = true;
    }
    if(include) {
      templateGroup.replace("%t", game.title);
      templateGroup.replace("%f", StrTools::stripBrackets(game.baseName));
      templateGroup.replace("%b", game.parNotes);
      templateGroup.replace("%B", game.sqrNotes);
      templateGroup.replace("%a", game.ages);
      templateGroup.replace("%d", game.developer);
      templateGroup.replace("%p", game.publisher);
      templateGroup.replace("%P", game.players);
      templateGroup.replace("%D", game.releaseDate);
      finalName.append(templateGroup);
    }
  }

  return finalName;
}

QString NameTools::removeArticle(const QString &baseName, const QString &spaceChar)
{
  // If the article is surrounded by space+letters -> keep, else -> remove
  // Consider articles in English, German, French, Spanish
  // Exceptions: "Las Vegas", "Die Hard"
  QString newName = "";
  int pos = 0;
  QString classCurrent = "void";
  QString classBefore = "void";
  while(pos < baseName.size()) {
    int articleSize = 0;
    QString article = baseName.mid(pos, 3);
    if (!article.compare("the", Qt::CaseInsensitive) ||
        !article.compare("der", Qt::CaseInsensitive) ||
        !article.compare("die", Qt::CaseInsensitive) ||
        !article.compare("das", Qt::CaseInsensitive) ||
        !article.compare("las", Qt::CaseInsensitive) ||
        !article.compare("les", Qt::CaseInsensitive) ||
        !article.compare("los", Qt::CaseInsensitive)) {
       if((article.compare("las", Qt::CaseInsensitive) ||
           baseName.mid(pos, 9).compare("las" + spaceChar + "vegas", Qt::CaseInsensitive)) &&
          (article.compare("die", Qt::CaseInsensitive) ||
           baseName.mid(pos, 8).compare("die" + spaceChar + "hard", Qt::CaseInsensitive))) {
         articleSize = 3;
       }
    } else {
      article = baseName.mid(pos, 2);
      if (!article.compare("el", Qt::CaseInsensitive) ||
          !article.compare("la", Qt::CaseInsensitive) ||
          !article.compare("le", Qt::CaseInsensitive)) {
         articleSize = 2;
      }
    }
    if (articleSize) {
      bool wordBefore = (classCurrent == "space" && classBefore == "word");
      bool wordAfter = false;
      int pos2 = pos+articleSize;
      QString classAfter = "void";
      QString classAfterAfter = "void";
      if(pos2 >= baseName.size()) {
        wordAfter = false;
      } else {
        while(pos2 < baseName.size()) {
          if(baseName[pos2].isSpace() || baseName[pos2] == spaceChar[0]) {
            if(classAfter == "void") {
              classAfter = "space";
            } else if(classAfter != "space") {
              classAfterAfter = "space";
              break;
            }
          } else if(baseName[pos2].isPunct()) {
            if(classAfter == "void") {
              classAfter = "punct";
            } else if(classAfter != "punct") {
              classAfterAfter = "punct";
              break;
            }
          } else /*if(baseName[pos2].isLetterOrNumber())*/ {
            if(classAfter == "void") {
              classAfter = "word";
            } else if(classAfter != "word") {
              classAfterAfter = "word";
              break;
            }
          }
          pos2++;
        }
        wordAfter = (classAfter == "space" && classAfterAfter == "word");
      }
      if((wordBefore && wordAfter) || (classCurrent == "word" || classAfter == "word")) {
        newName.append(article);
        if (classCurrent != "word") {
          classBefore = classCurrent;
          classCurrent = "word";
        }
        pos += articleSize;
      } else {
        pos += articleSize;
      }
    } else {
      if (classCurrent != "space" && (baseName[pos].isSpace() || baseName[pos] == spaceChar[0])) {
        classBefore = classCurrent;
        classCurrent = "space";
      } else if(classCurrent != "punct" && (baseName[pos].isPunct() || baseName[pos].isDigit())) {
        classBefore = classCurrent;
        classCurrent = "punct";
      } else if (classCurrent != "word" /*&& baseName[pos].isLetter()*/) {
        classBefore = classCurrent;
        classCurrent = "word";
      }
      newName.append(baseName[pos]);
      pos++;
    }
  }
  newName.replace(spaceChar + spaceChar, spaceChar);
  return newName;
}

QString NameTools::moveArticle(const QString &baseName, const bool &toFront)
{
  QString returnName = baseName;
  QRegularExpressionMatch match;
  if(toFront) {
    // Three digit articles in English, German, French, Spanish:
    match = QRegularExpression(", [Tt]he").match(returnName);
    if(!match.hasMatch()) {
      match = QRegularExpression(", [Dd]er").match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", [Dd]ie").match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", [Dd]as").match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", [Ll][eao]s").match(returnName);
    }
    if(match.hasMatch()) {
      returnName.replace(match.captured(0), "").prepend(match.captured(0).right(3) + " ");
    } else {
      // Two digit articles in French, Spanish:
      match = QRegularExpression(", [Ll][ea]").match(returnName);
      if(!match.hasMatch()) {
        match = QRegularExpression(", [Ee]l").match(returnName);
      }
      if(match.hasMatch()) {
        returnName.replace(match.captured(0), "").prepend(match.captured(0).right(2) + " ");
      }
    }
  } else {
    // Two and three digit articles in English, German, French, Spanish:
    // Exceptions: "Las Vegas", "Die Hard"
    match = QRegularExpression("^[Tt]he ").match(returnName);
    if(!match.hasMatch()) {
      match = QRegularExpression("^[Dd]er ").match(returnName);
    }
    if(!match.hasMatch()) {
      if(!QRegularExpression("^[Dd]ie [Hh]ard").match(returnName).hasMatch()) {
        match = QRegularExpression("^[Dd]ie ").match(returnName);
      }
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^[Dd]as ").match(returnName);
    }
    if(!match.hasMatch()) {
      if(!QRegularExpression("^[Ll]as [Vv]egas").match(returnName).hasMatch()) {
        match = QRegularExpression("^[Ll][eao]s ").match(returnName);
      }
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^[Ll][ea] ").match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^[Ee]l ").match(returnName);
    }
    if(match.hasMatch()) {
      returnName.replace(match.captured(0), "").append(", " + match.captured(0).replace(" ", ""));
    }
  }
  return returnName;
}

QString NameTools::removeEdition(const QString &newName)
{
  QRegularExpressionMatch match = QRegularExpression(" [[:alpha:]]+ edition",
                                     QRegularExpression::CaseInsensitiveOption).match(newName);
  if(match.hasMatch()) {
    QString woArticle = newName;
    return woArticle.replace(match.captured(0), " ").simplified();
  } else {
    return newName.simplified();
  }
}

QString NameTools::removeSubtitle(const QString &baseName, bool &hasSubtitle)
{
  QString noSubtitle;
  if(baseName.contains(":") || baseName.contains(" - ") || baseName.contains("~")) {
    noSubtitle = baseName.left(baseName.indexOf(":")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf(" - ")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf("~")).simplified();
    // Only add if longer than 3. We don't want to search for "the" for instance
    if(noSubtitle.length() > 3) {
      hasSubtitle = true;
      return noSubtitle;
    }
  }
  hasSubtitle = false;
  noSubtitle = baseName;
  return noSubtitle;
}
