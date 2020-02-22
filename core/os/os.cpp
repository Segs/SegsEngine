/*************************************************************************/
/*  os.cpp                                                               */
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

#include "os.h"

#include "core/image.h"
#include "core/method_enum_caster.h"
#include "core/object_db.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/input.h"
#include "core/os/main_loop.h"
#include "core/os/midi_driver.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/string_formatter.h"
#include "core/type_info.h"
#include "core/string_utils.inl"
#include "servers/audio_server.h"
#include "version_generated.gen.h"

#include "EASTL/fixed_hash_set.h"
#include "EASTL/string_view.h"
#include <QtCore/QStandardPaths>

#include <codecvt>

using namespace eastl;

namespace {
static String s_os_machine_id;
eastl::fixed_hash_set<se_string_view,16,4,true> s_dynamic_features;
} // end of anonymous namespace

OS *OS::singleton = nullptr;

OS *OS::get_singleton() {

    return singleton;
}

uint32_t OS::get_ticks_msec() const {
    return get_ticks_usec() / 1000;
}

String OS::get_iso_date_time(bool local) const {
    OS::Date date = get_date(local);
    OS::Time time = get_time(local);

    String timezone;
    if (!local) {
        TimeZoneInfo zone = get_time_zone_info();
        if (zone.bias >= 0) {
            timezone = "+";
        }
        timezone += FormatVE("%02d %02d",(zone.bias / 60),zone.bias % 60);
    } else {
        timezone = "Z";
    }

    return FormatVE("%02d-%02d-%02dT%02d:%02d:%02d",date.year,date.month,date.day,time.hour,time.min,time.sec) +
           timezone;
}

uint64_t OS::get_splash_tick_msec() const {
    return _msec_splash;
}
uint64_t OS::get_unix_time() const {

    return 0;
};
uint64_t OS::get_system_time_secs() const {
    return 0;
}
uint64_t OS::get_system_time_msecs() const {
    return 0;
}
void OS::debug_break(){

    // something
};

void OS::_set_logger(CompositeLogger *p_logger) {
    if (_logger) {
        memdelete(_logger);
    }
    _logger = p_logger;
}

void OS::add_logger(Logger *p_logger) {
    if (!_logger) {
        Vector<Logger *> loggers {p_logger};
        _logger = memnew(CompositeLogger(eastl::move(loggers)));
    } else {
        _logger->add_logger(p_logger);
    }
}

void OS::print_error(se_string_view p_function, se_string_view p_file, int p_line, se_string_view p_code, se_string_view p_rationale, Logger::ErrorType p_type) {

    _logger->log_error(p_function, p_file, p_line, p_code, p_rationale, p_type);
}

void OS::print(se_string_view p_msg) {
    _logger->logv(p_msg, false);
}

//void OS::print(const String &p_msg)
//{
//    _logger->logv(p_msg.constData(), false);
//};

void OS::printerr(se_string_view p_format) {
    _logger->logv(p_format, true);
};

void OS::set_keep_screen_on(bool p_enabled) {
    _keep_screen_on = p_enabled;
}

bool OS::is_keep_screen_on() const {
    return _keep_screen_on;
}

void OS::set_low_processor_usage_mode(bool p_enabled) {

    low_processor_usage_mode = p_enabled;
}

bool OS::is_in_low_processor_usage_mode() const {

    return low_processor_usage_mode;
}

void OS::set_low_processor_usage_mode_sleep_usec(int p_usec) {

    low_processor_usage_mode_sleep_usec = p_usec;
}

int OS::get_low_processor_usage_mode_sleep_usec() const {

    return low_processor_usage_mode_sleep_usec;
}

void OS::set_clipboard(se_string_view p_text) {

    _local_clipboard = p_text;
}
String OS::get_clipboard() const {

    return _local_clipboard;
}

String OS::get_executable_path() const {

    return _execpath;
}

