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

IMPL_GDCLASS(LibraryEntryInstance)


void LibraryEntryInstance::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_library", {"library"}), &LibraryEntryInstance::set_library);
    MethodBinder::bind_method(D_METHOD("get_library"), &LibraryEntryInstance::get_library);

    MethodBinder::bind_method(D_METHOD("set_entry", { "library" }), &LibraryEntryInstance::set_entry);
    MethodBinder::bind_method(D_METHOD("get_entry"), &LibraryEntryInstance::get_entry);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "library", PropertyHint::ResourceType, "SceneLibrary"), "set_library", "get_library");
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

void LibraryEntryInstance::update_instance()
{
    if (instantiated_child) {
        remove_child(instantiated_child);
    }
    if(!is_inside_tree())
        return;

    if(library && !entry_name.empty())
    {
        //instantiated_child->set_editable_instance(false);
        LibraryItemHandle h = library->find_item_by_name(entry_name);
        ERR_FAIL_COND_MSG(h == LibraryItemHandle(-1), "Library does not contain selected entry:" + entry_name);
        instantiated_child = library->get_item_scene(h)->instance();
        instantiated_child->set_name(library->get_name()+"::"+entry_name);
        if(library) {
            visit_child_and_assign_library(instantiated_child,library);
        }
        add_child(instantiated_child);

        // Not setting owner here, to prevent those nodes from being saved.

    }
    //EditorNode::get_singleton()->get_edited_scene()->set_editable_instance(node, false);
}

void LibraryEntryInstance::set_library(const Ref<SceneLibrary> &p_lib) {
    if(library==p_lib)
        return;
    library = p_lib;
    Object_change_notify(this);
    update_instance();
}

void LibraryEntryInstance::set_entry(StringView name)
{
    if (entry_name == name)
        return;
    entry_name = name;
    Object_change_notify(this);
    update_instance();
}

void LibraryEntryInstance::_notification(int p_what)
{
    if(p_what== NOTIFICATION_ENTER_WORLD)
    {
        update_instance();
        return;
    }
    if (p_what == NOTIFICATION_EXIT_WORLD)
    {
        if(instantiated_child)
            remove_child(instantiated_child);
        return;
    }
}


LibraryEntryInstance::LibraryEntryInstance() {
}

LibraryEntryInstance::~LibraryEntryInstance() {
}
