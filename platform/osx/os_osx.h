/*************************************************************************/
/*  os_osx.h                                                             */
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

#ifndef OS_OSX_H
#define OS_OSX_H

#include "camera_osx.h"
#include "core/os/input.h"
#include "crash_handler_osx.h"
#include "drivers/coreaudio/audio_driver_coreaudio.h"
#include "drivers/coremidi/midi_driver_coremidi.h"
#include "drivers/unix/os_unix.h"
#include "joypad_osx.h"
#include "main/input_default.h"
#include "servers/audio_server.h"
#include "servers/rendering/rasterizer.h"
#include "servers/rendering/rendering_server_wrap_mt.h"
#include "servers/rendering_server.h"

#include <AppKit/AppKit.h>
#include <AppKit/NSCursor.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreVideo/CoreVideo.h>

#undef BitMap
#undef CursorShape

class OS_OSX : public OS_Unix {
public:
    struct KeyEvent {
        unsigned int osx_state;
        bool pressed;
        bool echo;
        bool raw;
        uint32_t scancode;
        uint32_t physical_scancode;
        uint32_t unicode;
    };

    struct WarpEvent {
        NSTimeInterval timestamp;
        NSPoint delta;
    };
    Vector<WarpEvent> warp_events;
    NSTimeInterval last_warp = 0;
    bool ignore_warp = false;

    Vector<KeyEvent> key_event_buffer;
    int key_event_pos;

    bool force_quit;
    bool is_resizing = false;
    //  rasterizer seems to no longer be given to visual server, its using GLES3 directly?
    //Rasterizer *rasterizer;
    RenderingServer *visual_server;

    List<String> args;
    MainLoop *main_loop;

#ifdef COREAUDIO_ENABLED
    AudioDriverCoreAudio audio_driver;
#endif
#ifdef COREMIDI_ENABLED
    MIDIDriverCoreMidi midi_driver;
#endif

    InputDefault *input;
    JoypadOSX *joypad_osx;

    /* objc */

    CGEventSourceRef eventSource;

    void process_events();
    void process_key_events();

    void *framework;
    //          pthread_key_t   current;
    bool mouse_grab;
    Point2 mouse_pos;

    id delegate;
    id window_delegate;
    id window_object;
    id window_view;
    id autoreleasePool;
    id cursor;
    NSOpenGLPixelFormat *pixelFormat;
    NSOpenGLContext *context;
    NSOpenGLContext *context_offscreen;

    bool layered_window;
    bool waiting_for_vsync;
    NSCondition *vsync_condition;
    CVDisplayLinkRef displayLink;

    CursorShape cursor_shape;
    NSCursor *cursors[CURSOR_MAX];
    Map<CursorShape, Vector<Variant> > cursors_cache;
    MouseMode mouse_mode;

    String title;
    bool minimized;
    bool maximized;
    bool zoomed;
    bool resizable;
    bool window_focused;
    bool on_top;

    Size2 window_size;
    Rect2 restore_rect;

    String open_with_filename;

    Point2 im_position;
    bool im_active;
    String im_text;
    Point2 im_selection;

    Size2 min_size;
    Size2 max_size;

    PowerOSX *power_manager;
    CrashHandler crash_handler;

    float _mouse_scale(float p_scale) {
        if (_display_scale() > 1.0)
            return p_scale;
        else
            return 1.0;
    }

    float _display_scale() const;
    float _display_scale(id screen) const;

    void _update_window();

    int video_driver_index;
    virtual int get_current_video_driver() const;

    struct GlobalMenuItem {
        String label;
        Variant signal;
        Variant meta;

        GlobalMenuItem() {
            //NOP
        }

        GlobalMenuItem(const String &p_label, const Variant &p_signal, const Variant &p_meta) {
            label = p_label;
            signal = p_signal;
            meta = p_meta;
        }
    };

    Map<String, Vector<GlobalMenuItem> > global_menus;

    void _update_global_menu();
    static void pre_wait_observer_cb(CFRunLoopObserverRef p_observer, CFRunLoopActivity p_activiy, void *p_context);

protected:
    virtual void initialize_core();
    virtual Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver);
    virtual void finalize();

    virtual void set_main_loop(MainLoop *p_main_loop);
    virtual void delete_main_loop();

