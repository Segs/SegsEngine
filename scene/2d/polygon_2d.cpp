/*************************************************************************/
/*  polygon_2d.cpp                                                       */
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

#include "polygon_2d.h"

#include "scene/resources/texture.h"
#include "skeleton_2d.h"

#include "core/callable_method_pointer.h"
#include "core/dictionary.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/object_db.h"
#include "core/object_tooling.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(Polygon2D)
#ifdef TOOLS_ENABLED
Dictionary Polygon2D::_edit_get_state() const {
    Dictionary state = Node2D::_edit_get_state();
    state["offset"] = offset;
    return state;
}

void Polygon2D::_edit_set_state(const Dictionary &p_state) {
    Node2D::_edit_set_state(p_state);
    set_offset(p_state["offset"].as<Vector2>());
}

void Polygon2D::_edit_set_pivot(const Point2 &p_pivot) {
    set_position(get_transform().xform(p_pivot));
    set_offset(get_offset() - p_pivot);
}

Point2 Polygon2D::_edit_get_pivot() const {
    return Vector2();
}

bool Polygon2D::_edit_use_pivot() const {
    return true;
}

Rect2 Polygon2D::_edit_get_rect() const {
    if (rect_cache_dirty) {
        int l = polygon.size();
        PoolVector<Vector2>::Read r = polygon.read();
        item_rect = Rect2();
        for (int i = 0; i < l; i++) {
            Vector2 pos = r[i] + offset;
            if (i == 0)
                item_rect.position = pos;
            else
                item_rect.expand_to(pos);
        }
        rect_cache_dirty = false;
    }

    return item_rect;
}

bool Polygon2D::_edit_use_rect() const {
    return polygon.size() > 0;
}

bool Polygon2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    Span<const Vector2> polygon2d(polygon.read().ptr(),polygon.size());
    if (internal_vertices > 0) {
        polygon2d = polygon2d.first(polygon2d.size() - internal_vertices);
    }
    return Geometry::is_point_in_polygon(p_point - get_offset(), polygon2d);
}
#endif

void Polygon2D::_skeleton_bone_setup_changed() {
    update();
}

