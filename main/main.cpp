/*************************************************************************/
/*  main.cpp                                                             */
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

#include "main.h"

#include "core/class_db.h"
#include "core/crypto/crypto.h"
#include "core/external_profiler.h"
#include "core/input_map.h"
#include "core/io/file_access_network.h"
#include "core/io/file_access_pack.h"

#include "core/io/image_loader.h"
#include "core/io/ip.h"
#include "plugins/plugin_registry_interface.h"
#include "core/io/resource_loader.h"
#include "core/message_queue.h"
#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/register_core_types.h"
#include "core/script_debugger_local.h"
#include "core/script_language.h"
#include "core/translation.h"
#include "core/rotated_file_loger.h"
#include "core/version.h"
#include "core/version_hash.gen.h"
#include "drivers/register_driver_types.h"
#include "main/app_icon.gen.h"
#include "main/input_default.h"
#include "main/main_timer_sync.h"
#include "main/performance.h"
#include "main/splash.gen.h"
#include "main/splash_editor.gen.h"
#include "main/tests/test_main.h"
#include "modules/register_module_types.h"

#include "scene/debugger/script_debugger_remote.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/register_scene_types.h"
#include "scene/resources/packed_scene.h"
#include "servers/arvr_server.h"
#include "servers/audio_server.h"
#include "servers/camera_server.h"
#include "servers/navigation_server.h"
#include "servers/navigation_2d_server.h"
#include "servers/physics_2d_server.h"
#include "servers/physics_server.h"
#include "core/string_formatter.h"
#include "servers/register_server_types.h"


#ifdef TOOLS_ENABLED
#include "editor/doc/doc_data.h"
#include "editor/doc_data_class_path.gen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/project_manager.h"
#include "editor/progress_dialog.h"
#endif

#include <QCoreApplication>
/* Static members */

// Singletons

// Initialized in setup()
static Engine *engine = nullptr;

static ProjectSettings *globals = nullptr;
static InputMap *input_map = nullptr;
static TranslationServer *translation_server = nullptr;
static Performance *performance = nullptr;

static PackedData *packed_data = nullptr;
static FileAccessNetworkClient *file_access_network_client = nullptr;
static ScriptDebugger *script_debugger = nullptr;
static MessageQueue *message_queue = nullptr;

// Initialized in setup2()
static AudioServer *audio_server = nullptr;
static CameraServer *camera_server = nullptr;
static ARVRServer *arvr_server = nullptr;
static PhysicsServer *physics_server = nullptr;
static Physics2DServer *physics_2d_server = nullptr;
static NavigationServer *navigation_server = nullptr;
static Navigation2DServer *navigation_2d_server = nullptr;
// We error out if setup2() doesn't turn this true
static bool _start_success = false;

// Drivers

static int video_driver_idx = -1;
static int audio_driver_idx = -1;

// Engine config/tools

static bool editor = false;
static bool project_manager = false;
static String locale;
static bool show_help = false;
static bool auto_quit = false;
static OS::ProcessID allow_focus_steal_pid = 0;
#ifdef TOOLS_ENABLED
static bool auto_build_solutions = false;
#endif

// Display

static OS::VideoMode video_mode;
static int init_screen = -1;
static bool init_fullscreen = false;
static bool init_maximized = false;
static bool init_windowed = false;
static bool init_always_on_top = false;
static bool init_use_custom_pos = false;
static Vector2 init_custom_pos;
static bool force_lowdpi = false;

// Debug

static bool use_debug_profiler = false;
#ifdef DEBUG_ENABLED
static bool debug_collisions = false;
static bool debug_navigation = false;
#endif
static int frame_delay = 0;
static bool disable_render_loop = false;
static int fixed_fps = -1;
static bool print_fps = false;

/* Helper methods */

// Used by Mono module, should likely be registered in Engine singleton instead
// FIXME: This is also not 100% accurate, `project_manager` is only true when it was requested,
// but not if e.g. we fail to load and project and fallback to the manager.

bool Main::is_project_manager() {
    return project_manager;
}

static String unescape_cmdline(se_string_view p_str) {
    return String(p_str).replaced("%20", " ");
}

static String get_full_version_string() {
    String hash(VERSION_HASH);
    if (!hash.empty())
        hash = "." + StringUtils::left(hash,9);
    return String(VERSION_FULL_BUILD) + hash;
}

// FIXME: Could maybe be moved to PhysicsServerManager and Physics2DServerManager directly
// to have less code in main.cpp.
void initialize_physics() {

    /// 3D Physics Server
    physics_server = PhysicsServerManager::new_server(ProjectSettings::get_singleton()->get(PhysicsServerManager::setting_property_name));
    if (!physics_server) {
        // Physics server not found, Use the default physics
        physics_server = PhysicsServerManager::new_default_server();
    }
    ERR_FAIL_COND(!physics_server);
    physics_server->init();

    /// 2D Physics server
    physics_2d_server = Physics2DServerManager::new_server(ProjectSettings::get_singleton()->get(Physics2DServerManager::setting_property_name));
    if (!physics_2d_server) {
        // Physics server not found, Use the default physics
        physics_2d_server = Physics2DServerManager::new_default_server();
    }
    ERR_FAIL_COND(!physics_2d_server);
    physics_2d_server->init();
}

void finalize_physics() {
    physics_server->finish();
    memdelete(physics_server);

    physics_2d_server->finish();
    memdelete(physics_2d_server);

    Physics2DServerManager::cleanup();
    PhysicsServerManager::cleanup();
}
void initialize_navigation_server() {
    ERR_FAIL_COND(navigation_server != NULL);
    navigation_server = NavigationServerManager::new_default_server();
    Navigation2DServer::initialize_class();
    navigation_2d_server = memnew(Navigation2DServer);
}

void finalize_navigation_server() {
    memdelete(navigation_server);
    navigation_server = NULL;
    memdelete(navigation_2d_server);
    navigation_2d_server = NULL;
}

//#define DEBUG_INIT
#ifdef DEBUG_INIT
#define MAIN_PRINT(m_txt) print_line(m_txt)
#else
#define MAIN_PRINT(m_txt)
#endif

void Main::print_help(const String &p_binary) {

    print_line(String(VERSION_NAME) + " v" + get_full_version_string() + " - " + String(VERSION_WEBSITE));
    OS::get_singleton()->print("Free and open source software under the terms of the MIT license.\n");
    OS::get_singleton()->print("(c) 2007-2019 Juan Linietsky, Ariel Manzur.\n");
    OS::get_singleton()->print("(c) 2014-2019 Godot Engine contributors.\n");
    OS::get_singleton()->print("\n");
    OS::get_singleton()->print(FormatVE("Usage: %s [options] [path to scene or 'project.godot' file]\n", p_binary.c_str()));
    OS::get_singleton()->print("\n");

    OS::get_singleton()->print("General options:\n");
    OS::get_singleton()->print("  -h, --help                       Display this help message.\n");
    OS::get_singleton()->print("  --version                        Display the version string.\n");
    OS::get_singleton()->print("  -v, --verbose                    Use verbose stdout mode.\n");
    OS::get_singleton()->print("  --quiet                          Quiet mode, silences stdout messages. Errors are still displayed.\n");
    OS::get_singleton()->print("\n");

    OS::get_singleton()->print("Run options:\n");
#ifdef TOOLS_ENABLED
    OS::get_singleton()->print("  -e, --editor                     Start the editor instead of running the scene.\n");
    OS::get_singleton()->print("  -p, --project-manager            Start the project manager, even if a project is auto-detected.\n");
#endif
    OS::get_singleton()->print("  -q, --quit                       Quit after the first iteration.\n");
    OS::get_singleton()->print("  -l, --language <locale>          Use a specific locale (<locale> being a two-letter code).\n");
    OS::get_singleton()->print("  --path <directory>               Path to a project (<directory> must contain a 'project.godot' file).\n");
    OS::get_singleton()->print("  -u, --upwards                    Scan folders upwards for project.godot file.\n");
    OS::get_singleton()->print("  --main-pack <file>               Path to a pack (.pck) file to load.\n");
    OS::get_singleton()->print("  --render-thread <mode>           Render thread mode ('unsafe', 'safe', 'separate').\n");
    OS::get_singleton()->print("  --remote-fs <address>            Remote filesystem (<host/IP>[:<port>] address).\n");
    OS::get_singleton()->print("  --remote-fs-password <password>  Password for remote filesystem.\n");
    OS::get_singleton()->print("  --audio-driver <driver>          Audio driver (");
    for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {
        if (i != 0)
            OS::get_singleton()->print(", ");
        OS::get_singleton()->print(FormatVE("'%s'", OS::get_singleton()->get_audio_driver_name(i)));
    }
    OS::get_singleton()->print(").\n");
    OS::get_singleton()->print("  --video-driver <driver>          Video driver (");
    for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {
        if (i != 0)
            OS::get_singleton()->print(", ");
        OS::get_singleton()->print(FormatVE("'%s'", OS::get_singleton()->get_video_driver_name(i)));
    }
    OS::get_singleton()->print(").\n");
    OS::get_singleton()->print("\n");

#ifndef SERVER_ENABLED
    OS::get_singleton()->print("Display options:\n");
    OS::get_singleton()->print("  -f, --fullscreen                 Request fullscreen mode.\n");
    OS::get_singleton()->print("  -m, --maximized                  Request a maximized window.\n");
    OS::get_singleton()->print("  -w, --windowed                   Request windowed mode.\n");
    OS::get_singleton()->print("  -t, --always-on-top              Request an always-on-top window.\n");
    OS::get_singleton()->print("  --resolution <W>x<H>             Request window resolution.\n");
    OS::get_singleton()->print("  --position <X>,<Y>               Request window position.\n");
    OS::get_singleton()->print("  --low-dpi                        Force low-DPI mode (macOS and Windows only).\n");
    OS::get_singleton()->print("  --no-window                      Disable window creation (Windows only). Useful together with --script.\n");
    OS::get_singleton()->print("  --enable-vsync-via-compositor    When vsync is enabled, vsync via the OS' window compositor (Windows only).\n");
    OS::get_singleton()->print("  --disable-vsync-via-compositor   Disable vsync via the OS' window compositor (Windows only).\n");
    OS::get_singleton()->print("\n");
#endif

    OS::get_singleton()->print("Debug options:\n");
    OS::get_singleton()->print("  -d, --debug                      Debug (local stdout debugger).\n");
    OS::get_singleton()->print("  -b, --breakpoints                Breakpoint list as source::line comma-separated pairs, no spaces (use %%20 instead).\n");
    OS::get_singleton()->print("  --profiling                      Enable profiling in the script debugger.\n");
    OS::get_singleton()->print("  --remote-debug <address>         Remote debug (<host/IP>:<port> address).\n");
#if defined(DEBUG_ENABLED) && !defined(SERVER_ENABLED)
    OS::get_singleton()->print("  --debug-collisions               Show collision shapes when running the scene.\n");
    OS::get_singleton()->print("  --debug-navigation               Show navigation polygons when running the scene.\n");
#endif
    OS::get_singleton()->print("  --frame-delay <ms>               Simulate high CPU load (delay each frame by <ms> milliseconds).\n");
    OS::get_singleton()->print("  --time-scale <scale>             Force time scale (higher values are faster, 1.0 is normal speed).\n");
    OS::get_singleton()->print("  --disable-render-loop            Disable render loop so rendering only occurs when called explicitly from script.\n");
    OS::get_singleton()->print("  --disable-crash-handler          Disable crash handler when supported by the platform code.\n");
    OS::get_singleton()->print("  --fixed-fps <fps>                Force a fixed number of frames per second. This setting disables real-time synchronization.\n");
    OS::get_singleton()->print("  --print-fps                      Print the frames per second to the stdout.\n");
    OS::get_singleton()->print("\n");

    OS::get_singleton()->print("Standalone tools:\n");
    OS::get_singleton()->print("  -s, --script <script>            Run a script.\n");
    OS::get_singleton()->print("  --check-only                     Only parse for errors and quit (use with --script).\n");
#ifdef TOOLS_ENABLED
    OS::get_singleton()->print("  --export <preset> <path>         Export the project using the given preset and matching release template. The preset name should match one defined in export_presets.cfg.\n");
    OS::get_singleton()->print("                                   <path> should be absolute or relative to the project directory, and include the filename for the binary (e.g. 'builds/game.exe'). The target directory should exist.\n");
    OS::get_singleton()->print("  --export-debug <preset> <path>   Same as --export, but using the debug template.\n");
    OS::get_singleton()->print("  --export-pack <preset> <path>    Same as --export, but only export the game pack for the given preset. The <path> extension determines whether it will be in PCK or ZIP format.\n");
    OS::get_singleton()->print("  --doctool <path>                 Dump the engine API reference to the given <path> in XML format, merging if existing files are found.\n");
    OS::get_singleton()->print("  --no-docbase                     Disallow dumping the base types (used with --doctool).\n");
    OS::get_singleton()->print("  --build-solutions                Build the scripting solutions (e.g. for C# projects). Implies --editor and requires a valid project to edit.\n");
#ifdef DEBUG_METHODS_ENABLED
    OS::get_singleton()->print("  --gdnative-generate-json-api     Generate JSON dump of the Godot API for GDNative bindings.\n");
#endif
    OS::get_singleton()->print("  --test <test>                    Run a unit test (");
    const char **test_names = tests_get_names();
    const char *comma = "";
    while (*test_names) {
        OS::get_singleton()->print(FormatVE("%s'%s'", comma, *test_names));
        test_names++;
        comma = ", ";
    }
    OS::get_singleton()->print(").\n");
#endif
}
#include "core/plugin_interfaces/PluginDeclarations.h"
struct ArchivePluginResolver : public ResolverInterface
{
    PackedData *pack_data;
    ArchivePluginResolver(PackedData *pd) : pack_data(pd) {}

