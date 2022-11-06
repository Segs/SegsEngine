/*************************************************************************/
/*  asset_library_editor_plugin.cpp                                      */
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

#include "asset_library_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/fixed_string.h"
#include "core/io/json.h"
#include "core/io/image_loader.h"
#include "core/method_bind.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_scale.h"
#include "editor/project_settings_editor.h"
#include "scene/resources/style_box.h"

#include <utility>

#include "core/io/stream_peer_ssl.h"

IMPL_GDCLASS(EditorAssetLibraryItem)
IMPL_GDCLASS(EditorAssetLibraryItemDescription)
IMPL_GDCLASS(EditorAssetLibraryItemDownload)
IMPL_GDCLASS(EditorAssetLibrary)
IMPL_GDCLASS(AssetLibraryEditorPlugin)

static inline void setup_http_request(HTTPRequest *request) {
    request->set_use_threads(EDITOR_DEF_T("asset_library/use_threads", true));

    const String proxy_host = EDITOR_GET_T<String>("network/http_proxy/host");
    const int proxy_port = EDITOR_GET_T<int>("network/http_proxy/port");
    request->set_http_proxy(proxy_host, proxy_port);
    request->set_https_proxy(proxy_host, proxy_port);
}

void EditorAssetLibraryItem::configure(const StringName &p_title, int p_asset_id, StringView p_category, int p_category_id, StringView p_author, int p_author_id, StringView p_cost) {

    title->set_text(p_title.asCString());
    asset_id = p_asset_id;
    category->set_text(p_category);
    category_id = p_category_id;
    author->set_text(p_author);
    author_id = p_author_id;
    price->set_text(StringName(p_cost));
}

void EditorAssetLibraryItem::set_image(int p_type, int p_index, const Ref<Texture> &p_image) {

    ERR_FAIL_COND(p_type != EditorAssetLibrary::IMAGE_QUEUE_ICON);
    ERR_FAIL_COND(p_index != 0);

    icon->set_normal_texture(p_image);
}

void EditorAssetLibraryItem::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        icon->set_normal_texture(get_theme_icon("ProjectIconLoading", "EditorIcons"));
        category->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
        author->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
        price->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
    }
}

void EditorAssetLibraryItem::_asset_clicked() {

    emit_signal("asset_selected", asset_id);
}

void EditorAssetLibraryItem::_category_clicked() {

    emit_signal("category_selected", category_id);
}
void EditorAssetLibraryItem::_author_clicked() {

    emit_signal("author_selected", author_id);
}

void EditorAssetLibraryItem::_bind_methods() {

    MethodBinder::bind_method("set_image", &EditorAssetLibraryItem::set_image);
    ADD_SIGNAL(MethodInfo("asset_selected"));
    ADD_SIGNAL(MethodInfo("category_selected"));
    ADD_SIGNAL(MethodInfo("author_selected"));
}

EditorAssetLibraryItem::EditorAssetLibraryItem() {

    Ref<StyleBoxEmpty> border(make_ref_counted<StyleBoxEmpty>());
    border->set_default_margin(Margin::Left, 5 * EDSCALE);
    border->set_default_margin(Margin::Right, 5 * EDSCALE);
    border->set_default_margin(Margin::Bottom, 5 * EDSCALE);
    border->set_default_margin(Margin::Top, 5 * EDSCALE);
    add_theme_style_override("panel", border);

    HBoxContainer *hb = memnew(HBoxContainer);
    // Add some spacing to visually separate the icon from the asset details.
    hb->add_constant_override("separation", 15 * EDSCALE);
    add_child(hb);

    icon = memnew(TextureButton);
    icon->set_custom_minimum_size(Size2(64, 64) * EDSCALE);
    icon->set_default_cursor_shape(CURSOR_POINTING_HAND);
    icon->connect("pressed",callable_mp(this, &ClassName::_asset_clicked));

    hb->add_child(icon);

    VBoxContainer *vb = memnew(VBoxContainer);

    hb->add_child(vb);
    vb->set_h_size_flags(SIZE_EXPAND_FILL);

    title = memnew(LinkButton);
    title->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
    title->connect("pressed",callable_mp(this, &ClassName::_asset_clicked));
    vb->add_child(title);

    category = memnew(LinkButton);
    category->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
    category->connect("pressed",callable_mp(this, &ClassName::_category_clicked));
    vb->add_child(category);

    author = memnew(LinkButton);
    author->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
    author->connect("pressed",callable_mp(this, &ClassName::_author_clicked));
    vb->add_child(author);

    price = memnew(Label);
    vb->add_child(price);

    set_custom_minimum_size(Size2(250, 100) * EDSCALE);
    set_h_size_flags(SIZE_EXPAND_FILL);
}

//////////////////////////////////////////////////////////////////////////////

void EditorAssetLibraryItemDescription::set_image(int p_type, int p_index, const Ref<Texture> &p_image) {

    switch (p_type) {

        case EditorAssetLibrary::IMAGE_QUEUE_ICON: {

            item->call_va("set_image", p_type, p_index, p_image);
            icon = p_image;
        } break;
        case EditorAssetLibrary::IMAGE_QUEUE_THUMBNAIL: {

            for (int i = 0; i < preview_images.size(); i++) {
                if (preview_images[i].id == p_index) {
                    if (preview_images[i].is_video) {
                        Ref<Image> overlay = get_theme_icon("PlayOverlay", "EditorIcons")->get_data();
                        Ref<Image> thumbnail = p_image->get_data();
                        thumbnail = dynamic_ref_cast<Image>(thumbnail->duplicate());
                        Point2 overlay_pos = Point2((thumbnail->get_width() - overlay->get_width()) / 2, (thumbnail->get_height() - overlay->get_height()) / 2);
                        // Overlay and thumbnail need the same format for `blend_rect` to work.
                        thumbnail->convert(ImageData::FORMAT_RGBA8);
                        thumbnail->lock();
                        thumbnail->blend_rect(overlay, overlay->get_used_rect(), overlay_pos);
                        thumbnail->unlock();

                        Ref<ImageTexture> tex(make_ref_counted<ImageTexture>());
                        tex->create_from_image(thumbnail);

                        preview_images[i].button->set_button_icon(tex);
                        // Make it clearer that clicking it will open an external link
                        preview_images[i].button->set_default_cursor_shape(CURSOR_POINTING_HAND);
                    } else {
                        preview_images[i].button->set_button_icon(p_image);
                    }
                    break;
                }
            }
        } break;
        case EditorAssetLibrary::IMAGE_QUEUE_SCREENSHOT: {

            for (int i = 0; i < preview_images.size(); i++) {
                if (preview_images[i].id == p_index) {
                    preview_images[i].image = p_image;
                    if (preview_images[i].button->is_pressed()) {
                        _preview_click(p_index);
                    }
                    break;
                }
            }
        } break;
    }
}
void EditorAssetLibraryItemDescription::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            previews_bg->add_theme_style_override("panel", get_theme_stylebox("normal", "TextEdit"));
        } break;
    }
}
void EditorAssetLibraryItemDescription::_bind_methods() {
    SE_BIND_METHOD(EditorAssetLibraryItemDescription,set_image);
    SE_BIND_METHOD(EditorAssetLibraryItemDescription,_link_click);
    SE_BIND_METHOD(EditorAssetLibraryItemDescription,_preview_click);
}

