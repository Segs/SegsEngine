/*************************************************************************/
/*  core_bind.cpp                                                        */
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

#include "core_bind.h"

#include "core/crypto/crypto_core.h"
#include "core/image.h"
#include "core/io/file_access_compressed.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/ip_address.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/io/resource_format_loader.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/keyboard.h"
#include "core/os/main_loop.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/string.h"
#include "core/string_formatter.h"

#include "EASTL/sort.h"
#include "EASTL/map.h"
VARIANT_ENUM_CAST(_ResourceManager::SaverFlags);
VARIANT_ENUM_CAST(_OS::VideoDriver);
VARIANT_ENUM_CAST(_OS::Weekday);
VARIANT_ENUM_CAST(_OS::Month);
VARIANT_ENUM_CAST(_OS::SystemDir);
VARIANT_ENUM_CAST(_OS::ScreenOrientation);
VARIANT_ENUM_CAST(_OS::HandleType);
VARIANT_ENUM_CAST(_Geometry::PolyBooleanOperation);
VARIANT_ENUM_CAST(_Geometry::PolyJoinType);
VARIANT_ENUM_CAST(_Geometry::PolyEndType);
VARIANT_ENUM_CAST(_File::ModeFlags);
VARIANT_ENUM_CAST(_File::CompressionMode);
VARIANT_ENUM_CAST(_Thread::Priority);

/**
 *  Time constants borrowed from loc_time.h
 */
#define EPOCH_YR 1970 /* EPOCH = Jan 1 1970 00:00:00 */
#define SECS_DAY (24L * 60L * 60L)
#define LEAPYEAR(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEARSIZE(year) (LEAPYEAR(year) ? 366 : 365)
#define SECOND_KEY "second"
#define MINUTE_KEY "minute"
#define HOUR_KEY "hour"
#define DAY_KEY "day"
#define MONTH_KEY "month"
#define YEAR_KEY "year"
#define WEEKDAY_KEY "weekday"
#define DST_KEY "dst"

/// Table of number of days in each month (for regular year and leap year)
static const unsigned int MONTH_DAYS_TABLE[2][12] = { { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } };

_ResourceManager *_ResourceManager::singleton = nullptr;

IMPL_GDCLASS(_ResourceManager)

Ref<ResourceInteractiveLoader> _ResourceManager::load_interactive(StringView p_path, StringView p_type_hint, bool p_no_cache) {
    return gResourceManager().load_interactive(p_path, p_type_hint,p_no_cache);
}

RES _ResourceManager::load(StringView p_path, StringView p_type_hint, bool p_no_cache) {
    Error err = OK;
    RES ret(gResourceManager().load(p_path, p_type_hint, p_no_cache, &err));

    ERR_FAIL_COND_V_MSG(err != OK, ret, "Error loading resource: '" + String(p_path) + "'.");
    return ret;
}

PoolStringArray _ResourceManager::get_recognized_extensions_for_type(StringView p_type) {
    Vector<String> exts;
    gResourceManager().get_recognized_extensions_for_type(p_type, exts);
    PoolStringArray ret;
    for (const String &E : exts) {
        ret.push_back(E);
    }

    return ret;
}

void _ResourceManager::set_abort_on_missing_resources(bool p_abort) {
    gResourceManager().set_abort_on_missing_resources(p_abort);
}

Vector<String> _ResourceManager::get_dependencies(StringView p_path) {
    Vector<String> deps;
    gResourceManager().get_dependencies(p_path, deps);
    return deps;
}

bool _ResourceManager::has_cached(StringView p_path) {
    String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
    return ResourceCache::has(local_path);
}

bool _ResourceManager::exists(StringView p_path, StringView p_type_hint) {
    return gResourceManager().exists(p_path, p_type_hint);
}

Error _ResourceManager::save(StringView p_path, const Ref<Resource> &p_resource, SaverFlags p_flags) {
    ERR_FAIL_COND_V_MSG(
            not p_resource, ERR_INVALID_PARAMETER, "Can't save empty resource to path: " + String(p_path) + ".");
    return gResourceManager().save(p_path, p_resource, p_flags);
}

PoolVector<String> _ResourceManager::get_recognized_extensions(const RES &p_resource) {
    ERR_FAIL_COND_V_MSG(not p_resource, PoolVector<String>(), "It's not a reference to a valid Resource object.");
    Vector<String> exts;
    gResourceManager().get_recognized_extensions(p_resource, exts);
    PoolVector<String> ret;
    for (int i = 0, fin = exts.size(); i < fin; ++i) {
        ret.push_back(exts[i]);
    }
    return ret;
}

