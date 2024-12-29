/***************************************************************************
 *            main.cpp
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

#include <QtGlobal>

// Includes for Linux and MacOS
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <signal.h>
#endif

// Includes for Windows
#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <QApplication>
#include <QDir>
#include <QtDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTimer>
#include <QTextCodec>
#include <QCommandLineParser>
#include <QCommandLineOption>

/* Does not work without a Qt5 static install:
#include <QtPlugin>
Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)*/

#include "strtools.h"
#include "skyscraper.h"
#include "platform.h"

Skyscraper *x = nullptr;
int sigIntRequests = 0;

void customMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
  QString txt;
  // Decide which type of debug message it is, and add string to signify it
  // Then append the debug message itself to the same string.
  switch (type) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
  case QtInfoMsg:
    txt += QString("INFO: '%1'").arg(msg);
    break;
#endif
  case QtDebugMsg:
    txt += QString("DEBUG: '%1'").arg(msg);
    break;
  case QtWarningMsg:
    if(msg.contains("NetManager") ||
        msg.contains("known incorrect sRGB profile") ||
        msg.contains("profile matches sRGB but writing iCCP instead") ||
        msg.contains("libpng warning")) {
      /* ugly, needs proper fix: */
      // NetManager "Cannot create children for a parent that is in a
      // different thread."
      /* and */
      // libpng warning: iCCP: known incorrect sRGB profile
      return;
    }
    txt += QString("WARN: %1").arg(msg);
    break;
  case QtCriticalMsg:
    txt += QString("CRITICAL: '%1'").arg(msg);
    break;
  case QtFatalMsg:
    txt += QString("FATAL: '%1'").arg(msg);
    abort();
  }
  printf("%s\n", txt.toStdString().c_str());
  fflush(stdout);
}

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
void sigHandler(int signal)
#endif
#if defined(Q_OS_WIN)
BOOL WINAPI ConsoleHandler(DWORD dwType)
#endif
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
  if(signal == 2) {
    sigIntRequests++;
#endif
#if defined(Q_OS_WIN)
  if(dwType == CTRL_C_EVENT) {
    sigIntRequests++;
#endif
    if(sigIntRequests <= 2) {
      if(x != nullptr) {
        if(x->state == 0) {
          // Nothing important going on, just exit
          delete x;
          exit(1);
        } else if(x->state == 1) {
          // Ignore signal, something important is going on that needs to finish!
        } else if(x->state == 2) {
          // Cache being edited, clear the queue to quit nicely
          x->queue->clearAll();
        } else if(x->state == 3) {
          // Threads are running, clear queue for a nice exit
          printf("\033[1;33mUser wants to quit, trying to exit nicely. This can take a few seconds depending on how many threads are running...\033[0m\n");
          x->queue->clearAll();
        }
      } else {
        exit(1);
      }
    } else {
      printf("\033[1;31mUser REALLY wants to quit NOW, forcing unclean exit...\033[0m\n");
      exit(2);
    }
  }
#if defined(Q_OS_WIN)
  return TRUE;
#endif
}

int main(int argc, char *argv[])
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
  struct sigaction sigIntHandler;

  sigIntHandler.sa_handler = sigHandler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;

  sigaction(SIGINT, &sigIntHandler, NULL);
#endif

#if defined(Q_OS_WIN)
  SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE);
