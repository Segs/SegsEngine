/*************************************************************************/
/*  editor_resource_preview.cpp                                          */
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

#include "editor_resource_preview.h"

#include "core/script_language.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"

#include "core/method_bind.h"
#include "core/callable_method_pointer.h"
#include "core/io/resource_loader.h"
#include "core/resource/resource_manager.h"
#include "core/message_queue.h"
#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/project_settings.h"

#include "servers/rendering_server.h"

IMPL_GDCLASS(EditorResourcePreviewGenerator)
IMPL_GDCLASS(EditorResourcePreview)
uint32_t hash_edited_version(const Resource &resource) {
    uint32_t hash = hash_djb2_one_32(resource.get_tooling_interface()->get_edited_version());

    Vector<PropertyInfo> plist;
    resource.get_property_list(&plist);

    for (PropertyInfo &E : plist) {
        if (E.usage & PROPERTY_USAGE_STORAGE && E.type == VariantType::OBJECT && E.hint == PropertyHint::ResourceType) {
            RES res(refFromVariant<Resource>(resource.get(E.name)));
            if (res) {
                hash = hash_djb2_one_32(hash_edited_version(*res), hash);
            }
        }
    }

    return hash;
}

bool EditorResourcePreviewGenerator::handles(StringView p_type) const {

    if (get_script_instance() && get_script_instance()->has_method("handles")) {
        return get_script_instance()->call("handles", p_type).as<bool>();
    }
    ERR_FAIL_V_MSG(false, "EditorResourcePreviewGenerator::handles needs to be overridden.");
}

Ref<Texture> EditorResourcePreviewGenerator::generate(const RES &p_from, const Size2 &p_size) const {

    if (get_script_instance() && get_script_instance()->has_method("generate")) {
        return refFromVariant<Texture>(get_script_instance()->call("generate", p_from, p_size));
    }
    ERR_FAIL_V_MSG(Ref<Texture>(), "EditorResourcePreviewGenerator::generate needs to be overridden.");
}

Ref<Texture> EditorResourcePreviewGenerator::generate_from_path(StringView p_path, const Size2 &p_size) const {

    if (get_script_instance() && get_script_instance()->has_method("generate_from_path")) {
        return refFromVariant<Texture>(get_script_instance()->call("generate_from_path", p_path, p_size));
    }

    RES res(gResourceManager().load(p_path));
    if (not res)
        return Ref<Texture>();
    return generate(res, p_size);
}

bool EditorResourcePreviewGenerator::generate_small_preview_automatically() const {

    if (get_script_instance() && get_script_instance()->has_method("generate_small_preview_automatically")) {
        return get_script_instance()->call("generate_small_preview_automatically").as<bool>();
    }

    return false;
}

bool EditorResourcePreviewGenerator::can_generate_small_preview() const {

    if (get_script_instance() && get_script_instance()->has_method("can_generate_small_preview")) {
        return get_script_instance()->call("can_generate_small_preview").as<bool>();
    }

    return false;
}

void EditorResourcePreviewGenerator::_bind_methods() {
    ClassDB::add_virtual_method(get_class_static_name(),
            MethodInfo(VariantType::BOOL, "handles", PropertyInfo(VariantType::STRING, "type")));

    ClassDB::add_virtual_method(get_class_static_name(),
            MethodInfo(CLASS_INFO(Texture), "generate",
                    PropertyInfo(VariantType::OBJECT, "from", PropertyHint::ResourceType, "Resource"),
                    PropertyInfo(VariantType::VECTOR2, "size")));

    ClassDB::add_virtual_method(
            get_class_static_name(), MethodInfo(CLASS_INFO(Texture), "generate_from_path",
                                             PropertyInfo(VariantType::STRING, "path", PropertyHint::File),
                                             PropertyInfo(VariantType::VECTOR2, "size")));

    ClassDB::add_virtual_method(
            get_class_static_name(), MethodInfo(VariantType::BOOL, "generate_small_preview_automatically"));
    ClassDB::add_virtual_method(get_class_static_name(), MethodInfo(VariantType::BOOL, "can_generate_small_preview"));
}

EditorResourcePreviewGenerator::EditorResourcePreviewGenerator() {
}

EditorResourcePreview *EditorResourcePreview::singleton = nullptr;

void EditorResourcePreview::_thread_func(void *ud) {

    EditorResourcePreview *erp = (EditorResourcePreview *)ud;
    erp->_thread();
}

