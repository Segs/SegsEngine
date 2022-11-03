/*************************************************************************/
/*  editor_file_system.cpp                                               */
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

#include "editor_file_system.h"

#include "core/io/resource_importer.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/map.h"
#include "core/method_bind.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/resource/resource_tools.h"
#include "core/script_language.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "core/string_utils.inl"
#include "core/translation_helpers.h"
#include "core/variant_parser.h"
#include "editor_node.h"
#include "editor_resource_preview.h"
#include "editor_settings.h"
#include "EASTL/sort.h"

IMPL_GDCLASS(EditorFileSystemDirectory)
IMPL_GDCLASS(EditorFileSystem)

EditorFileSystem *EditorFileSystem::singleton = nullptr;

//the name is the version, to keep compatibility with different versions of Godot
#define CACHE_FILE_NAME "filesystem_cache6"

bool editor_should_skip_directory(StringView p_path) {
    String project_data_path = ProjectSettings::get_singleton()->get_project_data_path();
    if (p_path == project_data_path || p_path.starts_with(project_data_path + "/")) {
        return true;
    }

    if (FileAccess::exists(PathUtils::plus_file(p_path,"project.godot"))) // skip if another project inside this
        return true;

    if (FileAccess::exists(PathUtils::plus_file(p_path,".gdignore"))) // skip if a `.gdignore` file is inside this
        return true;

    return false;
}
void EditorFileSystemDirectory::sort_files() {
    eastl::sort(files.begin(),files.end());
}

int EditorFileSystemDirectory::find_file_index(StringView p_file) const {

    for (size_t i = 0; i < files.size(); i++) {
        if (files[i]->file == p_file)
            return i;
    }
    return -1;
}
int EditorFileSystemDirectory::find_dir_index(StringView p_dir) const {

    for (size_t i = 0; i < subdirs.size(); i++) {
        if (subdirs[i]->name == p_dir)
            return i;
    }

    return -1;
}

void EditorFileSystemDirectory::force_update() {
    // We set modified_time to 0 to force `EditorFileSystem::_scan_fs_changes` to search changes in the directory
    modified_time = 0;
}
int EditorFileSystemDirectory::get_subdir_count() const {

    return subdirs.size();
}

EditorFileSystemDirectory *EditorFileSystemDirectory::get_subdir(int p_idx) {

    ERR_FAIL_INDEX_V(p_idx, subdirs.size(), nullptr);
    return subdirs[p_idx];
}

int EditorFileSystemDirectory::get_file_count() const {

    return files.size();
}

String EditorFileSystemDirectory::get_file(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, files.size(), String());

    return files[p_idx]->file;
}

String EditorFileSystemDirectory::get_path() const {

    String p;
    const EditorFileSystemDirectory *d = this;
    while (d->parent) {
        p = PathUtils::plus_file(d->name,p);
        d = d->parent;
    }

    return "res://" + p;
}

String EditorFileSystemDirectory::get_file_path(int p_idx) const {

    String file(get_file(p_idx));
    const EditorFileSystemDirectory *d = this;
    while (d->parent) {
        file = PathUtils::plus_file(d->name,file);
        d = d->parent;
    }

    return "res://" + file;
}

String EditorFileSystemDirectory::get_named_file_path(StringView named_file) const
{
    String file(named_file);
    const EditorFileSystemDirectory *d = this;
    while (d->parent) {
        file = PathUtils::plus_file(d->name,file);
        d = d->parent;
    }

    return "res://" + file;
}

const Vector<String> &EditorFileSystemDirectory::get_file_deps(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, files.size(), null_string_pvec);
    return files[p_idx]->deps;
}

uint64_t EditorFileSystemDirectory::get_file_modified_time(int p_idx) const {
    ERR_FAIL_INDEX_V(p_idx, files.size(), 0);
    return files[p_idx]->modified_time;
}

bool EditorFileSystemDirectory::get_file_import_is_valid(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, files.size(), false);
    return files[p_idx]->import_valid;
}

StringName EditorFileSystemDirectory::get_file_script_class_name(int p_idx) const {
    return files[p_idx]->script_class_name;
}

StringName EditorFileSystemDirectory::get_file_script_class_extends(int p_idx) const {
    return files[p_idx]->script_class_extends;
}

const String & EditorFileSystemDirectory::get_file_script_class_icon_path(int p_idx) const {
    return files[p_idx]->script_class_icon_path;
}

StringName EditorFileSystemDirectory::get_file_type(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, files.size(), "");
    return files[p_idx]->type;
}

const String &EditorFileSystemDirectory::get_name() {

    return name;
}

EditorFileSystemDirectory *EditorFileSystemDirectory::get_parent() {

    return parent;
}

void EditorFileSystemDirectory::_bind_methods() {

    SE_BIND_METHOD(EditorFileSystemDirectory,get_subdir_count);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_subdir);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_count);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_path);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_type);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_script_class_name);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_script_class_extends);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_file_import_is_valid);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_name);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_path);
    SE_BIND_METHOD(EditorFileSystemDirectory,get_parent);
    SE_BIND_METHOD(EditorFileSystemDirectory,find_file_index);
    SE_BIND_METHOD(EditorFileSystemDirectory,find_dir_index);
}

EditorFileSystemDirectory::EditorFileSystemDirectory() {

    modified_time = 0;
    parent = nullptr;
    verified = false;
}

EditorFileSystemDirectory::~EditorFileSystemDirectory() {

    for (FileInfo * f : files) {
        memdelete(f);
    }

    for (EditorFileSystemDirectory * d : subdirs) {
        memdelete(d);
    }
}

void EditorFileSystem::_scan_from_cache()
{
    String fscache = PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(), CACHE_FILE_NAME);

    FileAccess *f = FileAccess::open(fscache, FileAccess::READ);
    if (!f)
        return;
    // read .fscache
    String cpath;

    bool first = true;

    while (!f->eof_reached()) {

        String l(StringUtils::strip_edges(f->get_line()));
        if (first) {
            if (first_scan) {
                // only use this on first scan, afterwards it gets ignored
                // this is so on first reimport we synchronize versions, then
                // we don't care until editor restart. This is for usability mainly so
                // your workflow is not killed after changing a setting by forceful reimporting
                // everything there is.
                filesystem_settings_version_for_import = StringUtils::strip_edges(l);
                if (filesystem_settings_version_for_import !=
                        ResourceFormatImporter::get_singleton()->get_import_settings_hash()) {
                    revalidate_import_files = true;
                }
            }
            first = false;
            continue;
        }
        if (l.empty()) continue;

        if (StringUtils::begins_with(l, "::")) {
            Vector<StringView> split = StringUtils::split(l, "::");
            ERR_CONTINUE(split.size() != 3);
            StringView name = split[1];

            cpath = name;

        } else {
            Vector<StringView> split = StringUtils::split(l, "::");
            ERR_CONTINUE(split.size() != 8);
            String name = PathUtils::plus_file(cpath, split[0]);

            FileCache fc;
            fc.type = split[1];
            fc.modification_time = StringUtils::to_int64(split[2]);
            fc.import_modification_time = StringUtils::to_int64(split[3]);
            fc.import_valid = StringUtils::to_int64(split[4]) != 0;
            fc.import_group_file = StringUtils::strip_edges(split[5]);
            fc.script_class_name = StringName(StringUtils::get_slice(split[6], "<>", 0));
            fc.script_class_extends = StringName(StringUtils::get_slice(split[6], "<>", 1));
            fc.script_class_icon_path = StringUtils::get_slice(split[6], "<>", 2);

            StringView deps = StringUtils::strip_edges(split[7]);
            if (deps.length()) {
                Vector<StringView> dp = StringUtils::split(deps, "<>");
                for (int i = 0; i < dp.size(); i++) {
                    StringView path = dp[i];
                    fc.deps.emplace_back(path);
                }
            }

            file_cache[String(name)] = fc;
        }
    }

    f->close();
    memdelete(f);
}

void EditorFileSystem::_scan_mark_updates()
{
    String update_cache =
            PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(), "filesystem_update4");

    if (!FileAccess::exists(update_cache))
        return;

    {
        FileAccessRef f2 = FileAccess::open(update_cache, FileAccess::READ);
        String l(StringUtils::strip_edges(f2->get_line()));
        while (!l.empty()) {

            file_cache.erase(l); // erase cache for this, so it gets updated
            l = StringUtils::strip_edges(f2->get_line());
        }
    }

    DirAccessRef d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    d->remove(update_cache); // bye bye update cache
}

