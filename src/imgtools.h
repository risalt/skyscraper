/***************************************************************************
 *            imgtools.h
 *
 *  Tue Feb 6 12:00:00 CEST 2018
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

#ifndef IMGTOOLS_H
#define IMGTOOLS_H

#include <QObject>
#include <QImage>

class ImgTools : public QObject
{
public:
  static QImage cropToFit(const QImage &image, bool cropBlack = false);

};

#endif // IMGTOOLS_H
