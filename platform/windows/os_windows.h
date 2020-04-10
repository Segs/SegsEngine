/*************************************************************************/
/*  os_windows.h                                                         */
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

#pragma once

#include "context_gl_windows.h"
#include "core/os/input.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "crash_handler_windows.h"
#include "drivers/unix/ip_unix.h"
#include "drivers/wasapi/audio_driver_wasapi.h"
#include "drivers/winmidi/midi_driver_winmidi.h"
#include "key_mapping_windows.h"
#include "core/input/input_default.h"
#include "servers/audio_server.h"
#include "servers/rendering/rasterizer.h"
#include "servers/rendering_server.h"
#ifdef XAUDIO2_ENABLED
#include "drivers/xaudio2/audio_driver_xaudio2.h"
#endif

#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

#ifndef POINTER_STRUCTURES

#define POINTER_STRUCTURES

typedef DWORD POINTER_INPUT_TYPE;
typedef UINT32 POINTER_FLAGS;
typedef UINT32 PEN_FLAGS;
typedef UINT32 PEN_MASK;

enum tagPOINTER_INPUT_TYPE {
    PT_POINTER = 0x00000001,
    PT_TOUCH = 0x00000002,
    PT_PEN = 0x00000003,
    PT_MOUSE = 0x00000004,
    PT_TOUCHPAD = 0x00000005
};

typedef enum tagPOINTER_BUTTON_CHANGE_TYPE {
    POINTER_CHANGE_NONE,
    POINTER_CHANGE_FIRSTBUTTON_DOWN,
    POINTER_CHANGE_FIRSTBUTTON_UP,
    POINTER_CHANGE_SECONDBUTTON_DOWN,
    POINTER_CHANGE_SECONDBUTTON_UP,
    POINTER_CHANGE_THIRDBUTTON_DOWN,
    POINTER_CHANGE_THIRDBUTTON_UP,
    POINTER_CHANGE_FOURTHBUTTON_DOWN,
    POINTER_CHANGE_FOURTHBUTTON_UP,
    POINTER_CHANGE_FIFTHBUTTON_DOWN,
    POINTER_CHANGE_FIFTHBUTTON_UP,
} POINTER_BUTTON_CHANGE_TYPE;