void EditorFileSystem::_scan_filesystem() {

    ERR_FAIL_COND(!scanning || new_filesystem);

    sources_changed.clear();
    file_cache.clear();

    // read the disk cache
    _scan_from_cache();

    _scan_mark_updates();

    EditorProgressBG scan_progress("efs", "ScanFS", 1000);

    ScanProgress sp;
    sp.low = 0;
    sp.hi = 1;
    sp.progress = &scan_progress;

    new_filesystem = memnew(EditorFileSystemDirectory);
    new_filesystem->parent = nullptr;

    DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    d->change_dir("res://");
    _scan_new_dir(new_filesystem, d, sp);

    file_cache.clear(); // clear caches, no longer needed

    memdelete(d);

    if (!first_scan) {
        // on the first scan this is done from the main thread after re-importing
        _save_filesystem_cache();
    }

    scanning = false;
}

void EditorFileSystem::_save_filesystem_cache() {

    group_file_cache.clear();

    String fscache = PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),CACHE_FILE_NAME);

    FileAccess *f = FileAccess::open(fscache, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(!f, "Cannot create file '" + fscache + "'. Check user write permissions.");

    f->store_line(filesystem_settings_version_for_import);
    _save_filesystem_cache(filesystem, f);
    f->close();
    memdelete(f);
}

void EditorFileSystem::_thread_func(void *_userdata) {

    EditorFileSystem *sd = (EditorFileSystem *)_userdata;
    sd->_scan_filesystem();
}

bool EditorFileSystem::_test_for_reimport(StringView p_path, bool p_only_imported_files) {

    if (!reimport_on_missing_imported_files && p_only_imported_files)
        return false;

    if (!FileAccess::exists(String(p_path) + ".import")) {
        return true;
    }

    if (!ResourceFormatImporter::get_singleton()->are_import_settings_valid(p_path)) {
        //reimport settings are not valid, reimport
        return true;
    }

    Error err;
    FileAccess *f = FileAccess::open(String(p_path) + ".import", FileAccess::READ, &err);

    if (!f) { //no import file, do reimport
        return true;
    }

    VariantParserStream *stream = VariantParser::get_file_stream(f);

    Variant value;
    VariantParser::Tag next_tag;

    int lines = 0;
    String error_text;

    Vector<String> to_check;

    String importer_name;
    String source_file;
    String source_md5;
    Vector<String> dest_files;
    String dest_md5;

    while (true) {

        String assign = Variant().as<String>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(stream, lines, error_text, next_tag, assign, value, nullptr, true);
        if (err == ERR_FILE_EOF) {
            break;
        } else if (err != OK) {
            ERR_PRINT("ResourceFormatImporter::load - '" + String(p_path) + ".import:" + ::to_string(lines) + "' error '" + error_text + "'.");
            VariantParser::release_stream(stream);
            memdelete(f);
            return false; //parse error, try reimport manually (Avoid reimport loop on broken file)
        }

        if (!assign.empty()) {
            if (StringUtils::begins_with(assign,"path")) {
                to_check.push_back(value.as<String>());
            } else if (assign == "files") {
                Array fa = value.as<Array>();
                for (int i = 0; i < fa.size(); i++) {
                    to_check.push_back(fa[i].as<String>());
                }
            } else if (assign == "importer") {
                importer_name = value.as<String>();
            } else if (!p_only_imported_files) {
                if (assign == "source_file") {
                    source_file = value.as<String>();
                } else if (assign == "dest_files") {
                    dest_files = value.as<Vector<String>>();
                }
            }

        } else if (next_tag.name != "remap" && next_tag.name != "deps") {
            break;
        }
    }

    VariantParser::release_stream(stream);
    memdelete(f);

    if (importer_name == "keep") {
        return false; //keep mode, do not reimport
    }
    // Read the md5's from a separate file (so the import parameters aren't dependent on the file version
    String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(p_path);
    FileAccess *md5s = FileAccess::open(base_path + ".md5", FileAccess::READ, &err);
    if (!md5s) { // No md5's stored for this resource
        return true;
    }

    VariantParserStream *md5_stream = VariantParser::get_file_stream(md5s);

    while (true) {
        String assign = Variant().as<String>();
        next_tag.fields.clear();
        next_tag.name.clear();

        err = VariantParser::parse_tag_assign_eof(md5_stream, lines, error_text, next_tag, assign, value, nullptr, true);

        if (err == ERR_FILE_EOF) {
            break;
        } else if (err != OK) {
            ERR_PRINT("ResourceFormatImporter::load - '" + String(p_path) + ".import.md5:" + ::to_string(lines) + "' error '" + error_text + "'.");
            VariantParser::release_stream(md5_stream);
            memdelete(md5s);
            return false; // parse error
        }
        if (!assign.empty() && !p_only_imported_files) {
            if (assign == "source_md5") {
                source_md5 = value.as<String>();
            } else if (assign == "dest_md5") {
                dest_md5 = value.as<String>();
            }
        }
    }
    VariantParser::release_stream(md5_stream);
    memdelete(md5s);

    //imported files are gone, reimport
    for (const String &E : to_check) {
        if (!FileAccess::exists(E)) {
            return true;
        }
    }

    //check source md5 matching
    if (!p_only_imported_files) {

        if (!source_file.empty() && StringView(source_file) != p_path) {
            return true; //file was moved, reimport
        }

        if (source_md5.empty()) {
            return true; //lacks md5, so just reimport
        }

        String md5(FileAccess::get_md5(p_path));
        if (md5 != source_md5) {
            return true;
        }

        if (!dest_files.empty() && !dest_md5.empty()) {
            md5 = FileAccess::get_multiple_md5(dest_files);
            if (md5 != dest_md5) {
                return true;
            }
        }
    }

    return false; //nothing changed
}

bool EditorFileSystem::_update_scan_actions() {

    sources_changed.clear();

    bool fs_changed = false;

    Vector<String> reimports;
    PoolVector<String> reloads;

    for (ItemAction &ia : scan_actions) {

        switch (ia.action) {
            case ItemAction::ACTION_NONE: {

            } break;
            case ItemAction::ACTION_DIR_ADD: {

                int idx = 0;
                for (int i = 0; i < ia.dir->subdirs.size(); i++) {

                    if (ia.new_dir->name < ia.dir->subdirs[i]->name)
                        break;
                    idx++;
                }
                if (idx == ia.dir->subdirs.size()) {
                    ia.dir->subdirs.push_back(ia.new_dir);
                } else {
                    ia.dir->subdirs.insert(ia.dir->subdirs.begin()+idx, ia.new_dir);
                }

                fs_changed = true;
            } break;
            case ItemAction::ACTION_DIR_REMOVE: {

                ERR_CONTINUE(!ia.dir->parent);
                ia.dir->parent->subdirs.erase_first(ia.dir);
                memdelete(ia.dir);
                fs_changed = true;
            } break;
            case ItemAction::ACTION_FILE_ADD: {

                int idx = 0;
                for (int i = 0; i < ia.dir->files.size(); i++) {

                    if (ia.new_file->file < ia.dir->files[i]->file)
                        break;
                    idx++;
                }
                if (idx == ia.dir->files.size()) {
                    ia.dir->files.push_back(ia.new_file);
                } else {
                    ia.dir->files.insert(ia.dir->files.begin()+idx, ia.new_file);
                }

                fs_changed = true;

            } break;
            case ItemAction::ACTION_FILE_REMOVE: {

                int idx = ia.dir->find_file_index(ia.file);
                ERR_CONTINUE(idx == -1);
                _delete_internal_files(ia.dir->files[idx]->file);
                memdelete(ia.dir->files[idx]);
                ia.dir->files.erase_at(idx);

                fs_changed = true;

            } break;
            case ItemAction::ACTION_FILE_TEST_REIMPORT: {

                int idx = ia.dir->find_file_index(ia.file);
                ERR_CONTINUE(idx == -1);
                String full_path = ia.dir->get_file_path(idx);
                if (_test_for_reimport(full_path, false)) {
                    //must reimport
                    reimports.push_back(full_path);
                    reimports.push_back(_get_dependencies(full_path));
                } else {
                    //must not reimport, all was good
                    //update modified times, to avoid reimport
                    ia.dir->files[idx]->modified_time = FileAccess::get_modified_time(full_path);
                    ia.dir->files[idx]->import_modified_time = FileAccess::get_modified_time(full_path + ".import");
                }

                fs_changed = true;
            } break;
            case ItemAction::ACTION_FILE_RELOAD: {

                int idx = ia.dir->find_file_index(ia.file);
                ERR_CONTINUE(idx == -1);
                String full_path = ia.dir->get_file_path(idx);

                reloads.push_back(full_path);

            } break;
        }
    }

    if (!reimports.empty()) {
        reimport_files(reimports);
    }

    if (first_scan) {
        //only on first scan this is valid and updated, then settings changed.
        revalidate_import_files = false;
        filesystem_settings_version_for_import = ResourceFormatImporter::get_singleton()->get_import_settings_hash();
        _save_filesystem_cache();
    }

    if (!reloads.empty()) {
        emit_signal("resources_reload", reloads);
    }
    scan_actions.clear();

    return fs_changed;
}

