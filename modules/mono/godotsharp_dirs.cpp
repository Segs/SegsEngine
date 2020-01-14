/*************************************************************************/
/*  godotsharp_dirs.cpp                                                  */
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

#include "godotsharp_dirs.h"

#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/se_string.h"
#include "core/string_utils.h"

#ifdef TOOLS_ENABLED
#include "core/version.h"
#include "editor/editor_settings.h"
#endif

#ifdef ANDROID_ENABLED
#include "mono_gd/gd_mono_android.h"
#endif

#include "mono_gd/gd_mono.h"

namespace GodotSharpDirs {

se_string _get_expected_build_config() {
#ifdef TOOLS_ENABLED
    return "Tools";
#else

#ifdef DEBUG_ENABLED
    return "Debug";
#else
    return "Release";
#endif

#endif
}

se_string _get_mono_user_dir() {
    using namespace PathUtils;
#ifdef TOOLS_ENABLED
    if (EditorSettings::get_singleton()) {
        return plus_file(EditorSettings::get_singleton()->get_data_dir(),"mono");
    } else {
        se_string settings_path;

        se_string exe_dir = get_base_dir(OS::get_singleton()->get_executable_path());
        DirAccessRef d = DirAccess::create_for_path(exe_dir);

        if (d->file_exists("._sc_") || d->file_exists("_sc_")) {
            // contain yourself
            settings_path = plus_file(exe_dir,"editor_data");
        } else {
            settings_path = plus_file(OS::get_singleton()->get_data_path(),OS::get_singleton()->get_godot_dir_name());
        }

        return plus_file(settings_path,"mono");
    }
#else
    return plus_file(OS::get_singleton()->get_user_data_dir(),"mono");
#endif
}

class _GodotSharpDirs {

public:
    se_string res_data_dir;
    se_string res_metadata_dir;
    se_string res_assemblies_base_dir;
    se_string res_assemblies_dir;
    se_string res_config_dir;
    se_string res_temp_dir;
    se_string res_temp_assemblies_base_dir;
    se_string res_temp_assemblies_dir;
    se_string mono_user_dir;
    se_string mono_logs_dir;

#ifdef TOOLS_ENABLED
    se_string mono_solutions_dir;
    se_string build_logs_dir;

    se_string sln_filepath;
    se_string csproj_filepath;

    se_string data_editor_tools_dir;
    se_string data_editor_prebuilt_api_dir;
#else
    // Equivalent of res_assemblies_dir, but in the data directory rather than in 'res://'.
    // Only defined on export templates. Used when exporting assemblies outside of PCKs.
    se_string data_game_assemblies_dir;
#endif

    se_string data_mono_etc_dir;
    se_string data_mono_lib_dir;

#ifdef WINDOWS_ENABLED
    se_string data_mono_bin_dir;
#endif

private:
    _GodotSharpDirs() {
        using namespace PathUtils;
        res_data_dir = "res://.mono";
        res_metadata_dir = plus_file(res_data_dir,"metadata");
        res_assemblies_base_dir = plus_file(res_data_dir,"assemblies");
        res_assemblies_dir = plus_file(res_assemblies_base_dir,GDMono::get_expected_api_build_config());
        res_config_dir = plus_file(plus_file(res_data_dir,"etc"),"mono");

        // TODO use paths from csproj
        res_temp_dir = res_data_dir.plus_file("temp");
        res_temp_assemblies_base_dir = res_temp_dir.plus_file("bin");
        res_temp_assemblies_dir = res_temp_assemblies_base_dir.plus_file(_get_expected_build_config());

#ifdef JAVASCRIPT_ENABLED
        mono_user_dir = "user://";
#else
        mono_user_dir = _get_mono_user_dir();
#endif
        mono_logs_dir = mono_user_dir.plus_file("mono_logs");

#ifdef TOOLS_ENABLED
        mono_solutions_dir = mono_user_dir.plus_file("solutions");
        build_logs_dir = mono_user_dir.plus_file("build_logs");

        se_string appname = ProjectSettings::get_singleton()->get("application/config/name").as<se_string>();
        se_string appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
        if (appname_safe.empty()) {
            appname_safe = "UnnamedProject";
        }

        se_string base_path = ProjectSettings::get_singleton()->globalize_path("res://");

        sln_filepath = base_path.plus_file(appname_safe + ".sln");
        csproj_filepath = base_path.plus_file(appname_safe + ".csproj");
#endif

        se_string exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();

#ifdef TOOLS_ENABLED

        se_string data_dir_root = exe_dir.plus_file("GodotSharp");
        data_editor_tools_dir = data_dir_root.plus_file("Tools");
        data_editor_prebuilt_api_dir = data_dir_root.plus_file("Api");

        se_string data_mono_root_dir = data_dir_root.plus_file("Mono");
        data_mono_etc_dir = data_mono_root_dir.plus_file("etc");

#ifdef ANDROID_ENABLED
        data_mono_lib_dir = GDMonoAndroid::get_app_native_lib_dir();
#else
        data_mono_lib_dir = data_mono_root_dir.plus_file("lib");
#endif

#ifdef WINDOWS_ENABLED
        data_mono_bin_dir = data_mono_root_dir.plus_file("bin");
#endif

#ifdef OSX_ENABLED
        if (!DirAccess::exists(data_editor_tools_dir)) {
            data_editor_tools_dir = exe_dir.plus_file("../Frameworks/GodotSharp/Tools");
        }

        if (!DirAccess::exists(data_editor_prebuilt_api_dir)) {
            data_editor_prebuilt_api_dir = exe_dir.plus_file("../Frameworks/GodotSharp/Api");
        }

        if (!DirAccess::exists(data_mono_root_dir)) {
            data_mono_etc_dir = exe_dir.plus_file("../Resources/GodotSharp/Mono/etc");
            data_mono_lib_dir = exe_dir.plus_file("../Frameworks/GodotSharp/Mono/lib");
        }
#endif

#else

        se_string appname = ProjectSettings::get_singleton()->get("application/config/name");
        se_string appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
        se_string data_dir_root = exe_dir.plus_file("data_" + appname_safe);
        if (!DirAccess::exists(data_dir_root)) {
            data_dir_root = exe_dir.plus_file("data_Godot");
        }

        se_string data_mono_root_dir = data_dir_root.plus_file("Mono");
        data_mono_etc_dir = data_mono_root_dir.plus_file("etc");

#ifdef ANDROID_ENABLED
        data_mono_lib_dir = GDMonoAndroid::get_app_native_lib_dir();
#else
        data_mono_lib_dir = data_mono_root_dir.plus_file("lib");
        data_game_assemblies_dir = data_dir_root.plus_file("Assemblies");
#endif

#ifdef WINDOWS_ENABLED
        data_mono_bin_dir = data_mono_root_dir.plus_file("bin");
#endif

#ifdef OSX_ENABLED
        if (!DirAccess::exists(data_mono_root_dir)) {
            data_mono_etc_dir = exe_dir.plus_file("../Resources/GodotSharp/Mono/etc");
            data_mono_lib_dir = exe_dir.plus_file("../Frameworks/GodotSharp/Mono/lib");
        }

        if (!DirAccess::exists(data_game_assemblies_dir)) {
            data_game_assemblies_dir = exe_dir.plus_file("../Frameworks/GodotSharp/Assemblies");
        }
#endif

#endif
    }

