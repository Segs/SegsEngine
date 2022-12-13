/*************************************************************************/
/*  export_template_manager.cpp                                          */
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

#include "export_template_manager.h"
#include "progress_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/string_formatter.h"
#include "core/io/json.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/version.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "scene/gui/link_button.h"
#include "scene/gui/menu_button.h"
#include "scene/resources/font.h"
#include "scene/gui/separator.h"


IMPL_GDCLASS(ExportTemplateManager)

void ExportTemplateManager::_update_template_status() {
    // Fetch installed templates from the file system.
    DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    const String &templates_dir = EditorSettings::get_singleton()->get_templates_dir();

    Error err = da->change_dir(templates_dir);
    ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + templates_dir + "'.");

    Set<String> templates;
    err = da->list_dir_begin();
    if (err == OK) {
        String c = da->get_next();
        while (!c.empty()) {
            if (da->current_is_dir() && !c.starts_with('.')) {
                templates.insert(c);
            }
            c = da->get_next();
        }
    }
    da->list_dir_end();
    memdelete(da);

    // Update the state of the current version.
    String current_version = VERSION_FULL_CONFIG;
    current_value->set_text(current_version);

    if (templates.contains(current_version)) {
        current_missing_label->hide();
        current_installed_label->show();

        current_installed_hb->show();
        current_version_exists = true;
    } else {
        current_installed_label->hide();
        current_missing_label->show();

        current_installed_hb->hide();
        current_version_exists = false;
    }

    if (is_downloading_templates) {
        install_options_vb->hide();
        download_progress_hb->show();
    } else {
        download_progress_hb->hide();
        install_options_vb->show();

        if (templates.contains(current_version)) {
            current_installed_path->set_text(PathUtils::plus_file(templates_dir,current_version));
        }
    }

    // Update the list of other installed versions.
    installed_table->clear();
    TreeItem *installed_root = installed_table->create_item();

    for (auto riter=templates.rbegin(),rfin=templates.rend(); riter!=rfin; ++riter) {
        const String &version_string = *riter;
        if (version_string == current_version) {
            continue;
        }

        TreeItem *ti = installed_table->create_item(installed_root);
        ti->set_text_utf8(0, version_string);

        ti->add_button(0, get_theme_icon("Folder", "EditorIcons"), OPEN_TEMPLATE_FOLDER, false, TTR("Open the folder containing these templates."));
        ti->add_button(0, get_theme_icon("Remove", "EditorIcons"), UNINSTALL_TEMPLATE, false, TTR("Uninstall these templates."));
    }

    minimum_size_changed();
    update();
}

void ExportTemplateManager::_download_current() {
    if (is_downloading_templates) {
        return;
    }
    is_downloading_templates = true;

    install_options_vb->hide();
    download_progress_hb->show();

    if (mirrors_available) {
        String mirror_url = _get_selected_mirror();
        if (mirror_url.empty()) {
            _set_current_progress_status(TTRS("There are no mirrors available."), true);
            return;
        }

        _download_template(mirror_url, true);
    } else if (!mirrors_available && !is_refreshing_mirrors) {
        _set_current_progress_status(TTRS("Retrieving the mirror list..."));
        _refresh_mirrors();
    }
}

void ExportTemplateManager::_download_template(const String &p_url, bool p_skip_check) {
    if (!p_skip_check && is_downloading_templates) {
        return;
    }
    is_downloading_templates = true;

    install_options_vb->hide();
    download_progress_hb->show();
    _set_current_progress_status(TTRS("Starting the download..."));

    download_templates->set_download_file(PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"tmp_templates.tpz"));
    download_templates->set_use_threads(true);

    const String proxy_host = EDITOR_GET_T<String>("network/http_proxy/host");
    const int proxy_port = EDITOR_GET_T<int>("network/http_proxy/port");
    download_templates->set_http_proxy(proxy_host, proxy_port);
    download_templates->set_https_proxy(proxy_host, proxy_port);

    Error err = download_templates->request(p_url);
    if (err != OK) {
        _set_current_progress_status(TTRS("Error requesting URL:") + " " + p_url, true);
        return;
    }

    set_process(true);
    _set_current_progress_status(TTRS("Connecting to the mirror..."));
}

