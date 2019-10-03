/*************************************************************************/
/*  os_x11.h                                                             */
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

#pragma once
#include "core/object.h"
#include "context_gl_x11.h"
#include "core/os/input.h"
#include "crash_handler_x11.h"
#include "drivers/alsa/audio_driver_alsa.h"
#include "drivers/alsamidi/midi_driver_alsamidi.h"
#include "drivers/pulseaudio/audio_driver_pulseaudio.h"
#include "drivers/unix/os_unix.h"
#include "joypad_linux.h"
#include "main/input_default.h"
#include "power_x11.h"
#include "servers/audio_server.h"
#include "servers/camera_server.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"
//#include "servers/visual/visual_server_wrap_mt.h"

#include <X11/Xcursor/Xcursor.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

// Hints for X11 fullscreen
typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} Hints;

typedef struct _xrr_monitor_info {
    Atom name;
    Bool primary;
    Bool automatic;
    int noutput;
    int x;
    int y;
    int width;
    int height;
    int mwidth;
    int mheight;
    RROutput *outputs;
} xrr_monitor_info;

#undef CursorShape

class OS_X11 : public OS_Unix {

    Atom wm_delete;
    Atom xdnd_enter;
    Atom xdnd_position;
    Atom xdnd_status;
    Atom xdnd_action_copy;
    Atom xdnd_drop;
    Atom xdnd_finished;
    Atom xdnd_selection;
    Atom requested;

    int xdnd_version;

#if defined(OPENGL_ENABLED)
    ContextGL_X11 *context_gl;
#endif
    //Rasterizer *rasterizer;
    VisualServer *visual_server;
    VideoMode current_videomode;
    ListPOD<String> args;
    Window x11_window;
    Window xdnd_source_window;
    MainLoop *main_loop;
    ::Display *x11_display;
    char *xmbstring;
    int xmblen;
    unsigned long last_timestamp;
    ::Time last_keyrelease_time;
    ::XIC xic;
    ::XIM xim;
    ::XIMStyle xim_style;
    static void xim_destroy_callback(::XIM im, ::XPointer client_data,
            ::XPointer call_data);

    // IME
    bool im_active;
    Vector2 im_position;

    Size2 min_size;
    Size2 max_size;

    Point2 last_mouse_pos;
    bool last_mouse_pos_valid;
    Point2i last_click_pos;
    uint64_t last_click_ms;
    int last_click_button_index;
    uint32_t last_button_state;

    struct {
        int opcode;
        Vector<int> touch_devices;
        Map<int, Vector2> absolute_devices;
        XIEventMask all_event_mask;
        XIEventMask all_master_event_mask;
        Map<int, Vector2> state;
        Vector2 mouse_pos_to_filter;
        Vector2 relative_motion;
        Vector2 raw_pos;
        Vector2 old_raw_pos;
        ::Time last_relative_time;
    } xi;

    bool refresh_device_info();

    unsigned int get_mouse_button_state(unsigned int p_x11_button, int p_x11_type);
    void get_key_modifier_state(unsigned int p_x11_state, Ref<InputEventWithModifiers> state);
    void flush_mouse_motion();

    CameraServer *camera_server;

    MouseMode mouse_mode;
    Point2i center;

    void handle_key_event(XKeyEvent *p_event, bool p_echo = false);
    void process_xevents();
    void delete_main_loop() override;

    bool force_quit;
    bool minimized;
    bool window_has_focus;
    bool do_mouse_warp;

    const char *cursor_theme;
    int cursor_size;
    XcursorImage *img[CURSOR_MAX];
    Cursor cursors[CURSOR_MAX];
    Cursor null_cursor;
    CursorShape current_cursor;
    Map<CursorShape, Vector<Variant> > cursors_cache;

    InputDefault *input;

#ifdef JOYDEV_ENABLED
    JoypadLinux *joypad;
#endif

#ifdef ALSA_ENABLED
    AudioDriverALSA driver_alsa;
#endif

#ifdef ALSAMIDI_ENABLED
    MIDIDriverALSAMidi driver_alsamidi;
#endif

#ifdef PULSEAUDIO_ENABLED
    AudioDriverPulseAudio driver_pulseaudio;
#endif

    PowerX11 *power_manager;

