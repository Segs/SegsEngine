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

#include "windows_export.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/string_formatter.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "platform/windows/logo.gen.h"

class EditorExportPlatformWindows final : public EditorExportPlatform {
    Error _rcedit_add_data(const Ref<EditorExportPreset> &p_preset, StringView p_path);
    Error _code_sign(const Ref<EditorExportPreset> &p_preset, StringView p_path);

public:
    Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags = 0) override;
    Error sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path) override;
    Error modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) override;
    Error fixup_embedded_pck(StringView p_path, int64_t p_embedded_start, int64_t p_embedded_size) override;
    void get_export_options(Vector<ExportOption> *r_options) override;
    bool get_option_visibility(const EditorExportPreset *p_preset, const StringName &p_option, const HashMap<StringName, Variant> &p_options) const override;
    bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const override;
    bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
};

Error EditorExportPlatformWindows::modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) {
    if (p_preset->get("application/modify_resources")) {
        _rcedit_add_data(p_preset, p_path);
    }
    return OK;
}

Error EditorExportPlatformWindows::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path, int p_flags) {
    String pck_path(p_path);
    if (p_preset->get("binary_format/embed_pck")) {
        pck_path = String(PathUtils::get_basename(p_path)) + ".tmp";
    }

    Error err = EditorExportPlatform::export_project(p_preset, p_debug, pck_path, p_flags);
    if (p_preset->get("codesign/enable") && err == OK) {
        _code_sign(p_preset, pck_path);
    }

    if (p_preset->get("binary_format/embed_pck") && err == OK) {
        DirAccessRef tmp_dir = DirAccess::create_for_path(PathUtils::get_base_dir(p_path));
        err = tmp_dir->rename(pck_path, p_path);
        if (err != OK) {
            add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding").asCString(), FormatVE(TTR("Failed to rename temporary file \"%s\".").asCString(), pck_path.c_str()));
        }
    }

    return err;
}

Error EditorExportPlatformWindows::sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, StringView p_path) {
    if (p_preset->get("codesign/enable").as<bool>()) {
        return _code_sign(p_preset, p_path);
    } else {
        return OK;
    }
}

bool EditorExportPlatformWindows::get_option_visibility(const EditorExportPreset *p_preset,const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {
    // This option is not supported by "osslsigncode", used on non-Windows host.
    if (!OS::get_singleton()->has_feature("Windows") && p_option == "codesign/identity_type") {
        return false;
    }
    return true;
}

void EditorExportPlatformWindows::get_export_options(Vector<EditorExportPlatform::ExportOption> *r_options) {
    EditorExportPlatform::get_export_options(r_options);

    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "codesign/enable"), false));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::INT, "codesign/identity_type", PropertyHint::Enum, "Select automatically,Use PKCS12 file (specify *.PFX/*.P12 file),Use certificate store (specify SHA1 hash)"), 0));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/identity", PropertyHint::GlobalFile, "*.pfx,*.p12"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/password"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "codesign/timestamp"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/timestamp_server_url"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::INT, "codesign/digest_algorithm", PropertyHint::Enum, "SHA1,SHA256"), 1));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "codesign/description"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::POOL_STRING_ARRAY, "codesign/custom_options"), PoolStringArray()));

    r_options->push_back(ExportOption(PropertyInfo(VariantType::BOOL, "application/modify_resources"), true));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/icon", PropertyHint::File, "*.ico"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/file_version", PropertyHint::PlaceholderText, "1.0.0.0"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/product_version", PropertyHint::PlaceholderText, "1.0.0.0"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/company_name", PropertyHint::PlaceholderText, "Company Name"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/product_name", PropertyHint::PlaceholderText, "Game Name"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/file_description"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/copyright"), ""));
    r_options->push_back(ExportOption(PropertyInfo(VariantType::STRING, "application/trademarks"), ""));
}