    bool new_plugin_detected(QObject * ob) override {
        bool res=false;
        auto interface = qobject_cast<PackSourceInterface *>(ob);
        if(interface) {
            print_line(String("Adding archive plugin:")+ob->metaObject()->className());
            pack_data->add_pack_source(interface);
            res=true;
        }
        return res;
    }
    void plugin_removed(QObject * ob)  override  {
        auto interface = qobject_cast<PackSourceInterface *>(ob);
        if(interface) {
            print_line(String("Removing archive plugin:")+ob->metaObject()->className());
            pack_data->remove_pack_source(interface);
        }
    }
};
struct ResourcePluginResolver : public ResolverInterface
{
    bool new_plugin_detected(QObject * ob) override {
        bool res=false;
        auto interface = qobject_cast<ResourceLoaderInterface *>(ob);
        if(interface) {
            print_line(String("Adding resource loader plugin:")+ob->metaObject()->className());
            ResourceLoader::add_resource_format_loader(interface);
            res=true;
        }
        return res;
    }
    void plugin_removed(QObject * ob)  override  {
        auto interface = qobject_cast<ResourceLoaderInterface *>(ob);
        if(interface) {
            print_line(String("Removing resource loader plugin:")+ob->metaObject()->className());
            ResourceLoader::remove_resource_format_loader(interface);
        }
    }
};
/* Engine initialization
 *
 * Consists of several methods that are called by each platform's specific main(argc, argv).
 * To fully understand engine init, one should therefore start from the platform's main and
 * see how it calls into the Main class' methods.
 *
 * The initialization is typically done in 3 steps (with the setup2 step triggered either
 * automatically by setup, or manually in the platform's main).
 *
 * - setup(p_second_phase) is the main entry point for all platforms,
 *   responsible for the initialization of all low level singletons and core types, and parsing
 *   command line arguments to configure things accordingly.
 *   If p_second_phase is true, it will chain into setup2() (default behaviour). This is
 *   disabled on some platforms (Android, iOS, UWP) which trigger the second step in their
 *   own time.
 *
 * - setup2(p_main_tid_override) registers high level servers and singletons, displays the
 *   boot splash, then registers higher level types (scene, editor, etc.).
 *
 * - start() is the last step and that's where command line tools can run, or the main loop
 *   can be created eventually and the project settings put into action. That's also where
 *   the editor node is created, if relevant.
 *   start() does it own argument parsing for a subset of the command line arguments described
 *   in help, it's a bit messy and should be globalized with the setup() parsing somehow.
 */
