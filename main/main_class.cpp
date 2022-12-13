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

#include "main_class.h"

#include "core/class_db.h"
#include "core/crypto/crypto.h"
#include "core/external_profiler.h"
#include "core/input/input_default.h"
#include "core/input/input_map.h"
#include "core/io/file_access_network.h"
#include "core/io/file_access_pack.h"
#include "core/io/image_loader.h"
#include "core/io/ip.h"
#include "core/io/resource_loader.h"
#include "core/os/time.h"
#include "core/string_utils.inl"
#include "core/message_queue.h"
#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/register_core_types.h"
#include "core/resource/resource_manager.h"
#include "core/rotated_file_loger.h"
#include "core/script_debugger_local.h"
#include "core/script_language.h"
#include "core/translation.h"
#include "core/version.h"
#include "drivers/register_driver_types.h"
#include "main/app_icon.gen.h"
#include "main/main_timer_sync.h"
#include "main/performance.h"
#include "main/splash.gen.h"
#include "modules/register_module_types.h"
#include "plugins/plugin_registry_interface.h"

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
#include "servers/physics_server_2d.h"
#include "servers/physics_server_3d.h"
#include "core/string_formatter.h"
#include "servers/register_server_types.h"
#include "servers/rendering_server_callbacks.h"

// Reflection generation command
#include "core/reflection_support/reflection_data.h"
#include "core/reflection_support/reflection_generator.h"

#ifdef TOOLS_ENABLED
#include "core/doc_support/doc_data.h"
#include "editor/doc_data_class_path.gen.h"
#include "editor/doc/doc_builder.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/project_manager.h"
#include "editor/progress_dialog.h"
#endif

#include <QCoreApplication>
#include <QDir>
#include <entt/meta/resolve.hpp>
/* Static members */

// Singletons

// Initialized in setup()
static Engine *engine = nullptr;

static ProjectSettings *globals = nullptr;
static InputMap *input_map = nullptr;
static TranslationServer *translation_server = nullptr;
static Performance *performance = nullptr;

static PackedData *packed_data = nullptr;
static Time *time_singleton = nullptr;
static FileAccessNetworkClient *file_access_network_client = nullptr;
static ScriptDebugger *script_debugger = nullptr;
static MessageQueue *message_queue = nullptr;

// Initialized in setup2()
static AudioServer *audio_server = nullptr;
static CameraServer *camera_server = nullptr;
static ARVRServer *arvr_server = nullptr;
static PhysicsServer3D *physics_server_3d = nullptr;
static PhysicsServer2D *physics_server_2d = nullptr;
static NavigationServer *navigation_server = nullptr;
static Navigation2DServer *navigation_2d_server = nullptr;
static RenderingServerCallbacks *rendering_server_callbacks = nullptr;
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
static bool delta_sync_after_draw = false;
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
static bool debug_shader_fallbacks = false;
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

static String unescape_cmdline(StringView p_str) {
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
    // This must be defined BEFORE the 3d physics server is created,
    // otherwise it won't always show up in the project settings page.
    T_GLOBAL_DEF("physics/3d/godot_physics/bvh_collision_margin", 0.1f);
    ProjectSettings::get_singleton()->set_custom_property_info("physics/3d/godot_physics/bvh_collision_margin", PropertyInfo(VariantType::FLOAT, "physics/3d/godot_physics/bvh_collision_margin", PropertyHint::Range, "0.0,2.0,0.01"));

    /// 3D Physics Server
    physics_server_3d = PhysicsServerManager::new_server(ProjectSettings::get_singleton()->getT<StringName>(PhysicsServerManager::setting_property_name));
    if (!physics_server_3d) {
        // Physics server not found, Use the default physics
        physics_server_3d = PhysicsServerManager::new_default_server();
    }
    ERR_FAIL_COND(!physics_server_3d);
    physics_server_3d->init();

    /// 2D Physics server
    physics_server_2d = Physics2DServerManager::new_server(ProjectSettings::get_singleton()->getT<StringName>(Physics2DServerManager::setting_property_name));
    if (!physics_server_2d) {
        // Physics server not found, Use the default physics
        physics_server_2d = Physics2DServerManager::new_default_server();
    }
    ERR_FAIL_COND(!physics_server_2d);
    physics_server_2d->init();
}

void finalize_physics() {
    physics_server_3d->finish();
    memdelete(physics_server_3d);

    physics_server_2d->finish();
    memdelete(physics_server_2d);

    Physics2DServerManager::cleanup();
    PhysicsServerManager::cleanup();
}
void initialize_navigation_server() {
    ERR_FAIL_COND(navigation_server != nullptr);
    navigation_server = NavigationServerManager::new_default_server();
    Navigation2DServer::initialize_class();
    navigation_2d_server = memnew(Navigation2DServer);
}

void finalize_navigation_server() {
    memdelete(navigation_server);
    navigation_server = nullptr;
    memdelete(navigation_2d_server);
    navigation_2d_server = nullptr;
}

//#define DEBUG_INIT
#ifdef DEBUG_INIT
#define MAIN_PRINT(m_txt) print_line(m_txt)
#else
#define MAIN_PRINT(m_txt)
#endif

void print_help(const String &p_binary) {

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
    OS::get_singleton()->print("  --no-window                      Run with invisible window. Useful together with --script.\n");
    OS::get_singleton()->print("  --enable-vsync-via-compositor    When vsync is enabled, vsync via the OS' window compositor (Windows only).\n");
    OS::get_singleton()->print("  --disable-vsync-via-compositor   Disable vsync via the OS' window compositor (Windows only).\n");
    OS::get_singleton()->print("  --enable-delta-smoothing         When vsync is enabled, enabled frame delta smoothing.\n");
    OS::get_singleton()->print("  --disable-delta-smoothing        Disable frame delta smoothing.\n");
    OS::get_singleton()->print("\n");
#endif

    OS::get_singleton()->print("Debug options:\n");
    OS::get_singleton()->print("  -d, --debug                      Debug (local stdout debugger).\n");
    OS::get_singleton()->print("  -b, --breakpoints                Breakpoint list as source::line comma-separated pairs, no spaces (use %%20 instead).\n");
    OS::get_singleton()->print("  --profiling                      Enable profiling in the script debugger.\n");
    OS::get_singleton()->print("  --remote-debug <address>         Remote debug (<host/IP>:<port> address).\n");
#if defined(DEBUG_ENABLED) && !defined(SERVER_ENABLED)
    OS::get_singleton()->print("  --debug-collisions               Show collision shapes when running the scene.\n");
    OS::get_singleton()->print("  --debug_navigation               Show navigation polygons when running the scene.\n");
#endif
    OS::get_singleton()->print("  --frame-delay <ms>               Simulate high CPU load (delay each frame by <ms> milliseconds).\n");
    OS::get_singleton()->print("  --time-scale <scale>             Force time scale (higher values are faster, 1.0 is normal speed).\n");
    OS::get_singleton()->print("  --disable-render-loop            Disable render loop so rendering only occurs when called explicitly from script.\n");
    OS::get_singleton()->print("  --disable-crash-handler          Disable crash handler when supported by the platform code.\n");
    OS::get_singleton()->print("  --fixed-fps <fps>                Force a fixed number of frames per second. This setting disables real-time synchronization.\n");
    OS::get_singleton()->print("  --print-fps                      Print the frames per second to the stdout.\n");
    OS::get_singleton()->print("\n");

    OS::get_singleton()->print("Standalone tools:\n");
#if defined(DEBUG_ENABLED)
    OS::get_singleton()->print("  --gen-reflection <path>          Generate reflection data.\n");
#endif

    OS::get_singleton()->print("  -s, --script <script>            Run a script.\n");
    OS::get_singleton()->print("  --check-only                     Only parse for errors and quit (use with --script).\n");
#ifdef TOOLS_ENABLED
    OS::get_singleton()->print("  --export <preset> <path>         Export the project using the given preset and matching release template. The preset name should match one defined in export_presets.cfg.\n");
    OS::get_singleton()->print("                                   <path> should be absolute or relative to the project directory, and include the filename for the binary (e.g. 'builds/game.exe'). The target directory should exist.\n");
    OS::get_singleton()->print("  --export-debug <preset> <path>   Same as --export, but using the debug template.\n");
    OS::get_singleton()->print("  --export-pack <preset> <path>    Same as --export, but only export the game pack for the given preset. The <path> extension determines whether it will be in PCK or ZIP format.\n");
    OS::get_singleton()->print("  --doctool [<path>]               Dump the engine API reference to the given <path> (defaults to current dir) in XML format, merging if existing files are found.\n");
    OS::get_singleton()->print("  --no-docbase                     Disallow dumping the base types (used with --doctool).\n");
    OS::get_singleton()->print("  --build-solutions                Build the scripting solutions (e.g. for C# projects). Implies --editor and requires a valid project to edit.\n");
#ifdef DEBUG_METHODS_ENABLED
    OS::get_singleton()->print("  --gdnative-generate-json-api     Generate JSON dump of the Godot API for GDNative bindings.\n");
#endif
    OS::get_singleton()->print(").\n");
#endif
}
#include "core/plugin_interfaces/PluginDeclarations.h"
struct ArchivePluginResolver : public ResolverInterface
{
    PackedData *pack_data;
    ArchivePluginResolver(PackedData *pd) : pack_data(pd) {}

