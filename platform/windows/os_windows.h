/*************************************************************************/
/*  os_windows.h                                                         */
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

#ifndef OS_WINDOWS_H
#define OS_WINDOWS_H

#include "camera_win.h"
#include "context_gl_windows.h"
#include "core/os/input.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "crash_handler_windows.h"
#include "drivers/unix/ip_unix.h"
#include "drivers/wasapi/audio_driver_wasapi.h"
#include "drivers/winmidi/midi_driver_winmidi.h"
#include "key_mapping_windows.h"
#include "main/input_default.h"
#include "power_windows.h"
#include "servers/audio_server.h"
#include "servers/visual/rasterizer.h"
#include "servers/visual_server.h"
#ifdef XAUDIO2_ENABLED
#include "drivers/xaudio2/audio_driver_xaudio2.h"
#endif

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

typedef struct {
    BYTE bWidth; // Width, in pixels, of the image
    BYTE bHeight; // Height, in pixels, of the image
    BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
    BYTE bReserved; // Reserved ( must be 0)
    WORD wPlanes; // Color Planes
    WORD wBitCount; // Bits per pixel
    DWORD dwBytesInRes; // How many bytes in this resource?
    DWORD dwImageOffset; // Where in the file is this image?
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct {
    WORD idReserved; // Reserved (must be 0)
    WORD idType; // Resource Type (1 for icons)
    WORD idCount; // How many images?
    ICONDIRENTRY idEntries[1]; // An entry for each image (idCount of 'em)
} ICONDIR, *LPICONDIR;

class JoypadWindows;
class OS_Windows : public OS {

    enum {
        KEY_EVENT_BUFFER_SIZE = 512
    };

    FILE *stdo;

    struct KeyEvent {

        bool alt, shift, control, meta;
        UINT uMsg;
        WPARAM wParam;
        LPARAM lParam;
    };

    KeyEvent key_event_buffer[KEY_EVENT_BUFFER_SIZE];
    int key_event_pos;

    uint64_t ticks_start;
    uint64_t ticks_per_second;

    bool old_invalid;
    bool outside;
    int old_x, old_y;
    Point2i center;
#if defined(OPENGL_ENABLED)
    ContextGL_Windows *gl_context;
#endif
    VisualServer *visual_server;
    CameraWindows *camera_server;
    int pressrc;
    HDC hDC; // Private GDI Device Context
    HINSTANCE hInstance; // Holds The Instance Of The Application
    HWND hWnd;
    Point2 last_pos;

    HBITMAP hBitmap; //DIB section for layered window
    uint8_t *dib_data;
    Size2 dib_size;
    HDC hDC_dib;
    bool layered_window;

    uint32_t move_timer_id;

    HCURSOR hCursor;

    Size2 min_size;
    Size2 max_size;

    Size2 window_rect;
    VideoMode video_mode;
    bool preserve_window_size = false;

    MainLoop *main_loop;

    WNDPROC user_proc;

    // IME
    HIMC im_himc;
    Vector2 im_position;

    MouseMode mouse_mode;
    bool alt_mem;
    bool gr_mem;
    bool shift_mem;
    bool control_mem;
    bool meta_mem;
    bool force_quit;
    bool window_has_focus;
    uint32_t last_button_state;
    bool use_raw_input;
    bool drop_events;

    HCURSOR cursors[CURSOR_MAX] = { NULL };
    CursorShape cursor_shape;
    Map<CursorShape, Vector<Variant> > cursors_cache;

    InputDefault *input;
    JoypadWindows *joypad;
    Map<int, Vector2> touch_state;

    PowerWindows *power_manager;

    int video_driver_index;
#ifdef WASAPI_ENABLED
    AudioDriverWASAPI driver_wasapi;
#endif
#ifdef XAUDIO2_ENABLED
    AudioDriverXAudio2 driver_xaudio2;
#endif
#ifdef WINMIDI_ENABLED
    MIDIDriverWinMidi driver_midi;
#endif

    CrashHandler crash_handler;

    void _drag_event(float p_x, float p_y, int idx);
    void _touch_event(bool p_pressed, float p_x, float p_y, int idx);

    void _update_window_style(bool repaint = true);

    void _set_mouse_mode_impl(MouseMode p_mode);

    // functions used by main to initialize/deinitialize the OS
protected:
    int get_current_video_driver() const override;

    void initialize_core() override;
    virtual Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver);

    void set_main_loop(MainLoop *p_main_loop) override;
    void delete_main_loop() override;

    void finalize() override;
    void finalize_core() override;

    void process_events();
    void process_key_events();

    struct ProcessInfo {

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
    };
    Map<ProcessID, ProcessInfo> *process_map;

    bool pre_fs_valid;
    RECT pre_fs_rect;
    bool maximized;
    bool minimized;
    bool borderless;
    bool console_visible;

public:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void alert(const String &p_alert, const String &p_title = "ALERT!") override;
    String get_stdin_string(bool p_block) override;

    void set_mouse_mode(MouseMode p_mode) override;
    MouseMode get_mouse_mode() const;

    void warp_mouse_position(const Point2 &p_to) override;
    Point2 get_mouse_position() const override;
    void update_real_mouse_position();
    int get_mouse_button_state() const override;
    void set_window_title(const String &p_title) override;

    virtual void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0);
    virtual VideoMode get_video_mode(int p_screen = 0) const;
    void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const override;

    int get_screen_count() const override;
    int get_current_screen() const override;
    void set_current_screen(int p_screen) override;
    Point2 get_screen_position(int p_screen = -1) const override;
    Size2 get_screen_size(int p_screen = -1) const override;
    int get_screen_dpi(int p_screen = -1) const override;

    Point2 get_window_position() const override;
    virtual void set_window_position(const Point2 &p_position);
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
    void set_console_visible(bool p_enabled) override;
    bool is_console_visible() const override;
    void request_attention() override;

    void set_borderless_window(bool p_borderless) override;
    bool get_borderless_window() override;

    bool get_window_per_pixel_transparency_enabled() const override;
    void set_window_per_pixel_transparency_enabled(bool p_enabled) override;

    uint8_t *get_layered_buffer_data() override;
    Size2 get_layered_buffer_size() override;
    virtual void swap_layered_buffer();

    Error open_dynamic_library(const String &p_path, void *&p_library_handle, bool p_also_set_library_path = false) override;
    Error close_dynamic_library(void *p_library_handle) override;
    Error get_dynamic_library_symbol_handle(void *p_library_handle, const String p_name, void *&p_symbol_handle, bool p_optional = false) override;

    MainLoop *get_main_loop() const override;

    String get_name() const override;

    Date get_date(bool utc) const override;
    Time get_time(bool utc) const override;
    virtual TimeZoneInfo get_time_zone_info() const;
    uint64_t get_unix_time() const override;
    uint64_t get_system_time_secs() const override;
    virtual uint64_t get_system_time_msecs() const;

    virtual bool can_draw() const;
    virtual Error set_cwd(const String &p_cwd);

    void delay_usec(uint32_t p_usec) const override;
    uint64_t get_ticks_usec() const override;

    Error execute(const String &p_path, const ListPOD<String> &p_arguments, bool p_blocking, ProcessID *r_child_id = nullptr, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr) override;
    Error kill(const ProcessID &p_pid) override;
    virtual int get_process_id() const;

    bool has_environment(const String &p_var) const override;
    String get_environment(const String &p_var) const override;
    bool set_environment(const String &p_var, const String &p_value) const override;

    virtual void set_clipboard(const String &p_text);
    String get_clipboard() const override;

    void set_cursor_shape(CursorShape p_shape) override;
    CursorShape get_cursor_shape() const override;
    void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;
    void GetMaskBitmaps(HBITMAP hSourceBitmap, COLORREF clrTransparent, OUT HBITMAP &hAndMaskBitmap, OUT HBITMAP &hXorMaskBitmap);

    void set_native_icon(const String &p_filename) override;
    void set_icon(const Ref<Image> &p_icon) override;

    virtual String get_executable_path() const;

    String get_locale() const override;

    int get_processor_count() const override;

    virtual LatinKeyboardVariant get_latin_keyboard_variant() const;

    virtual void enable_for_stealing_focus(ProcessID pid);
    void move_window_to_foreground() override;

    String get_config_path() const override;
    String get_data_path() const override;
    String get_cache_path() const override;
    String get_godot_dir_name() const override;

    String get_system_dir(SystemDir p_dir) const override;
    String get_user_data_dir() const override;

    String get_unique_id() const override;

    virtual void set_ime_active(const bool p_active);
    void set_ime_position(const Point2 &p_pos) override;

    void release_rendering_thread() override;
    void make_rendering_thread() override;
    void swap_buffers() override;

    Error shell_open(String p_uri) override;

    void run();

    bool get_swap_ok_cancel() override { return true; }

    bool is_joy_known(int p_device) override;
    String get_joy_guid(int p_device) const override;

    virtual void _set_use_vsync(bool p_enable);
    //virtual bool is_vsync_enabled() const;

    virtual OS::PowerState get_power_state();
    int get_power_seconds_left() override;
    int get_power_percent_left() override;

    virtual bool _check_internal_feature_support(const String &p_feature);

    void disable_crash_handler() override;
    bool is_disable_crash_handler() const override;
    void initialize_debugging() override;

    void force_process_input() override;

    Error move_to_trash(const String &p_path) override;

    void process_and_drop_events() override;

    OS_Windows(HINSTANCE _hInstance);
    ~OS_Windows() override;
};

#endif
