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
#include <QRegularExpression>
#include <QCryptographicHash>

#include "strtools.h"

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

QString StrTools::uriEscape(QString str)
{
  return QUrl::toPercentEncoding(str, "/");
}

QString StrTools::conformPlayers(const QString &str)
{
  if(QRegularExpression("^1 Player").match(str).hasMatch())
    return "1";

  if(QRegularExpression("^1 Only").match(str).hasMatch())
    return "1";

  if(QRegularExpression("^single player").match(str).hasMatch())
    return "1";

  if(QRegularExpression("^1 or 2").match(str).hasMatch())
    return "2";

  if(QRegularExpression("^\\d-\\d\\d").match(str).hasMatch())
    return str.mid(2, 2);

  if(QRegularExpression("^\\d-\\d").match(str).hasMatch())
    return str.mid(2, 1);

  if(QRegularExpression("^\\d - \\d\\d").match(str).hasMatch())
    return str.mid(4, 2);

  if(QRegularExpression("^\\d - \\d").match(str).hasMatch())
    return str.mid(4, 1);

  // A faulty Openretro entry is necessary as it marks "1 - 6" as "1 -6"
  if(QRegularExpression("^\\d -\\d\\d").match(str).hasMatch())
    return str.mid(3, 2);

  if(QRegularExpression("^\\d -\\d").match(str).hasMatch())
    return str.mid(3, 1);

  if(QRegularExpression("^\\d to \\d\\d").match(str).hasMatch())
    return str.mid(5, 2);

  if(QRegularExpression("^\\d to \\d").match(str).hasMatch())
    return str.mid(5, 1);

  if(QRegularExpression("^\\d\\+").match(str).hasMatch())
    return str.mid(0, 1);

  return str;
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
  if(QRegularExpression("^\\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[0-1]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MM").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[0-1]{1}\\d{1}-[0-3]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MM-dd").toString("yyyyMMdd");
  } else if(QRegularExpression("^[0-1]{1}\\d{1}/[0-3]{1}\\d{1}/\\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "MM/dd/yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^\\d{4}-[a-zA-Z]{3}-[0-3]{1}\\d{1}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "yyyy-MMM-dd").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{3}, \\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "MMM, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[a-zA-z]{3} [0-3]{1}\\d{1}, \\d{4}$").match(str).hasMatch()) {
    str = QDate::fromString(str, "MMM dd, yyyy").toString("yyyyMMdd");
  } else if(QRegularExpression("^[12]{1}[019]{1}[0-9]{2}[0-1]{1}[0-9]{1}[0-3]{1}[0-9]{1}T[0-9]{6}$").match(str).hasMatch()) {
    str = str.left(8);
  }
  return str;
}

QString StrTools::conformTags(const QString &str)
{
  QString tags = "";
#if QT_VERSION >= 0x050e00
  QList<QString> tagList = str.split(',', Qt::SkipEmptyParts);
#else
  QList<QString> tagList = str.split(',', QString::SkipEmptyParts);
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

  return QString("\033[1;34m" + dashesString + "\033[0m\n\033[1;33m" + headerString + "\033[0m\n\033[1;34m" + dashesString + "\033[0m\n");
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
  if (removeBrackets) {
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
        
  for (unsigned int i = 0; i < len2+1; i++)
    prevCol[i] = i;
  for (unsigned int i = 0; i < len1; i++) {
    col[0] = i+1;
    for (unsigned int j = 0; j < len2; j++)
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
  for (const QChar c : qAsConst(str)) {
    if (c.isDigit()) {
      result.append(c);
    }
  }
  return result;
}
