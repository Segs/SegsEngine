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

IMPL_GDCLASS(LibraryEntryInstance)


void LibraryEntryInstance::_bind_methods() {

    BIND_METHOD(LibraryEntryInstance,set_library);
    BIND_METHOD(LibraryEntryInstance,get_library);

    BIND_METHOD(LibraryEntryInstance,set_library_path);
    BIND_METHOD(LibraryEntryInstance,get_library_path);

    BIND_METHOD(LibraryEntryInstance,set_entry);
    BIND_METHOD(LibraryEntryInstance,get_entry);

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

void LibraryEntryInstance::update_instance()
{

    if(!resolved_library || entry_name.empty()) {
        return;
    }

    {
        LibraryItemHandle h = resolved_library->find_item_by_name(entry_name);
        set_filename(lib_name+"::"+StringUtils::num(h));
        ERR_FAIL_COND_MSG(h == LibraryItemHandle(-1), "Library does not contain selected entry:" + entry_name);

        if(instantiated_child) {
            remove_child(instantiated_child);
            instantiated_child->queue_delete();
        }
        Ref<PackedScene> resolved_scene = resolved_library->get_item_scene(h);
        instantiated_child = (Node3D *)resolved_scene->instance();
        instantiated_child->set_name(resolved_library->get_name()+"::"+entry_name);
        // Not setting owner here, to prevent those nodes from being saved.
    }
    //EditorNode::get_singleton()->get_edited_scene()->set_editable_instance(node, false);
}

void LibraryEntryInstance::set_library(const Ref<SceneLibrary> &p_lib) {
    if(resolved_library==p_lib)
        return;
    resolved_library = p_lib;
    if(p_lib)
        lib_name = p_lib->get_path();
    if(is_inside_tree())
    Object_change_notify(this);
    call_deferred([this]() {update_instance();});
}

void LibraryEntryInstance::set_library_path(const String &lib)
{
    bool lib_changed = lib!=lib_name;
    lib_name = lib;
    if(!lib_changed)
        return;

    resolved_library = dynamic_ref_cast<SceneLibrary>(gResourceManager().load(lib));
    if(!resolved_library) {
        return;
    }
    if(!entry_name.empty()) {
        if(is_inside_tree())
            Object_change_notify(this);
        update_instance();
    }
}

void LibraryEntryInstance::set_entry(StringView name)
{
    if (entry_name == name)
        return;
    entry_name = name;
    if(is_inside_tree())
        Object_change_notify(this);
    update_instance();
}

void LibraryEntryInstance::_notification(int p_what)
{
    if(p_what== NOTIFICATION_ENTER_WORLD)
    {
        if(instantiated_child) {
            _add_child_nocheck(instantiated_child,instantiated_child->get_name());
        }
    }
    else if (p_what == NOTIFICATION_EXIT_WORLD)
    {
        if(instantiated_child)
            remove_child(instantiated_child);
    }
}


LibraryEntryInstance::LibraryEntryInstance() = default;

LibraryEntryInstance::~LibraryEntryInstance() = default;