void EditorAssetLibraryItemDescription::_link_click(StringView p_url) {
    ERR_FAIL_COND(!StringUtils::begins_with(p_url,"http"));
    OS::get_singleton()->shell_open(p_url);
}

void EditorAssetLibraryItemDescription::_preview_click(int p_id) {
    for (int i = 0; i < preview_images.size(); i++) {
        if (preview_images[i].id == p_id) {
            preview_images[i].button->set_pressed(true);
            if (!preview_images[i].is_video) {
                if (preview_images[i].image) {
                    preview->set_texture(preview_images[i].image);
                    minimum_size_changed();
                }
            } else {
                _link_click(preview_images[i].video_link);
            }
        } else {
            preview_images[i].button->set_pressed(false);
        }
    }
}

void EditorAssetLibraryItemDescription::configure(const StringName &p_title, int p_asset_id, StringView p_category,
        int p_category_id, StringView p_author, int p_author_id, StringView p_cost, int p_version,
        StringView p_version_string, StringView p_description, StringView p_download_url,
        StringView p_browse_url, StringView p_sha256_hash) {

    asset_id = p_asset_id;
    title = p_title;
    download_url = p_download_url;
    sha256 = p_sha256_hash;
    item->configure(p_title, p_asset_id, p_category, p_category_id, p_author, p_author_id, p_cost);
    description->clear();
    description->add_text(String(TTR("Version:")) + " " + p_version_string + "\n");
    description->add_text(String(TTR("Contents:")) + " ");
    description->push_meta(p_browse_url);
    description->add_text(TTR("View Files"));
    description->pop();
    description->add_text("\n" + String(TTR("Description:")) + "\n\n");
    description->append_bbcode(p_description);
    description->set_selection_enabled(true);
    set_title(p_title);
}

void EditorAssetLibraryItemDescription::add_preview(int p_id, bool p_video, StringView p_url) {

    Preview preview;
    preview.id = p_id;
    preview.video_link = p_url;
    preview.is_video = p_video;
    preview.button = memnew(Button);
    preview.button->set_flat(true);
    preview.button->set_button_icon(get_theme_icon("ThumbnailWait", "EditorIcons"));
    preview.button->set_toggle_mode(true);
    preview.button->connectF("pressed",this,[this,p_id] { _preview_click(p_id); });
    preview_hb->add_child(preview.button);
    if (!p_video) {
        preview.image = get_theme_icon("ThumbnailWait", "EditorIcons");
    }
    preview_images.push_back(preview);
    if (preview_images.size() == 1 && !p_video) {
        _preview_click(p_id);
    }
}

EditorAssetLibraryItemDescription::EditorAssetLibraryItemDescription() {


    HBoxContainer *hbox = memnew(HBoxContainer);
    add_child(hbox);
    VBoxContainer *desc_vbox = memnew(VBoxContainer);
    hbox->add_child(desc_vbox);
    hbox->add_constant_override("separation", 15 * EDSCALE);

    item = memnew(EditorAssetLibraryItem);

    desc_vbox->add_child(item);
    desc_vbox->set_custom_minimum_size(Size2(440 * EDSCALE, 0));

    description = memnew(RichTextLabel);
    desc_vbox->add_child(description);
    description->set_v_size_flags(SIZE_EXPAND_FILL);
    description->connect("meta_clicked",callable_mp(this, &ClassName::_link_click));
    description->add_constant_override("line_separation", Math::round(5 * EDSCALE));

    VBoxContainer *previews_vbox = memnew(VBoxContainer);
    hbox->add_child(previews_vbox);
    previews_vbox->add_constant_override("separation", 15 * EDSCALE);
    previews_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
    previews_vbox->set_h_size_flags(SIZE_EXPAND_FILL);

    preview = memnew(TextureRect);
    previews_vbox->add_child(preview);
    preview->set_expand(true);
    preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
    preview->set_custom_minimum_size(Size2(640 * EDSCALE, 345 * EDSCALE));
    preview->set_v_size_flags(SIZE_EXPAND_FILL);
    preview->set_h_size_flags(SIZE_EXPAND_FILL);

    previews_bg = memnew(PanelContainer);
    previews_vbox->add_child(previews_bg);
    previews_bg->set_custom_minimum_size(Size2(640 * EDSCALE, 101 * EDSCALE));

    previews = memnew(ScrollContainer);
    previews_bg->add_child(previews);
    previews->set_enable_v_scroll(false);
    previews->set_enable_h_scroll(true);
    preview_hb = memnew(HBoxContainer);
    preview_hb->set_v_size_flags(SIZE_EXPAND_FILL);

    previews->add_child(preview_hb);
    get_ok()->set_text(TTR("Download"));
    get_cancel()->set_text(TTR("Close"));
}
///////////////////////////////////////////////////////////////////////////////////

void EditorAssetLibraryItemDownload::_http_download_completed(int p_status, int p_code, const PoolStringArray &headers, const PoolByteArray &p_data) {

    String error_text;
    TmpString<256,true> tmp(" " + host);
    switch (p_status) {

        case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH:
        case HTTPRequest::RESULT_CONNECTION_ERROR:
        case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED: {
            error_text = TTR("Connection error, please try again.");
            status->set_text(TTR("Can't connect."));
        } break;
        case HTTPRequest::RESULT_CANT_CONNECT:
        case HTTPRequest::RESULT_SSL_HANDSHAKE_ERROR: {
            error_text = TTR("Can't connect to host:") + tmp;
            status->set_text(TTR("Can't connect."));
        } break;
        case HTTPRequest::RESULT_NO_RESPONSE: {
            error_text = TTR("No response from host:") + tmp;
            status->set_text(TTR("No response."));
        } break;
        case HTTPRequest::RESULT_CANT_RESOLVE: {
            error_text = TTR("Can't resolve hostname:") + tmp;
            status->set_text(TTR("Can't resolve."));
        } break;
        case HTTPRequest::RESULT_REQUEST_FAILED: {
            error_text = TTR("Request failed, return code:") + " " + itos(p_code);
            status->set_text(TTR("Request failed."));
        } break;
        case HTTPRequest::RESULT_DOWNLOAD_FILE_CANT_OPEN:
        case HTTPRequest::RESULT_DOWNLOAD_FILE_WRITE_ERROR: {
            error_text = TTR("Cannot save response to:") + " " + download->get_download_file();
            status->set_text(TTR("Write error."));
        } break;
        case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
            error_text = TTR("Request failed, too many redirects");
            status->set_text(TTR("Redirect loop."));
        } break;
        case HTTPRequest::RESULT_TIMEOUT: {
            error_text = TTR("Request failed, timeout");
            status->set_text(TTR("Timeout."));
        } break;
        default: {
            if (p_code != 200) {
                error_text = TTR("Request failed, return code:") + " " + itos(p_code);
                status->set_text(TTR("Failed:") + " " + itos(p_code));
            } else if (!sha256.empty()) {
                String download_sha256 = FileAccess::get_sha256(download->get_download_file());
                if (sha256 != download_sha256) {
                    error_text = TTR("Bad download hash, assuming file has been tampered with.") + "\n";
                    error_text += FormatVE(TTR("Expected: %s\nGot: %s").asCString(),sha256.c_str(),download_sha256.c_str());
                    status->set_text(TTR("Failed SHA-256 hash check"));
                }
            }
        } break;
    }

    if (!error_text.empty()) {
        download_error->set_text(TTR("Asset Download Error:") + "\n" + error_text);
        download_error->popup_centered_minsize();
        // Let the user retry the download.
        retry->show();
        return;
    }

    install->set_disabled(false);
    status->set_text(TTR("Success!"));
    // Make the progress bar invisible but don't reflow other Controls around it.
    progress->set_modulate(Color(0, 0, 0, 0));

    set_process(false);
    // Automatically prompt for installation once the download is completed.
    _install();
}

