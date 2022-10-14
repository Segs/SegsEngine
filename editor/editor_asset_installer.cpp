/*************************************************************************/
/*  editor_asset_installer.cpp                                           */
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

#include "editor_asset_installer.h"
#include "progress_dialog.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/string_formatter.h"
#include "core/io/zip_io.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "editor_node.h"
#include "editor/editor_file_system.h"

IMPL_GDCLASS(EditorAssetInstaller)

void EditorAssetInstaller::_update_subitems(TreeItem *p_item, bool p_check, bool p_first) {

    if (p_check) {
        if (p_item->get_custom_color(0) == Color()) {
            p_item->set_checked(0, true);
        }
    } else {
        p_item->set_checked(0, false);
    }

    if (p_item->get_children()) {
        _update_subitems(p_item->get_children(), p_check);
    }

    if (!p_first && p_item->get_next()) {
        _update_subitems(p_item->get_next(), p_check);
    }
}

void EditorAssetInstaller::_uncheck_parent(TreeItem *p_item) {
    if (!p_item) {
        return;
    }

    bool any_checked = false;
    TreeItem *item = p_item->get_children();
    while (item) {
        if (item->is_checked(0)) {
            any_checked = true;
            break;
        }
        item = item->get_next();
    }

    if (!any_checked) {
        p_item->set_checked(0, false);
        _uncheck_parent(p_item->get_parent());
    }
}

void EditorAssetInstaller::_item_edited() {

    if (updating)
        return;

    TreeItem *item = tree->get_edited();
    if (!item)
        return;

    String path = item->get_metadata(0).as<String>();

    updating = true;
    if (path == String() || item == tree->get_root()) { //a dir or root
        _update_subitems(item, item->is_checked(0), true);
    }

    if (item->is_checked(0)) {
        while (item) {
            item->set_checked(0, true);
            item = item->get_parent();
        }
    } else {
        _uncheck_parent(item->get_parent());
    }
    updating = false;
}

