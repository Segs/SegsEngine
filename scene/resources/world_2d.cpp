/*************************************************************************/
/*  world_2d.cpp                                                         */
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

#include "world_2d.h"

#include "core/method_bind.h"
#include "core/project_settings.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/visibility_notifier_2d.h"
#include "scene/main/viewport.h"
#include "servers/navigation_2d_server.h"
#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(World2D)

struct SpatialIndexer2D {
    struct CellRef {
        int ref=0;

        _FORCE_INLINE_ int inc() {
            ref++;
            return ref;
        }
        _FORCE_INLINE_ int dec() {
            ref--;
            return ref;
        }
    };

    struct CellKey {
        union {
            struct {
                int32_t x;
                int32_t y;
            };
            uint64_t key;
        };

        bool operator==(const CellKey &p_key) const { return key == p_key.key; }
        _FORCE_INLINE_ bool operator<(const CellKey &p_key) const { return key < p_key.key; }

    private:
        friend eastl::hash<CellKey>;
        explicit operator size_t() const { return key; }
    };

    struct CellData {
        HashMap<VisibilityNotifier2D *, CellRef> notifiers;
    };

    HashMap<CellKey, CellData> cells;
    int cell_size;

    HashMap<VisibilityNotifier2D *, Rect2> notifiers;

    struct ViewportData {
        HashMap<VisibilityNotifier2D *, uint64_t> notifiers;
        Rect2 rect;
    };

    HashMap<Viewport *, ViewportData> viewports;

    bool changed;

    uint64_t pass;

    void _notifier_update_cells(VisibilityNotifier2D *p_notifier, const Rect2 &p_rect, bool p_add) {
        Point2i begin = p_rect.position;
        begin /= cell_size;
        Point2i end = p_rect.position + p_rect.size;
        end /= cell_size;
        for (int i = begin.x; i <= end.x; i++) {
            for (int j = begin.y; j <= end.y; j++) {
                CellKey ck;
                ck.x = i;
                ck.y = j;
                HashMap<CellKey, CellData>::iterator E = cells.find(ck);

                if (p_add) {
                    if (E == cells.end()) {
                        E = cells.emplace(ck, CellData()).first;
                    }
                    E->second.notifiers[p_notifier].inc();
                } else {
                    ERR_CONTINUE(E == cells.end());
                    if (E->second.notifiers[p_notifier].dec() == 0) {
                        E->second.notifiers.erase(p_notifier);
                        if (E->second.notifiers.empty()) {
                            cells.erase(E);
                        }
                    }
                }
            }
        }
    }

    void _notifier_add(VisibilityNotifier2D *p_notifier, const Rect2 &p_rect) {
        ERR_FAIL_COND(notifiers.contains(p_notifier));
        notifiers[p_notifier] = p_rect;
        _notifier_update_cells(p_notifier, p_rect, true);
        changed = true;
    }

    void _notifier_update(VisibilityNotifier2D *p_notifier, const Rect2 &p_rect) {
        HashMap<VisibilityNotifier2D *, Rect2>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E == notifiers.end());
        if (E->second == p_rect) {
            return;
        }

