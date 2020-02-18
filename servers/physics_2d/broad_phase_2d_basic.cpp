/*************************************************************************/
/*  broad_phase_2d_basic.cpp                                             */
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

#include "broad_phase_2d_basic.h"
BroadPhase2DBasic::ID BroadPhase2DBasic::create(CollisionObject2DSW *p_object_, int p_subindex) {

    current++;

    Element e;
    e.owner = p_object_;
    e._static = false;
    e.subindex = p_subindex;

    element_map[current] = e;
    return current;
}

void BroadPhase2DBasic::move(ID p_id, const Rect2 &p_aabb) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());
    E->second.aabb = p_aabb;
}
void BroadPhase2DBasic::set_static(ID p_id, bool p_static) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());
    E->second._static = p_static;
}
void BroadPhase2DBasic::remove(ID p_id) {

    Map<ID, Element>::iterator E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());
    element_map.erase(E);
}

CollisionObject2DSW *BroadPhase2DBasic::get_object(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),nullptr);
    return E->second.owner;
}
bool BroadPhase2DBasic::is_static(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),false);
    return E->second._static;
}
int BroadPhase2DBasic::get_subindex(ID p_id) const {

    Map<ID, Element>::const_iterator E = element_map.find(p_id);
    ERR_FAIL_COND_V(E==element_map.end(),-1);
    return E->second.subindex;
}

int BroadPhase2DBasic::cull_segment(const Vector2 &p_from, const Vector2 &p_to, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices) {

    int rc = 0;

    for (eastl::pair<const ID,Element> &E : element_map) {

        const Rect2 aabb = E.second.aabb;
        if (aabb.intersects_segment(p_from, p_to)) {

            p_results[rc] = E.second.owner;
            p_result_indices[rc] = E.second.subindex;
            rc++;
            if (rc >= p_max_results)
                break;
        }
    }

    return rc;
}
int BroadPhase2DBasic::cull_aabb(const Rect2 &p_aabb, CollisionObject2DSW **p_results, int p_max_results, int *p_result_indices) {

    int rc = 0;

    for (eastl::pair<const ID,Element> &E : element_map) {

        const Rect2 aabb = E.second.aabb;
        if (aabb.intersects(p_aabb)) {

            p_results[rc] = E.second.owner;
            p_result_indices[rc] = E.second.subindex;
            rc++;
            if (rc >= p_max_results)
                break;
        }
    }

    return rc;
}

void BroadPhase2DBasic::set_pair_callback(PairCallback p_pair_callback, void *p_userdata) {

    pair_userdata = p_userdata;
    pair_callback = p_pair_callback;
}
void BroadPhase2DBasic::set_unpair_callback(UnpairCallback p_unpair_callback, void *p_userdata) {

    unpair_userdata = p_userdata;
    unpair_callback = p_unpair_callback;
}

void BroadPhase2DBasic::update() {

    // recompute pairs
    for (Map<ID,Element>::iterator I = element_map.begin(); I!=element_map.end(); ++I) {
        Map<ID, Element>::iterator J=I;
        // start from I+1
        for (++J; J!=element_map.end(); ++J) {

            Element *elem_A = &I->second;
            Element *elem_B = &J->second;

            if (elem_A->owner == elem_B->owner)
                continue;

            bool pair_ok = elem_A->aabb.intersects(elem_B->aabb) && (!elem_A->_static || !elem_B->_static);

            PairKey key(I->first, J->first);

            HashMap<PairKey, void *>::iterator E = pair_map.find(key);

            if (!pair_ok && E!=pair_map.end()) {
                if (unpair_callback)
                    unpair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, E->second, unpair_userdata);
                pair_map.erase(key);
            }

            if (pair_ok && E==pair_map.end()) {

                void *data = nullptr;
                if (pair_callback)
                    data = pair_callback(elem_A->owner, elem_A->subindex, elem_B->owner, elem_B->subindex, unpair_userdata);
                pair_map.emplace(key, data);
            }
        }
    }
}

BroadPhase2DSW *BroadPhase2DBasic::_create() {

    return memnew(BroadPhase2DBasic);
}

BroadPhase2DBasic::BroadPhase2DBasic() {

    current = 1;
    unpair_callback = nullptr;
    unpair_userdata = nullptr;
    pair_callback = nullptr;
    pair_userdata = nullptr;
}
