/*************************************************************************/
/*  os_unix.cpp                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "os_unix.h"

#ifdef UNIX_ENABLED

#include "core/debugger/script_debugger.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/string_utils.inl"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/net_socket_posix.h"

#include "drivers/unix/thread_posix.h"
#include "servers/rendering_server.h"
#include "core/string_formatter.h"

#include <mutex>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include <cassert>
#include <dlfcn.h>
#include <cerrno>
#include <poll.h>
#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <sys/time.h>
#include <sys/wait.h>
#include <QCoreApplication>
#include <unistd.h>

/// Clock Setup function (used by get_ticks_usec)
static uint64_t _clock_start = 0;
#if defined(__APPLE__)
static double _clock_scale = 0;
static void _setup_clock() {
    mach_timebase_info_data_t info;
    kern_return_t ret = mach_timebase_info(&info);
    ERR_FAIL_COND_MSG(ret != 0, "OS CLOCK IS NOT WORKING!");
    _clock_scale = ((double)info.numer / (double)info.denom) / 1000.0;
    _clock_start = mach_absolute_time() * _clock_scale;
}
#else
#if defined(CLOCK_MONOTONIC_RAW) && !defined(JAVASCRIPT_ENABLED) // This is a better clock on Linux.
#define GODOT_CLOCK CLOCK_MONOTONIC_RAW
#else
#define GODOT_CLOCK CLOCK_MONOTONIC
#endif
static void _setup_clock() {
    struct timespec tv_now = { 0, 0 };
    ERR_FAIL_COND_MSG(clock_gettime(GODOT_CLOCK, &tv_now) != 0, "OS CLOCK IS NOT WORKING!");
    _clock_start = ((uint64_t)tv_now.tv_nsec / 1000L) + (uint64_t)tv_now.tv_sec * 1000000L;
}
#endif

void OS_Unix::debug_break() {

    assert(false);
}

static void handle_interrupt(int sig) {
    if (ScriptDebugger::get_singleton() == nullptr)
        return;

    ScriptDebugger::get_singleton()->set_depth(-1);
    ScriptDebugger::get_singleton()->set_lines_left(1);
}

void OS_Unix::initialize_debugging() {

    if (ScriptDebugger::get_singleton() != nullptr) {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = handle_interrupt;
        sigaction(SIGINT, &action, nullptr);
    }
}

int OS_Unix::unix_initialize_audio(int p_audio_driver) {

    return 0;
}

void OS_Unix::initialize_core() {

    init_thread_posix();

    FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
    FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
    FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
    DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
    DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
    DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

#ifndef NO_NETWORK
    NetSocketPosix::make_default();
    IP_Unix::make_default();
#endif

    _setup_clock();
}

void OS_Unix::finalize_core() {

    NetSocketPosix::cleanup();
}

void OS_Unix::alert(StringView p_alert, StringView p_title) {
    fprintf(stderr, "ALERT: %.*s: %.*s\n", int(p_title.length()),p_title.data(),int(p_alert.length()),p_alert.data());
}

String OS_Unix::get_stdin_string(bool p_block) {

    if (p_block) {
        char buff[1024];
        String ret = stdin_buf + fgets(buff, 1024, stdin);
        stdin_buf = "";
        return ret;
    }

    return null_string;
}

String OS_Unix::get_name() const {

    return "Unix";
}

uint64_t OS_Unix::get_unix_time() const {

    return time(nullptr);
}

uint64_t OS_Unix::get_system_time_secs() const {
    struct timeval tv_now;
    gettimeofday(&tv_now, nullptr);
    return uint64_t(tv_now.tv_sec);
}

uint64_t OS_Unix::get_system_time_msecs() const {
    struct timeval tv_now;
    gettimeofday(&tv_now, nullptr);
    return uint64_t(tv_now.tv_sec) * 1000 + uint64_t(tv_now.tv_usec) / 1000;
}

OS::Date OS_Unix::get_date(bool utc) const {
    time_t t = time(nullptr);
    struct tm lt;
    if (utc) {
        gmtime_r(&t, &lt);
    } else {
        localtime_r(&t, &lt);
    }
    Date ret;
    ret.year = 1900 + lt.tm_year;
    // Index starting at 1 to match OS_Unix::get_date
    //   and Windows SYSTEMTIME and tm_mon follows the typical structure
    //   of 0-11, noted here: http://www.cplusplus.com/reference/ctime/tm/
    ret.month = (Month)(lt.tm_mon + 1);
    ret.day = lt.tm_mday;
    ret.weekday = (Weekday)lt.tm_wday;
    ret.dst = lt.tm_isdst;

    return ret;
}

OS::Time OS_Unix::get_time(bool utc) const {
    time_t t = time(nullptr);
    struct tm lt;
    if (utc) {
        gmtime_r(&t, &lt);
    } else {
        localtime_r(&t, &lt);
    }
    Time ret;
    ret.hour = lt.tm_hour;
    ret.min = lt.tm_min;
    ret.sec = lt.tm_sec;
    get_time_zone_info();
    return ret;
}

OS::TimeZoneInfo OS_Unix::get_time_zone_info() const {
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    char name[16];
    strftime(name, 16, "%Z", &lt);
    name[15] = 0;
    TimeZoneInfo ret;
    ret.name = name;

    char bias_buf[16];
    strftime(bias_buf, 16, "%z", &lt);
    int bias;
    bias_buf[15] = 0;
    sscanf(bias_buf, "%d", &bias);

    // convert from ISO 8601 (1 minute=1, 1 hour=100) to minutes
    int hour = (int)bias / 100;
    int minutes = bias % 100;
    if (bias < 0)
        ret.bias = hour * 60 - minutes;
    else
        ret.bias = hour * 60 + minutes;

    return ret;
}

void OS_Unix::delay_usec(uint32_t p_usec) const {

    struct timespec rem = { static_cast<time_t>(p_usec / 1000000), (static_cast<long>(p_usec) % 1000000) * 1000 };
    while (nanosleep(&rem, &rem) == EINTR) {
    }
}
uint64_t OS_Unix::get_ticks_usec() const {

#if defined(__APPLE__)
    uint64_t longtime = mach_absolute_time() * _clock_scale;
#else
    // Unchecked return. Static analyzers might complain.
    // If _setup_clock() succeeded, we assume clock_gettime() works.
    struct timespec tv_now = { 0, 0 };
    clock_gettime(GODOT_CLOCK, &tv_now);
    uint64_t longtime = ((uint64_t)tv_now.tv_nsec / 1000L) + (uint64_t)tv_now.tv_sec * 1000000L;
#endif
    longtime -= _clock_start;

    return longtime;
}

Error OS_Unix::execute(StringView p_path, const Vector<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {

    if (p_blocking && r_pipe) {

        String argss;
        argss = "\"" + String(p_path) + "\"";

        for (const String &arg : p_arguments) {
            argss += " \"" + arg + "\"";
        }

        if (read_stderr) {
            argss += " 2>&1"; // Read stderr too
        } else {
            argss += " 2>/dev/null"; //silence stderr
        }
        FILE *f = popen(argss.c_str(), "r");

        ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, "Cannot pipe stream from process running with following arguments '" + argss + "'.");

        char buf[65535];

        while (fgets(buf, 65535, f)) {

            if (p_pipe_mutex) {
                p_pipe_mutex->lock();
            }
            (*r_pipe) += String(buf);
            if (p_pipe_mutex) {
                p_pipe_mutex->unlock();
            }
        }
        int rv = pclose(f);
        if (r_exitcode)
            *r_exitcode = WEXITSTATUS(rv);

        return OK;
    }

    pid_t pid = fork();
    ERR_FAIL_COND_V(pid < 0, ERR_CANT_FORK);

    if (pid == 0) {
        // is child

        if (!p_blocking) {
            // For non blocking calls, create a new session-ID so parent won't wait for it.
            // This ensures the process won't go zombie at end.
            setsid();
        }

        Vector<String> cs;

        cs.emplace_back(p_path);
        for (const String &arg : p_arguments)
            cs.emplace_back(arg);

        Vector<char *> args;
        args.reserve(cs.size());

        for (const String & i : cs)
            args.emplace_back((char *)i.data());
        args.push_back(nullptr);

        execvp(cs.front().c_str(), &args[0]);
        // still alive? something failed..
        fprintf(stderr, "**ERROR** OS_Unix::execute - Could not create child process while executing: %s\n", String(p_path).c_str());
        raise(SIGKILL);
    }

    if (p_blocking) {

        int status;
        waitpid(pid, &status, 0);
        if (r_exitcode)
            *r_exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : status;

    } else {

        if (r_child_id)
            *r_child_id = pid;
    }

    return OK;
}

Error OS_Unix::kill(const ProcessID &p_pid) {

    int ret = ::kill(p_pid, SIGKILL);
    if (!ret) {
        //avoid zombie process
        int st;
        ::waitpid(p_pid, &st, 0);
    }
    return ret ? ERR_INVALID_PARAMETER : OK;
}

int OS_Unix::get_process_id() const {

    return getpid();
}

bool OS_Unix::has_environment(StringView p_var) const {
    if(p_var[p_var.size()]==0)
        return getenv(p_var.data()) != nullptr;
    else {
        String zterm(p_var);
        return getenv(zterm.data()) != nullptr;
    }
}

const char *OS_Unix::get_locale() const {

    if (!has_environment("LANG"))
        return "en";

    static String locale;
    locale = get_environment("LANG");
    int tp = StringUtils::find(locale,".");
    if (tp != -1)
        locale = StringUtils::substr(locale,0, tp);
    return locale.c_str();
}

Error OS_Unix::open_dynamic_library(StringView p_path, void *&p_library_handle, bool p_also_set_library_path) {

    String path(p_path);

    if (FileAccess::exists(path) && PathUtils::is_rel_path(path)) {
        // dlopen expects a slash, in this case a leading ./ for it to be interpreted as a relative path,
        //  otherwise it will end up searching various system directories for the lib instead and finally failing.
        path = "./" + path;
    }

    if (!FileAccess::exists(path)) {
        //this code exists so gdnative can load .so files from within the executable path
        path = PathUtils::plus_file(PathUtils::get_base_dir(get_executable_path()),PathUtils::get_file(p_path));
    }

    if (!FileAccess::exists(path)) {
        //this code exists so gdnative can load .so files from a standard unix location
        path = PathUtils::plus_file(PathUtils::plus_file(PathUtils::get_base_dir(get_executable_path()),"../lib"),PathUtils::get_file(p_path));
    }

    p_library_handle = dlopen(String(path).c_str(), RTLD_NOW);
    ERR_FAIL_COND_V_MSG(!p_library_handle, ERR_CANT_OPEN, "Can't open dynamic library: " + String(p_path) + ". Error: " + dlerror());
    return OK;
}

Error OS_Unix::close_dynamic_library(void *p_library_handle) {
    if (dlclose(p_library_handle)) {
        return FAILED;
    }
    return OK;
}

Error OS_Unix::get_dynamic_library_symbol_handle(void *p_library_handle, StringView p_name, void *&p_symbol_handle, bool p_optional) {
    const char *error;
    dlerror(); // Clear existing errors

    p_symbol_handle = dlsym(p_library_handle, String(p_name).c_str());

    error = dlerror();
    if (error != nullptr) {
        ERR_FAIL_COND_V_MSG(!p_optional, ERR_CANT_RESOLVE, String("Can't resolve symbol ") + p_name + ". Error: " + error + ".");

        return ERR_CANT_RESOLVE;
    }
    return OK;
}

Error OS_Unix::set_cwd(StringView p_cwd) {

    if (chdir(String(p_cwd).c_str()) != 0)
        return ERR_CANT_OPEN;

    return OK;
}

String OS_Unix::get_environment(StringView p_var) const {
    String zterm(p_var);
    const char *res=getenv(zterm.data());
    if (res)
        return res;
    return String();
}

bool OS_Unix::set_environment(StringView p_var, StringView p_value) const {
    String zterm(p_var);
    String zval(p_value);
    return setenv(zterm.data(), zval.data(), /* overwrite: */ true) == 0;
}