        _notifier_update_cells(p_notifier, p_rect, true);
        _notifier_update_cells(p_notifier, E->second, false);
        E->second = p_rect;
        changed = true;
    }

    void _notifier_remove(VisibilityNotifier2D *p_notifier) {
        HashMap<VisibilityNotifier2D *, Rect2>::iterator E = notifiers.find(p_notifier);
        ERR_FAIL_COND(E == notifiers.end());
        _notifier_update_cells(p_notifier, E->second, false);
        notifiers.erase(p_notifier);

        Vector<Viewport *> removed;
        for (eastl::pair<Viewport *const, ViewportData> &F : viewports) {
            HashMap<VisibilityNotifier2D *, uint64_t>::const_iterator G = F.second.notifiers.find(p_notifier);

            if (G != F.second.notifiers.end()) {
                F.second.notifiers.erase(G);
                removed.push_back(F.first);
            }
        }
        for (Viewport *vp : removed) {
            p_notifier->_exit_viewport(vp);
        }

        changed = true;
    }

    void _add_viewport(Viewport *p_viewport, const Rect2 &p_rect) {
        ERR_FAIL_COND(viewports.contains(p_viewport));
        ViewportData vd;
        vd.rect = p_rect;
        viewports[p_viewport] = vd;
        changed = true;
    }

    void _update_viewport(Viewport *p_viewport, const Rect2 &p_rect) {
        HashMap<Viewport *, ViewportData>::iterator E = viewports.find(p_viewport);
        ERR_FAIL_COND(E == viewports.end());
        if (E->second.rect == p_rect) {
            return;
        }
        E->second.rect = p_rect;
        changed = true;
    }

    void _remove_viewport(Viewport *p_viewport) {
        ERR_FAIL_COND(!viewports.contains(p_viewport));
        Vector<VisibilityNotifier2D *> removed;
        for (auto &E : viewports[p_viewport].notifiers) {
            removed.push_back(E.first);
        }
        for (VisibilityNotifier2D *n : removed) {
            n->_exit_viewport(p_viewport);
        }

        viewports.erase(p_viewport);
    }

    void _update() {
        if (!changed) {
            return;
        }

        for (eastl::pair<Viewport *const, ViewportData> &E : viewports) {
            Point2i begin = E.second.rect.position;
            begin /= cell_size;
            Point2i end = E.second.rect.position + E.second.rect.size;
            end /= cell_size;
            pass++;
            Vector<VisibilityNotifier2D *> added;
            Vector<VisibilityNotifier2D *> removed;

            uint64_t visible_cells = (uint64_t)(end.x - begin.x) * (uint64_t)(end.y - begin.y);

            if (visible_cells > 10000) {
                // well you zoomed out a lot, it's your problem. To avoid freezing in the for loops below, we'll
                // manually check cell by cell

                for (eastl::pair<const CellKey, CellData> &F : cells) {
                    const CellKey &ck = F.first;

                    if (ck.x < begin.x || ck.x > end.x) {
                        continue;
                    }
                    if (ck.y < begin.y || ck.y > end.y) {
                        continue;
                    }

                    // notifiers in cell
                    for (auto &G : F.second.notifiers) {
                        HashMap<VisibilityNotifier2D *, uint64_t>::iterator H = E.second.notifiers.find(G.first);
                        if (H == E.second.notifiers.end()) {
                            H = E.second.notifiers.emplace(G.first, pass).first;
                            added.push_back(G.first);
                        } else {
                            H->second = pass;
                        }
                    }
                }

            } else {
                // check cells in grid fashion
                for (int i = begin.x; i <= end.x; i++) {
                    for (int j = begin.y; j <= end.y; j++) {
                        CellKey ck;
                        ck.x = i;
                        ck.y = j;

                        HashMap<CellKey, CellData>::iterator F = cells.find(ck);
                        if (F == cells.end()) {
                            continue;
                        }

                        // notifiers in cell
                        for (auto &G : F->second.notifiers) {
                            auto H = E.second.notifiers.find(G.first);
                            if (H == E.second.notifiers.end()) {
                                H = E.second.notifiers.emplace(G.first, pass).first;
                                added.push_back(G.first);
                            } else {
                                H->second = pass;
                            }
                        }
                    }
                }
            }

            for (auto &F : E.second.notifiers) {
                if (F.second != pass) {
                    removed.push_back(F.first);
                }
            }

            for (VisibilityNotifier2D *vn : added) {
                vn->_enter_viewport(E.first);
            }

            for (VisibilityNotifier2D *vn : removed) {
                E.second.notifiers.erase(vn);
                vn->_exit_viewport(E.first);
            }
        }

        changed = false;
    }

    SpatialIndexer2D() {
        pass = 0;
        changed = false;
        cell_size = T_GLOBAL_DEF<int>("world/2d/cell_size", 100);
    }
};

void World2D::_register_viewport(Viewport *p_viewport, const Rect2 &p_rect) {
    indexer->_add_viewport(p_viewport, p_rect);
}

void World2D::_update_viewport(Viewport *p_viewport, const Rect2 &p_rect) {
    indexer->_update_viewport(p_viewport, p_rect);
}
void World2D::_remove_viewport(Viewport *p_viewport) {
    indexer->_remove_viewport(p_viewport);
}

void World2D::_register_notifier(VisibilityNotifier2D *p_notifier, const Rect2 &p_rect) {
    indexer->_notifier_add(p_notifier, p_rect);
}
void World2D::_update_notifier(VisibilityNotifier2D *p_notifier, const Rect2 &p_rect) {
    indexer->_notifier_update(p_notifier, p_rect);
}
void World2D::_remove_notifier(VisibilityNotifier2D *p_notifier) {
    indexer->_notifier_remove(p_notifier);
}

