/*************************************************************************/
/*  export.cpp                                                           */
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

#include "export.h"

#include "core/class_db.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/version.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "platform/osx/logo.gen.h"
#include <cstring>
#include <sys/stat.h>

class EditorExportPlatformOSX final : public EditorExportPlatform {

    GDCLASS(EditorExportPlatformOSX,EditorExportPlatform)


    int version_code;

    Ref<ImageTexture> logo;

    void _fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, se_string_view p_binary);
    void _make_icon(const Ref<Image> &p_icon, Vector<uint8_t> &p_data);

    Error _code_sign(const Ref<EditorExportPreset> &p_preset, se_string_view p_path);
    Error _create_dmg(se_string_view p_dmg_path, se_string_view p_pkg_name, se_string_view p_app_path_name);

#ifdef OSX_ENABLED
    bool use_codesign() const { return true; }
    bool use_dmg() const { return true; }
#else
    bool use_codesign() const { return false; }
    bool use_dmg() const { return false; }
#endif

protected:
    void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<se_string> *r_features) override;
    void get_export_options(List<ExportOption> *r_options) override;

public:
    const se_string & get_name() const override { static const se_string v("Mac OSX"); return v;}
    const se_string & get_os_name() const override { static const se_string v("OSX"); return v; }
    Ref<Texture> get_logo() const override { return logo; }

    List<se_string> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const  override {
        List<se_string> list;
        if (use_dmg()) {
            list.push_back(("dmg"));
        }
        list.push_back(("zip"));
        return list;
    }
    Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags = 0) override;

    bool can_export(const Ref<EditorExportPreset> &p_preset, se_string &r_error, bool &r_missing_templates) const override;

    void get_platform_features(List<se_string> *r_features) override {

        r_features->push_back(("pc"));
        r_features->push_back(("s3tc"));
        r_features->push_back(("OSX"));
    }

    void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, Set<se_string> &p_features) override {
    }

    EditorExportPlatformOSX();
    ~EditorExportPlatformOSX() override;
};

IMPL_GDCLASS(EditorExportPlatformOSX)

void EditorExportPlatformOSX::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<se_string> *r_features) {
    if (p_preset->get("texture_format/s3tc")) {
        r_features->push_back(("s3tc"));
    }
    if (p_preset->get("texture_format/etc")) {
        r_features->push_back(("etc"));
    }
    if (p_preset->get("texture_format/etc2")) {
        r_features->push_back(("etc2"));
    }

    r_features->push_back(("64"));
}

void EditorExportPlatformOSX::get_export_options(List<ExportOption> *r_options) {

    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));

    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/info"), "Made with Godot Engine"));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/icon", PROPERTY_HINT_FILE, "*.png,*.icns"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/identifier", PROPERTY_HINT_PLACEHOLDER_TEXT, "com.example.game"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/signature"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/short_version"), "1.0"));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/version"), "1.0"));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/copyright"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "display/high_res"), false));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "privacy/camera_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the camera"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "privacy/microphone_usage_description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Provide a message if you need to use the microphone"), ""));


#ifdef OSX_ENABLED
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "codesign/enable"), false));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/identity", PROPERTY_HINT_PLACEHOLDER_TEXT, "Type: Name (ID)"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "codesign/timestamp"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "codesign/hardened_runtime"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/entitlements", PROPERTY_HINT_GLOBAL_FILE, "*.plist"), ""));
#endif

    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/s3tc"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/etc"), false));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "texture_format/etc2"), false));
}