void ExportTemplateManager::_download_template_completed(int p_status, int p_code, const PoolStringArray &headers, const PoolByteArray &p_data) {
    switch (p_status) {
        case HTTPRequest::RESULT_CANT_RESOLVE: {
            _set_current_progress_status(TTRS("Can't resolve the requested address."), true);
        } break;
        case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
        case HTTPRequest::RESULT_CONNECTION_ERROR:
        case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH:
        case HTTPRequest::RESULT_SSL_HANDSHAKE_ERROR:
        case HTTPRequest::RESULT_CANT_CONNECT: {
            _set_current_progress_status(TTRS("Can't connect to the mirror."), true);
        } break;
        case HTTPRequest::RESULT_NO_RESPONSE: {
            _set_current_progress_status(TTRS("No response from the mirror."), true);
        } break;
        case HTTPRequest::RESULT_REQUEST_FAILED: {
            _set_current_progress_status(TTRS("Request failed."), true);
        } break;
        case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
            _set_current_progress_status(TTRS("Request ended up in a redirect loop."), true);
        } break;
        default: {
            if (p_code != 200) {
                _set_current_progress_status(TTRS("Request failed:") + " " + itos(p_code), true);
            } else {
                _set_current_progress_status(TTRS("Download complete; extracting templates..."));
                String path = download_templates->get_download_file();

                is_downloading_templates = false;
                bool ret = _install_file_selected(path, true);
                if (ret) {
                    // Clean up downloaded file.
                    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
                    Error err = da->remove(path);
                    if (err != OK) {
                        EditorNode::get_singleton()->add_io_error_utf8(TTRS("Cannot remove temporary file:") + "\n" + path + "\n");
                    }
                } else {
                    EditorNode::get_singleton()->add_io_error_utf8(FormatVE(TTR("Templates installation failed.\nThe problematic templates archives can be found at '%s'.").asCString(), path.c_str()));
                }
            }
        } break;
    }

    set_process(false);
}

void ExportTemplateManager::_cancel_template_download() {
    if (!is_downloading_templates) {
        return;
    }

    download_templates->cancel_request();
    download_progress_hb->hide();
    install_options_vb->show();
    is_downloading_templates = false;
}

void ExportTemplateManager::_refresh_mirrors() {
    if (is_refreshing_mirrors) {
        return;
    }
    is_refreshing_mirrors = true;

    String current_version = VERSION_FULL_CONFIG;
    const String mirrors_metadata_url = "https://godotengine.org/mirrorlist/" + current_version + ".json";
    request_mirrors->request(mirrors_metadata_url);
}
void ExportTemplateManager::_refresh_mirrors_completed(int p_status, int p_code, const PoolStringArray &headers, const PoolByteArray &p_data) {
    if (p_status != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
        EditorNode::get_singleton()->show_warning(TTR("Error getting the list of mirrors."));
        is_refreshing_mirrors = false;
        if (is_downloading_templates) {
            _cancel_template_download();
        }
        return;
    }

    String response_json;
    {
        PoolByteArray::Read r = p_data.read();
        response_json.assign((const char *)r.ptr(), p_data.size());
    }

    Variant json;
    String errs;
    int errline;
    Error err = JSON::parse(response_json, json, errs, errline);
    if (err != OK) {
        EditorNode::get_singleton()->show_warning(TTR("Error parsing JSON with the list of mirrors. Please report this issue!"));
        is_refreshing_mirrors = false;
        if (is_downloading_templates) {
            _cancel_template_download();
        }
        return;
    }

    mirrors_list->clear();
    mirrors_list->add_item(TTR("Best available mirror"), 0);

    mirrors_available = false;

    Dictionary data = json.as<Dictionary>();
    if (data.has("mirrors")) {
        Array mirrors = data["mirrors"].as<Array>();

        for (int i = 0; i < mirrors.size(); i++) {
            Dictionary m = mirrors[i].as<Dictionary>();
            ERR_CONTINUE(!m.has("url") || !m.has("name"));

            mirrors_list->add_item(m["name"].as<StringName>());
            mirrors_list->set_item_metadata(i + 1, m["url"]);

            mirrors_available = true;
        }
    }
    if (!mirrors_available) {
        EditorNode::get_singleton()->show_warning(TTR("No download links found for this version. Direct download is only available for official releases."));
        if (is_downloading_templates) {
            _cancel_template_download();
        }
    }

    is_refreshing_mirrors = false;

    if (is_downloading_templates) {
        String mirror_url = _get_selected_mirror();
        if (mirror_url.empty()) {
            _set_current_progress_status(TTRS("There are no mirrors available."), true);
            return;
        }

        _download_template(mirror_url, true);
    }
}

