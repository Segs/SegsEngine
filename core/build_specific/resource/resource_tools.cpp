/*************************************************************************/
/*  resource.cpp                                                         */
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

#include "core/resource/resource_tools.h"

#include "core/hash_map.h"
#include "core/map.h"

#include "core/resource/resource_manager.h"
#include "scene/resources/packed_scene.h"

struct ResourceToolingData {
    String import_path;
    uint64_t last_modified_time=0;
    uint64_t import_last_modified_time=0;
};

namespace {
    HashMap<String, HashMap<String, int>> resource_path_cache; // each tscn has a set of resource paths and IDs
    HashMap<const Resource *, ResourceToolingData> s_tooling_data;
    RWLock path_cache_lock;

} // end of anonymous namespace

namespace ResourceTooling {

void set_import_path(Resource *r, StringView p_path) {
    s_tooling_data[r].import_path = p_path;
}

const String &get_import_path(const Resource *r) {
    const auto iter = s_tooling_data.find(r);
    static const String no_import_path = "";
    if (iter == s_tooling_data.end()) {
        return no_import_path;
    }
    return iter->second.import_path;
}

//helps keep IDs same number when loading/saving scenes. -1 clears ID and it Returns -1 when no id stored
void set_id_for_path(const Resource *r, StringView p_path, int p_id) {
    RWLockWrite wr(path_cache_lock);
    if (p_id == -1) {
        resource_path_cache[String(p_path)].erase(r->get_path());
    } else {
        resource_path_cache[String(p_path)][r->get_path()] = p_id;
    }
}

int get_id_for_path(const Resource *r,StringView p_path) {
    RWLockRead rd_lock(path_cache_lock);

    auto & res_path_cache(resource_path_cache[String(p_path)]);
    const auto iter = res_path_cache.find(r->get_path());
    if (iter!=res_path_cache.end()) {
        return iter->second;
    }
    return -1;
}

void set_last_modified_time(const Resource *r, uint64_t p_time) {
    //TODO: fix this hack, maybe by handling nested resources in a general way??
    const auto packed_res(object_cast<PackedScene>(r));
    if (packed_res) {
        packed_res->get_state()->set_last_modified_time(p_time);
    }
    s_tooling_data[r].last_modified_time = p_time;
}
uint64_t get_last_modified_time(const Resource *r) {
    const auto iter = s_tooling_data.find(r);
    if (iter == s_tooling_data.end())
        return 0;
    return iter->second.last_modified_time;
}

void set_last_modified_time_from_another(const Resource *r, const Resource *other) {
    const auto iter = s_tooling_data.find(other);
    set_last_modified_time(r, iter != s_tooling_data.end() ? iter->second.last_modified_time : 0);
}

void set_import_last_modified_time(const Resource *r, uint64_t p_time) {
    s_tooling_data[r].import_last_modified_time = p_time;
}
uint64_t get_import_last_modified_time(const Resource *r) {
    const auto iter = s_tooling_data.find(r);
    if (iter == s_tooling_data.end())
        return 0;
    return iter->second.import_last_modified_time;
}


} // namespace ResourceTooling