Error EditorExportPlatformWindows::_rcedit_add_data(const Ref<EditorExportPreset> &p_preset, StringView p_path) {
    String rcedit_path = EditorSettings::get_singleton()->get("export/windows/rcedit").as<String>();

    if (!rcedit_path.empty() || !FileAccess::exists(rcedit_path)) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification").asCString(), FormatVE(TTR("Could not find rcedit executable at \"%s\".").asCString(), rcedit_path.c_str()));
        return ERR_FILE_NOT_FOUND;
    }

    if (rcedit_path.empty()) {
        rcedit_path = "rcedit"; // try to run rcedit from PATH
    }

#ifndef WINDOWS_ENABLED
    // On non-Windows we need WINE to run rcedit
    String wine_path = EditorSettings::get_singleton()->get("export/windows/wine").as<String>();

    if (not wine_path.empty() && !FileAccess::exists(wine_path)) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification").asCString(), FormatVE(TTR("Could not find wine executable at \"%s\".").asCString(), wine_path.c_str()));
        return ERR_FILE_NOT_FOUND;
    }
    if (wine_path.empty()) {
        wine_path = "wine"; // try to run wine from PATH
    }
#endif

    String icon_path = ProjectSettings::get_singleton()->globalize_path(p_preset->get("application/icon").as<String>());
    String file_verion = p_preset->get("application/file_version").as<String>();
    String product_version = p_preset->get("application/product_version").as<String>();
    String company_name = p_preset->get("application/company_name").as<String>();
    String product_name = p_preset->get("application/product_name").as<String>();
    String file_description = p_preset->get("application/file_description").as<String>();
    String copyright = p_preset->get("application/copyright").as<String>();
    String trademarks = p_preset->get("application/trademarks").as<String>();
    String comments = p_preset->get("application/comments").as<String>();

    Vector<String> args;
    args.emplace_back(p_path);
    if (!icon_path.empty()) {
        args.push_back(("--set-icon"));
        args.push_back(icon_path);
    }
    if (!file_verion.empty()) {
        args.push_back(("--set-file-version"));
        args.push_back(file_verion);
    }
    if (!product_version.empty()) {
        args.push_back(("--set-product-version"));
        args.push_back(product_version);
    }
    if (!company_name.empty()) {
        args.push_back(("--set-version-string"));
        args.push_back(("CompanyName"));
        args.push_back(company_name);
    }
    if (!product_name.empty()) {
        args.push_back(("--set-version-string"));
        args.push_back(("ProductName"));
        args.push_back(product_name);
    }
    if (!file_description.empty()) {
        args.push_back(("--set-version-string"));
        args.push_back(("FileDescription"));
        args.push_back(file_description);
    }
    if (!copyright.empty()) {
        args.push_back(("--set-version-string"));
        args.push_back(("LegalCopyright"));
        args.push_back(copyright);
    }
    if (!trademarks.empty()) {
        args.push_back(("--set-version-string"));
        args.push_back(("LegalTrademarks"));
        args.push_back(trademarks);
    }
#ifndef WINDOWS_ENABLED
    // On non-Windows we need WINE to run rcedit
    args.push_front(rcedit_path);
    rcedit_path = wine_path;
#endif

    String str;
    Error err = OS::get_singleton()->execute(rcedit_path, args, true, nullptr, &str, nullptr, true);
    if (err != OK || (str.contains("not found")) || (str.contains("not recognized"))) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), TTR("Could not start rcedit executable. Configure rcedit path in the Editor Settings (Export > Windows > rcedit), or disable \"Application > Modify Resources\" in the export preset."));
        return err;
    }
    print_line("rcedit (" + p_path + "): " + str);

    if (str.contains("Fatal error")) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification").asCString(), FormatVE(TTR("rcedit failed to modify executable: %s.").asCString(), str.c_str()));
        return FAILED;
    }

    return OK;
}
Error EditorExportPlatformWindows::_code_sign(const Ref<EditorExportPreset> &p_preset, StringView p_path) {
    Vector<String> args;

#ifdef WINDOWS_ENABLED
    String signtool_path = EditorSettings::get_singleton()->getT<String>("export/windows/signtool");
    if (not signtool_path.empty() && !FileAccess::exists(signtool_path)) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing").asCString(), FormatVE(TTR("Could not find signtool executable at \"%s\".").asCString(), signtool_path.c_str()));
        return ERR_FILE_NOT_FOUND;
    }
    if (signtool_path.empty()) {
        signtool_path = "signtool"; // try to run signtool from PATH
    }