bool ExportTemplateManager::_humanize_http_status(HTTPRequest *p_request, String *r_status, int *r_downloaded_bytes, int *r_total_bytes) {
    *r_status = "";
    *r_downloaded_bytes = -1;
    *r_total_bytes = -1;
    bool success = true;

    switch (p_request->get_http_client_status()) {
        case HTTPClient::STATUS_DISCONNECTED:
            *r_status = TTR("Disconnected");
            success = false;
            break;
        case HTTPClient::STATUS_RESOLVING:
            *r_status = TTR("Resolving");
            break;
        case HTTPClient::STATUS_CANT_RESOLVE:
            *r_status = TTR("Can't Resolve");
            success = false;
            break;
        case HTTPClient::STATUS_CONNECTING:
            *r_status = TTR("Connecting...");
            break;
        case HTTPClient::STATUS_CANT_CONNECT:
            *r_status = TTR("Can't Connect");
            success = false;
            break;
        case HTTPClient::STATUS_CONNECTED:
            *r_status = TTR("Connected");
            break;
        case HTTPClient::STATUS_REQUESTING:
            *r_status = TTR("Requesting...");
            break;
        case HTTPClient::STATUS_BODY:
            *r_status = TTR("Downloading");
            *r_downloaded_bytes = p_request->get_downloaded_bytes();
            *r_total_bytes = p_request->get_body_size();

            if (p_request->get_body_size() > 0) {
                *r_status += " " + PathUtils::humanize_size(p_request->get_downloaded_bytes()) + "/" + PathUtils::humanize_size(p_request->get_body_size());
            } else {
                *r_status += " " + PathUtils::humanize_size(p_request->get_downloaded_bytes());
            }
            break;
        case HTTPClient::STATUS_CONNECTION_ERROR:
            *r_status = TTR("Connection Error");
            success = false;
            break;
        case HTTPClient::STATUS_SSL_HANDSHAKE_ERROR:
            *r_status = TTR("SSL Handshake Error");
            success = false;
            break;
    }

    return success;
}

void ExportTemplateManager::_set_current_progress_status(const String &p_status, bool p_error) {
    download_progress_bar->hide();
    download_progress_label->set_text(p_status);

    if (p_error) {
        download_progress_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
    } else {
        download_progress_label->add_theme_color_override("font_color", get_theme_color("font_color", "Label"));
    }
}

void ExportTemplateManager::_set_current_progress_value(float p_value, const String &p_status) {
    download_progress_bar->show();
    download_progress_bar->set_value(p_value);
    download_progress_label->set_text(p_status);

    // Progress cannot be happening with an error, so make sure that the color is correct.
    download_progress_label->add_theme_color_override("font_color", get_theme_color("font_color", "Label"));
}

void ExportTemplateManager::_install_file() {
    install_file_dialog->popup_centered_ratio();
}