void _ResourceManager::_bind_methods() {
    SE_BIND_METHOD_WITH_DEFAULTS(_ResourceManager, save, DEFVAL(0) );
    SE_BIND_METHOD(_ResourceManager,get_recognized_extensions);

    SE_BIND_METHOD_WITH_DEFAULTS(_ResourceManager, load_interactive, DEFVAL(String()), DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(_ResourceManager, load, DEFVAL(String()), DEFVAL(false) );
    SE_BIND_METHOD(_ResourceManager,get_recognized_extensions_for_type);
    SE_BIND_METHOD(_ResourceManager,set_abort_on_missing_resources);
    SE_BIND_METHOD(_ResourceManager,get_dependencies);
    SE_BIND_METHOD(_ResourceManager,has_cached);
    SE_BIND_METHOD_WITH_DEFAULTS(_ResourceManager, exists, DEFVAL(String()) );

    BIND_ENUM_CONSTANT(FLAG_RELATIVE_PATHS);
    BIND_ENUM_CONSTANT(FLAG_BUNDLE_RESOURCES);
    BIND_ENUM_CONSTANT(FLAG_CHANGE_PATH);
    BIND_ENUM_CONSTANT(FLAG_OMIT_EDITOR_PROPERTIES);
    BIND_ENUM_CONSTANT(FLAG_SAVE_BIG_ENDIAN);
    BIND_ENUM_CONSTANT(FLAG_COMPRESS);
    BIND_ENUM_CONSTANT(FLAG_REPLACE_SUBRESOURCE_PATHS);
}

_ResourceManager::_ResourceManager() {
    singleton = this;
}

/////////////////OS

void _OS::global_menu_add_item(
        const StringName &p_menu, const StringName &p_label, const Variant &p_signal, const Variant &p_meta) {
    OS::get_singleton()->global_menu_add_item(p_menu, p_label, p_signal, p_meta);
}

void _OS::global_menu_add_separator(const StringName &p_menu) {
    OS::get_singleton()->global_menu_add_separator(p_menu);
}

void _OS::global_menu_remove_item(const StringName &p_menu, int p_idx) {
    OS::get_singleton()->global_menu_remove_item(p_menu, p_idx);
}

void _OS::global_menu_clear(const StringName &p_menu) {
    OS::get_singleton()->global_menu_clear(p_menu);
}
Point2 _OS::get_mouse_position() const {
    return OS::get_singleton()->get_mouse_position();
}

void _OS::set_window_title(const String &p_title) {
    OS::get_singleton()->set_window_title(p_title);
}

void _OS::set_window_mouse_passthrough(const PoolVector2Array &p_region) {
    OS::get_singleton()->set_window_mouse_passthrough(p_region);
}

int _OS::get_mouse_button_state() const {
    return OS::get_singleton()->get_mouse_button_state();
}

const String &_OS::get_unique_id() const {
    return OS::get_singleton()->get_unique_id();
}
bool _OS::has_touchscreen_ui_hint() const {
    return OS::get_singleton()->has_touchscreen_ui_hint();
}

void _OS::set_clipboard(StringView p_text) {
    OS::get_singleton()->set_clipboard(p_text);
}
String _OS::get_clipboard() const {
    return OS::get_singleton()->get_clipboard();
}

bool _OS::has_clipboard() const {
    return OS::get_singleton()->has_clipboard();
}
int _OS::get_video_driver_count() const {
    return OS::get_singleton()->get_video_driver_count();
}

String _OS::get_video_driver_name(VideoDriver p_driver) const {
    return String(OS::get_singleton()->get_video_driver_name((int)p_driver));
}

_OS::VideoDriver _OS::get_current_video_driver() const {
    return (VideoDriver)OS::get_singleton()->get_current_video_driver();
}

int _OS::get_audio_driver_count() const {
    return OS::get_singleton()->get_audio_driver_count();
}

String _OS::get_audio_driver_name(int p_driver) const {
    return String(OS::get_singleton()->get_audio_driver_name(p_driver));
}

PoolStringArray _OS::get_connected_midi_inputs() {
    return OS::get_singleton()->get_connected_midi_inputs();
}

void _OS::open_midi_inputs() {
    OS::get_singleton()->open_midi_inputs();
}

void _OS::close_midi_inputs() {
    OS::get_singleton()->close_midi_inputs();
}

void _OS::set_video_mode(Size2 p_size, bool p_fullscreen, bool p_resizeable, int p_screen) {
    OS::VideoMode vm;
    vm.width = p_size.width;
    vm.height = p_size.height;
    vm.fullscreen = p_fullscreen;
    vm.resizable = p_resizeable;
    OS::get_singleton()->set_video_mode(vm, p_screen);
}
Size2 _OS::get_video_mode(int p_screen) const {
    OS::VideoMode vm = OS::get_singleton()->get_video_mode(p_screen);
    return Size2(vm.width, vm.height);
}
bool _OS::is_video_mode_fullscreen(int p_screen) const {
    OS::VideoMode vm = OS::get_singleton()->get_video_mode(p_screen);
    return vm.fullscreen;
}

int _OS::get_screen_count() const {
    return OS::get_singleton()->get_screen_count();
}

int _OS::get_current_screen() const {
    return OS::get_singleton()->get_current_screen();
}

void _OS::set_current_screen(int p_screen) {
    OS::get_singleton()->set_current_screen(p_screen);
}

Point2 _OS::get_screen_position(int p_screen) const {
    return OS::get_singleton()->get_screen_position(p_screen);
}

Size2 _OS::get_screen_size(int p_screen) const {
    return OS::get_singleton()->get_screen_size(p_screen);
}

int _OS::get_screen_dpi(int p_screen) const {
    return OS::get_singleton()->get_screen_dpi(p_screen);
}

float _OS::get_screen_scale(int p_screen) const {
    return OS::get_singleton()->get_screen_scale(p_screen);
}

float _OS::get_screen_max_scale() const {
    return OS::get_singleton()->get_screen_max_scale();
}

float _OS::get_screen_refresh_rate(int p_screen) const {
    return OS::get_singleton()->get_screen_refresh_rate();
}
Point2 _OS::get_window_position() const {
    return OS::get_singleton()->get_window_position();
}

void _OS::set_window_position(const Point2 &p_position) {
    OS::get_singleton()->set_window_position(p_position);
}

Size2 _OS::get_max_window_size() const {
    return OS::get_singleton()->get_max_window_size();
}

Size2 _OS::get_min_window_size() const {
    return OS::get_singleton()->get_min_window_size();
}

Size2 _OS::get_window_size() const {
    return OS::get_singleton()->get_window_size();
}

Size2 _OS::get_real_window_size() const {
    return OS::get_singleton()->get_real_window_size();
}

void _OS::set_max_window_size(const Size2 &p_size) {
    OS::get_singleton()->set_max_window_size(p_size);
}

void _OS::set_min_window_size(const Size2 &p_size) {
    OS::get_singleton()->set_min_window_size(p_size);
}

void _OS::set_window_size(const Size2 &p_size) {
    OS::get_singleton()->set_window_size(p_size);
}

Rect2 _OS::get_window_safe_area() const {
    return OS::get_singleton()->get_window_safe_area();
}

void _OS::set_window_fullscreen(bool p_enabled) {
    OS::get_singleton()->set_window_fullscreen(p_enabled);
}

bool _OS::is_window_fullscreen() const {
    return OS::get_singleton()->is_window_fullscreen();
}

void _OS::set_window_resizable(bool p_enabled) {
    OS::get_singleton()->set_window_resizable(p_enabled);
}

bool _OS::is_window_resizable() const {
    return OS::get_singleton()->is_window_resizable();
}

void _OS::set_window_minimized(bool p_enabled) {
    OS::get_singleton()->set_window_minimized(p_enabled);
}

bool _OS::is_window_minimized() const {
    return OS::get_singleton()->is_window_minimized();
}

void _OS::set_window_maximized(bool p_enabled) {
    OS::get_singleton()->set_window_maximized(p_enabled);
}

bool _OS::is_window_maximized() const {
    return OS::get_singleton()->is_window_maximized();
}

void _OS::set_window_always_on_top(bool p_enabled) {
    OS::get_singleton()->set_window_always_on_top(p_enabled);
}

bool _OS::is_window_always_on_top() const {
    return OS::get_singleton()->is_window_always_on_top();
}
bool _OS::is_window_focused() const {
    return OS::get_singleton()->is_window_focused();
}
void _OS::set_borderless_window(bool p_borderless) {
    OS::get_singleton()->set_borderless_window(p_borderless);
}

bool _OS::get_window_per_pixel_transparency_enabled() const {
    return OS::get_singleton()->get_window_per_pixel_transparency_enabled();
}

void _OS::set_window_per_pixel_transparency_enabled(bool p_enabled) {
    OS::get_singleton()->set_window_per_pixel_transparency_enabled(p_enabled);
}

bool _OS::get_borderless_window() const {
    return OS::get_singleton()->get_borderless_window();
}

void _OS::set_ime_active(const bool p_active) {
    OS::get_singleton()->set_ime_active(p_active);
}

void _OS::set_ime_position(const Point2 &p_pos) {
    OS::get_singleton()->set_ime_position(p_pos);
}

Point2 _OS::get_ime_selection() const {
    return OS::get_singleton()->get_ime_selection();
}

String _OS::get_ime_text() const {
    return OS::get_singleton()->get_ime_text();
}

void _OS::set_use_file_access_save_and_swap(bool p_enable) {
    FileAccess::set_backup_save(p_enable);
}

bool _OS::is_video_mode_resizable(int p_screen) const {
    OS::VideoMode vm = OS::get_singleton()->get_video_mode(p_screen);
    return vm.resizable;
}

Array _OS::get_fullscreen_mode_list(int p_screen) const {
    Vector<OS::VideoMode> vmlist;
    OS::get_singleton()->get_fullscreen_mode_list(&vmlist, p_screen);
    Array vmarr;
    for (const OS::VideoMode &E : vmlist) {
        vmarr.push_back(Size2(E.width, E.height));
    }

    return vmarr;
}

void _OS::set_low_processor_usage_mode(bool p_enabled) {
    OS::get_singleton()->set_low_processor_usage_mode(p_enabled);
}
bool _OS::is_in_low_processor_usage_mode() const {
    return OS::get_singleton()->is_in_low_processor_usage_mode();
}

void _OS::set_low_processor_usage_mode_sleep_usec(int p_usec) {
    OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(p_usec);
}

int _OS::get_low_processor_usage_mode_sleep_usec() const {
    return OS::get_singleton()->get_low_processor_usage_mode_sleep_usec();
}

String _OS::get_executable_path() const {
    return OS::get_singleton()->get_executable_path();
}

Error _OS::shell_open(const String& p_uri) {
    if (p_uri.starts_with("res://")) {
        WARN_PRINT("Attempting to open an URL with the \"res://\" protocol. Use `ProjectSettings.globalize_path()` to "
                   "convert a Godot-specific path to a system path before opening it with `OS.shell_open()`.");
    } else if (p_uri.starts_with("user://")) {
        WARN_PRINT("Attempting to open an URL with the \"user://\" protocol. Use `ProjectSettings.globalize_path()` to "
                   "convert a Godot-specific path to a system path before opening it with `OS.shell_open()`.");
    }
    return OS::get_singleton()->shell_open(eastl::move(p_uri));
}

int _OS::execute(
        StringView p_path, const Vector<String> &p_arguments, bool p_blocking, Array p_output, bool p_read_stderr, bool p_open_console) {
    OS::ProcessID pid = -2;
    int exitcode = 0;
    String pipe;
    Error err = OS::get_singleton()->execute(p_path, p_arguments, p_blocking, &pid, &pipe, &exitcode, p_read_stderr, nullptr, p_open_console);
    p_output.clear();
    p_output.push_back(Variant(pipe));
    if (err != OK) {
        return -1;
    } else if (p_blocking) {
        return exitcode;
    } else {
        return pid;
    }
}

Error _OS::kill(int p_pid) {
    return OS::get_singleton()->kill(p_pid);
}

int _OS::get_process_id() const {
    return OS::get_singleton()->get_process_id();
}

bool _OS::has_environment(const String &p_var) const {
    return OS::get_singleton()->has_environment(p_var);
}
String _OS::get_environment(const String &p_var) const {
    return OS::get_singleton()->get_environment(p_var);
}

bool _OS::set_environment(const String &p_var, const String &p_value) const {
    return OS::get_singleton()->set_environment(p_var, p_value);
}
String _OS::get_name() const {
    return OS::get_singleton()->get_name();
}
PoolVector<String> _OS::get_cmdline_args() {
    const Vector<String> &cmdline = OS::get_singleton()->get_cmdline_args();
    PoolVector<String> cmdlinev;
    for (const String &E : cmdline) {
        cmdlinev.push_back(E);
    }

    return cmdlinev;
}

String _OS::get_locale() const {
    return OS::get_singleton()->get_locale();
}
String _OS::get_locale_language() const {
    return OS::get_singleton()->get_locale_language();
}

String _OS::get_latin_keyboard_variant() const {
    switch (OS::get_singleton()->get_latin_keyboard_variant()) {
        case OS::LATIN_KEYBOARD_QWERTY:
            return ("QWERTY");
        case OS::LATIN_KEYBOARD_QWERTZ:
            return ("QWERTZ");
        case OS::LATIN_KEYBOARD_AZERTY:
            return ("AZERTY");
        case OS::LATIN_KEYBOARD_QZERTY:
            return ("QZERTY");
        case OS::LATIN_KEYBOARD_DVORAK:
            return ("DVORAK");
        case OS::LATIN_KEYBOARD_NEO:
            return ("NEO");
        case OS::LATIN_KEYBOARD_COLEMAK:
            return ("COLEMAK");
        default:
            return ("ERROR");
    }
}

String _OS::get_model_name() const {
    return OS::get_singleton()->get_model_name();
}

bool _OS::is_ok_left_and_cancel_right() const {
    return OS::get_singleton()->get_swap_ok_cancel();
}

Error _OS::set_thread_name(StringView p_name) {
    return Thread::set_name(p_name);
}

void _OS::set_use_vsync(bool p_enable) {
    OS::get_singleton()->set_use_vsync(p_enable);
}

bool _OS::is_vsync_enabled() const {
    return OS::get_singleton()->is_vsync_enabled();
}

void _OS::set_vsync_via_compositor(bool p_enable) {
    OS::get_singleton()->set_vsync_via_compositor(p_enable);
}

bool _OS::is_vsync_via_compositor_enabled() const {
    return OS::get_singleton()->is_vsync_via_compositor_enabled();
}

bool _OS::has_feature(StringView p_feature) const {
    return OS::get_singleton()->has_feature(p_feature);
}

/*
enum Weekday {
    DAY_SUNDAY,
    DAY_MONDAY,
    DAY_TUESDAY,
    DAY_WEDNESDAY,
    DAY_THURSDAY,
    DAY_FRIDAY,
    DAY_SATURDAY
};

enum Month {
    MONTH_JANUARY,
    MONTH_FEBRUARY,
    MONTH_MARCH,
    MONTH_APRIL,
    MONTH_MAY,
    MONTH_JUNE,
    MONTH_JULY,
    MONTH_AUGUST,
    MONTH_SEPTEMBER,
    MONTH_OCTOBER,
    MONTH_NOVEMBER,
    MONTH_DECEMBER
};
*/
/*
struct Date {

    int year;
    Month month;
    int day;
    Weekday weekday;
    bool dst;
};

struct Time {

    int hour;
    int min;
    int sec;
};
*/

uint64_t _OS::get_static_memory_usage() const {
    return OS::get_singleton()->get_static_memory_usage();
}

uint64_t _OS::get_static_memory_peak_usage() const {
    return OS::get_singleton()->get_static_memory_peak_usage();
}

void _OS::set_native_icon(const String &p_filename) {
    OS::get_singleton()->set_native_icon(p_filename);
}

void _OS::set_icon(const Ref<Image> &p_icon) {
    OS::get_singleton()->set_icon(p_icon);
}

int _OS::get_exit_code() const {
    return OS::get_singleton()->get_exit_code();
}

void _OS::set_exit_code(int p_code) {
    if (p_code < 0 || p_code > 125) {
        WARN_PRINT("For portability reasons, the exit code should be set between 0 and 125 (inclusive).");
    }
    OS::get_singleton()->set_exit_code(p_code);
}

/**
 *  Get current datetime with consideration for utc and
 *     dst
 */
Dictionary _OS::get_datetime(bool utc) const {
    OS::Date date = OS::get_singleton()->get_date(utc);
    OS::Time time = OS::get_singleton()->get_time(utc);
    Dictionary res;
    res[YEAR_KEY] = date.year;
    res[MONTH_KEY] = date.month;
    res[DAY_KEY] = date.day;
    res[WEEKDAY_KEY] = date.weekday;
    res[DST_KEY] = Variant(date.dst);
    res[HOUR_KEY] = time.hour;
    res[MINUTE_KEY] = time.min;
    res[SECOND_KEY] = time.sec;
    return res;
}

Dictionary _OS::get_date(bool utc) const {
    OS::Date date = OS::get_singleton()->get_date(utc);
    Dictionary dated;
    dated[YEAR_KEY] = date.year;
    dated[MONTH_KEY] = date.month;
    dated[DAY_KEY] = date.day;
    dated[WEEKDAY_KEY] = date.weekday;
    dated[DST_KEY] = date.dst;
    return dated;
}

Dictionary _OS::get_time(bool utc) const {
    OS::Time time = OS::get_singleton()->get_time(utc);
    Dictionary timed;
    timed[HOUR_KEY] = time.hour;
    timed[MINUTE_KEY] = time.min;
    timed[SECOND_KEY] = time.sec;
    return timed;
}

/**
 *  Get an epoch time value from a dictionary of time values
 *  @p datetime must be populated with the following keys:
 *    day, hour, minute, month, second, year. (dst is ignored).
 *
 *    You can pass the output from
 *   get_datetime_from_unix_time directly into this function
 *
 * @param datetime dictionary of date and time values to convert
 *
 * @return epoch calculated
 */
int64_t _OS::get_unix_time_from_datetime(Dictionary datetime) const {
    // if datetime is an empty Dictionary throws an error
    ERR_FAIL_COND_V_MSG(datetime.empty(), 0, "Invalid datetime Dictionary: Dictionary is empty");
    // Bunch of conversion constants
    static const unsigned int SECONDS_PER_MINUTE = 60;
    static const unsigned int MINUTES_PER_HOUR = 60;
    static const unsigned int HOURS_PER_DAY = 24;
    static const unsigned int SECONDS_PER_HOUR = MINUTES_PER_HOUR * SECONDS_PER_MINUTE;
    static const unsigned int SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY;

    // Get all time values from the dictionary, set to zero if it doesn't exist.
    //   Risk incorrect calculation over throwing errors
    unsigned int second = ((datetime.has(SECOND_KEY)) ? datetime[SECOND_KEY].as<uint32_t>() : 0);
    unsigned int minute = ((datetime.has(MINUTE_KEY)) ? datetime[MINUTE_KEY].as<uint32_t>() : 0);
    unsigned int hour = ((datetime.has(HOUR_KEY)) ? datetime[HOUR_KEY].as<uint32_t>() : 0);
    unsigned int day = ((datetime.has(DAY_KEY)) ? datetime[DAY_KEY].as<uint32_t>() : 1);
    unsigned int month = ((datetime.has(MONTH_KEY)) ? datetime[MONTH_KEY].as<uint32_t>() : 1);
    unsigned int year = ((datetime.has(YEAR_KEY)) ? datetime[YEAR_KEY].as<uint32_t>() : 1970);

    /// How many days come before each month (0-12)
    static const unsigned short int DAYS_PAST_THIS_YEAR_TABLE[2][13] = { /* Normal years.  */
        { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
        /* Leap years.  */
        { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
    };

    ERR_FAIL_COND_V_MSG(second > 59, 0, "Invalid second value of: " + itos(second) + ".");

    ERR_FAIL_COND_V_MSG(minute > 59, 0, "Invalid minute value of: " + itos(minute) + ".");

    ERR_FAIL_COND_V_MSG(hour > 23, 0, "Invalid hour value of: " + itos(hour) + ".");

    ERR_FAIL_COND_V_MSG(year == 0, 0, "Years before 1 AD are not supported. Value passed: " + itos(year) + ".");
    ERR_FAIL_COND_V_MSG(month > 12 || month == 0, 0, "Invalid month value of: " + itos(month) + ".");

    // Do this check after month is tested as valid
    unsigned int days_in_month = MONTH_DAYS_TABLE[LEAPYEAR(year)][month - 1];
    ERR_FAIL_COND_V_MSG(day == 0 || day > days_in_month, 0, "Invalid day value of: " + itos(day) + ". It should be comprised between 1 and " + itos(days_in_month) + " for month " + itos(month) + ".");
    // Calculate all the seconds from months past in this year
    uint64_t SECONDS_FROM_MONTHS_PAST_THIS_YEAR =
            DAYS_PAST_THIS_YEAR_TABLE[LEAPYEAR(year)][month - 1] * SECONDS_PER_DAY;

    int64_t SECONDS_FROM_YEARS_PAST = 0;
    if (year >= EPOCH_YR) {
        for (unsigned int iyear = EPOCH_YR; iyear < year; iyear++) {
            SECONDS_FROM_YEARS_PAST += YEARSIZE(iyear) * SECONDS_PER_DAY;
        }
    } else {
        for (unsigned int iyear = EPOCH_YR - 1; iyear >= year; iyear--) {
            SECONDS_FROM_YEARS_PAST -= YEARSIZE(iyear) * SECONDS_PER_DAY;
        }
    }

    int64_t epoch = second + minute * SECONDS_PER_MINUTE + hour * SECONDS_PER_HOUR +
                    // Subtract 1 from day, since the current day isn't over yet
                    //   and we cannot count all 24 hours.
                    (day - 1) * SECONDS_PER_DAY + SECONDS_FROM_MONTHS_PAST_THIS_YEAR + SECONDS_FROM_YEARS_PAST;
    return epoch;
}

/**
 *  Get a dictionary of time values when given epoch time
 *
 *  Dictionary Time values will be a union if values from #get_time
 *    and #get_date dictionaries (with the exception of dst =
 *    day light standard time, as it cannot be determined from epoch)
 *
 * @param unix_time_val epoch time to convert
 *
 * @return dictionary of date and time values
 */
Dictionary _OS::get_datetime_from_unix_time(int64_t unix_time_val) const {
    OS::Date date;
    OS::Time time;

    long dayclock, dayno;
    int year = EPOCH_YR;

    if (unix_time_val >= 0) {
        dayno = unix_time_val / SECS_DAY;
        dayclock = unix_time_val % SECS_DAY;
        /* day 0 was a thursday */
        date.weekday = static_cast<OS::Weekday>((dayno + 4) % 7);
        while (dayno >= YEARSIZE(year)) {
            dayno -= YEARSIZE(year);
            year++;
        }
    } else {
        dayno = (unix_time_val - SECS_DAY + 1) / SECS_DAY;
        dayclock = unix_time_val - dayno * SECS_DAY;
        date.weekday = static_cast<OS::Weekday>(((dayno % 7) + 11) % 7);
        do {
            year--;
            dayno += YEARSIZE(year);
        } while (dayno < 0);
    }

    time.sec = dayclock % 60;
    time.min = (dayclock % 3600) / 60;
    time.hour = dayclock / 3600;
    date.year = year;

    size_t imonth = 0;

    while ((unsigned long)dayno >= MONTH_DAYS_TABLE[LEAPYEAR(year)][imonth]) {
        dayno -= MONTH_DAYS_TABLE[LEAPYEAR(year)][imonth];
        imonth++;
    }

    /// Add 1 to month to make sure months are indexed starting at 1
    date.month = static_cast<OS::Month>(imonth + 1);

    date.day = dayno + 1;

    Dictionary timed;
    timed[HOUR_KEY] = time.hour;
    timed[MINUTE_KEY] = time.min;
    timed[SECOND_KEY] = time.sec;
    timed[YEAR_KEY] = date.year;
    timed[MONTH_KEY] = date.month;
    timed[DAY_KEY] = date.day;
    timed[WEEKDAY_KEY] = date.weekday;

    return timed;
}

Dictionary _OS::get_time_zone_info() const {
    OS::TimeZoneInfo info = OS::get_singleton()->get_time_zone_info();
    Dictionary infod;
    infod["bias"] = info.bias;
    infod["name"] = Variant(info.name);
    return infod;
}

uint64_t _OS::get_unix_time() const {
    return OS::get_singleton()->get_unix_time();
}

uint64_t _OS::get_system_time_secs() const {
    return OS::get_singleton()->get_system_time_secs();
}

uint64_t _OS::get_system_time_msecs() const {
    return OS::get_singleton()->get_system_time_msecs();
}
/** This method uses a signed argument for better error reporting as it's used from the scripting API. */
void _OS::delay_usec(uint32_t p_usec) const {
    ERR_FAIL_COND_MSG(
            p_usec < 0,
            FormatVE("Can't sleep for %d microseconds. The delay provided must be greater than or equal to 0 microseconds.", p_usec));

    OS::get_singleton()->delay_usec(p_usec);
}
/** This method uses a signed argument for better error reporting as it's used from the scripting API. */
void _OS::delay_msec(uint32_t p_msec) const {
    ERR_FAIL_COND_MSG(
            p_msec < 0,
            FormatVE("Can't sleep for %d milliseconds. The delay provided must be greater than or equal to 0 milliseconds.", p_msec));

    OS::get_singleton()->delay_usec(int64_t(p_msec) * 1000);
}

uint64_t _OS::get_ticks_msec() const {
    return OS::get_singleton()->get_ticks_msec();
}

uint64_t _OS::get_ticks_usec() const {
    return OS::get_singleton()->get_ticks_usec();
}

uint32_t _OS::get_splash_tick_msec() const {
    return OS::get_singleton()->get_splash_tick_msec();
}

bool _OS::can_use_threads() const {
    return OS::get_singleton()->can_use_threads();
}

bool _OS::can_draw() const {
    return OS::get_singleton()->can_draw();
}

bool _OS::is_userfs_persistent() const {
    return OS::get_singleton()->is_userfs_persistent();
}

int _OS::get_processor_count() const {
    return OS::get_singleton()->get_processor_count();
}

bool _OS::is_stdout_verbose() const {
    return OS::get_singleton()->is_stdout_verbose();
}

void _OS::dump_memory_to_file(const String &p_file) {
    OS::get_singleton()->dump_memory_to_file(p_file.c_str());
}

struct _OSCoreBindImg {
    String path;
    Size2 size;
    int fmt;
    GameEntity id;
    int vram;
    bool operator<(const _OSCoreBindImg &p_img) const {
        return vram == p_img.vram ? entt::to_integral(id) < entt::to_integral(p_img.id) : vram > p_img.vram;
    }
};

void _OS::print_all_textures_by_size() {
    Vector<_OSCoreBindImg> imgs;
    uint64_t total = 0;

    Vector<Ref<Resource>> rsrc;
    ResourceCache::get_cached_resources(rsrc);
    imgs.reserve(rsrc.size());

    for (const Ref<Resource> &E : rsrc) {
        if (!E->is_class("ImageTexture")) {
            continue;
        }

        Size2 size = E->call_va("get_size").as<Vector2>();
        int fmt = E->call_va("get_format").as<int>();

        _OSCoreBindImg img;
        img.size = size;
        img.fmt = fmt;
        img.path = E->get_path();
        img.vram = Image::get_image_data_size(img.size.width, img.size.height, Image::Format(img.fmt));
        img.id = E->get_instance_id();
        total += img.vram;
        imgs.push_back(img);
    }
    eastl::sort(imgs.begin(),imgs.end());
    if (imgs.empty()) {
        print_line("No textures seem used in this project.");
    } else {
        print_line("Textures currently in use, sorted by VRAM usage:\n"
                   "Path - VRAM usage (Dimensions)");
    }

    for(const _OSCoreBindImg &E : imgs) {
        print_line(FormatVE("%s - %s %s",
                E.path.c_str(),
                PathUtils::humanize_size(E.vram).c_str(),
                ((String)E.size).c_str()));
    }

    print_line(FormatVE("Total VRAM usage: %s.", PathUtils::humanize_size(total).c_str()));
}

void _OS::print_resources_by_type(const Vector<String> &p_types) {
    ERR_FAIL_COND_MSG(p_types.empty(), "At least one type should be provided to print resources by type.");

    print_line(FormatVE("Resources currently in use for the following types: %s", String::joined(p_types,",").c_str()));

    HashMap<String, int> type_count;
    Vector<Ref<Resource>> rsrc;
    ResourceCache::get_cached_resources(rsrc);

    for (const Ref<Resource> &r : rsrc) {
        bool found = false;

        for (const String &name : p_types) {
            if (r->is_class(name.data())) {
                found = true;
            }
        }
        if (!found) {
            continue;
        }

        if (!type_count.contains(r->get_class())) {
            type_count[r->get_class()] = 0;
        }

        type_count[r->get_class()]++;
        print_line(FormatVE("%s: %s", r->get_class(), r->get_path().c_str()));

        List<String> metas;
        r->get_meta_list(&metas);
        for (const String & F : metas) {
            print_line(FormatVE("  %s: %s", F.c_str(), r->get_meta(F).as<String>().c_str()));
    }
}

    for (const auto & E : type_count) {
        print_line(FormatVE("%s count: %d", E.first.c_str(), E.second));
    }
}

void _OS::print_all_resources(StringView p_to_file) {
    OS::get_singleton()->print_all_resources(p_to_file);
}

void _OS::print_resources_in_use(bool p_short) {
    OS::get_singleton()->print_resources_in_use(p_short);
}

void _OS::dump_resources_to_file(StringView p_file) {
    OS::get_singleton()->dump_resources_to_file(p_file);
}

String _OS::get_user_data_dir() const {
    return OS::get_singleton()->get_user_data_dir();
}

Error _OS::native_video_play(StringView p_path, float p_volume, StringView p_audio_track, StringView p_subtitle_track) {
    return OS::get_singleton()->native_video_play(p_path, p_volume, p_audio_track, p_subtitle_track);
}

bool _OS::native_video_is_playing() {
    return OS::get_singleton()->native_video_is_playing();
}

void _OS::native_video_pause() {
    OS::get_singleton()->native_video_pause();
}

void _OS::native_video_unpause() {
    OS::get_singleton()->native_video_unpause();
}

void _OS::native_video_stop() {
    OS::get_singleton()->native_video_stop();
}

void _OS::request_attention() {
    OS::get_singleton()->request_attention();
}

void _OS::center_window() {
    OS::get_singleton()->center_window();
}

void _OS::move_window_to_foreground() {
    OS::get_singleton()->move_window_to_foreground();
}

int64_t _OS::get_native_handle(HandleType p_handle_type) {
    return (int64_t)OS::get_singleton()->get_native_handle(p_handle_type);
}

String _OS::get_config_dir() const {
    // Exposed as `get_config_dir()` instead of `get_config_path()` for consistency with other exposed OS methods.
    return OS::get_singleton()->get_config_path();
}

String _OS::get_data_dir() const {
    // Exposed as `get_data_dir()` instead of `get_data_path()` for consistency with other exposed OS methods.
    return OS::get_singleton()->get_data_path();
}

String _OS::get_cache_dir() const {
    // Exposed as `get_cache_dir()` instead of `get_cache_path()` for consistency with other exposed OS methods.
    return OS::get_singleton()->get_cache_path();
}
bool _OS::is_debug_build() const {
#ifdef DEBUG_ENABLED
    return true;
#else
    return false;
#endif
}

void _OS::set_screen_orientation(ScreenOrientation p_orientation) {
    OS::get_singleton()->set_screen_orientation(OS::ScreenOrientation(p_orientation));
}

_OS::ScreenOrientation _OS::get_screen_orientation() const {
    return ScreenOrientation(OS::get_singleton()->get_screen_orientation());
}

void _OS::set_keep_screen_on(bool p_enabled) {
    OS::get_singleton()->set_keep_screen_on(p_enabled);
}

bool _OS::is_keep_screen_on() const {
    return OS::get_singleton()->is_keep_screen_on();
}

String _OS::get_system_dir(SystemDir p_dir) const {
    return OS::get_system_dir(OS::SystemDir(p_dir));
}

String _OS::get_keycode_string(uint32_t p_code) const {
    return keycode_get_string(p_code);
}
bool _OS::is_keycode_unicode(uint32_t p_unicode) const {
    return keycode_has_unicode(p_unicode);
}
int _OS::find_keycode_from_string(StringView p_code) const {
    return find_keycode(p_code);
}

void _OS::alert(StringView p_alert, StringView p_title) {
    OS::get_singleton()->alert(p_alert, p_title);
}

void _OS::crash(const String &p_message) {
    CRASH_NOW_MSG(p_message);
}

bool _OS::request_permission(StringView p_name) {
    return OS::get_singleton()->request_permission(p_name);
}
bool _OS::request_permissions() {
    return OS::get_singleton()->request_permissions();
}

PoolVector<String> _OS::get_granted_permissions() const {
    return OS::get_singleton()->get_granted_permissions();
}

IMPL_GDCLASS(_OS)

_OS *_OS::singleton = nullptr;

void _OS::_bind_methods() {
    // BIND_METHOD(_OS,get_mouse_position);
    // BIND_METHOD(_OS,is_mouse_grab_enabled);

    SE_BIND_METHOD(_OS,set_clipboard);
    SE_BIND_METHOD(_OS,get_clipboard);
    SE_BIND_METHOD(_OS,has_clipboard);

    // will not delete for now, just unexpose
    // BIND_METHOD_DEFAULTS(_OS, set_video_mode,DEFVAL(0));
    // MethodBinder::bind_method(D_METHOD("get_video_mode_size","screen"),&_OS::get_video_mode,{DEFVAL(0)});
    // BIND_METHOD_DEFAULTS(_OS, is_video_mode_fullscreen,DEFVAL(0));
    // BIND_METHOD_DEFAULTS(_OS, is_video_mode_resizable,DEFVAL(0));
    // BIND_METHOD_DEFAULTS(_OS, get_fullscreen_mode_list,DEFVAL(0));
    MethodBinder::bind_method(
            D_METHOD("global_menu_add_item", { "menu", "label", "id", "meta" }), &_OS::global_menu_add_item);
    SE_BIND_METHOD(_OS,global_menu_add_separator);
    SE_BIND_METHOD(_OS,global_menu_remove_item);
    SE_BIND_METHOD(_OS,global_menu_clear);

    SE_BIND_METHOD(_OS,get_video_driver_count);
    SE_BIND_METHOD(_OS,get_video_driver_name);
    SE_BIND_METHOD(_OS,get_current_video_driver);

    SE_BIND_METHOD(_OS,get_audio_driver_count);
    SE_BIND_METHOD(_OS,get_audio_driver_name);
    SE_BIND_METHOD(_OS,get_connected_midi_inputs);
    SE_BIND_METHOD(_OS,open_midi_inputs);
    SE_BIND_METHOD(_OS,close_midi_inputs);

    SE_BIND_METHOD(_OS,get_screen_count);
    SE_BIND_METHOD(_OS,get_current_screen);
    SE_BIND_METHOD(_OS,set_current_screen);
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_screen_position, DEFVAL(-1) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_screen_size, DEFVAL(-1) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_screen_dpi, DEFVAL(-1) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_screen_scale, DEFVAL(-1) );
    SE_BIND_METHOD(_OS,get_screen_max_scale);

    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_screen_refresh_rate,DEFVAL(-1));
    SE_BIND_METHOD(_OS,get_window_position);
    SE_BIND_METHOD(_OS,set_window_position);
    SE_BIND_METHOD(_OS,get_window_size);
    SE_BIND_METHOD(_OS,get_max_window_size);
    SE_BIND_METHOD(_OS,get_min_window_size);
    SE_BIND_METHOD(_OS,set_max_window_size);
    SE_BIND_METHOD(_OS,set_min_window_size);
    SE_BIND_METHOD(_OS,set_window_size);
    SE_BIND_METHOD(_OS,get_window_safe_area);
    SE_BIND_METHOD(_OS,set_window_fullscreen);
    SE_BIND_METHOD(_OS,is_window_fullscreen);
    SE_BIND_METHOD(_OS,set_window_resizable);
    SE_BIND_METHOD(_OS,is_window_resizable);
    SE_BIND_METHOD(_OS,set_window_minimized);
    SE_BIND_METHOD(_OS,is_window_minimized);
    SE_BIND_METHOD(_OS,set_window_maximized);
    SE_BIND_METHOD(_OS,is_window_maximized);
    SE_BIND_METHOD(_OS,set_window_always_on_top);
    SE_BIND_METHOD(_OS,is_window_always_on_top);
    SE_BIND_METHOD(_OS,is_window_focused);
    SE_BIND_METHOD(_OS,request_attention);
    SE_BIND_METHOD(_OS,get_real_window_size);
    SE_BIND_METHOD(_OS,center_window);
    SE_BIND_METHOD(_OS,move_window_to_foreground);

    SE_BIND_METHOD(_OS,get_native_handle);

    SE_BIND_METHOD(_OS,set_borderless_window);
    SE_BIND_METHOD(_OS,get_borderless_window);

    MethodBinder::bind_method(
            D_METHOD("get_window_per_pixel_transparency_enabled"), &_OS::get_window_per_pixel_transparency_enabled);
    MethodBinder::bind_method(D_METHOD("set_window_per_pixel_transparency_enabled", { "enabled" }),
            &_OS::set_window_per_pixel_transparency_enabled);

    SE_BIND_METHOD(_OS,set_ime_active);
    SE_BIND_METHOD(_OS,set_ime_position);
    SE_BIND_METHOD(_OS,get_ime_selection);
    SE_BIND_METHOD(_OS,get_ime_text);

    SE_BIND_METHOD(_OS,set_screen_orientation);
    SE_BIND_METHOD(_OS,get_screen_orientation);

    SE_BIND_METHOD(_OS,set_keep_screen_on);
    SE_BIND_METHOD(_OS,is_keep_screen_on);

    SE_BIND_METHOD(_OS,has_touchscreen_ui_hint);

    SE_BIND_METHOD(_OS,set_window_title);
    SE_BIND_METHOD(_OS,set_window_mouse_passthrough);

    MethodBinder::bind_method(
            D_METHOD("set_low_processor_usage_mode", { "enable" }), &_OS::set_low_processor_usage_mode);
    SE_BIND_METHOD(_OS,is_in_low_processor_usage_mode);

    MethodBinder::bind_method(D_METHOD("set_low_processor_usage_mode_sleep_usec", { "usec" }),
            &_OS::set_low_processor_usage_mode_sleep_usec);
    MethodBinder::bind_method(
            D_METHOD("get_low_processor_usage_mode_sleep_usec"), &_OS::get_low_processor_usage_mode_sleep_usec);

    SE_BIND_METHOD(_OS,get_processor_count);

    SE_BIND_METHOD(_OS,get_executable_path);
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, execute, DEFVAL(true), DEFVAL(Array()), DEFVAL(false) , DEFVAL(false));
    SE_BIND_METHOD(_OS,kill);
    SE_BIND_METHOD(_OS,shell_open);
    SE_BIND_METHOD(_OS,get_process_id);

    SE_BIND_METHOD(_OS,get_environment);
    SE_BIND_METHOD(_OS,set_environment);
    SE_BIND_METHOD(_OS,has_environment);

    SE_BIND_METHOD(_OS,get_name);
    SE_BIND_METHOD(_OS,get_cmdline_args);

    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_datetime, DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_date, DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, get_time, DEFVAL(false) );
    SE_BIND_METHOD(_OS,get_time_zone_info);
    SE_BIND_METHOD(_OS,get_unix_time);
    SE_BIND_METHOD(_OS, get_datetime_from_unix_time);
    SE_BIND_METHOD(_OS, get_unix_time_from_datetime);
    SE_BIND_METHOD(_OS,get_system_time_secs);
    SE_BIND_METHOD(_OS,get_system_time_msecs);

    SE_BIND_METHOD(_OS,set_native_icon);
    SE_BIND_METHOD(_OS,set_icon);

    SE_BIND_METHOD(_OS,get_exit_code);
    SE_BIND_METHOD(_OS,set_exit_code);

    SE_BIND_METHOD(_OS,delay_usec);
    SE_BIND_METHOD(_OS,delay_msec);
    SE_BIND_METHOD(_OS,get_ticks_msec);
    SE_BIND_METHOD(_OS,get_ticks_usec);
    SE_BIND_METHOD(_OS,get_splash_tick_msec);
    SE_BIND_METHOD(_OS,get_locale);
    SE_BIND_METHOD(_OS,get_latin_keyboard_variant);
    SE_BIND_METHOD(_OS,get_model_name);

    SE_BIND_METHOD(_OS,can_draw);
    SE_BIND_METHOD(_OS,is_userfs_persistent);
    SE_BIND_METHOD(_OS,is_stdout_verbose);

    SE_BIND_METHOD(_OS,can_use_threads);

    SE_BIND_METHOD(_OS,is_debug_build);

    // BIND_METHOD(_OS,get_mouse_button_state);

    SE_BIND_METHOD(_OS,dump_memory_to_file);
    SE_BIND_METHOD(_OS,dump_resources_to_file);
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, print_resources_in_use, DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(_OS, print_all_resources, DEFVAL(String()) );

    SE_BIND_METHOD(_OS,get_static_memory_usage);
    SE_BIND_METHOD(_OS,get_static_memory_peak_usage);

    SE_BIND_METHOD(_OS,get_user_data_dir);
    SE_BIND_METHOD(_OS,get_system_dir);
    SE_BIND_METHOD(_OS,get_config_dir);
    SE_BIND_METHOD(_OS,get_data_dir);
    SE_BIND_METHOD(_OS,get_cache_dir);
    SE_BIND_METHOD(_OS,get_unique_id);

    SE_BIND_METHOD(_OS,is_ok_left_and_cancel_right);

    SE_BIND_METHOD(_OS,print_all_textures_by_size);
    SE_BIND_METHOD(_OS,print_resources_by_type);

    MethodBinder::bind_method(D_METHOD("native_video_play", { "path", "volume", "audio_track", "subtitle_track" }),
            &_OS::native_video_play);
    SE_BIND_METHOD(_OS,native_video_is_playing);
    SE_BIND_METHOD(_OS,native_video_stop);
    SE_BIND_METHOD(_OS,native_video_pause);
    SE_BIND_METHOD(_OS,native_video_unpause);

    SE_BIND_METHOD(_OS,get_keycode_string);
    SE_BIND_METHOD(_OS,is_keycode_unicode);
    SE_BIND_METHOD(_OS,find_keycode_from_string);

    MethodBinder::bind_method(
            D_METHOD("set_use_file_access_save_and_swap", { "enabled" }), &_OS::set_use_file_access_save_and_swap);

    SE_BIND_METHOD_WITH_DEFAULTS(_OS, alert, DEFVAL("Alert!") );
    SE_BIND_METHOD(_OS,crash);

    SE_BIND_METHOD(_OS,set_thread_name);

    SE_BIND_METHOD(_OS,set_use_vsync);
    SE_BIND_METHOD(_OS,is_vsync_enabled);

    SE_BIND_METHOD(_OS,set_vsync_via_compositor);
    SE_BIND_METHOD(_OS,is_vsync_via_compositor_enabled);

    SE_BIND_METHOD(_OS,has_feature);

    SE_BIND_METHOD(_OS,request_permission);
    SE_BIND_METHOD(_OS,request_permissions);
    SE_BIND_METHOD(_OS,get_granted_permissions);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "clipboard"), "set_clipboard", "get_clipboard");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "current_screen"), "set_current_screen", "get_current_screen");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "exit_code"), "set_exit_code", "get_exit_code");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vsync_enabled"), "set_use_vsync", "is_vsync_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "vsync_via_compositor"), "set_vsync_via_compositor",
            "is_vsync_via_compositor_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "low_processor_usage_mode"), "set_low_processor_usage_mode",
            "is_in_low_processor_usage_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "low_processor_usage_mode_sleep_usec"),
            "set_low_processor_usage_mode_sleep_usec", "get_low_processor_usage_mode_sleep_usec");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "keep_screen_on"), "set_keep_screen_on", "is_keep_screen_on");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "min_window_size"), "set_min_window_size", "get_min_window_size");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "max_window_size"), "set_max_window_size", "get_max_window_size");
    ADD_PROPERTY(
            PropertyInfo(VariantType::INT, "screen_orientation", PropertyHint::Enum,
                    "Landscape,Portrait,Reverse Landscape,Reverse Portrait,Sensor Landscape,Sensor Portrait,Sensor"),
            "set_screen_orientation", "get_screen_orientation");
    ADD_GROUP("Window", "window_");
    ADD_PROPERTY(
            PropertyInfo(VariantType::BOOL, "window_borderless"), "set_borderless_window", "get_borderless_window");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "window_per_pixel_transparency_enabled"),
            "set_window_per_pixel_transparency_enabled", "get_window_per_pixel_transparency_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "window_fullscreen"), "set_window_fullscreen", "is_window_fullscreen");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "window_maximized"), "set_window_maximized", "is_window_maximized");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "window_minimized"), "set_window_minimized", "is_window_minimized");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "window_resizable"), "set_window_resizable", "is_window_resizable");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "window_position"), "set_window_position", "get_window_position");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "window_size"), "set_window_size", "get_window_size");

    // Those default values need to be specified for the docs generator,
    // to avoid using values from the documentation writer's own OS instance.
    ADD_PROPERTY_DEFAULT("clipboard", "");
    ADD_PROPERTY_DEFAULT("current_screen", 0);
    ADD_PROPERTY_DEFAULT("exit_code", 0);
    ADD_PROPERTY_DEFAULT("vsync_enabled", true);
    ADD_PROPERTY_DEFAULT("vsync_via_compositor", false);
    ADD_PROPERTY_DEFAULT("low_processor_usage_mode", false);
    ADD_PROPERTY_DEFAULT("low_processor_usage_mode_sleep_usec", 6900);
    ADD_PROPERTY_DEFAULT("keep_screen_on", true);
    ADD_PROPERTY_DEFAULT("min_window_size", Vector2());
    ADD_PROPERTY_DEFAULT("max_window_size", Vector2());
    ADD_PROPERTY_DEFAULT("screen_orientation", 0);
    ADD_PROPERTY_DEFAULT("window_borderless", false);
    ADD_PROPERTY_DEFAULT("window_per_pixel_transparency_enabled", false);
    ADD_PROPERTY_DEFAULT("window_fullscreen", false);
    ADD_PROPERTY_DEFAULT("window_maximized", false);
    ADD_PROPERTY_DEFAULT("window_minimized", false);
    ADD_PROPERTY_DEFAULT("window_resizable", true);
    ADD_PROPERTY_DEFAULT("window_position", Vector2());
    ADD_PROPERTY_DEFAULT("window_size", Vector2());

    BIND_ENUM_CONSTANT(VIDEO_DRIVER_GLES3);
    BIND_ENUM_CONSTANT(VIDEO_DRIVER_VULKAN);

    BIND_ENUM_CONSTANT(DAY_SUNDAY);
    BIND_ENUM_CONSTANT(DAY_MONDAY);
    BIND_ENUM_CONSTANT(DAY_TUESDAY);
    BIND_ENUM_CONSTANT(DAY_WEDNESDAY);
    BIND_ENUM_CONSTANT(DAY_THURSDAY);
    BIND_ENUM_CONSTANT(DAY_FRIDAY);
    BIND_ENUM_CONSTANT(DAY_SATURDAY);

    REGISTER_ENUM(Month, uint8_t);
    BIND_ENUM_CONSTANT(MONTH_JANUARY);
    BIND_ENUM_CONSTANT(MONTH_FEBRUARY);
    BIND_ENUM_CONSTANT(MONTH_MARCH);
    BIND_ENUM_CONSTANT(MONTH_APRIL);
    BIND_ENUM_CONSTANT(MONTH_MAY);
    BIND_ENUM_CONSTANT(MONTH_JUNE);
    BIND_ENUM_CONSTANT(MONTH_JULY);
    BIND_ENUM_CONSTANT(MONTH_AUGUST);
    BIND_ENUM_CONSTANT(MONTH_SEPTEMBER);
    BIND_ENUM_CONSTANT(MONTH_OCTOBER);
    BIND_ENUM_CONSTANT(MONTH_NOVEMBER);
    BIND_ENUM_CONSTANT(MONTH_DECEMBER);

    BIND_ENUM_CONSTANT(APPLICATION_HANDLE);
    BIND_ENUM_CONSTANT(DISPLAY_HANDLE);
    BIND_ENUM_CONSTANT(WINDOW_HANDLE);
    BIND_ENUM_CONSTANT(WINDOW_VIEW);
    BIND_ENUM_CONSTANT(OPENGL_CONTEXT);

    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_LANDSCAPE);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_PORTRAIT);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_REVERSE_PORTRAIT);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_SENSOR_PORTRAIT);
    BIND_ENUM_CONSTANT(SCREEN_ORIENTATION_SENSOR);

    BIND_ENUM_CONSTANT(SYSTEM_DIR_DESKTOP);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_DCIM);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_DOCUMENTS);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_DOWNLOADS);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_MOVIES);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_MUSIC);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_PICTURES);
    BIND_ENUM_CONSTANT(SYSTEM_DIR_RINGTONES);
}

