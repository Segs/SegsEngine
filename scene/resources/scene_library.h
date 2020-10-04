/* http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License.
 * See LICENSE.md for details.
*/
#pragma once

#include "core/resource.h"
#include "core/map.h"
#include "core/string.h"
#include "scene/resources/packed_scene.h"

class Texture;
class PackedScene;

using LibraryItemHandle = int;

class GODOT_EXPORT SceneLibrary : public Resource {

    GDCLASS(SceneLibrary,Resource)

    RES_BASE_EXTENSION("scenelib")

public:
    struct Item {
        String name;
        Ref<PackedScene> scene;
        Ref<Texture> preview;
    };

    Map<LibraryItemHandle, Item> item_map;
protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    static void _bind_methods();

public:
    LibraryItemHandle add_item(Item &&data);
    void create_item(LibraryItemHandle p_item);
    void set_item_name(LibraryItemHandle p_item, StringView p_name);
    void set_item_scene(LibraryItemHandle p_item, const Ref<PackedScene> &p_mesh);
    void set_item_preview(LibraryItemHandle p_item, const Ref<Texture> &p_preview);
    const String &get_item_name(LibraryItemHandle p_item) const;
    Ref<PackedScene> get_item_scene(LibraryItemHandle p_item) const;
    Ref<Texture> get_item_preview(LibraryItemHandle p_item) const;

    void remove_item(LibraryItemHandle p_item);
    bool has_item(LibraryItemHandle p_item) const;

    void clear();

    int find_item_by_name(StringView p_name) const;

    Vector<int> get_item_list() const;
    int get_last_unused_item_id() const;

    SceneLibrary();
    ~SceneLibrary() override;
};