Error OS::execute_utf8(se_string_view p_path, const Vector<String> &p_arguments, bool p_blocking,
        OS::ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex) {
    //TODO: SEGS: use QProcess ?
    List<String> converted_args;
    for(String a : p_arguments)
        converted_args.emplace_back(std::move(a));
    return execute(p_path,converted_args,p_blocking,r_child_id,r_pipe,r_exitcode,read_stderr,p_pipe_mutex);
}

int OS::get_process_id() const {

    return -1;
};

void OS::vibrate_handheld(int p_duration_ms) {

    WARN_PRINT("vibrate_handheld() only works with Android and iOS");
}

bool OS::is_stdout_verbose() const {

    return _verbose_stdout;
}

void OS::dump_memory_to_file(const char *p_file) {

    //Memory::dump_static_mem_to_file(p_file);
}

static FileAccess *_OSPRF = nullptr;

static void _OS_printres(Object *p_obj) {

    Resource *res = object_cast<Resource>(p_obj);
    if (!res)
        return;

    String str = FormatVE("%uz%s:%s - %s",res->get_instance_id(),res->get_class(),res->get_name().c_str(),res->get_path().c_str());
    if (_OSPRF)
        _OSPRF->store_line(str);
    else
        print_line(str);
}

bool OS::has_virtual_keyboard() const {

    return false;
}

void OS::show_virtual_keyboard(const String &p_existing_text, const Rect2 &p_screen_rect, int p_max_input_length) {
}

void OS::hide_virtual_keyboard() {
}

int OS::get_virtual_keyboard_height() const {
    return 0;
}

void OS::set_cursor_shape(CursorShape p_shape) {
}

OS::CursorShape OS::get_cursor_shape() const {
    return CURSOR_ARROW;
}

void OS::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
}

void OS::print_all_resources(se_string_view p_to_file) {

    ERR_FAIL_COND(!p_to_file.empty() && _OSPRF);
    if (!p_to_file.empty()) {

        Error err;
        _OSPRF = FileAccess::open(p_to_file, FileAccess::WRITE, &err);
        if (err != OK) {
            _OSPRF = nullptr;
            ERR_FAIL_MSG("Can't print all resources to file: " + String(p_to_file) + ".");
        }
    }

    ObjectDB::debug_objects(_OS_printres);

    if (!p_to_file.empty()) {

        if (_OSPRF)
            memdelete(_OSPRF);
        _OSPRF = nullptr;
    }
}

void OS::print_resources_in_use(bool p_short) {

    ResourceCache::dump(nullptr, p_short);
}

void OS::dump_resources_to_file(se_string_view p_file) {

    ResourceCache::dump(p_file);
}

void OS::set_no_window_mode(bool p_enable) {

    _no_window = p_enable;
}

bool OS::is_no_window_mode_enabled() const {

    return _no_window;
}

int OS::get_exit_code() const {

    return _exit_code;
}
void OS::set_exit_code(int p_code) {

    _exit_code = p_code;
}

const char *OS::get_locale() const {

    return "en";
}

// Helper function to ensure that a dir name/path will be valid on the OS
String OS::get_safe_dir_name(se_string_view p_dir_name, bool p_allow_dir_separator) const {

    constexpr char invalid_chars[7] = {':','*','?', '\\', '<','>','|'};

    String safe_dir_name(StringUtils::strip_edges(PathUtils::from_native_path(p_dir_name)));
    for (char s  : invalid_chars) {
        safe_dir_name.replace(s, '-');
    }
    if (p_allow_dir_separator) {
        // Dir separators are allowed, but disallow ".." to avoid going up the filesystem
        safe_dir_name.replace("..", "-");
    } else {
        safe_dir_name.replace("/", "-");
    }
    return safe_dir_name;
}

// Path to data, config, cache, etc. OS-specific folders

// Get properly capitalized engine name for system paths
String OS::get_godot_dir_name() const {

    // Default to lowercase, so only override when different case is needed
    return StringUtils::to_lower(se_string_view(VERSION_SHORT_NAME));
}

