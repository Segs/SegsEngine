/*************************************************************************/
/*  godot_windows.cpp                                                    */
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

#include "main/main.h"
#include "os_windows.h"
#include "version.h"

#include <locale.h>
#include <stdio.h>
#include <QCoreApplication>

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

int widechar_main(int argc, wchar_t **argv) {
	OS_Windows os(nullptr);

	setlocale(LC_CTYPE, "");

	Error err = Main::setup();

	if (err != OK) {
		return 255;
	}

	if (Main::start())
		os.run();
	Main::cleanup();

	return os.get_exit_code();
};

int _main() {
	LPWSTR *wc_argv;
	int argc;

	wc_argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (nullptr == wc_argv) {
		wprintf(L"CommandLineToArgvW failed\n");
		return 0;
	}

	int result = widechar_main(argc, wc_argv);

	LocalFree(wc_argv);
	return result;
}
//To appease __try 
int wrapped_main(int argc,char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(VERSION_SHORT_NAME);
    QCoreApplication::setApplicationVersion(VERSION_BRANCH);
    QCoreApplication::setOrganizationName("Segs");
    // _argc and _argv are ignored in the engine
    // we are going to use the WideChar version of them instead
    return _main();

}
int main(int _argc, char **_argv) {


#ifdef CRASH_HANDLER_EXCEPTION
	__try {
        return wrapped_main(_argc, _argv);
	} __except (CrashHandlerException(GetExceptionInformation())) {
		return 1;
	}
#else
	return wrapped_main(_argc, _argv);
#endif
}

HINSTANCE godot_hinstance = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	godot_hinstance = hInstance;
	return main(0, nullptr);
}
