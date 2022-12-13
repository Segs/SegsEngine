/*************************************************************************/
/*  sprite_3d.cpp                                                        */
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

#include "sprite_3d.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/math/triangle_mesh.h"
#include "core/translation_helpers.h"
#include "scene/scene_string_names.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(SpriteBase3D)
IMPL_GDCLASS(Sprite3D)
IMPL_GDCLASS(AnimatedSprite3D)
//TODO: SEGS: copied from material.cpp
VARIANT_ENUM_CAST(SpatialMaterial::BillboardMode);
VARIANT_ENUM_CAST(SpriteBase3D::DrawFlags);
VARIANT_ENUM_CAST(SpriteBase3D::AlphaCutMode);

Color SpriteBase3D::_get_color_accum() {

    if (!color_dirty)
        return color_accum;

    if (parent_sprite)
        color_accum = parent_sprite->_get_color_accum();
    else
        color_accum = Color(1, 1, 1, 1);

    color_accum.r *= modulate.r;
    color_accum.g *= modulate.g;
    color_accum.b *= modulate.b;
    color_accum.a *= modulate.a;
    color_dirty = false;
    return color_accum;
}

void SpriteBase3D::_propagate_color_changed() {

    if (color_dirty)
        return;

    color_dirty = true;
    _queue_update();

    for (SpriteBase3D * child : children) {

        child->_propagate_color_changed();
    }
}

void SpriteBase3D::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        if (!pending_update)
            _im_update();

        parent_sprite = object_cast<SpriteBase3D>(get_parent());
        if (parent_sprite) {
            parent_sprite->children.emplace_back(this);
        }
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {

        if (parent_sprite) {

            parent_sprite->children.erase_first(this);
            parent_sprite = nullptr;
        }
    }
}

