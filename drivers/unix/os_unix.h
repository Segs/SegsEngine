/*************************************************************************/
/*  os_unix.h                                                            */
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

#ifndef OS_UNIX_H
#define OS_UNIX_H

#ifdef UNIX_ENABLED

#include "core/os/os.h"
#include "drivers/unix/ip_unix.h"

class OS_Unix : public OS {

protected:
    // UNIX only handles the core functions.
    // inheriting platforms under unix (eg. X11) should handle the rest

    void initialize_core() override;
    virtual int unix_initialize_audio(int p_audio_driver);
    //virtual Error initialize(int p_video_driver,int p_audio_driver);

    void finalize_core() override;

    String stdin_buf;

public:
    OS_Unix();

    void alert(StringView p_alert, StringView p_title = StringView("ALERT!")) override;
    String get_stdin_string(bool p_block) override;

    //virtual void set_mouse_show(bool p_show);
    //virtual void set_mouse_grab(bool p_grab);
    //virtual bool is_mouse_grab_enabled() const = 0;
    //virtual void get_mouse_position(int &x, int &y) const;
    //virtual void set_window_title(const String& p_title);

    //virtual void set_video_mode(const VideoMode& p_video_mode);
    //virtual VideoMode get_video_mode() const;
    //virtual void get_fullscreen_mode_list(Vector<VideoMode> *p_list) const;

    Error open_dynamic_library(StringView p_path, void *&p_library_handle, bool p_also_set_library_path = false) override;
    Error close_dynamic_library(void *p_library_handle) override;
    Error get_dynamic_library_symbol_handle(void *p_library_handle, StringView p_name, void *&p_symbol_handle, bool p_optional = false) override;

    Error set_cwd(StringView p_cwd) override;

    String get_name() const override;

    Date get_date(bool utc) const override;
    Time get_time(bool utc) const override;
    TimeZoneInfo get_time_zone_info() const override;

    uint64_t get_unix_time() const override;
    uint64_t get_system_time_secs() const override;
    uint64_t get_system_time_msecs() const override;

    void delay_usec(uint32_t p_usec) const override;
    uint64_t get_ticks_usec() const override;

    Error execute(StringView p_path, const Vector<String> &p_arguments, bool p_blocking=true, ProcessID *r_child_id = nullptr, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr) override;
    Error kill(const ProcessID &p_pid) override;
    int get_process_id() const override;

    bool has_environment(StringView p_var) const override;
    String get_environment(StringView p_var) const override;
    bool set_environment(StringView p_var, StringView p_value) const override;
    const char * get_locale() const override;

    int get_processor_count() const override;

    void debug_break() override;
    void initialize_debugging() override;

    String get_executable_path() const override;
    String get_user_data_dir() const override;
};

class UnixTerminalLogger : public StdLogger {
public:
    void log_error(StringView p_function, StringView p_file, int p_line, StringView p_code, StringView p_rationale, ErrorType p_type = ERR_ERROR) override;
    ~UnixTerminalLogger() override;
};

#endif

#endif