void EditorFileSystem::scan() {

    if (false /*&& bool(Globals::get_singleton()->get("debug/disable_scan"))*/)
        return;

    if (scanning || scanning_changes || thread.is_started())
        return;

    _update_extensions();

    abort_scan = false;
    if (!use_threads) {
        scanning = true;
        scan_total = 0;
        _scan_filesystem();
        memdelete(filesystem);
        //file_type_cache.clear();
        filesystem = new_filesystem;
        new_filesystem = nullptr;
        _update_scan_actions();
        scanning = false;
        emit_signal("filesystem_changed");
        emit_signal("sources_changed", !sources_changed.empty());
        _queue_update_script_classes();
        first_scan = false;
    } else {

        ERR_FAIL_COND(thread.is_started());
        set_process(true);
        Thread::Settings s;
        scanning = true;
        scan_total = 0;
        s.priority = Thread::PRIORITY_LOW;
        thread.start(_thread_func, this, s);
        //tree->hide();
        //progress->show();
    }
}

void EditorFileSystem::ScanProgress::update(int p_current, int p_total) const {

    float ratio = low + (hi - low) / p_total * p_current;
    progress->step(ratio * 1000);
    EditorFileSystem::singleton->scan_total = ratio;
}

EditorFileSystem::ScanProgress EditorFileSystem::ScanProgress::get_sub(int p_current, int p_total) const {

    ScanProgress sp = *this;
    float slice = (sp.hi - sp.low) / p_total;
    sp.low += slice * p_current;
    sp.hi = slice;
    return sp;
}

void EditorFileSystem::_scan_new_dir(EditorFileSystemDirectory *p_dir, DirAccess *da, const ScanProgress &p_progress) {

    Vector<String> dirs;
    Vector<String> files;

    String cd = da->get_current_dir();

    p_dir->modified_time = FileAccess::get_modified_time(cd);

    da->list_dir_begin();
    while (da->has_next()) {

        String f = da->get_next();
        if (f.empty())
            break;

        if (da->current_is_hidden())
            continue;

        if (da->current_is_dir()) {

            if (StringUtils::begins_with(f,".")) { // Ignore special and . / ..
                continue;
            }

            if (editor_should_skip_directory(PathUtils::plus_file(cd,f))) {
                continue;
            }

            dirs.emplace_back(eastl::move(f));

        } else {

            files.emplace_back(eastl::move(f));
        }
    }

    da->list_dir_end();

    eastl::sort(dirs.begin(),dirs.end(),NaturalNoCaseComparator());
    eastl::sort(files.begin(),files.end(),NaturalNoCaseComparator());

    int total = dirs.size() + files.size();
    int idx = -1;

    for (const String &entry : dirs) {
        idx++;

        if (da->change_dir(entry) != OK) {
            ERR_PRINT("Cannot go into subdir: " + entry);
            p_progress.update(idx, total);
            continue;
        }

        String d = da->get_current_dir();

        if (d == cd || !StringUtils::begins_with(d,cd)) {
            da->change_dir(cd); //avoid recursion
        } else {

            EditorFileSystemDirectory *efd = memnew(EditorFileSystemDirectory);

            efd->parent = p_dir;
            efd->name = entry;

            _scan_new_dir(efd, da, p_progress.get_sub(idx, total));

            int idx2 = 0;
            for (auto & subdir : p_dir->subdirs) {

                if (efd->name < subdir->name)
                    break;
                idx2++;
            }
            if (idx2 == p_dir->subdirs.size()) {
                p_dir->subdirs.push_back(efd);
            } else {
                p_dir->subdirs.insert(p_dir->subdirs.begin()+idx2, efd);
            }

            da->change_dir("..");
        }

        p_progress.update(idx, total);
    }
    ResourceFormatImporter *rfi = ResourceFormatImporter::get_singleton();
    for (size_t i=0,fin=files.size(); i<fin; ++i, ++idx) {
        const String &fname(files[i]);

        String ext = StringUtils::to_lower(PathUtils::get_extension(fname));
        if (!valid_extensions.contains(ext)) {
            continue; //invalid
        }

        EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
        fi->file = fname;

        String path = PathUtils::plus_file(cd,fi->file);

        auto fc_iter = file_cache.find(path);
        FileCache *fc = file_cache.end()==fc_iter ? nullptr : &fc_iter->second;
        uint64_t mt = FileAccess::get_modified_time(path);

        if (import_extensions.contains(ext) && ResourceFormatImporter::get_singleton()->any_can_import(path)) {

            //is imported
            uint64_t import_mt = 0;
            if (FileAccess::exists(path + ".import")) {
                import_mt = FileAccess::get_modified_time(path + ".import");
            }

            if (fc && fc->modification_time == mt && fc->import_modification_time == import_mt && !_test_for_reimport(path, true)) {

                fi->type = StringName(fc->type);
                fi->deps = fc->deps;
                fi->modified_time = fc->modification_time;
                fi->import_modified_time = fc->import_modification_time;
                fi->import_valid = fc->import_valid;
                fi->script_class_name = fc->script_class_name;
                fi->import_group_file = fc->import_group_file;
                fi->script_class_extends = fc->script_class_extends;
                fi->script_class_icon_path = fc->script_class_icon_path;

                if (revalidate_import_files && !rfi->are_import_settings_valid(path)) {
                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                    ia.dir = p_dir;
                    ia.file = fname;
                    scan_actions.push_back(ia);
                }

                if (fc->type.empty()) {
                    fi->type = StringName(gResourceManager().get_resource_type(path));
                    fi->import_group_file = gResourceManager().get_import_group_file(path);
                    //there is also the chance that file type changed due to reimport, must probably check this somehow here
                    //(or kind of note it for next time in another file?)
                    //note: I think this should not happen any longer..
                }

            } else {

                fi->type = StringName(rfi->get_resource_type(path));
                fi->import_group_file = rfi->get_import_group_file(path);
                fi->script_class_name = _get_global_script_class(fi->type, path, &fi->script_class_extends, &fi->script_class_icon_path);
                fi->modified_time = 0;
                fi->import_modified_time = 0;
                fi->import_valid = gResourceManager().is_import_valid(path);

                ItemAction ia;
                ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                ia.dir = p_dir;
                ia.file = fname;
                scan_actions.push_back(ia);
            }
        } else {
            fi->import_modified_time = 0;
            fi->import_valid = true;

            if (fc && fc->modification_time == mt) {
                //not imported, so just update type if changed
                fi->type = StringName(fc->type);
                fi->modified_time = fc->modification_time;
                fi->deps = fc->deps;
                fi->script_class_name = fc->script_class_name;
                fi->script_class_extends = fc->script_class_extends;
                fi->script_class_icon_path = fc->script_class_icon_path;
            } else {
                //new or modified time
                fi->type = StringName(gResourceManager().get_resource_type(path));
                fi->script_class_name = _get_global_script_class(fi->type, path, &fi->script_class_extends, &fi->script_class_icon_path);
                fi->deps = _get_dependencies(path);
                fi->modified_time = mt;
            }
        }

        p_dir->files.push_back(fi);
        p_progress.update(idx, total);
    }
}
void EditorFileSystem::_process_directory_changes(EditorFileSystemDirectory *p_dir, const ScanProgress &p_progress) {
    String cd = p_dir->get_path();
    uint64_t current_mtime = FileAccess::get_modified_time(cd);
    p_dir->modified_time = current_mtime;
    // ooooops, dir changed, see what's going on

    // first mark everything as verified

    for (auto *fi : p_dir->files) {
        fi->verified = false;
    }
    for (auto *sd : p_dir->subdirs) {
        sd->verified = false;
    }

    // then scan files and directories and check what's different

    DirAccessRef da(DirAccess::create(DirAccess::ACCESS_RESOURCES));

    Error ret = da->change_dir(cd);
    ERR_FAIL_COND_MSG(ret != OK, "Cannot change to '" + cd + "' folder.");
    da->list_dir_begin();
    while (da->has_next()) {
        String f = da->get_next();
        if (f.empty()) // TODO: additional check that might be removed when we tighten up the get_next post-conditions.
            break;

        if (da->current_is_hidden())
            continue;

        if (da->current_is_dir()) {
            if (StringUtils::begins_with(f, ".")) // Ignore special and . / ..
                continue;

            int idx = p_dir->find_dir_index(f);
            if (idx == -1) {
                if (editor_should_skip_directory(PathUtils::plus_file(cd, f)))
                    continue;

                EditorFileSystemDirectory *efd = memnew(EditorFileSystemDirectory);

                efd->parent = p_dir;
                efd->name = f;
                DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
                d->change_dir(PathUtils::plus_file(cd, f));
                _scan_new_dir(efd, d, p_progress.get_sub(1, 1));
                memdelete(d);

                ItemAction ia;
                ia.action = ItemAction::ACTION_DIR_ADD;
                ia.dir = p_dir;
                ia.file = f;
                ia.new_dir = efd;
                scan_actions.push_back(ia);
            } else {
                p_dir->subdirs[idx]->verified = true;
            }

        } else {
            String ext = StringUtils::to_lower(PathUtils::get_extension(f));
            if (!valid_extensions.contains(ext)) {
                continue; // invalid
            }
            int idx = p_dir->find_file_index(f);

            if (idx == -1) {
                // never seen this file, add actition to add it
                EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
                fi->file = f;

                String path = PathUtils::plus_file(cd, fi->file);
                bool importer_can_import = ResourceFormatImporter::get_singleton()->any_can_import(path);
                fi->modified_time = FileAccess::get_modified_time(path);
                fi->import_modified_time = 0;
                fi->type = StringName(gResourceManager().get_resource_type(path));
                fi->script_class_name = _get_global_script_class(fi->type, path, &fi->script_class_extends, &fi->script_class_icon_path);
                fi->import_valid = gResourceManager().is_import_valid(path);
                fi->import_group_file = gResourceManager().get_import_group_file(path);

                {
                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_ADD;
                    ia.dir = p_dir;
                    ia.file = f;
                    ia.new_file = fi;
                    scan_actions.emplace_back(eastl::move(ia));
                }

                if (importer_can_import && import_extensions.contains(ext)) {
                    // if it can be imported, and it was added, it needs to be reimported
                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                    ia.dir = p_dir;
                    ia.file = f;
                    scan_actions.emplace_back(eastl::move(ia));
                }

            } else {
                p_dir->files[idx]->verified = true;
            }
        }
    }

    da->list_dir_end();

}
void EditorFileSystem::_scan_fs_changes(EditorFileSystemDirectory *p_startdir, const ScanProgress &p_progress) {

    Dequeue<EditorFileSystemDirectory *> work_queue;
    work_queue.push_back(p_startdir);
    while (!work_queue.empty()) {
        EditorFileSystemDirectory *p_dir = work_queue.front();
        work_queue.pop_front();

        String cd = p_dir->get_path();
        uint64_t current_mtime = FileAccess::get_modified_time(cd);
        bool updated_dir = current_mtime != p_dir->modified_time || using_fat32_or_exfat;

        if (updated_dir) {
            _process_directory_changes(p_dir, p_progress);
        }

        for (size_t i = 0; i < p_dir->files.size(); i++) {
            if (updated_dir && !p_dir->files[i]->verified) {
                // this file was removed, add action to remove it
                ItemAction ia;
                ia.action = ItemAction::ACTION_FILE_REMOVE;
                ia.dir = p_dir;
                ia.file = p_dir->files[i]->file;
                scan_actions.push_back(ia);
                continue;
            }

            String path = PathUtils::plus_file(cd, p_dir->files[i]->file);

            if (import_extensions.contains(StringUtils::to_lower(PathUtils::get_extension(p_dir->files[i]->file)))) {
                // check here if file must be imported or not
                bool importer_can_import = ResourceFormatImporter::get_singleton()->any_can_import(path);
                if (!importer_can_import) {
                    continue;
                }

                uint64_t mt = FileAccess::get_modified_time(path);

                bool reimport = false;

                if (mt != p_dir->files[i]->modified_time) {
                    reimport = true; // it was modified, must be reimported.
                } else if (!FileAccess::exists(path + ".import")) {
                    reimport = true; // no .import file, obviously reimport
                } else {
                    uint64_t import_mt = FileAccess::get_modified_time(path + ".import");
                    if (import_mt != p_dir->files[i]->import_modified_time) {
                        reimport = true;
                    } else if (_test_for_reimport(path, true)) {
                        reimport = true;
                    }
                }

                if (reimport) {
                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_TEST_REIMPORT;
                    ia.dir = p_dir;
                    ia.file = p_dir->files[i]->file;
                    scan_actions.push_back(ia);
                }
            } else if (ResourceCache::has(path)) { // test for potential reload

                uint64_t mt = FileAccess::get_modified_time(path);

                if (mt != p_dir->files[i]->modified_time) {
                    p_dir->files[i]->modified_time = mt; // save new time, but test for reload

                    ItemAction ia;
                    ia.action = ItemAction::ACTION_FILE_RELOAD;
                    ia.dir = p_dir;
                    ia.file = p_dir->files[i]->file;
                    scan_actions.push_back(ia);
                }
            }
        }

        for (EditorFileSystemDirectory *subdir : p_dir->subdirs) {
            if ((updated_dir && !subdir->verified) || editor_should_skip_directory(subdir->get_path())) {
                // this directory was removed, add action to remove it
                ItemAction ia;
                ia.action = ItemAction::ACTION_DIR_REMOVE;
                ia.dir = subdir;
                scan_actions.push_back(ia);
                continue;
            }
            work_queue.push_back(subdir);
        }
    }
}