void _rgba8_to_packbits_encode(int p_ch, int p_size, PoolVector<uint8_t> &p_source, Vector<uint8_t> &p_dest) {

    int src_len = p_size * p_size;

    Vector<uint8_t> result;
    result.resize(src_len * 1.25f); //temp vector for rle encoded data, make it 25% larger for worst case scenario
    int res_size = 0;

    uint8_t buf[128];
    int buf_size = 0;

    int i = 0;
    while (i < src_len) {
        uint8_t cur = p_source.read()[i * 4 + p_ch];

        if (i < src_len - 2) {

            if ((p_source.read()[(i + 1) * 4 + p_ch] == cur) && (p_source.read()[(i + 2) * 4 + p_ch] == cur)) {
                if (buf_size > 0) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size = 0;
                }

                uint8_t lim = i + 130 >= src_len ? src_len - i - 1 : 130;
                bool hit_lim = true;

                for (int j = 3; j <= lim; j++) {
                    if (p_source.read()[(i + j) * 4 + p_ch] != cur) {
                        hit_lim = false;
                        i = i + j - 1;
                        result.write[res_size++] = (uint8_t)(j - 3 + 0x80);
                        result.write[res_size++] = cur;
                        break;
                    }
                }
                if (hit_lim) {
                    result.write[res_size++] = (uint8_t)(lim - 3 + 0x80);
                    result.write[res_size++] = cur;
                    i = i + lim;
                }
            } else {
                buf[buf_size++] = cur;
                if (buf_size == 128) {
                    result.write[res_size++] = (uint8_t)(buf_size - 1);
                    memcpy(&result.write[res_size], &buf, buf_size);
                    res_size += buf_size;
                    buf_size = 0;
                }
            }
        } else {
            buf[buf_size++] = cur;
            result.write[res_size++] = (uint8_t)(buf_size - 1);
            memcpy(&result.write[res_size], &buf, buf_size);
            res_size += buf_size;
            buf_size = 0;
        }

        i++;
    }

    int ofs = p_dest.size();
    p_dest.resize(p_dest.size() + res_size);
    memcpy(&p_dest.write[ofs], result.ptr(), res_size);
}

void EditorExportPlatformOSX::_make_icon(const Ref<Image> &p_icon, Vector<uint8_t> &p_data) {

    Ref<ImageTexture> it(make_ref_counted<ImageTexture>());

    Vector<uint8_t> data;

    data.resize(8);
    data.write[0] = 'i';
    data.write[1] = 'c';
    data.write[2] = 'n';
    data.write[3] = 's';

    struct MacOSIconInfo {
        const char *name;
        const char *mask_name;
        bool is_png;
        int size;
    };

    static const MacOSIconInfo icon_infos[] = {
        { "ic10", "", true, 1024 }, //1024x1024 32-bit PNG and 512x512@2x 32-bit "retina" PNG
        { "ic09", "", true, 512 }, //512×512 32-bit PNG
        { "ic14", "", true, 512 }, //256x256@2x 32-bit "retina" PNG
        { "ic08", "", true, 256 }, //256×256 32-bit PNG
        { "ic13", "", true, 256 }, //128x128@2x 32-bit "retina" PNG
        { "ic07", "", true, 128 }, //128x128 32-bit PNG
        { "ic12", "", true, 64 }, //32x32@2x 32-bit "retina" PNG
        { "ic11", "", true, 32 }, //16x16@2x 32-bit "retina" PNG
        { "il32", "l8mk", false, 32 }, //32x32 24-bit RLE + 8-bit uncompressed mask
        { "is32", "s8mk", false, 16 } //16x16 24-bit RLE + 8-bit uncompressed mask
    };

    for (const MacOSIconInfo & icon_info : icon_infos) {
        const Ref<Image> &copy(p_icon);
        copy->convert(Image::FORMAT_RGBA8);
        copy->resize(icon_info.size, icon_info.size);

        if (icon_info.is_png) {
            // Encode PNG icon.
            it->create_from_image(copy);
            se_string path(PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"icon.png"));
            ResourceSaver::save(path, it);

            FileAccess *f = FileAccess::open(path, FileAccess::READ);
            if (!f) {
                // Clean up generated file.
                DirAccess::remove_file_or_error(path);
                ERR_FAIL();
            }

            int ofs = data.size();
            uint32_t len = f->get_len();
            data.resize(data.size() + len + 8);
            f->get_buffer(&data.write[ofs + 8], len);
            memdelete(f);
            len += 8;
            len = BSWAP32(len);
            memcpy(&data.write[ofs], icon_info.name, 4);
            encode_uint32(len, &data.write[ofs + 4]);

            // Clean up generated file.
            DirAccess::remove_file_or_error(path);

        } else {
            PoolVector<uint8_t> src_data = copy->get_data();

            //encode 24bit RGB RLE icon
            {
                int ofs = data.size();
                data.resize(data.size() + 8);

                _rgba8_to_packbits_encode(0, icon_info.size, src_data, data); // encode R
                _rgba8_to_packbits_encode(1, icon_info.size, src_data, data); // encode G
                _rgba8_to_packbits_encode(2, icon_info.size, src_data, data); // encode B

                int len = data.size() - ofs;
                len = BSWAP32(len);
                memcpy(&data.write[ofs], icon_info.name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }

            //encode 8bit mask uncompressed icon
            {
                int ofs = data.size();
                int len = copy->get_width() * copy->get_height();
                data.resize(data.size() + len + 8);

                for (int j = 0; j < len; j++) {
                    data.write[ofs + 8 + j] = src_data.read()[j * 4 + 3];
                }
                len += 8;
                len = BSWAP32(len);
                memcpy(&data.write[ofs], icon_info.mask_name, 4);
                encode_uint32(len, &data.write[ofs + 4]);
            }
        }
    }

    uint32_t total_len = data.size();
    total_len = BSWAP32(total_len);
    encode_uint32(total_len, &data.write[4]);

    p_data = data;
}

