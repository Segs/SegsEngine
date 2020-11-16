/*************************************************************************/
/*  octree.h                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#pragma once

#include "core/list.h"
#include "core/vector.h"
#include "core/hash_map.h"
#include "core/math/aabb.h"
#include "core/math/geometry.h"
#include "core/math/vector3.h"
#include "core/string.h"
#include "core/print_string.h"
#include "core/variant.h"


// We want 2 versions of the octree, Octree
// and Octree_CL which uses cached lists (optimized).
// we don't want to use the extra memory of cached lists on
// the non cached list version, so we use macros
// to avoid duplicating the code which is in octree_definition.
// The name of the class is overridden and the changes with the define
// OCTREE_USE_CACHED_LISTS.

// The two classes can be used identically but one contains the cached
// list optimization.


#undef OCTREE_CLASS_NAME



typedef uint32_t OctreeElementID;
#define OCTREE_ELEMENT_INVALID_ID 0
#define OCTREE_SIZE_LIMIT 1e15
#define OCTREE_DEFAULT_OCTANT_LIMIT 0

template <class T, bool cached_lists,bool use_pairs = false>
class OctreeTpl {
public:
    using PairCallback = void *(*)(void *, OctreeElementID, T *, int, OctreeElementID, T *, int);
    using UnpairCallback = void (*)(void *, OctreeElementID, T *, int, OctreeElementID, T *, int, void *);

private:
    enum {

        NEG = 0,
        POS = 1,
    };

    enum {
        OCTANT_NX_NY_NZ,
        OCTANT_PX_NY_NZ,
        OCTANT_NX_PY_NZ,
        OCTANT_PX_PY_NZ,
        OCTANT_NX_NY_PZ,
        OCTANT_PX_NY_PZ,
        OCTANT_NX_PY_PZ,
        OCTANT_PX_PY_PZ
    };

    struct PairKey {

        union {
            struct {
                OctreeElementID A;
                OctreeElementID B;
            };
            uint64_t key;
        };

        _FORCE_INLINE_ bool operator<(const PairKey &p_pair) const {

            return key < p_pair.key;
        }

        _FORCE_INLINE_ bool operator==(const PairKey& p_pair) const {

            return key == p_pair.key;
        }
        _FORCE_INLINE_ PairKey(OctreeElementID p_A, OctreeElementID p_B) {

            if (p_A < p_B) {

                A = p_A;
                B = p_B;
            } else {

                B = p_A;
                A = p_B;
            }
        }

        explicit operator size_t() const { return key; }
        PairKey() = default;
    };

    struct Element;

    // instead of iterating the linked list every time within octants,
    // we can cache a linear list of prepared elements containing essential data
    // for fast traversal, and rebuild it only when an octant changes.
    struct CachedList {
        Vector<AABB> aabbs;
        Vector<Element *> elements;

        void update(List<Element *> &eles) {
            // make sure local vector doesn't delete the memory
            // no need to be thrashing allocations
            aabbs.clear();
            elements.clear();

            for(Element *e : eles) {
                aabbs.push_back(e->aabb);
                elements.push_back(e);
            }
        }
    };
    template<class EL>
    struct OctantBase {
        // cached for FAST plane check
        AABB aabb;

        uint64_t last_pass;
        OctantBase *parent;
        OctantBase *children[8];

        int children_count; // cache for amount of childrens (fast check for removal)
        int parent_index; // cache for parent index (fast check for removal)

        List<EL *> pairable_elements;
        List<EL *> elements;

        OctantBase() {
            children_count = 0;
            parent_index = -1;
            last_pass = 0;
            parent = nullptr;
            for (int i = 0; i < 8; i++)
                children[i] = nullptr;
        }

        ~OctantBase() = default;

    };
    template<class EL,bool use_cache>
    struct OctantT;

    template<class EL>
    struct OctantT<EL,false> : public OctantBase<EL> {

    };
    template<class EL>
    struct OctantT<EL,true> : public OctantBase<EL> {
        // cached lists are linear in memory so are faster than using linked list
        CachedList clist_pairable;
        CachedList clist;

        // use dirty flag to indicate when cached lists need updating
        // this avoids having to update the cached list on lots of octants
        // if nothing is moving in them.
        bool dirty;

        void update_cached_lists() {
            if (!dirty) {
#ifdef TOOLS_ENABLED
//#define OCTREE_CACHED_LIST_ERROR_CHECKS
#endif
#ifdef OCTREE_CACHED_LIST_ERROR_CHECKS
                // debug - this will slow down performance a lot,
                // only enable these error checks for testing that the cached
                // lists are up to date.
                int hash_before_P = clist_pairable.aabbs.size();
                int hash_before_N = clist.aabbs.size();
                clist_pairable.update(pairable_elements);
                clist.update(elements);
                int hash_after_P = clist_pairable.aabbs.size();
                int hash_after_N = clist.aabbs.size();

                ERR_FAIL_COND(hash_before_P != hash_after_P);
                ERR_FAIL_COND(hash_before_N != hash_after_N);
#endif
                return;
            }
            clist_pairable.update(this->pairable_elements);
            clist.update(this->elements);
            dirty = false;
        }

        OctantT() {
            dirty = true;
        }
    };
    using Octant = OctantT<Element,cached_lists>;

    struct PairData;

    struct Element {

        OctreeTpl *octree;

        T *userdata;
        int subindex;
        bool pairable;
        uint32_t pairable_mask;
        uint32_t pairable_type;

        uint64_t last_pass;
        OctreeElementID _id;
        Octant *common_parent;

        AABB aabb;
        AABB container_aabb;

        List<PairData *> pair_list;

        struct OctantOwner {

            Octant *octant;
            typename List<Element *>::iterator E;
        }; // an element can be in max 8 octants

        List<OctantOwner> octant_owners;

        // when moving we need make all owner octants dirty, because the AABB can change.
        void moving() {
            if constexpr(cached_lists) {
                for (OctantOwner &F : octant_owners) {
                    Octant *o = F.octant;
                    o->dirty = true;
                }
            }
        }

        Element() {
            last_pass = 0;
            _id = 0;
            pairable = false;
            subindex = 0;
            userdata = nullptr;
            octree = nullptr;
            pairable_mask = 0;
            pairable_type = 0;
            common_parent = nullptr;
        }
    };

    using ElementIterator = typename List<Element *>::iterator;

    struct PairData {

        int refcount;
        bool intersect;
        Element *A, *B;
        void *ud;
        typename List<PairData *>::iterator eA, eB;
    };

    using ElementMap = eastl::unordered_map<OctreeElementID, Element, eastl::hash<OctreeElementID>,eastl::equal_to<OctreeElementID>, wrap_allocator>;
    using PairMap = eastl::unordered_map<PairKey, PairData,eastl::hash<PairKey>, eastl::equal_to<PairKey>, wrap_allocator>;
    ElementMap element_map;
    PairMap pair_map;

    PairCallback pair_callback = nullptr;
    UnpairCallback unpair_callback = nullptr;
    void *pair_callback_userdata = nullptr;
    void *unpair_callback_userdata = nullptr;

    OctreeElementID last_element_id;
    uint64_t pass;

    real_t unit_size;
    Octant *root = nullptr;
    int octant_count=0;
    int pair_count=0;
    int octant_elements_limit;

    _FORCE_INLINE_ void _pair_check(PairData *p_pair) {

        bool intersect = p_pair->A->aabb.intersects_inclusive(p_pair->B->aabb);

        if (intersect != p_pair->intersect) {

            if (intersect) {

                if (pair_callback) {
                    p_pair->ud = pair_callback(pair_callback_userdata, p_pair->A->_id, p_pair->A->userdata, p_pair->A->subindex, p_pair->B->_id, p_pair->B->userdata, p_pair->B->subindex);
                }
                pair_count++;
            } else {

                if (unpair_callback) {
                    unpair_callback(pair_callback_userdata, p_pair->A->_id, p_pair->A->userdata, p_pair->A->subindex, p_pair->B->_id, p_pair->B->userdata, p_pair->B->subindex, p_pair->ud);
                }
                pair_count--;
            }

            p_pair->intersect = intersect;
        }
    }

    _FORCE_INLINE_ void _pair_reference(Element *p_A, Element *p_B) {

        if (p_A == p_B || (p_A->userdata == p_B->userdata && p_A->userdata)) {
            return;
		}

        if (!(p_A->pairable_type & p_B->pairable_mask) &&
                !(p_B->pairable_type & p_A->pairable_mask)) {
            return; // none can pair with none
		}

        PairKey key(p_A->_id, p_B->_id);
        typename PairMap::iterator E = pair_map.find(key);

        if (E==pair_map.end()) {

            PairData pdata;
            pdata.refcount = 1;
            pdata.A = p_A;
            pdata.B = p_B;
            pdata.intersect = false;
            E = pair_map.emplace(key, pdata).first;
            E->second.eA = p_A->pair_list.insert(p_A->pair_list.end(),&E->second);
            E->second.eB = p_B->pair_list.insert(p_B->pair_list.end(),&E->second);

            /*
            if (pair_callback)
                pair_callback(pair_callback_userdata,p_A->userdata,p_B->userdata);
            */
        } else {

            E->second.refcount++;
        }
    }

    _FORCE_INLINE_ void _pair_unreference(Element *p_A, Element *p_B) {

        if (p_A == p_B)
            return;

        PairKey key(p_A->_id, p_B->_id);
        auto E = pair_map.find(key);
        if (E==pair_map.end()) {
            return; // no pair
        }

        E->second.refcount--;

        if (E->second.refcount == 0) {
            // bye pair

            if (E->second.intersect) {
                if (unpair_callback) {
                    unpair_callback(pair_callback_userdata, p_A->_id, p_A->userdata, p_A->subindex, p_B->_id, p_B->userdata, p_B->subindex, E->second.ud);
                }

                pair_count--;
            }

            if (p_A == E->second.B) {
                //may be reaching inverted
                SWAP(p_A, p_B);
            }

            p_A->pair_list.erase(E->second.eA);
            p_B->pair_list.erase(E->second.eB);
            pair_map.erase(E);
        }
    }

    _FORCE_INLINE_ void _element_check_pairs(Element *p_element) {

        auto E = p_element->pair_list.begin();
        while (E!=p_element->pair_list.end()) {

            _pair_check(*E);
            ++E;
        }
    }

    _FORCE_INLINE_ void _optimize() {

        while (root && root->children_count < 2 && !root->elements.size() && !(use_pairs && root->pairable_elements.size())) {

            Octant *new_root = nullptr;
            if (root->children_count == 1) {

                for (int i = 0; i < 8; i++) {

                    if (root->children[i]) {
                        new_root = (Octant *)root->children[i];
                        root->children[i] = nullptr;
                        break;
                    }
                }
                ERR_FAIL_COND(!new_root);
                new_root->parent = nullptr;
                new_root->parent_index = -1;
            }

            memdelete<Octant>(root);
            octant_count--;
            root = new_root;
        }
    }

    void _insert_element(Element *p_element, Octant *p_octant);
    void _ensure_valid_root(const AABB &p_aabb);
    bool _remove_element_pair_and_remove_empty_octants(Element *p_element, Octant *p_octant, Octant *p_limit = nullptr);
    void _remove_element(Element *p_element);
    void _pair_element(Element *p_element, Octant *p_octant);
    void _unpair_element(Element *p_element, Octant *p_octant);

    struct _CullConvexData {

        Span<const Plane> planes;
        Span<Vector3,8> points;
        T **result_array;
        int *result_idx;
        int result_max;
        uint32_t mask;
    };

    void _cull_convex(Octant *p_octant, _CullConvexData *p_cull);
    void _cull_aabb(Octant *p_octant, const AABB &p_aabb, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask);
    void _cull_segment(Octant *p_octant, const Vector3 &p_from, const Vector3 &p_to, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask);
    void _cull_point(Octant *p_octant, const Vector3 &p_point, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask);

    void _remove_tree(Octant *p_octant) {

		if (!p_octant) {
            return;
		}

        for (int i = 0; i < 8; i++) {

            if (p_octant->children[i])
                _remove_tree((Octant *)p_octant->children[i]);
        }

        memdelete<Octant>(p_octant);
    }

