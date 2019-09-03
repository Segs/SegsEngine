/*************************************************************************/
/*  godotsharp_dirs.cpp                                                  */
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

#include "godotsharp_dirs.h"

#include "core/os/dir_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"

#ifdef TOOLS_ENABLED
#include "core/version.h"
#include "editor/editor_settings.h"
#endif

#ifdef __ANDROID__
#include "utils/android_utils.h"
#endif

namespace GodotSharpDirs {

String _get_expected_build_config() {
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

String _get_expected_api_build_config() {
#ifdef TOOLS_ENABLED
	return "Debug";
#else

#ifdef DEBUG_ENABLED
	return "Debug";
#else
	return "Release";
#endif

#endif
}

String _get_mono_user_dir() {
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
                return PathUtils::plus_file(EditorSettings::get_singleton()->get_data_dir(),"mono");
	} else {
		String settings_path;

                String exe_dir = PathUtils::get_base_dir(OS::get_singleton()->get_executable_path());
		DirAccessRef d = DirAccess::create_for_path(exe_dir);

		if (d->file_exists("._sc_") || d->file_exists("_sc_")) {
			// contain yourself
                        settings_path = PathUtils::plus_file(exe_dir,"editor_data");
		} else {
                        settings_path = PathUtils::plus_file(OS::get_singleton()->get_data_path(),OS::get_singleton()->get_godot_dir_name());
		}

                return PathUtils::plus_file(settings_path,"mono");
	}
#else
        return PathUtils::plus_file(OS::get_singleton()->get_user_data_dir(),"mono");
#endif
}

class _GodotSharpDirs {

public:
	String res_data_dir;
	String res_metadata_dir;
	String res_assemblies_base_dir;
	String res_assemblies_dir;
	String res_config_dir;
	String res_temp_dir;
	String res_temp_assemblies_base_dir;
	String res_temp_assemblies_dir;
	String mono_user_dir;
	String mono_logs_dir;

#ifdef TOOLS_ENABLED
	String mono_solutions_dir;
	String build_logs_dir;

	String sln_filepath;
	String csproj_filepath;

	String data_editor_tools_dir;
	String data_editor_prebuilt_api_dir;
#endif