void EditorFileSystem::_delete_internal_files(StringView p_file) {
    if (FileAccess::exists(String(p_file) + ".import")) {
        Vector<String> paths;
        ResourceFormatImporter::get_singleton()->get_internal_resource_path_list(p_file, &paths);
        DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        for (const String &E : paths) {
            da->remove(E);
        }
        da->remove(String(p_file) + ".import");
        memdelete(da);
    }
}

void EditorFileSystem::_thread_func_sources(void *_userdata) {

    EditorFileSystem *efs = (EditorFileSystem *)_userdata;
    if (efs->filesystem) {
        EditorProgressBG pr(StringName("sources"), TTR("ScanSources"), 1000);
        ScanProgress sp {0,1,&pr};
        efs->_scan_fs_changes(efs->filesystem, sp);
    }
    efs->scanning_changes_done = true;
}

void EditorFileSystem::scan_changes() {

    if (first_scan || // Prevent a premature changes scan from inhibiting the first full scan
            scanning || scanning_changes || thread.is_started()) {
        scan_changes_pending = true;
        set_process(true);
        return;
    }

    _update_extensions();
    sources_changed.clear();
    scanning_changes = true;
    scanning_changes_done = false;

    abort_scan = false;

    if (!use_threads) {
        if (filesystem) {
            EditorProgressBG pr(("sources"), TTR("ScanSources"), 1000);
            ScanProgress sp{0,1,&pr};
            scan_total = 0;
            _scan_fs_changes(filesystem, sp);
            if (_update_scan_actions())
                emit_signal("filesystem_changed");
        }
        scanning_changes = false;
        scanning_changes_done = true;
        emit_signal("sources_changed", !sources_changed.empty());
    } else {

        ERR_FAIL_COND(thread_sources.is_started());
        set_process(true);
        scan_total = 0;
        Thread::Settings s;
        s.priority = Thread::PRIORITY_LOW;
        thread_sources.start(_thread_func_sources, this, s);
    }
}

void EditorFileSystem::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_ENTER_TREE: {
            //this should happen after every editor node entered the tree
            call_deferred([this]() {scan();});
            //call_deferred("scan"); //this should happen after every editor node entered the tree

        } break;
        case NOTIFICATION_EXIT_TREE: {
            Thread &active_thread = thread.is_started() ? thread : thread_sources;
            if (use_threads && active_thread.is_started()) {
                //abort thread if in progress
                abort_scan = true;
                while (scanning) {
                    OS::get_singleton()->delay_usec(1000);
                }
                active_thread.wait_to_finish();
                WARN_PRINT("Scan thread aborted...");
                set_process(false);
            }

            memdelete(filesystem);
            memdelete(new_filesystem);
            filesystem = nullptr;
            new_filesystem = nullptr;

        } break;
        case NOTIFICATION_PROCESS: {

            if (!use_threads) {
                break;
            }
            if (scanning_changes) {

                if (scanning_changes_done) {

                    scanning_changes = false;

                    set_process(false);

                    thread_sources.wait_to_finish();

                    if (_update_scan_actions())
                        emit_signal("filesystem_changed");
                    emit_signal("sources_changed", !sources_changed.empty());
                    _queue_update_script_classes();
                    first_scan = false;
                }
            } else if (!scanning && thread.is_started()) {

                set_process(false);

                if (filesystem)
                    memdelete(filesystem);
                filesystem = new_filesystem;
                new_filesystem = nullptr;
                thread.wait_to_finish();
                _update_scan_actions();
                emit_signal("filesystem_changed");
                emit_signal("sources_changed", !sources_changed.empty());
                _queue_update_script_classes();
                first_scan = false;
            }
            if (!is_processing() && scan_changes_pending) {
                scan_changes_pending = false;
                scan_changes();
            }
        } break;
    }
}

bool EditorFileSystem::is_scanning() const {

    return scanning || scanning_changes;
}
float EditorFileSystem::get_scanning_progress() const {

    return scan_total;
}

EditorFileSystemDirectory *EditorFileSystem::get_filesystem() {

    return filesystem;
}