void SpriteBase3D::draw_texture_rect(const Ref<Texture> &p_texture, Rect2 p_dst_rect, Rect2 p_src_rect) {
    ERR_FAIL_COND(!p_texture);

    Rect2 final_rect;
    Rect2 final_src_rect;
    if (!p_texture->get_rect_region(p_dst_rect, p_src_rect, final_rect, final_src_rect)) {
        return;
    }

    if (final_rect.size.x == 0 || final_rect.size.y == 0) {
        return;
    }

    // 2D:                                                     3D plane (axes match exactly when `axis == Vector3::AXIS_Z`):
    //   -X+                                                     -X+
    //  -                                                       +
    //  Y  +--------+       +--------+       +--------+         Y  +--------+
    //  +  | +--+   |       |        |  (2)  |        |         -  | 0--1   |
    //     | |ab|   |  (1)  | +--+   |  (3)  | 3--2   |            | |ab|   |
    //     | |cd|   |  -->  | |ab|   |  -->  | |cd|   |    <==>    | |cd|   |
    //     | +--+   |       | |cd|   |       | |ab|   |            | 3--2   |
    //     |        |       | +--+   |       | 0--1   |            |        |
    //     +--------+       +--------+       +--------+            +--------+

    // (1) Y-wise shift `final_rect` within `p_dst_rect` so after inverting Y
    // axis distances between top/bottom borders will be preserved (so for
    // example AtlasTextures with vertical margins will look the same in 2D/3D).
    final_rect.position.y = (p_dst_rect.position.y + p_dst_rect.size.y) - ((final_rect.position.y + final_rect.size.y) - p_dst_rect.position.y);

    Color color = _get_color_accum();
    color.a *= get_opacity();

    float pixel_size = get_pixel_size();

    // (2) Order vertices (0123) bottom-top in 2D / top-bottom in 3D.
    Vector2 vertices[4] = {
        (final_rect.position + Vector2(0, final_rect.size.y)) * pixel_size,
        (final_rect.position + final_rect.size) * pixel_size,
        (final_rect.position + Vector2(final_rect.size.x, 0)) * pixel_size,
        final_rect.position * pixel_size,
    };

    Vector2 src_tsize = p_texture->get_size();

    // Properly setup UVs for impostor textures (AtlasTexture).
    Ref<AtlasTexture> atlas_tex = dynamic_ref_cast<AtlasTexture>(p_texture);
    if (atlas_tex) {
        src_tsize[0] = atlas_tex->get_atlas()->get_width();
        src_tsize[1] = atlas_tex->get_atlas()->get_height();
    }

    // (3) Assign UVs (abcd) according to the vertices order (bottom-top in 2D / top-bottom in 3D).
    Vector2 uvs[4] = {
        final_src_rect.position / src_tsize,
        (final_src_rect.position + Vector2(final_src_rect.size.x, 0)) / src_tsize,
        (final_src_rect.position + final_src_rect.size) / src_tsize,
        (final_src_rect.position + Vector2(0, final_src_rect.size.y)) / src_tsize,
    };

    if (is_flipped_h()) {
        SWAP(uvs[0], uvs[1]);
        SWAP(uvs[2], uvs[3]);
    }
    if (is_flipped_v()) {
        SWAP(uvs[0], uvs[3]);
        SWAP(uvs[1], uvs[2]);
    }

    Vector3 normal;
    int axis = get_axis();
    normal[axis] = 1.0;

    Plane tangent;
    if (axis == Vector3::AXIS_X) {
        tangent = Plane(0, 0, -1, -1);
    } else {
        tangent = Plane(1, 0, 0, -1);
    }

    int x_axis = ((axis + 1) % 3);
    int y_axis = ((axis + 2) % 3);

    if (axis != Vector3::AXIS_Z) {
        SWAP(x_axis, y_axis);

        for (int i = 0; i < 4; i++) {
            // uvs[i] = Vector2(1.0,1.0)-uvs[i];
            // SWAP(vertices[i].x,vertices[i].y);
            if (axis == Vector3::AXIS_Y) {
                vertices[i].y = -vertices[i].y;
            } else if (axis == Vector3::AXIS_X) {
                vertices[i].x = -vertices[i].x;
            }
        }
    }

    AABB aabb;

    // Everything except position, color, and UV is compressed
    PoolVector<uint8_t>::Write write_buffer = mesh_buffer.write();
    RenderingServer *rs = RenderingServer::get_singleton();
    Vector2 normal_oct = rs->norm_to_oct(normal);
    int8_t v_normal[2] = {
        (int8_t)CLAMP<float>(normal_oct.x * 127, -128, 127),
        (int8_t)CLAMP<float>(normal_oct.y * 127, -128, 127),
    };

    Vector2 tangent_oct = rs->tangent_to_oct(tangent.normal, tangent.d, false);
    int8_t v_tangent[2] = {
        (int8_t)CLAMP<float>(tangent_oct.x * 127, -128, 127),
        (int8_t)CLAMP<float>(tangent_oct.y * 127, -128, 127),
    };

    for (int i = 0; i < 4; i++) {
        Vector3 vtx;
        vtx[x_axis] = vertices[i][0];
        vtx[y_axis] = vertices[i][1];
        if (i == 0) {
            aabb.position = vtx;
            aabb.size = Vector3();
        } else {
            aabb.expand_to(vtx);
        }

        float v_uv[2] = { uvs[i].x, uvs[i].y };
        memcpy(&write_buffer[i * mesh_stride[RS::ARRAY_TEX_UV] + mesh_surface_offsets[RS::ARRAY_TEX_UV]], v_uv, 8);

        float v_vertex[3] = { vtx.x, vtx.y, vtx.z };
        memcpy(&write_buffer[i * mesh_stride[RS::ARRAY_VERTEX] + mesh_surface_offsets[RS::ARRAY_VERTEX]], &v_vertex, sizeof(float) * 3);
        memcpy(&write_buffer[i * mesh_stride[RS::ARRAY_NORMAL] + mesh_surface_offsets[RS::ARRAY_NORMAL]], v_normal, 2);
        memcpy(&write_buffer[i * mesh_stride[RS::ARRAY_TANGENT] + mesh_surface_offsets[RS::ARRAY_TANGENT]], v_tangent, 2);
        memcpy(&write_buffer[i * mesh_stride[RS::ARRAY_COLOR] + mesh_surface_offsets[RS::ARRAY_COLOR]], color.components(), 4 * 4);
    }

    write_buffer.release();

    RenderingEntity mesh = get_mesh();
    rs->mesh_surface_update_region(mesh, 0, 0, mesh_buffer);

    rs->mesh_set_custom_aabb(mesh, aabb);
    set_aabb(aabb);

    RenderingEntity mat = SpatialMaterial::get_material_rid_for_2d(get_draw_flag(FLAG_SHADED), get_draw_flag(FLAG_TRANSPARENT), get_draw_flag(FLAG_DOUBLE_SIDED),
            get_alpha_cut_mode() == ALPHA_CUT_DISCARD, get_alpha_cut_mode() == ALPHA_CUT_OPAQUE_PREPASS,
            get_billboard_mode() == SpatialMaterial::BILLBOARD_ENABLED, get_billboard_mode() == SpatialMaterial::BILLBOARD_FIXED_Y,
            get_draw_flag(FLAG_DISABLE_DEPTH_TEST), get_draw_flag(FLAG_FIXED_SIZE));
    rs->material_set_shader(get_material(), rs->material_get_shader(mat));
    rs->material_set_param(get_material(), "texture_albedo", Variant::from(p_texture->get_rid()));
    if (get_alpha_cut_mode() == ALPHA_CUT_DISABLED) {
        rs->material_set_render_priority(get_material(), get_render_priority());
    }
    rs->instance_set_surface_material(get_instance(), 0, get_material());
}
void SpriteBase3D::set_centered(bool p_center) {

    centered = p_center;
    _queue_update();
}