#endif

  // Force non-graphic display:
  char *argv2[argc+2];
  char platform[10] = "-platform";
  char offscreen[10] = "offscreen";
  argv2[0] = argv[0];
  argv2[1] = platform;
  argv2[2] = offscreen;
  for(int i=1; i < argc; i++) {
    argv2[i+2] = argv[i];
  }
  argc = argc+2;

  QApplication app(argc, argv2);
  app.setApplicationVersion(VERSION);

  // Get current dir. If user has specified file(s) on command line we need this.
  QString currentDir = QDir::currentPath();

  // Set the working directory to the applications own path
  QDir skyDir(QDir::homePath() + "/.skyscraper");
  if(!skyDir.exists()) {
    if(!skyDir.mkpath(".")) {
      printf("Couldn't create folder '%s'. Please check permissions, now exiting...\n",
             skyDir.absolutePath().toStdString().c_str());
      return(3);
    }
  }

  // Create import paths
  skyDir.mkpath("import");
  skyDir.mkpath("import/textual");
  skyDir.mkpath("import/screenshots");
  skyDir.mkpath("import/covers");
  skyDir.mkpath("import/wheels");
  skyDir.mkpath("import/marquees");
  skyDir.mkpath("import/textures");
  skyDir.mkpath("import/videos");
  skyDir.mkpath("import/manuals");

  // Create resources folder
  skyDir.mkpath("resources");

  // Rename 'dbs' folder to migrate 2.x users to 3.x
  skyDir.rename(skyDir.absolutePath() + "/dbs", skyDir.absolutePath() + "/cache");

  // Create cache folder
  skyDir.mkpath("cache");
  QDir::setCurrent(skyDir.absolutePath());

  // Install the custom debug message handler used by qDebug()
  qInstallMessageHandler(customMessageHandler);

  Platform::get().loadConfig("platforms.json");

  QString platforms;
  const auto platformList = Platform::get().getPlatforms();
  for(const auto &platform: std::as_const(platformList)) {
    platforms.append("'" + platform + "', ");
  }
  // Remove the last ', '
  platforms = platforms.left(platforms.length() - 2);

  QCommandLineParser parser;

  QString headerString = "Running Skyscraper v" VERSION " by Lars Muldjord";
  QString dashesString = "";
  for(int a = 0; a < headerString.length(); ++a) {
    dashesString += "-";
  }

  parser.setApplicationDescription(StrTools::getVersionHeader() + "Skyscraper looks for compatible game files for the chosen platform (set with '-p'). It allows you to gather and cache media and game information for the files using various scraping modules (set with '-s'). It then lets you generate game lists for the supported frontends by combining all previously cached resources ('game list generation mode' is initiated by simply leaving out the '-s' option). While doing so it also composites game art for all files by following the recipe at '~/.skyscraper/artwork.xml'.\n\nIn addition to the command line options Skyscraper also provides a lot of customizable options for configuration, artwork, game name aliases, resource priorities and much more. Please check the full documentation at 'github.com/detain/skyscraper/tree/master/docs' for a detailed explanation of all features.\n\nRemember that most of the following options can also be set in the '~/.skyscraper/config.ini' file. All cli options and config.ini options are thoroughly documented at the above link.");
  parser.addHelpOption();
  parser.addVersionOption();
  QCommandLineOption fOption("f", "\nThe frontend you wish to generate a gamelist for. Remember to leave out the '-s' option when using this in order to enable Skyscraper's gamelist generation mode.\n(Currently supports 'emulationstation', 'retrobat', 'attractmode', 'pegasus', 'koillection' and 'xmlexport').\n(default is 'emulationstation')\n", "FRONTEND", "");
  QCommandLineOption pOption("p", "The platform you wish to scrape.\n(Currently supports " + platforms + ").\n.", "PLATFORM", "");
  QCommandLineOption sOption("s", "The scraping module you wish to gather resources from for the platform set with '-p'.\nLeave the '-s' option out to enable Skyscraper's gamelist generation mode.\n(WEB: 'arcadedb', 'igdb', 'mobygames', 'openretro', 'rawg', 'screenscraper', 'thegamesdb'  and 'worldofspectrum'; HYBRID: 'giantbomb', 'launchbox', 'offlinemobygames', 'offlinetgdb' and 'vgfacts'; OFFLINE: 'chiptune', 'docsdb', 'gamefaqs', 'mamehistory' and 'vgmaps'; LOCAL: 'customflags', 'esgamelist' and 'import').\n \nSUPPORT PARAMETERS:\n", "MODULE", "");
  QCommandLineOption aOption("a", "Specify a non-default artwork.xml file to use when setting up the artwork compositing when in gamelist generation mode.\n(default is '~/.skyscraper/artwork.xml')", "FILENAME", "");
  QCommandLineOption addextOption("addext", "Add this or these file extension(s) to accepted file extensions during a scraping run. (example: '*.zst' or '*.zst *.ext')", "EXTENSION(S)", "");
  QCommandLineOption cOption("c", "Use this config file to set up Skyscraper.\n(default is '~/.skyscraper/config.ini')", "FILENAME", "");
  QCommandLineOption cacheGbOption("cachegb", "Retrieve GiantBomb game header database for the selected platform.");
  QCommandLineOption dOption("d", "Set custom resource cache folder.\n(default is '~/.skyscraper/cache/PLATFORM')", "FOLDER", "");
  QCommandLineOption doctypesOption("doctypes", "Indicate the set of document sources that will be checked by the 'docsdb' scraper. (example: 'guidespdf cheatsttg')", "DOCTYPES", "");
  QCommandLineOption eOption("e", "Set extra frontend option. This is required by the 'attractmode' frontend to set the emulator and optionally for the 'pegasus' frontend to set the launch command.\n(default is none)", "STRING", "");
  QCommandLineOption endatOption("endat", "Tells Skyscraper which file to end at. Forces '--refresh' (or '--rescan').", "FILENAME", "");
  QCommandLineOption excludefilesOption("excludefiles", "(DEPRECATED, please use '--excludepattern' instead) Tells Skyscraper to always exclude the files matching the provided asterisk pattern(s). Remember to double-quote the pattern to avoid weird behaviour. You can add several patterns by separating them with ','. In cases where you need to match for a comma you need to escape it as '\\,'. (Pattern example: '\"*[BIOS]*,*proto*\"')", "PATTERN", "");
  QCommandLineOption excludefromOption("excludefrom", "Tells Skyscraper to exclude all files listed in FILENAME. One filename per line. This file can be generated with the '--cache report:missing' option or made manually.", "FILENAME", "");
  QCommandLineOption excludepatternOption("excludepattern", "Tells Skyscraper to always exclude the files matching the provided asterisk pattern(s). Remember to double-quote the pattern to avoid weird behaviour. You can add several patterns by separating them with ','. In cases where you need to match for a comma you need to escape it as '\\,'. (Pattern example: '\"*[BIOS]*,*proto*\"')", "PATTERN", "");
  QCommandLineOption fuzzySearchOption("fuzzysearch", "Allow some variations in the literal game name matching logic (0-20).\n(default is 2 variations)", "0-20", "2");
  QCommandLineOption gOption("g", "Game list export folder.\n(default depends on frontend)", "PATH", "");
  QCommandLineOption generateLbDbOption("generatelbdb", "Converts the XML Launchbox file into a SQLite database.");
  QCommandLineOption iOption("i", "Folder which contains the game/rom files.\n(default is '~/RetroPie/roms/PLATFORM')", "PATH", "");
  QCommandLineOption includefilesOption("includefiles", "(DEPRECATED, please use '--includepattern' instead) Tells Skyscraper to only include the files matching the provided asterisk pattern(s). Remember to double-quote the pattern to avoid weird behaviour. You can add several patterns by separating them with ','. In cases where you need to match for a comma you need to escape it as '\\,'. (Pattern example: '\"Super*,*Fighter*\"')", "PATTERN", "");
  QCommandLineOption includefromOption("includefrom", "Tells Skyscraper to only include the files listed in FILENAME. One filename per line. This file can be generated with the '--cache report:missing' option or made manually.", "FILENAME", "");
  QCommandLineOption includepatternOption("includepattern", "Tells Skyscraper to only include the files matching the provided asterisk pattern(s). Remember to double-quote the pattern to avoid weird behaviour. You can add several patterns by separating them with ','. In cases where you need to match for a comma you need to escape it as '\\,'. (Pattern example: '\"Super*,*Fighter*\"')", "PATTERN", "");
  QCommandLineOption lOption("l", "Maximum game description length. Everything longer than this will be truncated.\n(default is 2500)", "0-100000", "");
  QCommandLineOption langOption("lang", "Set preferred result language for scraping modules that support it.\n(default is 'en')", "CODE", "en");
  QCommandLineOption loadChecksum("loadchecksum", "Load the canonical data (game name, file name, size, CRC, SHA1, MD5) from an XML '.dat' (TOSEC, No-Intro, Redump) or '.xml' (MAME) file into the canonical database. The filename must be indicated. Use 'LUTRISDB' as filename to retrieve checksum data from the Lutris database.", "FILENAME", "");
  QCommandLineOption mOption("m", "Minimum match percentage when comparing search result titles to filename titles.\n(default is 65)", "0-100", "");
  QCommandLineOption maxfailsOption("maxfails", "Sets the allowed number of initial 'Not found' results before rage-quitting.\n(default is 42)", "1-200", "");
  QCommandLineOption oOption("o", "Game media export folder.\n(default depends on frontend)", "PATH", "");
  QCommandLineOption queryOption("query", "Allows you to set a custom search query (eg. 'rick+dangerous' for name based modules or 'sha1=CHECKSUM', 'md5=CHECKSUM' or 'romnom=FILENAME' for the 'screenscraper' module). Requires the single rom filename you wish to override for to be passed on command line as well, otherwise it will be ignored.", "QUERY", "");
  QCommandLineOption refreshOption("refresh", "Forces a refresh of existing cached resources for any scraping module. Same as '--cache refresh'. Incompatible with --rescan.");
  QCommandLineOption regionOption("region", "Add preferred game region for scraping modules that support it.\n(default prioritization is 'eu', 'us', 'wor' and 'jp' + others in that order)", "CODE", "eu");
  QCommandLineOption rescanOption("rescan", "Executes the scraping process for entries with existing resources, and validates that the scraper provides the same information as in the cache database. Deletes/refreshes it otherwise. Same as '--cache rescan'. Incompatible with (and overrides) --refresh.");
  QCommandLineOption startatOption("startat", "Tells Skyscraper which file to start at. Forces '--refresh' (or '--rescan').", "FILENAME", "");
  QCommandLineOption tOption("t", "Number of scraper threads to use. This might change depending on the scraping module limits.\n(default is 4)", "1-8", "");
  QCommandLineOption uOption("u", "userKey or UserID and Password for use with the selected scraping module.\n(default is none)", "KEY/USER:PASSWORD[:APIKEY]", "");
  QCommandLineOption verbosityOption("verbosity", "Print more info while scraping.\n(default is 0)\n \nSUBCOMMANDS:\n", "0-3", "0");
  QCommandLineOption cacheOption("cache", "This option is the master option for all options related to the resource cache. It must be followed by 'COMMAND[:OPTIONS]'.\nSee '--cache help' for a full description of all functions.", "COMMAND[:OPTIONS]", "");
  QCommandLineOption flagsOption("flags", "Allows setting flags that will impact the run in various ways. See '--flags help' for a list of all available flags and what they do.", "FLAG1,FLAG2,...", "");

  parser.addOption(fOption);
  parser.addOption(pOption);
  parser.addOption(sOption);
  parser.addOption(aOption);
  parser.addOption(addextOption);
  parser.addOption(cOption);
  parser.addOption(cacheGbOption);
  parser.addOption(dOption);
  parser.addOption(doctypesOption);
  parser.addOption(eOption);
  parser.addOption(endatOption);
  parser.addOption(excludefilesOption);
  parser.addOption(excludefromOption);
  parser.addOption(excludepatternOption);
  parser.addOption(fuzzySearchOption);
  parser.addOption(gOption);
  parser.addOption(generateLbDbOption);
  parser.addOption(iOption);
  parser.addOption(includefilesOption);
  parser.addOption(includefromOption);
  parser.addOption(includepatternOption);
  parser.addOption(lOption);
  parser.addOption(langOption);
  parser.addOption(loadChecksum);
  parser.addOption(mOption);
  parser.addOption(maxfailsOption);
  parser.addOption(oOption);
  parser.addOption(queryOption);
  parser.addOption(refreshOption);
  parser.addOption(rescanOption);
  parser.addOption(regionOption);
  parser.addOption(startatOption);
  parser.addOption(tOption);
  parser.addOption(uOption);
  parser.addOption(verbosityOption);
  parser.addOption(cacheOption);
  parser.addOption(flagsOption);

  parser.process(app);

  if(argc <= 1 || parser.isSet("help") || parser.isSet("h")) {
    parser.showHelp();
  } else {
    x = new Skyscraper(parser, currentDir);
    QObject::connect(x, &Skyscraper::finished, &app, &QApplication::quit);
    QTimer::singleShot(0, x, SLOT(run()));
  }
  return app.exec();
}