bool ExportTemplateManager::_install_file_selected(const String &p_file, bool p_skip_progress) {
    // unzClose() will take care of closing the file stored in the unzFile,
    // so we don't need to `memdelete(fa)` in this method.
    FileAccess *fa = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&fa);

    unzFile pkg = unzOpen2(p_file.c_str(), &io);
    if (!pkg) {
        EditorNode::get_singleton()->show_warning(TTR("Can't open the export templates file."));
        return false;
    }
    int ret = unzGoToFirstFile(pkg);

    // Count them and find version.
    int fc = 0;
    String version;
    String contents_dir;

    while (ret == UNZ_OK) {

        unz_file_info info;
        char fname[16384];
        ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

        String file(fname);

        if (StringUtils::ends_with(file,"version.txt")) {

            Vector<uint8_t> data;
            data.resize(info.uncompressed_size);

            //read
            unzOpenCurrentFile(pkg);
            ret = unzReadCurrentFile(pkg, data.data(), data.size());
            unzCloseCurrentFile(pkg);

            String data_str((const char *)data.data(), data.size());
            data_str =StringUtils::strip_edges( data_str);

            // Version number should be of the form major.minor[.patch].status[.module_config]
            // so it can in theory have 3 or more slices.
            if (StringUtils::get_slice_count(data_str,'.') < 3) {
                EditorNode::get_singleton()->show_warning(FormatVE(TTR("Invalid version.txt format inside the export templates file: %s.").asCString(), data_str.c_str()));
                unzClose(pkg);
                return false;
            }

            version = data_str;
            contents_dir = PathUtils::trim_trailing_slash(PathUtils::get_base_dir(file));
        }

        if (not PathUtils::get_file(file).empty()) {
            fc++;
        }

        ret = unzGoToNextFile(pkg);
    }

    if (version.empty()) {
        EditorNode::get_singleton()->show_warning(TTR("No version.txt found inside the export templates file."));
        unzClose(pkg);
        return false;
    }

    DirAccessRef d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    String template_path = PathUtils::plus_file(EditorSettings::get_singleton()->get_templates_dir(),version);

    Error err = d->make_dir_recursive(template_path);
    if (err != OK) {
        EditorNode::get_singleton()->show_warning(
                FormatSN(TTR("Error creating path for extracting templates:\n%s").asCString(),template_path.c_str()));
        unzClose(pkg);
        return false;
    }


    EditorProgress *p = nullptr;
    if (!p_skip_progress) {
        p = memnew(EditorProgress(("ltask"), TTR("Extracting Export Templates"), fc));
    }

    fc = 0;
    ret = unzGoToFirstFile(pkg);

    while (ret == UNZ_OK) {

        //get filename
        unz_file_info info;
        char fname[16384];
        unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

        String file_path(PathUtils::simplify_path(fname));

        String file(PathUtils::get_file(file_path));

        if (file.empty()) {
            ret = unzGoToNextFile(pkg);
            continue;
        }

        Vector<uint8_t> data;
        data.resize(info.uncompressed_size);

        //read
        unzOpenCurrentFile(pkg);
        unzReadCurrentFile(pkg, data.data(), data.size());
        unzCloseCurrentFile(pkg);

        String base_dir(StringUtils::trim_suffix(PathUtils::get_base_dir(file_path),("/")));

        if (base_dir != contents_dir && StringUtils::begins_with(base_dir,contents_dir)) {
            base_dir = StringUtils::trim_prefix(StringUtils::substr(base_dir,contents_dir.length(), file_path.length()),("/"));
            file = PathUtils::plus_file(base_dir,file);

            DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
            ERR_CONTINUE(!da);

            String output_dir = PathUtils::plus_file(template_path,base_dir);

            if (!DirAccess::exists(output_dir)) {
                Error mkdir_err = da->make_dir_recursive(output_dir);
                ERR_CONTINUE(mkdir_err != OK);
            }
        }

        if (p) {
            p->step(TTR("Importing:") + " " + file, fc);
        }

        String to_write = PathUtils::plus_file(template_path,file);
        FileAccessRef f = FileAccess::open(to_write, FileAccess::WRITE);

        if (!f) {
            ret = unzGoToNextFile(pkg);
            fc++;
            ERR_CONTINUE_MSG(true, "Can't open file from path '" + (to_write) + "'.");
        }

        f->store_buffer(data.data(), data.size());

#ifndef WINDOWS_ENABLED
        FileAccess::set_unix_permissions(to_write, (info.external_fa >> 16) & 0x01FF);
#endif

        ret = unzGoToNextFile(pkg);
        fc++;
    }

    memdelete(p);

    unzClose(pkg);

    _update_template_status();

    return true;
}

void ExportTemplateManager::_uninstall_template(const String &p_version) {
    uninstall_confirm->set_text(FormatVE(TTR("Remove templates for the version '%s'?").asCString(), p_version.c_str()));
    uninstall_confirm->popup_centered();
    uninstall_version = p_version;
}