    _GodotSharpDirs(const _GodotSharpDirs &);
    _GodotSharpDirs &operator=(const _GodotSharpDirs &);

public:
    static _GodotSharpDirs &get_singleton() {
        static _GodotSharpDirs singleton;
        return singleton;
    }
};

se_string get_res_data_dir() {
    return _GodotSharpDirs::get_singleton().res_data_dir;
}

se_string get_res_metadata_dir() {
    return _GodotSharpDirs::get_singleton().res_metadata_dir;
}

se_string get_res_assemblies_base_dir() {
    return _GodotSharpDirs::get_singleton().res_assemblies_base_dir;
}

se_string get_res_assemblies_dir() {
    return _GodotSharpDirs::get_singleton().res_assemblies_dir;
}

se_string get_res_config_dir() {
    return _GodotSharpDirs::get_singleton().res_config_dir;
}

se_string get_res_temp_dir() {
    return _GodotSharpDirs::get_singleton().res_temp_dir;
}

se_string get_res_temp_assemblies_base_dir() {
    return _GodotSharpDirs::get_singleton().res_temp_assemblies_base_dir;
}

se_string get_res_temp_assemblies_dir() {
    return _GodotSharpDirs::get_singleton().res_temp_assemblies_dir;
}

se_string get_mono_user_dir() {
    return _GodotSharpDirs::get_singleton().mono_user_dir;
}

se_string get_mono_logs_dir() {
    return _GodotSharpDirs::get_singleton().mono_logs_dir;
}

#ifdef TOOLS_ENABLED
se_string get_mono_solutions_dir() {
    return _GodotSharpDirs::get_singleton().mono_solutions_dir;
}

se_string get_build_logs_dir() {
    return _GodotSharpDirs::get_singleton().build_logs_dir;
}

se_string get_project_sln_path() {
    return _GodotSharpDirs::get_singleton().sln_filepath;
}

se_string get_project_csproj_path() {
    return _GodotSharpDirs::get_singleton().csproj_filepath;
}

se_string get_data_editor_tools_dir() {
    return _GodotSharpDirs::get_singleton().data_editor_tools_dir;
}

se_string get_data_editor_prebuilt_api_dir() {
    return _GodotSharpDirs::get_singleton().data_editor_prebuilt_api_dir;
}
#else
se_string get_data_game_assemblies_dir() {
    return _GodotSharpDirs::get_singleton().data_game_assemblies_dir;
}
#endif

se_string get_data_mono_etc_dir() {
    return _GodotSharpDirs::get_singleton().data_mono_etc_dir;
}

se_string get_data_mono_lib_dir() {
    return _GodotSharpDirs::get_singleton().data_mono_lib_dir;
}

#ifdef WINDOWS_ENABLED
se_string get_data_mono_bin_dir() {
    return _GodotSharpDirs::get_singleton().data_mono_bin_dir;
}
#endif

} // namespace GodotSharpDirs
