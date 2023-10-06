/***************************************************************************
 *            thegamesdb.cpp
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

#include <QJsonArray>

#include "thegamesdb.h"
#include "strtools.h"

TheGamesDb::TheGamesDb(Settings *config,
		       QSharedPointer<NetManager> manager)
  : AbstractScraper(config, manager)
{
  loadMaps();

  baseUrl = "https://api.thegamesdb.net/v1";

  searchUrlPre = "https://api.thegamesdb.net/v1/Games/ByGameName?apikey=";

  fetchOrder.append(RELEASEDATE);
  fetchOrder.append(DESCRIPTION);
  fetchOrder.append(TAGS);
  fetchOrder.append(PLAYERS);
  fetchOrder.append(AGES);
  fetchOrder.append(PUBLISHER);
  fetchOrder.append(DEVELOPER);
  //fetchOrder.append(RATING);
  fetchOrder.append(COVER);
  fetchOrder.append(SCREENSHOT);
  fetchOrder.append(WHEEL);
  fetchOrder.append(MARQUEE);
}

void TheGamesDb::getSearchResults(QList<GameEntry> &gameEntries,
				  QString searchName, QString platform)
{
  netComm->request(searchUrlPre + StrTools::unMagic("187;161;217;126;172;149;202;122;163;197;163;219;162;171;203;197;139;151;215;173;122;206;161;162;200;216;217;123;124;215;200;170;171;132;158;155;215;120;149;169;140;164;122;154;178;174;160;172;157;131;210;161;203;137;159;117;205;166;162;139;171;169;210;163") + "&name="+ searchName);
  q.exec();
  data = netComm->getData();

  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    return;
  }

  reqRemaining = jsonDoc.object()["remaining_monthly_allowance"].toInt();
  if(reqRemaining <= 0)
    printf("\033[1;31mYou've reached TheGamesdDb's request limit for this month.\033[0m\n");

  if(jsonDoc.object()["status"].toString() != "Success") {
    return;
  }
  if(jsonDoc.object()["data"].toObject()["count"].toInt() < 1) {
    return;
  }

  QJsonArray jsonGames = jsonDoc.object()["data"].toObject()["games"].toArray();

  while(!jsonGames.isEmpty()) {
    QJsonObject jsonGame = jsonGames.first().toObject();
    
    GameEntry game;
    // https://api.thegamesdb.net/v1/Games/ByGameID?id=88&apikey=XXX&fields=game_title,players,release_date,developer,publisher,genres,overview,rating,platform
    game.id = QString::number(jsonGame["id"].toInt());
    game.url = "https://api.thegamesdb.net/v1/Games/ByGameID?id=" + game.id + "&apikey=" + StrTools::unMagic("187;161;217;126;172;149;202;122;163;197;163;219;162;171;203;197;139;151;215;173;122;206;161;162;200;216;217;123;124;215;200;170;171;132;158;155;215;120;149;169;140;164;122;154;178;174;160;172;157;131;210;161;203;137;159;117;205;166;162;139;171;169;210;163") + "&fields=game_title,players,release_date,developers,publishers,genres,overview,rating";
    game.title = jsonGame["game_title"].toString();
    // Remove anything at the end with a parentheses. 'thegamesdb' has a habit of adding
    // for instance '(1993)' to the name.
    game.title = game.title.left(game.title.indexOf("(")).simplified();
    game.platform = platformMap[jsonGame["platform"].toString()].toString();
    if(platformMatch(game.platform, platform) /*' || FIXME default p_id per romfolder (from rertropie_map.csv)  */) { // put clause last to let user override with his aliases
      gameEntries.append(game);
    }
    jsonGames.removeFirst();
  }
}