Error Main::setup(bool p_second_phase) {
    RID_OwnerBase::init_rid();

#ifdef TOOLS_ENABLED
    OS::register_feature("editor");
#else
    OS::register_feature("standalone");
#endif
#ifdef DEBUG_ENABLED
    OS::register_feature("debug");
#else
    OS::register_feature("release");
#endif

    OS::get_singleton()->initialize_core();
    engine = memnew(Engine);

    ClassDB::init();

    MAIN_PRINT("Main: Initialize CORE");

    register_core_types();
    register_core_driver_types();

    MAIN_PRINT("Main: Initialize Globals");

    Thread::_main_thread_id = Thread::get_caller_id();
    ProjectSettings::initialize_class();
    InputMap::initialize_class();
    TranslationServer::initialize_class();
    Performance::initialize_class();

    globals = memnew(ProjectSettings);
    input_map = memnew(InputMap);

    register_core_settings(); //here globals is present

    translation_server = memnew(TranslationServer);
    performance = memnew(Performance);
    ClassDB::register_class<Performance>();
    engine->add_singleton(Engine::Singleton("Performance", performance));

    GLOBAL_DEF(StringName("debug/settings/crash_handler/message"), String("Please include this when reporting the bug on https://github.com/godotengine/godot/issues"));

    MAIN_PRINT("Main: Parse CMDLine");

    /* argument parsing and main creation */
    ListPOD<String> args;
    ListPOD<String> main_args;
    QStringList q_args = qApp->arguments();
    String execpath = StringUtils::to_utf8(q_args.takeFirst());

    for (const QString &arg : q_args) {
        args.push_back(StringUtils::to_utf8(arg));
    }

    ListPOD<String>::iterator I = args.begin();

    for(String &a : args) {

        a = unescape_cmdline(StringUtils::strip_edges(a));
    }

    StringName video_driver;
    StringName audio_driver;
    String project_path(".");
    bool upwards = false;
    String debug_mode;
    String debug_host;
    bool skip_breakpoints = false;
    String main_pack;
    bool quiet_stdout = false;
    int rtm = -1;

    String remotefs;
    String remotefs_pass;

    PODVector<se_string_view> breakpoints;
    bool use_custom_res = true;
    bool force_res = false;
    bool saw_vsync_via_compositor_override = false;
#ifdef TOOLS_ENABLED
    bool found_project = false;
#endif

    packed_data = PackedData::get_singleton();
    if (!packed_data)
        packed_data = memnew(PackedData);

    add_plugin_resolver(new ArchivePluginResolver(packed_data));

    I = args.begin();
    while (I!= args.end()) {

        ListPOD<String>::iterator N = eastl::next(I);

        if (*I == "-h" || *I == "--help" || *I == "/?") { // display help

            show_help = true;
            goto error;

        } else if (*I == "--version") {

            print_line(get_full_version_string());
            goto error;

        } else if (*I == "-v" || *I == "--verbose") { // verbose output

            OS::get_singleton()->_verbose_stdout = true;
        } else if (*I == "--quiet") { // quieter output

            quiet_stdout = true;

        } else if (*I == "--audio-driver") { // audio driver

            if (N != args.end()) {

                audio_driver = StringName(*N);
                bool found = false;
                for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {
                    if (audio_driver == OS::get_singleton()->get_audio_driver_name(i)) {
                        found = true;
                    }
                }

                if (!found) {
                    OS::get_singleton()->print(FormatVE("Unknown audio driver '%s', aborting.\nValid options are ", audio_driver.asCString()));

                    for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {
                        if (i == OS::get_singleton()->get_audio_driver_count() - 1) {
                            OS::get_singleton()->print(" and ");
                        } else if (i != 0) {
                            OS::get_singleton()->print(", ");
                        }

                        OS::get_singleton()->print(FormatVE("'%s'",OS::get_singleton()->get_audio_driver_name(i)));
                    }

                    OS::get_singleton()->print(".\n");

                    goto error;
                }
                ++N;
            } else {
                OS::get_singleton()->print("Missing audio driver argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--video-driver") { // force video driver

            if (N != args.end()) {

                video_driver = StringName(*N);
                bool found = false;
                for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {
                    if (video_driver == OS::get_singleton()->get_video_driver_name(i)) {
                        found = true;
                    }
                }

                if (!found) {
                    OS::get_singleton()->print(FormatVE("Unknown video driver '%s', aborting.\nValid options are ", video_driver.asCString()));

                    for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {
                        if (i == OS::get_singleton()->get_video_driver_count() - 1) {
                            OS::get_singleton()->print(" and ");
                        } else if (i != 0) {
                            OS::get_singleton()->print(", ");
                        }

                        OS::get_singleton()->print(FormatVE("'%s'", OS::get_singleton()->get_video_driver_name(i)));
                    }

                    OS::get_singleton()->print(".\n");

                    goto error;
                }
                ++N;
            } else {
                OS::get_singleton()->print("Missing video driver argument, aborting.\n");
                goto error;
            }
#ifndef SERVER_ENABLED
        } else if (*I == "-f" || *I == "--fullscreen") { // force fullscreen

            init_fullscreen = true;
        } else if (*I == "-m" || *I == "--maximized") { // force maximized window

            init_maximized = true;
            video_mode.maximized = true;

        } else if (*I == "-w" || *I == "--windowed") { // force windowed window

            init_windowed = true;
        } else if (*I == "-t" || *I == "--always-on-top") { // force always-on-top window

            init_always_on_top = true;
        } else if (*I == "--resolution") { // force resolution

            if (N != args.end()) {

                String vm = *N;

                if (not StringUtils::contains(vm,'x')) { // invalid parameter format

                    OS::get_singleton()->print("Invalid resolution '"+vm+"', it should be e.g. '1280x720'.\n");
                    goto error;
                }

                int w = StringUtils::to_int(StringUtils::get_slice(vm,"x", 0));
                int h = StringUtils::to_int(StringUtils::get_slice(vm,"x", 1));

                if (w <= 0 || h <= 0) {

                    OS::get_singleton()->print("Invalid resolution '"+vm+"', width and height must be above 0.\n");
                    goto error;
                }

                video_mode.width = w;
                video_mode.height = h;
                force_res = true;

                ++N;
            } else {
                OS::get_singleton()->print("Missing resolution argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--position") { // set window position

            if (N != args.end()) {

                String vm = *N;

                if (not StringUtils::contains(vm,',')) { // invalid parameter format

                    OS::get_singleton()->print("Invalid position '"+vm+"', it should be e.g. '80,128'.\n");
                    goto error;
                }

                int x = StringUtils::to_int(StringUtils::get_slice(vm,",", 0));
                int y = StringUtils::to_int(StringUtils::get_slice(vm,",", 1));

                init_custom_pos = Point2(x, y);
                init_use_custom_pos = true;

                ++N;
            } else {
                OS::get_singleton()->print("Missing position argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--low-dpi") { // force low DPI (macOS only)

            force_lowdpi = true;
        } else if (*I == "--no-window") { // disable window creation (Windows only)

            OS::get_singleton()->set_no_window_mode(true);
        } else if (*I == "--enable-vsync-via-compositor") {

            video_mode.vsync_via_compositor = true;
            saw_vsync_via_compositor_override = true;
        } else if (*I == "--disable-vsync-via-compositor") {

            video_mode.vsync_via_compositor = false;
            saw_vsync_via_compositor_override = true;
#endif
        } else if (*I == "--profiling") { // enable profiling

            use_debug_profiler = true;

        } else if (*I == "-l" || *I == "--language") { // language

            if (N != args.end()) {

                locale = *N;
                ++N;
            } else {
                OS::get_singleton()->print("Missing language argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--remote-fs") { // remote filesystem

            if (N != args.end()) {

                remotefs = (*N);
                ++N;
            } else {
                OS::get_singleton()->print("Missing remote filesystem address, aborting.\n");
                goto error;
            }
        } else if (*I == "--remote-fs-password") { // remote filesystem password

            if (N != args.end()) {

                remotefs_pass = *N;
                ++N;
            } else {
                OS::get_singleton()->print("Missing remote filesystem password, aborting.\n");
                goto error;
            }
        } else if (*I == "--render-thread") { // render thread mode

            if (N != args.end()) {

                if (*N == "safe")
                    rtm = OS::RENDER_THREAD_SAFE;
                else if (*N == "unsafe")
                    rtm = OS::RENDER_THREAD_UNSAFE;
                else if (*N == "separate")
                    rtm = OS::RENDER_SEPARATE_THREAD;

                ++N;
            } else {
                OS::get_singleton()->print("Missing render thread mode argument, aborting.\n");
                goto error;
            }

#ifdef TOOLS_ENABLED
        } else if (*I == "-e" || *I == "--editor") { // starts editor

            editor = true;
        } else if (*I == "-p" || *I == "--project-manager") { // starts project manager

            project_manager = true;
        } else if (*I == "--build-solutions") { // Build the scripting solution such C#

            auto_build_solutions = true;
            editor = true;
#ifdef DEBUG_METHODS_ENABLED
        } else if (*I == "--gdnative-generate-json-api") {
            // Register as an editor instance to use the GLES2 fallback automatically on hardware that doesn't support the GLES3 backend
            editor = true;

            // We still pass it to the main arguments since the argument handling itself is not done in this function
            main_args.push_back(*I);
#endif
        } else if (*I == "--export" || *I == "--export-debug" || *I == "--export-pack") { // Export project

            editor = true;
            main_args.push_back(*I);
#endif
        } else if (*I == "--path") { // set path of project to start or edit

            if (N != args.end()) {

                String p = *N;
                if (OS::get_singleton()->set_cwd(p) == OK) {
                    //nothing
                } else {
                    project_path = *N; //use project_path instead
                }
                ++N;
            } else {
                OS::get_singleton()->print("Missing relative or absolute path, aborting.\n");
                goto error;
            }
        } else if (*I == "-u" || *I == "--upwards") { // scan folders upwards
            upwards = true;
        } else if (*I == "-q" || *I == "--quit") { // Auto quit at the end of the first main loop iteration
            auto_quit = true;
        } else if (StringUtils::ends_with(*I,"project.godot")) {
            String path;
            String file = *I;

            path = PathUtils::path(file);

            if (OS::get_singleton()->set_cwd(path) == OK) {
                // path already specified, don't override
            } else {
                project_path = path;
            }
#ifdef TOOLS_ENABLED
            //editor = true;
#endif
        } else if (*I == "-b" || *I == "--breakpoints") { // add breakpoints

            if (N != args.end()) {

                String bplist = *N;
                breakpoints = StringUtils::split(bplist,',');
                ++N;
            } else {
                OS::get_singleton()->print("Missing list of breakpoints, aborting.\n");
                goto error;
            }

        } else if (*I == "--frame-delay") { // force frame delay

            if (N != args.end()) {

                frame_delay = StringUtils::to_int(*N);
                ++N;
            } else {
                OS::get_singleton()->print("Missing frame delay argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--time-scale") { // force time scale

            if (N != args.end()) {

                Engine::get_singleton()->set_time_scale(StringUtils::to_float(*N));
                ++N;
            } else {
                OS::get_singleton()->print("Missing time scale argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--main-pack") {

            if (N != args.end()) {

                main_pack = *N;
                ++N;
            } else {
                OS::get_singleton()->print("Missing path to main pack file, aborting.\n");
                goto error;
            }

        } else if (*I == "-d" || *I == "--debug") {
            debug_mode = "local";
#if defined(DEBUG_ENABLED) && !defined(SERVER_ENABLED)
        } else if (*I == "--debug-collisions") {
            debug_collisions = true;
        } else if (*I == "--debug-navigation") {
            debug_navigation = true;
#endif
        } else if (*I == "--remote-debug") {
            if (N != args.end()) {

                debug_mode = "remote";
                debug_host = *N;
                if (not StringUtils::contains(debug_host,':')) { // wrong address
                    OS::get_singleton()->print("Invalid debug host address, it should be of the form <host/IP>:<port>.\n");
                    goto error;
                }
                ++N;
            } else {
                OS::get_singleton()->print("Missing remote debug host address, aborting.\n");
                goto error;
            }
        } else if (*I == "--allow_focus_steal_pid") { // not exposed to user
            if (N != args.end()) {

                allow_focus_steal_pid = StringUtils::to_int64(*N);
                ++N;
            } else {
                OS::get_singleton()->print("Missing editor PID argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--disable-render-loop") {
            disable_render_loop = true;
        } else if (*I == "--fixed-fps") {
            if (N != args.end()) {
                fixed_fps = StringUtils::to_int(*N);
                ++N;
            } else {
                OS::get_singleton()->print("Missing fixed-fps argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--print-fps") {
            print_fps = true;
        } else if (*I == "--disable-crash-handler") {
            OS::get_singleton()->disable_crash_handler();
        } else if (*I == "--skip-breakpoints") {
            skip_breakpoints = true;
        } else {
            main_args.push_back(*I);
        }

        I = N;
    }
#ifdef TOOLS_ENABLED
    if (editor && project_manager) {
        OS::get_singleton()->print("Error: Command line arguments implied opening both editor and project manager, which is not possible. Aborting.\n");
        goto error;
    }
#endif
    // Network file system needs to be configured before globals, since globals are based on the
    // 'project.godot' file which will only be available through the network if this is enabled
    FileAccessNetwork::configure();
    if (!remotefs.empty()) {

        file_access_network_client = memnew(FileAccessNetworkClient);
        int port;
        if (StringUtils::contains(remotefs,':')) {
            port = StringUtils::to_int(StringUtils::get_slice(remotefs,':', 1));
            remotefs = StringUtils::get_slice(remotefs,':', 0);
        } else {
            port = 6010;
        }

        Error err = file_access_network_client->connect(remotefs, port, remotefs_pass);
        if (err) {
            OS::get_singleton()->printerr(("Could not connect to remotefs: "+remotefs+":"+::to_string(port)+".\n").c_str());
            goto error;
        }

        FileAccess::make_default<FileAccessNetwork>(FileAccess::ACCESS_RESOURCES);
    }
    if (globals->setup(project_path, main_pack, upwards) == OK) {
#ifdef TOOLS_ENABLED
        found_project = true;
#endif
    } else {

#ifdef TOOLS_ENABLED
        editor = false;
#else
        const String error_msg = "Error: Couldn't load project data at path \"" + project_path + "\". Is the .pck file missing?\nIf you've renamed the executable, the associated .pck file should also be renamed to match the executable's name (without the extension).\n";
        OS::get_singleton()->print("%s", error_msg.ascii().get_data());
        OS::get_singleton()->alert(error_msg);

        goto error;
#endif
    }

    GLOBAL_DEF("memory/limits/multithreaded_server/rid_pool_prealloc", 60);
    ProjectSettings::get_singleton()->set_custom_property_info("memory/limits/multithreaded_server/rid_pool_prealloc",
            PropertyInfo(VariantType::INT, "memory/limits/multithreaded_server/rid_pool_prealloc", PropertyHint::Range,
                    "0,500,1")); // No negative and limit to 500 due to crashes
    GLOBAL_DEF("network/limits/debugger_stdout/max_chars_per_second", 2048);
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/debugger_stdout/max_chars_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_chars_per_second", PropertyHint::Range,
                    "0, 4096, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_messages_per_frame", 10);
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/debugger_stdout/max_messages_per_frame",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_messages_per_frame", PropertyHint::Range,
                    "0, 20, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_errors_per_second", 100);
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/debugger_stdout/max_errors_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_errors_per_second", PropertyHint::Range,
                    "0, 200, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_warnings_per_second", 100);
    ProjectSettings::get_singleton()->set_custom_property_info("network/limits/debugger_stdout/max_warnings_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_warnings_per_second", PropertyHint::Range,
                    "0, 200, 1, or_greater"));

    if (debug_mode == "remote") {

        ScriptDebuggerRemote *sdr = memnew(ScriptDebuggerRemote);
        uint16_t debug_port = 6007;
        if (StringUtils::contains(debug_host,':')) {
            int sep_pos = StringUtils::find_last(debug_host,":");
            debug_port = StringUtils::to_int(StringUtils::substr(debug_host,sep_pos + 1, debug_host.length()));
            debug_host = StringUtils::substr(debug_host,0, sep_pos);
        }
        Error derr = sdr->connect_to_host(debug_host, debug_port);

        sdr->set_skip_breakpoints(skip_breakpoints);

        if (derr != OK) {
            memdelete(sdr);
        } else {
            script_debugger = sdr;
        }
    } else if (debug_mode == "local") {

        script_debugger = memnew(ScriptDebuggerLocal);
        OS::get_singleton()->initialize_debugging();
    }

    if (script_debugger) {
        //there is a debugger, parse breakpoints

        for (se_string_view bp : breakpoints) {

            auto sp = StringUtils::find_last(bp,':');
            ERR_CONTINUE_MSG(sp == String::npos, "Invalid breakpoint: '" + bp + "', expected file:line format.");

            script_debugger->insert_breakpoint(StringUtils::to_int(StringUtils::substr(bp,sp + 1, bp.length())), StringName(StringUtils::substr(bp,0, sp)));
        }
    }

#ifdef TOOLS_ENABLED
    if (editor) {
        packed_data->set_disabled(true);
        globals->set_disable_feature_overrides(true);
    }

#endif

    GLOBAL_DEF("logging/file_logging/enable_file_logging", false);
    GLOBAL_DEF("logging/file_logging/log_path", "user://logs/log.txt");
    GLOBAL_DEF("logging/file_logging/max_log_files", 10);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "logging/file_logging/max_log_files", PropertyInfo(VariantType::INT, "logging/file_logging/max_log_files",
                                                          PropertyHint::Range, "0,20,1,or_greater")); // no negative numbers
    if (FileAccess::get_create_func(FileAccess::ACCESS_USERDATA) &&
            GLOBAL_GET("logging/file_logging/enable_file_logging")) {
        String base_path = GLOBAL_GET("logging/file_logging/log_path");
        int max_files = GLOBAL_GET("logging/file_logging/max_log_files");
        OS::get_singleton()->add_logger(memnew(RotatedFileLogger(base_path, max_files)));
    }

#ifdef TOOLS_ENABLED
    if (editor) {
        Engine::get_singleton()->set_editor_hint(true);
        main_args.push_back("--editor");
        if (!init_windowed) {
            init_maximized = true;
            video_mode.maximized = true;
        }
    }

    if (!project_manager && !editor) {
        // Determine if the project manager should be requested
        project_manager = main_args.empty() && !found_project;
    }
#endif

    if (main_args.empty() && String(GLOBAL_DEF("application/run/main_scene", "")).empty()) {
#ifdef TOOLS_ENABLED
        if (!editor && !project_manager) {
#endif
            OS::get_singleton()->print("Error: Can't run project: no main scene defined.\n");
            goto error;
#ifdef TOOLS_ENABLED
        }
#endif
    }

    if (editor || project_manager) {
        Engine::get_singleton()->set_editor_hint(true);
        use_custom_res = false;
        input_map->load_default(); //keys for editor
    } else {
        input_map->load_from_globals(); //keys for game
    }

    if (bool(ProjectSettings::get_singleton()->get("application/run/disable_stdout"))) {
        quiet_stdout = true;
    }
    if (bool(ProjectSettings::get_singleton()->get("application/run/disable_stderr"))) {
        _print_error_enabled = false;
    }

    if (quiet_stdout)
        _print_line_enabled = false;

    OS::get_singleton()->set_cmdline(execpath, main_args);

    GLOBAL_DEF("rendering/quality/driver/driver_name", "GLES3");
    ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/driver/driver_name",
            PropertyInfo(VariantType::STRING, "rendering/quality/driver/driver_name", PropertyHint::Enum, "GLES2,GLES3"));
    if (video_driver.empty()) {
        video_driver = GLOBAL_GET("rendering/quality/driver/driver_name");
    }

    GLOBAL_DEF("rendering/quality/driver/fallback_to_gles2", false);

    // Assigning here even though it's GLES2-specific, to be sure that it appears in docs
    GLOBAL_DEF("rendering/quality/2d/gles2_use_nvidia_rect_flicker_workaround", false);
    GLOBAL_DEF("display/window/size/width", 1024);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "display/window/size/width", PropertyInfo(VariantType::INT, "display/window/size/width", PropertyHint::Range,
                                                 "0,7680,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/height", 600);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "display/window/size/height", PropertyInfo(VariantType::INT, "display/window/size/height", PropertyHint::Range,
                                                  "0,4320,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/resizable", true);
    GLOBAL_DEF("display/window/size/borderless", false);
    GLOBAL_DEF("display/window/size/fullscreen", false);
    GLOBAL_DEF("display/window/size/always_on_top", false);
    GLOBAL_DEF("display/window/size/test_width", 0);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "display/window/size/test_width", PropertyInfo(VariantType::INT, "display/window/size/test_width",
                                                      PropertyHint::Range, "0,7680,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/test_height", 0);
    ProjectSettings::get_singleton()->set_custom_property_info(
            "display/window/size/test_height", PropertyInfo(VariantType::INT, "display/window/size/test_height",
                                                       PropertyHint::Range, "0,4320,or_greater")); // 8K resolution

    if (use_custom_res) {

        if (!force_res) {
            video_mode.width = GLOBAL_GET("display/window/size/width");
            video_mode.height = GLOBAL_GET("display/window/size/height");

            if (globals->has_setting("display/window/size/test_width") && globals->has_setting("display/window/size/test_height")) {
                int tw = globals->get("display/window/size/test_width");
                if (tw > 0) {
                    video_mode.width = tw;
                }
                int th = globals->get("display/window/size/test_height");
                if (th > 0) {
                    video_mode.height = th;
                }
            }
        }

        video_mode.resizable = GLOBAL_GET("display/window/size/resizable");
        video_mode.borderless_window = GLOBAL_GET("display/window/size/borderless");
        video_mode.fullscreen = GLOBAL_GET("display/window/size/fullscreen");
        video_mode.always_on_top = GLOBAL_GET("display/window/size/always_on_top");
    }

    if (!force_lowdpi) {
        OS::get_singleton()->_allow_hidpi = GLOBAL_DEF("display/window/dpi/allow_hidpi", false);
    }


    video_mode.use_vsync = GLOBAL_DEF_RST("display/window/vsync/use_vsync", true);
    OS::get_singleton()->_use_vsync = video_mode.use_vsync;

    if (!saw_vsync_via_compositor_override) {
        // If one of the command line options to enable/disable vsync via the
        // window compositor ("--enable-vsync-via-compositor" or
        // "--disable-vsync-via-compositor") was present then it overrides the
        // project setting.
        video_mode.vsync_via_compositor = GLOBAL_DEF("display/window/vsync/vsync_via_compositor", false);
    }

    OS::get_singleton()->_vsync_via_compositor = video_mode.vsync_via_compositor;

    OS::get_singleton()->_allow_layered = GLOBAL_DEF("display/window/per_pixel_transparency/allowed", false);
    video_mode.layered = GLOBAL_DEF("display/window/per_pixel_transparency/enabled", false);

    GLOBAL_DEF("rendering/quality/intended_usage/framebuffer_allocation", 2);
    GLOBAL_DEF("rendering/quality/intended_usage/framebuffer_allocation.mobile", 3);

    if (editor || project_manager) {
        // The editor and project manager always detect and use hiDPI if needed
        OS::get_singleton()->_allow_hidpi = true;
        OS::get_singleton()->_allow_layered = false;
    }

    Engine::get_singleton()->_pixel_snap = GLOBAL_DEF("rendering/quality/2d/use_pixel_snap", false);
    OS::get_singleton()->_keep_screen_on = GLOBAL_DEF("display/window/energy_saving/keep_screen_on", true);
    if (rtm == -1) {
        rtm = GLOBAL_DEF("rendering/threads/thread_model", OS::RENDER_THREAD_SAFE);
    }

    if (rtm >= 0 && rtm < 3) {
        if (editor) {
            rtm = OS::RENDER_THREAD_SAFE;
        }
        OS::get_singleton()->_render_thread_mode = OS::RenderThreadMode(rtm);
    }

    /* Determine audio and video drivers */

    for (int i = 0; i < OS::get_singleton()->get_video_driver_count(); i++) {

        if (video_driver == OS::get_singleton()->get_video_driver_name(i)) {

            video_driver_idx = i;
            break;
        }
    }

    if (video_driver_idx < 0) {
        video_driver_idx = 0;
    }

    if (audio_driver.empty()) { // specified in project.godot
        audio_driver = GLOBAL_DEF_RST("audio/driver", OS::get_singleton()->get_audio_driver_name(0));
    }

    for (int i = 0; i < OS::get_singleton()->get_audio_driver_count(); i++) {

        if (audio_driver == OS::get_singleton()->get_audio_driver_name(i)) {

            audio_driver_idx = i;
            break;
        }
    }

    if (audio_driver_idx < 0) {
        audio_driver_idx = 0;
    }

    {
        UIString orientation = GLOBAL_DEF("display/window/handheld/orientation", "landscape");

        if (orientation == "portrait")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_PORTRAIT);
        else if (orientation == "reverse_landscape")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_REVERSE_LANDSCAPE);
        else if (orientation == "reverse_portrait")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_REVERSE_PORTRAIT);
        else if (orientation == "sensor_landscape")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR_LANDSCAPE);
        else if (orientation == "sensor_portrait")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR_PORTRAIT);
        else if (orientation == "sensor")
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_SENSOR);
        else
            OS::get_singleton()->set_screen_orientation(OS::SCREEN_LANDSCAPE);
    }

    Engine::get_singleton()->set_iterations_per_second(GLOBAL_DEF("physics/common/physics_fps", 60));
    ProjectSettings::get_singleton()->set_custom_property_info("physics/common/physics_fps", PropertyInfo(VariantType::INT, "physics/common/physics_fps", PropertyHint::Range, "1,120,1,or_greater"));
    Engine::get_singleton()->set_physics_jitter_fix(GLOBAL_DEF("physics/common/physics_jitter_fix", 0.5));
    Engine::get_singleton()->set_target_fps(GLOBAL_DEF("debug/settings/fps/force_fps", 0));
    ProjectSettings::get_singleton()->set_custom_property_info("debug/settings/fps/force_fps", PropertyInfo(VariantType::INT, "debug/settings/fps/force_fps", PropertyHint::Range, "0,120,1,or_greater"));

    GLOBAL_DEF("debug/settings/stdout/print_fps", false);

    if (!OS::get_singleton()->_verbose_stdout) //overridden
        OS::get_singleton()->_verbose_stdout = GLOBAL_DEF("debug/settings/stdout/verbose_stdout", false);

    if (frame_delay == 0) {
        frame_delay = GLOBAL_DEF("application/run/frame_delay_msec", 0);
        ProjectSettings::get_singleton()->set_custom_property_info("application/run/frame_delay_msec", PropertyInfo(VariantType::INT, "application/run/frame_delay_msec", PropertyHint::Range, "0,100,1,or_greater")); // No negative numbers
    }

    OS::get_singleton()->set_low_processor_usage_mode(GLOBAL_DEF("application/run/low_processor_mode", false));
    OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(GLOBAL_DEF("application/run/low_processor_mode_sleep_usec", 6900)); // Roughly 144 FPS
    ProjectSettings::get_singleton()->set_custom_property_info("application/run/low_processor_mode_sleep_usec", PropertyInfo(VariantType::INT, "application/run/low_processor_mode_sleep_usec", PropertyHint::Range, "0,33200,1,or_greater")); // No negative numbers

    GLOBAL_DEF("display/window/ios/hide_home_indicator", true);

    Engine::get_singleton()->set_frame_delay(frame_delay);

    message_queue = memnew(MessageQueue);


    if (p_second_phase)
        return setup2();

    return OK;

error:

    video_driver = "";
    audio_driver = "";
    project_path = "";

    args.clear();
    main_args.clear();

    if (show_help)
        print_help(execpath);

    if (performance)
        memdelete(performance);
    if (input_map)
        memdelete(input_map);
    if (translation_server)
        memdelete(translation_server);
    if (globals)
        memdelete(globals);
    if (engine)
        memdelete(engine);
    if (script_debugger)
        memdelete(script_debugger);
    if (packed_data)
        memdelete(packed_data);
    if (file_access_network_client)
        memdelete(file_access_network_client);


    unregister_core_driver_types();
    unregister_core_types();

    OS::get_singleton()->_cmdline.clear();

    if (message_queue)
        memdelete(message_queue);
    OS::get_singleton()->finalize_core();
    locale.clear();

    return ERR_INVALID_PARAMETER;
}

Error Main::setup2(Thread::ID p_main_tid_override) {

    load_all_plugins("plugins");

    // Print engine name and version
    print_line(String(VERSION_NAME) + " v" + get_full_version_string() + " - " + String(VERSION_WEBSITE));
    if (p_main_tid_override) {
        Thread::_main_thread_id = p_main_tid_override;
    }

    register_server_types();
    InputDefault::initialize_class();

    Error err = OS::get_singleton()->initialize(video_mode, video_driver_idx, audio_driver_idx);
    if (err != OK) {
        return err;
    }
    setup_server_defs(); // servers are setup after OS singleton opens the window

    print_line(" "); //add a blank line for readability
    if (init_use_custom_pos) {
        OS::get_singleton()->set_window_position(init_custom_pos);
    }

    // right moment to create and initialize the audio server

    audio_server = memnew(AudioServer);
    audio_server->init();

    // also init our arvr_server from here
    arvr_server = memnew(ARVRServer);

    register_core_singletons();

    MAIN_PRINT("Main: Setup Logo");

    bool show_logo = true;

    if (init_screen != -1) {
        OS::get_singleton()->set_current_screen(init_screen);
    }
    if (init_windowed) {
        //do none..
    } else if (init_maximized) {
        OS::get_singleton()->set_window_maximized(true);
    } else if (init_fullscreen) {
        OS::get_singleton()->set_window_fullscreen(true);
    }
    if (init_always_on_top) {
        OS::get_singleton()->set_window_always_on_top(true);
    }

    if (allow_focus_steal_pid) {
        OS::get_singleton()->enable_for_stealing_focus(allow_focus_steal_pid);
    }


    MAIN_PRINT("Main: Load Remaps");

    Color clear = GLOBAL_DEF("rendering/environment/default_clear_color", Color(0.3f, 0.3f, 0.3f));
    VisualServer::get_singleton()->set_default_clear_color(clear);

    if (show_logo) { //boot logo!
        String boot_logo_path = GLOBAL_DEF("application/boot_splash/image", String());
        bool boot_logo_scale = GLOBAL_DEF("application/boot_splash/fullsize", true);
        bool boot_logo_filter = GLOBAL_DEF("application/boot_splash/use_filter", true);
        ProjectSettings::get_singleton()->set_custom_property_info("application/boot_splash/image",
                PropertyInfo(VariantType::STRING, "application/boot_splash/image", PropertyHint::File, "*.png"));

        Ref<Image> boot_logo;

        boot_logo_path =StringUtils::strip_edges( boot_logo_path);

        if (!boot_logo_path.empty()) {
            boot_logo = make_ref_counted<Image>();
            Error load_err = ImageLoader::load_image(boot_logo_path, boot_logo);
            if (load_err) {
                ERR_PRINT("Non-existing or invalid boot splash at '" + boot_logo_path + "'. Loading default splash.");
            }
        }

        Color boot_bg_color = GLOBAL_DEF("application/boot_splash/bg_color", boot_splash_bg_color);
        if (boot_logo) {
            OS::get_singleton()->_msec_splash = OS::get_singleton()->get_ticks_msec();
            VisualServer::get_singleton()->set_boot_image(boot_logo, boot_bg_color, boot_logo_scale, boot_logo_filter);

        } else {
#ifndef NO_DEFAULT_BOOT_LOGO

            MAIN_PRINT("Main: Create bootsplash");
#if defined(TOOLS_ENABLED) && !defined(NO_EDITOR_SPLASH)

            Ref<Image> splash(make_ref_counted<Image>((editor || project_manager) ? boot_splash_editor_png : boot_splash_png));
#else
            Ref<Image> splash(make_ref_counted<Image>(boot_splash_png));
#endif

            MAIN_PRINT("Main: ClearColor");
            VisualServer::get_singleton()->set_default_clear_color(boot_bg_color);
            MAIN_PRINT("Main: Image");
            VisualServer::get_singleton()->set_boot_image(splash, boot_bg_color, false);
#endif
        }

#ifdef TOOLS_ENABLED
        Ref<Image> icon(make_ref_counted<Image>(app_icon_png));
        OS::get_singleton()->set_icon(icon);
#endif
    }

    MAIN_PRINT("Main: DCC");
    VisualServer::get_singleton()->set_default_clear_color(GLOBAL_DEF("rendering/environment/default_clear_color", Color(0.3f, 0.3f, 0.3f)));
    MAIN_PRINT("Main: END");

    GLOBAL_DEF("application/config/icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/icon", PropertyInfo(VariantType::STRING, "application/config/icon", PropertyHint::File, "*.png,*.webp"));

    GLOBAL_DEF("application/config/macos_native_icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/macos_native_icon", PropertyInfo(VariantType::STRING, "application/config/macos_native_icon", PropertyHint::File, "*.icns"));

    GLOBAL_DEF("application/config/windows_native_icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/windows_native_icon", PropertyInfo(VariantType::STRING, "application/config/windows_native_icon", PropertyHint::File, "*.ico"));

    InputDefault *id = object_cast<InputDefault>(Input::get_singleton());
    if (id) {
        if (bool(GLOBAL_DEF("input_devices/pointing/emulate_touch_from_mouse", false)) && !(editor || project_manager)) {
            if (!OS::get_singleton()->has_touchscreen_ui_hint()) {
                //only if no touchscreen ui hint, set emulation
                id->set_emulate_touch_from_mouse(true);
            }
        }

        id->set_emulate_mouse_from_touch(bool(GLOBAL_DEF("input_devices/pointing/emulate_mouse_from_touch", true)));
    }

    MAIN_PRINT("Main: Load Remaps");

    MAIN_PRINT("Main: Load Scene Types");

    register_scene_types();

    GLOBAL_DEF("display/mouse_cursor/custom_image", String());
    GLOBAL_DEF("display/mouse_cursor/custom_image_hotspot", Vector2());
    GLOBAL_DEF("display/mouse_cursor/tooltip_position_offset", Point2(10, 10));
    ProjectSettings::get_singleton()->set_custom_property_info("display/mouse_cursor/custom_image",
            PropertyInfo(VariantType::STRING, "display/mouse_cursor/custom_image", PropertyHint::File, "*.png,*.webp"));

    if (!String(ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image")).empty()) {

        Ref<Texture> cursor = dynamic_ref_cast<Texture>(
                ResourceLoader::load(ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image").as<String>()));
        if (cursor) {
            Vector2 hotspot = ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image_hotspot");
            Input::get_singleton()->set_custom_mouse_cursor(cursor, Input::CURSOR_ARROW, hotspot);
        }
    }
#ifdef TOOLS_ENABLED
    ClassDB::set_current_api(ClassDB::API_EDITOR);
    EditorNode::register_editor_types();

    ClassDB::set_current_api(ClassDB::API_CORE);

#endif


    MAIN_PRINT("Main: Load Modules, Physics, Drivers, Scripts");

    add_plugin_resolver(new ResourcePluginResolver);

    register_module_types();

    camera_server = CameraServer::create();

    initialize_physics();
    initialize_navigation_server();
    register_server_singletons();

    register_driver_types();

    // This loads global classes, so it must happen before custom loaders and savers are registered
    ScriptServer::init_languages();

    MAIN_PRINT("Main: Load Translations");

    translation_server->setup(); //register translations, load them, etc.
    if (!locale.empty()) {

        translation_server->set_locale(locale);
    }
    translation_server->load_translations();
    ResourceLoader::load_translation_remaps(); //load remaps for resources

    ResourceLoader::load_path_remaps();

    audio_server->load_default_bus_layout();

    if (use_debug_profiler && script_debugger) {
        script_debugger->profiling_start();
    }
    _start_success = true;
    locale.clear();

    ClassDB::set_current_api(ClassDB::API_NONE); //no more api is registered at this point

    print_verbose("CORE API HASH: " + itos(ClassDB::get_api_hash(ClassDB::API_CORE)));
    print_verbose("EDITOR API HASH: " + itos(ClassDB::get_api_hash(ClassDB::API_EDITOR)));
    MAIN_PRINT("Main: Done");

    return OK;
}

// everything the main loop needs to know about frame timings

static MainTimerSync main_timer_sync;

bool Main::start() {

    ERR_FAIL_COND_V(!_start_success, false);

    bool hasicon = false;
    String doc_tool;
    PODVector<String> removal_docs;
    String positional_arg;
    String game_path;
    String script;
    String test;
    bool check_only = false;
#ifdef TOOLS_ENABLED
    bool doc_base = true;
    String _export_preset;
    bool export_debug = false;
    bool export_pack_only = false;
#endif

    main_timer_sync.init(OS::get_singleton()->get_ticks_usec());

    const ListPOD<String> &args(OS::get_singleton()->get_cmdline_args());
    for (ListPOD<String>::const_iterator i = args.begin(); i!=args.end(); ++i) {
        ListPOD<String>::const_iterator next = i;
        ++next;
        bool has_next = next!=args.end();

        //parameters that do not have an argument to the right
        if (*i == "--check-only") {
            check_only = true;
#ifdef TOOLS_ENABLED
        } else if (*i == "--no-docbase") {
            doc_base = false;
        } else if (*i == "-e" || *i == "--editor") {
            editor = true;
        } else if (*i == "-p" || *i == "--project-manager") {
            project_manager = true;
#endif
        } else if (i->length() && i->at(0) != '-' && positional_arg == "") {
            positional_arg = *i;

            if (StringUtils::ends_with(positional_arg,".scn") ||
                StringUtils::ends_with(positional_arg,".tscn") ||
                StringUtils::ends_with(positional_arg,".escn")) {
                // Only consider the positional argument to be a scene path if it ends with
                // a file extension associated with Godot scenes. This makes it possible
                // for projects to parse command-line arguments for custom CLI arguments
                // or other file extensions without trouble. This can be used to implement
                // "drag-and-drop onto executable" logic, which can prove helpful
                // for non-game applications.
                game_path = positional_arg;
            }
        }
        //parameters that have an argument to the right
        else if (has_next) {
            bool parsed_pair = true;
            if (*i == "-s" || *i == "--script") {
                script = *next;
            } else if (*i == "--test") {
                test = *next;
#ifdef TOOLS_ENABLED
            } else if (*i == "--doctool") {
                doc_tool = *next;
                ListPOD<String>::const_iterator j = next;
                ++j;
                for ( ; j != args.end(); ++j)
                    removal_docs.push_back(*j);
            } else if (*i == "--export") {
                editor = true; //needs editor
                    _export_preset = *next;
            } else if (*i == "--export-debug") {
                editor = true; //needs editor
                _export_preset = *next;
                export_debug = true;
            } else if (*i == "--export-pack") {
                editor = true;
                _export_preset = *next;
                export_pack_only = true;
#endif
            } else {
                // The parameter does not match anything known, don't skip the next argument
                parsed_pair = false;
            }
            if (parsed_pair) {
                i=next; // skip over.
            }
        }
    }

    String main_loop_type;
#ifdef TOOLS_ENABLED
    if (!doc_tool.empty()) {

        Engine::get_singleton()->set_editor_hint(true); // Needed to instance editor-only classes for their default values
        {
            DirAccessRef da = DirAccess::open(doc_tool);
            ERR_FAIL_COND_V_MSG(!da, false, "Argument supplied to --doctool must be a base Godot build directory.");
        }
        DocData doc;
        doc.generate(doc_base);

        DocData docsrc;
        Map<StringName, String> doc_data_classes;
        Set<String> checked_paths;
        print_line("Loading docs...");

        for (int i = 0; i < _doc_data_class_path_count; i++) {
            String path = PathUtils::plus_file(doc_tool,_doc_data_class_paths[i].path);
            String name(_doc_data_class_paths[i].name);
            doc_data_classes[StringName(name)] = path;
            if (!checked_paths.contains(path)) {
                checked_paths.insert(path);
                // Create the module documentation directory if it doesn't exist
                DirAccess *da = DirAccess::create_for_path(path);
                da->make_dir_recursive(path);
                memdelete(da);
                docsrc.load_classes(path);
                print_line("Loading docs from: " + path);
            }
        }

        String index_path = PathUtils::plus_file(doc_tool,"doc/classes");
        // Create the main documentation directory if it doesn't exist
        DirAccess *da = DirAccess::create_for_path(index_path);
        da->make_dir_recursive(index_path);
        memdelete(da);
        docsrc.load_classes(index_path);
        checked_paths.insert(index_path);
        print_line("Loading docs from: " + index_path);

        print_line("Merging docs...");
        doc.merge_from(docsrc);
        for (const String &E : checked_paths) {
            print_line("Erasing old docs at: " + E);
            DocData::erase_classes(E);
        }

        print_line("Generating new docs...");
        doc.save_classes(index_path, doc_data_classes);

        return false;
    }
    if (not _export_preset.empty()) {
        if (positional_arg.empty()) {
            String err = "Command line includes export parameter option, but no destination path was given.\n";
            err += "Please specify the binary's file path to export to. Aborting export.";
            ERR_PRINT(err);
            return false;
        }
    }
#endif
    if (script.empty() && game_path.empty() && !String(GLOBAL_DEF("application/run/main_scene", "")).empty()) {
        game_path = (String)GLOBAL_DEF("application/run/main_scene", "");
    }

    MainLoop *main_loop = nullptr;
    if (editor) {
        main_loop = memnew(SceneTree);
    }

    if (!test.empty()) {
#ifdef TOOLS_ENABLED
        main_loop = test_main(test, args);

        if (!main_loop)
            return false;

#endif

    } else if (!script.empty()) {

        Ref<Script> script_res = dynamic_ref_cast<Script>(ResourceLoader::load(script));
        ERR_FAIL_COND_V_MSG(not script_res, false, "Can't load script: " + script);

        if (check_only) {
            if (!script_res->is_valid()) {
                OS::get_singleton()->set_exit_code(1);
            }
            return false;
        }

        if (script_res->can_instance() /*&& script_res->inherits_from("SceneTreeScripted")*/) {

            StringName instance_type = script_res->get_instance_base_type();
            Object *obj = ClassDB::instance(instance_type);
            MainLoop *script_loop = object_cast<MainLoop>(obj);
            if (!script_loop) {
                if (obj)
                    memdelete(obj);
                ERR_FAIL_V_MSG(false, "Can't load script '" + script + "', it does not inherit from a MainLoop type.");
            }

            script_loop->set_init_script(script_res);
            main_loop = script_loop;
        } else {

            return false;
        }

    } else {
        main_loop_type = (String)GLOBAL_DEF("application/run/main_loop_type", "");
    }

    if (!main_loop && main_loop_type.empty())
        main_loop_type = "SceneTree";

    if (!main_loop) {
        if (!ClassDB::class_exists(StringName(main_loop_type))) {
            OS::get_singleton()->alert(("Error: MainLoop type doesn't exist: " + main_loop_type));
            return false;
        } else {

            Object *ml = ClassDB::instance(StringName(main_loop_type));
            ERR_FAIL_COND_V_MSG(!ml, false, "Can't instance MainLoop type.");

            main_loop = object_cast<MainLoop>(ml);
            if (!main_loop) {

                memdelete(ml);
                ERR_FAIL_V_MSG(false, "Invalid MainLoop type.");
            }
        }
    }

    if (main_loop->is_class("SceneTree")) {

        SceneTree *sml = object_cast<SceneTree>(main_loop);

#ifdef DEBUG_ENABLED
        if (debug_collisions) {
            sml->set_debug_collisions_hint(true);
        }
        if (debug_navigation) {
            sml->set_debug_navigation_hint(true);
        }
#endif

        ResourceLoader::add_custom_loaders();
        ResourceSaver::add_custom_savers();
        if (!project_manager && !editor) { // game
            if (!game_path.empty() || !script.empty()) {
                if (script_debugger && script_debugger->is_remote()) {
                    ScriptDebuggerRemote *remote_debugger = static_cast<ScriptDebuggerRemote *>(script_debugger);

                    remote_debugger->set_scene_tree(sml);
                }
                //autoload
                PODVector<PropertyInfo> props;
                ProjectSettings::get_singleton()->get_property_list(&props);

                //first pass, add the constants so they exist before any script is loaded
                for (const PropertyInfo &E : props) {

                    StringName s = E.name;
                    if (!StringUtils::begins_with(s,"autoload/"))
                        continue;
                    StringName name(StringUtils::get_slice(s,'/', 1));
                    String path = ProjectSettings::get_singleton()->get(s);
                    bool global_var = false;
                    if (StringUtils::begins_with(path,"*")) {
                        global_var = true;
                    }

                    if (global_var) {
                        for (int i = 0; i < ScriptServer::get_language_count(); i++) {
                            ScriptServer::get_language(i)->add_global_constant(name, Variant());
                        }
                    }
                }

                //second pass, load into global constants
                PODVector<Node *> to_add;
                for (const PropertyInfo &E : props) {

                    StringName s(E.name);
                    if (!StringUtils::begins_with(s,"autoload/"))
                        continue;
                    StringName name(StringUtils::get_slice(s,'/', 1));
                    String path = ProjectSettings::get_singleton()->get(s);
                    bool global_var = false;
                    if (StringUtils::begins_with(path,"*")) {
                        global_var = true;
                        path = StringUtils::substr(path,1, path.length() - 1);
                    }

                    RES res(ResourceLoader::load(path));
                    ERR_CONTINUE_MSG(not res, "Can't autoload: " + path);
                    Node *n = nullptr;
                    if (res->is_class("PackedScene")) {
                        Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(res);
                        n = ps->instance();
                    } else if (res->is_class("Script")) {
                        Ref<Script> script_res = dynamic_ref_cast<Script>(res);
                        StringName ibt = script_res->get_instance_base_type();
                        bool valid_type = ClassDB::is_parent_class(ibt, "Node");
                        ERR_CONTINUE_MSG(!valid_type, "Script does not inherit a Node: " + path);

                        Object *obj = ClassDB::instance(ibt);

                        ERR_CONTINUE_MSG(obj == nullptr, "Cannot instance script for autoload, expected 'Node' inheritance, got: " + String(ibt));

                        n = object_cast<Node>(obj);
                        n->set_script(script_res.get_ref_ptr());
                    }

                    ERR_CONTINUE_MSG(!n, "Path in autoload not a node or script: " + path);
                    n->set_name(name);

                    //defer so references are all valid on _ready()
                    to_add.push_back(n);

                    if (global_var) {
                        for (int i = 0; i < ScriptServer::get_language_count(); i++) {
                            ScriptServer::get_language(i)->add_global_constant(name, Variant(n));
                        }
                    }
                }

                for (Node * n : to_add) {

                    sml->get_root()->add_child(n);
                }
            }
        }

#ifdef TOOLS_ENABLED

        EditorNode *editor_node = nullptr;
        if (editor) {

            editor_node = memnew(EditorNode);
            sml->get_root()->add_child(editor_node);

            if (!_export_preset.empty()) {
                editor_node->export_preset(_export_preset, positional_arg, export_debug, export_pack_only);
                game_path = ""; // Do not load anything.
            }
        }
#endif

        if (!editor && !project_manager) {
            //standard helpers that can be changed from main config

            UIString stretch_mode = GLOBAL_DEF("display/window/stretch/mode", "disabled");
            UIString stretch_aspect = GLOBAL_DEF("display/window/stretch/aspect", "ignore");
            Size2i stretch_size = Size2(GLOBAL_DEF("display/window/size/width", 0), GLOBAL_DEF("display/window/size/height", 0));
            real_t stretch_shrink = GLOBAL_DEF("display/window/stretch/shrink", 1.0);

            SceneTree::StretchMode sml_sm = SceneTree::STRETCH_MODE_DISABLED;
            if (stretch_mode == "2d")
                sml_sm = SceneTree::STRETCH_MODE_2D;
            else if (stretch_mode == "viewport")
                sml_sm = SceneTree::STRETCH_MODE_VIEWPORT;

            SceneTree::StretchAspect sml_aspect = SceneTree::STRETCH_ASPECT_IGNORE;
            if (stretch_aspect == "keep")
                sml_aspect = SceneTree::STRETCH_ASPECT_KEEP;
            else if (stretch_aspect == "keep_width")
                sml_aspect = SceneTree::STRETCH_ASPECT_KEEP_WIDTH;
            else if (stretch_aspect == "keep_height")
                sml_aspect = SceneTree::STRETCH_ASPECT_KEEP_HEIGHT;
            else if (stretch_aspect == "expand")
                sml_aspect = SceneTree::STRETCH_ASPECT_EXPAND;

            sml->set_screen_stretch(sml_sm, sml_aspect, stretch_size, stretch_shrink);

            sml->set_auto_accept_quit(GLOBAL_DEF("application/config/auto_accept_quit", true));
            sml->set_quit_on_go_back(GLOBAL_DEF("application/config/quit_on_go_back", true));
            StringName appname = ProjectSettings::get_singleton()->get("application/config/name");
            appname = TranslationServer::get_singleton()->translate(appname);
            OS::get_singleton()->set_window_title(appname);

            int shadow_atlas_size = GLOBAL_GET("rendering/quality/shadow_atlas/size");
            int shadow_atlas_q0_subdiv = GLOBAL_GET("rendering/quality/shadow_atlas/quadrant_0_subdiv");
            int shadow_atlas_q1_subdiv = GLOBAL_GET("rendering/quality/shadow_atlas/quadrant_1_subdiv");
            int shadow_atlas_q2_subdiv = GLOBAL_GET("rendering/quality/shadow_atlas/quadrant_2_subdiv");
            int shadow_atlas_q3_subdiv = GLOBAL_GET("rendering/quality/shadow_atlas/quadrant_3_subdiv");

            sml->get_root()->set_shadow_atlas_size(shadow_atlas_size);
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(0, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q0_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(1, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q1_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(2, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q2_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(3, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q3_subdiv));
            Viewport::Usage usage = Viewport::Usage(int(GLOBAL_GET("rendering/quality/intended_usage/framebuffer_allocation")));
            sml->get_root()->set_usage(usage);

            bool snap_controls = GLOBAL_DEF("gui/common/snap_controls_to_pixels", true);
            sml->get_root()->set_snap_controls_to_pixels(snap_controls);

            bool font_oversampling = GLOBAL_DEF("rendering/quality/dynamic_fonts/use_oversampling", true);
            sml->set_use_font_oversampling(font_oversampling);

        }
        else {
            GLOBAL_DEF("display/window/stretch/mode", "disabled");
            ProjectSettings::get_singleton()->set_custom_property_info("display/window/stretch/mode",
                    PropertyInfo(VariantType::STRING, "display/window/stretch/mode", PropertyHint::Enum, "disabled,2d,viewport"));
            GLOBAL_DEF("display/window/stretch/aspect", "ignore");
            ProjectSettings::get_singleton()->set_custom_property_info(
                    "display/window/stretch/aspect", PropertyInfo(VariantType::STRING, "display/window/stretch/aspect",
                                                             PropertyHint::Enum, "ignore,keep,keep_width,keep_height,expand"));
            GLOBAL_DEF("display/window/stretch/shrink", 1.0);
            ProjectSettings::get_singleton()->set_custom_property_info("display/window/stretch/shrink",
                    PropertyInfo(VariantType::REAL, "display/window/stretch/shrink", PropertyHint::Range, "1.0,8.0,0.1"));
            sml->set_auto_accept_quit(GLOBAL_DEF("application/config/auto_accept_quit", true));
            sml->set_quit_on_go_back(GLOBAL_DEF("application/config/quit_on_go_back", true));
            GLOBAL_DEF("gui/common/snap_controls_to_pixels", true);
            GLOBAL_DEF("rendering/quality/dynamic_fonts/use_oversampling", true);
        }

        String local_game_path;
        if (!game_path.empty() && !project_manager) {

            local_game_path = PathUtils::from_native_path(game_path);

            if (!StringUtils::begins_with(local_game_path,"res://")) {
                bool absolute = (local_game_path.size() > 1) && (local_game_path[0] == '/' || local_game_path[1] == ':');

                if (!absolute) {

                    if (ProjectSettings::get_singleton()->is_using_datapack()) {

                        local_game_path = "res://" + local_game_path;

                    } else {
                        auto sep = StringUtils::find_last(local_game_path,'/');

                        if (sep == String::npos) {
                            DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
                            local_game_path = PathUtils::plus_file(da->get_current_dir(),local_game_path);
                            memdelete(da);
                        } else {

                            DirAccess *da = DirAccess::open(StringUtils::substr(local_game_path,0, sep));
                            if (da) {
                                local_game_path = PathUtils::plus_file(da->get_current_dir(),StringUtils::substr(local_game_path,sep + 1, local_game_path.length()));
                                memdelete(da);
                            }
                        }
                    }
                }
            }

            local_game_path = ProjectSettings::get_singleton()->localize_path(local_game_path);

#ifdef TOOLS_ENABLED
            if (editor) {

                if (game_path != (String)GLOBAL_GET("application/run/main_scene") || !editor_node->has_scenes_in_session()) {
                    Error serr = editor_node->load_scene(local_game_path);
                    if (serr != OK) {
                        ERR_PRINT("Failed to load scene");
                    }
                }
                OS::get_singleton()->set_context(OS::CONTEXT_EDITOR);
            }
#endif
            if (!editor) {
                OS::get_singleton()->set_context(OS::CONTEXT_ENGINE);
            }
        }

        if (!project_manager && !editor) { // game
            // Load SSL Certificates from Project Settings (or builtin).
        Crypto::load_default_certificates(GLOBAL_DEF("network/ssl/certificates", "").as<String>());
            if (!game_path.empty()) {
                Node *scene = nullptr;
                Ref<PackedScene> scenedata = dynamic_ref_cast<PackedScene>(ResourceLoader::load(local_game_path));
                if (scenedata)
                    scene = scenedata->instance();

                ERR_FAIL_COND_V_MSG(!scene, false, "Failed loading scene: " + local_game_path);
                sml->add_current_scene(scene);
#ifdef OSX_ENABLED
                String mac_iconpath = GLOBAL_DEF("application/config/macos_native_icon", "Variant()");
                if (mac_iconpath != "") {
                    OS::get_singleton()->set_native_icon(mac_iconpath);
                    hasicon = true;
                }
#endif

#ifdef WINDOWS_ENABLED
                String win_iconpath = GLOBAL_DEF("application/config/windows_native_icon", "Variant()");
                if (not win_iconpath.empty()) {
                    OS::get_singleton()->set_native_icon(win_iconpath);
                    hasicon = true;
                }
#endif

                String iconpath = GLOBAL_DEF("application/config/icon", "Variant()");
                if ((!iconpath.empty()) && (!hasicon)) {
                    Ref<Image> icon(make_ref_counted<Image>());
                    if (ImageLoader::load_image(iconpath, icon) == OK) {
                        OS::get_singleton()->set_icon(icon);
                        hasicon = true;
                    }
                }
            }
        }

#ifdef TOOLS_ENABLED
        if (project_manager || (script.empty() && test.empty() && game_path.empty() && !editor)) {

            Engine::get_singleton()->set_editor_hint(true);
            ProjectManager *pmanager = memnew(ProjectManager);
            ProgressDialog *progress_dialog = memnew(ProgressDialog);
            pmanager->add_child(progress_dialog);
            sml->get_root()->add_child(pmanager);
            // Speed up rendering slightly by disabling 3D features while in the project manager.
            sml->get_root()->set_usage(Viewport::USAGE_2D_NO_SAMPLING);
            OS::get_singleton()->set_context(OS::CONTEXT_PROJECTMAN);
            project_manager = true;
        }
        if (project_manager || editor) {
            // Hide console window if requested (Windows-only).
            bool hide_console = EditorSettings::get_singleton()->get_setting("interface/editor/hide_console_window");
            OS::get_singleton()->set_console_visible(!hide_console);
        }

        if (project_manager || editor) {
            // Load SSL Certificates from Editor Settings (or builtin)
            Crypto::load_default_certificates(
                    EditorSettings::get_singleton()->get_setting("network/ssl/editor_ssl_certificates").as<String>());
        }
#endif
    }

    if (!hasicon) {
        Ref<Image> icon(make_ref_counted<Image>(app_icon_png));
        OS::get_singleton()->set_icon(icon);
    }

    OS::get_singleton()->set_main_loop(main_loop);

    return true;
}

/* Main iteration
 *
 * This is the iteration of the engine's game loop, advancing the state of physics,
 * rendering and audio.
 * It's called directly by the platform's OS::run method, where the loop is created
 * and monitored.
 *
 * The OS implementation can impact its draw step with the Main::force_redraw() method.
 */
uint64_t Main::last_ticks = 0;
uint64_t Main::target_ticks = 0;
uint32_t Main::frames = 0;
uint32_t Main::frame = 0;
bool Main::force_redraw_requested = false;
int Main::iterating = 0;
bool Main::is_iterating() {
    return iterating > 0;
}

// For performance metrics.
static uint64_t physics_process_max = 0;
static uint64_t idle_process_max = 0;

bool Main::iteration() {
    SCOPE_AUTONAMED;
    //for now do not error on this
    //ERR_FAIL_COND_V(iterating, false);

    iterating++;
    uint64_t ticks = OS::get_singleton()->get_ticks_usec();
    Engine::get_singleton()->_frame_ticks = ticks;
    main_timer_sync.set_cpu_ticks_usec(ticks);
    main_timer_sync.set_fixed_fps(fixed_fps);

    uint64_t ticks_elapsed = ticks - last_ticks;

    int physics_fps = Engine::get_singleton()->get_iterations_per_second();
    float frame_slice = 1.0f / physics_fps;

    float time_scale = Engine::get_singleton()->get_time_scale();
    MainFrameTime advance = main_timer_sync.advance(frame_slice, physics_fps);
    double step = advance.idle_step;
    double scaled_step = step * time_scale;

    Engine::get_singleton()->_frame_step = step;
    Engine::get_singleton()->_physics_interpolation_fraction = advance.interpolation_fraction;

    uint64_t physics_process_ticks = 0;
    uint64_t idle_process_ticks = 0;

    frame += ticks_elapsed;

    last_ticks = ticks;

    static const int max_physics_steps = 8;
    if (fixed_fps == -1 && advance.physics_steps > max_physics_steps) {
        step -= (advance.physics_steps - max_physics_steps) * frame_slice;
        advance.physics_steps = max_physics_steps;
    }


    bool exit = false;

    Engine::get_singleton()->_in_physics = true;

    for (int iters = 0; iters < advance.physics_steps; ++iters) {

        uint64_t physics_begin = OS::get_singleton()->get_ticks_usec();

        PhysicsServer::get_singleton()->sync();
        PhysicsServer::get_singleton()->flush_queries();

        Physics2DServer::get_singleton()->sync();
        Physics2DServer::get_singleton()->flush_queries();

        if (OS::get_singleton()->get_main_loop()->iteration(frame_slice * time_scale)) {
            exit = true;
            break;
        }

        message_queue->flush();

        PhysicsServer::get_singleton()->step(frame_slice * time_scale);
        NavigationServer::get_singleton_mut()->step(frame_slice * time_scale);

        Physics2DServer::get_singleton()->end_sync();
        Physics2DServer::get_singleton()->step(frame_slice * time_scale);

        message_queue->flush();

        physics_process_ticks = MAX(physics_process_ticks, OS::get_singleton()->get_ticks_usec() - physics_begin); // keep the largest one for reference
        physics_process_max = MAX(OS::get_singleton()->get_ticks_usec() - physics_begin, physics_process_max);
        Engine::get_singleton()->_physics_frames++;
    }

    Engine::get_singleton()->_in_physics = false;

    uint64_t idle_begin = OS::get_singleton()->get_ticks_usec();

    if (OS::get_singleton()->get_main_loop()->idle(step * time_scale)) {
        exit = true;
    }
    message_queue->flush();

    VisualServer::get_singleton()->sync(); //sync if still drawing from previous frames.

    if (OS::get_singleton()->can_draw() && !disable_render_loop) {

        if ((!force_redraw_requested) && OS::get_singleton()->is_in_low_processor_usage_mode()) {
            if (VisualServer::get_singleton()->has_changed()) {
                VisualServer::get_singleton()->draw(true, scaled_step); // flush visual commands
                Engine::get_singleton()->frames_drawn++;
            }
        } else {
            VisualServer::get_singleton()->draw(true, scaled_step); // flush visual commands
            Engine::get_singleton()->frames_drawn++;
            force_redraw_requested = false;
        }
    }

    idle_process_ticks = OS::get_singleton()->get_ticks_usec() - idle_begin;
    idle_process_max = MAX(idle_process_ticks, idle_process_max);
    uint64_t frame_time = OS::get_singleton()->get_ticks_usec() - ticks;

    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ScriptServer::get_language(i)->frame();
    }

    AudioServer::get_singleton()->update();

    if (script_debugger) {
        if (script_debugger->is_profiling()) {
            script_debugger->profiling_set_frame_times(USEC_TO_SEC(frame_time), USEC_TO_SEC(idle_process_ticks), USEC_TO_SEC(physics_process_ticks), frame_slice);
        }
        script_debugger->idle_poll();
    }

    frames++;
    Engine::get_singleton()->_idle_frames++;

    if (frame > 1000000) {

        if (editor || project_manager) {
            if (print_fps) {
                print_line("Editor FPS: " + itos(frames));
            }
        } else if (GLOBAL_GET("debug/settings/stdout/print_fps") || print_fps) {
            print_line("Game FPS: " + itos(frames));
        }

        Engine::get_singleton()->_fps = frames;
        performance->set_process_time(USEC_TO_SEC(idle_process_max));
        performance->set_physics_process_time(USEC_TO_SEC(physics_process_max));
        idle_process_max = 0;
        physics_process_max = 0;

        frame %= 1000000;
        frames = 0;
    }

    iterating--;

    if (fixed_fps != -1)
        return exit;

    if (OS::get_singleton()->is_in_low_processor_usage_mode() || !OS::get_singleton()->can_draw())
        OS::get_singleton()->delay_usec(OS::get_singleton()->get_low_processor_usage_mode_sleep_usec()); //apply some delay to force idle time
    else {
        uint32_t frame_delay = Engine::get_singleton()->get_frame_delay();
        if (frame_delay)
            OS::get_singleton()->delay_usec(Engine::get_singleton()->get_frame_delay() * 1000);
    }

    int target_fps = Engine::get_singleton()->get_target_fps();
    if (target_fps > 0 && !Engine::get_singleton()->is_editor_hint()) {
        uint64_t time_step = 1000000L / target_fps;
        target_ticks += time_step;
        uint64_t current_ticks = OS::get_singleton()->get_ticks_usec();
        if (current_ticks < target_ticks) OS::get_singleton()->delay_usec(target_ticks - current_ticks);
        current_ticks = OS::get_singleton()->get_ticks_usec();
        target_ticks = MIN(MAX(target_ticks, current_ticks - time_step), current_ticks + time_step);
    }

#ifdef TOOLS_ENABLED
    if (auto_build_solutions) {
        auto_build_solutions = false;
        // Only relevant when running the editor.
        if (!editor) {
            ERR_FAIL_V_MSG(true, "Command line option --build-solutions was passed, but no project is being edited. Aborting.");
        }
        if (!EditorNode::get_singleton()->call_build()) {
            ERR_FAIL_V_MSG(true, "Command line option --build-solutions was passed, but the build callback failed. Aborting.");
        }
    }
#endif

    return exit || auto_quit;
}

void Main::force_redraw() {

    force_redraw_requested = true;
}

/* Engine deinitialization
 *
 * Responsible for freeing all the memory allocated by previous setup steps,
 * so that the engine closes cleanly without leaking memory or crashing.
 * The order matters as some of those steps are linked with each other.
 */
void Main::cleanup() {

    ERR_FAIL_COND(!_start_success);
    if (script_debugger) {
        // Flush any remaining messages
        script_debugger->idle_poll();
    }

    ResourceLoader::remove_custom_loaders();
    ResourceSaver::remove_custom_savers();

    message_queue->flush();
    memdelete(message_queue);

    if (script_debugger) {
        if (use_debug_profiler) {
            script_debugger->profiling_end();
        }

        memdelete(script_debugger);
    }

    OS::get_singleton()->delete_main_loop();

    OS::get_singleton()->_cmdline.clear();
    OS::get_singleton()->_execpath = "";
    OS::get_singleton()->_local_clipboard = "";

    ResourceLoader::clear_translation_remaps();
    ResourceLoader::clear_path_remaps();

    ScriptServer::finish_languages();

    // Sync pending commands that may have been queued from a different thread during ScriptServer finalization
    VisualServer::get_singleton()->sync();

#ifdef TOOLS_ENABLED
    EditorNode::unregister_editor_types();
#endif

    if (arvr_server) {
        // cleanup now before we pull the rug from underneath...
        memdelete(arvr_server);
    }

    ImageLoader::cleanup();

    unregister_driver_types();
    unregister_module_types();
    unload_plugins();

    unregister_scene_types();
    unregister_server_types();

    if (audio_server) {
        audio_server->finish();
        memdelete(audio_server);
    }

    if (camera_server) {
        memdelete(camera_server);
    }

    OS::get_singleton()->finalize();
    finalize_physics();
    finalize_navigation_server();

    if (packed_data)
        memdelete(packed_data);
    if (file_access_network_client)
        memdelete(file_access_network_client);
    if (performance)
        memdelete(performance);
    if (input_map)
        memdelete(input_map);
    if (translation_server)
        memdelete(translation_server);
    if (globals)
        memdelete(globals);
    if (engine)
        memdelete(engine);
    if (OS::get_singleton()->is_restart_on_exit_set()) {
        //attempt to restart with arguments
        String exec = OS::get_singleton()->get_executable_path();
        ListPOD<String> args = OS::get_singleton()->get_restart_on_exit_arguments();
        OS::ProcessID pid = 0;
        OS::get_singleton()->execute(exec, args, false, &pid);
        OS::get_singleton()->set_restart_on_exit(false, ListPOD<String>()); //clear list (uses memory)
    }

    unregister_core_driver_types();
    unregister_core_types();

    OS::get_singleton()->finalize_core();
}