int OS_Unix::get_processor_count() const {

    return sysconf(_SC_NPROCESSORS_CONF);
}

String OS_Unix::get_user_data_dir() const {

    String appname(get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/name").as<String>()));
    if (!appname.empty()) {
        bool use_custom_dir = ProjectSettings::get_singleton()->getT<bool>("application/config/use_custom_user_dir");
        if (use_custom_dir) {
            String custom_dir = get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/custom_user_dir_name").as<String>(), true);
            if (custom_dir.empty()) {
                custom_dir = appname;
            }
            return PathUtils::plus_file(get_data_path(),custom_dir);
        } else {
            return PathUtils::join_path({get_data_path(),get_godot_dir_name(),"app_userdata",appname});
        }
    }

    return PathUtils::join_path({ get_data_path(),get_godot_dir_name(),"app_userdata","[unnamed project]"});
}

String OS_Unix::get_executable_path() const {
    return qPrintable(QCoreApplication::applicationFilePath());
}

void UnixTerminalLogger::log_error(StringView p_function, StringView p_file, int p_line, StringView p_code, StringView p_rationale, ErrorType p_type) {
    if (!should_log(true)) {
        return;
    }

    StringView err_details;
    if (not p_rationale.empty())
        err_details = p_rationale;
    else
        err_details = p_code;

    // Disable color codes if stdout is not a TTY.
    // This prevents Godot from writing ANSI escape codes when redirecting
    // stdout and stderr to a file.
    const bool tty = isatty(fileno(stdout));
    const char *gray = tty ? "\E[0;90m" : "";
    const char *red = tty ? "\E[0;91m" : "";
    const char *red_bold = tty ? "\E[1;31m" : "";
    const char *yellow = tty ? "\E[0;93m" : "";
    const char *yellow_bold = tty ? "\E[1;33m" : "";
    const char *magenta = tty ? "\E[0;95m" : "";
    const char *magenta_bold = tty ? "\E[1;35m" : "";
    const char *cyan = tty ? "\E[0;96m" : "";
    const char *cyan_bold = tty ? "\E[1;36m" : "";
    const char *reset = tty ? "\E[0m" : "";

    switch (p_type) {
        case ERR_WARNING:
            logf_error(FormatVE("%sWARNING:%s %.*s\n", yellow_bold, yellow, (int)err_details.size(),err_details.data()));
            break;
        case ERR_SCRIPT:
            logf_error(FormatVE("%sSCRIPT ERROR:%s %.*s\n", magenta_bold, magenta, (int)err_details.size(),err_details.data()));
            break;
        case ERR_SHADER:
            logf_error(FormatVE("%sSHADER ERROR:%s %.*s\n", cyan_bold, cyan, (int)err_details.size(),err_details.data()));
            break;
        case ERR_ERROR:
        default:
            logf_error(FormatVE("%sERROR:%s %.*s\n", red_bold, red, (int)err_details.size(),err_details.data()));
            break;
    }
    logf_error(FormatVE("%s     at: %.*s (%.*s:%i)%s\n",
                        gray,
                        (int)p_function.size(),p_function.data(),
                        (int)p_file.size(), p_file.data(),
                        p_line, reset));
}

UnixTerminalLogger::~UnixTerminalLogger() {}

OS_Unix::OS_Unix() {
    Vector<Logger *> loggers;
    loggers.push_back(memnew(UnixTerminalLogger));
    _set_logger(memnew_args(CompositeLogger,eastl::move(loggers)));
}

#endif