_OS::_OS() {
    singleton = this;
}

///////////////////// GEOMETRY

_Geometry *_Geometry::singleton = nullptr;

_Geometry *_Geometry::get_singleton() {
    return singleton;
}

PoolVector<Plane> _Geometry::build_box_planes(const Vector3 &p_extents) {
    return Geometry::build_box_planes(p_extents);
}

PoolVector<Plane> _Geometry::build_cylinder_planes(float p_radius, float p_height, int p_sides, Vector3::Axis p_axis) {
    return Geometry::build_cylinder_planes(p_radius, p_height, p_sides, p_axis);
}
PoolVector<Plane> _Geometry::build_capsule_planes(
        float p_radius, float p_height, int p_sides, int p_lats, Vector3::Axis p_axis) {
    return Geometry::build_capsule_planes(p_radius, p_height, p_sides, p_lats, p_axis);
}

bool _Geometry::is_point_in_circle(const Vector2 &p_point, const Vector2 &p_circle_pos, real_t p_circle_radius) {
    return Geometry::is_point_in_circle(p_point, p_circle_pos, p_circle_radius);
}

real_t _Geometry::segment_intersects_circle(
        const Vector2 &p_from, const Vector2 &p_to, const Vector2 &p_circle_pos, real_t p_circle_radius) {
    return Geometry::segment_intersects_circle(p_from, p_to, p_circle_pos, p_circle_radius);
}