// OS equivalent of XDG_DATA_HOME
String OS::get_data_path() const {
    return qPrintable(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).front());
}

// OS equivalent of XDG_CONFIG_HOME
String OS::get_config_path() const {
    return qPrintable(QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation).front());
}

// OS equivalent of XDG_CACHE_HOME
String OS::get_cache_path() const {
    return qPrintable(QStandardPaths::standardLocations(QStandardPaths::CacheLocation).front());
}

// OS specific path for user://
String OS::get_user_data_dir() const {

    return ".";
};

// Absolute path to res://
String OS::get_resource_dir() const {

    return ProjectSettings::get_singleton()->get_resource_path();
}

// Access system-specific dirs like Documents, Downloads, etc.
String OS::get_system_dir(SystemDir p_dir) {
    QStandardPaths::StandardLocation translated = QStandardPaths::DocumentsLocation;
    switch (p_dir) {
        case SYSTEM_DIR_DESKTOP: translated = QStandardPaths::DesktopLocation; break;
        case SYSTEM_DIR_DCIM:  translated = QStandardPaths::PicturesLocation; break;
        case SYSTEM_DIR_DOCUMENTS:  translated = QStandardPaths::DocumentsLocation; break;
        case SYSTEM_DIR_DOWNLOADS:  translated = QStandardPaths::DownloadLocation; break;
        case SYSTEM_DIR_MOVIES:  translated = QStandardPaths::MoviesLocation; break;
        case SYSTEM_DIR_MUSIC:  translated = QStandardPaths::MusicLocation; break;
        case SYSTEM_DIR_PICTURES:  translated = QStandardPaths::PicturesLocation; break;
        default: ;
    }
    return StringUtils::to_utf8(QStandardPaths::standardLocations(translated).front());
}

Error OS::shell_open(se_string_view p_uri) {
    //TODO: use qt desktop services here
    return ERR_UNAVAILABLE;
};

// implement these with the canvas?
Error OS::dialog_show(UIString p_title, UIString p_description, const Vector<UIString> p_buttons, Object *p_obj, const StringName & p_callback) {
    using namespace StringUtils;
    while (true) {

        print(qPrintable(UIString("%1\n--------\n%2\n").arg(p_title,p_description)));
        for (int i = 0; i < p_buttons.size(); i++) {
            if (i > 0) print(se_string_view(", "));
            print(qPrintable(UIString("%1=%2").arg(i + 1).arg(p_buttons[i])));
        }
        print(se_string_view("\n"));
        String res(StringUtils::strip_edges(get_stdin_string()));
        if (!is_numeric(res))
            continue;
        int n = to_int(res);
        if (n < 0 || n >= p_buttons.size())
            continue;
        if (p_obj && !p_callback.empty())
            p_obj->call_deferred(p_callback, n);
        break;
    }
    return OK;
};

Error OS::dialog_input_text(const UIString &p_title, const UIString &p_description, const UIString &p_partial, Object *p_obj, const StringName &p_callback) {

    ERR_FAIL_COND_V(!p_obj, FAILED);
    ERR_FAIL_COND_V(p_callback.empty(), FAILED);
    print(FormatVE("%s\n---------\n%s\n[%s]:\n",qPrintable(p_title), qPrintable(p_description), qPrintable(p_partial)));

    String res(StringUtils::strip_edges(get_stdin_string()));
    bool success = true;
    if (res.empty()) {
        res = StringUtils::to_utf8(p_partial).data();
    }

    p_obj->call_deferred(p_callback, success, Variant(res));

    return OK;
};

uint64_t OS::get_static_memory_usage() const {

    return Memory::get_mem_usage();
}
uint64_t OS::get_dynamic_memory_usage() const {

    return MemoryPool::total_memory;
}

uint64_t OS::get_static_memory_peak_usage() const {

    return Memory::get_mem_max_usage();
}