    bool layered_window;

    CrashHandler crash_handler;

    int video_driver_index;
    bool maximized;
    //void set_wm_border(bool p_enabled);
    void set_wm_fullscreen(bool p_enabled);
    void set_wm_above(bool p_enabled);

    using xrr_get_monitors_t = xrr_monitor_info *(*)(Display *, Window, int, int *);
    using xrr_free_monitors_t = void (*)(xrr_monitor_info *);
    xrr_get_monitors_t xrr_get_monitors;
    xrr_free_monitors_t xrr_free_monitors;
    void *xrandr_handle;
    Bool xrandr_ext_ok;

protected:
    int get_current_video_driver() const override;

    void initialize_core() override;
    Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) override;
    void finalize() override;

    void set_main_loop(MainLoop *p_main_loop) override;

    void _window_changed(XEvent *event);

    bool is_window_maximize_allowed();

public:
    String get_name() const override;

    void set_cursor_shape(CursorShape p_shape) override;
    CursorShape get_cursor_shape() const override;
    void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;

    void set_mouse_mode(MouseMode p_mode) override;
    MouseMode get_mouse_mode() const override;

    void warp_mouse_position(const Point2 &p_to) override;
    Point2 get_mouse_position() const override;
    int get_mouse_button_state() const override;
    void set_window_title(const String &p_title) override;

    void set_icon(const Ref<Image> &p_icon) override;

    MainLoop *get_main_loop() const override;

    bool can_draw() const override;

    void set_clipboard(const String &p_text) override;
    String get_clipboard() const override;

    void release_rendering_thread() override;
    void make_rendering_thread() override;
    void swap_buffers() override;

    Error shell_open(String p_uri) override;

    void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0) override;
    VideoMode get_video_mode(int p_screen = 0) const override;
    void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const override;

    int get_screen_count() const override;
    int get_current_screen() const override;
    void set_current_screen(int p_screen) override;
    Point2 get_screen_position(int p_screen = -1) const override;
    Size2 get_screen_size(int p_screen = -1) const override;
    int get_screen_dpi(int p_screen = -1) const override;
    Point2 get_window_position() const override;
    void set_window_position(const Point2 &p_position) override;
    Size2 get_window_size() const override;
    Size2 get_real_window_size() const override;
    Size2 get_max_window_size() const override;
    Size2 get_min_window_size() const override;
    void set_min_window_size(const Size2 p_size) override;
    void set_max_window_size(const Size2 p_size) override;
    void set_window_size(const Size2 p_size) override;
    void set_window_fullscreen(bool p_enabled) override;
    bool is_window_fullscreen() const override;
    void set_window_resizable(bool p_enabled) override;
    bool is_window_resizable() const override;
    void set_window_minimized(bool p_enabled) override;
    bool is_window_minimized() const override;
    void set_window_maximized(bool p_enabled) override;
    bool is_window_maximized() const override;
    void set_window_always_on_top(bool p_enabled) override;
    bool is_window_always_on_top() const override;
    void request_attention() override;

    void set_borderless_window(bool p_borderless) override;
    bool get_borderless_window() override;

    bool get_window_per_pixel_transparency_enabled() const override;
    void set_window_per_pixel_transparency_enabled(bool p_enabled) override;

    void set_ime_active(const bool p_active) override;
    void set_ime_position(const Point2 &p_pos) override;

    String get_unique_id() const override;

    void move_window_to_foreground() override;
    void alert(const String &p_alert, const String &p_title = "ALERT!") override;

    bool is_joy_known(int p_device) override;
    String get_joy_guid(int p_device) const override;

    void set_context(int p_context) override;

    void _set_use_vsync(bool p_enable) override;
    //virtual bool is_vsync_enabled() const;

    OS::PowerState get_power_state() override;
    int get_power_seconds_left() override;
    int get_power_percent_left() override;

    bool _check_internal_feature_support(const String &p_feature) override;

    void force_process_input() override;
    void run();

    void disable_crash_handler() override;
    bool is_disable_crash_handler() const override;

    Error move_to_trash(const String &p_path) override;

    LatinKeyboardVariant get_latin_keyboard_variant() const override;

    void update_real_mouse_position();
    OS_X11(void */*unused*/);
};