void EditorAssetLibraryItemDownload::configure(const StringName &p_title, int p_asset_id, const Ref<Texture> &p_preview, StringView p_download_url, StringView p_sha256_hash) {

    title->set_text(p_title);
    icon->set_texture(p_preview);
    asset_id = p_asset_id;
    if (not p_preview)
        icon->set_texture(get_theme_icon("FileBrokenBigThumb", "EditorIcons"));
    host = p_download_url;
    sha256 = p_sha256_hash;
    _make_request();
}

void EditorAssetLibraryItemDownload::_notification(int p_what) {

    switch (p_what) {

        // FIXME: The editor crashes if 'NOTICATION_THEME_CHANGED' is used.
        case NOTIFICATION_ENTER_TREE: {

            add_theme_style_override("panel", get_theme_stylebox("panel", "TabContainer"));
            dismiss->set_normal_texture(get_theme_icon("Close", "EditorIcons"));
        } break;
        case NOTIFICATION_PROCESS: {

            // Make the progress bar visible again when retrying the download.
            progress->set_modulate(Color(1, 1, 1, 1));

            if (download->get_downloaded_bytes() > 0) {
                progress->set_max(download->get_body_size());
                progress->set_value(download->get_downloaded_bytes());
            }

            int cstatus = download->get_http_client_status();

            if (cstatus == HTTPClient::STATUS_BODY) {
                if (download->get_body_size() > 0) {
                    status->set_text(StringName(FormatVE( TTR("Downloading (%s / %s)...").asCString(),
                            PathUtils::humanize_size(download->get_downloaded_bytes()).c_str(),
                            PathUtils::humanize_size(download->get_body_size()).c_str())));
                } else {
                    // Total file size is unknown, so it cannot be displayed.
                    progress->set_modulate(Color(0, 0, 0, 0));
                    status->set_text(FormatSN(
                            (String(TTR("Downloading...")) + " (%s)").c_str(),
                            PathUtils::humanize_size(download->get_downloaded_bytes()).c_str()));

                    status->set_text(TTR("Downloading..."));
                }
            }

            if (cstatus != prev_status) {
                switch (cstatus) {

                    case HTTPClient::STATUS_RESOLVING: {
                        status->set_text(TTR("Resolving..."));
                        progress->set_max(1);
                        progress->set_value(0);
                    } break;
                    case HTTPClient::STATUS_CONNECTING: {
                        status->set_text(TTR("Connecting..."));
                        progress->set_max(1);
                        progress->set_value(0);
                    } break;
                    case HTTPClient::STATUS_REQUESTING: {
                        status->set_text(TTR("Requesting..."));
                        progress->set_max(1);
                        progress->set_value(0);
                    } break;
                    default: {
                    }
                }
                prev_status = cstatus;
            }
        } break;
    }
}
void EditorAssetLibraryItemDownload::_close() {

    // Clean up downloaded file.
    DirAccess::remove_file_or_error(download->get_download_file());
    queue_delete();
}

void EditorAssetLibraryItemDownload::_install() {

    const String &file(download->get_download_file());

    if (external_install) {
        emit_signal("install_asset", file, title->get_text());
        return;
    }

    asset_installer->set_asset_name(title->get_text());
    asset_installer->open(file, 1);
}

void EditorAssetLibraryItemDownload::_make_request() {
    // Hide the Retry button if we've just pressed it.
    retry->hide();

    download->cancel_request();
    download->set_download_file(PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),"tmp_asset_" + itos(asset_id)) + ".zip");

    Error err = download->request(host);
    if (err != OK) {
        status->set_text(TTR("Error making request"));
    } else {
        set_process(true);
    }
}

void EditorAssetLibraryItemDownload::_bind_methods() {

    MethodBinder::bind_method("_http_download_completed", &EditorAssetLibraryItemDownload::_http_download_completed);
    MethodBinder::bind_method("_install", &EditorAssetLibraryItemDownload::_install);
    MethodBinder::bind_method("_close", &EditorAssetLibraryItemDownload::_close);
    MethodBinder::bind_method("_make_request", &EditorAssetLibraryItemDownload::_make_request);

    ADD_SIGNAL(MethodInfo("install_asset", PropertyInfo(VariantType::STRING, "zip_path"), PropertyInfo(VariantType::STRING, "name")));
}

EditorAssetLibraryItemDownload::EditorAssetLibraryItemDownload() {

    HBoxContainer *hb = memnew(HBoxContainer);
    add_child(hb);
    icon = memnew(TextureRect);
    hb->add_child(icon);

    VBoxContainer *vb = memnew(VBoxContainer);
    hb->add_child(vb);
    vb->set_h_size_flags(SIZE_EXPAND_FILL);

    HBoxContainer *title_hb = memnew(HBoxContainer);
    vb->add_child(title_hb);
    title = memnew(Label);
    title_hb->add_child(title);
    title->set_h_size_flags(SIZE_EXPAND_FILL);

    dismiss = memnew(TextureButton);
    dismiss->connect("pressed",callable_mp(this, &ClassName::_close));
    title_hb->add_child(dismiss);

    title->set_clip_text(true);

    vb->add_spacer();

    status = memnew(Label(TTR("Idle")));
    vb->add_child(status);
    status->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
    progress = memnew(ProgressBar);
    vb->add_child(progress);

    HBoxContainer *hb2 = memnew(HBoxContainer);
    vb->add_child(hb2);
    hb2->add_spacer();

    install = memnew(Button);
    install->set_text(TTR("Install..."));
    install->set_disabled(true);
    install->connect("pressed",callable_mp(this, &ClassName::_install));

    retry = memnew(Button);
    retry->set_text(TTR("Retry"));
    retry->connectF("pressed",this, [=]() { _make_request(); });
    // Only show the Retry button in case of a failure.
    retry->hide();

    hb2->add_child(retry);
    hb2->add_child(install);
    set_custom_minimum_size(Size2(310, 0) * EDSCALE);

    download = memnew(HTTPRequest);
    add_child(download);
    download->connect("request_completed",callable_mp(this, &ClassName::_http_download_completed));
    setup_http_request(download);

    download_error = memnew(AcceptDialog);
    add_child(download_error);
    download_error->set_title(TTR("Download Error"));

    asset_installer = memnew(EditorAssetInstaller);
    add_child(asset_installer);
    asset_installer->connect("confirmed",callable_mp(this, &ClassName::_close));

    prev_status = -1;

    external_install = false;
}

