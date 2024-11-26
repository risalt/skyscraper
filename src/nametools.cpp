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

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QSql>
#include <QSqlError>
#include <QSqlQuery>
#include <QXmlStreamReader>

#include "nametools.h"
#include "strtools.h"
#include "skyscraper.h"
#include "crc32.h"

NameTools::NameTools()
{
}

NameTools & NameTools::get()
{
  static NameTools instance;
  return instance;
}

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
    if((current == '_') ||
       (a > 4 && (!baseName.mid(a, 4).compare("Demo", Qt::CaseInsensitive) ||
       !baseName.mid(a, 5).compare("-WHDL", Qt::CaseInsensitive)))) {
      break;
    }
    if(a > 0) {
      if(current.isDigit() && (!previous.isDigit() && previous != 'x')) {
        withSpaces.append(" ");
      } else if(current == '&') {
        withSpaces.append(" ");
      } else if(current == 'D') {
        if(!baseName.mid(a, 6).compare("Deluxe", Qt::CaseInsensitive)) {
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
  if(onlyMainTitle) {
    bool dummy;
    newName = removeSubtitle(newName, dummy);
  }

  QRegularExpressionMatch match;
  // Remove " rev.X" instances
  match = QRegularExpression(" rev[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4}|[IVX]{1,5})$",
                             QRegularExpression::CaseInsensitiveOption).match(newName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    newName = newName.left(match.capturedStart(0)).simplified();
  }
  // Remove versioning instances
  match = QRegularExpression(" v[.]{0,1}([0-9]{1}[0-9]{0,2}[.]{0,1}[0-9]{1,4})$",
                             QRegularExpression::CaseInsensitiveOption).match(newName);
  if(match.hasMatch() && match.capturedStart(0) != -1) {
    newName = newName.left(match.capturedStart(0)).simplified();
  }

  newName = newName.toLower();

  newName = removeEdition(newName);

  // Always remove 'the' from beginning or end if equal to or longer than 10 chars.
  // If it's shorter the 'the' is of more significance and shouldn't be removed.
  // Consider articles in English, German, French, Spanish
  // Exceptions: "Las Vegas", "Die Hard"
  // Dirty trick of using words=-2 to prevent article removal.
  if(newName.length() >= 10 && words != -2) {
    newName = removeArticle(newName);
  }

  // Use space as separator character as a signal that the symbols need to be respected:
  newName = StrTools::altUriEscape(newName, spaceChar);

  if(words > 0) {
    QStringList wordList = newName.split(spaceChar);
    if(wordList.size() > words) {
      newName.clear();
      for(int a = 0; a < words; ++a) {
        newName.append(wordList.at(a) + spaceChar);
      }
      newName = newName.left(newName.length() - spaceChar.length());
    }
  }

  if(Skyscraper::config.verbosity > 3) {
    qDebug() << "Simplified from " << baseName << " to " << newName;
  }
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
  if(QRegularExpression(" [IVX]{1,5}([,: ]+|$)",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
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

  match = QRegularExpression(" [IVX]{1,5}([,: ]+|$)",
                             QRegularExpression::CaseInsensitiveOption).match(baseName);
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
    if(!roman.compare("I", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "1"));
    } else if(!roman.compare("II", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "2"));
    } else if(!roman.compare("III", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "3"));
    } else if(!roman.compare("IV", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "4"));
    } else if(!roman.compare("V", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "5"));
    } else if(!roman.compare("VI", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "6"));
    } else if(!roman.compare("VII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "7"));
    } else if(!roman.compare("VIII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "8"));
    } else if(!roman.compare("IX", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "9"));
    } else if(!roman.compare("X", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "10"));
    } else if(!roman.compare("XI", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "11"));
    } else if(!roman.compare("XII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "12"));
    } else if(!roman.compare("XIII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "13"));
    } else if(!roman.compare("XIV", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "14"));
    } else if(!roman.compare("XV", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "15"));
    } else if(!roman.compare("XVI", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "16"));
    } else if(!roman.compare("XVII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "17"));
    } else if(!roman.compare("XVIII", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "18"));
    } else if(!roman.compare("XIX", Qt::CaseInsensitive)) {
      return newName.replace(match.captured(0), match.captured(0).replace(roman, "19"));
    } else if(!roman.compare("XX", Qt::CaseInsensitive)) {
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
  if(QRegularExpression("[_[]{1}(AGA)[_\\]]{0,1}",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
    sqrNotes.append("[AGA]");
  if(QRegularExpression("[_[]{1}(CD32)[_\\]]{0,1}",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
    sqrNotes.append("[CD32]");
  if(QRegularExpression("[_[]{1}(CDTV)[_\\]]{0,1}",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
    sqrNotes.append("[CDTV]");
  if(QRegularExpression("[_[]{1}(NTSC)[_\\]]{0,1}",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
    sqrNotes.append("[NTSC]");
  if(QRegularExpression("(DEMO)[_\\]]{1}",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
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
  match = QRegularExpression("[0-9]{1,2}[ ]{0,1}Disk",
                             QRegularExpression::CaseInsensitiveOption).match(baseName);
  if(match.hasMatch()) {
    parNotes.append("(" + match.captured(0).left(match.captured(0).indexOf("Disk", 0, Qt::CaseInsensitive)).trimmed() + " Disk)");
  }
  // Add "CD" detection that DON'T match CD32 and CDTV
  if(QRegularExpression("[_[]{1}CD(?!32|TV)",
                        QRegularExpression::CaseInsensitiveOption).match(baseName).hasMatch())
    parNotes.append("(CD)");
  // Look for language and add it
  match = QRegularExpression("[_[]{1}(De|It|Pl|Fr|Es|Fi|Dk|Gr|Cz){1,10}[_\\]]{0,1}",
                             QRegularExpression::CaseInsensitiveOption).match(baseName);
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
  QStringList notesList = notes.split(delim, Qt::SkipEmptyParts);
#else
  QStringList notesList = notes.split(delim, QString::SkipEmptyParts);
#endif
  QStringList uniqueList;
  for(const auto &note: std::as_const(notesList)) {
    bool found = false;
    for(const auto &unique: std::as_const(uniqueList)) {
      if(note.toLower() == unique.toLower()) {
        found = true;
      }
    }
    if(!found) {
      uniqueList.append(delim + note);
    }
  }
  QString uniqueNotes;
  for(const auto &note: std::as_const(uniqueList)) {
    uniqueNotes.append(note);
  }
  return uniqueNotes;
}

QString NameTools::getCacheId(const QFileInfo &info)
{
  QCryptographicHash cacheId(QCryptographicHash::Sha1);

  QString suffix = info.suffix().toLower();
  // Use checksum of filename if file is a script or an "unstable" compressed filetype
  bool cacheIdFromData = true;
  if(suffix == "uae"  || suffix == "cue" ||
     suffix == "sh"   || suffix == "svm" ||
     suffix == "conf" || suffix == "mds" ||
     suffix == "zip"  || suffix == "7z"  ||
     suffix == "gdi"  || suffix == "ml"  ||
     suffix == "bat"  || suffix == "au3" ||
     suffix == "po"   || suffix == "dsk" ||
     suffix == "nib"  || suffix == "m3u" ||
     suffix == "scummvm") {
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
      printf("Couldn't calculate cache id of rom file '%s', please check permissions and try again, now exiting...\n",
             info.fileName().toStdString().c_str());
      return "";
    }
  } else {
    cacheId.addData(info.fileName().toUtf8());
  }

  return cacheId.result().toHex();
}

QString NameTools::getNameFromTemplate(const GameEntry &game, const QString &nameTemplate)
{
  QStringList templateGroups = nameTemplate.split(";");
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
    if(!article.compare("the", Qt::CaseInsensitive) ||
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
      if(!article.compare("el", Qt::CaseInsensitive) ||
          !article.compare("la", Qt::CaseInsensitive) ||
          !article.compare("le", Qt::CaseInsensitive)) {
         articleSize = 2;
      }
    }
    if(articleSize) {
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
        if(classCurrent != "word") {
          classBefore = classCurrent;
          classCurrent = "word";
        }
        pos += articleSize;
      } else {
        pos += articleSize;
      }
    } else {
      if(classCurrent != "space" && (baseName[pos].isSpace() || baseName[pos] == spaceChar[0])) {
        classBefore = classCurrent;
        classCurrent = "space";
      } else if(classCurrent != "punct" && (baseName[pos].isPunct() || baseName[pos].isDigit())) {
        classBefore = classCurrent;
        classCurrent = "punct";
      } else if(classCurrent != "word" /*&& baseName[pos].isLetter()*/) {
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
    match = QRegularExpression(", the",
                               QRegularExpression::CaseInsensitiveOption).match(returnName);
    if(!match.hasMatch()) {
      match = QRegularExpression(", der",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", die",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", das",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression(", l[eao]s",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(match.hasMatch()) {
      returnName.replace(match.captured(0), "").prepend(match.captured(0).right(3) + " ");
    } else {
      // Two digit articles in French, Spanish:
      match = QRegularExpression(", l[ea]",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
      if(!match.hasMatch()) {
        match = QRegularExpression(", el",
                                   QRegularExpression::CaseInsensitiveOption).match(returnName);
      }
      if(match.hasMatch()) {
        returnName.replace(match.captured(0), "").prepend(match.captured(0).right(2) + " ");
      }
    }
  } else {
    // Two and three digit articles in English, German, French, Spanish:
    // Exceptions: "Las Vegas", "Die Hard"
    match = QRegularExpression("^the ",
                               QRegularExpression::CaseInsensitiveOption).match(returnName);
    if(!match.hasMatch()) {
      match = QRegularExpression("^der ",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      if(!QRegularExpression("^die hard",
                             QRegularExpression::CaseInsensitiveOption).match(returnName).hasMatch()) {
        match = QRegularExpression("^die ",
                                   QRegularExpression::CaseInsensitiveOption).match(returnName);
      }
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^das ",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      if(!QRegularExpression("^las vegas",
                             QRegularExpression::CaseInsensitiveOption).match(returnName).hasMatch()) {
        match = QRegularExpression("^l[eao]s ",
                                   QRegularExpression::CaseInsensitiveOption).match(returnName);
      }
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^l[ea] ",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
    }
    if(!match.hasMatch()) {
      match = QRegularExpression("^el ",
                                 QRegularExpression::CaseInsensitiveOption).match(returnName);
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
  if(baseName.contains(":") || baseName.contains(" - ") ||
     baseName.contains("~") || baseName.contains(" / ")) {
    noSubtitle = baseName.left(baseName.indexOf(":")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf(" - ")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf("~")).simplified();
    noSubtitle = noSubtitle.left(noSubtitle.indexOf(" / ")).simplified();
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

void NameTools::generateSearchNames(const QString &baseName,
                                    QStringList &safeTransformations,
                                    QStringList &unsafeTransformations,
                                    bool offlineUsage)
{
  QString spaceChar = " ";
  /* Apparently this is not needed anymore...
  if(Skyscraper::config.scraper == "screenscraper") {
    spaceChar = "+";
  }*/

  // Safe transformations 1-5:
  // 1. Lowercase
  safeTransformations.append(StrTools::altUriEscape(baseName.toLower(), spaceChar));
  // 2. Lowercase + No brackets
  QString copy = StrTools::stripBrackets(baseName);
  safeTransformations.append(StrTools::altUriEscape(copy.toLower(), spaceChar));
  // 3a. Lowercase + No brackets + No revision/version/edition + Roman numbers + &->and
  copy.replace("&", " and ");
  QString copy2 = getUrlQueryName(convertToRomanNumeral(copy), -2, spaceChar);
  safeTransformations.append(copy2);
  // 3b. Lowercase + No brackets + No revision/version/edition + Roman numbers +
  //     Article removed + &->and
  copy2 = getUrlQueryName(convertToRomanNumeral(copy), -1, spaceChar);
  safeTransformations.append(copy2);
  // 4a. Lowercase + No brackets + No revision/version/edition + Arabic numbers + &->and
  copy2 = getUrlQueryName(convertToIntegerNumeral(copy), -2, spaceChar);
  safeTransformations.append(copy2);
  // 4b. Lowercase + No brackets + No revision/version/edition + Arabic numbers +
  //     Article removed + &->and
  copy = getUrlQueryName(convertToIntegerNumeral(copy), -1, spaceChar);
  safeTransformations.append(copy);
  // 5a. Offline only: Lowercase + No brackets + No revision/version/edition + Arabic numbers +
  //                   Article removed + &->and + no symbols + no spaces
  // 5b. Offline only: Lowercase + No brackets + No revision/version/edition + Arabic numbers +
  //                   &->and + no symbols + no spaces
  if(offlineUsage) {
    safeTransformations.append(StrTools::sanitizeName(copy2));
    safeTransformations.append(StrTools::sanitizeName(copy));
    safeTransformations.append(StrTools::simplifyLetters(StrTools::sanitizeName(copy2)));
    safeTransformations.append(StrTools::simplifyLetters(StrTools::sanitizeName(copy)));
  }
  safeTransformations.removeDuplicates();
  if(Skyscraper::config.verbosity > 3) {
    qDebug() << "Safe transformations for" << baseName << ":" << safeTransformations;
  }
  if(!Skyscraper::config.keepSubtitle) {
    bool hasSubtitle;
    // Unsafe transformations: Same as 1-5 removing the subtitle
    copy = removeSubtitle(baseName, hasSubtitle);
    if(hasSubtitle) {
      unsafeTransformations.append(StrTools::altUriEscape(copy.toLower(), spaceChar));
      copy = StrTools::stripBrackets(copy);
      unsafeTransformations.append(StrTools::altUriEscape(copy.toLower(), spaceChar));
      copy.replace("&", " and ");
      copy2 = getUrlQueryName(convertToRomanNumeral(copy), -2, spaceChar);
      unsafeTransformations.append(copy2);
      copy2 = getUrlQueryName(convertToRomanNumeral(copy), -1, spaceChar);
      unsafeTransformations.append(copy2);
      copy2 = getUrlQueryName(convertToIntegerNumeral(copy), -2, spaceChar);
      unsafeTransformations.append(copy2);
      copy = getUrlQueryName(convertToIntegerNumeral(copy), -1, spaceChar);
      unsafeTransformations.append(copy);
      if(offlineUsage) {
        unsafeTransformations.append(StrTools::sanitizeName(copy2));
        unsafeTransformations.append(StrTools::sanitizeName(copy));
        unsafeTransformations.append(StrTools::simplifyLetters(StrTools::sanitizeName(copy2)));
        unsafeTransformations.append(StrTools::simplifyLetters(StrTools::sanitizeName(copy)));
      }
      unsafeTransformations.removeDuplicates();
      if(Skyscraper::config.verbosity > 3) {
        qDebug() << "Unsafe transformations for" << baseName << ":" << unsafeTransformations;
      }
    }
  }
}

CanonicalData NameTools::getCanonicalData(const QFileInfo &info, bool onlyChecksums)
{
  CanonicalData canonicalData;

  bool foundInCache = false;
  GameEntry cachedData;
  QString quickId = get().cache->getQuickId(info);
  if(!quickId.isEmpty()) {
    cachedData.cacheId = quickId;
    get().cache->fillBlanks(cachedData, "generic");
    if(cachedData.canonical.size > 0) {
      canonicalData = cachedData.canonical;
      if(!onlyChecksums && canonicalData.name.isEmpty()) {
        get().searchCanonicalData(canonicalData);
      }
      foundInCache = true;
    }
  }
  if(!foundInCache) {
    if(info.size() != 0) {
      QStringList compressedFormats = {"xz", "zip", "7z", "rar", "arj", "gz", "bz2"};
      QCryptographicHash md5(QCryptographicHash::Md5);
      QCryptographicHash sha1(QCryptographicHash::Sha1);
      Crc32 crc;
      crc.initInstance(1);
      if(compressedFormats.contains(info.suffix().toLower())) {
        // Not much can be done with multiple file archives, so we just scan all of it
        QProcess decProc;
        decProc.setReadChannel(QProcess::StandardOutput);
        decProc.start("7z", QStringList({"x", "-so", info.absoluteFilePath()}));
        if (decProc.waitForFinished(30000)) {
          if (decProc.exitStatus() == QProcess::NormalExit) {
             QByteArray allData = decProc.readAllStandardOutput();
             md5.addData(allData);
             sha1.addData(allData);
             crc.pushData(1, allData.data(), allData.length());
          } else {
            printf("ERROR: Cannot decompress file '%s': checksum is not possible. Skipping file.\n",
                   info.absoluteFilePath().toStdString().c_str());
          }
        } else {
          printf("ERROR: Timeout decompressing file '%s': checksum is not possible. Skipping file.\n",
                 info.absoluteFilePath().toStdString().c_str());
        }
      } else {
        QFile romFile(info.absoluteFilePath());
        romFile.open(QIODevice::ReadOnly);
        while(!romFile.atEnd()) {
          QByteArray dataSeg = romFile.read(1024);
          md5.addData(dataSeg);
          sha1.addData(dataSeg);
          crc.pushData(1, dataSeg.data(), dataSeg.length());
        }
        romFile.close();
      }
      QString crcResult = QString::number(crc.releaseInstance(1), 16);
      while(crcResult.length() < 8) {
        crcResult.prepend("0");
      }
      QString md5Result = md5.result().toHex();
      while(md5Result.length() < 32) {
        md5Result.prepend("0");
      }
      QString sha1Result = sha1.result().toHex();
      while(sha1Result.length() < 40) {
        sha1Result.prepend("0");
      }
      // Size
      canonicalData.size = info.size();
      // CRC
      canonicalData.crc = crcResult.toLower();
      // SHA1
      canonicalData.sha1 = sha1Result.toLower();
      // MD5
      canonicalData.md5 = md5Result.toLower();
      // Name (removing double quotes if necessary)
      if(!onlyChecksums) {
        get().searchCanonicalData(canonicalData);
      }
    } else {
      printf("WARNING: File '%s' is empty: checksum is not possible. Skipping file.\n",
             info.absoluteFilePath().toStdString().c_str());
    }
  }

  if(!quickId.isEmpty()) {
    cachedData.cacheId = quickId;
    cachedData.canonical = canonicalData;
    cachedData.source = "generic";
    QString dummy;
    get().cache->addResources(cachedData, Skyscraper::config, dummy);
  }
  return canonicalData;
}

bool NameTools::searchCanonicalData(CanonicalData &canonical)
{
  if(dbInitialized.testAndSetAcquire(0, 1)) {
    db = QSqlDatabase::addDatabase("QSQLITE", "canonical");
    db.setDatabaseName(dbName);
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if(!db.open()) {
      printf("ERROR: Could not open the database %s. Please create it using the "
             "option '--loadchecksum FILENAME'\n", dbName.toStdString().c_str());
      qDebug() << db.lastError();
      return false;
    }
    loadConfig("checksumcatalogs.json", "name", "code");
  }
  QSqlQuery query(db);
  query.setForwardOnly(true);
  query.prepare("SELECT platform, game, file FROM canonicalData WHERE"
                " sha1='" + canonical.sha1 + "' OR md5='" + canonical.md5 + "'");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from table canonicalData.\n");
    return false;
  }
  bool found = false;
  QString foundOtherPlatform;
  while(query.next()) {
    QString platform = query.value(0).toString();
    if(platformCatalogs.contains(platform)) {
      // We stop at the first positive match
      canonical.platform = platform;
      canonical.name = query.value(1).toString();
      canonical.file = query.value(2).toString();
      found = true;
      break;
    } else {
      foundOtherPlatform = platform;
    }
  }
  query.finish();

  if(!found && !foundOtherPlatform.isEmpty()) {
    printf("WARNING: Match to another catalog: Consider adding '%s' as a catalog"
           " for platform '%s' in 'checksumcatalogs.json'.\n",
           foundOtherPlatform.toStdString().c_str(),
           Skyscraper::config.platform.toStdString().c_str());
  }
  return found;
}

bool NameTools::importCanonicalData(const QString &file)
{
  QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "canonical");
  bool exists = QFileInfo(get().dbName).isFile();
  db.setDatabaseName(get().dbName);
  if(!db.open()) {
    printf("ERROR: Could not open/create the database %s.\n", get().dbName.toStdString().c_str());
    qDebug() << db.lastError();
    return false;
  }
  bool errorDb = true;
  QSqlQuery query(db);
  if(!exists) {
    printf("INFO: Creating SQLite database... ");
    // Database structure creation
    query.prepare("CREATE TABLE canonicalData (platform TEXT not NULL, game TEXT not NULL,"
                  " file TEXT, size INTEGER, crc TEXT, sha1 TEXT, md5 TEXT)");
    if(query.exec()) {
      query.finish();
      query.prepare("CREATE INDEX md5sumidx ON canonicalData(md5)");
      if(query.exec()) {
        query.finish();
        query.prepare("CREATE INDEX sha1sumidx ON canonicalData(sha1)");
        if(query.exec()) {
          query.finish();
          errorDb = false;
        }
      }
    }
    if(errorDb) {
      qDebug() << query.lastError();
      printf("\nERROR: Error while creating the database structure. Exiting.\n");
      db.close();
      return false;
    } else {
      printf("OK\n");
    }
  } else {
    errorDb = false;
  }

  bool useTransaction = true;
  if(!errorDb && !db.transaction()) {
    printf("WARNING: Reduced performance as database does not support transactions.\n");
    qDebug() << db.lastError();
    useTransaction = false;
  }

  int gamesWithoutName = 0;
  int gamesWithoutChecksums = 0;
  int gamesWithoutSize = 0;
  int gamesOk = 0;
  printf("INFO: Reading data from import file... ");
  QList<CanonicalData> canonicalImport;
  if(file == "LUTRISDB") {
    QSqlDatabase lutrisdb = QSqlDatabase::addDatabase("QMYSQL", "lutrisdb");
    lutrisdb.setHostName("db");
    lutrisdb.setDatabaseName("lutrisdb");
    lutrisdb.setUserName("lutrisdb");
    lutrisdb.setPassword("lutrisdb");
    if(!lutrisdb.open()) {
      qDebug() << lutrisdb.lastError();
      printf("ERROR: Connection to Lutris database has failed.\n");
    }
    QString queryString = "SELECT tosec_toseccategory.name, tosec_tosecgame.name,"
                          " tosec_tosecrom.name, tosec_tosecrom.size, tosec_tosecrom.crc,"
                          " tosec_tosecrom.sha1, tosec_tosecrom.md5 "
                          "FROM tosec_tosecrom"
                          " INNER JOIN tosec_tosecgame ON"
                          "  tosec_tosecrom.game_id = tosec_tosecgame.id"
                          " INNER JOIN tosec_toseccategory ON"
                          "  tosec_tosecgame.category_id = tosec_toseccategory.id";
    QSqlQuery queryLutris(lutrisdb);
    queryLutris.setForwardOnly(true);
    queryLutris.prepare(queryString);
    if(!queryLutris.exec()) {
      qDebug() << queryLutris.lastError();
      printf("ERROR: Error while accessing TOSEC data from Lutris database.\n");
      return false;
    }
    printf("Reading TOSEC database...");
    while(queryLutris.next()) {
      CanonicalData item;
      item.platform = queryLutris.value(0).toString();
      item.name = queryLutris.value(1).toString();
      item.file = queryLutris.value(2).toString();
      item.size = queryLutris.value(3).toLongLong();
      item.crc  = queryLutris.value(4).toString().toLower();
      item.sha1 = queryLutris.value(5).toString().toLower();
      item.md5  = queryLutris.value(6).toString().toLower();
      if(item.name.isEmpty()) {
        gamesWithoutName++;
      } else if(item.sha1.isEmpty() && item.md5.isEmpty()) {
        gamesWithoutChecksums++;
      } else {
        if(item.size == 0) {
          gamesWithoutSize++;
        }
        canonicalImport.append(item);
        gamesOk++;
        if(gamesOk % 1000 == 0) {
          printf("."); fflush(stdout);
        }
      }
    }
    queryLutris.finish();
    lutrisdb.close();
  } else {
    QXmlStreamReader reader;
    QFile xmlFile(file);
    if(xmlFile.open(QIODevice::ReadOnly)) {
      reader.setDevice(&xmlFile);
      if(reader.readNext() && reader.isStartDocument()) {
        CanonicalData item;
        while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
          QString currentElement = reader.name().toString();
          if(currentElement == "header") {
            while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
              QString currentElement = reader.name().toString();
              if(currentElement == "name") {
                item.platform = reader.readElementText();
              } else {
                reader.skipCurrentElement();
              }
            }
          } else if(currentElement == "game") {
            if(reader.attributes().hasAttribute("name")) {
              item.name = reader.attributes().value("name").toString();
            } else {
              item.name = "";
            }
            while(!reader.atEnd() && !reader.hasError() && reader.readNextStartElement()) {
              QString currentElement = reader.name().toString();
              if(currentElement == "rom") {
                if(reader.attributes().hasAttribute("name")) {
                  item.file = reader.attributes().value("name").toString();
                } else {
                  item.file = "";
                }
                if(reader.attributes().hasAttribute("name")) {
                  item.size = reader.attributes().value("size").toLongLong();
                } else {
                  item.size = 0;
                }
                if(reader.attributes().hasAttribute("name")) {
                  item.crc = reader.attributes().value("crc").toString().toLower();
                } else {
                  item.crc = "";
                }
                if(reader.attributes().hasAttribute("name")) {
                  item.sha1 = reader.attributes().value("sha1").toString().toLower();
                } else {
                  item.sha1 = "";
                }
                if(reader.attributes().hasAttribute("name")) {
                  item.md5 = reader.attributes().value("md5").toString().toLower();
                } else {
                  item.md5 = "";
                }
                if(item.name.isEmpty()) {
                  gamesWithoutName++;
                } else if(item.sha1.isEmpty() && item.md5.isEmpty()) {
                  gamesWithoutChecksums++;
                } else {
                  if(item.size == 0) {
                    gamesWithoutSize++;
                  }
                  canonicalImport.append(item);
                  gamesOk++;
                }
              }
              reader.skipCurrentElement();
            }
          } else if(currentElement != "datafile") {
            reader.skipCurrentElement();
          }
        }
      }
    } else {
      printf("ERROR: Cannot open checksum DAT file. Exiting now.\n");
      db.close();
      return false;
    }
  }
  printf("OK\n\n");
  printf("INFO: Summary:\n\tGames without name: %d\n\tGames without MD5/SHA1: %d\n\t"
         "Games without size: %d\n\tTOTAL games imported: %d\n\n",
         gamesWithoutName, gamesWithoutChecksums, gamesWithoutSize, gamesOk);

  if(gamesOk) {
    printf("INFO: Loading data into database... ");
    if(!errorDb) {
      query.prepare("INSERT INTO canonicalData VALUES (?, ?, ?, ?, ?, ?, ?)");
      QVariantList platforms, games, files, sizes, crcs, sha1s, md5s;
      for(const auto &item: std::as_const(canonicalImport)) {
        platforms << item.platform;
        games << item.name;
        files << item.file;
        sizes << item.size;
        crcs  << item.crc;
        sha1s << item.sha1;
        md5s  << item.md5;
      }
      query.addBindValue(platforms);
      query.addBindValue(games);
      query.addBindValue(files);
      query.addBindValue(sizes);
      query.addBindValue(crcs);
      query.addBindValue(sha1s);
      query.addBindValue(md5s);
      if(!query.execBatch()) {
        printf("\nERROR: Error while adding data into table canonicalData. Exiting.\n");
        qDebug() << query.lastError();
        errorDb = true;
      } else {
        printf("OK\n");
      }
    }

    if(useTransaction && !db.commit()) {
      qDebug() << db.lastError();
      db.rollback();
      errorDb = true;
      printf("ERROR: Error while populating the database. Exiting.\n");
    } else if(!errorDb) {
      printf("INFO: Data import completed successfully. Now deleting duplicates... ");
      query.prepare("DELETE FROM canonicalData WHERE ROWID NOT IN"
                    " (SELECT MIN(ROWID) FROM canonicalData GROUP BY"
                    "  platform, game, file, crc, sha1, md5)");
      if(query.exec()) {
        query.finish();
      }
      printf("OK. Now exiting.\n");
    }
  } else {
    printf("WARNING: Nothing to be loaded. Exiting.\n");
  }
  db.close();

  return !errorDb;
}

void NameTools::loadConfig(const QString &configPath,
                           const QString &code, const QString &name)
{
  platformCatalogs.clear();

  QFile configFile(configPath);
  if(!configFile.open(QIODevice::ReadOnly))
    return;

  QByteArray saveData = configFile.readAll();
  QJsonDocument json(QJsonDocument::fromJson(saveData));

  if(json.isNull() || json.isEmpty())
    return;

  QJsonArray platformsArray = json["platforms"].toArray();
  for(int platformIndex = 0; platformIndex < platformsArray.size(); ++platformIndex) {
    QJsonObject platformObject = platformsArray[platformIndex].toObject();

    QString platformName = platformObject[name].toString();
    QString platformIdn = platformObject[code].toString();

    if(!platformIdn.isEmpty() && !platformName.isEmpty() &&
       platformName == Skyscraper::config.platform) {
      platformCatalogs.append(platformIdn);
    }
  }
}

qint64 NameTools::dirSize(const QString &dirPath)
{
  qint64 size = 0;
  QDir dir(dirPath);

  QDir::Filters fileFilters = QDir::Files|QDir::System|QDir::Hidden;
  const auto &listFiles = dir.entryInfoList(fileFilters);
  for(const QFileInfo &file: listFiles) {
    size += file.size();
  }

  QDir::Filters dirFilters = QDir::Dirs|QDir::NoDotAndDotDot|QDir::System|QDir::Hidden;
  const auto &listDirs = dir.entryInfoList(dirFilters);
  for(const QFileInfo &dir: listDirs) {
    size += dir.size();
    if(!dir.isSymLink()) {
      size += dirSize(dirPath + "/" + dir.fileName());
    }
  }
  return size;
}

qint64 NameTools::calculateGameSize(const QString &filePath)
{
  // Limitations: It is not possible to include 3DS updates in the 3DS games calculation
  //              It is not possible to accurately calculate the size of Linux games
  qint64 diskSize = 0;

  if(filePath.contains("[realhw]")) {
    diskSize = 0;
  } else if(filePath.endsWith(".m3u")) {
    QFile playlist(filePath);
    if(playlist.open(QFile::ReadOnly | QFile::Text)) {
      QTextStream filelist(&playlist);
      while(!filelist.atEnd()) {
        diskSize += calculateGameSize(QFileInfo(filePath).absolutePath()
                                           + '/' + filelist.readLine());
      }
      diskSize += QFileInfo(filePath).size();
    } else {
      printf("ERROR: Cannot open file '%s' for reading. Cannot determine game disk size.\n",
             filePath.toStdString().c_str());
    }
  } else if(filePath.endsWith(".cue")) {
    QFile playlist(filePath);
    if(playlist.open(QFile::ReadOnly | QFile::Text)) {
      QTextStream filelist(&playlist);
      while(!filelist.atEnd()) {
        QString line = filelist.readLine();
        if(line.startsWith("FILE ")) {
          int trackTypePos = line.lastIndexOf(" ");
          QString track = line.mid(5, trackTypePos - 5).remove("\"");
          diskSize += QFileInfo(QFileInfo(filePath).absolutePath()
                                   + '/' + track).size();
        }
      }
      diskSize += QFileInfo(filePath).size();
    } else {
      printf("ERROR: Cannot open file '%s' for reading. Cannot determine game disk size.\n",
             filePath.toStdString().c_str());
    }
  } else if(filePath.endsWith(".actionmax") ||
            filePath.endsWith(".daphne")) {
    QFile playlist(filePath);
    if(playlist.open(QFile::ReadOnly | QFile::Text)) {
      QTextStream filelist(&playlist);
      while(!filelist.atEnd()) {
        QString line = filelist.readLine();
        QStringList parameters;
        if(filePath.endsWith(".actionmax")) {
          parameters = line.split(".");
        } else {
          parameters = line.split(" ");
        }
        if(parameters.size() > 1) {
          diskSize += dirSize(QFileInfo(filePath).absolutePath()
                                              + '/' + parameters.at(1));
        }
      }
      diskSize += QFileInfo(filePath).size();
    } else {
      printf("ERROR: Cannot open file '%s' for reading. Cannot determine game disk size.\n",
             filePath.toStdString().c_str());
    }
  } else if(filePath.endsWith(".playdate") ||
            filePath.endsWith(".scummvm")  ||
            filePath.endsWith(".vita")) {
    diskSize += dirSize(QFileInfo(filePath).absolutePath());
  } else if(filePath.endsWith(".ps3") ||
            (filePath.endsWith(".wiiu") && !QFileInfo(filePath).isSymLink())) {
    diskSize += dirSize(QFileInfo(filePath).absolutePath() + '/'
                        + QFileInfo(filePath).completeBaseName());
  } else if(Skyscraper::config.platform == "switch") {
    diskSize += dirSize(QFileInfo(QFileInfo(filePath).symLinkTarget()).absolutePath());
  } else if(filePath.endsWith(".lutris")) {
    QFile playlist(filePath);
    if(playlist.open(QFile::ReadOnly | QFile::Text)) {
      QTextStream filelist(&playlist);
      while(!filelist.atEnd()) {
        QString line = filelist.readLine();
        QStringList parameters = line.split("/");
        if(parameters.size() > 1) {
          QString windowsDir = get().searchLutrisData(parameters.at(1));
          if(!windowsDir.isEmpty()) {
            diskSize += dirSize(QFileInfo(windowsDir).absolutePath());
            diskSize += QFileInfo(filePath).size();
          }
        }
      }
    } else {
      printf("ERROR: Cannot open file '%s' for reading. Cannot determine game disk size.\n",
             filePath.toStdString().c_str());
    }
  } else {
    diskSize += QFileInfo(filePath).size();
  }

  return diskSize;
}

QString NameTools::searchLutrisData(const QString &slugName)
{
  if(dbInitialized.testAndSetAcquire(0, 1)) {
    db = QSqlDatabase::addDatabase("QSQLITE", "lutris");
    db.setDatabaseName(dbLutris);
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if(!db.open()) {
      printf("ERROR: Could not open the database %s. Please create it using the "
             "option '--loadchecksum FILENAME'\n", dbLutris.toStdString().c_str());
      qDebug() << db.lastError();
      return "";
    }
  }
  QSqlQuery query(db);
  query.setForwardOnly(true);
  query.prepare("SELECT configpath FROM games WHERE slug='" + slugName + "'");
  if(!query.exec()) {
    qDebug() << query.lastError();
    printf("ERROR: Error while accessing data from Lutris DB.\n");
    return "";
  }
  bool found = false;
  QString configFile;
  while(query.next()) {
    configFile = query.value(0).toString();
    found = true;
    break;
  }
  query.finish();
  if(found) {
    configFile = configLutris + configFile + ".yml";
    QFile playlist(configFile);
    if(playlist.open(QFile::ReadOnly | QFile::Text)) {
      QTextStream filelist(&playlist);
      while(!filelist.atEnd()) {
        QString line = filelist.readLine();
        if(line.startsWith("  exe: ")) {
          return line.mid(7);
        }
      }
    } else {
      printf("ERROR: Cannot open file '%s' for reading. Cannot determine game disk size.\n",
             configFile.toStdString().c_str());
    }
  }
  printf("ERROR: Could not determine the base directory from the Lutris setup. Skipping.\n");

  return "";
}