bool SpriteBase3D::is_centered() const {

    return centered;
}

void SpriteBase3D::set_offset(const Point2 &p_offset) {

    offset = p_offset;
    _queue_update();
}
Point2 SpriteBase3D::get_offset() const {

    return offset;
}

void SpriteBase3D::set_flip_h(bool p_flip) {

    hflip = p_flip;
    _queue_update();
}
bool SpriteBase3D::is_flipped_h() const {

    return hflip;
}

void SpriteBase3D::set_flip_v(bool p_flip) {

    vflip = p_flip;
    _queue_update();
}
bool SpriteBase3D::is_flipped_v() const {

    return vflip;
}

void SpriteBase3D::set_modulate(const Color &p_color) {

    modulate = p_color;
    _propagate_color_changed();
    _queue_update();
}

Color SpriteBase3D::get_modulate() const {

    return modulate;
}

void SpriteBase3D::set_render_priority(int p_priority) {
    ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);
    render_priority = p_priority;
    _queue_update();
}

int SpriteBase3D::get_render_priority() const {
    return render_priority;
}

void SpriteBase3D::set_pixel_size(float p_amount) {

    pixel_size = p_amount;
    _queue_update();
}
float SpriteBase3D::get_pixel_size() const {

    return pixel_size;
}

void SpriteBase3D::set_opacity(float p_amount) {

    opacity = p_amount;
    _queue_update();
}
float SpriteBase3D::get_opacity() const {

    return opacity;
}

void SpriteBase3D::set_axis(Vector3::Axis p_axis) {

    ERR_FAIL_INDEX(p_axis, 3);
    axis = p_axis;
    _queue_update();
}
Vector3::Axis SpriteBase3D::get_axis() const {

    return axis;
}

void SpriteBase3D::_im_update() {

    _draw();

    pending_update = false;

    //texture->draw_rect_region(ci,dst_rect,src_rect,modulate);
}

void SpriteBase3D::_queue_update() {

    if (pending_update)
        return;

    triangle_mesh.unref();
    update_gizmo();

    pending_update = true;
    call_deferred([this] { _im_update(); });
}

AABB SpriteBase3D::get_aabb() const {

    return aabb;
}
Vector<Face3> SpriteBase3D::get_faces(uint32_t p_usage_flags) const {

    return Vector<Face3>();
}

Ref<TriangleMesh> SpriteBase3D::generate_triangle_mesh() const {
    if (triangle_mesh)
        return triangle_mesh;

    Vector<Vector3> faces;
    faces.resize(6);

    Rect2 final_rect = get_item_rect();

    if (final_rect.size.x == 0 || final_rect.size.y == 0)
        return Ref<TriangleMesh>();

    float pixel_size = get_pixel_size();

    Vector2 vertices[4] = {

        (final_rect.position + Vector2(0, final_rect.size.y)) * pixel_size,
        (final_rect.position + final_rect.size) * pixel_size,
        (final_rect.position + Vector2(final_rect.size.x, 0)) * pixel_size,
        final_rect.position * pixel_size,

    };

    int x_axis = ((axis + 1) % 3);
    int y_axis = ((axis + 2) % 3);

    if (axis != Vector3::AXIS_Z) {
        SWAP(x_axis, y_axis);

        for (int i = 0; i < 4; i++) {
            if (axis == Vector3::AXIS_Y) {
                vertices[i].y = -vertices[i].y;
            } else if (axis == Vector3::AXIS_X) {
                vertices[i].x = -vertices[i].x;
            }
        }
    }

    static const int indices[6] = {
        0, 1, 2,
        0, 2, 3
    };

    for (int j = 0; j < 6; j++) {
        int i = indices[j];
        Vector3 vtx;
        vtx[x_axis] = vertices[i][0];
        vtx[y_axis] = vertices[i][1];
        faces[j] = vtx;
    }

    triangle_mesh = make_ref_counted<TriangleMesh>();
    triangle_mesh->create(faces);

    return triangle_mesh;
}