void EditorFileSystem::_save_filesystem_cache(EditorFileSystemDirectory *p_dir, FileAccess *p_file) {

    if (!p_dir)
        return; //none
    p_file->store_line(FormatVE("::%s::%luz",p_dir->get_path().c_str(),p_dir->modified_time));

    for (size_t i = 0; i < p_dir->files.size(); i++) {

        if (!p_dir->files[i]->import_group_file.empty()) {
            group_file_cache.insert(p_dir->files[i]->import_group_file);
        }
        String s = p_dir->files[i]->file + "::" + p_dir->files[i]->type + "::" + ::to_string(p_dir->files[i]->modified_time) +
                   "::" + ::to_string(p_dir->files[i]->import_modified_time) + "::" + ::to_string(p_dir->files[i]->import_valid) +
                   "::" + p_dir->files[i]->import_group_file + "::" + p_dir->files[i]->script_class_name + "<>" +
                   p_dir->files[i]->script_class_extends + "<>" + p_dir->files[i]->script_class_icon_path;
        s += "::";
        for (int j = 0; j < p_dir->files[i]->deps.size(); j++) {

            if (j > 0)
                s += "<>";
            s += p_dir->files[i]->deps[j];
        }

        p_file->store_line(s);
    }

    for (size_t i = 0; i < p_dir->subdirs.size(); i++) {

        _save_filesystem_cache(p_dir->subdirs[i], p_file);
    }
}

bool EditorFileSystem::_find_file(StringView p_file, EditorFileSystemDirectory **r_d, int &r_file_pos) const {
    //todo make faster

    if (!filesystem || scanning)
        return false;

    String f = ProjectSettings::get_singleton()->localize_path(p_file);

    if (!StringUtils::begins_with(f,"res://"))
        return false;
    f = StringUtils::substr(f,6, f.length());
    f = PathUtils::from_native_path(f);

    Vector<StringView> path = StringUtils::split(f,'/');

    if (path.empty())
        return false;
    String file(path[path.size() - 1]);
    path.resize(path.size() - 1);

    EditorFileSystemDirectory *fs = filesystem;

    for (int i = 0; i < path.size(); i++) {

        if (StringUtils::begins_with(path[i],"."))
            return false;

        int idx = -1;
        for (int j = 0; j < fs->get_subdir_count(); j++) {

            if (fs->get_subdir(j)->get_name() == path[i]) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            //does not exist, create i guess?
            EditorFileSystemDirectory *efsd = memnew(EditorFileSystemDirectory);

            efsd->name = path[i];
            efsd->parent = fs;

            int idx2 = 0;
            for (int j = 0; j < fs->get_subdir_count(); j++) {

                if (efsd->name < fs->get_subdir(j)->get_name())
                    break;
                idx2++;
            }

            if (idx2 == fs->get_subdir_count())
                fs->subdirs.push_back(efsd);
            else
                fs->subdirs.insert(fs->subdirs.begin()+idx2, efsd);
            fs = efsd;
        } else {

            fs = fs->get_subdir(idx);
        }
    }

    int cpos = -1;
    for (int i = 0; i < fs->files.size(); i++) {

        if (fs->files[i]->file == file) {
            cpos = i;
            break;
        }
    }

    r_file_pos = cpos;
    *r_d = fs;

    return cpos != -1;
}
//TODO: SEGS: this could return either a naked pointer or a string view.
StringName EditorFileSystem::get_file_type(StringView p_file) const {

    EditorFileSystemDirectory *fs = nullptr;
    int cpos = -1;

    if (!_find_file(p_file, &fs, cpos)) {

        return StringName();
    }

    return fs->files[cpos]->type;
}

EditorFileSystemDirectory *EditorFileSystem::find_file(StringView p_file, int *r_index) const {

    if (!filesystem || scanning)
        return nullptr;

    EditorFileSystemDirectory *fs = nullptr;
    int cpos = -1;
    if (!_find_file(p_file, &fs, cpos)) {

        return nullptr;
    }

    if (r_index)
        *r_index = cpos;

    return fs;
}

EditorFileSystemDirectory *EditorFileSystem::get_filesystem_path(StringView p_path) {

    if (!filesystem || scanning)
        return nullptr;

    String f = ProjectSettings::get_singleton()->localize_path(p_path);

    if (!StringUtils::begins_with(f,"res://"))
        return nullptr;

    f = StringUtils::substr(f,6, f.length());
    f = PathUtils::from_native_path(f);
    if (f.empty())
        return filesystem;

    if (StringUtils::ends_with(f,"/"))
        f = StringUtils::substr(f,0, f.length() - 1);

    Vector<StringView> path = StringUtils::split(f,'/');

    if (path.empty())
        return nullptr;

    EditorFileSystemDirectory *fs = filesystem;

    for (size_t i = 0; i < path.size(); i++) {

        int idx = -1;
        for (int j = 0; j < fs->get_subdir_count(); j++) {

            if (fs->get_subdir(j)->get_name() == path[i]) {
                idx = j;
                break;
            }
        }

        if (idx == -1) {
            return nullptr;
        } else {

            fs = fs->get_subdir(idx);
        }
    }

    return fs;
}

void EditorFileSystem::_save_late_updated_files() {
    //files that already existed, and were modified, need re-scanning for dependencies upon project restart. This is done via saving this special file
    String fscache = PathUtils::plus_file(EditorSettings::get_singleton()->get_project_settings_dir(),"filesystem_update4");
    FileAccessRef f = FileAccess::open(fscache, FileAccess::WRITE);
    ERR_FAIL_COND_MSG(!f, "Cannot create file '" + fscache + "'. Check user write permissions.");
    for (const String &E : late_update_files) {
        f->store_line(E);
    }
}

Vector<String> EditorFileSystem::_get_dependencies(StringView p_path) {

    Vector<String> deps;
    gResourceManager().get_dependencies(p_path, deps);

    return deps;
}

StringName EditorFileSystem::_get_global_script_class(
        StringView p_type, StringView p_path, StringName *r_extends, String *r_icon_path) const {

    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        if (ScriptServer::get_language(i)->handles_global_class_type(p_type)) {
            StringName global_name;
            String extends;
            String icon_path;

            global_name = ScriptServer::get_language(i)->get_global_class_name(p_path, &extends, &icon_path);
            *r_extends = StringName(extends);
            *r_icon_path = icon_path;
            return global_name;
        }
    }
    *r_extends= StringName();
    r_icon_path->clear();
    return StringName();
}

void EditorFileSystem::_scan_script_classes(EditorFileSystemDirectory *p_dir) {
    for (const EditorFileSystemDirectory::FileInfo * fi : p_dir->files) {
        if (fi->script_class_name.empty()) {
            continue;
        }

        StringName lang;
        for (int j = 0; j < ScriptServer::get_language_count(); j++) {
            if (ScriptServer::get_language(j)->handles_global_class_type(fi->type)) {
                lang = ScriptServer::get_language(j)->get_name();
            }
        }
        ScriptServer::add_global_class(fi->script_class_name, fi->script_class_extends, lang, p_dir->get_named_file_path(fi->file));
        EditorNode::get_editor_data().script_class_set_icon_path(fi->script_class_name, fi->script_class_icon_path);
        EditorNode::get_editor_data().script_class_set_name(fi->file, fi->script_class_name);
    }
    for (int i = 0; i < p_dir->get_subdir_count(); i++) {
        _scan_script_classes(p_dir->get_subdir(i));
    }
}

void EditorFileSystem::update_script_classes() {

    if (!update_script_classes_queued.is_set())
        return;

    update_script_classes_queued.clear();
    ScriptServer::global_classes_clear();
    if (get_filesystem()) {
        _scan_script_classes(get_filesystem());
    }

    ScriptServer::save_global_classes();
    EditorNode::get_editor_data().script_class_save_icon_paths();

    // Rescan custom loaders and savers.
    // Doing the following here because the `filesystem_changed` signal fires multiple times and isn't always followed by script classes update.
    // So I thought it's better to do this when script classes really get updated
    gResourceManager().remove_custom_loaders();
    gResourceManager().add_custom_loaders();
    gResourceManager().remove_custom_savers();
    gResourceManager().add_custom_savers();
}

void EditorFileSystem::_queue_update_script_classes() {
    if (update_script_classes_queued.is_set()) {
        return;
    }

    update_script_classes_queued.set();
    call_deferred([this] {update_script_classes();});
}