void EditorAssetInstaller::open(StringView p_path, int p_depth) {

    package_path = p_path;
    Set<String> files_sorted;

    FileAccess *src_f = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    unzFile pkg = unzOpen2(package_path.c_str(), &io);
    if (!pkg) {
        error->set_text(FormatVE(TTR("Error opening asset file for \"%s\" (not in ZIP format).").asCString(), asset_name.c_str()));
        return;
    }

    int ret = unzGoToFirstFile(pkg);

    while (ret == UNZ_OK) {

        //get filename
        unz_file_info info;
        char fname[16384];
        unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

        String name(fname);
        files_sorted.insert(name);

        ret = unzGoToNextFile(pkg);
    }

    Map<StringView, Ref<Texture> > extension_guess;
    {
        extension_guess["bmp"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["dds"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["exr"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["hdr"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["jpg"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["jpeg"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["png"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["svg"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["tga"] = tree->get_theme_icon("ImageTexture", "EditorIcons");
        extension_guess["webp"] = tree->get_theme_icon("ImageTexture", "EditorIcons");

        extension_guess["wav"] = tree->get_theme_icon("AudioStreamSample", "EditorIcons");
        extension_guess["ogg"] = tree->get_theme_icon("AudioStreamOGGVorbis", "EditorIcons");
        extension_guess["mp3"] = tree->get_theme_icon("AudioStreamMP3", "EditorIcons");

        extension_guess["scn"] = tree->get_theme_icon("PackedScene", "EditorIcons");
        extension_guess["tscn"] = tree->get_theme_icon("PackedScene", "EditorIcons");
        extension_guess["escn"] = tree->get_theme_icon("PackedScene", "EditorIcons");
        extension_guess["dae"] = tree->get_theme_icon("PackedScene", "EditorIcons");
        extension_guess["gltf"] = tree->get_theme_icon("PackedScene", "EditorIcons");
        extension_guess["glb"] = tree->get_theme_icon("PackedScene", "EditorIcons");

        extension_guess["gdshader"] = tree->get_theme_icon("Shader", "EditorIcons");
        extension_guess["shader"] = tree->get_theme_icon("Shader", "EditorIcons");

        if (Engine::get_singleton()->has_singleton("GodotSharp")) {
            extension_guess["cs"] = tree->get_theme_icon("CSharpScript", "EditorIcons");
        } else {
            // Mark C# support as unavailable.
            extension_guess["cs"] = tree->get_theme_icon("ImportFail", "EditorIcons");
        }
        extension_guess["vs"] = tree->get_theme_icon("VisualScript", "EditorIcons");

        extension_guess["res"] = tree->get_theme_icon("Resource", "EditorIcons");
        extension_guess["tres"] = tree->get_theme_icon("Resource", "EditorIcons");
        extension_guess["atlastex"] = tree->get_theme_icon("AtlasTexture", "EditorIcons");
        // By default, OBJ files are imported as Mesh resources rather than PackedScenes.
        extension_guess["obj"] = tree->get_theme_icon("Mesh", "EditorIcons");

        extension_guess["txt"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["md"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["rst"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["json"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["yml"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["yaml"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["toml"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["cfg"] = tree->get_theme_icon("TextFile", "EditorIcons");
        extension_guess["ini"] = tree->get_theme_icon("TextFile", "EditorIcons");
    }
    Ref<Texture> generic_extension = get_theme_icon("Object", "EditorIcons");

    unzClose(pkg);

    updating = true;
    tree->clear();
    TreeItem *root = tree->create_item();
    root->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
    root->set_checked(0, true);
    root->set_icon(0, get_theme_icon("folder", "FileDialog"));
    root->set_text(0, "res://");
    root->set_editable(0, true);
    Map<String, TreeItem *> dir_map;
    int num_file_conflicts = 0;

    for (const String &E : files_sorted) {

        String path = E;
        int depth = p_depth;
        bool skip = false;
        while (depth > 0) {
            int pp = StringUtils::find(path,"/");
            if (pp == -1) {
                skip = true;
                break;
            }
            path = StringUtils::substr(path,pp + 1, path.length());
            depth--;
        }

        if (skip || path.empty())
            continue;

        bool isdir = false;

        if (StringUtils::ends_with(path,'/')) {
            //a directory
            path = StringUtils::substr(path,0, path.length() - 1);
            isdir = true;
        }

        int pp = StringUtils::rfind(path,'/');

        TreeItem *parent;
        if (pp == -1) {
            parent = root;
        } else {
            String ppath(StringUtils::substr(path,0, pp));
            ERR_CONTINUE(!dir_map.contains(ppath));
            parent = dir_map[ppath];
        }

        TreeItem *ti = tree->create_item(parent);
        ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
        ti->set_checked(0, true);
        ti->set_editable(0, true);
        if (isdir) {
            dir_map[path] = ti;
            ti->set_text_utf8(0, String(PathUtils::get_file(path)) + "/");
            ti->set_icon(0, get_theme_icon("folder", "FileDialog"));
            ti->set_metadata(0, StringView());
        } else {
            String file(PathUtils::get_file(path));
            String extension(StringUtils::to_lower(PathUtils::get_extension(file)));
            if (extension_guess.contains(extension)) {
                ti->set_icon(0, extension_guess[extension]);
            } else {
                ti->set_icon(0, generic_extension);
            }
            ti->set_text_utf8(0, file);

            String res_path = "res://" + path;
            if (FileAccess::exists(res_path)) {
                num_file_conflicts ++;
                ti->set_custom_color(0, get_theme_color("error_color", "Editor"));
                ti->set_tooltip(0, FormatSN(TTR("%s (already exists)").asCString(), res_path.c_str()));
                ti->set_checked(0, false);
            } else {
                ti->set_tooltip(0, StringName(res_path));
            }

            ti->set_metadata(0, res_path);
        }

        status_map[E] = ti;
    }

    if (num_file_conflicts >= 1) {
        asset_contents->set_text(FormatVE(TTR("Contents of asset \"%s\" - %d file(s) conflict with your project:").asCString(), asset_name.c_str(), num_file_conflicts));
    } else {
        asset_contents->set_text(FormatVE(TTR("Contents of asset \"%s\" - No files conflict with your project:").asCString(), asset_name.c_str()));
    }

    popup_centered_ratio();
    updating = false;
}

void EditorAssetInstaller::ok_pressed() {

    FileAccess *src_f = nullptr;
    zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

    unzFile pkg = unzOpen2(package_path.c_str(), &io);
    if (!pkg) {
        error->set_text(FormatVE(TTR("Error opening asset file for \"%s\" (not in ZIP format).").asCString(), asset_name.c_str()));
        return;
    }

    int ret = unzGoToFirstFile(pkg);

    Vector<String> failed_files;

    ProgressDialog::get_singleton()->add_task("uncompress", TTR("Uncompressing Assets"), status_map.size());

    int idx = 0;
    while (ret == UNZ_OK) {

        //get filename
        unz_file_info info;
        char fname[16384];
        //TODO: handle failure below.
        ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

        String name(fname);

        if (status_map.contains(name) && status_map[name]->is_checked(0)) {

            String path = status_map[name]->get_metadata(0).as<String>();
            if (path.empty()) { // a dir

                String dirpath;
                TreeItem *t = status_map[name];
                while (t) {
                    dirpath = t->get_text(0) + dirpath;
                    t = t->get_parent();
                }

                if (StringUtils::ends_with(dirpath,'/')) {
                    dirpath = StringUtils::substr(dirpath,0, dirpath.length() - 1);
                }

                DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
                da->make_dir(dirpath);
                memdelete(da);

            } else {
                size_t sz= info.uncompressed_size;
                auto data = eastl::make_unique<uint8_t[]>(sz);

                //read
                unzOpenCurrentFile(pkg);
                unzReadCurrentFile(pkg, data.get(), sz);
                unzCloseCurrentFile(pkg);

                FileAccess *f = FileAccess::open(path, FileAccess::WRITE);
                if (f) {
                    f->store_buffer(data.get(), info.uncompressed_size);
                    memdelete(f);
                } else {
                    failed_files.push_back(path);
                }

                ProgressDialog::get_singleton()->task_step("uncompress", path, idx);
            }
        }

        idx++;
        ret = unzGoToNextFile(pkg);
    }

    ProgressDialog::get_singleton()->end_task("uncompress");
    unzClose(pkg);

    if (!failed_files.empty()) {
        String msg = FormatVE(TTR("The following files failed extraction from asset \"%s\":").asCString(), asset_name.c_str()) + "\n\n";
        for (int i = 0; i < failed_files.size(); i++) {

            if (i > 15) {
                msg += "\n" + FormatVE(TTR("(and %d more files)").asCString(), failed_files.size() - i);
                break;
            }
            msg += failed_files[i];
        }
        if (EditorNode::get_singleton() != nullptr)
            EditorNode::get_singleton()->show_warning(StringName(msg));
    } else {
        if (EditorNode::get_singleton() != nullptr) {
            EditorNode::get_singleton()->show_warning(TTR("Package installed successfully!"), TTR("Success!"));
            EditorNode::get_singleton()->show_warning(FormatSN(TTR("Asset \"%s\" installed successfully!").asCString(), asset_name.c_str()), TTR("Success!"));

        }
    }
    EditorFileSystem::get_singleton()->scan_changes();
}

void EditorAssetInstaller::set_asset_name(StringView p_asset_name) {
    asset_name = p_asset_name;
}

String EditorAssetInstaller::get_asset_name() const {
    return asset_name;
}

EditorAssetInstaller::EditorAssetInstaller() {

    VBoxContainer *vb = memnew(VBoxContainer);
    add_child(vb);

    asset_contents = memnew(Label);
    vb->add_child(asset_contents);

    tree = memnew(Tree);
    tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    tree->connect("item_edited",callable_mp(this, &ClassName::_item_edited));
    vb->add_child(tree);

    error = memnew(AcceptDialog);
    add_child(error);
    get_ok()->set_text(TTR("Install"));
    set_title(TTR("Asset Installer"));

    updating = false;

    set_hide_on_ok(true);
}