void ExportTemplateManager::_uninstall_template_confirmed() {
    DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    const String &templates_dir = EditorSettings::get_singleton()->get_templates_dir();
    const String msg_path(PathUtils::plus_file(templates_dir,uninstall_version));
    Error err = da->change_dir(templates_dir);
    ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + templates_dir + "'.");
    err = da->change_dir(uninstall_version);
    ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + msg_path + "'.");

    err = da->erase_contents_recursive();
    ERR_FAIL_COND_MSG(err != OK, "Could not remove all templates in '" + msg_path + "'.");

    da->change_dir("..");
    err = da->remove(uninstall_version);
    ERR_FAIL_COND_MSG(err != OK, "Could not remove templates directory at '" + msg_path + "'.");

    _update_template_status();
}
String ExportTemplateManager::_get_selected_mirror() const {
    if (mirrors_list->get_item_count() == 1) {
        return "";
    }

    int selected = mirrors_list->get_selected_id();
    if (selected == 0) {
        // This is a special "best available" value; so pick the first available mirror from the rest of the list.
        selected = 1;
    }

    return mirrors_list->get_item_metadata(selected).as<String>();
}

void ExportTemplateManager::_mirror_options_button_cbk(int p_id) {
    switch (p_id) {
        case VISIT_WEB_MIRROR: {
            String mirror_url = _get_selected_mirror();
            if (mirror_url.empty()) {
                EditorNode::get_singleton()->show_warning(TTR("There are no mirrors available."));
                return;
            }

            OS::get_singleton()->shell_open(mirror_url);
        } break;

        case COPY_MIRROR_URL: {
            String mirror_url = _get_selected_mirror();
            if (mirror_url.empty()) {
                EditorNode::get_singleton()->show_warning(TTR("There are no mirrors available."));
                return;
            }

            OS::get_singleton()->set_clipboard(mirror_url);
        } break;
    }
}
void ExportTemplateManager::_installed_table_button_cbk(Object *p_item, int p_column, int p_id) {
    TreeItem *ti = object_cast<TreeItem>(p_item);
    if (!ti) {
        return;
    }

    switch (p_id) {
        case OPEN_TEMPLATE_FOLDER: {
            String version_string = ti->get_text(0);
            _open_template_folder(version_string);
        } break;

        case UNINSTALL_TEMPLATE: {
            String version_string = ti->get_text(0);
            _uninstall_template(version_string);
        } break;
    }
}

void ExportTemplateManager::_open_template_folder(const String &p_version) {
    const String &templates_dir = EditorSettings::get_singleton()->get_templates_dir();
    OS::get_singleton()->shell_open("file://" + PathUtils::plus_file(templates_dir,p_version));
}

void ExportTemplateManager::popup_manager() {
    _update_template_status();
    _refresh_mirrors();
    popup_centered(Size2(720, 280) * EDSCALE);
}

void ExportTemplateManager::ok_pressed() {
    if (!is_downloading_templates) {
        hide();
        return;
    }

    hide_dialog_accept->popup_centered();
}
void ExportTemplateManager::cancel_pressed() {
    // This won't stop the window from closing, but will show the alert if the download is active.
    ok_pressed();
}

void ExportTemplateManager::_hide_dialog() {
    hide();
}
void ExportTemplateManager::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {
            current_value->add_font_override("font", get_theme_font("bold", "EditorFonts"));
            current_missing_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));
            current_installed_label->add_theme_color_override("font_color", get_theme_color("disabled_font_color", "Editor"));

            mirror_options_button->set_button_icon(get_theme_icon("GuiTabMenuHl", "EditorIcons"));
        } break;

        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (!is_visible()) {
                set_process(false);
            } else if (is_visible() && is_downloading_templates) {
                set_process(true);
            }
        } break;

        case NOTIFICATION_PROCESS: {
            update_countdown -= get_process_delta_time();
            if (update_countdown > 0) {
                return;
            }
            update_countdown = 0.5;

            String status;
            int downloaded_bytes;
            int total_bytes;
            bool success = _humanize_http_status(download_templates, &status, &downloaded_bytes, &total_bytes);

            if (downloaded_bytes >= 0) {
                if (total_bytes > 0) {
                    _set_current_progress_value(float(downloaded_bytes) / total_bytes, status);
                } else {
                    _set_current_progress_value(0, status);
                }
            } else {
                _set_current_progress_status(status);
            }

            if (!success) {
                set_process(false);
            }
        } break;
    }
}