Variant _Geometry::segment_intersects_segment_2d(
        const Vector2 &p_from_a, const Vector2 &p_to_a, const Vector2 &p_from_b, const Vector2 &p_to_b) {
    Vector2 result;
    if (Geometry::segment_intersects_segment_2d(p_from_a, p_to_a, p_from_b, p_to_b, &result)) {
        return result;
    } else {
        return Variant();
    }
}

Variant _Geometry::line_intersects_line_2d(
        const Vector2 &p_from_a, const Vector2 &p_dir_a, const Vector2 &p_from_b, const Vector2 &p_dir_b) {
    Vector2 result;
    if (Geometry::line_intersects_line_2d(p_from_a, p_dir_a, p_from_b, p_dir_b, result)) {
        return result;
    } else {
        return Variant();
    }
}

PoolVector<Vector2> _Geometry::get_closest_points_between_segments_2d(
        const Vector2 &p1, const Vector2 &q1, const Vector2 &p2, const Vector2 &q2) {
    Vector2 r1, r2;
    Geometry::get_closest_points_between_segments(p1, q1, p2, q2, r1, r2);
    PoolVector<Vector2> r;
    r.resize(2);
    r.set(0, r1);
    r.set(1, r2);
    return r;
}

PoolVector<Vector3> _Geometry::get_closest_points_between_segments(
        const Vector3 &p1, const Vector3 &p2, const Vector3 &q1, const Vector3 &q2) {
    Vector3 r1, r2;
    Geometry::get_closest_points_between_segments(p1, p2, q1, q2, r1, r2);
    PoolVector<Vector3> r;
    r.resize(2);
    r.set(0, r1);
    r.set(1, r2);
    return r;
}
Vector2 _Geometry::get_closest_point_to_segment_2d(const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b) {
    Vector2 s[2] = { p_a, p_b };
    return Geometry::get_closest_point_to_segment_2d(p_point, s);
}
Vector3 _Geometry::get_closest_point_to_segment(const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b) {
    Vector3 s[2] = { p_a, p_b };
    return Geometry::get_closest_point_to_segment(p_point, s);
}
Vector2 _Geometry::get_closest_point_to_segment_uncapped_2d(
        const Vector2 &p_point, const Vector2 &p_a, const Vector2 &p_b) {
    Vector2 s[2] = { p_a, p_b };
    return Geometry::get_closest_point_to_segment_uncapped_2d(p_point, s);
}
Vector3 _Geometry::get_closest_point_to_segment_uncapped(
        const Vector3 &p_point, const Vector3 &p_a, const Vector3 &p_b) {
    Vector3 s[2] = { p_a, p_b };
    return Geometry::get_closest_point_to_segment_uncapped(p_point, s);
}
Variant _Geometry::ray_intersects_triangle(
        const Vector3 &p_from, const Vector3 &p_dir, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
    Vector3 res;
    if (Geometry::ray_intersects_triangle(p_from, p_dir, p_v0, p_v1, p_v2, &res)) {
        return res;
    } else {
        return Variant();
    }
}
Variant _Geometry::segment_intersects_triangle(
        const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2) {
    Vector3 res;
    if (Geometry::segment_intersects_triangle(p_from, p_to, p_v0, p_v1, p_v2, &res)) {
        return res;
    } else {
        return Variant();
    }
}