void Polygon2D::_notification(int p_what) {

    switch (p_what) {

        case NOTIFICATION_DRAW: {

            if (polygon.size() < 3)
                return;

            Skeleton2D *skeleton_node = nullptr;
            if (has_node(skeleton)) {
                skeleton_node = object_cast<Skeleton2D>(get_node(skeleton));
            }

            GameEntity new_skeleton_id {entt::null};

            if (skeleton_node) {
                RenderingServer::get_singleton()->canvas_item_attach_skeleton(get_canvas_item(), skeleton_node->get_skeleton());
                new_skeleton_id = skeleton_node->get_instance_id();
            } else {
                RenderingServer::get_singleton()->canvas_item_attach_skeleton(get_canvas_item(), entt::null);
            }

            if (new_skeleton_id != current_skeleton_id) {
                Object *old_skeleton = object_for_entity(current_skeleton_id);
                if (old_skeleton) {
                    old_skeleton->disconnect("bone_setup_changed",callable_mp(this, &ClassName::_skeleton_bone_setup_changed));
                }

                if (skeleton_node) {
                    skeleton_node->connect("bone_setup_changed",callable_mp(this, &ClassName::_skeleton_bone_setup_changed));
                }

                current_skeleton_id = new_skeleton_id;
            }

            //TODO: make local vectors use stack allocation/ FixedVector ?
            Vector<Vector2> points;
            Vector<Vector2> uvs;
            PoolVector<int> bones;
            PoolVector<float> weights;

            int len = polygon.size();
            if ((invert || polygons.empty()) && internal_vertices > 0) {
                //if no polygons are around, internal vertices must not be drawn, else let them be
                len -= internal_vertices;
            }

            if (len <= 0) {
                return;
            }
            points.resize(len);

            {

                PoolVector<Vector2>::Read polyr = polygon.read();
                for (int i = 0; i < len; i++) {
                    points[i] = polyr[i] + offset;
                }
            }

            if (invert) {

                Rect2 bounds;
                int highest_idx = -1;
                float highest_y = -1e20f;
                float sum = 0;

                for (int i = 0; i < len; i++) {
                    if (i == 0)
                        bounds.position = points[i];
                    else
                        bounds.expand_to(points[i]);
                    if (points[i].y > highest_y) {
                        highest_idx = i;
                        highest_y = points[i].y;
                    }
                    int ni = (i + 1) % len;
                    sum += (points[ni].x - points[i].x) * (points[ni].y + points[i].y);
                }

                bounds.grow_by(invert_border);

                Vector2 ep[7] = {
                    Vector2(points[highest_idx].x, points[highest_idx].y + invert_border),
                    Vector2(bounds.position + bounds.size),
                    Vector2(bounds.position + Vector2(bounds.size.x, 0)),
                    Vector2(bounds.position),
                    Vector2(bounds.position + Vector2(0, bounds.size.y)),
                    Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y + invert_border),
                    Vector2(points[highest_idx].x - CMP_EPSILON, points[highest_idx].y),
                };

                if (sum > 0) {
                    SWAP(ep[1], ep[4]);
                    SWAP(ep[2], ep[3]);
                    SWAP(ep[5], ep[0]);
                    SWAP(ep[6], points[highest_idx]);
                }

                points.resize(points.size() + 7);
                for (int i = points.size() - 1; i >= highest_idx + 7; i--) {

                    points[i] = points[i - 7];
                }

                for (int i = 0; i < 7; i++) {

                    points[highest_idx + i + 1] = ep[i];
                }

                len = points.size();
            }

            if (texture) {

                Transform2D texmat(tex_rot, tex_ofs);
                texmat.scale(tex_scale);
                Size2 tex_size = texture->get_size();

                uvs.resize(len);
                auto &uv_wr(uvs);
                if (points.size() == uv.size()) {

                    PoolVector<Vector2>::Read uvr = uv.read();

                    for (int i = 0; i < len; i++) {
                        uv_wr[i] = texmat.xform(uvr[i]) / tex_size;
                    }

                } else {
                    for (int i = 0; i < len; i++) {
                        uv_wr[i] = texmat.xform(points[i]) / tex_size;
                    }
                }
            }

            if (skeleton_node && !invert && !bone_weights.empty()) {
                //a skeleton is set! fill indices and weights
                int vc = len;
                bones.resize(vc * 4);
                weights.resize(vc * 4);

                auto bonesw = bones.write();
                auto weightsw = weights.write();

                for (int i = 0; i < vc * 4; i++) {
                    bonesw[i] = 0;
                    weightsw[i] = 0;
                }

                for (int i = 0; i < bone_weights.size(); i++) {
                    if (bone_weights[i].weights.size() != points.size()) {
                        continue; //different number of vertices, sorry not using.
                    }
                    if (!skeleton_node->has_node(bone_weights[i].path)) {
                        continue; //node does not exist
                    }
                    Bone2D *bone = object_cast<Bone2D>(skeleton_node->get_node(bone_weights[i].path));
                    if (!bone) {
                        continue;
                    }

                    int bone_index = bone->get_index_in_skeleton();
                    PoolVector<float>::Read r = bone_weights[i].weights.read();
                    for (int j = 0; j < vc; j++) {
                        if (r[j] == 0.0)
                            continue; //weight is unpainted, skip
                        //find an index with a weight
                        for (int k = 0; k < 4; k++) {
                            if (weightsw[j * 4 + k] < r[j]) {
                                //this is less than this weight, insert weight!
                                for (int l = 3; l > k; l--) {
                                    weightsw[j * 4 + l] = weightsw[j * 4 + l - 1];
                                    bonesw[j * 4 + l] = bonesw[j * 4 + l - 1];
                                }
                                weightsw[j * 4 + k] = r[j];
                                bonesw[j * 4 + k] = bone_index;
                                break;
                            }
                        }
                    }
                }

                //normalize the weights
                for (int i = 0; i < vc; i++) {
                    float tw = 0;
                    for (int j = 0; j < 4; j++) {
                        tw += weightsw[i * 4 + j];
                    }
                    if (tw == 0)
                        continue; //unpainted, do nothing

                    //normalize
                    for (int j = 0; j < 4; j++) {
                        weightsw[i * 4 + j] /= tw;
                    }
                }
            }

            Color single_color[1] = {color};
            Span<const Color> colors;
            if (vertex_colors.size() == points.size()) {
                colors = vertex_colors;
            } else {
                colors = single_color;
            }

//            Vector<int> indices = Geometry::triangulate_polygon(points);
//            RenderingServer::get_singleton()->canvas_item_add_triangle_array(get_canvas_item(), indices, points, colors, uvs, texture ? texture->get_rid() : RID());

            if (invert || polygons.empty()) {
                Vector<int> indices(Geometry::triangulate_polygon(points));
                if (!indices.empty()) {
                    RenderingServer::get_singleton()->canvas_item_add_triangle_array(get_canvas_item(), indices, points, colors,
                            uvs, bones, weights, texture ? texture->get_rid() : entt::null, -1, entt::null, antialiased);
                }
            } else {
                //draw individual polygons
                Vector<int> total_indices;
                for (int i = 0; i < polygons.size(); i++) {
                    PoolVector<int> src_indices = polygons[i].as<PoolVector<int>>();
                    int ic = src_indices.size();
                    if (ic < 3)
                        continue;
                    PoolVector<int>::Read r = src_indices.read();

                    Vector<Vector2> tmp_points;
                    tmp_points.resize(ic);

                    for (int j = 0; j < ic; j++) {
                        int idx = r[j];
                        ERR_CONTINUE(idx < 0 || idx >= points.size());
                        tmp_points[j] = points[r[j]];
                    }
                    Vector<int> indices = Geometry::triangulate_polygon(tmp_points);
                    int ic2 = indices.size();
                    const int *r2 = indices.data();

                    int bic = total_indices.size();
                    total_indices.resize(bic + ic2);
                    int *w2 = total_indices.data();

                    for (int j = 0; j < ic2; j++) {
                        w2[j + bic] = r[r2[j]];
                    }
                }

                if (!total_indices.empty()) {
                    RenderingServer::get_singleton()->canvas_item_add_triangle_array(get_canvas_item(), total_indices, points, colors, uvs,
                            bones, weights, texture ? texture->get_rid() : entt::null, -1, entt::null, antialiased);
                }
            }
        } break;
    }
}