void EditorFileSystem::update_file(StringView p_file) {

    EditorFileSystemDirectory *fs = nullptr;
    int cpos = -1;

    if (!_find_file(p_file, &fs, cpos)) {

        if (!fs)
            return;
    }

    if (!FileAccess::exists(p_file)) {
        //was removed
        _delete_internal_files(p_file);
        if (cpos != -1) { // Might've never been part of the editor file system (*.* files deleted in Open dialog).
            memdelete(fs->files[cpos]);
            fs->files.erase_at(cpos);
        }
        if (!fs_change_queued) {
            fs_change_queued = true;
            call_deferred([this] {
                emit_signal("filesystem_changed");
                fs_change_queued = false;
            }); // update later
        }
        _queue_update_script_classes();
        return;
    }

    const String type = gResourceManager().get_resource_type(p_file);

    if (cpos == -1) {
        // The file did not exist, it was added.

        late_added_files.insert(p_file); // Remember that it was added. This mean it will be scanned and imported on editor restart.
        int idx = 0;
        String file_name(PathUtils::get_file(p_file));

        for (const auto f : fs->files) {
            if (file_name < f->file) {
                break;
            }
            idx++;
        }

        EditorFileSystemDirectory::FileInfo *fi = memnew(EditorFileSystemDirectory::FileInfo);
        fi->file = file_name;
        fi->import_modified_time = 0;
        fi->import_valid = gResourceManager().is_import_valid(p_file);

        if (idx == fs->files.size()) {
            fs->files.push_back(fi);
        } else {

            fs->files.insert(fs->files.begin()+idx, fi);
        }
        cpos = idx;
    } else {

        //the file exists and it was updated, and was not added in this step.
        //this means we must force upon next restart to scan it again, to get proper type and dependencies
        late_update_files.insert(p_file);
        _save_late_updated_files(); //files need to be updated in the re-scan
    }

    fs->files[cpos]->type = StringName(type);
    fs->files[cpos]->script_class_name = _get_global_script_class(type, p_file, &fs->files[cpos]->script_class_extends, &fs->files[cpos]->script_class_icon_path);
    fs->files[cpos]->import_group_file = gResourceManager().get_import_group_file(p_file);
    fs->files[cpos]->modified_time = FileAccess::get_modified_time(p_file);
    fs->files[cpos]->deps = _get_dependencies(p_file);
    fs->files[cpos]->import_valid = gResourceManager().is_import_valid(p_file);

    // Update preview
    EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
    if (!fs_change_queued) {
        fs_change_queued = true;
        call_deferred([this] {
            emit_signal("filesystem_changed");
            fs_change_queued = false;
        }); // update later
    }
    _queue_update_script_classes();
}

Set<String> EditorFileSystem::get_valid_extensions() const {
    return valid_extensions;
}

Error EditorFileSystem::_reimport_group(StringView p_group_file, const Vector<String> &p_files) {

    String importer_name;

    Map<String, HashMap<StringName, Variant> > source_file_options;
    Map<String, String> base_paths;
    for (int i = 0; i < p_files.size(); i++) {

        Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
        Error err = config->load(p_files[i] + ".import");
        ERR_CONTINUE(err != OK);
        ERR_CONTINUE(!config->has_section_key("remap", "importer"));
        String file_importer_name = config->get_value("remap", "importer").as<String>();
        ERR_CONTINUE(file_importer_name.empty());

        if (!importer_name.empty() && importer_name != file_importer_name) {
            print_line("one importer: " + importer_name + " the other: " + file_importer_name);
            EditorNode::get_singleton()->show_warning(FormatSN(TTR("There are multiple importers for different types pointing to file %.*s, import aborted").asCString(), p_group_file.length(),p_group_file.data()));
            ERR_FAIL_V(ERR_FILE_CORRUPT);
        }

        source_file_options[p_files[i]] = HashMap<StringName, Variant>();
        importer_name = file_importer_name;

        if (importer_name == "keep") {
            continue; //do nothing
        }
        ResourceImporterInterface *importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
        ERR_FAIL_COND_V(importer==nullptr, ERR_FILE_CORRUPT);
        Vector<ResourceImporter::ImportOption> options;
        importer->get_import_options(&options);
        //set default values
        for (const ResourceImporter::ImportOption &E : options) {

            source_file_options[p_files[i]][E.option.name] = E.default_value;
        }

        if (config->has_section("params")) {
            Vector<String> sk = config->get_section_keys("params");
            for (const String &param : sk) {
                Variant value = config->get_value("params", param);
                //override with whatever is in file
                source_file_options[p_files[i]][StringName(param)] = value;
            }
        }

        base_paths[p_files[i]] = ResourceFormatImporter::get_singleton()->get_import_base_path(p_files[i]);
    }

    ERR_FAIL_COND_V(importer_name.empty(), ERR_UNCONFIGURED);
    if (importer_name == "keep") {
        return OK; // (do nothing)
    }

    ResourceImporterInterface *importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);

    Error err = importer->import_group_file(p_group_file, source_file_options, base_paths);

    //all went well, overwrite config files with proper remaps and md5s
    for (eastl::pair<const String, HashMap<StringName, Variant> > &E : source_file_options) {

        const String &file = E.first;
        String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(file);
        FileAccessRef f = FileAccess::open(file + ".import", FileAccess::WRITE);
        ERR_FAIL_COND_V(!f, ERR_FILE_CANT_OPEN);

        //write manually, as order matters ([remap] has to go first for performance).
        f->store_line("[remap]");
        f->store_line("");
        f->store_line(String("importer=\"") + importer->get_importer_name() + "\"");
        if (!importer->get_resource_type().empty()) {
            f->store_line(String("type=\"") + importer->get_resource_type() + "\"");
        }

        Vector<String> dest_paths;

        if (err == OK) {
            String path = base_path + "." + importer->get_save_extension();
            f->store_line("path=\"" + path + "\"");
            dest_paths.push_back(path);
        }

        f->store_line("group_file=" + Variant(p_group_file).get_construct_string());

        if (err == OK) {
            f->store_line("valid=true");
        } else {
            f->store_line("valid=false");
        }
        f->store_line("[deps]\n");

        f->store_line("");

        f->store_line("source_file=" + Variant(file).get_construct_string());
        if (!dest_paths.empty()) {
            Array dp;
            for (size_t i = 0; i < dest_paths.size(); i++) {
                dp.push_back(dest_paths[i]);
            }
            f->store_line("dest_files=" + Variant(dp).get_construct_string() + "\n");
        }
        f->store_line("[params]");
        f->store_line("");

        //store options in provided order, to avoid file changing. Order is also important because first match is accepted first.

        Vector<ResourceImporter::ImportOption> options;
        importer->get_import_options(&options);
        //set default values
        for (const ResourceImporter::ImportOption &F : options) {

            StringName base(F.option.name);
            Variant v = F.default_value;
            if (source_file_options[file].contains(base)) {
                v = source_file_options[file][base];
            }
            String value;
            VariantWriter::write_to_string(v, value);
            f->store_line(String(base) + "=" + value);
        }

        f->close();

        // Store the md5's of the various files. These are stored separately so that the .import files can be version controlled.
        FileAccessRef md5s = FileAccess::open(base_path + ".md5", FileAccess::WRITE);
        ERR_FAIL_COND_V_MSG(!md5s, ERR_FILE_CANT_OPEN, "Cannot open MD5 file '" + base_path + ".md5'.");

        md5s->store_line("source_md5=\"" + FileAccess::get_md5(file) + "\"");
        if (!dest_paths.empty()) {
            md5s->store_line("dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths) + "\"\n");
        }
        md5s->close();

        EditorFileSystemDirectory *fs = nullptr;
        int cpos = -1;
        bool found = _find_file(file, &fs, cpos);
        ERR_FAIL_COND_V_MSG(!found, ERR_UNCONFIGURED, "Can't find file '" + file + "'.");

        //update modified times, to avoid reimport
        fs->files[cpos]->modified_time = FileAccess::get_modified_time(file);
        fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(file + ".import");
        fs->files[cpos]->deps = _get_dependencies(file);
        fs->files[cpos]->type = importer->get_resource_type();
        fs->files[cpos]->import_valid = err == OK;

        //if file is currently up, maybe the source it was loaded from changed, so import math must be updated for it
        //to reload properly
        if (ResourceCache::has(file)) {

            Resource *r = ResourceCache::get(file);

            if (!ResourceTooling::get_import_path(r).empty()) {

                String dst_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(file);
                ResourceTooling::set_import_path(r,dst_path);
                ResourceTooling::set_import_last_modified_time(r,0);
            }
        }

        EditorResourcePreview::get_singleton()->check_for_invalidation(file);
    }

    return err;
}