void SpriteBase3D::set_draw_flag(DrawFlags p_flag, bool p_enable) {

    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    flags[p_flag] = p_enable;
    _queue_update();
}

bool SpriteBase3D::get_draw_flag(DrawFlags p_flag) const {
    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void SpriteBase3D::set_alpha_cut_mode(AlphaCutMode p_mode) {

    ERR_FAIL_INDEX(p_mode, 3);
    alpha_cut = p_mode;
    _queue_update();
}

SpriteBase3D::AlphaCutMode SpriteBase3D::get_alpha_cut_mode() const {

    return alpha_cut;
}

void SpriteBase3D::set_billboard_mode(SpatialMaterial::BillboardMode p_mode) {

    ERR_FAIL_INDEX(p_mode, 3);
    billboard_mode = p_mode;
    _queue_update();
}

SpatialMaterial::BillboardMode SpriteBase3D::get_billboard_mode() const {

    return billboard_mode;
}
void SpriteBase3D::_bind_methods() {

    SE_BIND_METHOD(SpriteBase3D,set_centered);
    SE_BIND_METHOD(SpriteBase3D,is_centered);

    SE_BIND_METHOD(SpriteBase3D,set_offset);
    SE_BIND_METHOD(SpriteBase3D,get_offset);

    SE_BIND_METHOD(SpriteBase3D,set_flip_h);
    SE_BIND_METHOD(SpriteBase3D,is_flipped_h);

    SE_BIND_METHOD(SpriteBase3D,set_flip_v);
    SE_BIND_METHOD(SpriteBase3D,is_flipped_v);

    SE_BIND_METHOD(SpriteBase3D,set_modulate);
    SE_BIND_METHOD(SpriteBase3D,get_modulate);

    SE_BIND_METHOD(SpriteBase3D,set_opacity);
    SE_BIND_METHOD(SpriteBase3D,get_opacity);

    SE_BIND_METHOD(SpriteBase3D,set_pixel_size);
    SE_BIND_METHOD(SpriteBase3D,get_pixel_size);

    SE_BIND_METHOD(SpriteBase3D,set_axis);
    SE_BIND_METHOD(SpriteBase3D,get_axis);

    SE_BIND_METHOD(SpriteBase3D,set_draw_flag);
    SE_BIND_METHOD(SpriteBase3D,get_draw_flag);

    SE_BIND_METHOD(SpriteBase3D,set_alpha_cut_mode);
    SE_BIND_METHOD(SpriteBase3D,get_alpha_cut_mode);
    SE_BIND_METHOD(SpriteBase3D,set_billboard_mode);
    SE_BIND_METHOD(SpriteBase3D,get_billboard_mode);

    SE_BIND_METHOD(SpriteBase3D,get_item_rect);
    SE_BIND_METHOD(SpriteBase3D,generate_triangle_mesh);

    SE_BIND_METHOD(SpriteBase3D,_im_update);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "centered"), "set_centered", "is_centered");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "modulate"), "set_modulate", "get_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "opacity", PropertyHint::Range, "0,1,0.01"), "set_opacity", "get_opacity");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pixel_size", PropertyHint::Range, "0.0001,128,0.0001"), "set_pixel_size", "get_pixel_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "axis", PropertyHint::Enum, "X-Axis,Y-Axis,Z-Axis"), "set_axis", "get_axis");
    ADD_GROUP("Flags", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "billboard", PropertyHint::Enum, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "transparent"), "set_draw_flag", "get_draw_flag", FLAG_TRANSPARENT);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "alpha_cut", PropertyHint::Enum, "Disabled,Discard,Opaque Pre-Pass"), "set_alpha_cut_mode", "get_alpha_cut_mode");

    BIND_ENUM_CONSTANT(FLAG_TRANSPARENT);
    BIND_ENUM_CONSTANT(FLAG_SHADED);
    BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
    BIND_ENUM_CONSTANT(FLAG_MAX);

    BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
    BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
    BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);
}