void EditorExportPlatformOSX::_fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, se_string_view p_binary) {

    se_string strnew;
    se_string str((const char *)plist.ptr(), plist.size());
    PODVector<se_string_view> lines = StringUtils::split(str,'\n');
    const std::pair<const char*,se_string> replacements[] = {
        {"$binary", se_string(p_binary)},
        {"$name", se_string(p_binary)},
        {"$info", p_preset->get("application/info")},
        {"$identifier", p_preset->get("application/identifier")},
        {"$short_version", p_preset->get("application/short_version")},
        {"$version", p_preset->get("application/version")},
        {"$signature", p_preset->get("application/signature")},
        {"$copyright", p_preset->get("application/copyright")},
        {"$highres", se_string(p_preset->get("display/high_res") ? "<true/>" : "<false/>")},
        {"$camera_usage_description",p_preset->get("privacy/camera_usage_description")},
        {"$microphone_usage_description",p_preset->get("privacy/microphone_usage_description")}
    };
    for (int i = 0; i < lines.size(); i++) {
        se_string line(lines[i]);
        for(const std::pair<const char *,se_string > &en : replacements)
        {
           line = StringUtils::replace(line,(en.first), en.second);
        }
        strnew += line + "\n";
    }

    plist.resize(strnew.size() - 1);
    for (size_t i = 0; i < strnew.size() - 1; i++) {
        plist.write[i] = strnew[i];
    }
}

/**
    If we're running the OSX version of the Godot editor we'll:
    - export our application bundle to a temporary folder
    - attempt to code sign it
    - and then wrap it up in a DMG
**/