public:
    static OS_OSX *singleton;

    void global_menu_add_item(const String &p_menu, const String &p_label, const Variant &p_signal, const Variant &p_meta);
    void global_menu_add_separator(const String &p_menu);
    void global_menu_remove_item(const String &p_menu, int p_idx);
    void global_menu_clear(const String &p_menu);

    void wm_minimized(bool p_minimized);

    virtual String get_name() const;

    virtual void alert(const String &p_alert, const String &p_title = "ALERT!");

    virtual Error open_dynamic_library(StringView p_path, void *&p_library_handle, bool p_also_set_library_path = false);

    virtual void set_cursor_shape(CursorShape p_shape);
    virtual CursorShape get_cursor_shape() const;
    virtual void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot);

    virtual void set_mouse_show(bool p_show);
    virtual void set_mouse_grab(bool p_grab);
    virtual bool is_mouse_grab_enabled() const;
    virtual void warp_mouse_position(const Point2 &p_to);
    virtual Point2 get_mouse_position() const;
    virtual int get_mouse_button_state() const;
    void update_real_mouse_position();
    virtual void set_window_title(const String &p_title);

    virtual Size2 get_window_size() const;
    virtual Size2 get_real_window_size() const;

    virtual void set_native_icon(const String &p_filename);
    virtual void set_icon(const Ref<Image> &p_icon);

    virtual MainLoop *get_main_loop() const;

    String get_godot_dir_name() const override;

    bool can_draw() const override;

    void set_clipboard(StringView p_text) override;
    String get_clipboard() const override;

    void release_rendering_thread() override;
    void make_rendering_thread() override;
    void swap_buffers() override;

    Error shell_open(String p_uri);
    void push_input(const Ref<InputEvent> &p_event);

    const char * get_locale() const override;

    void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0) override;
    VideoMode get_video_mode(int p_screen = 0) const override;
    void get_fullscreen_mode_list(Vector<VideoMode> *p_list, int p_screen = 0) const override;

    bool is_offscreen_gl_available() const override;
    void set_offscreen_gl_current(bool p_current) override;

    String get_executable_path() const override;

    LatinKeyboardVariant get_latin_keyboard_variant() const override;

    void move_window_to_foreground() override;

    int get_screen_count() const override;
    int get_current_screen() const override;
    void set_current_screen(int p_screen) override;
    Point2 get_screen_position(int p_screen = -1) const override;
    Size2 get_screen_size(int p_screen = -1) const override;
    int get_screen_dpi(int p_screen = -1) const override;
    float get_screen_scale(int p_screen = -1) const override;
    float get_screen_max_scale() const override;
    float get_screen_refresh_rate(int p_screen = -1) const override;

    Point2 get_window_position() const override;
    void set_window_position(const Point2 &p_position) override;
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
    bool is_window_focused() const override;
    void request_attention() override;
    StringName get_joy_guid(int p_device) const override;

    void set_borderless_window(bool p_borderless) override;
    bool get_borderless_window() override;

    bool get_window_per_pixel_transparency_enabled() const override;
    void set_window_per_pixel_transparency_enabled(bool p_enabled) override;

    void set_ime_active(const bool p_active) override;
    void set_ime_position(const Point2 &p_pos) override;
    Point2 get_ime_selection() const override;
    String get_ime_text() const override;

    bool _check_internal_feature_support(StringView p_feature) override;

    void _set_use_vsync(bool p_enable) override;
    //virtual bool is_vsync_enabled() const;

    void run();

    void set_mouse_mode(MouseMode p_mode);
    MouseMode get_mouse_mode() const;

    void disable_crash_handler();
    bool is_disable_crash_handler() const;

    void force_process_input();

    OS_OSX(void *);

private:
    Point2 get_native_screen_position(int p_screen) const;
    Point2 get_native_window_position() const;
    void set_native_window_position(const Point2 &p_position);
    Point2 get_screens_origin() const;
};

#endif
