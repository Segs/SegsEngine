/*************************************************************************/
/*  resource_preloader.cpp                                               */
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

#include "resource_preloader.h"
#include "core/method_bind.h"
#include "core/resource.h"
#include "core/pool_vector.h"

IMPL_GDCLASS(ResourcePreloader)

void ResourcePreloader::_set_resources(const Array &p_data) {

    resources.clear();

    ERR_FAIL_COND(p_data.size() != 2)
    PoolVector<String> names = p_data[0].as<PoolVector<String>>();
    Array resdata = p_data[1];

    ERR_FAIL_COND(names.size() != resdata.size())

    for (int i = 0; i < resdata.size(); i++) {

        StringName name(names[i]);
        RES resource(refFromRefPtr<Resource>(resdata[i]));
        ERR_CONTINUE(not resource);
        resources[name] = resource;

        //add_resource(name,resource);
    }
}

Array ResourcePreloader::_get_resources() const {

    PoolVector<String> names;
    Array arr;
    arr.resize(resources.size());
    names.resize(resources.size());

    Set<StringName> sorted_names;

    for (const eastl::pair<const StringName,RES> &E : resources) {
        sorted_names.insert(E.first);
    }

    int i = 0;
    for (const StringName &E : sorted_names) {

        names.set(i, E.asCString());
        arr[i] = resources.at(E);
        i++;
    }

    Array res;
    res.push_back(names);
    res.push_back(arr);
    return res;
}

void ResourcePreloader::add_resource(const StringName &p_name, const RES &p_resource) {

    ERR_FAIL_COND(not p_resource)
    if (resources.contains(p_name)) {

        StringName new_name;
        int idx = 2;

        while (true) {

            new_name = p_name + " " + itos(idx);
            if (resources.contains(new_name)) {
                idx++;
                continue;
            }

            break;
        }

        add_resource(new_name, p_resource);
    } else {

        resources[p_name] = p_resource;
    }
}

void ResourcePreloader::remove_resource(const StringName &p_name) {

    ERR_FAIL_COND(!resources.contains(p_name))
    resources.erase(p_name);
}
void ResourcePreloader::rename_resource(const StringName &p_from_name, const StringName &p_to_name) {

    ERR_FAIL_COND(!resources.contains(p_from_name))

    RES res(resources[p_from_name]);

    resources.erase(p_from_name);
    add_resource(p_to_name, res);
}

bool ResourcePreloader::has_resource(const StringName &p_name) const {

    return resources.contains(p_name);
}
RES ResourcePreloader::get_resource(const StringName &p_name) const {

    ERR_FAIL_COND_V(!resources.contains(p_name), RES());
    return resources.at(p_name);
}

PoolVector<String> ResourcePreloader::_get_resource_list() const {

    PoolVector<String> res;
    res.resize(resources.size());
    int i = 0;
    for (const eastl::pair<const StringName,RES> &E : resources) {
        res.set(i++, E.first.asCString());
    }

    return res;
}

void ResourcePreloader::get_resource_list(ListPOD<StringName> *p_list) {

    for (eastl::pair<const StringName,RES> &E : resources) {

        p_list->push_back(E.first);
    }
}

void ResourcePreloader::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_set_resources"), &ResourcePreloader::_set_resources);
    MethodBinder::bind_method(D_METHOD("_get_resources"), &ResourcePreloader::_get_resources);

    MethodBinder::bind_method(D_METHOD("add_resource", {"name", "resource"}), &ResourcePreloader::add_resource);
    MethodBinder::bind_method(D_METHOD("remove_resource", {"name"}), &ResourcePreloader::remove_resource);
    MethodBinder::bind_method(D_METHOD("rename_resource", {"name", "newname"}), &ResourcePreloader::rename_resource);
    MethodBinder::bind_method(D_METHOD("has_resource", {"name"}), &ResourcePreloader::has_resource);
    MethodBinder::bind_method(D_METHOD("get_resource", {"name"}), &ResourcePreloader::get_resource);
    MethodBinder::bind_method(D_METHOD("get_resource_list"), &ResourcePreloader::_get_resource_list);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "resources", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_resources", "_get_resources");
}

ResourcePreloader::ResourcePreloader() {
}