void Polygon2D::set_polygon(const PoolVector<Vector2> &p_polygon) {
    polygon = p_polygon;
    rect_cache_dirty = true;
    update();
}

PoolVector<Vector2> Polygon2D::get_polygon() const {

    return polygon;
}

void Polygon2D::set_internal_vertex_count(int p_count) {

    internal_vertices = p_count;
}

int Polygon2D::get_internal_vertex_count() const {
    return internal_vertices;
}

void Polygon2D::set_uv(const PoolVector<Vector2> &p_uv) {

    uv = p_uv;
    update();
}

PoolVector<Vector2> Polygon2D::get_uv() const {

    return uv;
}

void Polygon2D::set_polygons(const Array &p_polygons) {

    polygons = p_polygons;
    update();
}

Array Polygon2D::get_polygons() const {

    return polygons;
}

void Polygon2D::set_color(const Color &p_color) {

    color = p_color;
    update();
}
Color Polygon2D::get_color() const {

    return color;
}

void Polygon2D::set_vertex_colors(const Vector<Color> &p_colors) {

    vertex_colors = p_colors;
    update();
}
const Vector<Color> &Polygon2D::get_vertex_colors() const {

    return vertex_colors;
}

void Polygon2D::set_texture(const Ref<Texture> &p_texture) {

    texture = p_texture;

    /*if (texture) {
        uint32_t flags=texture->get_flags();
        flags&=~Texture::FLAG_REPEAT;
        if (tex_tile)
            flags|=Texture::FLAG_REPEAT;

        texture->set_flags(flags);
    }*/
    update();
}
Ref<Texture> Polygon2D::get_texture() const {

    return texture;
}

void Polygon2D::set_texture_offset(const Vector2 &p_offset) {

    tex_ofs = p_offset;
    update();
}
Vector2 Polygon2D::get_texture_offset() const {

    return tex_ofs;
}

void Polygon2D::set_texture_rotation(float p_rot) {

    tex_rot = p_rot;
    update();
}
float Polygon2D::get_texture_rotation() const {

    return tex_rot;
}

void Polygon2D::set_texture_rotation_degrees(float p_rot) {

    set_texture_rotation(Math::deg2rad(p_rot));
}
float Polygon2D::get_texture_rotation_degrees() const {

    return Math::rad2deg(get_texture_rotation());
}