bool _Geometry::point_is_inside_triangle(const Vector2 &s, const Vector2 &a, const Vector2 &b, const Vector2 &c) const {
    return Geometry::is_point_in_triangle(s, a, b, c);
}

PoolVector<Vector3> _Geometry::segment_intersects_sphere(
        const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_sphere_pos, real_t p_sphere_radius) {
    PoolVector<Vector3> r;
    Vector3 res, norm;
    if (!Geometry::segment_intersects_sphere(p_from, p_to, p_sphere_pos, p_sphere_radius, &res, &norm)) {
        return r;
    }

    r.resize(2);
    r.set(0, res);
    r.set(1, norm);
    return r;
}
PoolVector<Vector3> _Geometry::segment_intersects_cylinder(
        const Vector3 &p_from, const Vector3 &p_to, float p_height, float p_radius) {
    PoolVector<Vector3> r;
    Vector3 res, norm;
    if (!Geometry::segment_intersects_cylinder(p_from, p_to, p_height, p_radius, &res, &norm)) {
        return r;
    }

    r.resize(2);
    r.set(0, res);
    r.set(1, norm);
    return r;
}
PoolVector<Vector3> _Geometry::segment_intersects_convex(
        const Vector3 &p_from, const Vector3 &p_to, const PoolVector<Plane> &p_planes) {
    PoolVector<Vector3> r;
    Vector3 res, norm;
    if (!Geometry::segment_intersects_convex(p_from, p_to, p_planes.read().ptr(), p_planes.size(), &res, &norm)) {
        return r;
    }

    r.resize(2);
    r.set(0, res);
    r.set(1, norm);
    return r;
}

bool _Geometry::is_polygon_clockwise(const Vector<Vector2> &p_polygon) {
    return Geometry::is_polygon_clockwise(p_polygon);
}

bool _Geometry::is_point_in_polygon(const Point2 &p_point, const Vector<Vector2> &p_polygon) {
    return Geometry::is_point_in_polygon(p_point, p_polygon);
}

Vector<int> _Geometry::triangulate_polygon(Span<const Vector2> p_polygon) {
    return Geometry::triangulate_polygon(p_polygon);
}

Vector<int> _Geometry::triangulate_delaunay_2d(Span<const Vector2> p_points) {
    return Geometry::triangulate_delaunay_2d(p_points);
}

Vector<Point2> _Geometry::convex_hull_2d(Span<const Point2> p_points) {
    return Geometry::convex_hull_2d(p_points);
}

Vector<Vector3> _Geometry::clip_polygon(Span<const Vector3> p_points, const Plane &p_plane) {
    return Geometry::clip_polygon(p_points, p_plane);
}

Array _Geometry::merge_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
    Vector<Vector<Point2>> polys(Geometry::merge_polygons_2d(p_polygon_a, p_polygon_b));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::clip_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
    Vector<Vector<Point2>> polys(Geometry::clip_polygons_2d(p_polygon_a, p_polygon_b));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::intersect_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
    Vector<Vector<Point2>> polys(Geometry::intersect_polygons_2d(p_polygon_a, p_polygon_b));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::exclude_polygons_2d(const Vector<Vector2> &p_polygon_a, const Vector<Vector2> &p_polygon_b) {
    Vector<Vector<Point2>> polys(Geometry::exclude_polygons_2d(p_polygon_a, p_polygon_b));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::clip_polyline_with_polygon_2d(const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
    Vector<Vector<Point2>> polys(Geometry::clip_polyline_with_polygon_2d(p_polyline, p_polygon));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::intersect_polyline_with_polygon_2d(
        const Vector<Vector2> &p_polyline, const Vector<Vector2> &p_polygon) {
    Vector<Vector<Point2>> polys(Geometry::intersect_polyline_with_polygon_2d(p_polyline, p_polygon));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::offset_polygon_2d(const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type) {
    Vector<Vector<Point2>> polys(Geometry::offset_polygon_2d(p_polygon, p_delta, Geometry::PolyJoinType(p_join_type)));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Array _Geometry::offset_polyline_2d(
        const Vector<Vector2> &p_polygon, real_t p_delta, PolyJoinType p_join_type, PolyEndType p_end_type) {
    Vector<Vector<Point2>> polys(Geometry::offset_polyline_2d(
            p_polygon, p_delta, Geometry::PolyJoinType(p_join_type), Geometry::PolyEndType(p_end_type)));

    Array ret;
    ret.reserve(polys.size());
    for (Vector<Point2> &poly : polys) {
        ret.emplace_back(eastl::move(poly));
    }
    return ret;
}

Dictionary _Geometry::make_atlas(const Vector<Size2> &p_rects) {
    Dictionary ret;

    Vector<Size2i> rects;
    for (Size2 rect : p_rects) {
        rects.emplace_back(rect);
    }

    Vector<Point2i> result;
    Size2i size;

    Geometry::make_atlas(rects, result, size);

    Size2 r_size = size;

    Vector<Point2> r_result;
    r_result.reserve(result.size());

    for (Point2i v : result) {
        r_result.emplace_back(v);
    }

    ret["points"] = r_result;
    ret["size"] = r_size;

    return ret;
}

IMPL_GDCLASS(_Geometry)

void _Geometry::_bind_methods() {
    SE_BIND_METHOD(_Geometry,build_box_planes);
    SE_BIND_METHOD_WITH_DEFAULTS(_Geometry, build_cylinder_planes, DEFVAL(Vector3::AXIS_Z) );
    SE_BIND_METHOD_WITH_DEFAULTS(_Geometry, build_capsule_planes, DEFVAL(Vector3::AXIS_Z) );
    SE_BIND_METHOD(_Geometry,segment_intersects_circle);
    MethodBinder::bind_method(D_METHOD("segment_intersects_segment_2d", { "from_a", "to_a", "from_b", "to_b" }),
            &_Geometry::segment_intersects_segment_2d);
    MethodBinder::bind_method(D_METHOD("line_intersects_line_2d", { "from_a", "dir_a", "from_b", "dir_b" }),
            &_Geometry::line_intersects_line_2d);

    MethodBinder::bind_method(D_METHOD("get_closest_points_between_segments_2d", { "p1", "q1", "p2", "q2" }),
            &_Geometry::get_closest_points_between_segments_2d);
    MethodBinder::bind_method(D_METHOD("get_closest_points_between_segments", { "p1", "p2", "q1", "q2" }),
            &_Geometry::get_closest_points_between_segments);

    MethodBinder::bind_method(D_METHOD("get_closest_point_to_segment_2d", { "point", "s1", "s2" }),
            &_Geometry::get_closest_point_to_segment_2d);
    MethodBinder::bind_method(D_METHOD("get_closest_point_to_segment", { "point", "s1", "s2" }),
            &_Geometry::get_closest_point_to_segment);

    MethodBinder::bind_method(D_METHOD("get_closest_point_to_segment_uncapped_2d", { "point", "s1", "s2" }),
            &_Geometry::get_closest_point_to_segment_uncapped_2d);
    MethodBinder::bind_method(D_METHOD("get_closest_point_to_segment_uncapped", { "point", "s1", "s2" }),
            &_Geometry::get_closest_point_to_segment_uncapped);

    MethodBinder::bind_method(
            D_METHOD("ray_intersects_triangle", { "from", "dir", "a", "b", "c" }), &_Geometry::ray_intersects_triangle);
    MethodBinder::bind_method(D_METHOD("segment_intersects_triangle", { "from", "to", "a", "b", "c" }),
            &_Geometry::segment_intersects_triangle);
    MethodBinder::bind_method(
            D_METHOD("segment_intersects_sphere", { "from", "to", "sphere_position", "sphere_radius" }),
            &_Geometry::segment_intersects_sphere);
    MethodBinder::bind_method(D_METHOD("segment_intersects_cylinder", { "from", "to", "height", "radius" }),
            &_Geometry::segment_intersects_cylinder);
    MethodBinder::bind_method(
            D_METHOD("segment_intersects_convex", { "from", "to", "planes" }), &_Geometry::segment_intersects_convex);
    MethodBinder::bind_method(
            D_METHOD("point_is_inside_triangle", { "point", "a", "b", "c" }), &_Geometry::point_is_inside_triangle);

    SE_BIND_METHOD(_Geometry,is_polygon_clockwise);
    SE_BIND_METHOD(_Geometry,is_point_in_polygon);
    SE_BIND_METHOD(_Geometry,triangulate_polygon);
    SE_BIND_METHOD(_Geometry,triangulate_delaunay_2d);
    SE_BIND_METHOD(_Geometry,convex_hull_2d);
    SE_BIND_METHOD(_Geometry,clip_polygon);

    MethodBinder::bind_method(
            D_METHOD("merge_polygons_2d", { "polygon_a", "polygon_b" }), &_Geometry::merge_polygons_2d);
    SE_BIND_METHOD(_Geometry,clip_polygons_2d);
    MethodBinder::bind_method(
            D_METHOD("intersect_polygons_2d", { "polygon_a", "polygon_b" }), &_Geometry::intersect_polygons_2d);
    MethodBinder::bind_method(
            D_METHOD("exclude_polygons_2d", { "polygon_a", "polygon_b" }), &_Geometry::exclude_polygons_2d);

    MethodBinder::bind_method(D_METHOD("clip_polyline_with_polygon_2d", { "polyline", "polygon" }),
            &_Geometry::clip_polyline_with_polygon_2d);
    MethodBinder::bind_method(D_METHOD("intersect_polyline_with_polygon_2d", { "polyline", "polygon" }),
            &_Geometry::intersect_polyline_with_polygon_2d);

    SE_BIND_METHOD_WITH_DEFAULTS(_Geometry, offset_polygon_2d, DEFVAL(JOIN_SQUARE) );
    SE_BIND_METHOD_WITH_DEFAULTS(_Geometry, offset_polyline_2d, DEFVAL(JOIN_SQUARE), DEFVAL(END_SQUARE) );

    SE_BIND_METHOD(_Geometry,make_atlas);

    BIND_ENUM_CONSTANT(OPERATION_UNION);
    BIND_ENUM_CONSTANT(OPERATION_DIFFERENCE);
    BIND_ENUM_CONSTANT(OPERATION_INTERSECTION);
    BIND_ENUM_CONSTANT(OPERATION_XOR);

    BIND_ENUM_CONSTANT(JOIN_SQUARE);
    BIND_ENUM_CONSTANT(JOIN_ROUND);
    BIND_ENUM_CONSTANT(JOIN_MITER);

    BIND_ENUM_CONSTANT(END_POLYGON);
    BIND_ENUM_CONSTANT(END_JOINED);
    BIND_ENUM_CONSTANT(END_BUTT);
    BIND_ENUM_CONSTANT(END_SQUARE);
    BIND_ENUM_CONSTANT(END_ROUND);
}

_Geometry::_Geometry() {
    singleton = this;
}

///////////////////////// FILE

Error _File::open_encrypted(StringView p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key) {
    Error err = open(p_path, p_mode_flags);
    if (err) {
        return err;
    }

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse(f, p_key,
            (p_mode_flags == WRITE) ? FileAccessEncrypted::MODE_WRITE_AES256 : FileAccessEncrypted::MODE_READ);
    if (err) {
        memdelete(fae);
        close();
        return err;
    }
    f = fae;
    return OK;
}

Error _File::open_encrypted_pass(StringView p_path, ModeFlags p_mode_flags, StringView p_pass) {
    Error err = open(p_path, p_mode_flags);
    if (err) {
        return err;
    }

    FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
    err = fae->open_and_parse_password(f, p_pass,
            (p_mode_flags == WRITE) ? FileAccessEncrypted::MODE_WRITE_AES256 : FileAccessEncrypted::MODE_READ);
    if (err) {
        memdelete(fae);
        close();
        return err;
    }

    f = fae;
    return OK;
}

Error _File::open_compressed(StringView p_path, ModeFlags p_mode_flags, CompressionMode p_compress_mode) {
    FileAccessCompressed *fac = memnew(FileAccessCompressed);

    fac->configure("GCPF", (Compression::Mode)p_compress_mode);

    Error err = fac->_open(p_path, p_mode_flags);

    if (err) {
        memdelete(fac);
        return err;
    }

    f = fac;
    return OK;
}

Error _File::open(StringView p_path, ModeFlags p_mode_flags) {
    close();
    Error err;
    f = FileAccess::open(p_path, p_mode_flags, &err);
    if (f) {
        f->set_endian_swap(eswap);
    }
    return err;
}

void _File::flush() {
    ERR_FAIL_COND_MSG(!f, "File must be opened before flushing.");
    f->flush();
}

void _File::close() {
    memdelete(f);
    f = nullptr;
}

bool _File::is_open() const {
    return f != nullptr;
}

const String &_File::get_path() const {
    ERR_FAIL_COND_V_MSG(!f, null_string, "File must be opened before use.");
    return f->get_path();
}

const String &_File::get_path_absolute() const {
    ERR_FAIL_COND_V_MSG(!f, null_string, "File must be opened before use.");
    return f->get_path_absolute();
}

void _File::seek(int64_t p_position) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");
    ERR_FAIL_COND_MSG(p_position < 0, "Seek position must be a positive integer.");
    f->seek(p_position);
}
void _File::seek_end(int64_t p_position) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");
    f->seek_end(p_position);
}
uint64_t _File::get_position() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_position();
}

uint64_t _File::get_len() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_len();
}