void EditorResourcePreview::_preview_ready(StringView p_str, const Ref<Texture> &p_texture, const Ref<Texture> &p_small_texture, const Callable &callit) {

    preview_mutex.lock();

    String path(p_str);
    uint32_t hash = 0;
    uint64_t modified_time = 0;

    if (StringUtils::begins_with(p_str,"ID:")) {
        hash = uint32_t(StringUtils::to_int64(StringUtils::get_slice(p_str,':', 2)));
        path = String("ID:") + StringUtils::get_slice(p_str,':', 1);
    } else {
        modified_time = FileAccess::get_modified_time(path);
    }

    Item item;
    item.order = order++;
    item.preview = p_texture;
    item.small_preview = p_small_texture;
    item.last_hash = hash;
    item.modified_time = modified_time;

    cache[path] = item;

    preview_mutex.unlock();

    MessageQueue::get_singleton()->push_callable(callit, path, p_texture, p_small_texture);
}

void EditorResourcePreview::_generate_preview(Ref<ImageTexture> &r_texture, Ref<ImageTexture> &r_small_texture, const QueueItem &p_item, StringView cache_base) {
    String type;

    if (p_item.resource)
        type = p_item.resource->get_class();
    else
        type = gResourceManager().get_resource_type(p_item.path);

    if (type.empty()) {
        r_texture = Ref<ImageTexture>();
        r_small_texture = Ref<ImageTexture>();
        return; //could not guess type
    }

    int thumbnail_size = EditorSettings::get_singleton()->getT<int>("filesystem/file_dialog/thumbnail_size");
    thumbnail_size *= EDSCALE;

    r_texture = Ref<ImageTexture>();
    r_small_texture = Ref<ImageTexture>();

    for (Ref<EditorResourcePreviewGenerator>& preview_generator : preview_generators) {
        if (!preview_generator->handles(type))
            continue;

        Ref<Texture> generated;
        if (p_item.resource) {
            generated = preview_generator->generate(p_item.resource, Vector2(thumbnail_size, thumbnail_size));
        } else {
            generated = preview_generator->generate_from_path(p_item.path, Vector2(thumbnail_size, thumbnail_size));
        }
        r_texture = dynamic_ref_cast<ImageTexture>(generated);

        if (!EditorNode::get_singleton()->get_theme_base())
            return;

        int small_thumbnail_size = EditorNode::get_singleton()->get_theme_base()->get_theme_icon("Object", "EditorIcons")->get_width(); // Kind of a workaround to retrieve the default icon size

        if (preview_generator->can_generate_small_preview()) {
            Ref<Texture> generated_small;
            if (p_item.resource) {
                generated_small = preview_generator->generate(p_item.resource, Vector2(small_thumbnail_size, small_thumbnail_size));
            } else {
                generated_small = preview_generator->generate_from_path(p_item.path, Vector2(small_thumbnail_size, small_thumbnail_size));
            }
            r_small_texture = dynamic_ref_cast<ImageTexture>(generated_small);
        }

        if (not r_small_texture && r_texture && preview_generator->generate_small_preview_automatically()) {
            Ref<Image> small_image = r_texture->get_data();
            small_image = dynamic_ref_cast<Image>(small_image->duplicate());
            small_image->resize(small_thumbnail_size, small_thumbnail_size, Image::INTERPOLATE_CUBIC);
            r_small_texture = make_ref_counted<ImageTexture>();
            r_small_texture->create_from_image(small_image);
        }

        break;
    }

    if (not p_item.resource) {
        // cache the preview in case it's a resource on disk
        if (r_texture) {
            //wow it generated a preview... save cache
            bool has_small_texture = r_small_texture;
            gResourceManager().save(String(cache_base) + ".png", r_texture);
            if (has_small_texture) {
                gResourceManager().save(String(cache_base) + "_small.png", r_small_texture);
            }
            FileAccess *f = FileAccess::open(String(cache_base) + ".txt", FileAccess::WRITE);
            ERR_FAIL_COND_MSG(!f, "Cannot create file '" + cache_base + ".txt'. Check user write permissions.");
            f->store_line(itos(thumbnail_size));
            f->store_line(itos(has_small_texture));
            f->store_line(itos(FileAccess::get_modified_time(p_item.path)));
            f->store_line(FileAccess::get_md5(p_item.path));
            f->close();
            memdelete(f);
        }
    }
}