ExportTemplateManager::ExportTemplateManager() {
    set_title(TTR("Export Template Manager"));
    set_hide_on_ok(false);
    get_ok()->set_text(TTR("Close"));

    // Downloadable export templates are only available for stable and official alpha/beta/RC builds
    // (which always have a number following their status, e.g. "alpha1").
    // Therefore, don't display download-related features when using a development version
    // (whose builds aren't numbered).
    downloads_available =
            StringView(VERSION_STATUS) != StringView("dev") &&
            StringView(VERSION_STATUS) != StringView("alpha") &&
            StringView(VERSION_STATUS) != StringView("beta") &&
            StringView(VERSION_STATUS) != StringView("rc");

    VBoxContainer *main_vb = memnew(VBoxContainer);
    add_child(main_vb);

    // Current version controls.
    HBoxContainer *current_hb = memnew(HBoxContainer);
    main_vb->add_child(current_hb);

    Label *current_label = memnew(Label);
    current_label->set_text(TTR("Current Version:"));
    current_hb->add_child(current_label);

    current_value = memnew(Label);
    current_hb->add_child(current_value);

    // Current version statuses.
    // Status: Current version is missing.
    current_missing_label = memnew(Label);
    current_missing_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    current_missing_label->set_align(Label::ALIGN_RIGHT);
    current_missing_label->set_text(TTR("Export templates are missing. Download them or install from a file."));
    current_hb->add_child(current_missing_label);

    // Status: Current version is installed.
    current_installed_label = memnew(Label);
    current_installed_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    current_installed_label->set_align(Label::ALIGN_RIGHT);
    current_installed_label->set_text(TTR("Export templates are installed and ready to be used."));
    current_hb->add_child(current_installed_label);
    current_installed_label->hide();

    // Currently installed template.
    current_installed_hb = memnew(HBoxContainer);
    main_vb->add_child(current_installed_hb);

    current_installed_path = memnew(LineEdit);
    current_installed_path->set_editable(false);
    current_installed_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    current_installed_hb->add_child(current_installed_path);

    current_open_button = memnew(Button);
    current_open_button->set_text(TTR("Open Folder"));
    current_open_button->set_tooltip(TTR("Open the folder containing installed templates for the current version."));
    current_installed_hb->add_child(current_open_button);
    current_open_button->connectF("pressed", this, [=]() { _open_template_folder(VERSION_FULL_CONFIG); });

    current_uninstall_button = memnew(Button);
    current_uninstall_button->set_text(TTR("Uninstall"));
    current_uninstall_button->set_tooltip(TTR("Uninstall templates for the current version."));
    current_installed_hb->add_child(current_uninstall_button);
    current_uninstall_button->connectF("pressed", this, [=]() { _uninstall_template(VERSION_FULL_CONFIG); });

    main_vb->add_child(memnew(HSeparator));

    // Download and install section.
    HBoxContainer *install_templates_hb = memnew(HBoxContainer);
    main_vb->add_child(install_templates_hb);

    // Download and install buttons are available.
    install_options_vb = memnew(VBoxContainer);
    install_options_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    install_templates_hb->add_child(install_options_vb);

    HBoxContainer *download_install_hb = memnew(HBoxContainer);
    install_options_vb->add_child(download_install_hb);

    Label *mirrors_label = memnew(Label);
    mirrors_label->set_text(TTR("Download from:"));
    download_install_hb->add_child(mirrors_label);

    mirrors_list = memnew(OptionButton);
    mirrors_list->set_custom_minimum_size(Size2(280, 0) * EDSCALE);
    download_install_hb->add_child(mirrors_list);
    mirrors_list->add_item(TTR("Best available mirror"), 0);

    request_mirrors = memnew(HTTPRequest);
    mirrors_list->add_child(request_mirrors);
    request_mirrors->connect("request_completed", callable_mp(this, &ClassName::_refresh_mirrors_completed));

    mirror_options_button = memnew(MenuButton);
    mirror_options_button->get_popup()->add_item(TTR("Open in Web Browser"), VISIT_WEB_MIRROR);
    mirror_options_button->get_popup()->add_item(TTR("Copy Mirror URL"), COPY_MIRROR_URL);
    download_install_hb->add_child(mirror_options_button);
    mirror_options_button->get_popup()->connect("id_pressed", callable_mp(this, &ClassName::_mirror_options_button_cbk));

    download_install_hb->add_spacer();

    Button *download_current_button = memnew(Button);
    download_current_button->set_text(TTR("Download and Install"));
    download_current_button->set_tooltip(TTR("Download and install templates for the current version from the best possible mirror."));
    download_install_hb->add_child(download_current_button);
    download_current_button->connect("pressed", callable_mp(this, &ClassName::_download_current));

    // Update downloads buttons to prevent unsupported downloads.
    if (!downloads_available) {
        download_current_button->set_disabled(true);
        download_current_button->set_tooltip(TTR("Official export templates aren't available for development builds."));
    }

    HBoxContainer *install_file_hb = memnew(HBoxContainer);
    install_file_hb->set_alignment(BoxContainer::ALIGN_END);
    install_options_vb->add_child(install_file_hb);

    install_file_button = memnew(Button);
    install_file_button->set_text(TTR("Install from File"));
    install_file_button->set_tooltip(TTR("Install templates from a local file."));
    install_file_hb->add_child(install_file_button);
    install_file_button->connect("pressed", callable_mp(this, &ClassName::_install_file));

    // Templates are being downloaded; buttons unavailable.
    download_progress_hb = memnew(HBoxContainer);
    download_progress_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    install_templates_hb->add_child(download_progress_hb);
    download_progress_hb->hide();

    download_progress_bar = memnew(ProgressBar);
    download_progress_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    download_progress_bar->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    download_progress_bar->set_min(0);
    download_progress_bar->set_max(1);
    download_progress_bar->set_value(0);
    download_progress_bar->set_step(0.01f);
    download_progress_hb->add_child(download_progress_bar);

    download_progress_label = memnew(Label);
    download_progress_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    download_progress_label->set_clip_text(true);
    download_progress_hb->add_child(download_progress_label);

    Button *download_cancel_button = memnew(Button);
    download_cancel_button->set_text(TTR("Cancel"));
    download_cancel_button->set_tooltip(TTR("Cancel the download of the templates."));
    download_progress_hb->add_child(download_cancel_button);
    download_cancel_button->connect("pressed", callable_mp(this, &ClassName::_cancel_template_download));

    download_templates = memnew(HTTPRequest);
    install_templates_hb->add_child(download_templates);
    download_templates->connect("request_completed", callable_mp(this, &ClassName::_download_template_completed));

    main_vb->add_child(memnew(HSeparator));

    // Other installed templates table.
    HBoxContainer *installed_versions_hb = memnew(HBoxContainer);
    main_vb->add_child(installed_versions_hb);
    Label *installed_label = memnew(Label);
    installed_label->set_text(TTR("Other Installed Versions:"));
    installed_versions_hb->add_child(installed_label);

    installed_table = memnew(Tree);
    installed_table->set_hide_root(true);
    installed_table->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
    installed_table->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    main_vb->add_child(installed_table);
    installed_table->connect("button_pressed", callable_mp(this, &ClassName::_installed_table_button_cbk));

    // Dialogs.
    uninstall_confirm = memnew(ConfirmationDialog);
    uninstall_confirm->set_title(TTR("Uninstall Template"));
    add_child(uninstall_confirm);
    uninstall_confirm->connect("confirmed", callable_mp(this, &ClassName::_uninstall_template_confirmed));

    install_file_dialog = memnew(FileDialog);
    install_file_dialog->set_title(TTR("Select Template File"));
    install_file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
    install_file_dialog->set_mode(FileDialog::MODE_OPEN_FILE);
    install_file_dialog->add_filter("*.tpz ; " + TTR("Godot Export Templates"));
    auto handler = [=](const String &p_file) { _install_file_selected(p_file); };
    install_file_dialog->connect("file_selected", callable_gen(this, handler) );
    add_child(install_file_dialog);

    hide_dialog_accept = memnew(AcceptDialog);
    hide_dialog_accept->set_text(TTR("The templates will continue to download.\nYou may experience a short editor freeze when they finish."));
    add_child(hide_dialog_accept);
    hide_dialog_accept->connect("confirmed", callable_mp(this, &ClassName::_hide_dialog));
}