void Polygon2D::set_texture_scale(const Size2 &p_scale) {

    tex_scale = p_scale;
    update();
}
Size2 Polygon2D::get_texture_scale() const {

    return tex_scale;
}

void Polygon2D::set_invert(bool p_invert) {

    invert = p_invert;
    update();
}
bool Polygon2D::get_invert() const {

    return invert;
}

void Polygon2D::set_antialiased(bool p_antialiased) {

    antialiased = p_antialiased;
    update();
}
bool Polygon2D::get_antialiased() const {

    return antialiased;
}

void Polygon2D::set_invert_border(float p_invert_border) {

    invert_border = p_invert_border;
    update();
}
float Polygon2D::get_invert_border() const {

    return invert_border;
}

void Polygon2D::set_offset(const Vector2 &p_offset) {

    offset = p_offset;
    rect_cache_dirty = true;
    update();
    Object_change_notify(this,"offset");
}

Vector2 Polygon2D::get_offset() const {

    return offset;
}

void Polygon2D::add_bone(const NodePath &p_path, const PoolVector<float> &p_weights) {

    Bone bone;
    bone.path = p_path;
    bone.weights = p_weights;
    bone_weights.emplace_back(bone);
}
int Polygon2D::get_bone_count() const {
    return bone_weights.size();
}
NodePath Polygon2D::get_bone_path(int p_index) const {
    ERR_FAIL_INDEX_V(p_index, bone_weights.size(), NodePath());
    return bone_weights[p_index].path;
}
PoolVector<float> Polygon2D::get_bone_weights(int p_index) const {

    ERR_FAIL_INDEX_V(p_index, bone_weights.size(), PoolVector<float>());
    return bone_weights[p_index].weights;
}
void Polygon2D::erase_bone(int p_idx) {

    ERR_FAIL_INDEX(p_idx, bone_weights.size());
    bone_weights.erase_at(p_idx);
}

void Polygon2D::clear_bones() {
    bone_weights.clear();
}

void Polygon2D::set_bone_weights(int p_index, const PoolVector<float> &p_weights) {
    ERR_FAIL_INDEX(p_index, bone_weights.size());
    bone_weights[p_index].weights = p_weights;
    update();
}
void Polygon2D::set_bone_path(int p_index, const NodePath &p_path) {
    ERR_FAIL_INDEX(p_index, bone_weights.size());
    bone_weights[p_index].path = p_path;
    update();
}

Array Polygon2D::_get_bones() const {
    Array bones;
    for (int i = 0; i < get_bone_count(); i++) {
        // Convert path property to String to avoid errors due to invalid node path in editor,
        // because it's relative to the Skeleton2D node and not Polygon2D.
        bones.push_back(String(get_bone_path(i)));
        bones.push_back(get_bone_weights(i));
    }
    return bones;
}
void Polygon2D::_set_bones(const Array &p_bones) {

    ERR_FAIL_COND(p_bones.size() & 1);
    clear_bones();
    for (int i = 0; i < p_bones.size(); i += 2) {
        // Convert back from String to NodePath.
        add_bone(NodePath(p_bones[i].as<String>()), p_bones[i + 1].as< PoolVector<float>>());
    }
}

void Polygon2D::set_skeleton(const NodePath &p_skeleton) {
    if (skeleton == p_skeleton)
        return;
    skeleton = p_skeleton;
    update();
}

NodePath Polygon2D::get_skeleton() const {
    return skeleton;
}