Error EditorExportPlatformOSX::_code_sign(const Ref<EditorExportPreset> &p_preset, se_string_view p_path) {
    ListPOD<se_string> args;

    if (p_preset->get("codesign/timestamp")) {
        args.emplace_back(("--timestamp"));
    }
    if (p_preset->get("codesign/hardened_runtime")) {
        args.emplace_back(("--options"));
        args.emplace_back(("runtime"));
    }

    if (p_preset->get("codesign/entitlements") != "") {
        /* this should point to our entitlements.plist file that sandboxes our application, I don't know if this should also be placed in our app bundle */
        args.emplace_back(("--entitlements"));
        args.emplace_back(p_preset->get("codesign/entitlements"));
    }
    PoolVector<se_string> user_args = p_preset->get("codesign/custom_options").as<PoolVector<se_string>>();
    for (int i = 0; i < user_args.size(); i++) {
        se_string_view user_arg = StringUtils::strip_edges(user_args[i]);
        if (!user_arg.empty()) {
            args.emplace_back(user_arg);
        }
    }
    args.emplace_back(("-s"));
    args.emplace_back(p_preset->get("codesign/identity"));
    args.emplace_back(("-v")); /* provide some more feedback */
    args.emplace_back(p_path);

    se_string str;
    Error err = OS::get_singleton()->execute(("codesign"), args, true, nullptr, &str, nullptr, true);
    ERR_FAIL_COND_V(err != OK, err)

    print_line(se_string("codesign (") + p_path + "): " + str);
    if (StringUtils::contains(str,"no identity found")) {
        EditorNode::add_io_error(("codesign: no identity found"));
        return FAILED;
    }
    if ((StringUtils::contains(str,"unrecognized blob type")) || (StringUtils::contains(str,"cannot read entitlement data"))) {
        EditorNode::add_io_error(("codesign: invalid entitlements file"));
        return FAILED;
    }

    return OK;
}

Error EditorExportPlatformOSX::_create_dmg(se_string_view p_dmg_path, se_string_view p_pkg_name, se_string_view p_app_path_name) {
    ListPOD<se_string> args;

    if (FileAccess::exists(p_dmg_path)) {
        OS::get_singleton()->move_to_trash(p_dmg_path);
    }

    args.emplace_back(("create"));
    args.emplace_back(p_dmg_path);
    args.emplace_back(("-volname"));
    args.emplace_back(p_pkg_name);
    args.emplace_back(("-fs"));
    args.emplace_back(("HFS+"));
    args.emplace_back(("-srcfolder"));
    args.emplace_back(p_app_path_name);

    se_string str;
    Error err = OS::get_singleton()->execute(("hdiutil"), args, true, nullptr, &str, nullptr, true);
    ERR_FAIL_COND_V(err != OK, err)

    print_line("hdiutil returned: " + str);
    if (StringUtils::contains(str,"create failed")) {
        if (StringUtils::contains(str,"File exists")) {
            EditorNode::add_io_error(("hdiutil: create failed - file exists"));
        } else {
            EditorNode::add_io_error(("hdiutil: create failed"));
        }
        return FAILED;
    }

    return OK;
}