typedef struct tagPOINTER_INFO {
    POINTER_INPUT_TYPE pointerType;
    UINT32 pointerId;
    UINT32 frameId;
    POINTER_FLAGS pointerFlags;
    HANDLE sourceDevice;
    HWND hwndTarget;
    POINT ptPixelLocation;
    POINT ptHimetricLocation;
    POINT ptPixelLocationRaw;
    POINT ptHimetricLocationRaw;
    DWORD dwTime;
    UINT32 historyCount;
    INT32 InputData;
    DWORD dwKeyStates;
    UINT64 PerformanceCount;
    POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef struct tagPOINTER_PEN_INFO {
    POINTER_INFO pointerInfo;
    PEN_FLAGS penFlags;
    PEN_MASK penMask;
    UINT32 pressure;
    UINT32 rotation;
    INT32 tiltX;
    INT32 tiltY;
} POINTER_PEN_INFO;

#endif

typedef BOOL(WINAPI *GetPointerTypePtr)(uint32_t p_id, POINTER_INPUT_TYPE *p_type);
typedef BOOL(WINAPI *GetPointerPenInfoPtr)(uint32_t p_id, POINTER_PEN_INFO *p_pen_info);
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
    static GetPointerTypePtr win8p_GetPointerType;
    static GetPointerPenInfoPtr win8p_GetPointerPenInfo;

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
    RenderingServer *rendering_server;
    int pressrc;
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

    void _update_window_style(bool p_repaint = true, bool p_maximized = false);

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
    bool window_focused;
    bool console_visible;
    bool was_maximized;

public:
    LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void alert(StringView p_alert, StringView p_title = "ALERT!") override;
    String get_stdin_string(bool p_block) override;

    void set_mouse_mode(MouseMode p_mode) override;
    MouseMode get_mouse_mode() const;

    void warp_mouse_position(const Point2 &p_to) override;
    Point2 get_mouse_position() const override;
    void update_real_mouse_position();
    int get_mouse_button_state() const override;
    void set_window_title(StringView p_title) override;

    void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0) override;
    VideoMode get_video_mode(int p_screen = 0) const override;
    void get_fullscreen_mode_list(Vector<VideoMode> *p_list, int p_screen = 0) const override;

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
    bool is_window_focused() const override;
    void set_console_visible(bool p_enabled) override;
    bool is_console_visible() const override;
    void request_attention() override;

    void set_borderless_window(bool p_borderless) override;
    bool get_borderless_window() override;

    bool get_window_per_pixel_transparency_enabled() const override;
    void set_window_per_pixel_transparency_enabled(bool p_enabled) override;

    uint8_t *get_layered_buffer_data() override;
    Size2 get_layered_buffer_size() override;
    void swap_layered_buffer() override;

    Error open_dynamic_library(StringView p_path, void *&p_library_handle, bool p_also_set_library_path = false) override;
    Error close_dynamic_library(void *p_library_handle) override;
    Error get_dynamic_library_symbol_handle(void *p_library_handle, StringView p_name, void *&p_symbol_handle, bool p_optional = false) override;

    MainLoop *get_main_loop() const override;

    String get_name() const override;

    Date get_date(bool utc) const override;
    Time get_time(bool utc) const override;
    TimeZoneInfo get_time_zone_info() const override;
    uint64_t get_unix_time() const override;
    uint64_t get_system_time_secs() const override;
    uint64_t get_system_time_msecs() const override;

    bool can_draw() const override;
    Error set_cwd(StringView p_cwd) override;

    void delay_usec(uint32_t p_usec) const override;
    uint64_t get_ticks_usec() const override;

    Error execute(StringView p_path, const Vector<String> &p_arguments, bool p_blocking=true, ProcessID *r_child_id = nullptr, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr) override;
    Error kill(const ProcessID &p_pid) override;
    int get_process_id() const override;

    bool has_environment(StringView p_var) const override;
    String get_environment(StringView p_var) const override;
    bool set_environment(StringView p_var, StringView p_value) const override;

    void set_clipboard(StringView p_text) override;
    String get_clipboard() const override;

    void set_cursor_shape(CursorShape p_shape) override;
    CursorShape get_cursor_shape() const override;
    void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) override;
    void GetMaskBitmaps(HBITMAP hSourceBitmap, COLORREF clrTransparent, OUT HBITMAP &hAndMaskBitmap, OUT HBITMAP &hXorMaskBitmap);

    void set_native_icon(const String &p_filename) override;
    void set_icon(const Ref<Image> &p_icon) override;

    String get_executable_path() const override;

    const char *get_locale() const override;

    int get_processor_count() const override;

    virtual LatinKeyboardVariant get_latin_keyboard_variant() const;

    void enable_for_stealing_focus(ProcessID pid) override;
    void move_window_to_foreground() override;

    String get_godot_dir_name() const override;

    String get_user_data_dir() const override;

    void set_ime_active(const bool p_active) override;
    void set_ime_position(const Point2 &p_pos) override;

    void release_rendering_thread() override;
    void make_rendering_thread() override;
    void swap_buffers() override;

    Error shell_open(StringView p_uri) override;

    void run();

    bool get_swap_ok_cancel() override { return true; }

    bool is_joy_known(int p_device) override;
    StringName get_joy_guid(int p_device) const override;

    void _set_use_vsync(bool p_enable) override;
    //virtual bool is_vsync_enabled() const;

    bool _check_internal_feature_support(StringView p_feature) override;

    void disable_crash_handler() override;
    bool is_disable_crash_handler() const override;
    void initialize_debugging() override;

    void force_process_input() override;

    Error move_to_trash(StringView p_path) override;

    void process_and_drop_events() override;

    OS_Windows(HINSTANCE _hInstance);
    ~OS_Windows() override;
};
