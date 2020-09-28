#include "scene_map.h"

#include "core/object.h"
#include "core/class_db.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"

IMPL_GDCLASS(SceneMap)


SceneMap::SceneMap() {
    
}

void SceneMap::create_item(int p_item) {

    ERR_FAIL_COND(p_item < 0);
    ERR_FAIL_COND(item_map.contains(p_item));
    item_map[p_item] = MapEntry();
    Object_change_notify(this);
}
void SceneMap::set_item_name(int p_item, StringView p_name) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].m_path = p_name;
    Object_change_notify(this);
}

void SceneMap::set_item_library(int p_item, const Ref<SceneLibrary>& p_lib) {

    ERR_FAIL_COND(!item_map.contains(p_item));
    item_map[p_item].m_library = p_lib;
    Object_change_notify(this);
}
bool SceneMap::_set(const StringName& p_name, const Variant& p_value) {
    using namespace eastl;
    StringView name = p_name;
    if (StringUtils::begins_with(name, "item/")) {

        int idx = StringUtils::to_int(StringUtils::get_slice(name, '/', 1));
        StringView what = StringUtils::get_slice(name, '/', 2);
        if (!item_map.contains(idx))
            create_item(idx);

        if (what == "name"_sv)
            set_item_name(idx, p_value.as<String>());
        else if (what == "scene"_sv)
            set_item_library(idx, refFromVariant<SceneLibrary>(p_value));
        else
            return false;

        return true;
    }

    return false;
}
const String& SceneMap::get_item_name(int p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), null_string, "Requested for nonexistent SceneMap library '" + itos(p_item) + "'.");
    return item_map.at(p_item).m_path;
}

Ref<SceneLibrary> SceneMap::get_item_library(int p_item) const {

    ERR_FAIL_COND_V_MSG(!item_map.contains(p_item), Ref<SceneLibrary>(), "Requested for nonexistent SceneLibrary item '" + itos(p_item) + "'.");
    return item_map.at(p_item).m_library;
}
bool SceneMap::_get(const StringName& p_name, Variant& r_ret) const {
    using namespace eastl;

    StringView name(p_name);
    int idx = StringUtils::to_int(StringUtils::get_slice(name, '/', 1));
    ERR_FAIL_COND_V(!item_map.contains(idx), false);
    StringView what = StringUtils::get_slice(name, '/', 2);

    if (what == "name"_sv)
        r_ret = get_item_name(idx);
    else if (what == "scene"_sv)
        r_ret = get_item_library(idx);
    else
        return false;

    return true;
}

void SceneMap::_get_property_list(Vector<PropertyInfo>* p_list) const {

    for (const eastl::pair<const int, MapEntry>& E : item_map) {

        String name = "item/" + itos(E.first) + "/";
        p_list->push_back(PropertyInfo(VariantType::STRING, StringName(name + "name")));
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName(name + "library"), PropertyHint::ResourceType, "SceneLibrary"));
    }
}

void SceneMap::_bind_methods() {
}