SpriteBase3D::SpriteBase3D() {

    color_dirty = true;
    centered = true;
    hflip = false;
    vflip = false;
    parent_sprite = nullptr;

    for (int i = 0; i < FLAG_MAX; i++)
        flags[i] = i == FLAG_TRANSPARENT || i == FLAG_DOUBLE_SIDED;

    alpha_cut = ALPHA_CUT_DISABLED;
    billboard_mode = SpatialMaterial::BILLBOARD_DISABLED;
    axis = Vector3::AXIS_Z;
    pixel_size = 0.01;
    modulate = Color(1, 1, 1, 1);
    pending_update = false;
    opacity = 1.0;
    auto ren_server = RenderingServer::get_singleton();
    material = RenderingServer::get_singleton()->material_create();
    // Set defaults for material, names need to match up those in SpatialMaterial
    ren_server->material_set_param(material, "albedo", Color(1, 1, 1, 1));
    ren_server->material_set_param(material, "specular", 0.5);
    ren_server->material_set_param(material, "metallic", 0.0);
    ren_server->material_set_param(material, "roughness", 1.0);
    ren_server->material_set_param(material, "uv1_offset", Vector3(0, 0, 0));
    ren_server->material_set_param(material, "uv1_scale", Vector3(1, 1, 1));
    ren_server->material_set_param(material, "uv2_offset", Vector3(0, 0, 0));
    ren_server->material_set_param(material, "uv2_scale", Vector3(1, 1, 1));
    ren_server->material_set_param(material, "alpha_scissor_threshold", 0.98);

    mesh = ren_server->mesh_create();

    Vector<Vector3> mesh_vertices;
    Vector<Vector3> mesh_normals;
    Vector<float> mesh_tangents;
    Vector<Color> mesh_colors;
    Vector<Vector2> mesh_uvs;

    // create basic mesh and store format information
    mesh_vertices.resize(4,Vector3(0.0f, 0.0f, 0.0f));
    mesh_normals.resize(4,Vector3(0.0, 0.0, 1.0));
    mesh_tangents.reserve(16);
    constexpr float tangents[4] = {0,0,1,1};
    for(int i=0; i<4; ++i) {
        mesh_tangents.insert(mesh_tangents.end(),eastl::begin(tangents),eastl::end(tangents));
    }
    mesh_colors.resize(4,Color(1.0, 1.0, 1.0, 1.0));
    mesh_uvs.resize(4,Vector2(0.0, 0.0));

    SurfaceArrays mesh_array(eastl::move(mesh_vertices));

    mesh_array.m_normals = eastl::move(mesh_normals);
    mesh_array.m_tangents = eastl::move(mesh_tangents);
    mesh_array.m_colors = eastl::move(mesh_colors);
    mesh_array.m_uv_1 = eastl::move(mesh_uvs);

    uint32_t compress_format = (RS::ARRAY_COMPRESS_DEFAULT & ~RS::ARRAY_COMPRESS_TEX_UV) & ~RS::ARRAY_COMPRESS_COLOR;
    compress_format |= RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE;

    ren_server->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLE_FAN, mesh_array, Vector<SurfaceArrays>(), compress_format);

    const int surface_vertex_len = ren_server->mesh_surface_get_array_len(mesh, 0);
    const int surface_index_len = ren_server->mesh_surface_get_array_index_len(mesh, 0);

    mesh_surface_format = ren_server->mesh_surface_get_format(mesh, 0);
    mesh_buffer = ren_server->mesh_surface_get_array(mesh, 0);
    ren_server->mesh_surface_make_offsets_from_format(mesh_surface_format, surface_vertex_len, surface_index_len, mesh_surface_offsets, mesh_stride);
    set_base(mesh);
}

SpriteBase3D::~SpriteBase3D() {

    RenderingServer::get_singleton()->free_rid(mesh);
    RenderingServer::get_singleton()->free_rid(material);
}

///////////////////////////////////////////

void Sprite3D::_draw() {
    if (get_base() != get_mesh()) {
        set_base(get_mesh());
    }

    set_base(entt::null);

    if (not texture) {
        return;
    }

    Vector2 tsize = texture->get_size();
    if (tsize.x == 0.0f || tsize.y == 0.0f)
        return;

    Rect2 base_rect;
    if (region)
        base_rect = region_rect;
    else
        base_rect = Rect2(0, 0, texture->get_width(), texture->get_height());

    Size2 frame_size = base_rect.size / Size2(hframes, vframes);
    Point2 frame_offset = Point2(frame % hframes, frame / hframes);
    frame_offset *= frame_size;

    Point2 dst_offset = get_offset();
    if (is_centered()) {
        dst_offset -= frame_size / 2;
    }

    Rect2 src_rect(base_rect.position + frame_offset, frame_size);
    Rect2 dst_rect(dst_offset, frame_size);

    draw_texture_rect(texture, dst_rect, src_rect);
    }