void EditorResourcePreview::_thread() {

#ifndef SERVER_ENABLED
    exited.clear();
    while (!exit.is_set()) {

        preview_sem.wait();
        preview_mutex.lock();

        if (queue.empty()) {
            preview_mutex.unlock();
            continue;
        }

        QueueItem item = eastl::move(queue.front());
        queue.pop_front();

        if (cache.contains(item.path)) {
            //already has it because someone loaded it, just let it know it's ready
            String path = item.path;
            if (item.resource) {
                path += ":" + itos(cache[item.path].last_hash); //keep last hash (see description of what this is in condition below)
            }

            _preview_ready(path, cache[item.path].preview, cache[item.path].small_preview, item.callable);

            preview_mutex.unlock();
        } else {

            preview_mutex.unlock();

            Ref<ImageTexture> texture;
            Ref<ImageTexture> small_texture;

            int thumbnail_size = EditorSettings::get_singleton()->getT<int>("filesystem/file_dialog/thumbnail_size");
            thumbnail_size *= EDSCALE;

            if (item.resource) {

                _generate_preview(texture, small_texture, item, {});

                //adding hash to the end of path (should be ID:<objid>:<hash>) because of 5 argument limit to call_deferred
                _preview_ready(item.path + ":" + itos(hash_edited_version(*item.resource)), texture, small_texture,
                        item.callable);

            } else {

                String temp_path = EditorSettings::get_singleton()->get_cache_dir();
                String cache_base(StringUtils::md5_text(ProjectSettings::get_singleton()->globalize_path(item.path)));
                cache_base = PathUtils::plus_file(temp_path,"resthumb-" + cache_base);

                //does not have it, try to load a cached thumbnail

                String file = cache_base + ".txt";
                FileAccess *f = FileAccess::open(file, FileAccess::READ);
                if (!f) {

                    // No cache found, generate
                    _generate_preview(texture, small_texture, item, cache_base);
                } else {

                    uint64_t modtime = FileAccess::get_modified_time(item.path);
                    int tsize = StringUtils::to_int64(f->get_line());
                    bool has_small_texture = StringUtils::to_int(f->get_line());
                    uint64_t last_modtime = StringUtils::to_int64(f->get_line());

                    bool cache_valid = true;

                    if (tsize != thumbnail_size) {

                        cache_valid = false;
                        memdelete(f);
                    } else if (last_modtime != modtime) {

                        String last_md5 = f->get_line();
                        String md5 = FileAccess::get_md5(item.path);
                        memdelete(f);

                        if (last_md5 != md5) {

                            cache_valid = false;

                        } else {
                            //update modified time

                            f = FileAccess::open(file, FileAccess::WRITE);
                            if (!f) {
                                // Not returning as this would leave the thread hanging and would require
                                // some proper cleanup/disabling of resource preview generation.
                                ERR_PRINT("Cannot create file '" + file + "'. Check user write permissions.");
                            } else {
                                f->store_line(itos(thumbnail_size));
                                f->store_line(itos(has_small_texture));
                                f->store_line(itos(modtime));
                                f->store_line(md5);
                                memdelete(f);
                            }
                        }
                    } else {
                        memdelete(f);
                    }

                    if (cache_valid) {

                        Ref<Image> img(make_ref_counted<Image>());
                        Ref<Image> small_img(make_ref_counted<Image>());

                        if (img->load(cache_base + ".png") != OK) {
                            cache_valid = false;
                        } else {

                            texture = make_ref_counted<ImageTexture>();
                            texture->create_from_image(img, Texture::FLAG_FILTER);

                            if (has_small_texture) {
                                if (small_img->load(cache_base + "_small.png") != OK) {
                                    cache_valid = false;
                                } else {
                                    small_texture = make_ref_counted<ImageTexture>();
                                    small_texture->create_from_image(small_img, Texture::FLAG_FILTER);
                                }
                            }
                        }
                    }

                    if (!cache_valid) {

                        _generate_preview(texture, small_texture, item, cache_base);
                    }
                }
                _preview_ready(item.path, texture, small_texture, item.callable);
            }
        }
    }
#endif
    exited.set();
}
//TODO: make this function take a eastl::function<void(Variant)>, would need to support c# delegate to eastl::function wrapping.
void EditorResourcePreview::queue_edited_resource_preview(const Ref<Resource> &p_res, const Callable &entry) {
    ERR_FAIL_NULL(entry.get_object());
    ERR_FAIL_COND(not p_res);

    preview_mutex.lock();

    String path_id = "ID:" + itos(entt::to_integral(p_res->get_instance_id()));

    if (cache.contains(path_id) && cache[path_id].last_hash == hash_edited_version(*p_res)) {

        cache[path_id].order = order++;
        Variant args[] = {
            path_id, cache[path_id].preview, cache[path_id].small_preview
        };
        const Variant *pargs[] = { &args[0],&args[1],&args[2]};
        Variant res;
        Callable::CallError ce;
        entry.call(pargs,3,res,ce);
        preview_mutex.unlock();
        return;
    }

    cache.erase(path_id); //erase if exists, since it will be regen

    QueueItem item;
    item.callable = entry;
    item.resource = p_res;
    item.path = path_id;

    queue.emplace_back(item);
    preview_mutex.unlock();
    preview_sem.post();
}

