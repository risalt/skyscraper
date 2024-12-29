/***************************************************************************
 *            strtools.cpp
 *
 *  Wed Jun 7 12:00:00 CEST 2017
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

#include <QUrl>
#include <QDate>
#include <QLocale>
#include <QRegularExpression>
#include <QCryptographicHash>

#include "strtools.h"
#include "skyscraper.h"

QString StrTools::xmlUnescape(QString str)
{
  str = str.replace("&amp;", "&").
    replace("&lt;", "<").
    replace("&gt;", ">").
    replace("&quot;", "\"").
    replace("&apos;", "'").
    replace("&copy;", "(c)").
    replace("&#32;", " ").
    replace("&#33;", "!").
    replace("&#34;", "\"").
    replace("&#35;", "#").
    replace("&#36;", "$").
    replace("&#37;", "%").
    replace("&#38;", "&").
    replace("&#39;", "'").
    replace("&#40;", "(").
    replace("&#41;", ")").
    replace("&#42;", "*").
    replace("&#43;", "+").
    replace("&#44;", ",").
    replace("&#45;", "-").
    replace("&#46;", ".").
    replace("&#47;", "/").
    replace("&#032;", " ").
    replace("&#033;", "!").
    replace("&#034;", "\"").
    replace("&#035;", "#").
    replace("&#036;", "$").
    replace("&#037;", "%").
    replace("&#038;", "&").
    replace("&#039;", "'").
    replace("&#040;", "(").
    replace("&#041;", ")").
    replace("&#042;", "*").
    replace("&#043;", "+").
    replace("&#044;", ",").
    replace("&#045;", "-").
    replace("&#046;", ".").
    replace("&#047;", "/").
    replace("&#160;", " ").
    replace("&#179;", "3").
    replace("&#8211;", "-").
    replace("&#8217;", "'").
    replace("&#xF4;", "o").
    replace("&#xE3;", "a").
    replace("&#xE4;", "ae").
    replace("&#xE1;", "a").
    replace("&#xE9;", "e").
    replace("&#xED;", "i").
    replace("&#x16B;", "uu").
    replace("&#x22;", "\"").
    replace("&#x26;", "&").
    replace("&#x27;", "'").
    replace("&#xB3;", "3").
    replace("&#x14D;", "o");

  while(str.contains("&") && str.contains(";") && str.indexOf("&") < str.indexOf(";") &&
        str.indexOf(";") - str.indexOf("&") <= 10) {
    str = str.remove(str.indexOf("&"), str.indexOf(";") + 1 - str.indexOf("&"));
  }

  return str;
}

QString StrTools::xmlEscape(QString str)
{
  str = xmlUnescape(str);

  return str.
    replace("&", "&amp;").
    replace("<", "&lt;").
    replace(">", "&gt;").
    replace("\"", "&quot;").
    replace("'", "&apos;");
}

// Only to be used to encode filenames (as the "/" symbols are respected)
QString StrTools::uriEscape(QString str)
{
  return QUrl::toPercentEncoding(str, "/");
}

QString StrTools::altUriEscape(const QString &str, const QString &spaceChar)
{
  QString newName = str;
  if(spaceChar != " ") {
    // A few game names have faulty "s's". Fix them to "s'"
    // Finally change all spaces to requested char. Default is '+' since that's what most search engines seem to understand
    return newName.replace("_", " ")
                  .replace(" - ", " ")
                  .replace(",", " ")
                  .replace("&", "%26")
                  .replace("+", " ")
                  .replace("s's", "s'")
                  .replace("'", "%27")
                  .replace(" ", spaceChar)
                  .simplified();
  } else {
    return newName.simplified();
  }
}

QString StrTools::conformPlayers(const QString &str)
{
  QString strRet = str.left(str.indexOf("(")).simplified();

  if(QRegularExpression("^1 Player",
                        QRegularExpression::CaseInsensitiveOption).match(str).hasMatch())
    return "1";

  if(QRegularExpression("^1 Only",
                        QRegularExpression::CaseInsensitiveOption).match(str).hasMatch())
    return "1";

  if(QRegularExpression("^single player",
                        QRegularExpression::CaseInsensitiveOption).match(str).hasMatch())
    return "1";

  if(QRegularExpression("^1 or 2",
                        QRegularExpression::CaseInsensitiveOption).match(str).hasMatch())
    return "2";

  if(QRegularExpression("^\\d-\\d\\d").match(str).hasMatch())
    return strRet; //.mid(2, 2);

  if(QRegularExpression("^\\d-\\d").match(str).hasMatch())
    return strRet; //.mid(2, 1);

  if(QRegularExpression("^\\d - \\d\\d").match(str).hasMatch())
    return strRet.remove(' '); //.mid(4, 2);

  if(QRegularExpression("^\\d - \\d").match(str).hasMatch())
    return strRet.remove(' '); //.mid(4, 1);

  // A faulty Openretro entry is necessary as it marks "1 - 6" as "1 -6"
  if(QRegularExpression("^\\d -\\d\\d").match(str).hasMatch())
    return strRet.remove(' '); //.mid(3, 2);

  if(QRegularExpression("^\\d -\\d").match(str).hasMatch())
    return strRet.remove(' '); //.mid(3, 1);

  if(QRegularExpression("^\\d to \\d\\d").match(str).hasMatch())
    return strRet.replace(" to ", "-"); //.mid(5, 2);

  if(QRegularExpression("^\\d to \\d").match(str).hasMatch())
    return strRet.replace(" to ", "-"); //.mid(5, 1);

  if(QRegularExpression("^\\d\\+").match(str).hasMatch())
    return strRet; //.mid(0, 1);

  return strRet;
}

QString StrTools::agesLabel(const QString &str) {
  if(str == "1") {
    return "E - Everyone";
  } else if(str == "3") {
    return "EC - Early Childhood";
  } else if(str == "6") {
    return "KA - Kids to Adults";
  } else if(str == "8") {
    return "G8+";
  } else if(str == "10") {
    return "E10+ - Everyone 10+";
  } else if(str == "11") {
    return "11+";
  } else if(str == "13") {
    return "T - Teen";
  } else if(str == "15") {
    return "MA15+";
  } else if(str == "17") {
    return "MA-17";
  } else if(str == "18") {
    return "18+ - Adults Only";
  } else {
    return "";
  }
}

QString StrTools::conformAges(QString str)
{
  if(str == "0 (ohne Altersbeschränkung)") {
    str = "1";
  } else if(str == "U") {
    str = "1";
  } else if(str == "E") {
    str = "1";
  } else if(str == "E - Everyone") {
    str = "1";
  } else if(str == "Everyone") {
    str = "1";
  } else if(str == "GA") {
    str = "1";
  } else if(str == "EC") {
    str = "3";
  } else if(str == "Early Childhood") {
    str = "3";
  } else if(str == "EC - Early Childhood") {
    str = "3";
  } else if(str == "3+") {
    str = "3";
  } else if(str == "G") {
    str = "3";
  } else if(str == "KA") {
    str = "6";
  } else if(str == "Kids to Adults") {
    str = "6";
  } else if(str == "G8+") {
    str = "8";
  } else if(str == "E10+") {
    str = "10";
  } else if(str == "E10+ - Everyone 10+") {
    str = "10";
  } else if(str == "Everyone 10+") {
    str = "10";
  } else if(str == "11+") {
    str = "11";
  } else if(str == "12+") {
    str = "11";
  } else if(str == "MA-13") {
    str = "13";
  } else if(str == "T") {
    str = "13";
  } else if(str == "T - Teen") {
    str = "13";
  } else if(str == "Teen") {
    str = "13";
  } else if(str == "M") {
    str = "15";
  } else if(str == "M15+") {
    str = "15";
  } else if(str == "MA 15+") {
    str = "15";
  } else if(str == "MA15+") {
    str = "15";
  } else if(str == "PG") {
    str = "15";
  } else if(str == "15+") {
    str = "15";
  } else if(str == "MA-17") {
    str = "17";
  } else if(str == "M") {
    str = "17";
  } else if(str == "18+") {
    str = "18";
  } else if(str == "R18+") {
    str = "18";
  } else if(str == "18 (keine Jugendfreigabe)") {
    str = "18";
  } else if(str == "A") {
    str = "18";
  } else if(str == "AO") {
    str = "18";
  } else if(str == "AO - Adults Only") {
    str = "18";
  } else if(str == "Adults Only") {
    str = "18";
  } else if(str == "M - Mature") {
    str = "18";
  } else if(str == "Mature") {
    str = "18";
  }

  return str;
}

QString StrTools::conformReleaseDate(QString str)
{
  if(str.isEmpty()) {
    return str;
  } else if(QRegularExpression("^\\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[0-1]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MM").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-M").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[0-1]{1}\\d{1}-[0-3]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MM-dd").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-\\d{1}-[0-3]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-M-dd").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[0-1]{1}\\d{1}-\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MM-d").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-\\d{1}-\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-M-d").toString("yyyyMMdd");
  } else if(QRegularExpression("^[0-1]{1}\\d{1}/[0-3]{1}\\d{1}/\\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "MM/dd/yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[0-1]{1}\\d{1}/[0-3]{1}\\d{1}/[789012]{1}\\d{1}$").match(str).hasMatch()) {
    QDate strDate = QDate::fromString(str, "MM/dd/yy");
    if(strDate.isValid() && strDate.year() < 1950) {
      strDate = strDate.addYears(100);
    }
    str = strDate.toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[a-zA-Z]{3}-[0-3]{1}\\d{1}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = QDate::fromString(str, "yyyy-MMM-dd").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{3}, \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMM, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{4,9}, \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMMM, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{3} [0-3]{1}\\d{1}, \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMM dd, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{3} \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMM yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{4,9} \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMMM yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{4,9} [1-3]{0,1}\\d{1}, \\d{4}$").match(str).hasMatch()) {
    QLocale english(QLocale::English);
    str = english.toDate(str, "MMMM d, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[12]{1}[019]{1}[0-9]{2}[0-1]{1}[0-9]{1}[0-3]{1}[0-9]{1}T[0-9]{6}$").match(str).hasMatch()) {
    str = str.left(8);
  } else {
    str = QDate::fromString(str, "yyyyMMdd").toString("yyyyMMdd");
  }
  QDate finalDate = QDate::fromString(str, "yyyyMMdd");
  if(finalDate < QDate::fromString("19711231", "yyyyMMdd") ||
     finalDate > QDate::currentDate().addMonths(6) || !finalDate.isValid()) {
    if(Skyscraper::config.scraper != "cache") {
      printf("\nWARNING: Incorrect date '%s'. Ignoring.\n", str.toStdString().c_str());
    }
    return "";
  } else {
    return str;
  }
}

QString StrTools::conformTags(const QString &str)
{
  QString tags = "";
#if QT_VERSION >= 0x050e00
  QStringList tagList = str.split(',', Qt::SkipEmptyParts);
#else
  QStringList tagList = str.split(',', QString::SkipEmptyParts);
#endif
  for(auto &tag: tagList) {
    tag = tag.simplified();
    tag = tag.left(1).toUpper() + tag.mid(1, tag.length() - 1);
    tags += tag.simplified() + ", ";
  }
  tags = tags.left(tags.length() - 2);
  return tags;
}

QString StrTools::getVersionHeader()
{
  QString headerString = "Running Skyscraper v" VERSION " by Lars Muldjord";
  QString dashesString = "";
  for(int a = 0; a < headerString.length(); ++a) {
    dashesString += "-";
  }

  return QString("\033[1;34m" + dashesString + "\033[0m\n\033[1;33m" + headerString +
                 "\033[0m\n\033[1;34m" + dashesString + "\033[0m\n");
}

QString StrTools::stripBrackets(const QString &str)
{
  return str.left(str.indexOf("(")).left(str.indexOf("[")).simplified();
}

QString StrTools::sanitizeName(const QString &str, bool removeBrackets)
{
  QString sanitizedName = str;
  sanitizedName.remove(QChar('.'))
               .remove(QChar(','))
               .remove(QChar(':'))
               .remove(QChar(';'))
               .remove(QChar(' '))
               .remove(QChar('#'))
               .remove(QChar('$'))
               .remove(QChar('%'))
               .remove(QChar('='))
               .remove(QChar('-'))
               .remove(QChar('_'))
               .remove(QChar(0x2026))
               .remove(QChar('^'))
               .remove(QChar('*'))
               .remove(QChar('@'))
               .remove(QChar('~'))
               .remove(QChar(' '))
               .remove(QChar('/'))
               .remove(QChar('\\'))
               .remove(QChar('+'))
               .remove(QChar(0x2018))
               .remove(QChar(0x2019))
               .remove(QChar(0x201A))
               .remove(QChar(0x201B))
               .remove(QChar(0x201C))
               .remove(QChar(0x201D))
               .remove(QChar('"'))
               .remove(QChar('\''))
               .remove(QChar(0x00BA))
               .remove(QChar('?'))
               .remove(QChar(0x00BF))
               .remove(QChar('!'))
               .remove(QChar(0x2013))
               .replace("&", "and");
  sanitizedName = sanitizedName.toLower();
  if(removeBrackets) {
    return stripBrackets(sanitizedName).simplified();
  }
  else {
    return sanitizedName.simplified();
  }
}

QString StrTools::stripHtmlTags(QString str)
{
  while(str.contains("<") && str.contains(">") && str.indexOf("<") < str.indexOf(">")) {
    str = str.remove(str.indexOf("<"), str.indexOf(">") + 1 - str.indexOf("<"));
  }
  return str;
}

QString StrTools::getMd5Sum(const QByteArray &data)
{
  QCryptographicHash md5(QCryptographicHash::Md5);
  md5.addData(data);
  return md5.result().toHex();
}

int StrTools::distanceBetweenStrings(const QString &first, const QString &second, bool simplify)
{
  QString firstStr = simplify ? first.simplified().toLower().replace(" ", "") : first;
  QString secondStr = simplify ? second.simplified().toLower().replace(" ", "") : second;
  std::string s1 = firstStr.toStdString();
  std::string s2 = secondStr.toStdString();
  const std::size_t len1 = s1.size(), len2 = s2.size();
  unsigned int *col = (unsigned int *) malloc(sizeof(unsigned int)*(len2+1));
  unsigned int *prevCol = (unsigned int *) malloc(sizeof(unsigned int)*(len2+1));
  unsigned int *aux;

  for(unsigned int i = 0; i < len2+1; i++) {
    prevCol[i] = i;
  }
  for(unsigned int i = 0; i < len1; i++) {
    col[0] = i+1;
    for(unsigned int j = 0; j < len2; j++)
      // C++11 allows std::min({arg1, arg2, arg3}) but it's way slower
      col[j+1] = std::min(std::min(prevCol[1 + j] + 1, col[j] + 1), prevCol[j] + (s1[i]==s2[j] ? 0 : 1));
    aux = col;
    col = prevCol;
    prevCol = aux;
  }
  int distance = prevCol[len2];
  free(col);
  free(prevCol);
  return distance;
}

QString StrTools::onlyNumbers(const QString &str)
{
  QString result;
  for(const auto &c: std::as_const(str)) {
    if(c.isDigit()) {
      result.append(c);
    }
  }
  return result;
}

bool StrTools::readCSVRow(QTextStream &in, QStringList *row)
{
  static const int delta[][5] = {
        //  ,    "   \n    ?  eof
        {   1,   2,  -1,   0,  -1  }, // 0: parsing (store char)
        {   1,   2,  -1,   0,  -1  }, // 1: parsing (store column)
        {   3,   4,   3,   3,  -2  }, // 2: quote entered (no-op)
        {   3,   4,   3,   3,  -2  }, // 3: parsing inside quotes (store char)
        {   1,   3,  -1,   0,  -1  }, // 4: quote exited (no-op)
        // -1: end of row, store column, success
        // -2: eof inside quotes
  };

  row->clear();

  if(in.atEnd()) {
    return false;
  }

  int state = 0, t;
  char ch;
  QString cell;

  while(state >= 0) {
    if(in.atEnd()) {
      t = 4;
    }
    else {
      in >> ch;
      if(ch == ',') t = 0;
      else if(ch == '\"') t = 1;
      else if(ch == '\n') t = 2;
      else t = 3;
    }

    state = delta[state][t];
    if(state == 0 || state == 3) {
      cell += ch;
    } else if(state == -1 || state == 1) {
      row->append(cell);
      cell = "";
    }
  }

  if(state == -2) {
    printf("ERROR: End-of-file found while inside quotes.\n");
  }
  return true;
}

QString StrTools::simplifyLetters(const QString &str)
{
  QString simplified = str.normalized(QString::NormalizationForm_KD);
  //simplified.remove(QRegExp("[^a-zA-Z\\s]"));
  return simplified;
}