void Sprite3D::_texture_changed() {
    _queue_update();
}
void Sprite3D::set_texture(const Ref<Texture> &p_texture) {

    if (p_texture == texture)
        return;
    if (texture) {
        texture->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Sprite3D::_texture_changed));
    }
    texture = p_texture;
    if (texture) {
        texture->set_flags(texture->get_flags()); //remove repeat from texture, it looks bad in sprites
        texture->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Sprite3D::_texture_changed));
    }
    _queue_update();
}

Ref<Texture> Sprite3D::get_texture() const {

    return texture;
}

void Sprite3D::set_region(bool p_region) {

    if (p_region == region)
        return;

    region = p_region;
    _queue_update();
}

bool Sprite3D::is_region() const {

    return region;
}

void Sprite3D::set_region_rect(const Rect2 &p_region_rect) {

    bool changed = region_rect != p_region_rect;
    region_rect = p_region_rect;
    if (region && changed) {
        _queue_update();
    }
}

Rect2 Sprite3D::get_region_rect() const {

    return region_rect;
}

void Sprite3D::set_frame(int p_frame) {

    ERR_FAIL_INDEX(p_frame, int64_t(vframes) * hframes);

    _queue_update();

    Object_change_notify(this,"frame");
    Object_change_notify(this,"frame_coords");
    emit_signal(SceneStringNames::frame_changed);
}

int Sprite3D::get_frame() const {

    return frame;
}

void Sprite3D::set_frame_coords(const Vector2 &p_coord) {
    ERR_FAIL_INDEX(int(p_coord.x), hframes);
    ERR_FAIL_INDEX(int(p_coord.y), vframes);

    set_frame(int(p_coord.y) * hframes + int(p_coord.x));
}

Vector2 Sprite3D::get_frame_coords() const {
    return Vector2(frame % hframes, frame / hframes);
}

void Sprite3D::set_vframes(int p_amount) {

    ERR_FAIL_COND(p_amount < 1);
    vframes = p_amount;
    _queue_update();
    Object_change_notify(this);
}
int Sprite3D::get_vframes() const {

    return vframes;
}

void Sprite3D::set_hframes(int p_amount) {

    ERR_FAIL_COND(p_amount < 1);
    hframes = p_amount;
    _queue_update();
    Object_change_notify(this);
}
int Sprite3D::get_hframes() const {

    return hframes;
}

Rect2 Sprite3D::get_item_rect() const {

    if (not texture)
        return Rect2(0, 0, 1, 1);
    /*
    if (not texture)
        return CanvasItem::get_item_rect();
    */

    Size2 s;

    if (region) {

        s = region_rect.size;
    } else {
        s = texture->get_size();
        s = s / Point2(hframes, vframes);
    }

    Point2 ofs = get_offset();
    if (is_centered())
        ofs -= s / 2;

    if (s == Size2(0, 0))
        s = Size2(1, 1);

    return Rect2(ofs, s);
}

void Sprite3D::_validate_property(PropertyInfo &property) const {

    if (property.name == "frame") {
        property.hint = PropertyHint::Range;
        property.hint_string = "0," + itos(vframes * hframes - 1) + ",1";
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
    if (property.name == "frame_coords") {
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
}

void Sprite3D::_bind_methods() {

    SE_BIND_METHOD(Sprite3D,set_texture);
    SE_BIND_METHOD(Sprite3D,get_texture);

    SE_BIND_METHOD(Sprite3D,set_region);
    SE_BIND_METHOD(Sprite3D,is_region);

    SE_BIND_METHOD(Sprite3D,set_region_rect);
    SE_BIND_METHOD(Sprite3D,get_region_rect);

    SE_BIND_METHOD(Sprite3D,set_frame);
    SE_BIND_METHOD(Sprite3D,get_frame);

    SE_BIND_METHOD(Sprite3D,set_frame_coords);
    SE_BIND_METHOD(Sprite3D,get_frame_coords);

    SE_BIND_METHOD(Sprite3D,set_vframes);
    SE_BIND_METHOD(Sprite3D,get_vframes);

    SE_BIND_METHOD(Sprite3D,set_hframes);
    SE_BIND_METHOD(Sprite3D,get_hframes);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "texture", PropertyHint::ResourceType, "Texture"), "set_texture", "get_texture");
    ADD_GROUP("Animation", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "hframes", PropertyHint::Range, "1,16384,1"), "set_hframes", "get_hframes");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "vframes", PropertyHint::Range, "1,16384,1"), "set_vframes", "get_vframes");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frame"), "set_frame", "get_frame");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "frame_coords", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "set_frame_coords", "get_frame_coords");
    ADD_GROUP("Region", "region_");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "region_enabled"), "set_region", "is_region");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region_rect"), "set_region_rect", "get_region_rect");

    ADD_SIGNAL(MethodInfo("frame_changed"));
}