Error EditorExportPlatformOSX::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, se_string_view p_path, int p_flags) {
    ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

    se_string src_pkg_name;

    EditorProgress ep(("export"), ("Exporting for OSX"), 3, true);

    if (p_debug)
        src_pkg_name = p_preset->get("custom_template/debug").as<se_string>();
    else
        src_pkg_name = p_preset->get("custom_template/release").as<se_string>();

    if (src_pkg_name.empty()) {
        se_string err;
        src_pkg_name = find_export_template(("osx.zip"), &err);
        if (src_pkg_name.empty()) {
            EditorNode::add_io_error(StringName(err));
            return ERR_FILE_NOT_FOUND;
        }
    }

    if (!DirAccess::exists(PathUtils::get_base_dir(p_path))) {
        return ERR_FILE_BAD_PATH;
    }

    FileAccess *src_f = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    if (ep.step(("Creating app"), 0)) {
        return ERR_SKIP;
    }

    unzFile src_pkg_zip = unzOpen2(src_pkg_name.c_str(), &io);
    if (!src_pkg_zip) {

        EditorNode::add_io_error_utf8("Could not find template app to export:\n" + src_pkg_name);
        return ERR_FILE_NOT_FOUND;
    }

    int ret = unzGoToFirstFile(src_pkg_zip);

    se_string binary_to_use = "godot_osx_" + se_string(p_debug ? "debug" : "release") + ".64";

    se_string pkg_name;
    if (p_preset->get("application/name") != "")
        pkg_name = p_preset->get("application/name").as<se_string>(); // app_name
    else if (!se_string(ProjectSettings::get_singleton()->get("application/config/name")).empty())
        pkg_name = se_string(ProjectSettings::get_singleton()->get("application/config/name"));
    else
        pkg_name = "Unnamed";

    auto pkg_name_safe = OS::get_singleton()->get_safe_dir_name(pkg_name);

    Error err = OK;
    se_string tmp_app_path_name;
    zlib_filefunc_def io2 = io;
    FileAccess *dst_f = nullptr;
    io2.opaque = &dst_f;
    zipFile dst_pkg_zip = nullptr;
    DirAccess *tmp_app_path = nullptr;

    String export_format(use_dmg() && StringUtils::ends_with(p_path,"dmg") ? "dmg" : "zip");
    if (export_format == "dmg") {
        // We're on OSX so we can export to DMG, but first we create our application bundle
        tmp_app_path_name = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),pkg_name + ".app");
        print_line("Exporting to " + tmp_app_path_name);
        tmp_app_path = DirAccess::create_for_path(tmp_app_path_name);
        if (!tmp_app_path) {
            err = ERR_CANT_CREATE;
        }

        // Create our folder structure or rely on unzip?
        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/MacOS");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/MacOS");
        }

        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/Frameworks");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/Frameworks");
        }

        if (err == OK) {
            print_line("Creating " + tmp_app_path_name + "/Contents/Resources");
            err = tmp_app_path->make_dir_recursive(tmp_app_path_name + "/Contents/Resources");
        }
    } else {
        // Open our destination zip file
        dst_pkg_zip = zipOpen2(se_string(p_path).c_str(), APPEND_STATUS_CREATE, nullptr, &io2);
        if (!dst_pkg_zip) {
            err = ERR_CANT_CREATE;
        }
    }

    // Now process our template
    bool found_binary = false;
    int total_size = 0;

    while (ret == UNZ_OK && err == OK) {
        bool is_execute = false;

        //get filename
        unz_file_info info;
        char fname[16384];
        ret = unzGetCurrentFileInfo(src_pkg_zip, &info, fname, 16384, nullptr, 0, nullptr, 0);

        se_string file(fname);

        Vector<uint8_t> data;
        data.resize(info.uncompressed_size);

        //read
        unzOpenCurrentFile(src_pkg_zip);
        unzReadCurrentFile(src_pkg_zip, data.ptrw(), data.size());
        unzCloseCurrentFile(src_pkg_zip);

        //write

        file =StringUtils::replace_first(file,("osx_template.app/"), se_string());

        if (file == "Contents/Info.plist") {
            _fix_plist(p_preset, data, pkg_name);
        }

        if (StringUtils::begins_with(file,"Contents/MacOS/godot_")) {
            if (file != "Contents/MacOS/" + binary_to_use) {
                ret = unzGoToNextFile(src_pkg_zip);
                continue; //ignore!
            }
            found_binary = true;
            is_execute = true;
            file = "Contents/MacOS/" + pkg_name;
        }

        if (file == "Contents/Resources/icon.icns") {
            //see if there is an icon
            se_string iconpath;
            if (p_preset->get("application/icon") != "")
                iconpath = se_string(p_preset->get("application/icon"));
            else
                iconpath = se_string(ProjectSettings::get_singleton()->get("application/config/icon"));

            if (!iconpath.empty()) {
                if (PathUtils::get_extension(iconpath) == se_string_view("icns")) {
                    FileAccess *icon = FileAccess::open(iconpath, FileAccess::READ);
                    if (icon) {
                        data.resize(icon->get_len());
                        icon->get_buffer(&data.write[0], icon->get_len());
                        icon->close();
                        memdelete(icon);
                    }
                } else {
                    Ref<Image> icon(make_ref_counted<Image>());
                    icon->load(iconpath);
                    if (!icon->empty()) {
                        _make_icon(icon, data);
                    }
                }
            }
        }

        if (!data.empty()) {
            if (file.contains("/data.mono.osx.64.release_debug/")) {
                if (!p_debug) {
                    ret = unzGoToNextFile(src_pkg_zip);
                    continue; //skip
                }
                file = file.replaced("/data.mono.osx.64.release_debug/", "/data_" + pkg_name_safe + "/");
            }
            if (file.contains("/data.mono.osx.64.release/")) {
                if (p_debug) {
                    ret = unzGoToNextFile(src_pkg_zip);
                    continue; //skip
                }
                file = file.replaced("/data.mono.osx.64.release/", "/data_" + pkg_name_safe + "/");
            }

            print_line("ADDING: " + file + " size: " + itos(data.size()));
            total_size += data.size();

            if (export_format == "dmg") {
                // write it into our application bundle
                file = PathUtils::plus_file(tmp_app_path_name,file);
                if (err == OK) {
                    err = tmp_app_path->make_dir_recursive(PathUtils::get_base_dir(file));
                }
                if (err == OK) {
                    // write the file, need to add chmod
                    FileAccess *f = FileAccess::open(file, FileAccess::WRITE);
                    if (f) {
                        f->store_buffer(data.ptr(), data.size());
                        f->close();
                        if (is_execute) {
                            // Chmod with 0755 if the file is executable
                            FileAccess::set_unix_permissions(file, 0755);
                        }
                        memdelete(f);
                    } else {
                        err = ERR_CANT_CREATE;
                    }
                }
            } else {
                // add it to our zip file
                file = pkg_name + ".app/" + file;

                zip_fileinfo fi;
                fi.tmz_date.tm_hour = info.tmu_date.tm_hour;
                fi.tmz_date.tm_min = info.tmu_date.tm_min;
                fi.tmz_date.tm_sec = info.tmu_date.tm_sec;
                fi.tmz_date.tm_mon = info.tmu_date.tm_mon;
                fi.tmz_date.tm_mday = info.tmu_date.tm_mday;
                fi.tmz_date.tm_year = info.tmu_date.tm_year;
                fi.dosDate = info.dosDate;
                fi.internal_fa = info.internal_fa;
                fi.external_fa = info.external_fa;

                zipOpenNewFileInZip(dst_pkg_zip,
                        file.c_str(),
                        &fi,
                        nullptr,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        Z_DEFLATED,
                        Z_DEFAULT_COMPRESSION);

                zipWriteInFileInZip(dst_pkg_zip, data.ptr(), data.size());
                zipCloseFileInZip(dst_pkg_zip);
            }
        }

        ret = unzGoToNextFile(src_pkg_zip);
    }

    // we're done with our source zip
    unzClose(src_pkg_zip);

    if (!found_binary) {
        ERR_PRINT("Requested template binary '" + binary_to_use + "' not found. It might be missing from your template archive.");
        err = ERR_FILE_NOT_FOUND;
    }

    if (err == OK) {
        if (ep.step(("Making PKG"), 1)) {
            return ERR_SKIP;
        }

        if (export_format == "dmg") {
            se_string pack_path = tmp_app_path_name + "/Contents/Resources/" + pkg_name + ".pck";
            Vector<SharedObject> shared_objects;
            err = save_pack(p_preset, pack_path, &shared_objects);

            // see if we can code sign our new package
            bool sign_enabled = p_preset->get("codesign/enable");

            if (err == OK) {
                DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
                for (int i = 0; i < shared_objects.size(); i++) {
                    err = da->copy(shared_objects[i].path, tmp_app_path_name + "/Contents/Frameworks/" + PathUtils::get_file(shared_objects[i].path));
                    if (err == OK && sign_enabled) {
                        err = _code_sign(p_preset, tmp_app_path_name + "/Contents/Frameworks/" + PathUtils::get_file(shared_objects[i].path));
                    }
                }
                memdelete(da);
            }

            if (err == OK && sign_enabled) {
                if (ep.step(("Code signing bundle"), 2)) {
                    return ERR_SKIP;
                }

                // the order in which we code sign is important, this is a bit of a shame or we could do this in our loop that extracts the files from our ZIP

                // start with our application
                err = _code_sign(p_preset, tmp_app_path_name + "/Contents/MacOS/" + pkg_name);

                ///@TODO we should check the contents of /Contents/Frameworks for frameworks to sign
            }

            // and finally create a DMG
            if (err == OK) {
                if (ep.step(("Making DMG"), 3)) {
                    return ERR_SKIP;
                }
                err = _create_dmg(p_path, pkg_name, tmp_app_path_name);
            }

            // Clean up temporary .app dir
            OS::get_singleton()->move_to_trash(tmp_app_path_name);

        } else { // pck

            se_string pack_path = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),pkg_name + ".pck");

            Vector<SharedObject> shared_objects;
            err = save_pack(p_preset, pack_path, &shared_objects);

            if (err == OK) {
                zipOpenNewFileInZip(dst_pkg_zip,
                        (pkg_name + ".app/Contents/Resources/" + pkg_name + ".pck").c_str(),
                        nullptr,
                        nullptr,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        Z_DEFLATED,
                        Z_DEFAULT_COMPRESSION);

                FileAccess *pf = FileAccess::open(pack_path, FileAccess::READ);
                if (pf) {
                    const int BSIZE = 16384;
                    uint8_t buf[BSIZE];

                    while (true) {

                        int r = pf->get_buffer(buf, BSIZE);
                        if (r <= 0)
                            break;
                        zipWriteInFileInZip(dst_pkg_zip, buf, r);
                    }

                    zipCloseFileInZip(dst_pkg_zip);
                    memdelete(pf);
                } else {
                    err = ERR_CANT_OPEN;
                }
            }

            if (err == OK) {
                //add shared objects
                for (int i = 0; i < shared_objects.size(); i++) {
                    PODVector<uint8_t> file = FileAccess::get_file_as_array(shared_objects[i].path);
                    ERR_CONTINUE(file.empty())

                    zipOpenNewFileInZip(dst_pkg_zip,
                            (PathUtils::plus_file((pkg_name + ".app/Contents/Frameworks/"),PathUtils::get_file(shared_objects[i].path))).c_str(),
                            nullptr,
                            nullptr,
                            0,
                            nullptr,
                            0,
                            nullptr,
                            Z_DEFLATED,
                            Z_DEFAULT_COMPRESSION);

                    zipWriteInFileInZip(dst_pkg_zip, file.data(), file.size());
                    zipCloseFileInZip(dst_pkg_zip);
                }
            }

            // Clean up generated file.
            DirAccess::remove_file_or_error(pack_path);
        }
    }

    if (dst_pkg_zip) {
        zipClose(dst_pkg_zip, nullptr);
    }

    return err;
}