void EditorResourcePreview::queue_edited_resource_preview_lambda(const Ref<Resource> &p_res, Object *owner, eastl::function<void (const String &, const Ref<Texture> &, const Ref<Texture> &)> &&cb)
{
    ERR_FAIL_NULL(owner);
    ERR_FAIL_COND(not p_res);

    preview_mutex.lock();

    String path_id = "ID:" + itos(entt::to_integral(p_res->get_instance_id()));

    if (cache.contains(path_id) && cache[path_id].last_hash == hash_edited_version(*p_res)) {

        cache[path_id].order = order++;
        cb(path_id, cache[path_id].preview, cache[path_id].small_preview);
        preview_mutex.unlock();
        return;
    }

    cache.erase(path_id); //erase if exists, since it will be regen

    QueueItem item;
    item.callable = callable_gen(owner,cb);
    item.resource = p_res;
    item.path = path_id;

    queue.emplace_back(item);
    preview_mutex.unlock();
    preview_sem.post();
}

void EditorResourcePreview::queue_resource_preview(StringView p_path, const Callable &callback) {

    ERR_FAIL_NULL(callback.get_object());
    preview_mutex.lock();
    if (cache.contains_as(p_path)) {
        auto & entry(cache[String(p_path)]);
        entry.order = order++;
        Variant args[] = {
            p_path, entry.preview, entry.small_preview
        };
        const Variant *pargs[] = { &args[0],&args[1],&args[2] };
        Variant res;
        Callable::CallError ce;
        callback.call(pargs,3,res,ce);
        preview_mutex.unlock();
        return;
    }

    QueueItem item;
    item.callable = callback;
    item.path = p_path;

    queue.emplace_back(item);
    preview_mutex.unlock();
    preview_sem.post();
}

void EditorResourcePreview::add_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator) {

    preview_generators.push_back(p_generator);
}

void EditorResourcePreview::remove_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator) {
    auto iter = eastl::find(preview_generators.begin(), preview_generators.end(), p_generator);
    if(iter!= preview_generators.end())
        preview_generators.erase(iter);
}

EditorResourcePreview *EditorResourcePreview::get_singleton() {

    return singleton;
}

void EditorResourcePreview::_bind_methods() {

    MethodBinder::bind_method("_preview_ready", &EditorResourcePreview::_preview_ready);

    SE_BIND_METHOD(EditorResourcePreview,queue_resource_preview);
    SE_BIND_METHOD(EditorResourcePreview,queue_edited_resource_preview);
    SE_BIND_METHOD(EditorResourcePreview,add_preview_generator);
    SE_BIND_METHOD(EditorResourcePreview,remove_preview_generator);
    SE_BIND_METHOD(EditorResourcePreview,check_for_invalidation);

    ADD_SIGNAL(MethodInfo("preview_invalidated", PropertyInfo(VariantType::STRING, "path")));
}

void EditorResourcePreview::check_for_invalidation(StringView p_path) {

    preview_mutex.lock();

    bool call_invalidated = false;
    auto iter = cache.find_as(p_path);
    if (iter!=cache.end()) {

        uint64_t modified_time = FileAccess::get_modified_time(p_path);
        if (modified_time != iter->second.modified_time) {
            cache.erase(iter);
            call_invalidated = true;
        }
    }

    preview_mutex.unlock();

    if (call_invalidated) { //do outside mutex
        call_deferred([this,path=Variant(p_path)] { emit_signal("preview_invalidated", path);});
    }
}

void EditorResourcePreview::start() {
    ERR_FAIL_COND_MSG(thread.is_started(), "Thread already started.");
    thread.start(_thread_func, this);
}

void EditorResourcePreview::stop() {
    if (thread.is_started()) {
        exit.set();
        preview_sem.post();
        while (!exited.is_set()) {
            OS::get_singleton()->delay_usec(10000);
            RenderingServer::sync_thread(); //sync pending stuff, as thread may be blocked on visual server
        }
        thread.wait_to_finish();
    }
}

EditorResourcePreview::EditorResourcePreview() {
    singleton = this;
    order = 0;
}

EditorResourcePreview::~EditorResourcePreview() {
    stop();
}
