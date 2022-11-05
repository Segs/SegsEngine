/*************************************************************************/
/*  multimesh_instance_3d.cpp                                            */
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

#include "instantiation.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/resource/resource_manager.h"
#include "scene/main/scene_tree.h"
#include "core/message_queue.h"
#include "editor/editor_node.h"

IMPL_GDCLASS(LibraryEntryInstance)

//TODO: consider connecting instances of this class with their respective resource's changed signal to retry instantiations

void LibraryEntryInstance::_bind_methods() {

    SE_BIND_METHOD(LibraryEntryInstance,set_library);
    SE_BIND_METHOD(LibraryEntryInstance,get_library);

    SE_BIND_METHOD(LibraryEntryInstance,set_library_path);
    SE_BIND_METHOD(LibraryEntryInstance,get_library_path);

    SE_BIND_METHOD(LibraryEntryInstance,set_entry);
    SE_BIND_METHOD(LibraryEntryInstance,get_entry);

    ClassDB::add_property(get_class_static_name(),
            PropertyInfo(VariantType::OBJECT, "library", PropertyHint::ResourceType, "SceneLibrary",PROPERTY_USAGE_EDITOR),
            "set_library", "get_library");
    ClassDB::add_property(get_class_static_name(),
            PropertyInfo(VariantType::STRING, "library_path",PropertyHint::None,"",PROPERTY_USAGE_NOEDITOR|PROPERTY_USAGE_INTERNAL),
            "set_library_path", "get_library_path");

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "entry"), "set_entry", "get_entry");
}

static void visit_child_and_assign_library(Node *n,const Ref<SceneLibrary> &lib) {
    LibraryEntryInstance *child_c = object_cast<LibraryEntryInstance>(n);
    if(child_c) {
        if(child_c->get_library())
            return; // already has it can return.
        child_c->set_library(lib); // sub-node can still be missing a library
    }
    for(int i=0,fin=n->get_child_count(); i<fin; ++i) {
        visit_child_and_assign_library(n->get_child(i),lib);
    }
}

static void search_for_parent_with_library(LibraryEntryInstance *n) {

    auto iter =n ? n->get_parent() : nullptr;
    while(iter) {
        LibraryEntryInstance *parent = object_cast<LibraryEntryInstance>(iter);
        if(parent && parent->get_library()) {
            n->set_library(parent->get_library());
            return;
        }
        iter = iter->get_parent();
    }
}

static void set_owner_deep(Node *owner, Node *start) {
    start->set_owner(owner);
    for (Node *n : start->children()) {
        set_owner_deep(owner, n);
    }
}
// recursively replace all LibraryEntryInstance by their associated packed scene instances
// when no library is found, the node is removed and nullptr is returned
static Node *replace_all_instances(Node *n) {
    if (!n) {
        return n;
    }

    auto lib_inst = object_cast<LibraryEntryInstance>(n);
    if (lib_inst) {
        // this is lib instance, replace ourselves with the instance of packed scene
        return replace_all_instances(lib_inst->instantiate_resolved());
    }
    for (int idx = 0; idx < n->get_child_count(); ++idx) {
        Node *child = n->get_child(idx);
        
        Node *new_child = replace_all_instances(child);
        if (new_child == child) {
            continue;
        }

        n->remove_child(child);
        if (new_child) {
            Node3D *as3d_child = object_cast<Node3D>(child);
            if (as3d_child) {
                Node3D *as3d_new_child = object_cast<Node3D>(new_child);
                as3d_new_child->set_transform(as3d_child->get_transform());
            }
            n->add_child(new_child);
        }
        memdelete(child);
    }
    return n;
}

Node *LibraryEntryInstance::instantiate_resolved() {
    if (!lib_name.empty() && !entry_name.empty()) {
        resolved_library = dynamic_ref_cast<SceneLibrary>(gResourceManager().load(lib_name));
    }
    ERR_FAIL_COND_V_MSG(!resolved_library, nullptr, "Library cannot be resolved:" + lib_name);
    LibraryItemHandle h = resolved_library->find_item_by_name(entry_name);
    ERR_FAIL_COND_V_MSG(h == LibraryItemHandle(-1), nullptr, "Library does not contain selected entry:" + entry_name);
    Ref<PackedScene> resolved_scene = resolved_library->get_item_scene(h);
    return resolved_scene->instance(GEN_EDIT_STATE_MAIN);
}

bool LibraryEntryInstance::instantiate() {
    if (!resolved_library || entry_name.empty()) {
        ERR_FAIL_COND_V_MSG(!resolved_library || entry_name.empty(), false, "Library does not contain selected entry:" + entry_name);
    }
    Node *base = get_parent();
    if (!base) {
        return false;
    }
    assert(children().empty());

    // get the packed scene from library.
    LibraryItemHandle h = resolved_library->find_item_by_name(entry_name);
    set_filename(lib_name + "::" + StringUtils::num(h));
    ERR_FAIL_COND_V_MSG(h == LibraryItemHandle(-1), false, "Library does not contain selected entry:" + entry_name);
    Ref<PackedScene> resolved_scene = resolved_library->get_item_scene(h);

    auto *src_scene = resolved_scene->instance(GEN_EDIT_STATE_MAIN);
    auto *scene = (Node3D *)replace_all_instances(src_scene);
    if (scene != src_scene) {
        memdelete(src_scene);
    }

    // replace ourselves in our parent with the instance.
    call_deferred([=] {
        // create the target scene instance
        scene->set_name(scene->get_name() + "_libinstance");
        Node *parent = get_parent();
        int pos = get_position_in_parent();
        auto t = get_transform();
        queue_delete();
        parent->remove_child(this);
        parent->add_child(scene);
        parent->move_child(scene, pos);
        scene->set_transform(t);
        set_owner_deep(EditorNode::get_singleton()->get_edited_scene(),scene);
    });
    return true;
}

void LibraryEntryInstance::set_library(const Ref<SceneLibrary> &p_lib) {
    if(resolved_library==p_lib) {
        return;
    }
    resolved_library = p_lib;
    if(p_lib) {
        lib_name = p_lib->get_path();
    }
}

void LibraryEntryInstance::set_library_path(const String &lib)
{
    bool lib_changed = lib!=lib_name;
    lib_name = lib;
    if(!lib_changed) {
        return;
    }
    if (lib_name.empty()) {
        resolved_library = {};
        return;
    }
    if (!lib_name.empty() && !entry_name.empty()) {
        resolved_library = dynamic_ref_cast<SceneLibrary>(gResourceManager().load(lib_name));
    }
}

void LibraryEntryInstance::set_entry(StringView name)
{
    if (entry_name == name)
        return;
    entry_name = name;
}

void LibraryEntryInstance::_notification(int p_what)
{
    if(p_what== NOTIFICATION_ENTER_WORLD)
    {
        if (!lib_name.empty() && !entry_name.empty()) {
            resolved_library = dynamic_ref_cast<SceneLibrary>(gResourceManager().load(lib_name));
        }
        // we try to replace ourselves in the scene tree when we enter
        instantiate();
    }
}


LibraryEntryInstance::LibraryEntryInstance() = default;

LibraryEntryInstance::~LibraryEntryInstance() = default;