#ifdef TOOLS_ENABLED
    String debug_aabb_to_string(const AABB &aabb) const;
    void debug_octant(const Octant &oct, int depth = 0);
#endif

public:
    OctreeElementID create(T *p_userdata, const AABB &p_aabb = AABB(), int p_subindex = 0, bool p_pairable = false, uint32_t p_pairable_type = 0, uint32_t pairable_mask = 1);
    void move(OctreeElementID p_id, const AABB &p_aabb);
    void set_pairable(OctreeElementID p_id, bool p_pairable = false, uint32_t p_pairable_type = 0, uint32_t pairable_mask = 1);
    void erase(OctreeElementID p_id);

    bool is_pairable(OctreeElementID p_id) const;
    T *get(OctreeElementID p_id) const;
    int get_subindex(OctreeElementID p_id) const;

    int cull_convex(Span<const Plane,6> p_convex, T **p_result_array, int p_result_max, uint32_t p_mask = 0xFFFFFFFF);
    int cull_aabb(const AABB &p_aabb, T **p_result_array, int p_result_max, int *p_subindex_array = nullptr, uint32_t p_mask = 0xFFFFFFFF);
    int cull_segment(const Vector3 &p_from, const Vector3 &p_to, T **p_result_array, int p_result_max, int *p_subindex_array = nullptr, uint32_t p_mask = 0xFFFFFFFF);

    int cull_point(const Vector3 &p_point, T **p_result_array, int p_result_max, int *p_subindex_array = nullptr, uint32_t p_mask = 0xFFFFFFFF);

    void set_pair_callback(PairCallback p_callback, void *p_userdata);
    void set_unpair_callback(UnpairCallback p_callback, void *p_userdata);

    int get_octant_count() const { return octant_count; }
    int get_pair_count() const { return pair_count; }
    void set_octant_elements_limit(int p_limit) { octant_elements_limit = p_limit; }

    // just convenience for project settings, as users don't need to know exact numbers
    void set_balance(float p_bal) // 0.0 is optimized for multiple tests, 1.0 is for multiple edits (moves etc)
    {
        float v = CLAMP(p_bal, 0.0f, 1.0f);
        v *= v;
        v *= v;
        v *= 8096.0f; // these values have been found empirically
        int l = 0 + v;
        set_octant_elements_limit(l);
    }

