/*************************************************************************/
/*  godot_x11.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

// included first since os_x11.h pulls in xlib with it's stupid #define Bool
#include <QCoreApplication>
#include <qlogging.h>

#include "version.h"
#include "main/main.h"
#include "core/os/os.h"
#include "EASTL/unique_ptr.h"

#include <climits>
#include <clocale>
#include <cstdlib>
#include <QDir>

#ifdef Q_OS_WIN32
#include <windows.h>
static HINSTANCE godot_hinstance = nullptr;
#else
static void *godot_hinstance = nullptr;
#endif
#ifdef Q_OS_WIN32

// For export templates, add a section; the exporter will patch it to enclose
// the data appended to the executable (bundled PCK)
#ifndef TOOLS_ENABLED
#if defined _MSC_VER
#pragma section("pck", read)
__declspec(allocate("pck")) static char dummy[8] = { 0 };
#elif defined __GNUC__
static const char dummy[8] __attribute__((section("pck"), used)) = { 0 };
#endif
#endif
#endif
#define WRAP_QT_MESSAGES
#ifdef WRAP_QT_MESSAGES
/* NOTE: enable this to set breakpoints on qdebug messages. */
void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &lMessage)
{
    QString text;
    switch (type)
    {
    case QtDebugMsg:
        text = QString("Qt::Debug (%1:%2, %3): %4").arg(context.file).arg(context.line).arg(context.function).arg(lMessage.constData());
        break;

    case QtWarningMsg:
        text = QString("Qt::Warning (%1:%2, %3): %4").arg(context.file).arg(context.line).arg(context.function).arg(lMessage.constData());
        break;

    case QtCriticalMsg:
        text = QString("Qt::Critical (%1:%2, %3): %4").arg(context.file).arg(context.line).arg(context.function).arg(lMessage.constData());
        break;

    case QtFatalMsg:
        text = QString("Qt::Fatal (%1:%2, %3): %4").arg(context.file).arg(context.line).arg(context.function).arg(lMessage.constData());
        abort();
    }
    QByteArray az = text.toUtf8();
    printf("%s\n",az.data());
}

#endif
int mainT(int argc, char *argv[]) {

#ifdef WRAP_QT_MESSAGES
    qInstallMessageHandler(myMessageOutput);
#endif
    QCoreApplication app(argc,argv);
    QCoreApplication::setApplicationName(VERSION_SHORT_NAME);
    QCoreApplication::setApplicationVersion(VERSION_BRANCH);
    QCoreApplication::setOrganizationName("Segs");

    eastl::unique_ptr<OS> os(instantiateOS(godot_hinstance));
    setlocale(LC_CTYPE, "");

    QString cwd = QDir::currentPath();

    Error err = Main::setup();
    if (err != OK) {
        return 255;
    }

    if (Main::start())
        os->run(); // it is actually the OS that decides how to run
    Main::cleanup();

    if (!QDir::setCurrent(cwd)) {
        ERR_PRINT("Couldn't return to previous working directory.")
    }

    return os->get_exit_code();
}

int main(int argc, char *argv[]) {
#ifdef CRASH_HANDLER_EXCEPTION
    godot_hinstance = GetModuleHandle(nullptr);
    __try {
        return mainT<OS_Windows>(argc,argv);
    } __except (CrashHandlerException(GetExceptionInformation())) {
        return 1;
    }
#else
    return mainT(argc,argv);
#endif
}
