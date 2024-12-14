/*
 *  This file is part of skyscraper.
 *  Copyright 2017 Lars Muldjord
 *  Copyright 2023 Gemba @ GitHub
 *
 *  skyscraper is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
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

#include "config.h"

#include "platform.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringBuilder>
#include <QStringList>
#include <QTextStream>

Config::SkyFolders skyFolders;

void Config::initSkyFolders() {
    const QString appFolder = "skyscraper";
#ifndef XDG
    // genuine Skyscraper folders
    skyFolders[SkyFolderType::CONFIG] = QDir::homePath() % "/." % appFolder;
    skyFolders[SkyFolderType::CACHE] =
        skyFolders[SkyFolderType::CONFIG] % "/cache";
    skyFolders[SkyFolderType::IMPORT] =
        skyFolders[SkyFolderType::CONFIG] % "/import";
    skyFolders[SkyFolderType::RESOURCE] =
        skyFolders[SkyFolderType::CONFIG] % "/resources";
    skyFolders[SkyFolderType::REPORT] =
        skyFolders[SkyFolderType::CONFIG] % "/reports";
    skyFolders[SkyFolderType::LOG] = skyFolders[SkyFolderType::CONFIG];
#else
    // XDG Spec
    QMap<QString, QString> xdgEnvs = {
        {"XDG_CONFIG_HOME", QDir::homePath() % "/.config"},
        {"XDG_CACHE_HOME", QDir::homePath() % "/.cache"},
        {"XDG_DATA_HOME", QDir::homePath() % "/.local/share"},
        {"XDG_STATE_HOME", QDir::homePath() % "/.local/state"}};

    for (const auto &e : xdgEnvs.keys()) {
        QString xdgDir =
            QProcessEnvironment::systemEnvironment().value(e, xdgEnvs.value(e));
        if (QFileInfo(xdgDir).isAbsolute()) {
            xdgEnvs[e] = xdgDir;
            QDir d(xdgDir % "/" % appFolder);
            if (!d.exists() && !d.mkpath(".")) {
                printf("Couldn't create folder '%s'. Please check permissions, "
                       "now exiting...\n",
                       d.absolutePath().toStdString().c_str());
                exit(1);
            }
        }
    }

    skyFolders[SkyFolderType::CONFIG] =
        xdgEnvs["XDG_CONFIG_HOME"] % "/" % appFolder;
    skyFolders[SkyFolderType::CACHE] =
        xdgEnvs["XDG_CACHE_HOME"] % "/" % appFolder;
    skyFolders[SkyFolderType::IMPORT] =
        xdgEnvs["XDG_DATA_HOME"] % "/" % appFolder % "/import";
    skyFolders[SkyFolderType::RESOURCE] =
        xdgEnvs["XDG_DATA_HOME"] % "/" % appFolder % "/resources";
    skyFolders[SkyFolderType::REPORT] =
        xdgEnvs["XDG_STATE_HOME"] % "/" % appFolder % "/reports";
    skyFolders[SkyFolderType::LOG] =
        xdgEnvs["XDG_STATE_HOME"] % "/" % appFolder;

    QDir(skyFolders[SkyFolderType::IMPORT]).mkpath(".");
    QDir(skyFolders[SkyFolderType::RESOURCE]).mkpath(".");
    QDir(skyFolders[SkyFolderType::REPORT]).mkpath(".");
#endif // XDG

#ifndef QT_NO_DEBUG_OUTPUT
    qDebug("Skyscraper folder config:");
    for (auto it : skyFolders.toStdMap()) {
        qDebug() << static_cast<int>(it.first) << it.second;
    }
#endif
}

QString Config::getSkyFolder(SkyFolderType type) { return skyFolders[type]; }

void Config::copyFile(const QString &src, const QString &dest, FileOp fileOp) {
    if (QFileInfo::exists(src)) {
        if (QFileInfo::exists(dest)) {
            if (fileOp == FileOp::OVERWRITE) {
                QFile::remove(dest);
                QFile::copy(src, dest);
                qDebug() << "Overwritten file" << dest;
            } else if (fileOp == FileOp::CREATE_DIST) {
                QString d = QString(dest + ".dist");
                QFile::remove(d);
                QFile::copy(src, d);
                qDebug() << "Copied original distribution file" << src << "as"
                         << d;
            }
        } else {
            QFile::copy(src, dest);
            qDebug() << "Created file" << dest;
        }
    } else {
        printf("\033[1;31mSource config file not found '%s'. Please check "
               "setup, bailing out...\033[0m\n",
               src.toStdString().c_str());
        exit(1);
    }
}

void Config::setupUserConfig() {
    // Set the working directory to the applications own path
    QDir skyDir(getSkyFolder());
    if (!skyDir.exists()) {
        if (!skyDir.mkpath(".")) {
            printf("Couldn't create folder '%s'. Please check permissions, "
                   "now exiting...\n",
                   skyDir.absolutePath().toStdString().c_str());
            exit(1);
        }
    }

    // Create import paths
    QStringList paths = {"covers",  "manuals",  "marquees", "screenshots",
                         "textual", "textures", "videos",   "wheels"};
    for (auto p : paths) {
        QDir(getSkyFolder(SkyFolderType::IMPORT) % "/" % p).mkpath(".");
    }

    // Create resources folder
    QDir(getSkyFolder(SkyFolderType::RESOURCE)).mkpath(".");

    // Create cache folder
    QDir(getSkyFolder(SkyFolderType::CACHE)).mkpath(".");

    // defaults to the folder containing config.ini, artwork.xml, hints.xml, ...
    // any file outside this folder or subfolders to this folder shall use
    // Config::getSkyFolder(type ...)
    QDir::setCurrent(getSkyFolder());

    // copy configs
    QString localEtcPath = QString(
        (PREFIX != "/usr") ? PREFIX "/etc/skyscraper/" : "/etc/skyscraper/");

    if (!QFileInfo::exists(localEtcPath)) {
        // RetroPie or Windows installation type: handled externally
        return;
    }

    QMap<QString, QPair<QString, FileOp>> configFiles = {
        {"ARTWORK.md", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"artwork.xml.example1", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"artwork.xml.example2", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"artwork.xml.example3", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"artwork.xml.example4", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"cache/priorities.xml.example",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"config.ini.example",
         QPair<QString, FileOp>("config.ini.example", FileOp::OVERWRITE)},
        {"CACHE.md",
         QPair<QString, FileOp>("cache/README.md", FileOp::OVERWRITE)},
        {"hints.xml", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"import/definitions.dat.example1",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"import/definitions.dat.example2",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"import/IMPORT.md",
         QPair<QString, FileOp>("import/README.md", FileOp::OVERWRITE)},
        {"mameMap.csv", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"mobygames_platforms.json",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"README.md", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"resources/boxfront.png",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"resources/boxside.png",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"screenscraper_platforms.json",
         QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"tgdb_developers.json", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"tgdb_genres.json", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"tgdb_platforms.json", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        {"tgdb_publishers.json", QPair<QString, FileOp>("", FileOp::OVERWRITE)},
        // do not overwrite
        {"config.ini.example",
         QPair<QString, FileOp>("config.ini", FileOp::KEEP)},
        {"import/definitions.dat.example2",
         QPair<QString, FileOp>("import/definitions.dat", FileOp::KEEP)},
        {"resources/frameexample.png",
         QPair<QString, FileOp>("", FileOp::KEEP)},
        {"resources/maskexample.png", QPair<QString, FileOp>("", FileOp::KEEP)},
        {"resources/scanlines1.png", QPair<QString, FileOp>("", FileOp::KEEP)},
        {"resources/scanlines2.png", QPair<QString, FileOp>("", FileOp::KEEP)},
        // create <fn>.dist if exists
        {"aliasMap.csv", QPair<QString, FileOp>("", FileOp::CREATE_DIST)},
        {"artwork.xml", QPair<QString, FileOp>("", FileOp::CREATE_DIST)},
        {"peas.json", QPair<QString, FileOp>("", FileOp::CREATE_DIST)},
        {"platforms_idmap.csv",
         QPair<QString, FileOp>("", FileOp::CREATE_DIST)}};

    for (auto src : configFiles.keys()) {
        QString dest = configFiles.value(src).first;
        if (dest.isEmpty()) {
            dest = src;
        }
        QString tgtDir = getSkyFolder();
        if (src.startsWith("cache/") || src == "CACHE.md") {
            tgtDir = getSkyFolder(SkyFolderType::CACHE);
            dest = dest.replace("cache/", "");
        } else if (src.startsWith("import/")) {
            tgtDir = getSkyFolder(SkyFolderType::IMPORT);
            dest = dest.replace("import/", "");
        } else if (src.startsWith("resources/")) {
            tgtDir = getSkyFolder(SkyFolderType::RESOURCE);
            dest = dest.replace("resources/", "");
        }
        QString tgt = tgtDir % "/" % dest;
        copyFile(localEtcPath % src, tgt, configFiles.value(src).second);
    }
}

void Config::checkLegacyFiles() {
    QStringList legacyJsons = {"mobygames", "platforms", "screenscraper"};
    for (auto bn : legacyJsons) {
        QString fn = getSkyFolder() % "/" % bn % ".json";
        if (QFileInfo::exists(fn)) {
            printf(
                "\033[1;33mFile '%s' found, which is no longer used in this "
                "version of Skyscraper. Please move file to mute this warning. "
                "See docs/PLATFORMS.md for additional info.\033[0m\n",
                fn.toUtf8().constData());
        }
    }
}

QString Config::getSupportedPlatforms() {
    if (!Platform::get().loadConfig()) {
        exit(1);
    }

    QString platforms;
    for (const auto &platform : Platform::get().getPlatforms()) {
        platforms.append("'" + platform + "', ");
    }
    platforms.chop(2);
    return platforms;
}

QString Config::getRetropieVersion() {
    // return RetroPie version if any
    QString ver = "";
    QFile rpVer("/opt/retropie/VERSION");
    if (rpVer.open(QIODevice::ReadOnly)) {
        QTextStream in(&rpVer);
        while (!in.atEnd()) {
            ver = in.readLine();
            break;
        }
        rpVer.close();
    }
    return ver;
}