Error EditorFileSystem::_reimport_file(const String &p_file, Vector<String> &r_missing_deps, bool final_try) {

    EditorFileSystemDirectory *fs = nullptr;
    int cpos = -1;
    bool found = _find_file(p_file, &fs, cpos);
    ERR_FAIL_COND_V_MSG(!found, ERR_FILE_CANT_OPEN, "Can't find file '" + p_file + "'.");

    //try to obtain existing params

    HashMap<StringName, Variant> params;
    String importer_name;

    if (FileAccess::exists(p_file + ".import")) {
        //use existing
        Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
        Error err = cf->load(p_file + ".import");
        if (err == OK) {
            if (cf->has_section("params")) {
                Vector<String> sk = cf->get_section_keys("params");
                for (const String &E : sk) {
                    params[StringName(E)] = cf->get_value("params", E);
                }
            }
            if (cf->has_section("remap")) {
                importer_name = cf->get_value("remap", "importer").as<String>();
            }
        }

    } else {
        late_added_files.insert(p_file); //imported files do not call update_file(), but just in case..
        params["nodes/use_legacy_names"] = false;
    }
    if (importer_name == "keep") {
        //keep files, do nothing.
        fs->files[cpos]->modified_time = FileAccess::get_modified_time(p_file);
        fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(p_file + ".import");
        fs->files[cpos]->deps.clear();
        fs->files[cpos]->type = "";
        fs->files[cpos]->import_valid = false;
        EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
        return OK;
    }

    ResourceImporterInterface *importer=nullptr;
    bool load_default = false;
    //find the importer
    if (!importer_name.empty()) {
        importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
    }

    if (importer==nullptr) {
        //not found by name, find by extension
        importer = ResourceFormatImporter::get_singleton()->get_importer_by_extension(PathUtils::get_extension(p_file));
        load_default = true;
        if (importer==nullptr) {
            ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, "BUG: File queued for import, but can't be imported!");
        }
    }

    //mix with default params, in case a parameter is missing

    Vector<ResourceImporter::ImportOption> opts;
    importer->get_import_options(&opts);
    for (const ResourceImporter::ImportOption &E : opts) {
        if (!params.contains(E.option.name)) { //this one is not present
            params[E.option.name] = E.default_value;
        }
    }

    if (load_default && ProjectSettings::get_singleton()->has_setting(StringName(String("importer_defaults/") + importer->get_importer_name()))) {
        //use defaults if exist
        Dictionary d = ProjectSettings::get_singleton()->get(StringName(String("importer_defaults/") + importer->get_importer_name())).as<Dictionary>();
        auto v(d.get_key_list());

        for (const auto &E : v) {
            params[E] = d[E];
        }
    }

    //finally, perform import!!
    String base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(p_file);

    Vector<String> import_variants;
    Vector<String> gen_files;

    Variant metadata;
    Error err = importer->import(p_file, base_path, params, r_missing_deps,&import_variants, &gen_files, &metadata);

    if (err != OK) {
        ERR_PRINT("Error importing '" + p_file + "'.");
        if(err==ERR_FILE_MISSING_DEPENDENCIES && !final_try) {
            return ERR_FILE_MISSING_DEPENDENCIES;
        }
    }

    //as import is complete, save the .import file

    FileAccess *f = FileAccess::open(p_file + ".import", FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!f,ERR_FILE_CANT_WRITE, "Cannot open file from path '" + p_file + ".import'.");

    //write manually, as order matters ([remap] has to go first for performance).
    f->store_line("[remap]");
    f->store_line("");
    f->store_line(String("importer=\"") + importer->get_importer_name() + "\"");
    if (!importer->get_resource_type().empty()) {
        f->store_line(String("type=\"") + importer->get_resource_type() + "\"");
    }

    Vector<String> dest_paths;

    if (err == OK) {

        if (importer->get_save_extension().empty()) {
            //no path
        } else if (!import_variants.empty()) {
            //import with variants
            for (const String &E : import_variants) {

                String path = StringUtils::c_escape(base_path) + "." + E + "." + importer->get_save_extension();

                f->store_line("path." + E + "=\"" + path + "\"");
                dest_paths.push_back(path);
            }
        } else {
            String path = base_path + "." + importer->get_save_extension();
            f->store_line("path=\"" + path + "\"");
            dest_paths.push_back(path);
        }

    } else {

        f->store_line("valid=false");
    }

    if (metadata != Variant()) {
        f->store_line("metadata=" + metadata.get_construct_string());
    }

    f->store_line("");

    f->store_line("[deps]\n");

    if (!gen_files.empty()) {
        Array genf;
        for (const String &E : gen_files) {
            genf.push_back(E);
            dest_paths.push_back(E);
        }

        String value;
        VariantWriter::write_to_string(genf, value);
        f->store_line("files=" + value);
        f->store_line("");
    }

    f->store_line("source_file=" + Variant(p_file).get_construct_string());

    if (!dest_paths.empty()) {
        Array dp;
        for (size_t i = 0; i < dest_paths.size(); i++) {
            dp.push_back(dest_paths[i]);
        }
        f->store_line("dest_files=" + Variant(dp).get_construct_string() + "\n");
    }

    f->store_line("[params]");
    f->store_line("");

    //store options in provided order, to avoid file changing. Order is also important because first match is accepted first.

    for (const ResourceImporter::ImportOption &E : opts) {

        StringName base(E.option.name);
        String value;
        VariantWriter::write_to_string(params[base], value);
        f->store_line(String(base) + "=" + value);
    }

    f->close();
    memdelete(f);

    // Store the md5's of the various files. These are stored separately so that the .import files can be version controlled.
    FileAccess *md5s = FileAccess::open(base_path + ".md5", FileAccess::WRITE);
    ERR_FAIL_COND_V(!md5s,ERR_FILE_CANT_WRITE);
    md5s->store_line("source_md5=\"" + FileAccess::get_md5(p_file) + "\"");
    if (!dest_paths.empty()) {
        md5s->store_line("dest_md5=\"" + FileAccess::get_multiple_md5(dest_paths) + "\"\n");
    }
    md5s->close();
    memdelete(md5s);

    //update modified times, to avoid reimport
    fs->files[cpos]->modified_time = FileAccess::get_modified_time(p_file);
    fs->files[cpos]->import_modified_time = FileAccess::get_modified_time(p_file + ".import");
    fs->files[cpos]->deps = _get_dependencies(p_file);
    fs->files[cpos]->type = importer->get_resource_type();
    fs->files[cpos]->import_valid = gResourceManager().is_import_valid(p_file);

    //if file is currently up, maybe the source it was loaded from changed, so import math must be updated for it
    //to reload properly
    if (ResourceCache::has(p_file)) {

        Resource *r = ResourceCache::get(p_file);

        if (!ResourceTooling::get_import_path(r).empty()) {

            String dst_path = ResourceFormatImporter::get_singleton()->get_internal_resource_path(p_file);
            ResourceTooling::set_import_path(r,dst_path);
            ResourceTooling::set_import_last_modified_time(r,0);
        }
    }

    EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
    return OK;
}

void EditorFileSystem::_find_group_files(EditorFileSystemDirectory *efd, Map<String, Vector<String> > &group_files, Set<String> &groups_to_reimport) {

    for (const EditorFileSystemDirectory::FileInfo * fi : efd->files) {
        if (groups_to_reimport.contains(fi->import_group_file)) {
            group_files[fi->import_group_file].push_back(efd->get_named_file_path(fi->file));
        }
    }

    for (int i = 0; i < efd->get_subdir_count(); i++) {
        _find_group_files(efd->get_subdir(i), group_files, groups_to_reimport);
    }
}
// Find the order the give set of files need to be imported in, taking into account dependencies between resources.
void EditorFileSystem::ordered_reimport(EditorProgress &pr, Vector<ImportFile> &files) {
    eastl::sort(files.begin(),files.end());
    //TODO: use slab allocator here, and just 'forget' all deallocations.
    HashMap<String, HashSet<String>> missing_deps;
    HashSet<String> correct_imports;

    correct_imports.reserve(files.size());
    gResourceManager().set_save_callback_pause(true);
    int idx=0;
    // At the beginning we don't know cross-resource dependencies, so we go linearly
    for (const auto & fi : files) {
        pr.step(StringName(PathUtils::get_file(fi.path)), idx);
        Vector<String> deps;

        auto err = _reimport_file(fi.path, deps);

        if (err == OK) {
            idx++; // count success as progress
            correct_imports.insert(fi.path);
        }
        else if(ERR_FILE_MISSING_DEPENDENCIES==err) {
            // This path is missing those dependencies:
            missing_deps[fi.path].insert(eastl::make_move_iterator(deps.begin()), eastl::make_move_iterator(deps.end()));
        }
    }
    if (missing_deps.empty()) {
        gResourceManager().set_save_callback_pause(false);
        return;
    }
    OS::get_singleton()->print("Missing deps:");
    Vector<String> ordered_imports;
    //NOTE: this should probably use graph theoretic algorithms -> detect cycles + topological sort
    // 1. Remove dependent files that were loaded after files that needed them.
    for(auto iter=missing_deps.begin(); iter!= missing_deps.end(); ) {
        for(auto iter2=iter->second.begin(); iter2!= iter->second.end();) {
            OS::get_singleton()->print(FormatVE("    %s\n", iter2->c_str()));
            if(correct_imports.contains(*iter2)) { // got it !
                iter2 = iter->second.erase(iter2);
            }
            else
                ++iter2;
        }
        if(iter->second.empty()) {
            ordered_imports.push_back(iter->first);
            iter = missing_deps.erase(iter);
        }
        else
            ++iter;
    }
    // Loop until we have all ordered, or can't add new part to ordered_imports
    size_t start_of_chunk=0;
    size_t end_of_chunk= ordered_imports.size();

    while(!missing_deps.empty()) {
        for (auto iter = missing_deps.begin(); iter != missing_deps.end(); ) {
            Span<const String> last_chunk(ordered_imports.data() + start_of_chunk, end_of_chunk - start_of_chunk);
            // Remove what's already on the list from deps.
            for (auto iter2 = iter->second.begin(); iter2 != iter->second.end();) {
                if (last_chunk.end()!=eastl::find(last_chunk.begin(), last_chunk.end(),*iter2)) { // got it !
                    iter2 = iter->second.erase(iter2);
                }
                else
                    ++iter2;
            }
            if (iter->second.empty()) {
                ordered_imports.push_back(iter->first);
                iter = missing_deps.erase(iter);
            }
            else
                ++iter;
        }
        if(end_of_chunk==ordered_imports.size())
            break; // can't reduce anymore ?
        start_of_chunk = end_of_chunk;
        end_of_chunk = ordered_imports.size();
    }
    for (const auto & fi : ordered_imports) {
        pr.step(StringName(PathUtils::get_file(fi)), idx);
        Vector<String> deps;

        auto err = _reimport_file(fi, deps,true); // marked as final try, since we want those files to be marked as failed in this case.

        if (err == OK) {
            idx++; // count success as progress
        }
    }
    // mark the last missing deps by calling _reimport_file with final_try set
    for(const auto &f : missing_deps) {
        Vector<String> deps;
        _reimport_file(f.first,deps,true);
    }
    gResourceManager().set_save_callback_pause(false);
}