#else
    String signtool_path = EditorSettings::get_singleton()->getT<String>("export/windows/osslsigncode");
    if (not signtool_path.empty() && !FileAccess::exists(signtool_path)) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing").asCString(), FormatVE(TTR("Could not find osslsigncode executable at \"%s\".").asCString(), signtool_path.c_str()));
        return ERR_FILE_NOT_FOUND;
    }
    if (not signtool_path.empty()) {
        signtool_path = "osslsigncode"; // try to run signtool from PATH
    }
#endif

    args.push_back(("sign"));

    //identity
#ifdef WINDOWS_ENABLED
    int id_type = p_preset->getT<int>("codesign/identity_type");
    if (id_type == 0) { //auto select
        args.push_back("/a");
    } else if (id_type == 1) { //pkcs12
        if (p_preset->get("codesign/identity") != "") {
            args.push_back("/f");
            args.push_back(p_preset->getT<String>("codesign/identity"));
        } else {
            add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
            return FAILED;
        }
    } else if (id_type == 2) { //Windows certificate store
        if (p_preset->get("codesign/identity") != "") {
            args.push_back("/sha1");
            args.push_back(p_preset->getT<String>("codesign/identity"));
        } else {
            add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
            return FAILED;
        }
    } else {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Invalid identity type."));
        return FAILED;
    }
#else
    if (p_preset->get("codesign/identity") != "") {
        args.push_back(("-pkcs12"));
        args.push_back(p_preset->get("codesign/identity").as<String>());
    } else {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
        return FAILED;
    }
#endif

    //password
    if (p_preset->get("codesign/password") != "") {
#ifdef WINDOWS_ENABLED
        args.push_back("/p");
#else
        args.push_back(("-pass"));
#endif
        args.push_back(p_preset->get("codesign/password").as<String>());
    }

    //timestamp
    if (p_preset->get("codesign/timestamp").as<bool>()) {
        if (p_preset->get("codesign/timestamp_server") != "") {
#ifdef WINDOWS_ENABLED
            args.push_back("/tr");
            args.push_back(p_preset->getT<String>("codesign/timestamp_server_url"));
            args.push_back("/td");
            if (p_preset->getT<int>("codesign/digest_algorithm") == 0) {
                args.push_back("sha1");
            } else {
                args.push_back("sha256");
            }
#else
            args.push_back(("-ts"));
            args.push_back(p_preset->getT<String>("codesign/timestamp_server_url"));
#endif
        } else {
            add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Invalid timestamp server."));
            return FAILED;
        }
    }

    //digest
#ifdef WINDOWS_ENABLED
    args.push_back("/fd");
#else
    args.push_back(("-h"));
#endif
    if (p_preset->getT<int>("codesign/digest_algorithm") == 0) {
        args.push_back(("sha1"));
    } else {
        args.push_back(("sha256"));
    }

    //description
    if (p_preset->get("codesign/description") != "") {
#ifdef WINDOWS_ENABLED
        args.push_back("/d");
#else
        args.push_back(("-n"));
#endif
        args.push_back(p_preset->get("codesign/description").as<String>());
    }

    //user options
    PoolVector<String> user_args(p_preset->get("codesign/custom_options").as<PoolVector<String>>());
    for (int i = 0; i < user_args.size(); i++) {
        String user_arg(StringUtils::strip_edges(user_args[i]));
        if (!user_arg.empty()) {
            args.emplace_back(eastl::move(user_arg));
        }
    }