////////////////////////////////////////////////////////////////////////////////
void EditorAssetLibrary::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            error_tr->set_texture(get_theme_icon("Error", "EditorIcons"));
            filter->set_right_icon(get_theme_icon("Search", "EditorIcons"));
            filter->set_clear_button_enabled(true);

            error_label->raise();
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {
            if (is_visible()) {
                // Focus the search box automatically when switching to the Templates tab (in the Project Manager)
                // or switching to the AssetLib tab (in the editor).
                // The Project Manager's project filter box is automatically focused in the project manager code.
                filter->grab_focus();

                if (initial_loading) {
                    _repository_changed(0); // Update when shown for the first time.
                }
            }
        } break;
        case NOTIFICATION_PROCESS: {
            HTTPClient::Status s = request->get_http_client_status();
            const bool loading = s != HTTPClient::STATUS_DISCONNECTED;

            if (loading) {
                library_scroll->set_modulate(Color(1, 1, 1, 0.5));
            } else {
                library_scroll->set_modulate(Color(1, 1, 1, 1));
            }

            const bool no_downloads = downloads_hb->get_child_count() == 0;
            if (no_downloads == downloads_scroll->is_visible()) {
                downloads_scroll->set_visible(!no_downloads);
            }

        } break;
        case NOTIFICATION_THEME_CHANGED: {
            library_scroll_bg->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
            downloads_scroll->add_theme_style_override("bg", get_theme_stylebox("bg", "Tree"));
            error_tr->set_texture(get_theme_icon("Error", "EditorIcons"));
            filter->set_right_icon(get_theme_icon("Search", "EditorIcons"));
            filter->set_clear_button_enabled(true);
        } break;
        case NOTIFICATION_RESIZED: {
            _update_asset_items_columns();
        } break;
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
            _update_repository_options();
            setup_http_request(request);
        } break;
    }
}

void EditorAssetLibrary::_install_asset() {

    ERR_FAIL_COND(!description);

    for (int i = 0; i < downloads_hb->get_child_count(); i++) {

        EditorAssetLibraryItemDownload *d = object_cast<EditorAssetLibraryItemDownload>(downloads_hb->get_child(i));
        if (d && d->get_asset_id() == description->get_asset_id()) {

            if (EditorNode::get_singleton() != nullptr)
                EditorNode::get_singleton()->show_warning(TTR("Download for this asset is already in progress!"));
            return;
        }
    }

    EditorAssetLibraryItemDownload *download = memnew(EditorAssetLibraryItemDownload);
    downloads_hb->add_child(download);
    download->configure(description->get_title(), description->get_asset_id(), description->get_preview_icon(), description->get_download_url(), description->get_sha256());

    if (templates_only) {
        download->set_external_install(true);
        download->connect("install_asset",callable_mp(this, &ClassName::_install_external_asset));
    }
}

const char *EditorAssetLibrary::sort_key[SORT_MAX] = {
    "updated",
    "updated",
    "name",
    "name",
    "cost",
    "cost",
};

const char *EditorAssetLibrary::sort_text[SORT_MAX] = {
    TTRC("Recently Updated"),
    TTRC("Least Recently Updated"),
    TTRC("Name (A-Z)"),
    TTRC("Name (Z-A)"),
    TTRC("License (A-Z)"), // "cost" stores the SPDX license name in the Godot Asset Library.
    TTRC("License (Z-A)"), // "cost" stores the SPDX license name in the Godot Asset Library.
};

const char *EditorAssetLibrary::support_key[SUPPORT_MAX] = {
    "official",
    "community",
    "testing",
};

void EditorAssetLibrary::_select_author(int p_id) {

    // Open author window.
}

void EditorAssetLibrary::_select_category(int p_id) {

    for (int i = 0; i < categories->get_item_count(); i++) {

        if (i == 0)
            continue;
        int id = categories->get_item_metadata(i).as<int>();
        if (id == p_id) {
            categories->select(i);
            _search();
            break;
        }
    }
}
void EditorAssetLibrary::_select_asset(int p_id) {

    _api_request("asset/" + itos(p_id), REQUESTING_ASSET);
}

void EditorAssetLibrary::_image_update(bool use_cache, bool final, const PoolByteArray &p_data, int p_queue_id) {
    Object *obj = object_for_entity(image_queue[p_queue_id].target);

    if (obj) {
        bool image_set = false;
        PoolByteArray image_data = p_data;

        if (use_cache) {
            String cache_filename_base = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),
                    String("assetimage_" + StringUtils::md5_text(image_queue[p_queue_id].image_url)));

            FileAccess *file = FileAccess::open(cache_filename_base + ".data", FileAccess::READ);

            if (file) {
                PoolByteArray cached_data;
                int len = file->get_32();
                cached_data.resize(len);

                PoolByteArray::Write w = cached_data.write();
                file->get_buffer(w.ptr(), len);

                image_data = cached_data;
                file->close();
                memdelete(file);
            }
        }

        int len = image_data.size();
        PoolByteArray::Read r = image_data.read();
        Ref<Image> image(make_ref_counted<Image>());

        uint8_t png_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
        uint8_t jpg_signature[3] = { 255, 216, 255 };

        if (r.ptr()) {
            if (memcmp(&r[0], &png_signature[0], 8) == 0) {
                image->create(ImageLoader::load_image("png",r.ptr(), len));
            } else if (memcmp(&r[0], &jpg_signature[0], 3) == 0) {
                image->create(ImageLoader::load_image("jpg",r.ptr(), len));
            }
        }

        if (!image->is_empty()) {
            switch (image_queue[p_queue_id].image_type) {
                case IMAGE_QUEUE_ICON:

                    image->resize(64 * EDSCALE, 64 * EDSCALE, Image::INTERPOLATE_LANCZOS);

                    break;
                case IMAGE_QUEUE_THUMBNAIL: {
                    float max_height = 85 * EDSCALE;

                    float scale_ratio = max_height / (image->get_height() * EDSCALE);
                    if (scale_ratio < 1) {
                        image->resize(image->get_width() * EDSCALE * scale_ratio, image->get_height() * EDSCALE * scale_ratio, Image::INTERPOLATE_LANCZOS);
                    }
                } break;
                case IMAGE_QUEUE_SCREENSHOT: {
                    float max_height = 397 * EDSCALE;

                    float scale_ratio = max_height / (image->get_height() * EDSCALE);
                    if (scale_ratio < 1) {
                        image->resize(image->get_width() * EDSCALE * scale_ratio, image->get_height() * EDSCALE * scale_ratio, Image::INTERPOLATE_LANCZOS);
                    }
                } break;
            }

            Ref<ImageTexture> tex(make_ref_counted<ImageTexture>());
            tex->create_from_image(image);

            obj->call_va("set_image", image_queue[p_queue_id].image_type, image_queue[p_queue_id].image_index, tex);
            image_set = true;
        }

        if (!image_set && final) {
            obj->call_va("set_image", image_queue[p_queue_id].image_type, image_queue[p_queue_id].image_index, get_theme_icon("FileBrokenBigThumb", "EditorIcons"));
        }
    }
}