bool _File::eof_reached() const {
    ERR_FAIL_COND_V_MSG(!f, false, "File must be opened before use.");
    return f->eof_reached();
}

uint8_t _File::get_8() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_8();
}
uint16_t _File::get_16() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_16();
}
uint32_t _File::get_32() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_32();
}
uint64_t _File::get_64() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_64();
}

float _File::get_float() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    MarshallFloat mf;
    mf.i = f->get_32();
    return mf.f;
}
double _File::get_double() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_double();
}
real_t _File::get_real() const {
    ERR_FAIL_COND_V_MSG(!f, 0, "File must be opened before use.");
    return f->get_real();
}

PoolVector<uint8_t> _File::get_buffer(int64_t p_length) const {
    PoolVector<uint8_t> data;
    ERR_FAIL_COND_V_MSG(!f, data, "File must be opened before use.");

    ERR_FAIL_COND_V_MSG(p_length < 0, data, "Length of buffer cannot be smaller than 0.");
    if (p_length == 0) {
        return data;
    }

    Error err = data.resize(p_length);
    ERR_FAIL_COND_V_MSG(err != OK, data, "Can't resize data to " + itos(p_length) + " elements.");

    PoolVector<uint8_t>::Write w = data.write();
    int64_t len = f->get_buffer(&w[0], p_length);
    ERR_FAIL_COND_V(len < 0, PoolVector<uint8_t>());

    w.release();

    if (len < p_length) {
        data.resize(p_length);
    }

    return data;
}

String _File::get_as_text() const {
    ERR_FAIL_COND_V_MSG(!f, String(), "File must be opened before use.");

    String text;
    uint64_t original_pos = f->get_position();
    f->seek(0);

    String l(get_line());
    while (!eof_reached()) {
        text += l + "\n";
        l = get_line();
    }
    text += l;

    f->seek(original_pos);

    return text;
}

String _File::get_md5(StringView p_path) const {
    return FileAccess::get_md5(p_path);
}

String _File::get_sha256(StringView p_path) const {
    return FileAccess::get_sha256(p_path);
}

String _File::get_line() const {
    ERR_FAIL_COND_V_MSG(!f, String(), "File must be opened before use.");
    return f->get_line();
}

Vector<String> _File::get_csv_line(int8_t p_delim) const {
    ERR_FAIL_COND_V_MSG(!f, {}, "File must be opened before use.");
    return f->get_csv_line(p_delim);
}

/**< use this for files WRITTEN in _big_ endian machines (ie, amiga/mac)
 * It's not about the current CPU type but file formats.
 * this flags get reset to false (little endian) on each open
 */

void _File::set_endian_swap(bool p_swap) {
    eswap = p_swap;
    if (f) {
        f->set_endian_swap(p_swap);
    }
}
bool _File::get_endian_swap() {
    return eswap;
}

Error _File::get_error() const {
    if (!f) {
        return ERR_UNCONFIGURED;
    }
    return f->get_error();
}

void _File::store_8(uint8_t p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_8(p_dest);
}
void _File::store_16(uint16_t p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_16(p_dest);
}
void _File::store_32(uint32_t p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_32(p_dest);
}
void _File::store_64(uint64_t p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_64(p_dest);
}

void _File::store_float(float p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_float(p_dest);
}
void _File::store_double(double p_dest) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_double(p_dest);
}
void _File::store_real(real_t p_real) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_real(p_real);
}

void _File::store_string(StringView p_string) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_string(p_string);
}

void _File::store_pascal_string(StringView p_string) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    f->store_pascal_string(p_string);
}

String _File::get_pascal_string() {
    ERR_FAIL_COND_V_MSG(!f, String(), "File must be opened before use.");

    return f->get_pascal_string();
}

void _File::store_line(StringView p_string) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");
    f->store_line(p_string);
}

void _File::store_csv_line(const PoolVector<String> &p_values, int8_t p_delim) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");
    auto rd = p_values.read();
    Vector<String> vals(rd.ptr(), rd.ptr() + p_values.size());
    f->store_csv_line(vals, p_delim);
}

void _File::store_buffer(const PoolVector<uint8_t> &p_buffer) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");

    uint64_t len = p_buffer.size();
    if (len == 0) {
        return;
    }

    PoolVector<uint8_t>::Read r = p_buffer.read();

    f->store_buffer(&r[0], len);
}

bool _File::file_exists(StringView p_name) const {
    return FileAccess::exists(p_name);
}

void _File::store_var(const Variant &p_var, bool p_full_objects) {
    ERR_FAIL_COND_MSG(!f, "File must be opened before use.");
    int len;
    Error err = encode_variant(p_var, nullptr, len, p_full_objects);
    ERR_FAIL_COND_MSG(err != OK, "Error when trying to encode Variant.");

    PoolVector<uint8_t> buff;
    buff.resize(len);

    PoolVector<uint8_t>::Write w = buff.write();
    err = encode_variant(p_var, &w[0], len, p_full_objects);
    ERR_FAIL_COND_MSG(err != OK, "Error when trying to encode Variant.");
    w.release();

    store_32(len);
    store_buffer(buff);
}

Variant _File::get_var(bool p_allow_objects) const {
    ERR_FAIL_COND_V_MSG(!f, Variant(), "File must be opened before use.");
    uint32_t len = get_32();
    PoolVector<uint8_t> buff = get_buffer(len);
    ERR_FAIL_COND_V((uint32_t)buff.size() != len, Variant());

    PoolVector<uint8_t>::Read r = buff.read();

    Variant v;
    Error err = decode_variant(v, &r[0], len, nullptr, p_allow_objects);
    ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to encode Variant.");

    return v;
}

uint64_t _File::get_modified_time(StringView p_file) const {
    return FileAccess::get_modified_time(p_file);
}

IMPL_GDCLASS(_File)

void _File::_bind_methods() {
    SE_BIND_METHOD(_File,open_encrypted);
    MethodBinder::bind_method(
            D_METHOD("open_encrypted_with_pass", { "path", "mode_flags", "pass" }), &_File::open_encrypted_pass);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, open_compressed, DEFVAL(0) );

    SE_BIND_METHOD(_File,open);
    SE_BIND_METHOD(_File,flush);
    SE_BIND_METHOD(_File,close);
    SE_BIND_METHOD(_File,get_path);
    SE_BIND_METHOD(_File,get_path_absolute);
    SE_BIND_METHOD(_File,is_open);
    SE_BIND_METHOD(_File,seek);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, seek_end, DEFVAL(0) );
    SE_BIND_METHOD(_File,get_position);
    SE_BIND_METHOD(_File,get_len);
    SE_BIND_METHOD(_File,eof_reached);
    SE_BIND_METHOD(_File,get_8);
    SE_BIND_METHOD(_File,get_16);
    SE_BIND_METHOD(_File,get_32);
    SE_BIND_METHOD(_File,get_64);
    SE_BIND_METHOD(_File,get_float);
    SE_BIND_METHOD(_File,get_double);
    SE_BIND_METHOD(_File,get_real);
    SE_BIND_METHOD(_File,get_buffer);
    SE_BIND_METHOD(_File,get_line);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, get_csv_line, DEFVAL(',') );
    SE_BIND_METHOD(_File,get_as_text);
    SE_BIND_METHOD(_File,get_md5);
    SE_BIND_METHOD(_File,get_sha256);
    SE_BIND_METHOD(_File,get_endian_swap);
    SE_BIND_METHOD(_File,set_endian_swap);
    SE_BIND_METHOD(_File,get_error);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, get_var, DEFVAL(false) );

    SE_BIND_METHOD(_File,store_8);
    SE_BIND_METHOD(_File,store_16);
    SE_BIND_METHOD(_File,store_32);
    SE_BIND_METHOD(_File,store_64);
    SE_BIND_METHOD(_File,store_float);
    SE_BIND_METHOD(_File,store_double);
    SE_BIND_METHOD(_File,store_real);
    SE_BIND_METHOD(_File,store_buffer);
    SE_BIND_METHOD(_File,store_line);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, store_csv_line, DEFVAL(',') );
    SE_BIND_METHOD(_File,store_string);
    SE_BIND_METHOD_WITH_DEFAULTS(_File, store_var, DEFVAL(false) );

    SE_BIND_METHOD(_File,store_pascal_string);
    SE_BIND_METHOD(_File,get_pascal_string);

    SE_BIND_METHOD(_File,file_exists);
    SE_BIND_METHOD(_File,get_modified_time);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "endian_swap"), "set_endian_swap", "get_endian_swap");

    BIND_ENUM_CONSTANT(READ);
    BIND_ENUM_CONSTANT(WRITE);
    BIND_ENUM_CONSTANT(READ_WRITE);
    BIND_ENUM_CONSTANT(WRITE_READ);

    BIND_ENUM_CONSTANT(COMPRESSION_FASTLZ);
    BIND_ENUM_CONSTANT(COMPRESSION_DEFLATE);
    BIND_ENUM_CONSTANT(COMPRESSION_ZSTD);
    BIND_ENUM_CONSTANT(COMPRESSION_GZIP);
}

_File::_File() {
    f = nullptr;
    eswap = false;
}

_File::~_File() {
    memdelete(f);
}

///////////////////////////////////////////////////////

Error _Directory::open(StringView p_path) {
    Error err;
    DirAccess *alt = DirAccess::open(p_path, &err);

    if (!alt) {
        return err;
    }
    memdelete(d);
    d = alt;

    return OK;
}

Error _Directory::list_dir_begin(bool p_skip_navigational, bool p_skip_hidden) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");

    _list_skip_navigational = p_skip_navigational;
    _list_skip_hidden = p_skip_hidden;

    return d->list_dir_begin();
}

String _Directory::get_next() {
    ERR_FAIL_COND_V_MSG(!d, String(), "Directory must be opened before use.");

    String next = d->get_next();
    while (!next.empty() && ((_list_skip_navigational && (next == "." || next == "..")) ||
                                    (_list_skip_hidden && d->current_is_hidden()))) {
        next = d->get_next();
    }
    return next;
}
bool _Directory::current_is_dir() const {
    ERR_FAIL_COND_V_MSG(!d, false, "Directory must be opened before use.");
    return d->current_is_dir();
}

void _Directory::list_dir_end() {
    ERR_FAIL_COND_MSG(!d, "Directory must be opened before use.");
    d->list_dir_end();
}

int _Directory::get_drive_count() {
    ERR_FAIL_COND_V_MSG(!d, 0, "Directory must be opened before use.");
    return d->get_drive_count();
}
String _Directory::get_drive(int p_drive) {
    ERR_FAIL_COND_V_MSG(!d, String(), "Directory must be opened before use.");
    return d->get_drive(p_drive);
}
int _Directory::get_current_drive() {
    ERR_FAIL_COND_V_MSG(!d, 0, "Directory must be opened before use.");
    return d->get_current_drive();
}

Error _Directory::change_dir(StringView p_dir) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    return d->change_dir(p_dir);
}
String _Directory::get_current_dir() {
    ERR_FAIL_COND_V_MSG(!d, String(), "Directory must be opened before use.");
    return d->get_current_dir();
}
Error _Directory::make_dir(StringView p_dir) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    if (!PathUtils::is_rel_path(p_dir)) {
        DirAccess *d = DirAccess::create_for_path(p_dir);
        Error err = d->make_dir(p_dir);
        memdelete(d);
        return err;
    }
    return d->make_dir(p_dir);
}
Error _Directory::make_dir_recursive(StringView p_dir) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    if (!PathUtils::is_rel_path(p_dir)) {
        DirAccess *d = DirAccess::create_for_path(p_dir);
        Error err = d->make_dir_recursive(p_dir);
        memdelete(d);
        return err;
    }
    return d->make_dir_recursive(p_dir);
}

bool _Directory::file_exists(StringView p_file) {
    ERR_FAIL_COND_V_MSG(!d, false, "Directory must be opened before use.");

    if (!PathUtils::is_rel_path(p_file)) {
        return FileAccess::exists(p_file);
    }

    return d->file_exists(p_file);
}