#ifndef WINDOWS_ENABLED
    args.push_back(("-in"));
#endif
    args.emplace_back(p_path);
#ifndef WINDOWS_ENABLED
    args.push_back(("-out"));
    args.emplace_back(String(p_path) + "_signed");
#endif

    String str;
    Error err = OS::get_singleton()->execute(signtool_path, args, true, nullptr, &str, nullptr, true);
    if (err != OK || (str.contains("not found")) || (str.contains("not recognized"))) {
#ifndef WINDOWS_ENABLED
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start signtool executable. Configure signtool path in the Editor Settings (Export > Windows > signtool), or disable \"Codesign\" in the export preset."));
#else
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start osslsigncode executable. Configure signtool path in the Editor Settings (Export > Windows > osslsigncode), or disable \"Codesign\" in the export preset."));
#endif
        return err;
    }

    print_line("codesign (" + String(p_path) + "): " + str);
#ifndef WINDOWS_ENABLED
    if (StringUtils::contains(str,"SignTool Error")) {
#else
    if (StringUtils::contains(str,"Failed")) {
#endif
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing").asCString(), FormatVE(TTR("Signtool failed to sign executable: %s.").asCString(), str.c_str()));
        return FAILED;
    }
#ifndef WINDOWS_ENABLED
    DirAccessRef tmp_dir = DirAccess::create_for_path(PathUtils::get_base_dir(p_path));

    err = tmp_dir->remove(p_path);
    if (err != OK) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing").asCString(), FormatVE(TTR("Failed to remove temporary file \"%.*s\".").asCString(), p_path.size(), p_path.data()));
        return err;
    }

    err = tmp_dir->rename(String(p_path) + "_signed", p_path);
    if (err != OK) {
        add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing").asCString(), FormatVE(TTR("Failed to rename temporary file \"%s\".").asCString(), (String(p_path) + "_signed").c_str()));
        return err;
    }
#endif
    return OK;
}
bool EditorExportPlatformWindows::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates) const {
    String err = "";
    bool valid = EditorExportPlatform::has_valid_export_configuration(p_preset, err, r_missing_templates);

    String rcedit_path = EditorSettings::get_singleton()->getT<String>("export/windows/rcedit");
    if (p_preset->get("application/modify_resources") && rcedit_path.empty()) {
        err += TTR("The rcedit tool must be configured in the Editor Settings (Export > Windows > rcedit) to change the icon or app information data.") + "\n";
    }
    if (!err.empty()) {
        r_error = err;
    }

    return valid;
}

bool EditorExportPlatformWindows::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
    String err = "";
    bool valid = true;

    String icon_path = ProjectSettings::get_singleton()->globalize_path(p_preset->getT<String>("application/icon"));
    if (!icon_path.empty() && !FileAccess::exists(icon_path)) {
        err += TTR("Invalid icon path:") + " " + icon_path + "\n";
    }

    // Only non-negative integers can exist in the version string.
    const auto all_valid_integers = [](Span<const StringView> v)->bool {
        for(StringView str : v) {
            if(!StringUtils::is_valid_integer(str))
                return false;
        }
        return true;
    };

    String file_version = p_preset->getT<String>("application/file_version");
    if (!file_version.empty()) {
        FixedVector<StringView,4,true> version_array;
        file_version.split_ref(version_array,".", false);

        if (version_array.size() != 4 || !all_valid_integers(version_array) || file_version.contains('-')) {
            err += TTR("Invalid file version:") + " " + file_version + "\n";
        }
    }

    String product_version = p_preset->getT<String>("application/product_version");
    if (!product_version.empty()) {
        FixedVector<StringView,4,true> version_array;
        product_version.split_ref(version_array,".", false);
        if (version_array.size() != 4 || !all_valid_integers(version_array) || product_version.contains('-')) {
            err += TTR("Invalid product version:") + " " + product_version + "\n";
        }
    }

    if (!err.empty()) {
        r_error = err;
    }

    return valid;
}