Error OS::set_cwd(se_string_view p_cwd) {

    return ERR_CANT_OPEN;
}

bool OS::has_touchscreen_ui_hint() const {

    //return false;
    return Input::get_singleton() && Input::get_singleton()->is_emulating_touch_from_mouse();
}

uint64_t OS::get_free_static_memory() const {

    return Memory::get_mem_available();
}

void OS::yield() {
}

void OS::set_screen_orientation(ScreenOrientation p_orientation) {

    _orientation = p_orientation;
}

OS::ScreenOrientation OS::get_screen_orientation() const {

    return (OS::ScreenOrientation)_orientation;
}

void OS::_ensure_user_data_dir() {

    String dd(get_user_data_dir());
    DirAccess *da = DirAccess::open(dd);
    if (da) {
        memdelete(da);
        return;
    }

    da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    Error err = da->make_dir_recursive(dd);
    ERR_FAIL_COND_MSG(err != OK, "Error attempting to create data dir: " + dd + ".");

    memdelete(da);
}

void OS::set_native_icon(const String &p_filename) {
}

void OS::set_icon(const Ref<Image> &p_icon) {
}

String OS::get_model_name() const {

    return String("GenericDevice");
}

void OS::set_cmdline(se_string_view p_execpath, const List<String> &p_args) {

    _execpath = p_execpath;
    _cmdline = p_args;
};

void OS::release_rendering_thread() {
}

void OS::make_rendering_thread() {
}

void OS::swap_buffers() {
}

const String &OS::get_unique_id() const {
    if(s_os_machine_id.empty())
        s_os_machine_id = QSysInfo::machineUniqueId().data();
    return s_os_machine_id;
}

int OS::get_processor_count() const {

    return 1;
}

Error OS::native_video_play(se_string_view p_path, float p_volume, se_string_view p_audio_track, se_string_view p_subtitle_track) {

    return FAILED;
};

bool OS::native_video_is_playing() const {

    return false;
};

void OS::native_video_pause(){

};

void OS::native_video_unpause(){

};

void OS::native_video_stop(){

};

void OS::set_mouse_mode(MouseMode p_mode) {
}

bool OS::can_use_threads() const {
    return true;
}

OS::MouseMode OS::get_mouse_mode() const {

    return MOUSE_MODE_VISIBLE;
}

OS::LatinKeyboardVariant OS::get_latin_keyboard_variant() const {

    return LATIN_KEYBOARD_QWERTY;
}

bool OS::is_joy_known(int p_device) {
    return true;
}

StringName OS::get_joy_guid(int p_device) const {
    return "Default Joypad";
}

void OS::set_context(int p_context) {
}

OS::SwitchVSyncCallbackInThread OS::switch_vsync_function = nullptr;

void OS::set_use_vsync(bool p_enable) {
    _use_vsync = p_enable;
    if (switch_vsync_function) { //if a function was set, use function
        switch_vsync_function(p_enable);
    } else { //otherwise just call here
        _set_use_vsync(p_enable);
    }
}

bool OS::is_vsync_enabled() const {

    return _use_vsync;
}

void OS::set_vsync_via_compositor(bool p_enable) {
    _vsync_via_compositor = p_enable;
}

bool OS::is_vsync_via_compositor_enabled() const {
    return _vsync_via_compositor;
}

void OS::set_has_server_feature_callback(HasServerFeatureCallback p_callback) {

    has_server_feature_callback = p_callback;
}