void EditorAssetLibrary::_image_request_completed(
        int p_status, int p_code, const PoolVector<String> &headers, const PoolByteArray &p_data, int p_queue_id) {

    ERR_FAIL_COND(!image_queue.contains(p_queue_id));

    if (p_status == HTTPRequest::RESULT_SUCCESS && p_code < HTTPClient::RESPONSE_BAD_REQUEST) {

        if (p_code != HTTPClient::RESPONSE_NOT_MODIFIED) {
            for (int i = 0; i < headers.size(); i++) {
                StringView hdr(headers[i]);
                if (StringUtils::findn(hdr, "ETag:") == 0) { // Save etag
                    String cache_filename_base = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),
                            String("assetimage_" + StringUtils::md5_text(image_queue[p_queue_id].image_url)));
                    StringView new_etag = StringUtils::strip_edges(StringUtils::substr(
                            hdr, StringUtils::find(hdr, ":") + 1));
                    FileAccessRef<true> file(FileAccess::open(cache_filename_base + ".etag", FileAccess::WRITE));
                    if (file) {
                        file->store_line(new_etag);
                    }

                    int len = p_data.size();
                    PoolByteArray::Read r = p_data.read();
                    file = FileAccess::open(cache_filename_base + ".data", FileAccess::WRITE);
                    if (file) {
                        file->store_32(len);
                        file->store_buffer(r.ptr(), len);
                    }

                    break;
                }
            }
        }
        _image_update(p_code == HTTPClient::RESPONSE_NOT_MODIFIED, true, p_data, p_queue_id);

    } else {
        WARN_PRINT("Error getting image file from URL: " + image_queue[p_queue_id].image_url);
        Object *obj = object_for_entity(image_queue[p_queue_id].target);
        if (obj) {
            obj->call_va("set_image", image_queue[p_queue_id].image_type, image_queue[p_queue_id].image_index, get_theme_icon("FileBrokenBigThumb", "EditorIcons"));
        }
    }

    image_queue[p_queue_id].request->queue_delete();
    image_queue.erase(p_queue_id);

    _update_image_queue();
}

void EditorAssetLibrary::_update_image_queue() {

    const int max_images = 6;
    int current_images = 0;

    Vector<int> to_delete;
    for (eastl::pair<const int, ImageQueue> &E : image_queue) {
        if (!E.second.active && current_images < max_images) {

            String cache_filename_base = PathUtils::plus_file(EditorSettings::get_singleton()->get_cache_dir(),
                    "assetimage_" + StringUtils::md5_text(E.second.image_url));
            Vector<String> headers;

            if (FileAccess::exists(cache_filename_base + ".etag") && FileAccess::exists(cache_filename_base + ".data")) {
                FileAccess *file = FileAccess::open(cache_filename_base + ".etag", FileAccess::READ);
                if (file) {
                    headers.push_back("If-None-Match: " + file->get_line());
                    file->close();
                    memdelete(file);
                }
            }

            Error err = E.second.request->request(E.second.image_url, headers);
            if (err != OK) {
                to_delete.push_back(E.first);
            } else {
                E.second.active = true;
            }
            current_images++;
        } else if (E.second.active) {
            current_images++;
        }
    }

    for(int d : to_delete) {
        image_queue[d].request->queue_delete();
        image_queue.erase(d);
    }
}

void EditorAssetLibrary::_request_image(GameEntity p_for, String p_image_url, ImageType p_type, int p_image_index) {

    ImageQueue iq;
    iq.image_url = eastl::move(p_image_url);
    iq.image_index = p_image_index;
    iq.image_type = p_type;
    iq.request = memnew(HTTPRequest);
    setup_http_request(iq.request);

    iq.target = p_for;
    iq.queue_id = ++last_queue_id;
    iq.active = false;
    auto lambda=[this, qid = iq.queue_id](int p_status, int p_code, const PoolStringArray &headers,
            const PoolByteArray &p_data) { _image_request_completed(p_status,p_code,headers,p_data,qid); };
    iq.request->connect("request_completed", callable_gen(this,lambda));

    image_queue[iq.queue_id] = iq;

    add_child(iq.request);

    _image_update(true, false, PoolByteArray(), iq.queue_id);
    _update_image_queue();
}

void EditorAssetLibrary::_repository_changed(int p_repository_id) {
    library_error->hide();
    library_info->set_text(TTR("Loading..."));
    library_info->show();

    asset_top_page->hide();
    asset_bottom_page->hide();
    asset_items->hide();

    filter->set_editable(false);
    sort->set_disabled(true);
    categories->set_disabled(true);
    support->set_disabled(true);

    host = repository->get_item_metadata(p_repository_id).as<String>();
    if (templates_only) {
        _api_request("configure", REQUESTING_CONFIG, "?type=project");
    } else {
        _api_request("configure", REQUESTING_CONFIG);
    }
}

void EditorAssetLibrary::_support_toggled(int p_support) {
    support->get_popup()->set_item_checked(p_support, !support->get_popup()->is_item_checked(p_support));
    _search();
}

void EditorAssetLibrary::_rerun_search(int p_ignore) {
    _search();
}

void EditorAssetLibrary::_search(int p_page) {

    String args;

    if (templates_only) {
        args += "?type=project&";
    } else {
        args += "?";
    }
    args += String("sort=") + sort_key[sort->get_selected()];

    // We use the "branch" version, i.e. major.minor, as patch releases should be compatible
    args += "&godot_version=" + String(VERSION_BRANCH);

    String support_list;
    for (int i = 0; i < SUPPORT_MAX; i++) {
        if (support->get_popup()->is_item_checked(i)) {
            support_list += String(support_key[i]) + "+";
        }
    }
    if (!support_list.empty()) {
        args += String("&support=") + StringUtils::substr(support_list,0, support_list.length() - 1);
    }

    if (categories->get_selected() > 0) {

        args += "&category=" + itos(categories->get_item_metadata(categories->get_selected()).as<int>());
    }

    // Sorting options with an odd index are always the reverse of the previous one
    if (sort->get_selected() % 2 == 1) {
        args += "&reverse=true";
    }

    if (!filter->get_text_ui().isEmpty()) {
        args += String("&filter=") + StringUtils::http_escape(filter->get_text());
    }

    if (p_page > 0) {
        args += "&page=" + itos(p_page);
    }

    _api_request("asset", REQUESTING_SEARCH, args);
}

void EditorAssetLibrary::_search_text_changed(StringView p_text) {
    filter_debounce_timer->start();
}

void EditorAssetLibrary::_filter_debounce_timer_timeout() {
    _search();
}

void EditorAssetLibrary::_request_current_config() {
    _repository_changed(repository->get_selected());
}