	String data_mono_etc_dir;
	String data_mono_lib_dir;

#ifdef WINDOWS_ENABLED
	String data_mono_bin_dir;
#endif

private:
	_GodotSharpDirs() {
		res_data_dir = "res://.mono";
                res_metadata_dir = PathUtils::plus_file(res_data_dir,"metadata");
                res_assemblies_base_dir = PathUtils::plus_file(res_data_dir,"assemblies");
                res_assemblies_dir = PathUtils::plus_file(res_assemblies_base_dir,_get_expected_api_build_config());
                res_config_dir = PathUtils::plus_file(PathUtils::plus_file(res_data_dir,"etc"),"mono");

		// TODO use paths from csproj
                res_temp_dir = PathUtils::plus_file(res_data_dir,"temp");
                res_temp_assemblies_base_dir = PathUtils::plus_file(res_temp_dir,"bin");
                res_temp_assemblies_dir = PathUtils::plus_file(res_temp_assemblies_base_dir,_get_expected_build_config());

		mono_user_dir = _get_mono_user_dir();
                mono_logs_dir = PathUtils::plus_file(mono_user_dir,"mono_logs");

#ifdef TOOLS_ENABLED
                mono_solutions_dir = PathUtils::plus_file(mono_user_dir,"solutions");
                build_logs_dir = PathUtils::plus_file(mono_user_dir,"build_logs");

		String appname = ProjectSettings::get_singleton()->get("application/config/name");
		String appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
		if (appname_safe.empty()) {
			appname_safe = "UnnamedProject";
		}

		String base_path = ProjectSettings::get_singleton()->globalize_path("res://");

                sln_filepath = PathUtils::plus_file(base_path,appname_safe + ".sln");
                csproj_filepath = PathUtils::plus_file(base_path,appname_safe + ".csproj");
#endif

                String exe_dir = PathUtils::get_base_dir(OS::get_singleton()->get_executable_path());

#ifdef TOOLS_ENABLED

                String data_dir_root = PathUtils::plus_file(exe_dir,"GodotSharp");
                data_editor_tools_dir = PathUtils::plus_file(data_dir_root,"Tools");
                data_editor_prebuilt_api_dir = PathUtils::plus_file(data_dir_root,"Api");

                String data_mono_root_dir = PathUtils::plus_file(data_dir_root,"Mono");
                data_mono_etc_dir = PathUtils::plus_file(data_mono_root_dir,"etc");

#if __ANDROID__
		data_mono_lib_dir = GDMonoUtils::Android::get_app_native_lib_dir();
#else
                data_mono_lib_dir = PathUtils::plus_file(data_mono_root_dir,"lib");
#endif

#ifdef WINDOWS_ENABLED
                data_mono_bin_dir = PathUtils::plus_file(data_mono_root_dir,"bin");
#endif

#ifdef OSX_ENABLED
		if (!DirAccess::exists(data_editor_tools_dir)) {
                        data_editor_tools_dir = PathUtils::plus_file(exe_dir,"../Frameworks/GodotSharp/Tools");
		}

		if (!DirAccess::exists(data_editor_prebuilt_api_dir)) {
                        data_editor_prebuilt_api_dir = PathUtils::plus_file(exe_dir,"../Frameworks/GodotSharp/Api");
		}

		if (!DirAccess::exists(data_mono_root_dir)) {
                        data_mono_etc_dir = PathUtils::plus_file(exe_dir,"../Resources/GodotSharp/Mono/etc");
                        data_mono_lib_dir = PathUtils::plus_file(exe_dir,"../Frameworks/GodotSharp/Mono/lib");
		}
#endif

#else

		String appname = ProjectSettings::get_singleton()->get("application/config/name");
		String appname_safe = OS::get_singleton()->get_safe_dir_name(appname);
                String data_dir_root = PathUtils::plus_file(exe_dir,"data_" + appname_safe);
		if (!DirAccess::exists(data_dir_root)) {
                        data_dir_root = PathUtils::plus_file(exe_dir,"data_Godot");
		}

		String data_mono_root_dir = data_dir_root,"Mono");
                data_mono_etc_dir = PathUtils::plus_file(data_mono_root_dir,"etc");

#if __ANDROID__
		data_mono_lib_dir = GDMonoUtils::Android::get_app_native_lib_dir();
#else
                data_mono_lib_dir = PathUtils::plus_file(data_mono_root_dir,"lib");
#endif

#ifdef WINDOWS_ENABLED
                data_mono_bin_dir = PathUtils::plus_file(data_mono_root_dir,"bin");
#endif

#ifdef OSX_ENABLED
		if (!DirAccess::exists(data_mono_root_dir)) {
                        data_mono_etc_dir = PathUtils::plus_file(exe_dir,"../Resources/GodotSharp/Mono/etc");
                        data_mono_lib_dir = PathUtils::plus_file(exe_dir,"../Frameworks/GodotSharp/Mono/lib");
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

String get_res_data_dir() {
	return _GodotSharpDirs::get_singleton().res_data_dir;
}

String get_res_metadata_dir() {
	return _GodotSharpDirs::get_singleton().res_metadata_dir;
}

String get_res_assemblies_base_dir() {
	return _GodotSharpDirs::get_singleton().res_assemblies_base_dir;
}

String get_res_assemblies_dir() {
	return _GodotSharpDirs::get_singleton().res_assemblies_dir;
}

String get_res_config_dir() {
	return _GodotSharpDirs::get_singleton().res_config_dir;
}

String get_res_temp_dir() {
	return _GodotSharpDirs::get_singleton().res_temp_dir;
}

String get_res_temp_assemblies_base_dir() {
	return _GodotSharpDirs::get_singleton().res_temp_assemblies_base_dir;
}

String get_res_temp_assemblies_dir() {
	return _GodotSharpDirs::get_singleton().res_temp_assemblies_dir;
}

String get_mono_user_dir() {
	return _GodotSharpDirs::get_singleton().mono_user_dir;
}

String get_mono_logs_dir() {
	return _GodotSharpDirs::get_singleton().mono_logs_dir;
}

#ifdef TOOLS_ENABLED
String get_mono_solutions_dir() {
	return _GodotSharpDirs::get_singleton().mono_solutions_dir;
}

String get_build_logs_dir() {
	return _GodotSharpDirs::get_singleton().build_logs_dir;
}

String get_project_sln_path() {
	return _GodotSharpDirs::get_singleton().sln_filepath;
}

String get_project_csproj_path() {
	return _GodotSharpDirs::get_singleton().csproj_filepath;
}

String get_data_editor_tools_dir() {
	return _GodotSharpDirs::get_singleton().data_editor_tools_dir;
}

String get_data_editor_prebuilt_api_dir() {
	return _GodotSharpDirs::get_singleton().data_editor_prebuilt_api_dir;
}
#endif

String get_data_mono_etc_dir() {
	return _GodotSharpDirs::get_singleton().data_mono_etc_dir;
}

String get_data_mono_lib_dir() {
	return _GodotSharpDirs::get_singleton().data_mono_lib_dir;
}

#ifdef WINDOWS_ENABLED
String get_data_mono_bin_dir() {
	return _GodotSharpDirs::get_singleton().data_mono_bin_dir;
}
#endif

} // namespace GodotSharpDirs