void EditorFileSystem::_create_project_data_dir_if_necessary()
{
    // Check that the project data directory exists
    DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    String project_data_path = ProjectSettings::get_singleton()->get_project_data_path();
    if (da->change_dir(project_data_path) != OK) {
        Error err = da->make_dir(project_data_path);
            if (err) {
                memdelete(da);
            ERR_FAIL_MSG("Failed to create folder " + project_data_path);
            }
        }
        memdelete(da);
    // Check that the project data directory '.gdignore' file exists
    String project_data_gdignore_file_path = PathUtils::plus_file(project_data_path,".gdignore");
    if (!FileAccess::exists(project_data_gdignore_file_path)) {
        // Add an empty .gdignore file to avoid scan.
        FileAccessRef f = FileAccess::open(project_data_gdignore_file_path, FileAccess::WRITE);
        if (f) {
            f->store_line("");
            f->close();
        } else {
            ERR_FAIL_MSG("Failed to create file " + project_data_gdignore_file_path);
    }
    }
}

void EditorFileSystem::reimport_files(const Vector<String> &p_files) {

    _create_project_data_dir_if_necessary();

    importing = true;
    EditorProgress pr(("reimport"), TTR("(Re)Importing Assets"), p_files.size());

    Vector<ImportFile> files;
    Set<String> groups_to_reimport;

    for (const auto &p_file : p_files) {

        String group_file = ResourceFormatImporter::get_singleton()->get_import_group_file(p_file);

        if (group_file_cache.contains(p_file)) {
            //maybe the file itself is a group!
            groups_to_reimport.insert(p_file);
            //groups do not belong to grups
            group_file.clear();
        } else if (!group_file.empty()) {
            //it's a group file, add group to import and skip this file
            groups_to_reimport.insert(group_file);
        } else {
            //it's a regular file
            ImportFile ifile;
            ifile.path = p_file;
            ifile.order = ResourceFormatImporter::get_singleton()->get_import_order(p_file);
            files.push_back(ifile);
        }

        //group may have changed, so also update group reference
        EditorFileSystemDirectory *fs = nullptr;
        int cpos = -1;
        if (_find_file(p_file, &fs, cpos)) {

            fs->files[cpos]->import_group_file = group_file;
        }
    }

    ordered_reimport(pr, files);

    //reimport groups

    if (!groups_to_reimport.empty()) {
        Map<String, Vector<String> > group_files;
        _find_group_files(filesystem, group_files, groups_to_reimport);
        for (eastl::pair<const String,Vector<String> > &E : group_files) {

            Error err = _reimport_group(E.first, E.second);
            if (err == OK) {
                Vector<String> missing_deps;
                _reimport_file(E.first, missing_deps,true);
            }
        }
    }

    _save_filesystem_cache();
    importing = false;
    if (!is_scanning()) {
        emit_signal("filesystem_changed");
    }

    emit_signal("resources_reimported", Variant::from(p_files));
}

Error EditorFileSystem::_resource_import(StringView p_path) {

    Vector<String> files { String(p_path) };

    singleton->update_file(p_path);
    singleton->reimport_files(files);

    return OK;
}

bool EditorFileSystem::is_group_file(StringView p_path) const {
    return group_file_cache.contains_as(p_path);
}

void EditorFileSystem::_move_group_files(EditorFileSystemDirectory *efd, StringView p_group_file, StringView p_new_location) {

    for (EditorFileSystemDirectory::FileInfo * fi : efd->files) {

        if (fi->import_group_file == p_group_file) {

            fi->import_group_file = p_new_location;

            Ref<ConfigFile> config(make_ref_counted<ConfigFile>());
            String path = efd->get_named_file_path(fi->file) + ".import";
            Error err = config->load(path);
            if (err != OK) {
                continue;
            }
            if (config->has_section_key("remap", "group_file")) {

                config->set_value("remap", "group_file", p_new_location);
            }

            Vector<String> sk = config->get_section_keys("params");
            for (const String & param : sk) {
                //not very clean, but should work
                String value = config->get_value("params", param).as<String>();
                if (value == p_group_file) {
                    config->set_value("params", param, p_new_location);
                }
            }

            config->save(path);
        }
    }

    for (int i = 0; i < efd->get_subdir_count(); i++) {
        _move_group_files(efd->get_subdir(i), p_group_file, p_new_location);
    }
}

void EditorFileSystem::move_group_file(StringView p_path, StringView p_new_path) {

    if (get_filesystem()) {
        _move_group_files(get_filesystem(), p_path, p_new_path);
        auto iter=group_file_cache.find_as(p_path);
        if (iter!=group_file_cache.end()) {
            group_file_cache.erase(iter);
            group_file_cache.insert(p_new_path);
        }
    }
}

void EditorFileSystem::_bind_methods() {

    SE_BIND_METHOD(EditorFileSystem,get_filesystem);
    SE_BIND_METHOD(EditorFileSystem,is_scanning);
    SE_BIND_METHOD(EditorFileSystem,get_scanning_progress);
    SE_BIND_METHOD(EditorFileSystem,scan);
    SE_BIND_METHOD(EditorFileSystem,scan_changes);
    SE_BIND_METHOD(EditorFileSystem,update_file);
    SE_BIND_METHOD(EditorFileSystem,get_filesystem_path);
    SE_BIND_METHOD(EditorFileSystem,get_file_type);
    SE_BIND_METHOD(EditorFileSystem,update_script_classes);

    ADD_SIGNAL(MethodInfo("filesystem_changed"));
    ADD_SIGNAL(MethodInfo("sources_changed", PropertyInfo(VariantType::BOOL, "exist")));
    ADD_SIGNAL(MethodInfo("resources_reimported", PropertyInfo(VariantType::POOL_STRING_ARRAY, "resources")));
    ADD_SIGNAL(MethodInfo("resources_reload", PropertyInfo(VariantType::POOL_STRING_ARRAY, "resources")));
}

void EditorFileSystem::_update_extensions() {

    valid_extensions.clear();
    import_extensions.clear();

    Vector<String> tmp_extensions;
    gResourceManager().get_recognized_extensions_for_type("", tmp_extensions);
    for (String &E : tmp_extensions) {

        valid_extensions.emplace(eastl::move(E));
    }

    tmp_extensions.clear();
    ResourceFormatImporter::get_singleton()->get_recognized_extensions(tmp_extensions);
    for (String &E : tmp_extensions) {

        import_extensions.emplace(eastl::move(E));
    }
}

EditorFileSystem::EditorFileSystem() {
    g_import_func = _resource_import;
    reimport_on_missing_imported_files = T_GLOBAL_DEF("editor/reimport_missing_imported_files", true);

    singleton = this;
    filesystem = memnew(EditorFileSystemDirectory); //like, empty
    filesystem->parent = nullptr;

    scanning = false;
    importing = false;
    use_threads = true;
    new_filesystem = nullptr;

    abort_scan = false;
    scanning_changes = false;
    scanning_changes_done = false;

    _create_project_data_dir_if_necessary();

    // This should probably also work on Unix and use the string it returns for FAT32 or exFAT
    DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    String fstype = da->get_filesystem_type();
    using_fat32_or_exfat = fstype == "FAT32" || fstype == "exFAT";
    memdelete(da);

    scan_total = 0;
    first_scan = true;
    scan_changes_pending = false;
    revalidate_import_files = false;
}

EditorFileSystem::~EditorFileSystem() {
}
