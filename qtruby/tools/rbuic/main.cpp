/****************************************************************************
**
** Copyright (C) 1992-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the tools applications of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include "uic.h"
#include "option.h"
#include "driver.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTextCodec>

static const char *error = 0;

void showHelp(const char *appName)
{
    fprintf(stderr, "Qt User Interface Compiler version %s\n", QT_VERSION_STR);
    if (error)
        fprintf(stderr, "%s: %s\n", appName, error);

    fprintf(stderr, "Usage: %s [options] <uifile>\n\n"
            "  -h, -help                 display this help and exit\n"
            "  -v, -version              display version\n"
            "  -d, -dependencies         display the dependencies\n"
            "  -o <file>                 place the output into <file>\n"
            "  -tr <func>                use func() for i18n\n"
            "  -p, -no-protection        disable header protection\n"
            "  -g <name>                 change generator\n"
            "  -x                        generate extra code to test the class\n"
            "\n", appName);
}

int main(int argc, char *argv[])
{
    Driver driver;

    const char *fileName = 0;

    int arg = 1;
    while (arg < argc) {
        QString opt = QString::fromLocal8Bit(argv[arg]);
        if (opt == QLatin1String("-h") || opt == QLatin1String("-help")) {
            showHelp(argv[0]);
            return 0;
        } else if (opt == QLatin1String("-d") || opt == QLatin1String("-dependencies")) {
            driver.option().dependencies = true;
        } else if (opt == QLatin1String("-v") || opt == QLatin1String("-version")) {
            fprintf(stderr, "Qt User Interface Compiler version %s\n", QT_VERSION_STR);
            return 0;
        } else if (opt == QLatin1String("-o") || opt == QLatin1String("-output")) {
            ++arg;
            if (!argv[arg]) {
                showHelp(argv[0]);
                return 1;
            }
            driver.option().outputFile = QFile::decodeName(argv[arg]);
#ifdef QT_UIC_RUBY_GENERATOR
        } else if (opt == QLatin1String("-x")) {
            driver.option().execCode = 1;
#endif
        } else if (opt == QLatin1String("-p") || opt == QLatin1String("-no-protection")) {
            driver.option().headerProtection = false;
        } else if (opt == QLatin1String("-postfix")) {
            ++arg;
            if (!argv[arg]) {
                showHelp(argv[0]);
                return 1;
            }
            driver.option().postfix = QLatin1String(argv[arg]);
        } else if (opt == QLatin1String("-3")) {
            ++arg;
            if (!argv[arg]) {
                showHelp(argv[0]);
                return 1;
            }
            driver.option().uic3 = QFile::decodeName(argv[arg]);
        } else if (opt == QLatin1String("-tr") || opt == QLatin1String("-translate")) {
            ++arg;
            if (!argv[arg]) {
                showHelp(argv[0]);
                return 1;
            }
            driver.option().translateFunction = QLatin1String(argv[arg]);
        } else if (opt == QLatin1String("-g") || opt == QLatin1String("-generator")) {
            ++arg;
            if (!argv[arg]) {
                showHelp(argv[0]);
                return 1;
            }
            QString name = QString::fromLocal8Bit(argv[arg]).toLower ();
            if (name == QLatin1String("java")) {
                driver.option().generator = Option::JavaGenerator;
            } else if (name == QLatin1String("ruby")) {
                driver.option().generator = Option::RubyGenerator;
            } else {
                driver.option().generator = Option::CppGenerator;
            }
        } else if (!fileName) {
            fileName = argv[arg];
        } else {
            showHelp(argv[0]);
            return 1;
        }

        ++arg;
    }

    // report Qt usage for commercial customers with a "metered license" (currently experimental)
#if QT_EDITION != QT_EDITION_OPENSOURCE
#ifdef QT_CONFIGURE_BINARIES_PATH
    const char *binariesPath = QT_CONFIGURE_BINARIES_PATH;
    QString reporterPath = QString::fromLocal8Bit(binariesPath) + QDir::separator()
                           + "qtusagereporter";
#if defined(Q_OS_WIN)
    reporterPath += ".exe";
#endif
    if (QFile::exists(reporterPath))
        system(qPrintable(reporterPath + " uic"));
#endif
#endif

    QString inputFile;
    if (fileName)
        inputFile = QString::fromLocal8Bit(fileName);
    else
        driver.option().headerProtection = false;

    if (driver.option().dependencies) {
        return !driver.printDependencies(inputFile);
    }

    QTextStream *out = 0;
    QFile f;
    if (driver.option().outputFile.size()) {
        f.setFileName(driver.option().outputFile);
        if (!f.open(QIODevice::WriteOnly | QFile::Text)) {
            fprintf(stderr, "Could not create output file\n");
            return 1;
        }
        out = new QTextStream(&f);
        out->setCodec(QTextCodec::codecForName("UTF-8"));
    }

    bool rtn = driver.uic(inputFile, out);
    if (!rtn)
        fprintf(stderr, "File '%s' is not valid\n", inputFile.isEmpty() ? "<stdin>" : inputFile.toLocal8Bit().constData());

    delete out;

    return !rtn;
}