HBoxContainer *EditorAssetLibrary::_make_pages(int p_page, int p_page_count, int p_page_len, int p_total_items, int p_current_items) {

    HBoxContainer *hbc = memnew(HBoxContainer);

    if (p_page_count < 2) {
        return hbc;
    }

    //do the mario
    int from = eastl::max(0,p_page - 5);
    int to = eastl::min(from + 10,p_page_count);

    hbc->add_spacer();
    hbc->add_constant_override("separation", 5 * EDSCALE);

    Button *first = memnew(Button);
    first->set_text(TTR("First", "Pagination"));
    if (p_page != 0) {
        first->connectF("pressed",this, [this]() { _search(0); });
    } else {
        first->set_disabled(true);
        first->set_focus_mode(Control::FOCUS_NONE);
    }
    hbc->add_child(first);

    Button *prev = memnew(Button);
    prev->set_text(TTR("Previous", "Pagination"));
    if (p_page > 0) {
        prev->connectF("pressed",this, [this,p_page]() { _search(p_page-1); });
    } else {
        prev->set_disabled(true);
        prev->set_focus_mode(Control::FOCUS_NONE);
    }
    hbc->add_child(prev);
    hbc->add_child(memnew(VSeparator));

    for (int i = from; i < to; i++) {

        if (i == p_page) {

            Button *current = memnew(Button);
            // Keep the extended padding for the currently active page (see below).
            current->set_text(FormatVE(" %d ", i + 1));
            current->set_disabled(true);
            current->set_focus_mode(Control::FOCUS_NONE);

            hbc->add_child(current);
        } else {

            Button *current = memnew(Button);
            // Add padding to make page number buttons easier to click.
            current->set_text(FormatVE(" %d ", i + 1));
            current->connectF("pressed",this, [this,i]() { _search(i); });

            hbc->add_child(current);
        }
    }

    Button *next = memnew(Button);
    next->set_text(TTR("Next", "Pagination"));
    if (p_page < p_page_count - 1) {
        next->connectF("pressed",this, [this,p_page]() { _search(p_page+1); });
    } else {
        next->set_disabled(true);
        next->set_focus_mode(Control::FOCUS_NONE);
    }
    hbc->add_child(memnew(VSeparator));
    hbc->add_child(next);

    Button *last = memnew(Button);
    last->set_text(TTR("Last", "Pagination"));
    if (p_page != p_page_count - 1) {
        last->connectF("pressed",this, [this,p_page_count]() { _search(p_page_count-1); });
    } else {
        last->set_disabled(true);
        last->set_focus_mode(Control::FOCUS_NONE);
    }
    hbc->add_child(last);

    hbc->add_spacer();

    return hbc;
}

void EditorAssetLibrary::_api_request(StringView p_request, RequestType p_request_type, StringView p_arguments) {

    if (requesting != REQUESTING_NONE) {
        request->cancel_request();
    }

    requesting = p_request_type;

    error_hb->hide();
    request->request(host + "/" + p_request + p_arguments);
}

void EditorAssetLibrary::_http_request_completed(int p_status, int p_code, const PoolStringArray &headers, const PoolByteArray &p_data) {

    String str;

    {
        int datalen = p_data.size();
        PoolByteArray::Read r = p_data.read();
        str.assign((const char *)r.ptr(), datalen);
    }

    bool error_abort = true;
    String ui_host_suffix(" " + host);
    switch (p_status) {

        case HTTPRequest::RESULT_CANT_RESOLVE: {
            error_label->set_text(TTR("Can't resolve hostname:") + StringView(ui_host_suffix));
        } break;
        case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
        case HTTPRequest::RESULT_CONNECTION_ERROR:
        case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH: {
            error_label->set_text(TTR("Connection error, please try again."));
        } break;
        case HTTPRequest::RESULT_SSL_HANDSHAKE_ERROR:
        case HTTPRequest::RESULT_CANT_CONNECT: {
            error_label->set_text(TTR("Can't connect to host:") + StringView(ui_host_suffix));
        } break;
        case HTTPRequest::RESULT_NO_RESPONSE: {
            error_label->set_text(TTR("No response from host:") + StringView(ui_host_suffix));
        } break;
        case HTTPRequest::RESULT_REQUEST_FAILED: {
            error_label->set_text(TTR("Request failed, return code:") + StringView(" " + itos(p_code)));
        } break;
        case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
            error_label->set_text(TTR("Request failed, too many redirects"));

        } break;
        default: {
            if (p_code != 200) {
                error_label->set_text(TTR("Request failed, return code:") + StringView(" " + itos(p_code)));
            } else {

                error_abort = false;
            }
        } break;
    }

    if (error_abort) {
        if (requesting == REQUESTING_CONFIG) {
            library_info->hide();
            library_error->show();
        }
        error_hb->show();
        return;
    }

    Dictionary d;
    {
        Variant js;
        String errs;
        int errl;
        JSON::parse(str, js, errs, errl);
        d = js.as<Dictionary>();
    }

    RequestType requested = requesting;
    requesting = REQUESTING_NONE;

    switch (requested) {
        case REQUESTING_CONFIG: {

            categories->clear();
            categories->add_item(TTR("All"));
            categories->set_item_metadata(0, 0);
            if (d.has("categories")) {
                Array clist = d["categories"].as<Array>();
                for (int i = 0; i < clist.size(); i++) {
                    Dictionary cat = clist[i].as<Dictionary>();
                    if (!cat.has("name") || !cat.has("id"))
                        continue;
                    StringName name = cat["name"].as<StringName>();
                    int id = cat["id"].as<int>();
                    categories->add_item(name);
                    categories->set_item_metadata(categories->get_item_count() - 1, id);
                    category_map[id] = name;
                }
            }
            filter->set_editable(true);
            sort->set_disabled(false);
            categories->set_disabled(false);
            support->set_disabled(false);

            _search();
        } break;
        case REQUESTING_SEARCH: {

            initial_loading = false;

            memdelete(asset_items);
            memdelete(asset_top_page);
            memdelete(asset_bottom_page);

            int page = 0;
            int pages = 1;
            int page_len = 10;
            int total_items = 1;
            Array result;

            if (d.has("page")) {
                page = d["page"].as<int>();
            }
            if (d.has("pages")) {
                pages = d["pages"].as<int>();
            }
            if (d.has("page_length")) {
                page_len = d["page_length"].as<int>();
            }
            if (d.has("total")) {
                total_items = d["total"].as<int>();
            }
            if (d.has("result")) {
                result = d["result"].as<Array>();
            }

            asset_top_page = _make_pages(page, pages, page_len, total_items, result.size());
            library_vb->add_child(asset_top_page);

            asset_items = memnew(GridContainer);
            _update_asset_items_columns();
            asset_items->add_constant_override("hseparation", 10 * EDSCALE);
            asset_items->add_constant_override("vseparation", 10 * EDSCALE);

            library_vb->add_child(asset_items);

            asset_bottom_page = _make_pages(page, pages, page_len, total_items, result.size());
            library_vb->add_child(asset_bottom_page);

            if (result.empty()) {
                library_info->set_text(FormatSN(TTR("No results for \"%s\".").asCString(), filter->get_text().c_str()));
                library_info->show();
            }
            else {
                library_info->hide();
            }

            for (int i = 0; i < result.size(); i++) {

                Dictionary r = result[i].as<Dictionary>();

                ERR_CONTINUE(!r.has("title"));
                ERR_CONTINUE(!r.has("asset_id"));
                ERR_CONTINUE(!r.has("author"));
                ERR_CONTINUE(!r.has("author_id"));
                ERR_CONTINUE(!r.has("category_id"));
                ERR_FAIL_COND(!category_map.contains(r["category_id"].as<int>()));
                ERR_CONTINUE(!r.has("cost"));

                EditorAssetLibraryItem *item = memnew(EditorAssetLibraryItem);
                asset_items->add_child(item);
                item->configure(r["title"].as<StringName>(), r["asset_id"].as<int>(), category_map[r["category_id"].as<int>()].as<String>(), r["category_id"].as<int>(), r["author"].as<String>(), r["author_id"].as<int>(), r["cost"].as<String>());
                item->connect("asset_selected",callable_mp(this, &ClassName::_select_asset));
                item->connect("author_selected",callable_mp(this, &ClassName::_select_author));
                item->connect("category_selected",callable_mp(this, &ClassName::_select_category));

                if (r.has("icon_url") && r["icon_url"] != "") {
                    _request_image(item->get_instance_id(), r["icon_url"].as<String>(), IMAGE_QUEUE_ICON, 0);
                }
            }
            if (!result.empty()) {
                library_scroll->set_v_scroll(0);
            }
        } break;
        case REQUESTING_ASSET: {
            Dictionary r = d;

            ERR_FAIL_COND(!r.has("title"));
            ERR_FAIL_COND(!r.has("asset_id"));
            ERR_FAIL_COND(!r.has("author"));
            ERR_FAIL_COND(!r.has("author_id"));
            ERR_FAIL_COND(!r.has("version"));
            ERR_FAIL_COND(!r.has("version_string"));
            ERR_FAIL_COND(!r.has("category_id"));
            ERR_FAIL_COND(!category_map.contains(r["category_id"].as<int>()));
            ERR_FAIL_COND(!r.has("cost"));
            ERR_FAIL_COND(!r.has("description"));
            ERR_FAIL_COND(!r.has("download_url"));
            ERR_FAIL_COND(!r.has("download_hash"));
            ERR_FAIL_COND(!r.has("browse_url"));

            memdelete(description);

            description = memnew(EditorAssetLibraryItemDescription);
            add_child(description);
            description->popup_centered_minsize();
            description->connect("confirmed",callable_mp(this, &ClassName::_install_asset));

            description->configure(r["title"].as<StringName>(), r["asset_id"].as<int>(), category_map[r["category_id"].as<int>()].as<String>(),
                    r["category_id"].as<int>(), r["author"].as<String>(), r["author_id"].as<int>(), r["cost"].as<String>(), r["version"].as<int>(),
                    r["version_string"].as<String>(), r["description"].as<String>(),
                    r["download_url"].as<String>(), r["browse_url"].as<String>(),
                    r["download_hash"].as<String>());

            if (r.has("icon_url") && r["icon_url"] != "") {
                _request_image(description->get_instance_id(), r["icon_url"].as<String>(), IMAGE_QUEUE_ICON, 0);
            }

            if (d.has("previews")) {
                Array previews = d["previews"].as<Array>();

                for (int i = 0; i < previews.size(); i++) {

                    Dictionary p = previews[i].as<Dictionary>();

                    ERR_CONTINUE(!p.has("type"));
                    ERR_CONTINUE(!p.has("link"));

                    bool is_video = p.has("type") && p["type"].as<String>() == "video";
                    String video_url;
                    if (is_video && p.has("link")) {
                        video_url = p["link"].as<String>();
                    }

                    description->add_preview(i, is_video, video_url);

                    if (p.has("thumbnail")) {
                        _request_image(description->get_instance_id(), p["thumbnail"].as<String>(), IMAGE_QUEUE_THUMBNAIL, i);
                    }

                    if (!is_video) {
                        _request_image(description->get_instance_id(), p["link"].as<String>(), IMAGE_QUEUE_SCREENSHOT, i);
                    }
                }
            }
        } break;
        default:
        break;
    }
}