void TheGamesDb::getGameData(GameEntry &game)
{
  netComm->request(game.url);
  q.exec();
  data = netComm->getData();
  jsonDoc = QJsonDocument::fromJson(data);
  if(jsonDoc.isEmpty()) {
    printf("No returned json data, is 'thegamesdb' down?\n");
    reqRemaining = 0;
  }

  reqRemaining = jsonDoc.object()["remaining_monthly_allowance"].toInt();

  if(jsonDoc.object()["data"].toObject()["count"].toInt() < 1) {
    printf("No returned json game document, is 'thegamesdb' down?\n");
    reqRemaining = 0;
  }

  jsonObj = jsonDoc.object()["data"].toObject()["games"].toArray().first().toObject();

  for(int a = 0; a < fetchOrder.length(); ++a) {
    switch(fetchOrder.at(a)) {
    case DESCRIPTION:
      getDescription(game);
      break;
    case DEVELOPER:
      getDeveloper(game);
      break;
    case PUBLISHER:
      getPublisher(game);
      break;
    case PLAYERS:
      getPlayers(game);
      break;
    case AGES:
      getAges(game);
      break;
    case RATING:
      getRating(game);
      break;
    case TAGS:
      getTags(game);
      break;
    case RELEASEDATE:
      getReleaseDate(game);
      break;
    case COVER:
      if(config->cacheCovers) {
	getCover(game);
      }
      break;
    case SCREENSHOT:
      if(config->cacheScreenshots) {
	getScreenshot(game);
      }
      break;
    case WHEEL:
      if(config->cacheWheels) {
	getWheel(game);
      }
      break;
    case MARQUEE:
      if(config->cacheMarquees) {
	getMarquee(game);
      }
      break;
    case VIDEO:
      if(config->videos) {
	getVideo(game);
      }
      break;
    default:
      ;
    }
  }
}

void TheGamesDb::getReleaseDate(GameEntry &game)
{
  if(jsonObj["release_date"] != QJsonValue::Undefined)
    game.releaseDate = jsonObj["release_date"].toString();
}

void TheGamesDb::getDeveloper(GameEntry &game)
{
  QJsonArray developers = jsonObj["developers"].toArray();
  if(developers.count() != 0)
    game.developer = developerMap[developers.first().toString()].toString();
}

void TheGamesDb::getPublisher(GameEntry &game)
{
  QJsonArray publishers = jsonObj["publishers"].toArray();
  if(publishers.count() != 0)
    game.publisher = publisherMap[publishers.first().toString()].toString();
}

void TheGamesDb::getDescription(GameEntry &game)
{
  game.description = jsonObj["overview"].toString();
}

void TheGamesDb::getPlayers(GameEntry &game)
{
  int players = jsonObj["players"].toInt();
  if(players != 0)
    game.players = QString::number(players);
}

void TheGamesDb::getAges(GameEntry &game)
{
  if(jsonObj["rating"] != QJsonValue::Undefined)
    game.ages = jsonObj["rating"].toString();
}

void TheGamesDb::getTags(GameEntry &game)
{
  QJsonArray genres = jsonObj["genres"].toArray();
  if(genres.count() != 0) {
    while(!genres.isEmpty()) {
      game.tags.append(genreMap[genres.first().toString()].toString() + ", ");
      genres.removeFirst();
    }
    game.tags = game.tags.left(game.tags.length() - 2);
  }
}

void TheGamesDb::getCover(GameEntry &game)
{
  netComm->request("https://cdn.thegamesdb.net/images/original/boxart/front/" + game.id + "-1.jpg");
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.coverData = netComm->getData();
  }
}

void TheGamesDb::getScreenshot(GameEntry &game)
{
  netComm->request("https://cdn.thegamesdb.net/images/original/screenshots/" + game.id + "-1.jpg");
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.screenshotData = netComm->getData();
  }
}

void TheGamesDb::getWheel(GameEntry &game)
{
  netComm->request("https://cdn.thegamesdb.net/images/original/clearlogo/" + game.id + ".png");
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.wheelData = netComm->getData();
  }
}

void TheGamesDb::getMarquee(GameEntry &game)
{
  netComm->request("https://cdn.thegamesdb.net/images/original/graphical/" + game.id + "-g.jpg");
  q.exec();
  QImage image;
  if(netComm->getError() == QNetworkReply::NoError &&
     image.loadFromData(netComm->getData())) {
    game.marqueeData = netComm->getData();
  }
}

void TheGamesDb::loadMaps()
{
  genreMap = readJson("tgdb_genres.json");
  developerMap = readJson("tgdb_developers.json");
  publisherMap = readJson("tgdb_publishers.json");
  platformMap = readJson("tgdb_platforms.json");
}

QVariantMap TheGamesDb::readJson(QString filename) 
{
  QVariantMap m;
  QFile jsonFile(filename);
  if(jsonFile.open(QIODevice::ReadOnly)) {
    QJsonObject jsonDevs = QJsonDocument::fromJson(jsonFile.readAll()).object();
    m = jsonDevs.toVariantMap();
    jsonFile.close();
  }
  return m;
}