    bool new_plugin_detected(QObject *ob,const QJsonObject &/*metadata*/,const char *) override {

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
    bool new_plugin_detected(QObject *ob,const QJsonObject &/*metadata*/,const char *) override {

        bool res=false;
        auto interface = qobject_cast<ResourceLoaderInterface *>(ob);
        if(interface) {
            print_line(String("Adding resource loader plugin:")+ob->metaObject()->className());
            gResourceManager().add_resource_format_loader(interface);
            res=true;
        }
        return res;
    }
    void plugin_removed(QObject * ob)  override  {
        auto interface = qobject_cast<ResourceLoaderInterface *>(ob);
        if(interface) {
            print_line(String("Removing resource loader plugin:")+ob->metaObject()->className());
            gResourceManager().remove_resource_format_loader(interface);
        }
    }
};
struct ModulePluginResolver : public ResolverInterface {

    bool new_plugin_detected(QObject *ob,const QJsonObject &/*metadata*/,const char *) override {

        bool res = false;
        auto interface = qobject_cast<ModuleInterface*>(ob);
        if (interface) {
            print_line(String("Adding module plugin:") + ob->metaObject()->className());
            res = interface->register_module();
        }
        return res;
    }
    void plugin_removed(QObject *ob) override {
        auto interface = qobject_cast<ModuleInterface*>(ob);
        if (interface) {
            print_line(String("Removing resource loader plugin:") + ob->metaObject()->className());
            interface->unregister_module();
        }
    }
};
void Main::dumpReflectedTypes() {
    using namespace entt;
    printf("Types known after initial setup\n");
    for (auto reg_type : entt::resolve()) {
        printf("%.*s data:\n", (uint32_t)reg_type.info().name().size(), reg_type.info().name().data());
        for (auto data : reg_type.data()) {
            auto data_type(data.type());
            auto name(data_type.info().name());
            printf("    %.*s\n", (uint32_t)name.size(), name.data());
            if (data_type.is_enum()) {
                for (auto enum_entry : data_type.data()) {
                    auto enum_entry_val(enum_entry.get({}));
                    enum_entry_val.allow_cast<int>();
                    auto disp_name = enum_entry.prop("DisplayName"_hs);
                    printf("    E: %d ", enum_entry_val.cast<int>());
                    if (disp_name) {
                        printf(" [%s]", disp_name.value().get({}).cast<StringName>().asCString());
                    }
                    printf("\n");
                }
            }
            // ...
        }

    }
}
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
#if defined(DEBUG_ENABLED) && !defined(NO_THREADS)
    static_assert(
        std::atomic<uint32_t>::is_always_lock_free &&
        std::atomic<uint64_t>::is_always_lock_free &&
        std::atomic<float>::is_always_lock_free &&
        std::atomic_bool::is_always_lock_free
    ,
    "Your compiler does not support lockless atomics.");
#endif
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
    OS *os = OS::get_singleton();
    os->initialize_core();
    engine = memnew(Engine);

    MAIN_PRINT("Main: Initialize CORE");

    register_core_types();
    register_core_driver_types();

    MAIN_PRINT("Main: Initialize Globals");

    Thread::main_thread_id = Thread::get_caller_id();

    ProjectSettings::initialize_class();
    InputMap::initialize_class();
    TranslationServer::initialize_class();
    Performance::initialize_class();
    Time::initialize_class();

    globals = memnew(ProjectSettings);
    input_map = memnew(InputMap);
    time_singleton = memnew(Time);

    register_core_settings(); //here globals is present

    translation_server = memnew(TranslationServer);
    performance = memnew(Performance);
    ClassDB::register_class<Performance>();
    engine->add_singleton(Engine::Singleton("Performance", performance));

    GLOBAL_DEF(StringName("debug/settings/crash_handler/message"), String("Please include this when reporting the bug on https://github.com/godotengine/godot/issues"));

    MAIN_PRINT("Main: Parse CMDLine");

    /* argument parsing and main creation */
    Vector<String> args;
    Vector<String> main_args;
    QStringList q_args = qApp->arguments();
    String execpath = StringUtils::to_utf8(q_args.takeFirst());
    ProjectSettings* project_settings = ProjectSettings::get_singleton();

    for (const QString &arg : q_args) {
        args.push_back(StringUtils::to_utf8(arg));
    }

    Vector<String>::iterator I = args.begin();

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

    Vector<StringView> breakpoints;
    bool use_custom_res = true;
    bool force_res = false;
    bool saw_vsync_via_compositor_override = false;
    bool delta_smoothing_override = false;
#ifdef TOOLS_ENABLED
    bool found_project = false;
#endif

    packed_data = PackedData::get_singleton();
    if (!packed_data)
        packed_data = memnew(PackedData);

    add_plugin_resolver(new ArchivePluginResolver(packed_data));