bool EditorExportPlatformOSX::can_export(const Ref<EditorExportPreset> &p_preset, se_string &r_error, bool &r_missing_templates) const {

    se_string err;
    bool valid = false;

    // Look for export templates (first official, and if defined custom templates).

    bool dvalid = exists_export_template("osx.zip", &err);
    bool rvalid = dvalid; // Both in the same ZIP.

    if (p_preset->get("custom_template/debug") != "") {
        dvalid = FileAccess::exists(p_preset->get("custom_template/debug").as<se_string>());
        if (!dvalid) {
            err += TTR("Custom debug template not found.") + "\n";
        }
    }
    if (p_preset->get("custom_template/release") != "") {
        rvalid = FileAccess::exists(p_preset->get("custom_template/release").as<se_string>());
        if (!rvalid) {
            err += TTR("Custom release template not found.") + "\n";
        }
    }

    valid = dvalid || rvalid;
    r_missing_templates = !valid;

    if (!err.empty())
        r_error = err;
    return valid;
}
EditorExportPlatformOSX::EditorExportPlatformOSX() {

    Ref<Image> img(make_ref_counted<Image>(_osx_logo));
    logo = make_ref_counted<ImageTexture>();
    logo->create_from_image(img);
}

EditorExportPlatformOSX::~EditorExportPlatformOSX() {
}

void register_osx_exporter() {
    EditorExportPlatformOSX::initialize_class();
    Ref<EditorExportPlatformOSX> platform(make_ref_counted<EditorExportPlatformOSX>());

    EditorExport::get_singleton()->add_export_platform(platform);
}