void Polygon2D::_bind_methods() {

    SE_BIND_METHOD(Polygon2D,set_polygon);
    SE_BIND_METHOD(Polygon2D,get_polygon);

    SE_BIND_METHOD(Polygon2D,set_uv);
    SE_BIND_METHOD(Polygon2D,get_uv);

    SE_BIND_METHOD(Polygon2D,set_color);
    SE_BIND_METHOD(Polygon2D,get_color);

    SE_BIND_METHOD(Polygon2D,set_polygons);
    SE_BIND_METHOD(Polygon2D,get_polygons);

    SE_BIND_METHOD(Polygon2D,set_vertex_colors);
    SE_BIND_METHOD(Polygon2D,get_vertex_colors);

    SE_BIND_METHOD(Polygon2D,set_texture);
    SE_BIND_METHOD(Polygon2D,get_texture);

    SE_BIND_METHOD(Polygon2D,set_texture_offset);
    SE_BIND_METHOD(Polygon2D,get_texture_offset);

    SE_BIND_METHOD(Polygon2D,set_texture_rotation);
    SE_BIND_METHOD(Polygon2D,get_texture_rotation);

    SE_BIND_METHOD(Polygon2D,set_texture_rotation_degrees);
    SE_BIND_METHOD(Polygon2D,get_texture_rotation_degrees);

    SE_BIND_METHOD(Polygon2D,set_texture_scale);
    SE_BIND_METHOD(Polygon2D,get_texture_scale);

    SE_BIND_METHOD(Polygon2D,set_invert);
    SE_BIND_METHOD(Polygon2D,get_invert);

    SE_BIND_METHOD(Polygon2D,set_antialiased);
    SE_BIND_METHOD(Polygon2D,get_antialiased);

    SE_BIND_METHOD(Polygon2D,set_invert_border);
    SE_BIND_METHOD(Polygon2D,get_invert_border);

    SE_BIND_METHOD(Polygon2D,set_offset);
    SE_BIND_METHOD(Polygon2D,get_offset);

    SE_BIND_METHOD(Polygon2D,add_bone);
    SE_BIND_METHOD(Polygon2D,get_bone_count);
    SE_BIND_METHOD(Polygon2D,get_bone_path);
    SE_BIND_METHOD(Polygon2D,get_bone_weights);
    SE_BIND_METHOD(Polygon2D,erase_bone);
    SE_BIND_METHOD(Polygon2D,clear_bones);
    SE_BIND_METHOD(Polygon2D,set_bone_path);
    SE_BIND_METHOD(Polygon2D,set_bone_weights);

    SE_BIND_METHOD(Polygon2D,set_skeleton);
    SE_BIND_METHOD(Polygon2D,get_skeleton);

    SE_BIND_METHOD(Polygon2D,set_internal_vertex_count);
    SE_BIND_METHOD(Polygon2D,get_internal_vertex_count);

    SE_BIND_METHOD(Polygon2D,_set_bones);
    SE_BIND_METHOD(Polygon2D,_get_bones);

    SE_BIND_METHOD(Polygon2D,_skeleton_bone_setup_changed);

    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "color"), "set_color", "get_color");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "antialiased"), "set_antialiased", "get_antialiased");
    ADD_GROUP("Texture", "texture_");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture_data", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "texture_offset"), "set_texture_offset", "get_texture_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "texture_scale"), "set_texture_scale", "get_texture_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "texture_rotation_degrees", PropertyHint::Range, "-360,360,0.1,or_lesser,or_greater"), "set_texture_rotation_degrees", "get_texture_rotation_degrees");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "texture_rotation", PropertyHint::None, "", 0), "set_texture_rotation", "get_texture_rotation");
    ADD_GROUP("Skeleton", "");
    ADD_PROPERTY(PropertyInfo(VariantType::NODE_PATH, "skeleton", PropertyHint::NodePathValidTypes, "Skeleton2D"), "set_skeleton", "get_skeleton");

    ADD_GROUP("Invert", "invert_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "invert_enable"), "set_invert", "get_invert");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "invert_border", PropertyHint::Range, "0.1,16384,0.1"), "set_invert_border", "get_invert_border");

    ADD_GROUP("Data", "");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "polygon"), "set_polygon", "get_polygon");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_VECTOR2_ARRAY, "uv"), "set_uv", "get_uv");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_COLOR_ARRAY, "vertex_colors"), "set_vertex_colors", "get_vertex_colors");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "polygons"), "set_polygons", "get_polygons");
    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "bones", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "_set_bones", "_get_bones");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "internal_vertex_count", PropertyHint::Range, "0,1000"), "set_internal_vertex_count", "get_internal_vertex_count");
}

Polygon2D::Polygon2D() {

    invert = false;
    invert_border = 100;
    antialiased = false;
    tex_rot = 0;
    tex_tile = true;
    tex_scale = Vector2(1, 1);
    color = Color(1, 1, 1);
    rect_cache_dirty = true;
    internal_vertices = 0;
    current_skeleton_id = entt::null;
}