#ifdef TOOLS_ENABLED
    void debug_octants();
#endif

    OctreeTpl(real_t p_unit_size = 1.0);
    ~OctreeTpl() { _remove_tree(root); }
};

/* PRIVATE FUNCTIONS */

template <class T, bool cached_lists,bool use_pairs>
T * OctreeTpl<T,cached_lists,use_pairs>::get(OctreeElementID p_id) const {
    const typename ElementMap::Element *E = element_map.find(p_id);
    ERR_FAIL_COND_V(!E, nullptr);
    return E->get().userdata;
}

template <class T, bool cached_lists,bool use_pairs>
bool OctreeTpl<T,cached_lists,use_pairs>::is_pairable(OctreeElementID p_id) const {

    const typename ElementMap::Element *E = element_map.find(p_id);
    ERR_FAIL_COND_V(!E, false);
    return E->get().pairable;
}

template <class T, bool cached_lists,bool use_pairs>
int OctreeTpl<T,cached_lists,use_pairs>::get_subindex(OctreeElementID p_id) const {

    const typename ElementMap::Element *E = element_map.find(p_id);
    ERR_FAIL_COND_V(!E, -1);
    return E->get().subindex;
}

#define OCTREE_DIVISOR 4

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_insert_element(Element *p_element, Octant *p_octant) {

    real_t element_size = p_element->aabb.get_longest_axis_size() * 1.01; // avoid precision issues

    // don't create new child octants unless there is more than a certain number in
    // this octant. This prevents runaway creation of too many octants, and is more efficient
    // because brute force is faster up to a certain point.
    bool can_split = true;

    if (p_element->pairable) {
        if (p_octant->pairable_elements.size() < octant_elements_limit)
            can_split = false;
    } else {
        if (p_octant->elements.size() < octant_elements_limit)
            can_split = false;
    }

    if (!can_split || (element_size > (p_octant->aabb.size.x / OCTREE_DIVISOR))) {

        /* at smallest possible size for the element  */
        typename Element::OctantOwner owner;
        owner.octant = p_octant;

        if (use_pairs && p_element->pairable) {
            owner.E = p_octant->pairable_elements.insert(p_octant->pairable_elements.end(),p_element);
        } else {
            owner.E = p_octant->elements.insert(p_octant->elements.end(),p_element);
        }
        if constexpr(cached_lists) {
            p_octant->dirty = true;
        }
        p_element->octant_owners.push_back(owner);

        if (p_element->common_parent == nullptr) {
            p_element->common_parent = p_octant;
            p_element->container_aabb = p_octant->aabb;
        } else {
            p_element->container_aabb.merge_with(p_octant->aabb);
        }

        if (use_pairs && p_octant->children_count > 0) {

            pass++; //elements below this only get ONE reference added

            for (int i = 0; i < 8; i++) {

                if (p_octant->children[i]) {
                    _pair_element(p_element, (Octant *)p_octant->children[i]);
                }
            }
        }
    } else {
        /* not big enough, send it to subitems */
        int splits = 0;
        bool candidate = p_element->common_parent == nullptr;

        for (int i = 0; i < 8; i++) {

            if (p_octant->children[i]) {
                /* element exists, go straight to it */
                if (p_octant->children[i]->aabb.intersects_inclusive(p_element->aabb)) {
                    _insert_element(p_element, (Octant *)p_octant->children[i]);
                    splits++;
                }
            } else {
                /* check against AABB where child should be */

                AABB aabb = p_octant->aabb;
                aabb.size *= 0.5;

                if (i & 1)
                    aabb.position.x += aabb.size.x;
                if (i & 2)
                    aabb.position.y += aabb.size.y;
                if (i & 4)
                    aabb.position.z += aabb.size.z;

                if (aabb.intersects_inclusive(p_element->aabb)) {
                    /* if actually intersects, create the child */

                    Octant *child = memnew(Octant);
                    p_octant->children[i] = child;
                    child->parent = p_octant;
                    child->parent_index = i;

                    child->aabb = aabb;

                    p_octant->children_count++;

                    _insert_element(p_element, child);
                    octant_count++;
                    splits++;
                }
            }
        }

        if (candidate && splits > 1) {

            p_element->common_parent = p_octant;
        }
    }

    if (use_pairs) {

        for(auto &E : p_octant->pairable_elements) {
            _pair_reference(p_element, E);
        }

        if (p_element->pairable) {
            // and always test non-pairable if element is pairable
            for(auto &E : p_octant->elements) {
                _pair_reference(p_element, E);
            }
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_ensure_valid_root(const AABB &p_aabb) {

    if (!root) {
        // octre is empty

        AABB base(Vector3(), Vector3(1.0, 1.0, 1.0) * unit_size);

        while (!base.encloses(p_aabb)) {

            if (ABS(base.position.x + base.size.x) <= ABS(base.position.x)) {
                /* grow towards positive */
                base.size *= 2.0;
            } else {
                base.position -= base.size;
                base.size *= 2.0;
            }
        }

        root = memnew(Octant);

        root->parent = nullptr;
        root->parent_index = -1;
        root->aabb = base;

        octant_count++;

    } else {

        AABB base = root->aabb;

        while (!base.encloses(p_aabb)) {

            ERR_FAIL_COND_MSG(base.size.x > OCTREE_SIZE_LIMIT, "Octree upper size limit reached, does the AABB supplied contain NAN?");

            Octant *gp = memnew(Octant);
            octant_count++;
            root->parent = gp;

            if (ABS(base.position.x + base.size.x) <= ABS(base.position.x)) {
                /* grow towards positive */
                base.size *= 2.0;
                gp->aabb = base;
                gp->children[0] = root;
                root->parent_index = 0;
            } else {
                base.position -= base.size;
                base.size *= 2.0;
                gp->aabb = base;
                gp->children[(1 << 0) | (1 << 1) | (1 << 2)] = root; // add at all-positive
                root->parent_index = (1 << 0) | (1 << 1) | (1 << 2);
            }

            gp->children_count = 1;
            root = gp;
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
bool OctreeTpl<T,cached_lists,use_pairs>::_remove_element_pair_and_remove_empty_octants(Element *p_element, Octant *p_octant, Octant *p_limit) {

    bool octant_removed = false;

    while (true) {

        // check all exit conditions

        if (p_octant == p_limit) // reached limit, nothing to erase, exit
            return octant_removed;

        bool unpaired = false;

        if (use_pairs && p_octant->last_pass != pass) {
            // check whether we should unpair stuff
            // always test pairable
            for(auto &E : p_octant->pairable_elements) {
                _pair_unreference(p_element, E);
            }
            if (p_element->pairable) {
                // and always test non-pairable if element is pairable
                for(auto &E : p_octant->elements) {
                    _pair_unreference(p_element, E);
                }
            }
            p_octant->last_pass = pass;
            unpaired = true;
        }

        bool removed = false;

        Octant *parent = (Octant *)p_octant->parent;

        if (p_octant->children_count == 0 && p_octant->elements.empty() && p_octant->pairable_elements.empty()) {

            // erase octant

            if (p_octant == root) { // won't have a parent, just erase

                root = nullptr;
            } else {
                ERR_FAIL_INDEX_V(p_octant->parent_index, 8, octant_removed);

                parent->children[p_octant->parent_index] = nullptr;
                parent->children_count--;
            }

            memdelete<Octant>(p_octant);
            octant_count--;
            removed = true;
            octant_removed = true;
        }

        if (!removed && !unpaired)
            return octant_removed; // no reason to keep going up anymore! was already visited and was not removed

        p_octant = parent;
    }

    return octant_removed;
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_unpair_element(Element *p_element, Octant *p_octant) {

    // always test pairable
    auto E = p_octant->pairable_elements.begin();
    while (E!=p_octant->pairable_elements.end()) {
        if ((*E)->last_pass != pass) { // only remove ONE reference
            _pair_unreference(p_element, *E);
            (*E)->last_pass = pass;
        }
        ++E;
    }

    if (p_element->pairable) {
        // and always test non-pairable if element is pairable
        for(auto E = p_octant->elements.begin(); E!=p_octant->elements.end(); ++E) {
            if ((*E)->last_pass != pass) { // only remove ONE reference
                _pair_unreference(p_element, *E);
                (*E)->last_pass = pass;
            }
        }
    }

    p_octant->last_pass = pass;

    if (p_octant->children_count == 0)
        return; // small optimization for leafs

    for (int i = 0; i < 8; i++) {

        if (p_octant->children[i])
            _unpair_element(p_element, (Octant *)p_octant->children[i]);
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_pair_element(Element *p_element, Octant *p_octant) {

    // always test pairable

    auto E = p_octant->pairable_elements.begin();

    while (E!=p_octant->pairable_elements.end()) {

        if ((*E)->last_pass != pass) { // only get ONE reference
            _pair_reference(p_element, (*E));
            (*E)->last_pass = pass;
        }
        ++E;
    }

    if (p_element->pairable) {
        // and always test non-pairable if element is pairable

        for(auto E = p_octant->elements.begin();E!=p_octant->elements.end(); ++E) {
            if ((*E)->last_pass != pass) { // only get ONE reference
                _pair_reference(p_element, (*E));
                (*E)->last_pass = pass;
            }
        }
    }
    p_octant->last_pass = pass;

    if (p_octant->children_count == 0)
        return; // small optimization for leafs

    for (int i = 0; i < 8; i++) {

        if (p_octant->children[i])
            _pair_element(p_element, (Octant *)p_octant->children[i]);
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_remove_element(Element *p_element) {

    pass++; // will do a new pass for this

    auto I = p_element->octant_owners.begin();

    for (; I!=p_element->octant_owners.end(); ++I) {

        Octant *o = I->octant;

        if (!use_pairs) {
            o->elements.erase(I->E);
        } else {
            // erase children pairs, they are erased ONCE even if repeated
            pass++;
            for (int i = 0; i < 8; i++) {
                if (o->children[i]) {
                    _unpair_element(p_element, (Octant *)o->children[i]);
                }
            }

            if (p_element->pairable) {
                o->pairable_elements.erase(I->E);
            } else {
                o->elements.erase(I->E);
            }
        }
        if constexpr(cached_lists)
            o->dirty = true;

        _remove_element_pair_and_remove_empty_octants(p_element, o);
    }

    p_element->octant_owners.clear();

    if (use_pairs) {

        int remaining = p_element->pair_list.size();
        //p_element->pair_list.clear();
        ERR_FAIL_COND(remaining);
    }
}

template <class T, bool cached_lists,bool use_pairs>
OctreeElementID OctreeTpl<T,cached_lists,use_pairs>::create(T *p_userdata, const AABB &p_aabb, int p_subindex, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {

// check for AABB validity
#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_V(p_aabb.position.x > 1e15f || p_aabb.position.x < -1e15f, 0);
    ERR_FAIL_COND_V(p_aabb.position.y > 1e15f || p_aabb.position.y < -1e15f, 0);
    ERR_FAIL_COND_V(p_aabb.position.z > 1e15f || p_aabb.position.z < -1e15f, 0);
    ERR_FAIL_COND_V(p_aabb.size.x > 1e15f || p_aabb.size.x < 0.0f, 0);
    ERR_FAIL_COND_V(p_aabb.size.y > 1e15f || p_aabb.size.y < 0.0f, 0);
    ERR_FAIL_COND_V(p_aabb.size.z > 1e15f || p_aabb.size.z < 0.0f, 0);
    ERR_FAIL_COND_V(Math::is_nan(p_aabb.size.x), 0);
    ERR_FAIL_COND_V(Math::is_nan(p_aabb.size.y), 0);
    ERR_FAIL_COND_V(Math::is_nan(p_aabb.size.z), 0);

#endif
    typename ElementMap::iterator E = element_map.emplace(last_element_id++,
            Element()).first;
    Element &e = E->second;

    e.aabb = p_aabb;
    e.userdata = p_userdata;
    e.subindex = p_subindex;
    e.last_pass = 0;
    e.octree = this;
    e.pairable = p_pairable;
    e.pairable_type = p_pairable_type;
    e.pairable_mask = p_pairable_mask;
    e._id = last_element_id - 1;

    if (!e.aabb.has_no_surface()) {
        _ensure_valid_root(p_aabb);
        _insert_element(&e, root);
        if (use_pairs)
            _element_check_pairs(&e);
    }

    return last_element_id - 1;
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::move(OctreeElementID p_id, const AABB &p_aabb) {

#ifdef DEBUG_ENABLED
    // check for AABB validity
    ERR_FAIL_COND(p_aabb.position.x > 1e15 || p_aabb.position.x < -1e15);
    ERR_FAIL_COND(p_aabb.position.y > 1e15 || p_aabb.position.y < -1e15);
    ERR_FAIL_COND(p_aabb.position.z > 1e15 || p_aabb.position.z < -1e15);
    ERR_FAIL_COND(p_aabb.size.x > 1e15 || p_aabb.size.x < 0.0);
    ERR_FAIL_COND(p_aabb.size.y > 1e15 || p_aabb.size.y < 0.0);
    ERR_FAIL_COND(p_aabb.size.z > 1e15 || p_aabb.size.z < 0.0);
    ERR_FAIL_COND(Math::is_nan(p_aabb.size.x));
    ERR_FAIL_COND(Math::is_nan(p_aabb.size.y));
    ERR_FAIL_COND(Math::is_nan(p_aabb.size.z));
#endif
    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());
    Element &e = E->second;

    bool old_has_surf = !e.aabb.has_no_surface();
    bool new_has_surf = !p_aabb.has_no_surface();

    if (old_has_surf != new_has_surf) {

        if (old_has_surf) {
            _remove_element(&e); // removing
            e.common_parent = nullptr;
            e.aabb = AABB();
            _optimize();
        } else {
            _ensure_valid_root(p_aabb); // inserting
            e.common_parent = nullptr;
            e.aabb = p_aabb;
            _insert_element(&e, root);
            if (use_pairs)
                _element_check_pairs(&e);
        }

        return;
    }

    if (!old_has_surf) // doing nothing
        return;

    // it still is enclosed in the same AABB it was assigned to
    if (e.container_aabb.encloses(p_aabb)) {

        e.aabb = p_aabb;
        if (use_pairs)
            _element_check_pairs(&e); // must check pairs anyway

#ifdef OCTREE_USE_CACHED_LISTS
        e.moving();
#endif
        return;
    }

    AABB combined = e.aabb;
    combined.merge_with(p_aabb);
    _ensure_valid_root(combined);

    ERR_FAIL_COND(e.octant_owners.empty());

    /* FIND COMMON PARENT */

    List<typename Element::OctantOwner> owners = e.octant_owners; // save the octant owners
    Octant *common_parent = e.common_parent;
    ERR_FAIL_COND(!common_parent);

    //src is now the place towards where insertion is going to happen
    pass++;

    while (common_parent && !common_parent->aabb.encloses(p_aabb))
        common_parent = (Octant *)common_parent->parent;

    ERR_FAIL_COND(!common_parent);

    //prepare for reinsert
    e.octant_owners.clear();
    e.common_parent = nullptr;
    e.aabb = p_aabb;

    _insert_element(&e, common_parent); // reinsert from this point

    pass++;

    for (auto F = owners.begin(); F!=owners.end();) {

        Octant *o = F->octant;
        auto N = F;
        ++N;

        /*
        if (!use_pairs)
            o->elements.erase( F->get().E );
        */

        if (use_pairs && e.pairable)
            o->pairable_elements.erase(F->E);
        else
            o->elements.erase(F->E);

#ifdef OCTREE_USE_CACHED_LISTS
        o->dirty = true;
#endif

        if (_remove_element_pair_and_remove_empty_octants(&e, o, (Octant *)common_parent->parent)) {

            owners.erase(F);
        }

        F = N;
    }

    if (use_pairs) {
        //unpair child elements in anything that survived
        for (auto & F : owners) {

            Octant *o = F.octant;

            // erase children pairs, unref ONCE
            pass++;
            for (int i = 0; i < 8; i++) {

                if (o->children[i])
                    _unpair_element(&e, (Octant *)o->children[i]);
            }
        }

        _element_check_pairs(&e);
    }

    _optimize();
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::set_pairable(OctreeElementID p_id, bool p_pairable, uint32_t p_pairable_type, uint32_t p_pairable_mask) {

    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());

    Element &e = E->second;

    if (p_pairable == e.pairable && e.pairable_type == p_pairable_type && e.pairable_mask == p_pairable_mask)
        return; // no changes, return

    if (!e.aabb.has_no_surface()) {
        _remove_element(&e);
    }

    e.pairable = p_pairable;
    e.pairable_type = p_pairable_type;
    e.pairable_mask = p_pairable_mask;
    e.common_parent = nullptr;

    if (!e.aabb.has_no_surface()) {
        _ensure_valid_root(e.aabb);
        _insert_element(&e, root);
        if (use_pairs)
            _element_check_pairs(&e);
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::erase(OctreeElementID p_id) {

    auto E = element_map.find(p_id);
    ERR_FAIL_COND(E==element_map.end());

    Element &e = E->second;

    if (!e.aabb.has_no_surface()) {

        _remove_element(&e);
    }

    element_map.erase(p_id);
    _optimize();
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_cull_convex(Octant *p_octant, _CullConvexData *p_cull) {

    if (*p_cull->result_idx == p_cull->result_max)
        return; //pointless

    if (!p_octant->elements.empty()) {
        if constexpr(cached_lists) {
            // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
            p_octant->update_cached_lists();

            int num_elements = p_octant->clist.elements.size();
            for (int n = 0; n < num_elements; n++) {
                const AABB &aabb = p_octant->clist.aabbs[n];
                Element *e = p_octant->clist.elements[n];

                // in most cases with the cached linear  list tests we will do the AABB checks BEFORE last pass and cull mask.
                // The reason is that the later checks are more expensive because they are not in cache, and many of the AABB
                // tests will fail so we can avoid these cache misses.
                if (aabb.intersects_convex_shape(p_cull->planes, p_cull->points)) {

                    if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_cull->mask)))
                        continue;
                    e->last_pass = pass;

                    if (*p_cull->result_idx < p_cull->result_max) {
                        p_cull->result_array[*p_cull->result_idx] = e->userdata;
                        (*p_cull->result_idx)++;
                    } else {
                        return; // pointless to continue
                    }
                }
            } // for n
        } else {
            auto I = p_octant->elements.begin();

            for (; I!=p_octant->elements.end(); ++I) {

                Element *e = *I;
                const AABB &aabb = e->aabb;

                if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_cull->mask)))
                    continue;
                e->last_pass = pass;

                if (aabb.intersects_convex_shape(p_cull->planes, p_cull->points)) {
                    if (*p_cull->result_idx < p_cull->result_max) {
                        p_cull->result_array[*p_cull->result_idx] = e->userdata;
                        (*p_cull->result_idx)++;
                    } else {

                        return; // pointless to continue
                    }
                }
            }
        }
    } // if elements not empty

    if (use_pairs && !p_octant->pairable_elements.empty()) {
        if constexpr(cached_lists) {
            // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
            p_octant->update_cached_lists();

            int num_elements = p_octant->clist_pairable.elements.size();
            for (int n = 0; n < num_elements; n++) {
                const AABB &aabb = p_octant->clist_pairable.aabbs[n];
                Element *e = p_octant->clist_pairable.elements[n];

                if (aabb.intersects_convex_shape(p_cull->planes, p_cull->points)) {

                    if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_cull->mask)))
                        continue;
                    e->last_pass = pass;

                    if (*p_cull->result_idx < p_cull->result_max) {
                        p_cull->result_array[*p_cull->result_idx] = e->userdata;
                        (*p_cull->result_idx)++;
                    } else {

                        return; // pointless to continue
                    }
                }
            }
        }
        else {
            auto I = p_octant->pairable_elements.begin();

            for (; I!=p_octant->pairable_elements.end(); ++I) {

                Element *e = *I;
                const AABB &aabb = e->aabb;

                if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_cull->mask)))
                    continue;
                e->last_pass = pass;

                if (aabb.intersects_convex_shape(p_cull->planes, p_cull->points)) {

                    if (*p_cull->result_idx < p_cull->result_max) {

                        p_cull->result_array[*p_cull->result_idx] = e->userdata;
                        (*p_cull->result_idx)++;
                    } else {

                        return; // pointless to continue
                    }
                }
            }
        }
    }

    for (int i = 0; i < 8; i++) {

        if (p_octant->children[i] && p_octant->children[i]->aabb.intersects_convex_shape(p_cull->planes, p_cull->points)) {
            _cull_convex((Octant *)p_octant->children[i], p_cull);
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_cull_aabb(Octant *p_octant, const AABB &p_aabb, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (*p_result_idx == p_result_max)
        return; //pointless

    if (!p_octant->elements.empty()) {
        if constexpr(cached_lists) {
            // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
            p_octant->update_cached_lists();

            int num_elements = p_octant->clist.elements.size();
            for (int n = 0; n < num_elements; n++) {
                const AABB &aabb = p_octant->clist.aabbs[n];
                Element *e = p_octant->clist.elements[n];

                if (p_aabb.intersects_inclusive(aabb)) {

                    if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                        continue;
                    e->last_pass = pass;

                    if (*p_result_idx < p_result_max) {

                        p_result_array[*p_result_idx] = e->userdata;
                        if (p_subindex_array)
                            p_subindex_array[*p_result_idx] = e->subindex;

                        (*p_result_idx)++;
                    } else {

                        return; // pointless to continue
                    }
                }
            }
        }
        else {
            auto I = p_octant->elements.begin();
            for (; I!=p_octant->elements.end(); ++I) {

                Element *e = *I;
                const AABB &aabb = e->aabb;

                if (p_aabb.intersects_inclusive(aabb)) {

                    if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                        continue;
                    e->last_pass = pass;

                    if (*p_result_idx < p_result_max) {

                        p_result_array[*p_result_idx] = e->userdata;
                        if (p_subindex_array)
                            p_subindex_array[*p_result_idx] = e->subindex;

                        (*p_result_idx)++;
                    } else {

                        return; // pointless to continue
                    }
                }
            }
        }
    }

    if (use_pairs && !p_octant->pairable_elements.empty()) {
if constexpr(cached_lists) {
        // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
        p_octant->update_cached_lists();

        int num_elements = p_octant->clist_pairable.elements.size();
        for (int n = 0; n < num_elements; n++) {
            const AABB &aabb = p_octant->clist_pairable.aabbs[n];
            Element *e = p_octant->clist_pairable.elements[n];

            if (p_aabb.intersects_inclusive(aabb)) {

                if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                    continue;
                e->last_pass = pass;

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;
                } else {

                    return; // pointless to continue
                }
            }
        }
} else {

        auto I = p_octant->pairable_elements.begin();
        for (; I!=p_octant->pairable_elements.end(); ++I) {

            Element *e = *I;
            const AABB &aabb = e->aabb;

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;
            e->last_pass = pass;

            if (p_aabb.intersects_inclusive(aabb)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;
                } else {

                    return; // pointless to continue
                }
            }
        }
}
    }

    for (int i = 0; i < 8; i++) {

        if (p_octant->children[i] && p_octant->children[i]->aabb.intersects_inclusive(p_aabb)) {
            _cull_aabb((Octant *)p_octant->children[i], p_aabb, p_result_array, p_result_idx, p_result_max, p_subindex_array, p_mask);
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_cull_segment(Octant *p_octant, const Vector3 &p_from, const Vector3 &p_to, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (*p_result_idx == p_result_max)
        return; //pointless

    if (!p_octant->elements.empty()) {

if constexpr(cached_lists) {
        // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
        p_octant->update_cached_lists();

        int num_elements = p_octant->clist.elements.size();
        for (int n = 0; n < num_elements; n++) {
            const AABB &aabb = p_octant->clist.aabbs[n];
            Element *e = p_octant->clist.elements[n];

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;
            e->last_pass = pass;

            if (aabb.intersects_segment(p_from, p_to)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
} else {

        auto I = p_octant->elements.begin();
        for (; I!=p_octant->elements.end(); ++I) {

            Element *e = *I;
            const AABB &aabb = e->aabb;

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;
            e->last_pass = pass;

            if (aabb.intersects_segment(p_from, p_to)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
}
    }

    if (use_pairs && !p_octant->pairable_elements.empty()) {

if constexpr(cached_lists) {
        // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
        p_octant->update_cached_lists();

        int num_elements = p_octant->clist_pairable.elements.size();
        for (int n = 0; n < num_elements; n++) {
            const AABB &aabb = p_octant->clist_pairable.aabbs[n];
            Element *e = p_octant->clist_pairable.elements[n];

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;

            e->last_pass = pass;

            if (aabb.intersects_segment(p_from, p_to)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;

                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
} else {
        for (auto I = p_octant->pairable_elements.begin(); I!=p_octant->pairable_elements.end(); ++I) {

            Element *e = *I;
            const AABB &aabb = e->aabb;

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;

            e->last_pass = pass;

            if (aabb.intersects_segment(p_from, p_to)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;

                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
}
    }

    for (int i = 0; i < 8; i++) {

        if (p_octant->children[i] && ((Octant *)p_octant->children[i])->aabb.intersects_segment(p_from, p_to)) {
            _cull_segment((Octant *)p_octant->children[i], p_from, p_to, p_result_array, p_result_idx, p_result_max, p_subindex_array, p_mask);
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::_cull_point(Octant *p_octant, const Vector3 &p_point, T **p_result_array, int *p_result_idx, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (*p_result_idx == p_result_max)
        return; //pointless

    if (!p_octant->elements.empty()) {

if constexpr(cached_lists) {
        // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
        p_octant->update_cached_lists();

        int num_elements = p_octant->clist.elements.size();
        for (int n = 0; n < num_elements; n++) {
            const AABB &aabb = p_octant->clist.aabbs[n];
            Element *e = p_octant->clist.elements[n];

            if (aabb.has_point(p_point)) {

                if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                    continue;
                e->last_pass = pass;

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
} else {
        auto I = p_octant->elements.begin();
        for (; I!=p_octant->elements.end(); ++I) {

            Element *e = *I;
            const AABB &aabb = e->aabb;

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;
            e->last_pass = pass;

            if (aabb.has_point(p_point)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;
                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
}
    }

    if (use_pairs && !p_octant->pairable_elements.empty()) {

if constexpr(cached_lists) {
        // make sure cached list of element pointers and aabbs is up to date if this octant is dirty
        p_octant->update_cached_lists();

        int num_elements = p_octant->clist_pairable.elements.size();
        for (int n = 0; n < num_elements; n++) {
            const AABB &aabb = p_octant->clist_pairable.aabbs[n];
            Element *e = p_octant->clist_pairable.elements[n];

            if (aabb.has_point(p_point)) {

                if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                    continue;

                e->last_pass = pass;

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;

                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
} else {
        for (auto I = p_octant->pairable_elements.begin(); I!=p_octant->pairable_elements.end(); ++I) {

            Element *e = *I;
            const AABB &aabb = e->aabb;

            if (e->last_pass == pass || (use_pairs && !(e->pairable_type & p_mask)))
                continue;

            e->last_pass = pass;

            if (aabb.has_point(p_point)) {

                if (*p_result_idx < p_result_max) {

                    p_result_array[*p_result_idx] = e->userdata;
                    if (p_subindex_array)
                        p_subindex_array[*p_result_idx] = e->subindex;

                    (*p_result_idx)++;

                } else {

                    return; // pointless to continue
                }
            }
        }
}
    }

    for (int i = 0; i < 8; i++) {

        //could be optimized..
        if (p_octant->children[i] && p_octant->children[i]->aabb.has_point(p_point)) {
            _cull_point(p_octant->children[i], p_point, p_result_array, p_result_idx, p_result_max, p_subindex_array, p_mask);
        }
    }
}

template <class T, bool cached_lists,bool use_pairs>
int OctreeTpl<T,cached_lists,use_pairs>::cull_convex(Span<const Plane,6> p_convex, T **p_result_array, int p_result_max, uint32_t p_mask) {

    if (!root || p_convex.empty())
        return 0;

    FixedVector<Vector3,8,false> convex_points {Geometry::compute_convex_mesh_points(p_convex)};
    if (convex_points.empty())
        return 0;

    int result_count = 0;
    pass++;
    _CullConvexData cdata;
    cdata.planes = p_convex;
    cdata.points = convex_points;
    cdata.result_array = p_result_array;
    cdata.result_max = p_result_max;
    cdata.result_idx = &result_count;
    cdata.mask = p_mask;

    _cull_convex(root, &cdata);

    return result_count;
}

template <class T, bool cached_lists,bool use_pairs>
int OctreeTpl<T,cached_lists,use_pairs>::cull_aabb(const AABB &p_aabb, T **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (!root)
        return 0;

    int result_count = 0;
    pass++;
    _cull_aabb(root, p_aabb, p_result_array, &result_count, p_result_max, p_subindex_array, p_mask);

    return result_count;
}

template <class T, bool cached_lists,bool use_pairs>
int OctreeTpl<T,cached_lists,use_pairs>::cull_segment(const Vector3 &p_from, const Vector3 &p_to, T **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (!root)
        return 0;

    int result_count = 0;
    pass++;
    _cull_segment(root, p_from, p_to, p_result_array, &result_count, p_result_max, p_subindex_array, p_mask);

    return result_count;
}

template <class T, bool cached_lists,bool use_pairs>
int OctreeTpl<T,cached_lists,use_pairs>::cull_point(const Vector3 &p_point, T **p_result_array, int p_result_max, int *p_subindex_array, uint32_t p_mask) {

    if (!root)
        return 0;

    int result_count = 0;
    pass++;
    _cull_point(root, p_point, p_result_array, &result_count, p_result_max, p_subindex_array, p_mask);

    return result_count;
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::set_pair_callback(PairCallback p_callback, void *p_userdata) {

    pair_callback = p_callback;
    pair_callback_userdata = p_userdata;
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::set_unpair_callback(UnpairCallback p_callback, void *p_userdata) {

    unpair_callback = p_callback;
    unpair_callback_userdata = p_userdata;
}

template <class T, bool cached_lists,bool use_pairs>
OctreeTpl<T,cached_lists,use_pairs>::OctreeTpl(real_t p_unit_size) {

    last_element_id = 1;
    pass = 1;
    unit_size = p_unit_size;
    root = nullptr;

    octant_count = 0;
    pair_count = 0;
    octant_elements_limit = OCTREE_DEFAULT_OCTANT_LIMIT;

    pair_callback = nullptr;
    unpair_callback = nullptr;
    pair_callback_userdata = nullptr;
    unpair_callback_userdata = nullptr;
}

#ifdef TOOLS_ENABLED
template <class T, bool cached_lists,bool use_pairs>
String OctreeTpl<T,cached_lists,use_pairs>::debug_aabb_to_string(const AABB &aabb) const {
    String sz;
    sz = "( " + String(aabb.position);
    sz += " ) - ( ";
    Vector3 max = aabb.position + aabb.size;
    sz += String(max) + " )";
    return sz;
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::debug_octants() {
    if (root)
        debug_octant(*root);
}

template <class T, bool cached_lists,bool use_pairs>
void OctreeTpl<T,cached_lists,use_pairs>::debug_octant(const Octant &oct, int depth) {
    String sz = "";
    for (int d = 0; d < depth; d++)
        sz += "\t";

    sz += "Octant " + debug_aabb_to_string(oct.aabb);
    sz += "\tnum_children " + itos(oct.children_count);
    sz += ", num_eles " + itos(oct.elements.size());
    sz += ", num_paired_eles" + itos(oct.pairable_elements.size());
    print_line(sz);

    for (int n = 0; n < 8; n++) {
        const Octant *pChild = oct.children[n];
        if (pChild) {
            debug_octant(*pChild, depth + 1);
        }
    }
}
#endif // TOOLS_ENABLED

// standard octree
template<class T,bool pairing=false>
using Octree = OctreeTpl<T,false,pairing>;

// cached lists octree
template<class T,bool pairing=false>
using Octree_CL = OctreeTpl<T,true,pairing>;