bool _Directory::dir_exists(StringView p_dir) {
    ERR_FAIL_COND_V_MSG(!d, false, "Directory must be opened before use.");
    if (!PathUtils::is_rel_path(p_dir)) {
        DirAccess *d = DirAccess::create_for_path(p_dir);
        bool exists = d->dir_exists(p_dir);
        memdelete(d);
        return exists;

    } else {
        return d->dir_exists(p_dir);
    }
}

uint64_t _Directory::get_space_left() {
    ERR_FAIL_COND_V_MSG(!d, 0, "Directory must be opened before use.");
    return d->get_space_left() / 1024 * 1024; // Truncate to closest MiB.
}

Error _Directory::copy(StringView p_from, StringView p_to) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    return d->copy(p_from, p_to);
}
Error _Directory::rename(StringView p_from, StringView p_to) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    ERR_FAIL_COND_V_MSG(p_from.empty() || p_from == "." || p_from == "..", ERR_INVALID_PARAMETER, "Invalid path to rename.");

    if (!PathUtils::is_rel_path(p_from)) {
        DirAccess *d = DirAccess::create_for_path(p_from);
        ERR_FAIL_COND_V_MSG(!d->file_exists(p_from) && !d->dir_exists(p_from), ERR_DOES_NOT_EXIST, "File or directory does not exist.");
        Error err = d->rename(p_from, p_to);
        memdelete(d);
        return err;
    }

    ERR_FAIL_COND_V_MSG(!d->file_exists(p_from) && !d->dir_exists(p_from), ERR_DOES_NOT_EXIST, "File or directory does not exist.");
    return d->rename(p_from, p_to);
}
Error _Directory::remove(StringView p_name) {
    ERR_FAIL_COND_V_MSG(!d, ERR_UNCONFIGURED, "Directory must be opened before use.");
    bool file_exists = d->file_exists(p_name);
    if (!PathUtils::is_rel_path(p_name)) {
        DirAccess *d = DirAccess::create_for_path(p_name);
        ERR_FAIL_COND_V_MSG(!file_exists, ERR_DOES_NOT_EXIST, "File does not exist.");
        Error err = d->remove(p_name);
        memdelete(d);
        return err;
    }

    ERR_FAIL_COND_V_MSG(!file_exists, ERR_DOES_NOT_EXIST, "File does not exist.");
    return d->remove(p_name);
}

IMPL_GDCLASS(_Directory)

void _Directory::_bind_methods() {
    SE_BIND_METHOD(_Directory,open);
    SE_BIND_METHOD_WITH_DEFAULTS(_Directory, list_dir_begin, DEFVAL(false), DEFVAL(false) );
    SE_BIND_METHOD(_Directory,get_next);
    SE_BIND_METHOD(_Directory,current_is_dir);
    SE_BIND_METHOD(_Directory,list_dir_end);
    SE_BIND_METHOD(_Directory,get_drive_count);
    SE_BIND_METHOD(_Directory,get_drive);
    SE_BIND_METHOD(_Directory,get_current_drive);
    SE_BIND_METHOD(_Directory,change_dir);
    SE_BIND_METHOD(_Directory,get_current_dir);
    SE_BIND_METHOD(_Directory,make_dir);
    SE_BIND_METHOD(_Directory,make_dir_recursive);
    SE_BIND_METHOD(_Directory,file_exists);
    SE_BIND_METHOD(_Directory,dir_exists);
    // BIND_METHOD(_Directory,get_modified_time);
    SE_BIND_METHOD(_Directory,get_space_left);
    SE_BIND_METHOD(_Directory,copy);
    SE_BIND_METHOD(_Directory,rename);
    SE_BIND_METHOD(_Directory,remove);
}

_Directory::_Directory() {
    d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
}

_Directory::~_Directory() {
    memdelete(d);
}

_Marshalls *_Marshalls::singleton = nullptr;

_Marshalls *_Marshalls::get_singleton() {
    return singleton;
}

String _Marshalls::variant_to_base64(const Variant &p_var, bool p_full_objects) {
    int len;
    Error err = encode_variant(p_var, nullptr, len, p_full_objects);
    ERR_FAIL_COND_V_MSG(err != OK, {}, "Error when trying to encode Variant.");

    PoolVector<uint8_t> buff;
    buff.resize(len);
    PoolVector<uint8_t>::Write w = buff.write();

    err = encode_variant(p_var, &w[0], len, p_full_objects);
    ERR_FAIL_COND_V_MSG(err != OK, {}, "Error when trying to encode Variant.");

    String ret = CryptoCore::b64_encode_str(&w[0], len);
    ERR_FAIL_COND_V(ret.empty(), ret);

    return ret;
}

Variant _Marshalls::base64_to_variant(StringView p_str, bool p_allow_objects) {
    int strlen = p_str.size();
    String cstr(p_str);

    PoolVector<uint8_t> buf;
    buf.resize(strlen / 4 * 3 + 1);
    PoolVector<uint8_t>::Write w = buf.write();

    size_t len = 0;
    ERR_FAIL_COND_V(
            CryptoCore::b64_decode(&w[0], buf.size(), &len, (unsigned char *)cstr.data(), strlen) != OK, Variant());

    Variant v;
    Error err = decode_variant(v, &w[0], len, nullptr, p_allow_objects);
    ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to decode Variant.");

    return v;
}

String _Marshalls::raw_to_base64(const PoolVector<uint8_t> &p_arr) {
    String ret = CryptoCore::b64_encode_str(p_arr.read().ptr(), p_arr.size());
    ERR_FAIL_COND_V(ret.empty(), ret);
    return ret;
}

PoolVector<uint8_t> _Marshalls::base64_to_raw(StringView p_str) {
    int strlen = p_str.size();
    String cstr(p_str);

    size_t arr_len = 0;
    PoolVector<uint8_t> buf;
    {
        buf.resize(strlen / 4 * 3 + 1);
        PoolVector<uint8_t>::Write w = buf.write();

        ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &arr_len, (unsigned char *)cstr.data(), strlen) != OK,
                PoolVector<uint8_t>());
    }
    buf.resize(arr_len);

    return buf;
}

String _Marshalls::utf8_to_base64(StringView p_str) {
    String ret = CryptoCore::b64_encode_str((const unsigned char *)p_str.data(), p_str.length());
    ERR_FAIL_COND_V(ret.empty(), ret);
    return ret;
}

String _Marshalls::base64_to_utf8(StringView p_str) {
    int strlen = p_str.length();

    PoolVector<uint8_t> buf;
    buf.resize(strlen / 4 * 3 + 1 + 1);
    PoolVector<uint8_t>::Write w = buf.write();

    size_t len = 0;
    ERR_FAIL_COND_V(CryptoCore::b64_decode(&w[0], buf.size(), &len, (const unsigned char *)p_str.data(), strlen) != OK,
            String());

    return String((const char *)&w[0], len);
}

IMPL_GDCLASS(_Marshalls)

void _Marshalls::_bind_methods() {
    SE_BIND_METHOD_WITH_DEFAULTS(_Marshalls, variant_to_base64, DEFVAL(false) );
    SE_BIND_METHOD_WITH_DEFAULTS(_Marshalls, base64_to_variant, DEFVAL(false) );

    SE_BIND_METHOD(_Marshalls,raw_to_base64);
    SE_BIND_METHOD(_Marshalls,base64_to_raw);

    SE_BIND_METHOD(_Marshalls,utf8_to_base64);
    SE_BIND_METHOD(_Marshalls,base64_to_utf8);
}

////////////////

void _Semaphore::wait() {
    semaphore.wait();
}

void _Semaphore::post() {
    semaphore.post();
}

IMPL_GDCLASS(_Semaphore)

void _Semaphore::_bind_methods() {
    SE_BIND_METHOD(_Semaphore,wait);
    SE_BIND_METHOD(_Semaphore,post);
}

///////////////

void _Mutex::lock() {
    mutex.lock();
}

Error _Mutex::try_lock() {
    return mutex.try_lock() ? OK : FAILED;
}

void _Mutex::unlock() {
    mutex.unlock();
}

IMPL_GDCLASS(_Mutex)

void _Mutex::_bind_methods() {
    SE_BIND_METHOD(_Mutex,lock);
    SE_BIND_METHOD(_Mutex,try_lock);
    SE_BIND_METHOD(_Mutex,unlock);
}


///////////////

void _Thread::_start_func(void *ud) {
    Ref<_Thread> *tud = (Ref<_Thread> *)ud;
    Ref<_Thread> t = *tud;
    memdelete(tud);
    Callable::CallError ce;
    const Variant *arg[1] = { &t->userdata };

    Thread::set_name(t->target_method);

    Object *target_instance = object_for_entity(t->target_instance_id);
    if (!target_instance) {
        ERR_FAIL_MSG(FormatVE("Could not call function '%s' on previously freed instance to start thread %s.",
                t->target_method.asCString(), t->get_id().c_str()));
    }

    t->ret = target_instance->call(t->target_method, arg, 1, ce);
    if (ce.error != Callable::CallError::CALL_OK) {
        String reason;
        switch (ce.error) {
            case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT: {
                reason = "Invalid Argument #" + itos(ce.argument);
            } break;
            case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {
                reason = "Too Many Arguments";
            } break;
            case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS: {
                reason = "Too Few Arguments";
            } break;
            case Callable::CallError::CALL_ERROR_INVALID_METHOD: {
                reason = "Method Not Found";
            } break;
            default: {
            }
        }

        t->running.clear();
        ERR_FAIL_MSG("Could not call function '" + String(t->target_method.asCString()) + "' to start thread " +
                     t->get_id() + ": " + reason + ".");
    }
    t->running.clear();
}

Error _Thread::start(Object *p_instance, const StringName &p_method, const Variant &p_userdata, Priority p_priority) {
    ERR_FAIL_COND_V_MSG(is_active(), ERR_ALREADY_IN_USE, "Thread already started.");
    ERR_FAIL_COND_V(!p_instance, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(p_method.empty() || !p_instance->has_method(p_method), ERR_INVALID_PARAMETER);
    ERR_FAIL_INDEX_V(p_priority, PRIORITY_MAX, ERR_INVALID_PARAMETER);

    ret = Variant();
    target_method = p_method;
    target_instance_id = p_instance->get_instance_id();
    userdata = p_userdata;
    running.set();

    Ref<_Thread> *ud = memnew(Ref<_Thread>(this));

    Thread::Settings s;
    s.priority = (Thread::Priority)p_priority;
    thread.start(_start_func, ud, s);

    return OK;
}

String _Thread::get_id() const {
    return itos(std::hash<std::thread::id>()(thread.get_id()));
}

bool _Thread::is_active() const {
    return thread.is_started();
}
bool _Thread::is_alive() const {
    return running.is_set();
}
Variant _Thread::wait_to_finish() {
    ERR_FAIL_COND_V_MSG(!is_active(), Variant(), "Thread must have been started to wait for its completion.");
    thread.wait_to_finish();
    Variant r = ret;
    running.clear();
    target_method = StringName();
    target_instance_id = entt::null;
    userdata = Variant();

    return r;
}

IMPL_GDCLASS(_Thread)

void _Thread::_bind_methods() {
    SE_BIND_METHOD_WITH_DEFAULTS(_Thread, start, DEFVAL(Variant()), DEFVAL(PRIORITY_NORMAL) );
    SE_BIND_METHOD(_Thread,get_id);
    SE_BIND_METHOD(_Thread,is_active);
    SE_BIND_METHOD(_Thread,is_alive);
    SE_BIND_METHOD(_Thread,wait_to_finish);

    BIND_ENUM_CONSTANT(PRIORITY_LOW);
    BIND_ENUM_CONSTANT(PRIORITY_NORMAL);
    BIND_ENUM_CONSTANT(PRIORITY_HIGH);
}
_Thread::_Thread() {
    target_instance_id = entt::null;
}

_Thread::~_Thread() {
    ERR_FAIL_COND_MSG(is_active(), "Reference to a Thread object was lost while the thread is still running...");
}
/////////////////////////////////////

PoolStringArray _ClassDB::get_class_list() const {
    Vector<StringName> classes;
    ClassDB::get_class_list(&classes);

    PoolStringArray ret;
    ret.resize(classes.size());
    int idx = 0;
    for (const StringName &sn : classes) {
        ret.set(idx++, String(sn));
    }

    return ret;
}
PoolStringArray _ClassDB::get_inheriters_from_class(const StringName &p_class) const {
    Vector<StringName> classes;
    ClassDB::get_inheriters_from_class(p_class, &classes);

    PoolStringArray ret;
    ret.resize(classes.size());
    int idx = 0;
    for (const StringName &E : classes) {
        ret.set(idx++, String(E));
    }

    return ret;
}
StringName _ClassDB::get_parent_class(const StringName &p_class) const {
    return ClassDB::get_parent_class(p_class);
}
bool _ClassDB::class_exists(const StringName &p_class) const {
    return ClassDB::class_exists(p_class);
}
bool _ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) const {
    return ClassDB::is_parent_class(p_class, p_inherits);
}
bool _ClassDB::can_instance(const StringName &p_class) const {
    return ClassDB::can_instance(p_class);
}
Variant _ClassDB::instance(const StringName &p_class) const {
    Object *obj = ClassDB::instance(p_class);
    if (!obj) {
        return Variant();
    }

    auto *r = object_cast<RefCounted>(obj);
    if (r) {
        return REF(r,DoNotAddRef);
    } else {
        return Variant(obj);
    }
}

bool _ClassDB::has_signal(StringName p_class, StringName p_signal) const {
    return ClassDB::has_signal(p_class, p_signal);
}
Dictionary _ClassDB::get_signal(StringName p_class, StringName p_signal) const {
    MethodInfo signal;
    if (ClassDB::get_signal(p_class, p_signal, &signal)) {
        return signal.operator Dictionary();
    } else {
        return Dictionary();
    }
}
Array _ClassDB::get_signal_list(StringName p_class, bool p_no_inheritance) const {
    Vector<MethodInfo> defined_signals;
    ClassDB::get_signal_list(p_class, &defined_signals, p_no_inheritance);
    Array ret;

    for (const MethodInfo &E : defined_signals) {
        ret.push_back(E.operator Dictionary());
    }

    return ret;
}