void World2D::_update() {
    indexer->_update();
}

RenderingEntity World2D::get_canvas() {
    return canvas;
}

RID World2D::get_space() {
    return space;
}

RID World2D::get_navigation_map() const {
    return navigation_map;
}
void World2D::get_viewport_list(Vector<Viewport *> *r_viewports) {
    for (const eastl::pair<Viewport *const, SpatialIndexer2D::ViewportData> &E : indexer->viewports) {
        r_viewports->push_back(E.first);
    }
}

void World2D::_bind_methods() {
    SE_BIND_METHOD(World2D,get_canvas);
    SE_BIND_METHOD(World2D,get_space);
    SE_BIND_METHOD(World2D,get_navigation_map);

    SE_BIND_METHOD(World2D,get_direct_space_state);

    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "canvas", PropertyHint::None, "", 0), "", "get_canvas");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "space", PropertyHint::None, "", 0), "", "get_space");
    ADD_PROPERTY(PropertyInfo(VariantType::_RID, "navigation_map", PropertyHint::None, "", 0), "", "get_navigation_map");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "direct_space_state", PropertyHint::ResourceType,
                         "PhysicsDirectSpaceState2D", 0),
            "", "get_direct_space_state");
}

PhysicsDirectSpaceState2D *World2D::get_direct_space_state() {
    return PhysicsServer2D::get_singleton()->space_get_direct_state(space);
}

World2D::World2D() {
    canvas = RenderingServer::get_singleton()->canvas_create();
    space = PhysicsServer2D::get_singleton()->space_create();

    // set space2D to be more friendly with pixels than meters, by adjusting some constants
    PhysicsServer2D::get_singleton()->space_set_active(space, true);
    PhysicsServer2D::get_singleton()->area_set_param(
            space, PhysicsServer2D::AREA_PARAM_GRAVITY, GLOBAL_DEF("physics/2d/default_gravity", 98));
    PhysicsServer2D::get_singleton()->area_set_param(space, PhysicsServer2D::AREA_PARAM_GRAVITY_VECTOR,
            GLOBAL_DEF("physics/2d/default_gravity_vector", Vector2(0, 1)));
    PhysicsServer2D::get_singleton()->area_set_param(
            space, PhysicsServer2D::AREA_PARAM_LINEAR_DAMP, GLOBAL_DEF("physics/2d/default_linear_damp", 0.1));
    ProjectSettings::get_singleton()->set_custom_property_info(
            "physics/2d/default_linear_damp", PropertyInfo(VariantType::FLOAT, "physics/2d/default_linear_damp",
                                                      PropertyHint::Range, "-1,100,0.001,or_greater"));
    PhysicsServer2D::get_singleton()->area_set_param(
            space, PhysicsServer2D::AREA_PARAM_ANGULAR_DAMP, GLOBAL_DEF("physics/2d/default_angular_damp", 1.0));
    ProjectSettings::get_singleton()->set_custom_property_info(
            "physics/2d/default_angular_damp", PropertyInfo(VariantType::FLOAT, "physics/2d/default_angular_damp",
                                                       PropertyHint::Range, "-1,100,0.001,or_greater"));
        // Create default navigation map
    navigation_map = Navigation2DServer::get_singleton()->map_create();
    Navigation2DServer::get_singleton()->map_set_active(navigation_map, true);
    Navigation2DServer::get_singleton()->map_set_cell_size(navigation_map, T_GLOBAL_DEF("navigation/2d/default_cell_size", 1.0f));
    Navigation2DServer::get_singleton()->map_set_cell_height(navigation_map, T_GLOBAL_DEF("navigation/2d/default_cell_height", 1.0f));
    Navigation2DServer::get_singleton()->map_set_edge_connection_margin(
            navigation_map, T_GLOBAL_DEF("navigation/2d/default_edge_connection_margin", 1.0f));
    indexer = memnew(SpatialIndexer2D);
}

World2D::~World2D() {
    RenderingServer::get_singleton()->free_rid(canvas);
    PhysicsServer2D::get_singleton()->free_rid(space);
    Navigation2DServer::get_singleton()->free_rid(navigation_map);
    memdelete(indexer);
}