void EditorAssetLibrary::_asset_file_selected(StringView p_file) {

    memdelete(asset_installer);
    asset_installer = memnew(EditorAssetInstaller);
    asset_installer->set_asset_name(PathUtils::get_basename(p_file));
    add_child(asset_installer);
    asset_installer->open(p_file);
}

void EditorAssetLibrary::_asset_open() {

    asset_open->popup_centered_ratio();
}

void EditorAssetLibrary::_manage_plugins() {

    ProjectSettingsEditor::get_singleton()->popup_project_settings();
    ProjectSettingsEditor::get_singleton()->set_plugins_page();
}

void EditorAssetLibrary::_install_external_asset(StringView p_zip_path, StringView p_title) {

    emit_signal("install_asset", p_zip_path, p_title);
}

void EditorAssetLibrary::_update_asset_items_columns() {
    int new_columns = get_size().x / (450.0 * EDSCALE);
    new_columns = M_MAX(1, new_columns);

    if (new_columns != asset_items->get_columns()) {
        asset_items->set_columns(new_columns);
    }
}


void EditorAssetLibrary::disable_community_support() {
    support->get_popup()->set_item_checked(SUPPORT_COMMUNITY, false);
}

void EditorAssetLibrary::_bind_methods() {
    ADD_SIGNAL(MethodInfo("install_asset", PropertyInfo(VariantType::STRING, "zip_path"), PropertyInfo(VariantType::STRING, "name")));
}

void EditorAssetLibrary::_update_repository_options()
{
    Dictionary default_urls;
    default_urls["godotengine.org"] = "https://godotengine.org/asset-library/api";
    default_urls["localhost"] = "http://127.0.0.1/asset-library/api";
    Dictionary available_urls = _EDITOR_DEF("asset_library/available_urls", default_urls, true).as<Dictionary>();
    auto keys = available_urls.get_key_list();
    for (int i = 0; i < available_urls.size(); i++) {
        auto key = keys[i];
        repository->add_item(key);
        repository->set_item_metadata(i, available_urls[key]);
    }
}