Array _ClassDB::get_property_list(StringName p_class, bool p_no_inheritance) const {
    Vector<PropertyInfo> plist;
    ClassDB::get_property_list(p_class, &plist, p_no_inheritance);
    Array ret;
    for (const PropertyInfo &E : plist) {
        ret.push_back(E.operator Dictionary());
    }

    return ret;
}

Variant _ClassDB::get_property(Object *p_object, const StringName &p_property) const {
    Variant ret;
    ClassDB::get_property(p_object, p_property, ret);
    return ret;
}

Error _ClassDB::set_property(Object *p_object, const StringName &p_property, const Variant &p_value) const {
    Variant ret;
    bool valid;
    if (!ClassDB::set_property(p_object, p_property, p_value, &valid)) {
        return ERR_UNAVAILABLE;
    } else if (!valid) {
        return ERR_INVALID_DATA;
    }
    return OK;
}

bool _ClassDB::has_method(StringName p_class, StringName p_method, bool p_no_inheritance) const {
    return ClassDB::has_method(p_class, p_method, p_no_inheritance);
}

Array _ClassDB::get_method_list(StringName p_class, bool p_no_inheritance) const {
    Vector<MethodInfo> methods;
    ClassDB::get_method_list(p_class, &methods, p_no_inheritance);
    Array ret;

    for (const MethodInfo &E : methods) {
#ifdef DEBUG_METHODS_ENABLED
        ret.push_back(E.operator Dictionary());
#else
        Dictionary dict;
        dict["name"] = E.name;
        ret.push_back(dict);
#endif
    }

    return ret;
}

PoolStringArray _ClassDB::get_integer_constant_list(const StringName &p_class, bool p_no_inheritance) const {
    Vector<String> constants;
    ClassDB::get_integer_constant_list(p_class, &constants, p_no_inheritance);

    PoolStringArray ret;
    ret.resize(constants.size());
    int idx = 0;
    for (const String &E : constants) {
        ret.set(idx++, E);
    }

    return ret;
}

bool _ClassDB::has_integer_constant(const StringName &p_class, const StringName &p_name) const {
    bool success;
    ClassDB::get_integer_constant(p_class, p_name, &success);
    return success;
}

int _ClassDB::get_integer_constant(const StringName &p_class, const StringName &p_name) const {
    bool found;
    int c = ClassDB::get_integer_constant(p_class, p_name, &found);
    ERR_FAIL_COND_V(!found, 0);
    return c;
}
StringName _ClassDB::get_category(const StringName &p_node) const {
    return ClassDB::get_category(p_node);
}

bool _ClassDB::is_class_enabled(StringName p_class) const {
    return ClassDB::is_class_enabled(p_class);
}

IMPL_GDCLASS(_ClassDB)

void _ClassDB::_bind_methods() {
    SE_BIND_METHOD(_ClassDB,get_class_list);
    SE_BIND_METHOD(_ClassDB,get_inheriters_from_class);
    SE_BIND_METHOD(_ClassDB,get_parent_class);
    SE_BIND_METHOD(_ClassDB,class_exists);
    SE_BIND_METHOD(_ClassDB,is_parent_class);
    SE_BIND_METHOD(_ClassDB,can_instance);
    SE_BIND_METHOD(_ClassDB,instance);

    MethodBinder::bind_method(D_METHOD("class_has_signal", { "class", "signal" }), &_ClassDB::has_signal);
    MethodBinder::bind_method(D_METHOD("class_get_signal", { "class", "signal" }), &_ClassDB::get_signal);
    MethodBinder::bind_method(D_METHOD("class_get_signal_list", { "class", "no_inheritance" }),
            &_ClassDB::get_signal_list, { DEFVAL(false) });

    MethodBinder::bind_method(D_METHOD("class_get_property_list", { "class", "no_inheritance" }),
            &_ClassDB::get_property_list, { DEFVAL(false) });
    MethodBinder::bind_method(D_METHOD("class_get_property", { "object", "property" }), &_ClassDB::get_property);
    MethodBinder::bind_method(
            D_METHOD("class_set_property", { "object", "property", "value" }), &_ClassDB::set_property);

    MethodBinder::bind_method(D_METHOD("class_has_method", { "class", "method", "no_inheritance" }),
            &_ClassDB::has_method, { DEFVAL(false) });

    MethodBinder::bind_method(D_METHOD("class_get_method_list", { "class", "no_inheritance" }),
            &_ClassDB::get_method_list, { DEFVAL(false) });

    MethodBinder::bind_method(D_METHOD("class_get_integer_constant_list", { "class", "no_inheritance" }),
            &_ClassDB::get_integer_constant_list, { DEFVAL(false) });

    MethodBinder::bind_method(
            D_METHOD("class_has_integer_constant", { "class", "name" }), &_ClassDB::has_integer_constant);
    MethodBinder::bind_method(
            D_METHOD("class_get_integer_constant", { "class", "name" }), &_ClassDB::get_integer_constant);
//    MethodBinder::bind_method(D_METHOD("class_has_enum", {"class", "name", "no_inheritance"}), &_ClassDB::has_enum, {DEFVAL(false)});
//    MethodBinder::bind_method(D_METHOD("class_get_enum_list", {"class", "no_inheritance"}), &_ClassDB::get_enum_list, {DEFVAL(false)});
//    MethodBinder::bind_method(D_METHOD("class_get_enum_constants", {"class", "enum", "no_inheritance"}), &_ClassDB::get_enum_constants, {DEFVAL(false)});
//    MethodBinder::bind_method(D_METHOD("class_get_integer_constant_enum", {"class", "name", "no_inheritance"}), &_ClassDB::get_integer_constant_enum, {DEFVAL(false)});

    MethodBinder::bind_method(D_METHOD("class_get_category", { "class" }), &_ClassDB::get_category);
    SE_BIND_METHOD(_ClassDB,is_class_enabled);
}

_ClassDB::_ClassDB() = default;
_ClassDB::~_ClassDB() = default;
///////////////////////////////

void _Engine::set_iterations_per_second(int p_ips) {
    Engine::get_singleton()->set_iterations_per_second(p_ips);
}
int _Engine::get_iterations_per_second() const {
    return Engine::get_singleton()->get_iterations_per_second();
}

void _Engine::set_physics_jitter_fix(float p_threshold) {
    Engine::get_singleton()->set_physics_jitter_fix(p_threshold);
}

float _Engine::get_physics_jitter_fix() const {
    return Engine::get_singleton()->get_physics_jitter_fix();
}

float _Engine::get_physics_interpolation_fraction() const {
    return Engine::get_singleton()->get_physics_interpolation_fraction();
}

void _Engine::set_target_fps(int p_fps) {
    Engine::get_singleton()->set_target_fps(p_fps);
}

int _Engine::get_target_fps() const {
    return Engine::get_singleton()->get_target_fps();
}

float _Engine::get_frames_per_second() const {
    return Engine::get_singleton()->get_frames_per_second();
}

uint64_t _Engine::get_physics_frames() const {
    return Engine::get_singleton()->get_physics_frames();
}

uint64_t _Engine::get_idle_frames() const {
    return Engine::get_singleton()->get_idle_frames();
}

void _Engine::set_time_scale(float p_scale) {
    Engine::get_singleton()->set_time_scale(p_scale);
}

float _Engine::get_time_scale() {
    return Engine::get_singleton()->get_time_scale();
}

int _Engine::get_frames_drawn() {
    return Engine::get_singleton()->get_frames_drawn();
}

MainLoop *_Engine::get_main_loop() const {
    // needs to remain in OS, since it's actually OS that interacts with it, but it's better exposed here
    return OS::get_singleton()->get_main_loop();
}

Dictionary _Engine::get_version_info() const {
    return Engine::get_singleton()->get_version_info();
}

Dictionary _Engine::get_author_info() const {
    return Engine::get_singleton()->get_author_info();
}

Array _Engine::get_copyright_info() const {
    return Engine::get_singleton()->get_copyright_info();
}

Dictionary _Engine::get_donor_info() const {
    return Engine::get_singleton()->get_donor_info();
}

Dictionary _Engine::get_license_info() const {
    return Engine::get_singleton()->get_license_info();
}

String _Engine::get_license_text() const {
    return Engine::get_singleton()->get_license_text();
}

bool _Engine::is_in_physics_frame() const {
    return Engine::get_singleton()->is_in_physics_frame();
}

bool _Engine::has_singleton(StringView p_name) const {
    return Engine::get_singleton()->has_singleton(StringName(p_name));
}

Object *_Engine::get_named_singleton(const StringName &p_name) const {
    return Engine::get_singleton()->get_named_singleton(p_name);
}

void _Engine::set_editor_hint(bool p_enabled) {
    Engine::get_singleton()->set_editor_hint(p_enabled);
}

bool _Engine::is_editor_hint() const {
    return Engine::get_singleton()->is_editor_hint();
}

IMPL_GDCLASS(_Engine)

void _Engine::_bind_methods() {
    MethodBinder::bind_method(
            D_METHOD("set_iterations_per_second", { "iterations_per_second" }), &_Engine::set_iterations_per_second);
    SE_BIND_METHOD(_Engine,get_iterations_per_second);
    MethodBinder::bind_method(
            D_METHOD("set_physics_jitter_fix", { "physics_jitter_fix" }), &_Engine::set_physics_jitter_fix);
    SE_BIND_METHOD(_Engine,get_physics_jitter_fix);
    MethodBinder::bind_method(
            D_METHOD("get_physics_interpolation_fraction"), &_Engine::get_physics_interpolation_fraction);
    SE_BIND_METHOD(_Engine,set_target_fps);
    SE_BIND_METHOD(_Engine,get_target_fps);

    SE_BIND_METHOD(_Engine,set_time_scale);
    SE_BIND_METHOD(_Engine,get_time_scale);

    SE_BIND_METHOD(_Engine,get_frames_drawn);
    SE_BIND_METHOD(_Engine,get_frames_per_second);
    SE_BIND_METHOD(_Engine,get_physics_frames);
    SE_BIND_METHOD(_Engine,get_idle_frames);

    SE_BIND_METHOD(_Engine,get_main_loop);

    SE_BIND_METHOD(_Engine,get_version_info);
    SE_BIND_METHOD(_Engine,get_author_info);
    SE_BIND_METHOD(_Engine,get_copyright_info);
    SE_BIND_METHOD(_Engine,get_donor_info);
    SE_BIND_METHOD(_Engine,get_license_info);
    SE_BIND_METHOD(_Engine,get_license_text);

    SE_BIND_METHOD(_Engine,is_in_physics_frame);

    SE_BIND_METHOD(_Engine,has_singleton);
    SE_BIND_METHOD(_Engine,get_named_singleton);

    SE_BIND_METHOD(_Engine,set_editor_hint);
    SE_BIND_METHOD(_Engine,is_editor_hint);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editor_hint"), "set_editor_hint", "is_editor_hint");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "iterations_per_second"), "set_iterations_per_second",
            "get_iterations_per_second");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "target_fps"), "set_target_fps", "get_target_fps");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "time_scale"), "set_time_scale", "get_time_scale");
    ADD_PROPERTY(
            PropertyInfo(VariantType::FLOAT, "physics_jitter_fix"), "set_physics_jitter_fix", "get_physics_jitter_fix");
}

_Engine *_Engine::singleton = nullptr;

_Engine::_Engine() {
    singleton = this;
}

IMPL_GDCLASS(JSONParseResult)

void JSONParseResult::_bind_methods() {
    SE_BIND_METHOD(JSONParseResult,get_error);
    SE_BIND_METHOD(JSONParseResult,get_error_string);
    SE_BIND_METHOD(JSONParseResult,get_error_line);
    SE_BIND_METHOD(JSONParseResult,get_result);

    SE_BIND_METHOD(JSONParseResult,set_error);
    SE_BIND_METHOD(JSONParseResult,set_error_string);
    SE_BIND_METHOD(JSONParseResult,set_error_line);
    SE_BIND_METHOD(JSONParseResult,set_result);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "error", PropertyHint::None, "Error", PROPERTY_USAGE_CLASS_IS_ENUM),
            "set_error", "get_error");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "error_string"), "set_error_string", "get_error_string");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "error_line"), "set_error_line", "get_error_line");
    ADD_PROPERTY(PropertyInfo(VariantType::NIL, "result", PropertyHint::None, "", PROPERTY_USAGE_NIL_IS_VARIANT),
            "set_result", "get_result");
}

void JSONParseResult::set_error(Error p_error) {
    error = p_error;
}

Error JSONParseResult::get_error() const {
    return error;
}

void JSONParseResult::set_error_string(StringView p_error_string) {
    error_string = p_error_string;
}

const String &JSONParseResult::get_error_string() const {
    return error_string;
}

void JSONParseResult::set_error_line(int p_error_line) {
    error_line = p_error_line;
}

int JSONParseResult::get_error_line() const {
    return error_line;
}

void JSONParseResult::set_result(const Variant &p_result) {
    result = p_result;
}

Variant JSONParseResult::get_result() const {
    return result;
}

IMPL_GDCLASS(_JSON)

void _JSON::_bind_methods() {
    SE_BIND_METHOD_WITH_DEFAULTS(_JSON, print, DEFVAL(String()), DEFVAL(false) );
    SE_BIND_METHOD(_JSON,parse);
}

String _JSON::print(const Variant &p_value, StringView p_indent, bool p_sort_keys) {
    return JSON::print(p_value, p_indent, p_sort_keys);
}

Ref<JSONParseResult> _JSON::parse(const String &p_json) {
    Ref<JSONParseResult> result(make_ref_counted<JSONParseResult>());

    result->error = JSON::parse(p_json, result->result, result->error_string, result->error_line);
    if (result->error != OK) {
        ERR_PRINT(FormatVE("Error parsing JSON at line %d: %s", result->error_line, result->error_string.c_str()));
    }
    return result;
}

_JSON *_JSON::singleton = nullptr;

_JSON::_JSON() {
    singleton = this;
}
