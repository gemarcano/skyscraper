/***************************************************************************
 *            compositor.cpp
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

#include "compositor.h"

#include "fxbalance.h"
#include "fxblur.h"
#include "fxbrightness.h"
#include "fxcolorize.h"
#include "fxcontrast.h"
#include "fxframe.h"
#include "fxgamebox.h"
#include "fxhue.h"
#include "fxmask.h"
#include "fxopacity.h"
#include "fxrotate.h"
#include "fxrounded.h"
#include "fxsaturation.h"
#include "fxscanlines.h"
#include "fxshadow.h"
#include "fxstroke.h"
#include "gameentry.h"
#include "imgtools.h"
#include "strtools.h"

#include <QDebug>
#include <QDir>
#include <QDomDocument>
#include <QFileInfo>
#include <QPainter>
#include <QSettings>
#include <QStringBuilder>
#include <cmath>

Compositor::Compositor(Settings *config) { this->config = config; }

bool Compositor::processXml() {
    Layer newOutputs;

    // Check document for errors before running through it
    QDomDocument doc;
    if (!doc.setContent(config->artworkXml))
        return false;

    QXmlStreamReader xml(config->artworkXml);

    // Init recursive parsing
    addChildLayers(newOutputs, xml);

    // Assign global outputs to these new outputs
    outputs = newOutputs;
    return true;
}

void Compositor::addChildLayers(Layer &layer, QXmlStreamReader &xml) {
    while (xml.readNext() && !xml.atEnd()) {
        Layer newLayer;
        if (xml.isStartElement() && xml.name() == QLatin1String("output")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("type")) {
                newLayer.setType(T_OUTPUT);
                newLayer.setResType(attribs.value("type").toString());
                if (attribs.hasAttribute("resource")) {
                    newLayer.setResource(attribs.value("resource").toString());
                } else {
                    newLayer.setResource(attribs.value("type").toString());
                }
            }
            if (attribs.hasAttribute("width"))
                newLayer.setWidth(attribs.value("width").toInt());
            if (attribs.hasAttribute("height"))
                newLayer.setHeight(attribs.value("height").toInt());
            if (attribs.hasAttribute("mpixels"))
                newLayer.setMPixels(attribs.value("mpixels").toDouble());

            if (newLayer.type != T_NONE) {
                addChildLayers(newLayer, xml);
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("layer")) {
            QXmlStreamAttributes attribs = xml.attributes();
            newLayer.setType(T_LAYER);
            if (attribs.hasAttribute("resource"))
                newLayer.setResource(attribs.value("resource").toString());
            if (attribs.hasAttribute("mode"))
                newLayer.setMode(attribs.value("mode").toString());
            if (attribs.hasAttribute("opacity"))
                newLayer.setOpacity(attribs.value("opacity").toInt());
            if (attribs.hasAttribute("width"))
                newLayer.setWidth(attribs.value("width").toInt());
            if (attribs.hasAttribute("height"))
                newLayer.setHeight(attribs.value("height").toInt());
            if (attribs.hasAttribute("mpixels"))
                newLayer.setMPixels(attribs.value("mpixels").toDouble());
            if (attribs.hasAttribute("align"))
                newLayer.setAlign(attribs.value("align").toString());
            if (attribs.hasAttribute("valign"))
                newLayer.setVAlign(attribs.value("valign").toString());
            if (attribs.hasAttribute("x"))
                newLayer.setX(attribs.value("x").toInt());
            if (attribs.hasAttribute("y"))
                newLayer.setY(attribs.value("y").toInt());
            addChildLayers(newLayer, xml);
            layer.addLayer(newLayer);
        } else if (xml.isStartElement() && xml.name() == QLatin1String("shadow")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("distance") &&
                attribs.hasAttribute("softness") &&
                attribs.hasAttribute("opacity")) {
                newLayer.setType(T_SHADOW);
                newLayer.setDistance(attribs.value("distance").toInt());
                newLayer.setSoftness(attribs.value("softness").toInt());
                newLayer.setOpacity(attribs.value("opacity").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("blur")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("softness")) {
                newLayer.setType(T_BLUR);
                newLayer.setSoftness(attribs.value("softness").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("mask")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("file")) {
                newLayer.setType(T_MASK);
                newLayer.setResource(attribs.value("file").toString());
                if (attribs.hasAttribute("width"))
                    newLayer.setWidth(attribs.value("width").toInt());
                if (attribs.hasAttribute("height"))
                    newLayer.setHeight(attribs.value("height").toInt());
                if (attribs.hasAttribute("x"))
                    newLayer.setX(attribs.value("x").toInt());
                if (attribs.hasAttribute("y"))
                    newLayer.setY(attribs.value("y").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("frame")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("file")) {
                newLayer.setType(T_FRAME);
                newLayer.setResource(attribs.value("file").toString());
                if (attribs.hasAttribute("width"))
                    newLayer.setWidth(attribs.value("width").toInt());
                if (attribs.hasAttribute("height"))
                    newLayer.setHeight(attribs.value("height").toInt());
                if (attribs.hasAttribute("x"))
                    newLayer.setX(attribs.value("x").toInt());
                if (attribs.hasAttribute("y"))
                    newLayer.setY(attribs.value("y").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("stroke")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("width")) {
                newLayer.setType(T_STROKE);
                newLayer.setWidth(attribs.value("width").toInt());
                if (attribs.hasAttribute("color"))
                    newLayer.colorFromHex(attribs.value("color").toString());
                if (attribs.hasAttribute("red"))
                    newLayer.setRed(attribs.value("red").toInt());
                if (attribs.hasAttribute("green"))
                    newLayer.setGreen(attribs.value("green").toInt());
                if (attribs.hasAttribute("blue"))
                    newLayer.setBlue(attribs.value("blue").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("rounded")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("radius")) {
                newLayer.setType(T_ROUNDED);
                newLayer.setWidth(attribs.value("radius").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("brightness")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("value")) {
                newLayer.setType(T_BRIGHTNESS);
                newLayer.setDelta(attribs.value("value").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("opacity")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("value")) {
                newLayer.setType(T_OPACITY);
                newLayer.setOpacity(attribs.value("value").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("contrast")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("value")) {
                newLayer.setType(T_CONTRAST);
                newLayer.setDelta(attribs.value("value").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("balance")) {
            QXmlStreamAttributes attribs = xml.attributes();
            newLayer.setType(T_BALANCE);
            if (attribs.hasAttribute("red"))
                newLayer.setRed(attribs.value("red").toInt());
            if (attribs.hasAttribute("green"))
                newLayer.setGreen(attribs.value("green").toInt());
            if (attribs.hasAttribute("blue"))
                newLayer.setBlue(attribs.value("blue").toInt());
            layer.addLayer(newLayer);
        } else if (xml.isStartElement() && xml.name() == QLatin1String("gamebox")) {
            QXmlStreamAttributes attribs = xml.attributes();
            newLayer.setType(T_GAMEBOX);
            if (attribs.hasAttribute("side"))
                newLayer.setResource(attribs.value("side").toString());
            if (attribs.hasAttribute("rotate"))
                newLayer.setDelta(attribs.value("rotate").toInt());
            if (attribs.hasAttribute("sidescaling"))
                newLayer.setScaling(attribs.value("sidescaling").toString());
            layer.addLayer(newLayer);
        } else if (xml.isStartElement() && xml.name() == QLatin1String("hue")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("value")) {
                newLayer.setType(T_HUE);
                newLayer.setDelta(attribs.value("value").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("saturation")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("value")) {
                newLayer.setType(T_SATURATION);
                newLayer.setDelta(attribs.value("value").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("colorize")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("hue")) {
                newLayer.setType(T_COLORIZE);
                newLayer.setValue(attribs.value("hue").toInt());
                if (attribs.hasAttribute("saturation"))
                    newLayer.setDelta(attribs.value("saturation").toInt());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("rotate")) {
            QXmlStreamAttributes attribs = xml.attributes();
            if (attribs.hasAttribute("degrees")) {
                newLayer.setType(T_ROTATE);
                newLayer.setDelta(attribs.value("degrees").toInt());
                if (attribs.hasAttribute("axis"))
                    newLayer.setAxis(attribs.value("axis").toString());
                layer.addLayer(newLayer);
            }
        } else if (xml.isStartElement() && xml.name() == QLatin1String("scanlines")) {
            QXmlStreamAttributes attribs = xml.attributes();
            newLayer.setType(T_SCANLINES);
            if (attribs.hasAttribute("file")) {
                newLayer.setResource(attribs.value("file").toString());
            }
            if (attribs.hasAttribute("scale")) {
                newLayer.setScaling(attribs.value("scale").toString());
            }
            if (attribs.hasAttribute("opacity")) {
                newLayer.setOpacity(attribs.value("opacity").toInt());
            }
            if (attribs.hasAttribute("mode")) {
                newLayer.setMode(attribs.value("mode").toString());
            } else {
                newLayer.setMode("overlay");
            }
            layer.addLayer(newLayer);
        } else if (xml.isEndElement() && xml.name() == QLatin1String("layer")) {
            return;
        } else if (xml.isEndElement() && xml.name() == QLatin1String("output")) {
            return;
        }
    }
}

void Compositor::saveAll(GameEntry &game, QString completeBaseName) {
    bool createSubfolder = false;
    QString fn = "/" % completeBaseName % ".png";
    QString subPath = getSubpath(game.path);
    if (subPath != ".") {
        fn.prepend("/" % subPath);
        createSubfolder = true;
    }

    for (auto &output : outputs.getLayers()) {
        QString filename = fn;
        if (output.resType == QLatin1String("cover")) {
            filename.prepend(config->coversFolder);
            if (config->skipExistingCovers && QFileInfo::exists(filename)) {
                game.coverFile = filename;
                continue;
            }
        } else if (output.resType == QLatin1String("screenshot")) {
            filename.prepend(config->screenshotsFolder);
            if (config->skipExistingScreenshots &&
                QFileInfo::exists(filename)) {
                game.screenshotFile = filename;
                continue;
            }
        } else if (output.resType == QLatin1String("wheel")) {
            filename.prepend(config->wheelsFolder);
            if (config->skipExistingWheels && QFileInfo::exists(filename)) {
                game.wheelFile = filename;
                continue;
            }
        } else if (output.resType == QLatin1String("marquee")) {
            filename.prepend(config->marqueesFolder);
            if (config->skipExistingMarquees && QFileInfo::exists(filename)) {
                game.marqueeFile = filename;
                continue;
            }
        } else if (output.resType == QLatin1String("texture")) {
            filename.prepend(config->texturesFolder);
            if (config->skipExistingTextures && QFileInfo::exists(filename)) {
                game.textureFile = filename;
                continue;
            }
        }

        if (output.resource == QLatin1String("cover")) {
            output.setCanvas(QImage::fromData(game.coverData));
        } else if (output.resource == QLatin1String("screenshot")) {
            output.setCanvas(QImage::fromData(game.screenshotData));
        } else if (output.resource == QLatin1String("wheel")) {
            output.setCanvas(QImage::fromData(game.wheelData));
        } else if (output.resource == QLatin1String("marquee")) {
            output.setCanvas(QImage::fromData(game.marqueeData));
        } else if (output.resource == QLatin1String("texture")) {
            output.setCanvas(QImage::fromData(game.textureData));
        }

        if (output.canvas.isNull() && output.hasLayers()) {
            QImage tmpImage(10, 10, QImage::Format_ARGB32_Premultiplied);
            output.setCanvas(tmpImage);
        }

        output.premultiply();
        output.scale();

        if (output.hasLayers()) {
            // Reset output.canvas since composite layers exist
            output.makeTransparent();
            // Initiate recursive compositing
            processChildLayers(game, output);
        }

        if (createSubfolder) {
            QFileInfo fi = QFileInfo(filename);
            if (!QDir().mkpath(fi.absolutePath())) {
                qWarning() << "Path could not be created" << fi.absolutePath()
                           << " Check file permissions, gamelist binary data "
                              "maybe incomplete.";
            }
        }

        if (output.resType == QLatin1String("cover") && output.save(filename)) {
            game.coverFile = filename;
        } else if (output.resType == QLatin1String("screenshot") && output.save(filename)) {
            game.screenshotFile = filename;
        } else if (output.resType == QLatin1String("wheel") && output.save(filename)) {
            game.wheelFile = filename;
        } else if (output.resType == QLatin1String("marquee") && output.save(filename)) {
            game.marqueeFile = filename;
        } else if (output.resType == QLatin1String("texture") && output.save(filename)) {
            game.textureFile = filename;
        }
    }
}

void Compositor::processChildLayers(GameEntry &game, Layer &layer) {
    for (int a = 0; a < layer.getLayers().length(); ++a) {
        // Create new layer and set canvas to relevant resource (or empty if
        // left out in xml)
        Layer thisLayer = layer.getLayers().at(a);
        if (thisLayer.type == T_LAYER) {
            if (thisLayer.resource == QLatin1String("")) {
                QImage emptyCanvas(1, 1, QImage::Format_ARGB32_Premultiplied);
                emptyCanvas.fill(Qt::transparent);
                thisLayer.setCanvas(emptyCanvas);
            } else if (thisLayer.resource == QLatin1String("cover")) {
                thisLayer.setCanvas(QImage::fromData(game.coverData));
            } else if (thisLayer.resource == QLatin1String("screenshot")) {
                thisLayer.setCanvas(QImage::fromData(game.screenshotData));
            } else if (thisLayer.resource == QLatin1String("wheel")) {
                thisLayer.setCanvas(QImage::fromData(game.wheelData));
            } else if (thisLayer.resource == QLatin1String("marquee")) {
                thisLayer.setCanvas(QImage::fromData(game.marqueeData));
            } else if (thisLayer.resource == QLatin1String("texture")) {
                thisLayer.setCanvas(QImage::fromData(game.textureData));
            } else {
                thisLayer.setCanvas(config->resources[thisLayer.resource]);
            }

            // If no meaningful canvas could be created, stop processing this
            // layer branch entirely
            if (thisLayer.canvas.isNull()) {
                continue;
            }

            thisLayer.premultiply();
            if (thisLayer.resource == QLatin1String("screenshot")) {
                // Crop away transparency and, if configured, black borders
                // around screenshots
                thisLayer.setCanvas(
                    ImgTools::cropToFit(thisLayer.canvas, config->cropBlack));
            } else {
                // Crop away transparency around all other types. Never crop
                // black on these as many have black outlines that are very much
                // needed
                thisLayer.setCanvas(ImgTools::cropToFit(thisLayer.canvas));
            }
            thisLayer.scale();

            // Update width + height as we will need them for easier placement
            // and alignment
            thisLayer.updateSize();

            // Continue concurrency if this layer has children
            if (thisLayer.hasLayers()) {
                processChildLayers(game, thisLayer);
            }

            // Composite image on canvas (which is the parent canvas at this
            // point)
            QPainter painter;
            painter.begin(&layer.canvas);
            painter.setCompositionMode(thisLayer.mode);
            if (thisLayer.opacity != -1)
                painter.setOpacity(thisLayer.opacity * 0.01);

            int x = 0;
            if (thisLayer.align == QLatin1String("center")) {
                x = (layer.width / 2) - (thisLayer.width / 2);
            } else if (thisLayer.align == QLatin1String("right")) {
                x = layer.width - thisLayer.width;
            }
            x += thisLayer.x;

            int y = 0;
            if (thisLayer.valign == QLatin1String("middle")) {
                y = (layer.height / 2) - (thisLayer.height / 2);
            } else if (thisLayer.valign == QLatin1String("bottom")) {
                y = layer.height - thisLayer.height;
            }
            y += thisLayer.y;

            painter.drawImage(x, y, thisLayer.canvas);
            painter.end();

        } else if (thisLayer.type == T_SHADOW) {
            FxShadow effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_BLUR) {
            FxBlur effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_MASK) {
            FxMask effect;
            layer.setCanvas(
                effect.applyEffect(layer.canvas, thisLayer, config));
        } else if (thisLayer.type == T_FRAME) {
            FxFrame effect;
            layer.setCanvas(
                effect.applyEffect(layer.canvas, thisLayer, config));
        } else if (thisLayer.type == T_STROKE) {
            FxStroke effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_ROUNDED) {
            FxRounded effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_BRIGHTNESS) {
            FxBrightness effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_CONTRAST) {
            FxContrast effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_BALANCE) {
            FxBalance effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_OPACITY) {
            FxOpacity effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_GAMEBOX) {
            FxGamebox effect;
            layer.setCanvas(
                effect.applyEffect(layer.canvas, thisLayer, game, config));
        } else if (thisLayer.type == T_HUE) {
            FxHue effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_SATURATION) {
            FxSaturation effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_COLORIZE) {
            FxColorize effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_ROTATE) {
            FxRotate effect;
            layer.setCanvas(effect.applyEffect(layer.canvas, thisLayer));
        } else if (thisLayer.type == T_SCANLINES) {
            FxScanlines effect;
            layer.setCanvas(
                effect.applyEffect(layer.canvas, thisLayer, config));
        }
        // Update width and height only for effects that change the dimensions
        // in a way that necessitates an update. For instance T_SHADOW does NOT
        // require an update since we don't want the alignment of the layer to
        // take the shadow into consideration.
        if (thisLayer.type == T_STROKE || thisLayer.type == T_ROTATE ||
            thisLayer.type == T_GAMEBOX) {
            layer.updateSize();
        }
    }
}

QString Compositor::getSubpath(const QString &absPath) {
    QString subPath = ".";
    // only esde expects media files in same subpath as game file
    if (config->frontend == QLatin1String("esde")) {
        QDir inputDir = QDir(config->inputFolder);
        QFileInfo entryInfo(absPath);
        QString entryDir = entryInfo.absolutePath();
        subPath = inputDir.relativeFilePath(entryDir);
    }
    return subPath;
}