Sprite3D::Sprite3D() {

    region = false;
    frame = 0;
    vframes = 1;
    hframes = 1;
}

////////////////////////////////////////

void AnimatedSprite3D::_draw() {

    if (get_base() != get_mesh()) {
        set_base(get_mesh());
    }

    if (not frames || frame < 0 || !frames->has_animation(animation)) {
        return;
    }

    Ref<Texture> texture = frames->get_frame(animation, frame);
    if (not texture) {
        set_base(entt::null);
        return; //no texuture no life
    }
    Vector2 tsize = texture->get_size();
    if (tsize.x == 0 || tsize.y == 0)
        return;

    Rect2 src_rect;
    src_rect.size = tsize;

    Point2 ofs = get_offset();
    if (is_centered()) {
        ofs -= tsize / 2;
    }

    Rect2 dst_rect(ofs, tsize);

    draw_texture_rect(texture, dst_rect, src_rect);
}

void AnimatedSprite3D::_validate_property(PropertyInfo &property) const {

    if (not frames)
        return;
    if (property.name == "animation") {

        property.hint = PropertyHint::Enum;
        List<StringName> names;
        frames->get_animation_list(&names);
        names.sort(WrapAlphaCompare());

        bool current_found = false;

        for (List<StringName>::iterator E = names.begin(); E!=names.end(); ++E) {
            if (E!=names.begin()) {
                property.hint_string += ',';
            }

            property.hint_string += (*E);
            if (animation == *E) {
                current_found = true;
            }
        }

        if (!current_found) {
            if (property.hint_string.empty()) {
                property.hint_string = (animation);
            } else {
                property.hint_string = String(animation) + "," + property.hint_string;
            }
        }
    }

    if (property.name == "frame") {
        property.hint = PropertyHint::Range;
        if (frames->has_animation(animation) && frames->get_frame_count(animation) > 1) {
            property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
        }
        property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
    }
}

void AnimatedSprite3D::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_INTERNAL_PROCESS: {

            if (not frames || frame < 0 || !frames->has_animation(animation))
                return;

            float remaining = get_process_delta_time();

            while (remaining) {
                float speed = frames->get_animation_speed(animation);
                if (speed == 0.0f) {
                    return; // Do nothing.
                }

                if (timeout <= 0) {

                    timeout = 1.0f / speed;

                    int fc = frames->get_frame_count(animation);
                    if (frame >= fc - 1) {
                        if (frames->get_animation_loop(animation)) {
                            frame = 0;
                        } else {
                            frame = fc - 1;
                        }
                        emit_signal(SceneStringNames::animation_finished);
                    } else {
                        frame++;
                    }

                    _queue_update();
                    Object_change_notify(this,"frame");
                    emit_signal(SceneStringNames::frame_changed);
                }

                float to_process = MIN(timeout, remaining);
                remaining -= to_process;
                timeout -= to_process;
            }
        } break;
    }
}

void AnimatedSprite3D::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {

    if (frames)
        frames->disconnect("changed",callable_mp(this, &ClassName::_res_changed));
    frames = p_frames;
    if (frames)
        frames->connect("changed",callable_mp(this, &ClassName::_res_changed));

    if (not frames) {
        frame = 0;
    } else {
        set_frame(frame);
    }

    Object_change_notify(this);
    _reset_timeout();
    _queue_update();
    update_configuration_warning();
}

Ref<SpriteFrames> AnimatedSprite3D::get_sprite_frames() const {

    return frames;
}