Error EditorExportPlatformWindows::fixup_embedded_pck(StringView p_path, int64_t p_embedded_start, int64_t p_embedded_size) {
    // Patch the header of the "pck" section in the PE file so that it corresponds to the embedded data

    if (p_embedded_size + p_embedded_start >= 0x100000000) { // Check for total executable size
        add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Windows executables cannot be >= 4 GiB."));
        return ERR_INVALID_DATA;
    }

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ_WRITE);
    if (!f) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding").asCString(), FormatVE(TTR("Failed to open executable file \"%.*s\".").asCString(), p_path.size(), p_path.data()));
        return ERR_CANT_OPEN;
    }

         // Jump to the PE header and check the magic number
    {
        f->seek(0x3c);
        uint32_t pe_pos = f->get_32();

        f->seek(pe_pos);
        uint32_t magic = f->get_32();
        if (magic != 0x00004550) {
            f->close();
            add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable file header corrupted."));
            return ERR_FILE_CORRUPT;
        }
    }

         // Process header

    int num_sections;
    {
        int64_t header_pos = f->get_position();

        f->seek(header_pos + 2);
        num_sections = f->get_16();
        f->seek(header_pos + 16);
        uint16_t opt_header_size = f->get_16();

             // Skip rest of header + optional header to go to the section headers
        f->seek(f->get_position() + 2 + opt_header_size);
    }

         // Search for the "pck" section

    int64_t section_table_pos = f->get_position();

    bool found = false;
    for (int i = 0; i < num_sections; ++i) {

        int64_t section_header_pos = section_table_pos + i * 40;
        f->seek(section_header_pos);

        uint8_t section_name[9];
        f->get_buffer(section_name, 8);
        section_name[8] = '\0';

        if (strcmp((char *)section_name, "pck") == 0) {
            // "pck" section found, let's patch!

                 // Set virtual size to a little to avoid it taking memory (zero would give issues)
            f->seek(section_header_pos + 8);
            f->store_32(8);

            f->seek(section_header_pos + 16);
            f->store_32(p_embedded_size);
            f->seek(section_header_pos + 20);
            f->store_32(p_embedded_start);

            found = true;
            break;
        }
    }

    f->close();

    if (!found) {
        add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable \"pck\" section not found."));
        return ERR_FILE_CORRUPT;
    }
    return OK;
}


void register_windows_exporter() {
    EDITOR_DEF("export/windows/rcedit", "");
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "export/windows/rcedit", PropertyHint::GlobalFile, "*.exe"));
#ifdef WINDOWS_ENABLED
    EDITOR_DEF("export/windows/signtool", "");
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "export/windows/signtool", PropertyHint::GlobalFile, "*.exe"));
#else
    EDITOR_DEF("export/windows/osslsigncode", "");
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "export/windows/osslsigncode", PropertyHint::GlobalFile));
    // On non-Windows we need WINE to run rcedit
    EDITOR_DEF("export/windows/wine", "");
    EditorSettings::get_singleton()->add_property_hint(PropertyInfo(VariantType::STRING, "export/windows/wine", PropertyHint::GlobalFile));
#endif
    EditorExportPlatformWindows::initialize_class();
    Ref<EditorExportPlatformWindows> platform(make_ref_counted<EditorExportPlatformWindows>());

    Ref<Image> img(make_ref_counted<Image>(_windows_logo));
    Ref<ImageTexture> logo(make_ref_counted<ImageTexture>());
    logo->create_from_image(img);
    platform->set_logo(logo);
    platform->set_name(("Windows Desktop"));
    platform->set_extension(("exe"));
    platform->set_release_32(("windows_32_release.exe"));
    platform->set_debug_32(("windows_32_debug.exe"));
    platform->set_release_64(("windows_64_release.exe"));
    platform->set_debug_64(("windows_64_debug.exe"));
    platform->set_os_name(("Windows"));

    EditorExport::get_singleton()->add_export_platform(platform);
}

