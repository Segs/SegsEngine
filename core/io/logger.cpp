/*************************************************************************/
/*  logger.cpp                                                           */
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

#include "logger.h"
#include "core/rotated_file_loger.h"

#include "core/os/file_access.h"
#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/string_formatter.h"
#include "core/set.h"

#include <cstdio>

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
//#define sprintf sprintf_s
#endif

bool Logger::should_log(bool p_err) {
    return (!p_err || _print_error_enabled) && (p_err || _print_line_enabled);
}

bool Logger::_flush_stdout_on_print = true;

void Logger::set_flush_stdout_on_print(bool value) {
    _flush_stdout_on_print = value;
}


void Logger::log_error(StringView p_function, StringView p_file, int p_line, StringView p_code, StringView p_rationale, ErrorType p_type) {
    if (!should_log(true)) {
        return;
    }

    const char *err_type = "ERROR";
    switch (p_type) {
        case ERR_ERROR:
            err_type = "ERROR";
            break;
        case ERR_WARNING:
            err_type = "WARNING";
            break;
        case ERR_SCRIPT:
            err_type = "SCRIPT ERROR";
            break;
        case ERR_SHADER:
            err_type = "SHADER ERROR";
            break;
        default:
            ERR_PRINT("Unknown error type");
            break;
    }

    StringView err_details;
    if (!p_rationale.empty())
        err_details = p_rationale;
    else
        err_details = p_code;

    logf_error(FormatVE("%s: %.*s\n",err_type,(int)err_details.size(),err_details.data()));
    logf_error(FormatVE("   at: %.*s (%.*s:%i) - %.*s\n",
               (int)p_function.size(),p_function.data(),
               (int)p_file.size(),p_file.data(),p_line,
               (int)p_code.size(),p_code.data())
      );
}

void Logger::logf(StringView p_msg) {
    if (!should_log(false)) {
        return;
    }

    logv(p_msg, false);
}

void Logger::logf_error(StringView p_msg) {
    if (!should_log(true)) {
        return;
    }

    logv(p_msg, true);
}
Logger::~Logger() = default;

void RotatedFileLogger::close_file() {
    memdelete(file);
    file = nullptr;
}

void RotatedFileLogger::clear_old_backups() const {
    int max_backups = max_files - 1; // -1 for the current file

    StringView basename =  PathUtils::get_basename(PathUtils::get_file(base_path));
    StringView extension = PathUtils::get_extension(base_path);

    DirAccess *da = DirAccess::open(PathUtils::get_base_dir(base_path));
    if (!da) {
        return;
    }

    da->list_dir_begin();
    String f = da->get_next();
    Set<String> backups;
    while (!f.empty()) {
        if (!da->current_is_dir() && StringUtils::begins_with(f, basename) && PathUtils::get_extension(f) == extension &&
                StringView(f) != PathUtils::get_file(base_path)) {
            backups.insert(f);
        }
        f = da->get_next();
    }
    da->list_dir_end();

    if (backups.size() > max_backups) {
        // since backups are appended with timestamp and Set iterates them in sorted order,
        // first backups are the oldest
        int to_delete = backups.size() - max_backups;
        for (const String &E : backups) {
            if(to_delete--<=0)
                break;
            da->remove(E);
        }
    }

    memdelete(da);
}

void RotatedFileLogger::rotate_file() {
    close_file();

    if (FileAccess::exists(base_path)) {
        if (max_files > 1) {
            char timestamp[21];
            OS::Date date = OS::get_singleton()->get_date();
            OS::Time time = OS::get_singleton()->get_time();
            sprintf(timestamp, "_%04d-%02d-%02d_%02d.%02d.%02d", date.year, date.month, date.day, time.hour, time.min, time.sec);

            String backup_name = String(PathUtils::get_basename(base_path)) + timestamp;
            if (!PathUtils::get_extension(base_path).empty()) {
                backup_name.push_back('.');
                backup_name.append(PathUtils::get_extension(base_path));
            }

            DirAccess *da = DirAccess::open(PathUtils::get_base_dir(base_path));
            if (da) {
                da->copy(base_path, backup_name);
                memdelete(da);
            }
            clear_old_backups();
        }
    } else {
        DirAccess *da = DirAccess::create(DirAccess::ACCESS_USERDATA);
        if (da) {
            da->make_dir_recursive(PathUtils::get_base_dir(base_path));
            memdelete(da);
        }
    }

    file = FileAccess::open(base_path, FileAccess::WRITE);
}

RotatedFileLogger::RotatedFileLogger(const String &p_base_path, int p_max_files) :
        base_path(PathUtils::simplify_path(p_base_path)),
        max_files(p_max_files > 0 ? p_max_files : 1),
        file(nullptr) {
    rotate_file();
}

void RotatedFileLogger::logv(StringView p_format, bool p_err) {
    if (!should_log(p_err)) {
        return;
    }

    if (!file)
        return;

    file->store_buffer((const uint8_t *)p_format.data(), p_format.length());

    if (p_err || _flush_stdout_on_print) {
        // Don't always flush when printing stdout to avoid performance
        // issues when `print()` is spammed in release builds.
        file->flush();
    }

}

RotatedFileLogger::~RotatedFileLogger() {
    close_file();
}

void StdLogger::logv(StringView p_format, bool p_err) {
    if (!should_log(p_err)) {
        return;
    }

    if (p_err) {
        fprintf(stderr, "%.*s",uint32_t(p_format.length()),p_format.data());
    } else {
        printf("%.*s", uint32_t(p_format.length()),p_format.data());
        if (_flush_stdout_on_print) {
            // Don't always flush when printing stdout to avoid performance
            // issues when `print()` is spammed in release builds.
            fflush(stdout);
        }
    }
}

StdLogger::~StdLogger() = default;

CompositeLogger::CompositeLogger(Vector<Logger *> && p_loggers) :
        loggers(p_loggers) {
}

void CompositeLogger::logv(StringView p_msg, bool p_err) {
    if (!should_log(p_err)) {
        return;
    }

    for (Logger * l : loggers) {
        l->logv(p_msg, p_err);
    }
}
void CompositeLogger::log_error(StringView p_function, StringView p_file, int p_line, StringView p_code, StringView p_rationale, ErrorType p_type) {
    if (!should_log(true)) {
        return;
    }

    for (Logger * l : loggers) {
        l->log_error(p_function, p_file, p_line, p_code, p_rationale, p_type);
    }
}

void CompositeLogger::add_logger(Logger *p_logger) {
    loggers.push_back(p_logger);
}

CompositeLogger::~CompositeLogger() {
    for (Logger * l : loggers) {
        memdelete(l);
    }
}