void AnimatedSprite3D::set_frame(int p_frame) {

    if (not frames) {
        return;
    }

    if (frames->has_animation(animation)) {
        int limit = frames->get_frame_count(animation);
        if (p_frame >= limit)
            p_frame = limit - 1;
    }

    if (p_frame < 0)
        p_frame = 0;

    if (frame == p_frame)
        return;

    frame = p_frame;
    _reset_timeout();
    _queue_update();
    Object_change_notify(this,"frame");
    emit_signal(SceneStringNames::frame_changed);
}
int AnimatedSprite3D::get_frame() const {

    return frame;
}

Rect2 AnimatedSprite3D::get_item_rect() const {

    if (not frames || !frames->has_animation(animation) || frame < 0 || frame >= frames->get_frame_count(animation)) {
        return Rect2(0, 0, 1, 1);
    }

    Ref<Texture> t;
    if (animation)
        t = frames->get_frame(animation, frame);
    if (not t)
        return Rect2(0, 0, 1, 1);
    Size2 s = t->get_size();

    Point2 ofs = get_offset();
    if (centered)
        ofs -= s / 2;

    if (s == Size2(0, 0))
        s = Size2(1, 1);

    return Rect2(ofs, s);
}

void AnimatedSprite3D::_res_changed() {

    set_frame(frame);
    Object_change_notify(this,"frame");
    Object_change_notify(this,"animation");
    _queue_update();
}

void AnimatedSprite3D::_set_playing(bool p_playing) {

    if (playing == p_playing)
        return;
    playing = p_playing;
    _reset_timeout();
    set_process_internal(playing);
}

bool AnimatedSprite3D::_is_playing() const {

    return playing;
}

void AnimatedSprite3D::play(const StringName &p_animation) {

    if (p_animation)
        set_animation(p_animation);
    _set_playing(true);
}

void AnimatedSprite3D::stop() {

    _set_playing(false);
}

bool AnimatedSprite3D::is_playing() const {

    return playing;
}

void AnimatedSprite3D::_reset_timeout() {

    if (!playing)
        return;

    if (frames && frames->has_animation(animation)) {
        float speed = frames->get_animation_speed(animation);
        if (speed > 0) {
            timeout = 1.0 / speed;
        } else {
            timeout = 0;
        }
    } else {
        timeout = 0;
    }
}

void AnimatedSprite3D::set_animation(const StringName &p_animation) {

    if (animation == p_animation)
        return;

    animation = p_animation;
    _reset_timeout();
    set_frame(0);
    Object_change_notify(this);
    _queue_update();
}
StringName AnimatedSprite3D::get_animation() const {

    return animation;
}

String AnimatedSprite3D::get_configuration_warning() const {

    String warning = BaseClassName::get_configuration_warning();
    if (!frames) {
        if (warning != String()) {
            warning += "\n\n";
        }
        warning += TTR("A SpriteFrames resource must be created or set in the \"Frames\" property in order for AnimatedSprite3D to display frames.");
    }

    return warning;

}

void AnimatedSprite3D::_bind_methods() {

    SE_BIND_METHOD(AnimatedSprite3D,set_sprite_frames);
    SE_BIND_METHOD(AnimatedSprite3D,get_sprite_frames);

    SE_BIND_METHOD(AnimatedSprite3D,set_animation);
    SE_BIND_METHOD(AnimatedSprite3D,get_animation);

    SE_BIND_METHOD(AnimatedSprite3D,_set_playing);
    SE_BIND_METHOD(AnimatedSprite3D,_is_playing);

    MethodBinder::bind_method(D_METHOD("play", {"anim"}), &AnimatedSprite3D::play, {DEFVAL(StringName())});
    SE_BIND_METHOD(AnimatedSprite3D,stop);
    SE_BIND_METHOD(AnimatedSprite3D,is_playing);

    SE_BIND_METHOD(AnimatedSprite3D,set_frame);
    SE_BIND_METHOD(AnimatedSprite3D,get_frame);

    SE_BIND_METHOD(AnimatedSprite3D,_res_changed);

    ADD_SIGNAL(MethodInfo("frame_changed"));
    ADD_SIGNAL(MethodInfo("animation_finished"));

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "frames", PropertyHint::ResourceType, "SpriteFrames"), "set_sprite_frames", "get_sprite_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "animation"), "set_animation", "get_animation");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frame"), "set_frame", "get_frame");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "playing"), "_set_playing", "_is_playing");
}

AnimatedSprite3D::AnimatedSprite3D() {

    frame = 0;
    playing = false;
    animation = "default";
    timeout = 0;
}
