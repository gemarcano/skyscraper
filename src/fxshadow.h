/***************************************************************************
 *            fxshadow.h
 *
 *  Sat Feb 3 12:00:00 CEST 2018
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

#ifndef FXSHADOW_H
#define FXSHADOW_H

#include "gameentry.h"
#include "layer.h"
#include "settings.h"

#include <QImage>
#include <QXmlStreamReader>
#include <QObject>

class FxShadow : public QObject {
    Q_OBJECT

public:
    FxShadow();
    QImage applyEffect(const QImage &src, const Layer &layer);

private:
    QVector<double> getGaussBoxes(double sigma, double n);
    void boxBlur(QRgb *buffer1, QRgb *buffer2, int width, int height,
                 int radius);
    void boxBlurHorizontal(QRgb *buffer1, QRgb *buffer2, int width, int height,
                           int radius);
    void boxBlurTotal(QRgb *buffer1, QRgb *buffer2, int width, int height,
                      int radius);
};

#endif // FXSHADOW_H
