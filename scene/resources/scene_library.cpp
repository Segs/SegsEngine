/* http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License.
 * See LICENSE.md for details.
*/

#include "scene_library.h"

#include "scene/resources/texture.h"
#include "scene/resources/packed_scene.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/engine.h"

IMPL_GDCLASS(SceneLibrary)
RES_BASE_EXTENSION_IMPL(SceneLibrary,"scenelib")

bool SceneLibrary::_set(const StringName &p_name, const Variant &p_value) {
    using namespace eastl;
    StringView name = p_name;
    if (StringUtils::begins_with(name,"item/")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(name,'/', 1));
        StringView what = StringUtils::get_slice(name,'/', 2);
        if (!item_map.contains(idx))
            create_item(idx);

        if (what == "name"_sv)
            set_item_name(idx, p_value.as<String>());
        else if (what == "scene"_sv)
            set_item_scene(idx, refFromVariant<PackedScene>(p_value));
        else if (what == "preview"_sv)
            set_item_preview(idx, refFromVariant<Texture>(p_value));
        else
            return false;

        return true;
    }

    return false;
}

bool SceneLibrary::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace eastl;

    StringView name(p_name);
    int idx = StringUtils::to_int(StringUtils::get_slice(name,'/', 1));
    ERR_FAIL_COND_V(!item_map.contains(idx), false);
    StringView what = StringUtils::get_slice(name,'/', 2);

    if (what == "name"_sv)
        r_ret = get_item_name(idx);
    else if (what == "scene"_sv)
        r_ret = get_item_scene(idx);
    else if (what == "preview"_sv)
        r_ret = get_item_preview(idx);
    else
        return false;

    return true;
}

void SceneLibrary::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (const eastl::pair<const int,Item> &E : item_map) {

        String name = "item/" + itos(E.first) + "/";
        p_list->push_back(PropertyInfo(VariantType::STRING, StringName(name + "name")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "scene"), PropertyHint::ResourceType, "PackedScene"));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "preview"), PropertyHint::ResourceType, "Texture")); //, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_EDITOR_HELPER
    }
}

void SceneLibrary::create_item(LibraryItemHandle p_item) {

    ERR_FAIL_COND(p_item < 0);
    ERR_FAIL_COND(item_map.contains(p_item));
    item_map[p_item] = Item();
    Object_change_notify(this);
}

void SceneLibrary::set_item_name(LibraryItemHandle p_item, StringView p_name) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].name = p_name;
    emit_changed();
    Object_change_notify(this);
}

void SceneLibrary::set_item_scene(LibraryItemHandle p_item, const Ref<PackedScene> &p_mesh) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].scene = p_mesh;
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
}

void SceneLibrary::set_item_preview(LibraryItemHandle p_item, const Ref<Texture> &p_preview) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].preview = p_preview;
    emit_changed();
    Object_change_notify(this);
}

const String &SceneLibrary::get_item_name(LibraryItemHandle p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), null_string, "Requested for nonexistent SceneLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).name;
}

Ref<PackedScene> SceneLibrary::get_item_scene(LibraryItemHandle p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Ref<PackedScene>(), "Requested for nonexistent SceneLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).scene;
}

Ref<Texture> SceneLibrary::get_item_preview(LibraryItemHandle p_item) const {

    if (!Engine::get_singleton()->is_editor_hint()) {
        ERR_PRINT("SceneLibrary item previews are only generated in an editor context, which means they aren't available in a running project.");
        return Ref<Texture>();
    }

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Ref<Texture>(), "Requested for nonexistent SceneLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).preview;
}

bool SceneLibrary::has_item(LibraryItemHandle p_item) const {

    return item_map.contains(p_item);
}
void SceneLibrary::remove_item(int p_item) {

    ERR_FAIL_COND_MSG(!item_map.contains(p_item), "Requested for nonexistent SceneLibrary item '" + itos(p_item) + "'.");
    item_map.erase(p_item);
    notify_change_to_owners();
    Object_change_notify(this);
    emit_changed();
}

void SceneLibrary::clear() {

    item_map.clear();
    notify_change_to_owners();
    Object_change_notify(this);
    emit_changed();
}

Vector<int> SceneLibrary::get_item_list() const {

    Vector<int> ret;
    ret.reserve(item_map.size());

    for (const eastl::pair<const int,Item> &E : item_map) {

        ret.push_back(E.first);
    }

    return ret;
}

LibraryItemHandle SceneLibrary::find_item_by_name(StringView p_name) const {

    for (const eastl::pair<const int,Item> &E : item_map) {
            
        if (E.second.name == p_name)
            return E.first;
    }
    return -1;
}

LibraryItemHandle SceneLibrary::get_last_unused_item_id() const {

    if (item_map.empty())
        return 0;

    return item_map.rbegin()->first + 1;
}

LibraryItemHandle SceneLibrary::add_item(Item &&data)
{
    ERR_FAIL_COND_V_MSG(find_item_by_name(data.name) != -1, LibraryItemHandle(-1), "SceneLibrary only accepts unique scene names.");
    int last_key = get_last_unused_item_id();
    item_map[last_key] = eastl::move(data);
    notify_change_to_owners();
    emit_changed();
    Object_change_notify(this);
    return LibraryItemHandle(last_key);
}

void SceneLibrary::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("create_item", {"id"}), &SceneLibrary::create_item);
    MethodBinder::bind_method(D_METHOD("set_item_name", {"id", "name"}), &SceneLibrary::set_item_name);
    MethodBinder::bind_method(D_METHOD("set_item_preview", {"id", "texture"}), &SceneLibrary::set_item_preview);
    MethodBinder::bind_method(D_METHOD("get_item_name", {"id"}), &SceneLibrary::get_item_name);
    MethodBinder::bind_method(D_METHOD("get_item_preview", {"id"}), &SceneLibrary::get_item_preview);
    MethodBinder::bind_method(D_METHOD("remove_item", {"id"}), &SceneLibrary::remove_item);
    MethodBinder::bind_method(D_METHOD("find_item_by_name", {"name"}), &SceneLibrary::find_item_by_name);

    MethodBinder::bind_method(D_METHOD("clear"), &SceneLibrary::clear);
    MethodBinder::bind_method(D_METHOD("get_item_list"), &SceneLibrary::get_item_list);
    MethodBinder::bind_method(D_METHOD("get_last_unused_item_id"), &SceneLibrary::get_last_unused_item_id);
}


SceneLibrary::SceneLibrary() = default;
SceneLibrary::~SceneLibrary() = default;