EditorAssetLibrary::EditorAssetLibrary(bool p_templates_only) {

    requesting = REQUESTING_NONE;
    templates_only = p_templates_only;
    initial_loading = true;

    VBoxContainer *library_main = memnew(VBoxContainer);

    add_child(library_main);

    HBoxContainer *search_hb = memnew(HBoxContainer);

    library_main->add_child(search_hb);
    library_main->add_constant_override("separation", 10 * EDSCALE);

    filter = memnew(LineEdit);
    if (templates_only) {
        filter->set_placeholder(TTR("Search templates, projects, and demos"));
    } else {
        filter->set_placeholder(TTR("Search assets (excluding templates, projects, and demos)"));
    }
    search_hb->add_child(filter);
    filter->set_h_size_flags(SIZE_EXPAND_FILL);
    filter->connect("text_entered",callable_mp(this, &ClassName::_search_text_changed));

    // Perform a search automatically if the user hasn't entered any text for a certain duration.
    // This way, the user doesn't need to press Enter to initiate their search.
    filter_debounce_timer = memnew(Timer);
    filter_debounce_timer->set_one_shot(true);
    filter_debounce_timer->set_wait_time(0.25f);
    filter_debounce_timer->connect("timeout", callable_mp(this, &EditorAssetLibrary::_filter_debounce_timer_timeout));
    search_hb->add_child(filter_debounce_timer);

    if (!p_templates_only)
        search_hb->add_child(memnew(VSeparator));

    Button *open_asset = memnew(Button);
    open_asset->set_text(TTR("Import..."));
    search_hb->add_child(open_asset);
    open_asset->connect("pressed",callable_mp(this, &ClassName::_asset_open));

    Button *plugins = memnew(Button);
    plugins->set_text(TTR("Plugins..."));
    search_hb->add_child(plugins);
    plugins->connect("pressed",callable_mp(this, &ClassName::_manage_plugins));

    if (p_templates_only) {
        open_asset->hide();
        plugins->hide();
    }

    HBoxContainer *search_hb2 = memnew(HBoxContainer);
    library_main->add_child(search_hb2);

    search_hb2->add_child(memnew(Label(TTR("Sort:") + " ")));
    sort = memnew(OptionButton);
    for (int i = 0; i < SORT_MAX; i++) {
        sort->add_item(StringName(TTRGET(sort_text[i])));
    }

    search_hb2->add_child(sort);

    sort->set_h_size_flags(SIZE_EXPAND_FILL);
    sort->set_clip_text(true);
    sort->connect("item_selected",callable_mp(this, &ClassName::_rerun_search));

    search_hb2->add_child(memnew(VSeparator));

    search_hb2->add_child(memnew(Label(TTR("Category:") + " ")));
    categories = memnew(OptionButton);
    categories->add_item(TTR("All"));
    search_hb2->add_child(categories);
    categories->set_h_size_flags(SIZE_EXPAND_FILL);
    categories->set_clip_text(true);
    categories->connect("item_selected",callable_mp(this, &ClassName::_rerun_search));

    search_hb2->add_child(memnew(VSeparator));

    search_hb2->add_child(memnew(Label(TTR("Site:") + " ")));
    repository = memnew(OptionButton);

    _update_repository_options();

    repository->connect("item_selected",callable_mp(this, &ClassName::_repository_changed));

    search_hb2->add_child(repository);
    repository->set_h_size_flags(SIZE_EXPAND_FILL);
    repository->set_clip_text(true);

    search_hb2->add_child(memnew(VSeparator));

    support = memnew(MenuButton);
    search_hb2->add_child(support);
    support->set_text(TTR("Support"));
    support->get_popup()->set_hide_on_checkable_item_selection(false);
    support->get_popup()->add_check_item(TTR("Official"), SUPPORT_OFFICIAL);
    support->get_popup()->add_check_item(TTR("Community"), SUPPORT_COMMUNITY);
    support->get_popup()->add_check_item(TTR("Testing"), SUPPORT_TESTING);
    support->get_popup()->set_item_checked(SUPPORT_OFFICIAL, true);
    support->get_popup()->set_item_checked(SUPPORT_COMMUNITY, true);
    support->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_support_toggled));

    /////////

    library_scroll_bg = memnew(PanelContainer);
    library_main->add_child(library_scroll_bg);
    library_scroll_bg->set_v_size_flags(SIZE_EXPAND_FILL);

    library_scroll = memnew(ScrollContainer);
    library_scroll->set_enable_v_scroll(true);
    library_scroll->set_enable_h_scroll(false);

    library_scroll_bg->add_child(library_scroll);

    Ref<StyleBoxEmpty> border2(make_ref_counted<StyleBoxEmpty>());
    border2->set_default_margin(Margin::Left, 15 * EDSCALE);
    border2->set_default_margin(Margin::Right, 35 * EDSCALE);
    border2->set_default_margin(Margin::Bottom, 15 * EDSCALE);
    border2->set_default_margin(Margin::Top, 15 * EDSCALE);

    PanelContainer *library_vb_border = memnew(PanelContainer);
    library_scroll->add_child(library_vb_border);
    library_vb_border->add_theme_style_override("panel", border2);
    library_vb_border->set_h_size_flags(SIZE_EXPAND_FILL);

    library_vb = memnew(VBoxContainer);
    library_vb->set_h_size_flags(SIZE_EXPAND_FILL);

    library_vb_border->add_child(library_vb);

    library_info = memnew(Label);
    library_info->set_align(Label::ALIGN_CENTER);
    library_vb->add_child(library_info);

    library_error = memnew(VBoxContainer);
    library_error->hide();
    library_vb->add_child(library_error);

    library_error_label = memnew(Label(TTR("Failed to get repository configuration.")));
    library_error_label->set_align(Label::ALIGN_CENTER);
    library_error->add_child(library_error_label);

    library_error_retry = memnew(Button(TTR("Retry")));
    library_error_retry->set_h_size_flags(SIZE_SHRINK_CENTER);
    library_error_retry->connect("pressed", callable_mp(this, &EditorAssetLibrary::_request_current_config));
    library_error->add_child(library_error_retry);

    asset_top_page = memnew(HBoxContainer);
    library_vb->add_child(asset_top_page);

    asset_items = memnew(GridContainer);
    _update_asset_items_columns();
    asset_items->add_constant_override("hseparation", 10 * EDSCALE);
    asset_items->add_constant_override("vseparation", 10 * EDSCALE);

    library_vb->add_child(asset_items);

    asset_bottom_page = memnew(HBoxContainer);
    library_vb->add_child(asset_bottom_page);

    request = memnew(HTTPRequest);
    add_child(request);
    setup_http_request(request);
    request->connect("request_completed",callable_mp(this, &ClassName::_http_request_completed));

    last_queue_id = 0;

    library_vb->add_constant_override("separation", 20 * EDSCALE);

    error_hb = memnew(HBoxContainer);
    library_main->add_child(error_hb);
    error_label = memnew(Label);
    error_label->add_theme_color_override("color", get_theme_color("error_color", "Editor"));
    error_hb->add_child(error_label);
    error_tr = memnew(TextureRect);
    error_tr->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
    error_hb->add_child(error_tr);

    description = nullptr;

    set_process(true);

    downloads_scroll = memnew(ScrollContainer);
    downloads_scroll->set_enable_h_scroll(true);
    downloads_scroll->set_enable_v_scroll(false);
    library_main->add_child(downloads_scroll);
    downloads_hb = memnew(HBoxContainer);
    downloads_scroll->add_child(downloads_hb);

    asset_open = memnew(EditorFileDialog);

    asset_open->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
    asset_open->add_filter("*.zip ; " + TTR("Assets ZIP File"));
    asset_open->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    add_child(asset_open);
    asset_open->connect("file_selected",callable_mp(this, &ClassName::_asset_file_selected));

    asset_installer = nullptr;
}

///////

bool AssetLibraryEditorPlugin::is_available() {
    return StreamPeerSSL::is_available();
}

void AssetLibraryEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {

        addon_library->show();
    } else {

        addon_library->hide();
    }
}

AssetLibraryEditorPlugin::AssetLibraryEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    addon_library = memnew(EditorAssetLibrary);
    addon_library->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    editor->get_viewport()->add_child(addon_library);
    addon_library->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    addon_library->hide();
}

AssetLibraryEditorPlugin::~AssetLibraryEditorPlugin() {
}