    I = args.begin();
    while (I!= args.end()) {
#ifdef OSX_ENABLED
        // Ignore the process serial number argument passed by macOS Gatekeeper.
        // Otherwise, Godot would try to open a non-existent project on the first start and abort.
        if (I->starts_with("-psn_")) {
            ++I;
            continue;
        }
#endif
        Vector<String>::iterator N = eastl::next(I);

        if (*I == "-h" || *I == "--help" || *I == "/?") { // display help

            show_help = true;
            goto error;

        } else if (*I == "--version") {

            print_line(get_full_version_string());
            goto error;

        } else if (*I == "-v" || *I == "--verbose") { // verbose output

            os->_verbose_stdout = true;
        } else if (*I == "--quiet") { // quieter output

            quiet_stdout = true;

        } else if (*I == "--audio-driver") { // audio driver

            if (N != args.end()) {

                audio_driver = StringName(*N);
                bool found = false;
                for (int i = 0; i < os->get_audio_driver_count(); i++) {
                    if (audio_driver == os->get_audio_driver_name(i)) {
                        found = true;
                    }
                }

                if (!found) {
                    os->print(FormatVE("Unknown audio driver '%s', aborting.\nValid options are ", audio_driver.asCString()));

                    for (int i = 0; i < os->get_audio_driver_count(); i++) {
                        if (i == os->get_audio_driver_count() - 1) {
                            os->print(" and ");
                        } else if (i != 0) {
                            os->print(", ");
                        }

                        os->print(FormatVE("'%s'",OS::get_singleton()->get_audio_driver_name(i)));
                    }

                    os->print(".\n");

                    goto error;
                }
                ++N;
            } else {
                os->print("Missing audio driver argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--video-driver") { // force video driver

            if (N != args.end()) {

                video_driver = StringName(*N);
                bool found = false;
                for (int i = 0; i < os->get_video_driver_count(); i++) {
                    if (video_driver == os->get_video_driver_name(i)) {
                        found = true;
                    }
                }

                if (!found) {
                    os->print(FormatVE("Unknown video driver '%s', aborting.\nValid options are ", video_driver.asCString()));

                    for (int i = 0; i < os->get_video_driver_count(); i++) {
                        if (i == os->get_video_driver_count() - 1) {
                            os->print(" and ");
                        } else if (i != 0) {
                            os->print(", ");
                        }

                        os->print(FormatVE("'%s'", OS::get_singleton()->get_video_driver_name(i)));
                    }

                    os->print(".\n");

                    goto error;
                }
                ++N;
            } else {
                os->print("Missing video driver argument, aborting.\n");
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

                    os->print("Invalid resolution '"+vm+"', it should be e.g. '1280x720'.\n");
                    goto error;
                }

                int w = StringUtils::to_int(StringUtils::get_slice(vm,"x", 0));
                int h = StringUtils::to_int(StringUtils::get_slice(vm,"x", 1));

                if (w <= 0 || h <= 0) {

                    os->print("Invalid resolution '"+vm+"', width and height must be above 0.\n");
                    goto error;
                }

                video_mode.width = w;
                video_mode.height = h;
                force_res = true;

                ++N;
            } else {
                os->print("Missing resolution argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--position") { // set window position

            if (N != args.end()) {

                String vm = *N;

                if (not StringUtils::contains(vm,',')) { // invalid parameter format

                    os->print("Invalid position '"+vm+"', it should be e.g. '80,128'.\n");
                    goto error;
                }

                int x = StringUtils::to_int(StringUtils::get_slice(vm,",", 0));
                int y = StringUtils::to_int(StringUtils::get_slice(vm,",", 1));

                init_custom_pos = Point2(x, y);
                init_use_custom_pos = true;

                ++N;
            } else {
                os->print("Missing position argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--low-dpi") { // force low DPI (macOS only)

            force_lowdpi = true;
        } else if (*I == "--no-window") { // run with an invisible window

            os->set_no_window_mode(true);
        } else if (*I == "--enable-vsync-via-compositor") {

            video_mode.vsync_via_compositor = true;
            saw_vsync_via_compositor_override = true;
        } else if (*I == "--disable-vsync-via-compositor") {

            video_mode.vsync_via_compositor = false;
            saw_vsync_via_compositor_override = true;
        } else if (*I == "--enable-delta-smoothing") {
            OS::get_singleton()->set_delta_smoothing(true);
            delta_smoothing_override = true;
        } else if (*I == "--disable-delta-smoothing") {
            OS::get_singleton()->set_delta_smoothing(false);
            delta_smoothing_override = true;
#endif
        } else if (*I == "--profiling") { // enable profiling

            use_debug_profiler = true;

        } else if (*I == "-l" || *I == "--language") { // language

            if (N != args.end()) {

                locale = *N;
                ++N;
            } else {
                os->print("Missing language argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--remote-fs") { // remote filesystem

            if (N != args.end()) {

                remotefs = (*N);
                ++N;
            } else {
                os->print("Missing remote filesystem address, aborting.\n");
                goto error;
            }
        } else if (*I == "--remote-fs-password") { // remote filesystem password

            if (N != args.end()) {

                remotefs_pass = *N;
                ++N;
            } else {
                os->print("Missing remote filesystem password, aborting.\n");
                goto error;
            }
        } else if (*I == "--render-thread") { // render thread mode

            if (N != args.end()) {

                if (*N == "safe") {
                    rtm = OS::RENDER_THREAD_SAFE;
                } else if (*N == "separate") {
                    rtm = OS::RENDER_SEPARATE_THREAD;
                }
                ++N;
            } else {
                os->print("Missing render thread mode argument, aborting.\n");
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
                if (os->set_cwd(p) == OK) {
                    //nothing
                } else {
                    project_path = *N; //use project_path instead
                }
                ++N;
            } else {
                os->print("Missing relative or absolute path, aborting.\n");
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
                os->print("Missing list of breakpoints, aborting.\n");
                goto error;
            }

        } else if (*I == "--frame-delay") { // force frame delay

            if (N != args.end()) {

                frame_delay = StringUtils::to_int(*N);
                ++N;
            } else {
                os->print("Missing frame delay argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--time-scale") { // force time scale

            if (N != args.end()) {

                Engine::get_singleton()->set_time_scale(StringUtils::to_float(*N));
                ++N;
            } else {
                os->print("Missing time scale argument, aborting.\n");
                goto error;
            }

        } else if (*I == "--main-pack") {

            if (N != args.end()) {

                main_pack = *N;
                ++N;
            } else {
                os->print("Missing path to main pack file, aborting.\n");
                goto error;
            }

        } else if (*I == "-d" || *I == "--debug") {
            debug_mode = "local";
            OS::get_singleton()->_debug_stdout = true;
#if defined(DEBUG_ENABLED) && !defined(SERVER_ENABLED)
        } else if (*I == "--debug-collisions") {
            debug_collisions = true;
        } else if (*I == "--debug_navigation") {
            debug_navigation = true;
#endif
        } else if (*I == "--remote-debug") {
            if (N != args.end()) {

                debug_mode = "remote";
                debug_host = *N;
                if (not StringUtils::contains(debug_host,':')) { // wrong address
                    os->print("Invalid debug host address, it should be of the form <host/IP>:<port>.\n");
                    goto error;
                }
                ++N;
            } else {
                os->print("Missing remote debug host address, aborting.\n");
                goto error;
            }
        } else if (*I == "--allow_focus_steal_pid") { // not exposed to user
            if (N != args.end()) {

                allow_focus_steal_pid = StringUtils::to_int64(*N);
                ++N;
            } else {
                os->print("Missing editor PID argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--disable-render-loop") {
            disable_render_loop = true;
        } else if (*I == "--fixed-fps") {
            if (N != args.end()) {
                fixed_fps = StringUtils::to_int(*N);
                ++N;
            } else {
                os->print("Missing fixed-fps argument, aborting.\n");
                goto error;
            }
        } else if (*I == "--print-fps") {
            print_fps = true;
        } else if (*I == "--disable-crash-handler") {
            os->disable_crash_handler();
        } else if (*I == "--skip-breakpoints") {
            skip_breakpoints = true;
        } else {
            main_args.push_back(*I);
        }

        I = N;
    }
#ifdef TOOLS_ENABLED
    if (editor && project_manager) {
        os->print("Error: Command line arguments implied opening both editor and project manager, which is not possible. Aborting.\n");
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
            os->printerr(("Could not connect to remotefs: "+remotefs+":"+::to_string(port)+".\n").c_str());
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
        OS::get_singleton()->print(error_msg.c_str());
        OS::get_singleton()->alert(error_msg);

        goto error;
#endif
    }
    // Initialize user data dir.
    OS::get_singleton()->ensure_user_data_dir();

    GLOBAL_DEF("memory/limits/multithreaded_server/rid_pool_prealloc", 60);

    project_settings->set_custom_property_info("memory/limits/multithreaded_server/rid_pool_prealloc",
            PropertyInfo(VariantType::INT, "memory/limits/multithreaded_server/rid_pool_prealloc", PropertyHint::Range,
                    "0,500,1")); // No negative and limit to 500 due to crashes
    GLOBAL_DEF("network/limits/debugger_stdout/max_chars_per_second", 2048);
    project_settings->set_custom_property_info("network/limits/debugger_stdout/max_chars_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_chars_per_second", PropertyHint::Range,
                    "0, 4096, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_messages_per_frame", 10);
    project_settings->set_custom_property_info("network/limits/debugger_stdout/max_messages_per_frame",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_messages_per_frame", PropertyHint::Range,
                    "0, 20, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_errors_per_second", 100);
    project_settings->set_custom_property_info("network/limits/debugger_stdout/max_errors_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_errors_per_second", PropertyHint::Range,
                    "0, 200, 1, or_greater"));
    GLOBAL_DEF("network/limits/debugger_stdout/max_warnings_per_second", 100);
    project_settings->set_custom_property_info("network/limits/debugger_stdout/max_warnings_per_second",
            PropertyInfo(VariantType::INT, "network/limits/debugger_stdout/max_warnings_per_second", PropertyHint::Range,
                    "0, 200, 1, or_greater"));

    if (debug_mode == "remote") {

        ScriptDebuggerRemote *sdr = memnew(ScriptDebuggerRemote);
        uint16_t debug_port = 6007;
        if (StringUtils::contains(debug_host,':')) {
            int sep_pos = StringUtils::rfind(debug_host,":");
            debug_port = StringUtils::to_int(StringUtils::substr(debug_host,sep_pos + 1, debug_host.length()));
            debug_host = StringUtils::substr(debug_host,0, sep_pos);
        }
        Error derr = sdr->connect_to_host(debug_host, debug_port);

        sdr->set_skip_breakpoints(skip_breakpoints);

        if (derr != OK) {
            memdelete(sdr);
        } else {
            script_debugger = sdr;
            sdr->set_allow_focus_steal_pid(allow_focus_steal_pid);
        }
    } else if (debug_mode == "local") {

        script_debugger = memnew(ScriptDebuggerLocal);
        os->initialize_debugging();
    }

    if (script_debugger) {
        //there is a debugger, parse breakpoints

        for (StringView bp : breakpoints) {

            auto sp = StringUtils::rfind(bp,':');
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
    // Only flush stdout in debug builds by default, as spamming `print()` will
    // decrease performance if this is enabled.
    GLOBAL_DEF_RST("application/run/flush_stdout_on_print", false);
    GLOBAL_DEF_RST("application/run/flush_stdout_on_print.debug", true);

    GLOBAL_DEF("logging/file_logging/enable_file_logging", false);
    // Only file logging by default on desktop platforms as logs can't be
    // accessed easily on mobile/Web platforms (if at all).
    // This also prevents logs from being created for the editor instance, as feature tags
    // are disabled while in the editor (even if they should logically apply).
    GLOBAL_DEF("logging/file_logging/enable_file_logging.pc", true);
    GLOBAL_DEF("logging/file_logging/log_path", "user://logs/log.txt");
    GLOBAL_DEF("logging/file_logging/max_log_files", 10);
    project_settings->set_custom_property_info(
            "logging/file_logging/max_log_files", PropertyInfo(VariantType::INT, "logging/file_logging/max_log_files",
                                                          PropertyHint::Range, "0,20,1,or_greater")); // no negative numbers
    if (FileAccess::get_create_func(FileAccess::ACCESS_USERDATA) &&
            GLOBAL_GET("logging/file_logging/enable_file_logging").as<bool>()) {
        String base_path = T_GLOBAL_GET<String>("logging/file_logging/log_path");
        int max_files = T_GLOBAL_GET<int>("logging/file_logging/max_log_files");
        os->add_logger(memnew(RotatedFileLogger(base_path, max_files)));
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

    if (main_args.empty() && T_GLOBAL_DEF<String>("application/run/main_scene", "").empty()) {
#ifdef TOOLS_ENABLED
        if (!editor && !project_manager) {
#endif
            const String error_msg = "Error: Can't run project: no main scene defined in the project.\n";
            OS::get_singleton()->print(error_msg);
            OS::get_singleton()->alert(error_msg);
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

    if ((project_settings->getT<bool>("application/run/disable_stdout"))) {
        quiet_stdout = true;
    }
    if ((project_settings->getT<bool>("application/run/disable_stderr"))) {
        _print_error_enabled = false;
    }

    if (quiet_stdout) {
        _print_line_enabled = false;
    }

    Logger::set_flush_stdout_on_print(ProjectSettings::get_singleton()->get("application/run/flush_stdout_on_print").as<bool>());

    os->set_cmdline(execpath, eastl::move(main_args));

    GLOBAL_DEF("rendering/quality/driver/driver_name", "GLES3");
    project_settings->set_custom_property_info("rendering/quality/driver/driver_name",
            PropertyInfo(VariantType::STRING, "rendering/quality/driver/driver_name", PropertyHint::Enum, "GLES3")); //GLES2,
    if (video_driver.empty()) {
        video_driver = T_GLOBAL_GET<StringName>("rendering/quality/driver/driver_name");
    }

    GLOBAL_DEF("rendering/quality/driver/fallback_to_gles2", false);

    // Assigning here even though it's GLES2-specific, to be sure that it appears in docs
    GLOBAL_DEF("rendering/2d/options/use_nvidia_rect_flicker_workaround", false);
    GLOBAL_DEF("display/window/size/width", 1024);
    project_settings->set_custom_property_info(
            "display/window/size/width", PropertyInfo(VariantType::INT, "display/window/size/width", PropertyHint::Range,
                                                 "0,7680,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/height", 600);
    project_settings->set_custom_property_info(
            "display/window/size/height", PropertyInfo(VariantType::INT, "display/window/size/height", PropertyHint::Range,
                                                  "0,4320,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/resizable", true);
    GLOBAL_DEF("display/window/size/borderless", false);
    GLOBAL_DEF("display/window/size/fullscreen", false);
    GLOBAL_DEF("display/window/size/always_on_top", false);
    GLOBAL_DEF("display/window/size/test_width", 0);
    project_settings->set_custom_property_info(
            "display/window/size/test_width", PropertyInfo(VariantType::INT, "display/window/size/test_width",
                                                      PropertyHint::Range, "0,7680,or_greater")); // 8K resolution
    GLOBAL_DEF("display/window/size/test_height", 0);
    project_settings->set_custom_property_info(
            "display/window/size/test_height", PropertyInfo(VariantType::INT, "display/window/size/test_height",
                                                       PropertyHint::Range, "0,4320,or_greater")); // 8K resolution

    if (use_custom_res) {

        if (!force_res) {
            video_mode.width = T_GLOBAL_GET<int>("display/window/size/width");
            video_mode.height = T_GLOBAL_GET<int>("display/window/size/height");

            if (globals->has_setting("display/window/size/test_width") && globals->has_setting("display/window/size/test_height")) {
                int tw = globals->getT<int>("display/window/size/test_width");
                if (tw > 0) {
                    video_mode.width = tw;
                }
                int th = globals->getT<int>("display/window/size/test_height");
                if (th > 0) {
                    video_mode.height = th;
                }
            }
        }

        video_mode.resizable = T_GLOBAL_GET<bool>("display/window/size/resizable");
        video_mode.borderless_window = T_GLOBAL_GET<bool>("display/window/size/borderless");
        video_mode.fullscreen = T_GLOBAL_GET<bool>("display/window/size/fullscreen");
        video_mode.always_on_top = T_GLOBAL_GET<bool>("display/window/size/always_on_top");
    }

    if (!force_lowdpi) {
        os->_allow_hidpi = T_GLOBAL_DEF("display/window/dpi/allow_hidpi", false);
    }


    video_mode.use_vsync = GLOBAL_DEF_T_RST("display/window/vsync/use_vsync", true,bool);
    os->_use_vsync = video_mode.use_vsync;

    if (!saw_vsync_via_compositor_override) {
        // If one of the command line options to enable/disable vsync via the
        // window compositor ("--enable-vsync-via-compositor" or
        // "--disable-vsync-via-compositor") was present then it overrides the
        // project setting.
        video_mode.vsync_via_compositor = T_GLOBAL_DEF("display/window/vsync/vsync_via_compositor", false);
    }

    os->_vsync_via_compositor = video_mode.vsync_via_compositor;

    os->_allow_layered = T_GLOBAL_DEF("display/window/per_pixel_transparency/allowed", false);
    video_mode.layered = T_GLOBAL_DEF("display/window/per_pixel_transparency/enabled", false);

    GLOBAL_DEF("rendering/quality/intended_usage/framebuffer_allocation", 2);

    if (editor || project_manager) {
        // The editor and project manager always detect and use hiDPI if needed
        os->_allow_hidpi = true;
        os->_allow_layered = false;
    }

    Engine::get_singleton()->_gpu_pixel_snap = T_GLOBAL_DEF("rendering/2d/snapping/use_gpu_pixel_snap", false);

    os->_keep_screen_on = T_GLOBAL_DEF("display/window/energy_saving/keep_screen_on", true);
    if (rtm == -1) {
        rtm = T_GLOBAL_DEF("rendering/threads/thread_model", OS::RENDER_THREAD_SAFE);
    }
    T_GLOBAL_DEF("rendering/threads/thread_safe_bvh", false);

    if (rtm >= OS::RENDER_THREAD_SAFE && rtm < OS::RENDER_THREAD_MAX) {
        if (editor) {
            rtm = OS::RENDER_THREAD_SAFE;
        }
        os->_render_thread_mode = OS::RenderThreadMode(rtm);
    }

    /* Determine audio and video drivers */

    for (int i = 0; i < os->get_video_driver_count(); i++) {

        if (video_driver == os->get_video_driver_name(i)) {

            video_driver_idx = i;
            break;
        }
    }

    if (video_driver_idx < 0) {
        video_driver_idx = 0;
    }

    if (audio_driver.empty()) { // specified in project.godot
        audio_driver = T_GLOBAL_DEF<StringName>("audio/driver", StringName(OS::get_singleton()->get_audio_driver_name(0)),true,true);
    }

    for (int i = 0; i < os->get_audio_driver_count(); i++) {

        if (audio_driver == os->get_audio_driver_name(i)) {

            audio_driver_idx = i;
            break;
        }
    }

    if (audio_driver_idx < 0) {
        audio_driver_idx = 0;
    }

    //String orientation = T_GLOBAL_DEF<String>("display/window/handheld/orientation", "landscape");
    os->set_screen_orientation(OS::SCREEN_LANDSCAPE);

    Engine::get_singleton()->set_iterations_per_second(T_GLOBAL_DEF<int>("physics/common/physics_fps", 60));
    project_settings->set_custom_property_info("physics/common/physics_fps", PropertyInfo(VariantType::INT, "physics/common/physics_fps", PropertyHint::Range, "1,1000,1,or_greater"));
    Engine::get_singleton()->set_physics_jitter_fix(T_GLOBAL_DEF<float>("physics/common/physics_jitter_fix", 0.5));
    Engine::get_singleton()->set_target_fps(T_GLOBAL_DEF<int>("debug/settings/fps/force_fps", 0));
    project_settings->set_custom_property_info("debug/settings/fps/force_fps", PropertyInfo(VariantType::INT, "debug/settings/fps/force_fps", PropertyHint::Range, "0,1000,1,or_greater"));
    GLOBAL_DEF("physics/common/enable_pause_aware_picking", false);

    T_GLOBAL_DEF("debug/settings/stdout/print_fps", false);
    T_GLOBAL_DEF("debug/settings/stdout/verbose_stdout", false);

    if (!OS::get_singleton()->_verbose_stdout) { // Not manually overridden.
        OS::get_singleton()->_verbose_stdout = T_GLOBAL_GET<bool>("debug/settings/stdout/verbose_stdout");
    }
    if (frame_delay == 0) {
        frame_delay = T_GLOBAL_DEF<int>("application/run/frame_delay_msec", 0);
        project_settings->set_custom_property_info("application/run/frame_delay_msec", PropertyInfo(VariantType::INT, "application/run/frame_delay_msec", PropertyHint::Range, "0,100,1,or_greater")); // No negative numbers
    }

    os->set_low_processor_usage_mode(T_GLOBAL_DEF("application/run/low_processor_mode", false));
    os->set_low_processor_usage_mode_sleep_usec(T_GLOBAL_DEF<int>("application/run/low_processor_mode_sleep_usec", 6900)); // Roughly 144 FPS
    project_settings->set_custom_property_info("application/run/low_processor_mode_sleep_usec", PropertyInfo(VariantType::INT, "application/run/low_processor_mode_sleep_usec", PropertyHint::Range, "0,33200,1,or_greater")); // No negative numbers
    delta_sync_after_draw = T_GLOBAL_DEF<bool>("application/run/delta_sync_after_draw", false);
    GLOBAL_DEF("application/run/delta_smoothing", true);
    if (!delta_smoothing_override) {
        OS::get_singleton()->set_delta_smoothing(T_GLOBAL_GET<bool>("application/run/delta_smoothing"));
    }

    Engine::get_singleton()->set_frame_delay(frame_delay);

    message_queue = memnew(MessageQueue);

    dumpReflectedTypes();

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

    memdelete(performance);
    memdelete(input_map);
    memdelete(time_singleton);
    memdelete(translation_server);
    memdelete(globals);
    memdelete(engine);
    memdelete(script_debugger);
    memdelete(packed_data);
    memdelete(file_access_network_client);


    unregister_core_driver_types();
    unregister_core_types();

    os->_cmdline.clear();

    memdelete(message_queue);
    os->finalize_core();
    locale.clear();

    return ERR_INVALID_PARAMETER;
}

Error Main::setup2() {

    String plugins_dir = String(PathUtils::path(OS::get_singleton()->get_executable_path())) + "/plugins";
    load_all_plugins(plugins_dir.c_str());

    // Print engine name and version
    print_line(String(VERSION_NAME) + " v" + get_full_version_string() + " - " + String(VERSION_WEBSITE));
    Thread::main_thread_id = Thread::get_caller_id();

#ifdef UNIX_ENABLED
    // Print warning before initializing audio.
    if (OS::get_singleton()->get_environment("USER") == "root" &&
            !OS::get_singleton()->has_environment("GODOT_SILENCE_ROOT_WARNING")) {
        WARN_PRINT("Started the engine as `root`/superuser. This is a security risk, and subsystems like audio may not "
                   "work correctly.\nSet the environment variable `GODOT_SILENCE_ROOT_WARNING` to 1 to silence this "
                   "warning.");
    }
#endif

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

    MAIN_PRINT("Main: Load Boot Image");

    Color clear = T_GLOBAL_DEF("rendering/environment/default_clear_color", Color(0.3f, 0.3f, 0.3f));
    RenderingServer::get_singleton()->set_default_clear_color(clear);

    if (show_logo) { //boot logo!
        String boot_logo_path = T_GLOBAL_DEF("application/boot_splash/image", String());
        bool boot_logo_scale = T_GLOBAL_DEF("application/boot_splash/fullsize", true);
        bool boot_logo_filter = T_GLOBAL_DEF("application/boot_splash/use_filter", true);
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

        const Color boot_bg_color = T_GLOBAL_DEF("application/boot_splash/bg_color", boot_splash_bg_color);
        if (boot_logo) {
            OS::get_singleton()->_msec_splash = OS::get_singleton()->get_ticks_msec();
            RenderingServer::get_singleton()->set_boot_image(boot_logo, boot_bg_color, boot_logo_scale, boot_logo_filter);

        } else {
#ifndef NO_DEFAULT_BOOT_LOGO

            MAIN_PRINT("Main: Create bootsplash");
            Ref<Image> splash(make_ref_counted<Image>(boot_splash_png));
            MAIN_PRINT("Main: ClearColor");
            RenderingServer::get_singleton()->set_default_clear_color(boot_bg_color);
            MAIN_PRINT("Main: Image");
            RenderingServer::get_singleton()->set_boot_image(splash, boot_bg_color, false);
#endif
        }

#ifdef TOOLS_ENABLED
        if (OS::get_singleton()->get_bundle_icon_path().empty()) {
            Ref<Image> icon(make_ref_counted<Image>(app_icon_png));
            OS::get_singleton()->set_icon(icon);
        }
#endif
    }

    MAIN_PRINT("Main: DCC");
    RenderingServer::get_singleton()->set_default_clear_color(T_GLOBAL_DEF("rendering/environment/default_clear_color", Color(0.3f, 0.3f, 0.3f)));

    GLOBAL_DEF("application/config/icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/icon",
            PropertyInfo(VariantType::STRING, "application/config/icon", PropertyHint::File, "*.png,*.webp,*.svg"));

    GLOBAL_DEF("application/config/macos_native_icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/macos_native_icon", PropertyInfo(VariantType::STRING, "application/config/macos_native_icon", PropertyHint::File, "*.icns"));

    GLOBAL_DEF("application/config/windows_native_icon", String());
    ProjectSettings::get_singleton()->set_custom_property_info("application/config/windows_native_icon", PropertyInfo(VariantType::STRING, "application/config/windows_native_icon", PropertyHint::File, "*.ico"));

    InputDefault *id = object_cast<InputDefault>(Input::get_singleton());
    if (id) {
        agile_input_event_flushing = T_GLOBAL_DEF<bool>("input_devices/buffering/agile_event_flushing", false);
        if (T_GLOBAL_DEF("input_devices/pointing/emulate_touch_from_mouse", false) && !(editor || project_manager)) {
            if (!OS::get_singleton()->has_touchscreen_ui_hint()) {
                //only if no touchscreen ui hint, set emulation
                id->set_emulate_touch_from_mouse(true);
            }
        }

        id->set_emulate_mouse_from_touch(T_GLOBAL_DEF("input_devices/pointing/emulate_mouse_from_touch", true));
    }
    MAIN_PRINT("Main: Load Translations and Remaps");

    translation_server->setup(); //register translations, load them, etc.
    if (!locale.empty()) {
        translation_server->set_locale(locale);
    }
    translation_server->load_translations();
    gResourceRemapper().load_translation_remaps(); //load remaps for resources

    gResourceRemapper().load_path_remaps();

    MAIN_PRINT("Main: Load Scene Types");

    register_scene_types();

#ifdef TOOLS_ENABLED
    ClassDB::set_current_api(ClassDB_APIType::API_EDITOR);
    EditorNode::register_editor_types();

    ClassDB::set_current_api(ClassDB_APIType::API_CORE);

#endif


    MAIN_PRINT("Main: Load Modules, Physics, Drivers, Scripts");

    add_plugin_resolver(new ResourcePluginResolver);
    add_plugin_resolver(new ModulePluginResolver);

    //register_platform_apis();
    register_module_types();
    // Theme needs modules to be initialized so that sub-resources can be loaded.
    initialize_theme();

    GLOBAL_DEF("display/mouse_cursor/custom_image", String());
    GLOBAL_DEF("display/mouse_cursor/custom_image_hotspot", Vector2());
    GLOBAL_DEF("display/mouse_cursor/tooltip_position_offset", Point2(10, 10));
    ProjectSettings::get_singleton()->set_custom_property_info("display/mouse_cursor/custom_image",
            PropertyInfo(VariantType::STRING, "display/mouse_cursor/custom_image", PropertyHint::File, "*.png,*.webp"));

    if (!ProjectSettings::get_singleton()->getT<String>("display/mouse_cursor/custom_image").empty()) {

        Ref<Texture> cursor = dynamic_ref_cast<Texture>(
                gResourceManager().load(ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image").as<String>()));
        if (cursor) {
            Vector2 hotspot = ProjectSettings::get_singleton()->getT<Vector2>("display/mouse_cursor/custom_image_hotspot");
            Input::get_singleton()->set_custom_mouse_cursor(cursor, Input::CURSOR_ARROW, hotspot);
        }
    }

    camera_server = CameraServer::create();

    initialize_physics();
    initialize_navigation_server();
    register_server_singletons();

    register_driver_types();
    const Vector<String> & args(OS::get_singleton()->get_cmdline_args());
    const auto refl_idx = args.index_of("--gen-reflection");
    bool reflection_requested = refl_idx != args.size();
    // This loads global classes, so it must happen before custom loaders and savers are registered
    // but if we're generating reflection data, we encounter chicken&egg problem:
    //   script language assemblies/libraries need reflected data to be built
    //   initializing scripting language needs the assemblies/libraries
    if(!reflection_requested) {
        ScriptServer::init_languages();
    }
#ifdef DEBUG_METHODS_ENABLED
    if (reflection_requested) {
        String tgt_dir = (refl_idx+1)<args.size() ? args[refl_idx+1] : ".";
        ReflectionData core_rd;
        _initialize_reflection_data(core_rd,ReflectionSource::Core);
        if(!core_rd.save_to_file(PathUtils::plus_file(tgt_dir,"GodotCore.json"))) {
            print_error("Failed to save reflection data json file.");
        }
#ifdef TOOLS_ENABLED
        ReflectionData editor_rd;
        _initialize_reflection_data(editor_rd,ReflectionSource::Editor);
        if(!editor_rd.save_to_file(PathUtils::plus_file(tgt_dir,"GodotEditor.json"))) {
            print_error("Failed to save reflection data json file.");
        }
#endif
        _start_success = true;
        cleanup();
        exit(0);
    }
#endif


    audio_server->load_default_bus_layout();

    if (use_debug_profiler && script_debugger) {
        script_debugger->profiling_start();
    }
    rendering_server_callbacks = memnew(RenderingServerCallbacks);
    RenderingServer::get_singleton()->callbacks_register(rendering_server_callbacks);
    _start_success = true;
    locale.clear();

    ClassDB::set_current_api(ClassDB_APIType::API_NONE); //no more api is registered at this point

    print_verbose("CORE API HASH: " + itos(ClassDB::get_api_hash(ClassDB_APIType::API_CORE)));
    print_verbose("EDITOR API HASH: " + itos(ClassDB::get_api_hash(ClassDB_APIType::API_EDITOR)));
    MAIN_PRINT("Main: Done");

    return OK;
}

// everything the main loop needs to know about frame timings

static MainTimerSync main_timer_sync;

bool Main::start() {

    ERR_FAIL_COND_V(!_start_success, false);

    bool hasicon = false;
    String doc_tool_path;
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

    const Vector<String> &args(OS::get_singleton()->get_cmdline_args());
    for (Vector<String>::const_iterator i = args.begin(); i!=args.end(); ++i) {
        Vector<String>::const_iterator next = i;
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
        } else if (i->length() && i->at(0) != '-' && positional_arg.empty()) {
            positional_arg = *i;

            if (StringUtils::ends_with(positional_arg,".scn") ||
                StringUtils::ends_with(positional_arg,".tscn") ||
                StringUtils::ends_with(positional_arg,".escn") ||
                StringUtils::ends_with(positional_arg,".res") ||
                StringUtils::ends_with(positional_arg,".tres")) {
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
                doc_tool_path = *next;
                if (doc_tool_path.starts_with('-')) {
                    // Assuming other command line arg, so default to cwd.
                    doc_tool_path = ".";
                    parsed_pair = false;
                }
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
        } else if (*i == "--doctool") {
            // Handle case where no path is given to --doctool.
            doc_tool_path = ".";
        }
    }

#ifdef TOOLS_ENABLED
    if (!doc_tool_path.empty()) {

        Engine::get_singleton()->set_editor_hint(true); // Needed to instance editor-only classes for their default values
        {
            DirAccessRef da = DirAccess::open(doc_tool_path);
            ERR_FAIL_COND_V_MSG(!da, false, "Argument supplied to --doctool must be a valid directory path.");
        }
        DocData doc;
        generate_docs_from_running_program(doc,doc_base);

        DocData docsrc;
        HashMap<String, UIString> doc_data_classes;
        HashSet<String> checked_paths;
        print_line("Loading docs...");

        for (int i = 0; i < _doc_data_class_path_count; i++) {
            String path = PathUtils::plus_file(doc_tool_path,_doc_data_class_paths[i].path);
            String name(_doc_data_class_paths[i].name);
            doc_data_classes[name] = path.c_str();
            if (!checked_paths.contains(path)) {
                checked_paths.insert(path);
                // Create the module documentation directory if it doesn't exist
                DirAccess *da = DirAccess::create_for_path(path);
                da->make_dir_recursive(path);
                memdelete(da);
                docsrc.load_classes(path.c_str());
                print_line("Loading docs from: " + path);
            }
        }

        String index_path = PathUtils::plus_file(doc_tool_path,"doc/classes");
        // Create the main documentation directory if it doesn't exist
        DirAccess *da = DirAccess::create_for_path(index_path);
        da->make_dir_recursive(index_path);
        memdelete(da);
        docsrc.load_classes(index_path.c_str());
        checked_paths.insert(index_path);
        print_line("Loading docs from: " + index_path);

        print_line("Merging docs...");
        doc.merge_from(docsrc);
        for (const String &E : checked_paths) {
            print_line("Erasing old docs at: " + E);
            DocData::erase_classes(E.c_str());
        }

        print_line("Generating new docs...");
        doc.save_classes(index_path.c_str(), VERSION_BRANCH,doc_data_classes);

        return false;
    }
#endif
    if (script.empty() && game_path.empty() && !T_GLOBAL_DEF("application/run/main_scene", String()).empty()) {
        game_path = T_GLOBAL_DEF("application/run/main_scene", String());
    }

    MainLoop *main_loop = nullptr;
    if (editor) {
        main_loop = memnew(SceneTree);
    }
    StringName main_loop_type = T_GLOBAL_DEF<StringName>("application/run/main_loop_type", "SceneTree");

    if (!script.empty()) {

        Ref<Script> script_res = dynamic_ref_cast<Script>(gResourceManager().load(script));
        ERR_FAIL_COND_V_MSG(not script_res, false, "Can't load script: " + script);

        if (check_only) {
            if (!script_res->is_valid()) {
                OS::get_singleton()->set_exit_code(EXIT_FAILURE);
            } else {
                OS::get_singleton()->set_exit_code(EXIT_SUCCESS);
            }
            return false;
        }

        if (!script_res->can_instance() ) {
            return false;
        }

            StringName instance_type = script_res->get_instance_base_type();
            Object *obj = ClassDB::instance(instance_type);
            MainLoop *script_loop = object_cast<MainLoop>(obj);
            if (!script_loop) {
                memdelete(obj);
                ERR_FAIL_V_MSG(false, "Can't load the script '" + script + "' as it doesn't inherit from SceneTree or MainLoop.");
            }

            script_loop->set_init_script(script_res);
            main_loop = script_loop;

    } else { // Not based on script path.
        if (!editor && !ClassDB::class_exists(main_loop_type) && ScriptServer::is_global_class(main_loop_type)) {
            StringView script_path = ScriptServer::get_global_class_path(main_loop_type);
            Ref<Script> script_res = gResourceManager().loadT<Script>(script_path, "Script", true);
            StringName script_base = ScriptServer::get_global_class_native_base(main_loop_type);
            Object *obj = ClassDB::instance(script_base);
            MainLoop *script_loop = object_cast<MainLoop>(obj);
            if (!script_loop) {
                if (obj) {
                    memdelete(obj);
        }

                OS::get_singleton()->alert("Error: Invalid MainLoop script base type: " + script_base);
                ERR_FAIL_V_MSG(false, FormatVE("The global class %s does not inherit from SceneTree or MainLoop.", main_loop_type.asCString()));
    }

            script_loop->set_init_script(script_res);
            main_loop = script_loop;
        }
    }
    if (!main_loop && main_loop_type.empty())
        main_loop_type = "SceneTree";

    if (!main_loop) {
        if (!ClassDB::class_exists(StringName(main_loop_type))) {
            OS::get_singleton()->alert(("Error: MainLoop type doesn't exist: " + main_loop_type));
            return false;
        }

        Object *ml = ClassDB::instance(StringName(main_loop_type));
        ERR_FAIL_COND_V_MSG(!ml, false, "Can't instance MainLoop type.");

        main_loop = object_cast<MainLoop>(ml);
        if (!main_loop) {

            memdelete(ml);
            ERR_FAIL_V_MSG(false, "Invalid MainLoop type.");
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

        gResourceManager().add_custom_loaders();
        gResourceManager().add_custom_savers();
        if (!project_manager && !editor) { // game
            if (!game_path.empty() || !script.empty()) {
                if (script_debugger && script_debugger->is_remote()) {
                    ScriptDebuggerRemote *remote_debugger = static_cast<ScriptDebuggerRemote *>(script_debugger);

                    remote_debugger->set_scene_tree(sml);
                }
                //autoload
                Vector<PropertyInfo> props;
                ProjectSettings::get_singleton()->get_property_list(&props);

                //first pass, add the constants so they exist before any script is loaded
                for (const PropertyInfo &E : props) {

                    StringName s = E.name;
                    if (!StringUtils::begins_with(s,"autoload/"))
                        continue;
                    StringName name(StringUtils::get_slice(s,'/', 1));
                    String path = ProjectSettings::get_singleton()->getT<String>(s);
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
                Vector<Node *> to_add;
                for (const PropertyInfo &E : props) {

                    StringName s(E.name);
                    if (!StringUtils::begins_with(s,"autoload/"))
                        continue;
                    StringName name(StringUtils::get_slice(s,'/', 1));
                    String path = ProjectSettings::get_singleton()->getT<String>(s);
                    bool global_var = false;
                    if (StringUtils::begins_with(path,"*")) {
                        global_var = true;
                        path = StringUtils::substr(path,1, path.length() - 1);
                    }

                    RES res(gResourceManager().load(path));
                    ERR_CONTINUE_MSG(not res, "Can't autoload: " + path);
                    Node *n = nullptr;
                    if (res->is_class("PackedScene")) {
                        Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(res);
                        n = ps->instance();
                    } else if (res->is_class("Script")) {
                        Ref<Script> script_res = dynamic_ref_cast<Script>(res);
                        StringName ibt = script_res->get_instance_base_type();
                        bool valid_type = ClassDB::is_parent_class(ibt, "Node");
                        ERR_CONTINUE_MSG(!valid_type, "Script does not inherit from Node: " + path);

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

            String stretch_mode = T_GLOBAL_DEF("display/window/stretch/mode", String("disabled"));
            String stretch_aspect = T_GLOBAL_DEF("display/window/stretch/aspect", String("ignore"));
            Size2i stretch_size = Size2(T_GLOBAL_DEF<int>("display/window/size/width", 0), T_GLOBAL_DEF<int>("display/window/size/height", 0));
            // out of compatibility reasons stretch_scale is called shrink when exposed to the user.
            real_t stretch_scale = T_GLOBAL_DEF("display/window/stretch/shrink", 1.0);

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

            sml->set_screen_stretch(sml_sm, sml_aspect, stretch_size, stretch_scale);

            sml->set_auto_accept_quit(T_GLOBAL_DEF("application/config/auto_accept_quit", true));
            sml->set_quit_on_go_back(T_GLOBAL_DEF("application/config/quit_on_go_back", true));
            StringName appname = ProjectSettings::get_singleton()->getT<StringName>("application/config/name");
            appname = TranslationServer::get_singleton()->translate(appname);
#ifdef DEBUG_ENABLED
            // Append a suffix to the window title to denote that the project is running
            // from a debug build (including the editor). Since this results in lower performance,
            // this should be clearly presented to the user.
            OS::get_singleton()->set_window_title(FormatVE("%s (DEBUG)", appname.asCString()));
#else
            OS::get_singleton()->set_window_title(appname);
#endif
            // Define a very small minimum window size to prevent bugs such as GH-37242.
            // It can still be overridden by the user in a script.
            OS::get_singleton()->set_min_window_size(Size2(64, 64));

            int shadow_atlas_size = T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/size");
            int shadow_atlas_q0_subdiv = T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/quadrant_0_subdiv");
            int shadow_atlas_q1_subdiv = T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/quadrant_1_subdiv");
            int shadow_atlas_q2_subdiv = T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/quadrant_2_subdiv");
            int shadow_atlas_q3_subdiv = T_GLOBAL_GET<int>("rendering/quality/shadow_atlas/quadrant_3_subdiv");

            sml->get_root()->set_shadow_atlas_size(shadow_atlas_size);
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(0, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q0_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(1, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q1_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(2, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q2_subdiv));
            sml->get_root()->set_shadow_atlas_quadrant_subdiv(3, Viewport::ShadowAtlasQuadrantSubdiv(shadow_atlas_q3_subdiv));
            Viewport::Usage usage = T_GLOBAL_GET<Viewport::Usage>("rendering/quality/intended_usage/framebuffer_allocation");
            sml->get_root()->set_usage(usage);

            bool snap_controls = T_GLOBAL_DEF("gui/common/snap_controls_to_pixels", true);
            sml->get_root()->set_snap_controls_to_pixels(snap_controls);

            bool font_oversampling = T_GLOBAL_DEF("rendering/quality/dynamic_fonts/use_oversampling", true);
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
                    PropertyInfo(VariantType::FLOAT, "display/window/stretch/shrink", PropertyHint::Range, "0.1,8,0.01,or_greater"));
            sml->set_auto_accept_quit(T_GLOBAL_DEF("application/config/auto_accept_quit", true));
            sml->set_quit_on_go_back(T_GLOBAL_DEF("application/config/quit_on_go_back", true));
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
                        auto sep = StringUtils::rfind(local_game_path,'/');

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

                if (game_path != T_GLOBAL_GET<String>("application/run/main_scene") || !editor_node->has_scenes_in_session()) {
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
                Ref<PackedScene> scenedata = dynamic_ref_cast<PackedScene>(gResourceManager().load(local_game_path));
                if (scenedata)
                    scene = scenedata->instance();

                ERR_FAIL_COND_V_MSG(!scene, false, "Failed loading scene: " + local_game_path);
                sml->add_current_scene(scene);
#ifdef OSX_ENABLED
                String mac_iconpath = T_GLOBAL_DEF("application/config/macos_native_icon", String());
                if (mac_iconpath != "") {
                    OS::get_singleton()->set_native_icon(mac_iconpath);
                    hasicon = true;
                }
#endif

#ifdef WINDOWS_ENABLED
                String win_iconpath = T_GLOBAL_DEF("application/config/windows_native_icon", String());
                if (not win_iconpath.empty()) {
                    OS::get_singleton()->set_native_icon(win_iconpath);
                    hasicon = true;
                }
#endif

                String iconpath = T_GLOBAL_DEF("application/config/icon", String());
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
            // Load SSL Certificates from Editor Settings (or builtin)
            Crypto::load_default_certificates(
                    EditorSettings::get_singleton()->get_setting("network/ssl/editor_ssl_certificates").as<String>());
        }
#endif
    }

    if (!hasicon && OS::get_singleton()->get_bundle_icon_path().empty()) {
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
uint32_t Main::frames = 0;
uint32_t Main::frame = 0;
bool Main::force_redraw_requested = false;
bool Main::agile_input_event_flushing = false;
int Main::iterating = 0;
bool Main::is_iterating() {
    return iterating > 0;
}

// For performance metrics.
static uint64_t physics_process_max = 0;
static uint64_t idle_process_max = 0;
#ifndef TOOLS_ENABLED
static uint64_t frame_delta_sync_time = 0;
#endif

bool Main::iteration() {
    SCOPE_AUTONAMED;
    //for now do not error on this
    //ERR_FAIL_COND_V(iterating, false);

    iterating++;
    // ticks may become modified later on, and we want to store the raw measured
    // value for profiling.
    uint64_t raw_ticks_at_start = OS::get_singleton()->get_ticks_usec();

#ifdef TOOLS_ENABLED
    uint64_t ticks = raw_ticks_at_start;
#else
    // we can either sync the delta from here, or later in the iteration
    uint64_t ticks_difference = raw_ticks_at_start - frame_delta_sync_time;

    // if we are syncing at start or if frame_delta_sync_time is being initialized
    // or a large gap has happened between the last delta_sync_time and now
    if (!delta_sync_after_draw || (ticks_difference > 100000)) {
        frame_delta_sync_time = raw_ticks_at_start;
    }
    uint64_t ticks = frame_delta_sync_time;
#endif
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

    static constexpr int max_physics_steps = 8;
    if (fixed_fps == -1 && advance.physics_steps > max_physics_steps) {
        step -= (advance.physics_steps - max_physics_steps) * frame_slice;
        advance.physics_steps = max_physics_steps;
    }
    PhysicsServer2D *physicsServer2D = PhysicsServer2D::get_singleton();
    PhysicsServer3D *physicsServer3D = PhysicsServer3D::get_singleton();


    bool exit = false;
    for (int iters = 0; iters < advance.physics_steps; ++iters) {
        if (InputDefault::get_singleton()->is_using_input_buffering() && agile_input_event_flushing) {
            InputDefault::get_singleton()->flush_buffered_events();
        }

        Engine::get_singleton()->_in_physics = true;

        uint64_t physics_begin = OS::get_singleton()->get_ticks_usec();

        physicsServer3D->flush_queries();

        physicsServer2D->sync();
        physicsServer2D->flush_queries();

        if (OS::get_singleton()->get_main_loop()->iteration(frame_slice * time_scale)) {
            exit = true;
            Engine::get_singleton()->_in_physics = false;
            break;
        }

        message_queue->flush();

        physicsServer3D->step(frame_slice * time_scale);
        NavigationServer::get_singleton_mut()->process(frame_slice * time_scale);

        physicsServer2D->end_sync();
        physicsServer2D->step(frame_slice * time_scale);

        message_queue->flush();

        physics_process_ticks = M_MAX(physics_process_ticks, OS::get_singleton()->get_ticks_usec() - physics_begin); // keep the largest one for reference
        physics_process_max = M_MAX(OS::get_singleton()->get_ticks_usec() - physics_begin, physics_process_max);
        Engine::get_singleton()->_physics_frames++;
        Engine::get_singleton()->_in_physics = false;
    }

    if (InputDefault::get_singleton()->is_using_input_buffering() && agile_input_event_flushing) {
        InputDefault::get_singleton()->flush_buffered_events();
    }

    Engine::get_singleton()->_in_physics = false;

    {
        SCOPE_PROFILE("canvas updates");
        bool done = false;
        while(!done) {
            done = update_all_pending_canvas_items();
        }
    }


    uint64_t idle_begin = OS::get_singleton()->get_ticks_usec();

    if (OS::get_singleton()->get_main_loop()->idle(step * time_scale)) {
        exit = true;
    }
    rendering_server_callbacks->flush();
    message_queue->flush();

    RenderingServer::sync_thread(); //sync if still drawing from previous frames.

    if (OS::get_singleton()->can_draw() && !disable_render_loop) {

        if ((!force_redraw_requested) && OS::get_singleton()->is_in_low_processor_usage_mode()) {
            if (RenderingServer::get_singleton()->has_changed()) {
                RenderingServer::get_singleton()->draw(true, scaled_step); // flush visual commands
                Engine::get_singleton()->frames_drawn++;
            }
        } else {
            RenderingServer::get_singleton()->draw(true, scaled_step); // flush visual commands
            Engine::get_singleton()->frames_drawn++;
            force_redraw_requested = false;
        }
    }

#ifndef TOOLS_ENABLED
    // we can choose to sync delta from here, just after the draw
    if (delta_sync_after_draw) {
        frame_delta_sync_time = OS::get_singleton()->get_ticks_usec();
    }
#endif
    // profiler timing information
    idle_process_ticks = OS::get_singleton()->get_ticks_usec() - idle_begin;
    idle_process_max = M_MAX(idle_process_ticks, idle_process_max);
    uint64_t frame_time = OS::get_singleton()->get_ticks_usec() - raw_ticks_at_start;

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

        const char *exe_type = (editor || project_manager) ? "Editor" : "Project";
        bool should_show_fps = false;
        if (editor || project_manager) {
            should_show_fps = print_fps;
        } else {
            should_show_fps = T_GLOBAL_GET<bool>("debug/settings/stdout/print_fps") || print_fps;
            }
        if(should_show_fps)
            print_line(FormatVE("%s FPS: %d (%s mspf)", exe_type, frames, StringUtils::pad_decimals(rtos(1000.0f / frames),1).c_str()));

        Engine::get_singleton()->_fps = frames;
        performance->set_process_time(USEC_TO_SEC(idle_process_max));
        performance->set_physics_process_time(USEC_TO_SEC(physics_process_max));
        idle_process_max = 0;
        physics_process_max = 0;

        frame %= 1000000;
        frames = 0;
    }

    iterating--;
    // Needed for OSs using input buffering regardless accumulation (like Android)
    if (InputDefault::get_singleton()->is_using_input_buffering() && !agile_input_event_flushing) {
        InputDefault::get_singleton()->flush_buffered_events();
    }

    if (fixed_fps != -1)
        return exit;

    OS::get_singleton()->add_frame_delay(OS::get_singleton()->can_draw());

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
void Main::cleanup(bool p_force) {

    if (!p_force) {
    ERR_FAIL_COND(!_start_success);
    }
    if (script_debugger) {
        // Flush any remaining messages
        script_debugger->idle_poll();
    }

    gResourceManager().remove_custom_loaders();
    gResourceManager().remove_custom_savers();

    message_queue->flush();
    memdelete(message_queue);
    memdelete(rendering_server_callbacks);

    if (script_debugger) {
        if (use_debug_profiler) {
            script_debugger->profiling_end();
        }

        memdelete(script_debugger);
    }

    OS::get_singleton()->delete_main_loop();

    OS::get_singleton()->_cmdline.clear();
    OS::get_singleton()->_execpath = "";
    OS::get_singleton()->_local_clipboard.clear();
    OS::get_singleton()->_primary_clipboard.clear();

    gResourceRemapper().clear_translation_remaps();
    gResourceRemapper().clear_path_remaps();

    ScriptServer::finish_languages();

    // Sync pending commands that may have been queued from a different thread during ScriptServer finalization
    RenderingServer::sync_thread();

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
    // cleanup plugin registry
    remove_all_resolvers();

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

    memdelete(packed_data);
    memdelete(file_access_network_client);
    memdelete(performance);
    memdelete(input_map);
    memdelete(time_singleton);
    memdelete(translation_server);
    memdelete(globals);
    memdelete(engine);

    if (OS::get_singleton()->is_restart_on_exit_set()) {
        //attempt to restart with arguments
        String exec = OS::get_singleton()->get_executable_path();
        Vector<String> args = OS::get_singleton()->get_restart_on_exit_arguments();
        OS::ProcessID pid = 0;
        OS::get_singleton()->execute(exec, args, false, &pid);
        OS::get_singleton()->set_restart_on_exit(false, Vector<String>()); //clear list (uses memory)
    }

    unregister_core_driver_types();
    unregister_core_types();

    OS::get_singleton()->finalize_core();
}
