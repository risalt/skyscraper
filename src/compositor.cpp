/***************************************************************************
 *            compositor.cpp
 *
 *  Wed Jun 18 12:00:00 CEST 2017
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

#include <cmath>
#include <QDebug>
#include <QSettings>
#include <QPainter>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>

#include "compositor.h"
#include "strtools.h"
#include "imgtools.h"

#include "fxshadow.h"
#include "fxblur.h"
#include "fxmask.h"
#include "fxframe.h"
#include "fxrounded.h"
#include "fxstroke.h"
#include "fxbrightness.h"
#include "fxcontrast.h"
#include "fxbalance.h"
#include "fxopacity.h"
#include "fxgamebox.h"
#include "fxhue.h"
#include "fxsaturation.h"
#include "fxcolorize.h"
#include "fxrotate.h"
#include "fxscanlines.h"

Compositor::Compositor(Settings *config)
{
  this->config = config;
}

bool Compositor::processXml()
{
  Layer newOutputs;

  // Check document for errors before running through it
  QDomDocument doc;
  if(!doc.setContent(config->artworkXml))
     return false;

  QXmlStreamReader xml(config->artworkXml);

  // Init recursive parsing
  addChildLayers(newOutputs, xml);

  // Assign global outputs to these new outputs
  outputs = newOutputs;
  return true;
}

void Compositor::addChildLayers(Layer &layer, QXmlStreamReader &xml)
{
  while(xml.readNext() && !xml.atEnd()) {
    Layer newLayer;
    if(xml.isStartElement() && xml.name() == "output") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("type")) {
        newLayer.setType(T_OUTPUT);
        newLayer.setResType(attribs.value("type").toString());
        if(attribs.hasAttribute("resource")) {
          newLayer.setResource(attribs.value("resource").toString());
        } else {
          newLayer.setResource(attribs.value("type").toString());
        }
      }
      if(attribs.hasAttribute("width"))
        newLayer.setWidth(attribs.value("width").toInt());
      if(attribs.hasAttribute("height"))
        newLayer.setHeight(attribs.value("height").toInt());
      if(attribs.hasAttribute("mpixels"))
        newLayer.setMPixels(attribs.value("mpixels").toDouble());

      if(newLayer.type != T_NONE) {
        addChildLayers(newLayer, xml);
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "layer") {
      QXmlStreamAttributes attribs = xml.attributes();
      newLayer.setType(T_LAYER);
      if(attribs.hasAttribute("resource"))
        newLayer.setResource(attribs.value("resource").toString());
      if(attribs.hasAttribute("mode"))
        newLayer.setMode(attribs.value("mode").toString());
      if(attribs.hasAttribute("opacity"))
        newLayer.setOpacity(attribs.value("opacity").toInt());
      if(attribs.hasAttribute("width"))
        newLayer.setWidth(attribs.value("width").toInt());
      if(attribs.hasAttribute("height"))
        newLayer.setHeight(attribs.value("height").toInt());
      if(attribs.hasAttribute("mpixels"))
        newLayer.setMPixels(attribs.value("mpixels").toDouble());
      if(attribs.hasAttribute("align"))
        newLayer.setAlign(attribs.value("align").toString());
      if(attribs.hasAttribute("valign"))
        newLayer.setVAlign(attribs.value("valign").toString());
      if(attribs.hasAttribute("x"))
        newLayer.setX(attribs.value("x").toInt());
      if(attribs.hasAttribute("y"))
        newLayer.setY(attribs.value("y").toInt());
      addChildLayers(newLayer, xml);
      layer.addLayer(newLayer);
    } else if(xml.isStartElement() && xml.name() == "shadow") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("distance") &&
         attribs.hasAttribute("softness") &&
         attribs.hasAttribute("opacity")) {
        newLayer.setType(T_SHADOW);
        newLayer.setDistance(attribs.value("distance").toInt());
        newLayer.setSoftness(attribs.value("softness").toInt());
        newLayer.setOpacity(attribs.value("opacity").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "blur") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("softness")) {
        newLayer.setType(T_BLUR);
        newLayer.setSoftness(attribs.value("softness").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "mask") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("file")) {
        newLayer.setType(T_MASK);
        newLayer.setResource(attribs.value("file").toString());
        if(attribs.hasAttribute("width"))
          newLayer.setWidth(attribs.value("width").toInt());
        if(attribs.hasAttribute("height"))
          newLayer.setHeight(attribs.value("height").toInt());
        if(attribs.hasAttribute("x"))
          newLayer.setX(attribs.value("x").toInt());
        if(attribs.hasAttribute("y"))
          newLayer.setY(attribs.value("y").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "frame") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("file")) {
        newLayer.setType(T_FRAME);
        newLayer.setResource(attribs.value("file").toString());
        if(attribs.hasAttribute("width"))
          newLayer.setWidth(attribs.value("width").toInt());
        if(attribs.hasAttribute("height"))
          newLayer.setHeight(attribs.value("height").toInt());
        if(attribs.hasAttribute("x"))
          newLayer.setX(attribs.value("x").toInt());
        if(attribs.hasAttribute("y"))
          newLayer.setY(attribs.value("y").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "stroke") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("width")) {
        newLayer.setType(T_STROKE);
        newLayer.setWidth(attribs.value("width").toInt());
        if(attribs.hasAttribute("color"))
          newLayer.colorFromHex(attribs.value("color").toString());
        if(attribs.hasAttribute("red"))
          newLayer.setRed(attribs.value("red").toInt());
        if(attribs.hasAttribute("green"))
          newLayer.setGreen(attribs.value("green").toInt());
        if(attribs.hasAttribute("blue"))
          newLayer.setBlue(attribs.value("blue").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "rounded") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("radius")) {
        newLayer.setType(T_ROUNDED);
        newLayer.setWidth(attribs.value("radius").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "brightness") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("value")) {
        newLayer.setType(T_BRIGHTNESS);
        newLayer.setDelta(attribs.value("value").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "opacity") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("value")) {
        newLayer.setType(T_OPACITY);
        newLayer.setOpacity(attribs.value("value").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "contrast") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("value")) {
        newLayer.setType(T_CONTRAST);
        newLayer.setDelta(attribs.value("value").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "balance") {
      QXmlStreamAttributes attribs = xml.attributes();
      newLayer.setType(T_BALANCE);
      if(attribs.hasAttribute("red"))
        newLayer.setRed(attribs.value("red").toInt());
      if(attribs.hasAttribute("green"))
        newLayer.setGreen(attribs.value("green").toInt());
      if(attribs.hasAttribute("blue"))
        newLayer.setBlue(attribs.value("blue").toInt());
      layer.addLayer(newLayer);
    } else if(xml.isStartElement() && xml.name() == "gamebox") {
      QXmlStreamAttributes attribs = xml.attributes();
      newLayer.setType(T_GAMEBOX);
      if(attribs.hasAttribute("side"))
        newLayer.setResource(attribs.value("side").toString());
      if(attribs.hasAttribute("rotate"))
        newLayer.setDelta(attribs.value("rotate").toInt());
      if(attribs.hasAttribute("sidescaling"))
        newLayer.setScaling(attribs.value("sidescaling").toString());
      layer.addLayer(newLayer);
    } else if(xml.isStartElement() && xml.name() == "hue") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("value")) {
        newLayer.setType(T_HUE);
        newLayer.setDelta(attribs.value("value").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "saturation") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("value")) {
        newLayer.setType(T_SATURATION);
        newLayer.setDelta(attribs.value("value").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "colorize") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("hue")) {
        newLayer.setType(T_COLORIZE);
        newLayer.setValue(attribs.value("hue").toInt());
        if(attribs.hasAttribute("saturation"))
          newLayer.setDelta(attribs.value("saturation").toInt());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "rotate") {
      QXmlStreamAttributes attribs = xml.attributes();
      if(attribs.hasAttribute("degrees")) {
        newLayer.setType(T_ROTATE);
        newLayer.setDelta(attribs.value("degrees").toInt());
        if(attribs.hasAttribute("axis"))
          newLayer.setAxis(attribs.value("axis").toString());
        layer.addLayer(newLayer);
      }
    } else if(xml.isStartElement() && xml.name() == "scanlines") {
      QXmlStreamAttributes attribs = xml.attributes();
      newLayer.setType(T_SCANLINES);
      if(attribs.hasAttribute("file")) {
        newLayer.setResource(attribs.value("file").toString());
      }
      if(attribs.hasAttribute("scale")) {
        newLayer.setScaling(attribs.value("scale").toString());
      }
      if(attribs.hasAttribute("opacity")) {
        newLayer.setOpacity(attribs.value("opacity").toInt());
      }
      if(attribs.hasAttribute("mode")) {
        newLayer.setMode(attribs.value("mode").toString());
      } else {
        newLayer.setMode("overlay");
      }
      layer.addLayer(newLayer);
    } else if(xml.isEndElement() && xml.name() == "layer") {
      return;
    } else if(xml.isEndElement() && xml.name() == "output") {
      return;
    }
  }
}

void Compositor::saveAll(GameEntry &game, QString completeBaseName)
{
  for(auto &output: outputs.getLayers()) {
    QString filename = "/" + completeBaseName + ".png";
    bool generateThumbnail = false;
    bool isAutogenerated = false;
    if(output.resType == "cover") {
      filename.prepend(config->coversFolder);
      generateThumbnail = false;
      if(config->skipExistingCovers && QFileInfo::exists(filename)) {
        isAutogenerated = false;
        QImage exif(filename);
        if(!exif.isNull()) {
          QString softwareTag = exif.text("Software");
          // printf("3 '%s'\n", softwareTag.toStdString().c_str());
          if(!softwareTag.isEmpty() && softwareTag == "www.inkscape.org") {
            // Real covers always have priority over autogenerated ones:
            isAutogenerated = true;
          }
        }
        if((isAutogenerated && game.coverData.isEmpty()) ||
           (!isAutogenerated && !game.coverData.isEmpty() && !config->symlinkImages)) {
          game.coverFile = filename;
          continue;
        } else if(!isAutogenerated && game.coverData.isEmpty()) {
          generateThumbnail = true;
        }
      } else if(game.coverData.isEmpty()) {
        generateThumbnail = true;
      }
      if(generateThumbnail) {
        QString sanitizedName = game.title;
        if(sanitizedName.isEmpty()) {
          sanitizedName = "N/A";
        }
        QProcess thumbnailer;
        QString convertcommand("/usr/bin/convert");
        QStringList convertargs;
        QString thumbnail;
        QString templateimage("/opt/pegasus/roms/default/missingboxart.png");
        convertargs << templateimage
                    << "-size" << "184x256" << "-pointsize" << "30" << "-background" << "none" << "-fill" << "gray10" << "-gravity" << "South"
                    << "caption:" + sanitizedName
                    << "-flatten"
                    << filename;
        thumbnailer.start(convertcommand, convertargs, QIODevice::ReadOnly);
        thumbnailer.waitForFinished(10000);
        thumbnailer.close();
        game.coverFile = filename;
        continue;
      }
    } else if(output.resType == "screenshot") {
      filename.prepend(config->screenshotsFolder);
      if(game.screenshotData.isEmpty() && QFileInfo::exists(filename)) {
        QFile::remove(filename);
      } else if(config->skipExistingScreenshots &&
              !game.screenshotData.isEmpty() &&
              QFileInfo::exists(filename)) {
        game.screenshotFile = filename;
        continue;
      }
    } else if(output.resType == "wheel") {
      filename.prepend(config->wheelsFolder);
      if(game.wheelData.isEmpty() && QFileInfo::exists(filename)) {
        QFile::remove(filename);
      } else if(config->skipExistingWheels &&
              !game.wheelData.isEmpty() &&
              QFileInfo::exists(filename)) {
        game.wheelFile = filename;
        continue;
      }
    } else if(output.resType == "marquee") {
      filename.prepend(config->marqueesFolder);
      if(game.marqueeData.isEmpty() && QFileInfo::exists(filename)) {
        QFile::remove(filename);
      } else if(config->skipExistingMarquees &&
              !game.marqueeData.isEmpty() &&
              QFileInfo::exists(filename)) {
        game.marqueeFile = filename;
        continue;
      }
    } else if(output.resType == "texture") {
      filename.prepend(config->texturesFolder);
      if(game.textureData.isEmpty() && QFileInfo::exists(filename)) {
        QFile::remove(filename);
      } else if(config->skipExistingTextures &&
              !game.textureData.isEmpty() &&
              QFileInfo::exists(filename)) {
        game.textureFile = filename;
        continue;
      }
    }

    if(output.resource == "cover") {
      if(config->symlinkImages && !generateThumbnail && !isAutogenerated) {
        QFileInfo file(filename);
        bool createLink = false;
        QString sourceFile;
        sourceFile = game.coverFile;
        if(!sourceFile.isEmpty() && QFileInfo::exists(sourceFile)) {
          game.coverFile = filename;
        } else {
          if(file.exists()) {
            QFile::remove(filename);
          }
          continue;
        }
        if(file.exists()) {
          if(!file.isSymbolicLink()) {
            QFile::remove(filename);
            createLink = true;
          } else if(file.symLinkTarget() != sourceFile) {
            QFile::remove(filename);
            createLink = true;
          }
        } else {
          createLink = true;
        }
        if(createLink) {
          QFile::link(sourceFile, filename);
        }
      } else {
        output.setCanvas(QImage::fromData(game.coverData));
        if(output.canvas.isNull() && output.hasLayers()) {
          QImage tmpImage(10, 10, QImage::Format_ARGB32_Premultiplied);
          output.setCanvas(tmpImage);
        }
        output.premultiply();
        output.scale();
        if(output.hasLayers()) {
          // Reset output.canvas since composite layers exist
          output.makeTransparent();
          // Initiate recursive compositing
          processChildLayers(game, output);
        }
        if(output.save(filename)) {
          game.coverFile = filename;
        }
      }
    } else {
      if(config->symlinkImages) {
        QFileInfo file(filename);
        bool createLink = false;
        QString sourceFile;
        if(output.resource == "screenshot") {
          sourceFile = game.screenshotFile;
          if(!sourceFile.isEmpty() && QFileInfo::exists(sourceFile)) {
            game.screenshotFile = filename;
          } else {
            if(file.exists()) {
              QFile::remove(filename);
            }
            continue;
          }
        } else if(output.resource == "wheel") {
          sourceFile = game.wheelFile;
          if(!sourceFile.isEmpty() && QFileInfo::exists(sourceFile)) {
            game.wheelFile = filename;
          } else {
            if(file.exists()) {
              QFile::remove(filename);
            }
            continue;
          }
        } else if(output.resource == "marquee") {
          sourceFile = game.marqueeFile;
          if(!sourceFile.isEmpty() && QFileInfo::exists(sourceFile)) {
            game.marqueeFile = filename;
          } else {
            if(file.exists()) {
              QFile::remove(filename);
            }
            continue;
          }
        } else if(output.resource == "texture") {
          sourceFile = game.textureFile;
          if(!sourceFile.isEmpty() && QFileInfo::exists(sourceFile)) {
            game.textureFile = filename;
          } else {
            if(file.exists()) {
              QFile::remove(filename);
            }
            continue;
          }
        } else {
          continue;
        }
        if(file.exists()) {
          if(!file.isSymbolicLink()) {
            QFile::remove(filename);
            createLink = true;
          } else if(file.symLinkTarget() != sourceFile) {
            QFile::remove(filename);
            createLink = true;
          }
        } else {
          createLink = true;
        }
        if(createLink) {
          if(!QFile::link(sourceFile, filename)) {
            printf("ERROR: Could not link '%s' to '%s' for resource type '%s'.\n",
                   sourceFile.toStdString().c_str(), filename.toStdString().c_str(),
                   output.resource.toStdString().c_str());
          }
        }
      } else {
        if(output.resource == "screenshot") {
          output.setCanvas(QImage::fromData(game.screenshotData));
        } else if(output.resource == "wheel") {
          output.setCanvas(QImage::fromData(game.wheelData));
        } else if(output.resource == "marquee") {
          output.setCanvas(QImage::fromData(game.marqueeData));
        } else if(output.resource == "texture") {
          output.setCanvas(QImage::fromData(game.textureData));
        }

        if(output.canvas.isNull() && output.hasLayers()) {
          QImage tmpImage(10, 10, QImage::Format_ARGB32_Premultiplied);
          output.setCanvas(tmpImage);
        }

        output.premultiply();
        output.scale();

        if(output.hasLayers()) {
          // Reset output.canvas since composite layers exist
          output.makeTransparent();
          // Initiate recursive compositing
          processChildLayers(game, output);
        }

        if(output.resType == "screenshot" && output.save(filename)) {
          game.screenshotFile = filename;
        } else if(output.resType == "wheel" && output.save(filename)) {
          game.wheelFile = filename;
        } else if(output.resType == "marquee" && output.save(filename)) {
          game.marqueeFile = filename;
        } else if(output.resType == "texture" && output.save(filename)) {
          game.textureFile = filename;
        }
      }
    }
  }
}

void Compositor::processChildLayers(GameEntry &game, Layer &layer)
{
  for(int a = 0; a < layer.getLayers().length(); ++a) {
    // Create new layer and set canvas to relevant resource (or empty if left out in xml)
    Layer thisLayer = layer.getLayers().at(a);
    if(thisLayer.type == T_LAYER) {
      if(thisLayer.resource == "") {
        QImage emptyCanvas(1, 1, QImage::Format_ARGB32_Premultiplied);
        emptyCanvas.fill(Qt::transparent);
        thisLayer.setCanvas(emptyCanvas);
      } else if(thisLayer.resource == "cover") {
        thisLayer.setCanvas(QImage::fromData(game.coverData));
      } else if(thisLayer.resource == "screenshot") {
          thisLayer.setCanvas(QImage::fromData(game.screenshotData));
      } else if(thisLayer.resource == "wheel") {
          thisLayer.setCanvas(QImage::fromData(game.wheelData));
      } else if(thisLayer.resource == "marquee") {
          thisLayer.setCanvas(QImage::fromData(game.marqueeData));
      } else if(thisLayer.resource == "texture") {
          thisLayer.setCanvas(QImage::fromData(game.textureData));
      } else {
        thisLayer.setCanvas(config->resources[thisLayer.resource]);
      }

      // If no meaningful canvas could be created, stop processing this layer branch entirely
      if(thisLayer.canvas.isNull()) {
        continue;
      }

      thisLayer.premultiply();
      if(thisLayer.resource == "screenshot") {
        // Crop away transparency and, if configured, black borders around screenshots
        thisLayer.setCanvas(ImgTools::cropToFit(thisLayer.canvas, config->cropBlack));
      } else {
        // Crop away transparency around all other types. Never crop black on these as many
        // have black outlines that are very much needed
        thisLayer.setCanvas(ImgTools::cropToFit(thisLayer.canvas));
      }
      thisLayer.scale();

      // Update width + height as we will need them for easier placement and alignment
      thisLayer.updateSize();

      // Continue concurrency if this layer has children
      if(thisLayer.hasLayers()) {
        processChildLayers(game, thisLayer);
      }

      // Composite image on canvas (which is the parent canvas at this point)
      QPainter painter;
      painter.begin(&layer.canvas);
      painter.setCompositionMode(thisLayer.mode);
      if(thisLayer.opacity != -1)
        painter.setOpacity(thisLayer.opacity * 0.01);

      int x = 0;
      if(thisLayer.align == "center") {
        x = (layer.width / 2) - (thisLayer.width / 2);
      } else if(thisLayer.align == "right") {
        x = layer.width - thisLayer.width;
      }
      x += thisLayer.x;

      int y = 0;
      if(thisLayer.valign == "middle") {
        y = (layer.height / 2) - (thisLayer.height / 2);
      } else if(thisLayer.valign == "bottom") {
        y = layer.height - thisLayer.height;
      }
      y += thisLayer.y;

      painter.drawImage(x, y, thisLayer.canvas);
      painter.end();

    } else if(thisLayer.type == T_SHADOW) {
      FxShadow effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_BLUR) {
      FxBlur effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_MASK) {
      FxMask effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer, config));
    } else if(thisLayer.type == T_FRAME) {
      FxFrame effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer, config));
    } else if(thisLayer.type == T_STROKE) {
      FxStroke effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_ROUNDED) {
      FxRounded effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_BRIGHTNESS) {
      FxBrightness effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_CONTRAST) {
      FxContrast effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_BALANCE) {
      FxBalance effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_OPACITY) {
      FxOpacity effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_GAMEBOX) {
      FxGamebox effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer, game, config));
    } else if(thisLayer.type == T_HUE) {
      FxHue effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_SATURATION) {
      FxSaturation effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_COLORIZE) {
      FxColorize effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_ROTATE) {
      FxRotate effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
    } else if(thisLayer.type == T_SCANLINES) {
      FxScanlines effect;
      layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer, config));
    }
    // Update width and height only for effects that change the dimensions in a way that
    // necessitates an update. For instance T_SHADOW does NOT require an update since we don't
    // want the alignment of the layer to take the shadow into consideration.
    if(thisLayer.type == T_STROKE ||
       thisLayer.type == T_ROTATE ||
       thisLayer.type == T_GAMEBOX) {
      layer.updateSize();
    }
  }
}