bool OS::has_feature(se_string_view p_feature) {

    if (p_feature == se_string_view(get_name()))
        return true;
    if constexpr(sizeof(void *) == 8) {
        if (p_feature == "64"_sv) {
            return true;
        }
    }
    else if constexpr(sizeof(void *) == 4) {
        if(p_feature == "32"_sv)
            return true;
    }
#if defined(__x86_64) || defined(__x86_64__) || defined(__amd64__)
    if (p_feature == "x86_64"_sv) {
        return true;
    }
#elif (defined(__i386) || defined(__i386__))
    if (p_feature == "x86") {
        return true;
    }
#elif defined(__arm__)
    if (p_feature == "arm") {
        return true;
    }
#endif
    auto iter = s_dynamic_features.find(p_feature);
    if(iter!=s_dynamic_features.end())
        return true;

    if (_check_internal_feature_support(p_feature))
        return true;

    if (has_server_feature_callback && has_server_feature_callback(p_feature)) {
        return true;
    }

    if (ProjectSettings::get_singleton()->has_custom_feature(p_feature))
        return true;

    return false;
}

void OS::register_feature(const char *name)
{
    assert(!s_dynamic_features.contains(name));
    s_dynamic_features.emplace(name);
}
void OS::unregister_feature(const char *name)
{
    assert(s_dynamic_features.contains(name));
    s_dynamic_features.erase(name);
}

void OS::center_window() {

    if (is_window_fullscreen()) return;

    Point2 sp = get_screen_position(get_current_screen());
    Size2 scr = get_screen_size(get_current_screen());
    Size2 wnd = get_real_window_size();

    int x = sp.width + (scr.width - wnd.width) / 2;
    int y = sp.height + (scr.height - wnd.height) / 2;

    set_window_position(Vector2(x, y));
}

int OS::get_video_driver_count() const {

    return 2;
}

const char *OS::get_video_driver_name(int p_driver) const {

    switch (p_driver) {
        case VIDEO_DRIVER_GLES3:
            return "GLES3";
        case VIDEO_DRIVER_VULKAN:
        default:
            return "Vulkan";
    }
}

int OS::get_audio_driver_count() const {

    return AudioDriverManager::get_driver_count();
}

const char *OS::get_audio_driver_name(int p_driver) const {

    AudioDriver *driver = AudioDriverManager::get_driver(p_driver);
    ERR_FAIL_COND_V_MSG(!driver, "", "Cannot get audio driver at index '" + itos(p_driver) + "'.");
    return AudioDriverManager::get_driver(p_driver)->get_name();
}

void OS::set_restart_on_exit(bool p_restart, const List<String> &p_restart_arguments) {
    restart_on_exit = p_restart;
    restart_commandline = p_restart_arguments;
}

bool OS::is_restart_on_exit_set() const {
    return restart_on_exit;
}

List<String> OS::get_restart_on_exit_arguments() const {
    return restart_commandline;
}

PoolVector<String> OS::get_granted_permissions() const { return PoolVector<String>(); }

PoolVector<String> OS::get_connected_midi_inputs() {

    if (MIDIDriver::get_singleton())
        return MIDIDriver::get_singleton()->get_connected_inputs();

    PoolStringArray list;
    return list;
}

void OS::open_midi_inputs() {

    if (MIDIDriver::get_singleton())
        MIDIDriver::get_singleton()->open();
}

void OS::close_midi_inputs() {

    if (MIDIDriver::get_singleton())
        MIDIDriver::get_singleton()->close();
}

OS::OS() {
    void *volatile stack_bottom;

    restart_on_exit = false;
    singleton = this;
    _keep_screen_on = true; // set default value to true, because this had been true before godot 2.0.
    low_processor_usage_mode = false;
    low_processor_usage_mode_sleep_usec = 10000;
    _verbose_stdout = false;
    _no_window = false;
    _exit_code = 0;
    _orientation = SCREEN_LANDSCAPE;

    _render_thread_mode = RENDER_THREAD_SAFE;

    _allow_hidpi = false;
    _allow_layered = false;
    _stack_bottom = (void *)(&stack_bottom);

    _logger = nullptr;

    has_server_feature_callback = nullptr;

    Vector<Logger *> loggers { memnew(StdLogger) };
    _set_logger(memnew_args(CompositeLogger,eastl::move(loggers)));
}

OS::~OS() {
    memdelete(_logger);
    singleton = nullptr;
}
