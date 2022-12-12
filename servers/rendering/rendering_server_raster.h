/*************************************************************************/
/*  rendering_server_raster.h                                               */
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

#pragma once

#include "servers/rendering/rasterizer.h"
#include "servers/rendering_server.h"
#include "rendering_server_canvas.h"
#include "rendering_server_globals.h"
#include "rendering_server_scene.h"
#include "rendering_server_viewport.h"

struct RenderingEntityName {
    char name[64];
};

class  RenderingServerRaster : public RenderingServer {

    enum {

        MAX_INSTANCE_CULL = 8192,
        MAX_INSTANCE_LIGHTS = 4,
        LIGHT_CACHE_DIRTY = -1,
        MAX_LIGHTS_CULLED = 256,
        MAX_ROOM_CULL = 32,
        MAX_EXTERIOR_PORTALS = 128,
        MAX_LIGHT_SAMPLERS = 256,
        INSTANCE_ROOMLESS_MASK = (1 << 20)

    };

    // low and high priority
    static int changes[2];

    int black_margin[4];
    RenderingEntity black_image[4];

    Vector<FrameDrawnCallback> frame_drawn_callbacks;

    void _draw_margins();
    // This function is NOT dead code.
    // It is specifically for debugging redraws to help identify problems with
    // undesired constant editor updating.
    // The function will be called in DEV builds (and thus does not require a recompile),
    // allowing you to place a breakpoint either at the first line or the semicolon.
    // You can then look at the callstack to find the cause of the redraw.
    static void _changes_changed(int p_priority) {
        if (p_priority) {
            ;
        }
    }

public:
    // if editor is redrawing when it shouldn't, use a DEV build and put a breakpoint in _changes_changed()
    _FORCE_INLINE_ static void redraw_request(bool p_high_priority = true) {
        int priority = p_high_priority ? 1 : 0;
        changes[priority] += 1;
#ifdef DEV_ENABLED
        _changes_changed(priority);
#endif
    }

#ifdef DEV_ENABLED
#define DISPLAY_CHANGED \
    changes[1] += 1;                                                                                                   \
    _changes_changed(1);
#else
#define DISPLAY_CHANGED changes[1] += 1;
#endif

#define BIND0R(m_r, m_name) \
    m_r m_name() override { return BINDBASE->m_name(); }
#define BIND1R(m_r, m_name, m_type1)  \
    m_r m_name(m_type1 arg1) override { return BINDBASE->m_name(arg1); }
#define BIND1RC(m_r, m_name, m_type1)  \
    m_r m_name(m_type1 arg1) const override { return BINDBASE->m_name(arg1); }
#define BIND2R(m_r, m_name, m_type1, m_type2) \
    m_r m_name(m_type1 arg1, m_type2 arg2) override { return BINDBASE->m_name(arg1, arg2); }
#define BIND2RC(m_r, m_name, m_type1, m_type2) \
    m_r m_name(m_type1 arg1, m_type2 arg2) const override { return BINDBASE->m_name(arg1, arg2); }
#define BIND3RC(m_r, m_name, m_type1, m_type2, m_type3) \
    m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) const override { return BINDBASE->m_name(arg1, arg2, arg3); }
#define BIND4RC(m_r, m_name, m_type1, m_type2, m_type3, m_type4) \
    m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) const override { return BINDBASE->m_name(arg1, arg2, arg3, arg4); }

#define BIND1(m_name, m_type1) \
    void m_name(m_type1 arg1) override { DISPLAY_CHANGED BINDBASE->m_name(arg1); }
#define BIND2(m_name, m_type1, m_type2) \
    void m_name(m_type1 arg1, m_type2 arg2) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2); }
#define BIND2C(m_name, m_type1, m_type2) \
    void m_name(m_type1 arg1, m_type2 arg2) const override { BINDBASE->m_name(arg1, arg2); }
#define BIND3(m_name, m_type1, m_type2, m_type3) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3); }
#define BIND4(m_name, m_type1, m_type2, m_type3, m_type4) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4); }
#define BIND5(m_name, m_type1, m_type2, m_type3, m_type4, m_type5) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5); }
#define BIND6(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6); }
#define BIND7(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7); }
#define BIND8(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); }
#define BIND9(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); }
#define BIND10(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); }
#define BIND11(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); }
#define BIND12(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12); }
#define BIND13(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12, m_type13) \
    void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12, m_type13 arg13) override { DISPLAY_CHANGED BINDBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13); }

//from now on, calls forwarded to this singleton
#define BINDBASE VSG::storage

    void set_ent_debug_name(RenderingEntity p1, StringView p2) const override;
    /* TEXTURE API */

    RenderingEntity texture_create() override { return VSG::storage->texture_create(); }
    void texture_allocate(RenderingEntity arg1, int arg2, int arg3, int arg4, Image::Format arg5, RS::TextureType arg6, uint32_t arg7) override { DISPLAY_CHANGED VSG::storage->texture_allocate(arg1, arg2, arg3, arg4, arg5, arg6, arg7); }
    void texture_set_data(RenderingEntity arg1, const Ref<Image> & arg2, int arg3) override { DISPLAY_CHANGED VSG::storage->texture_set_data(arg1, arg2, arg3); }
    void texture_set_data_partial(RenderingEntity arg1, const Ref<Image> & arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10) override { DISPLAY_CHANGED VSG::storage->texture_set_data_partial(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); }
    Ref<Image> texture_get_data(RenderingEntity arg1, int arg2) const override { return VSG::storage->texture_get_data(arg1, arg2); }
    void texture_set_flags(RenderingEntity arg1, uint32_t arg2) override { DISPLAY_CHANGED VSG::storage->texture_set_flags(arg1, arg2); }
    uint32_t texture_get_flags(RenderingEntity arg1) const override { return VSG::storage->texture_get_flags(arg1); }
    Image::Format texture_get_format(RenderingEntity arg1) const override { return VSG::storage->texture_get_format(arg1); }
    RS::TextureType texture_get_type(RenderingEntity arg1) const override { return VSG::storage->texture_get_type(arg1); }
    uint32_t texture_get_texid(RenderingEntity arg1) const override { return VSG::storage->texture_get_texid(arg1); }
    uint32_t texture_get_width(RenderingEntity arg1) const override { return VSG::storage->texture_get_width(arg1); }
    uint32_t texture_get_height(RenderingEntity arg1) const override { return VSG::storage->texture_get_height(arg1); }
    uint32_t texture_get_depth(RenderingEntity arg1) const override { return VSG::storage->texture_get_depth(arg1); }
    void texture_set_size_override(RenderingEntity arg1, int arg2, int arg3, int arg4) override { DISPLAY_CHANGED VSG::storage->texture_set_size_override(arg1, arg2, arg3, arg4); }
    void texture_bind(RenderingEntity arg1, uint32_t arg2) override { DISPLAY_CHANGED VSG::storage->texture_bind(arg1, arg2); }

    void texture_set_detect_3d_callback(RenderingEntity arg1, TextureDetectCallback arg2, void * arg3) override { DISPLAY_CHANGED VSG::storage->texture_set_detect_3d_callback(arg1, arg2, arg3); }
    void texture_set_detect_srgb_callback(RenderingEntity arg1, TextureDetectCallback arg2, void * arg3) override { DISPLAY_CHANGED VSG::storage->texture_set_detect_srgb_callback(arg1, arg2, arg3); }
    void texture_set_detect_normal_callback(RenderingEntity arg1, TextureDetectCallback arg2, void * arg3) override { DISPLAY_CHANGED VSG::storage->texture_set_detect_normal_callback(arg1, arg2, arg3); }

    void texture_set_path(RenderingEntity arg1, StringView arg2) override { DISPLAY_CHANGED VSG::storage->texture_set_path(arg1, arg2); }
    const String & texture_get_path(RenderingEntity arg1) const override { return VSG::storage->texture_get_path(arg1); }
    void texture_set_shrink_all_x2_on_set_data(bool arg1) override { DISPLAY_CHANGED VSG::storage->texture_set_shrink_all_x2_on_set_data(arg1); }
    void texture_debug_usage(Vector<TextureInfo> * arg1) override { DISPLAY_CHANGED VSG::storage->texture_debug_usage(arg1); }

    void textures_keep_original(bool arg1) override { DISPLAY_CHANGED VSG::storage->textures_keep_original(arg1); }

    void texture_set_proxy(RenderingEntity arg1, RenderingEntity arg2) override { DISPLAY_CHANGED VSG::storage->texture_set_proxy(arg1, arg2); }

    void texture_set_force_redraw_if_visible(RenderingEntity arg1, bool arg2) override { DISPLAY_CHANGED VSG::storage->texture_set_force_redraw_if_visible(arg1, arg2); }

    /* SKY API */

    RenderingEntity sky_create() override { return VSG::storage->sky_create(); }
    void sky_set_texture(RenderingEntity arg1, RenderingEntity arg2, int arg3) override { DISPLAY_CHANGED VSG::storage->sky_set_texture(arg1, arg2, arg3); }

    /* SHADER API */

    RenderingEntity shader_create() override { return VSG::storage->shader_create(); }

    void shader_set_code(RenderingEntity arg1, const String & arg2) override { DISPLAY_CHANGED VSG::storage->shader_set_code(arg1, arg2); }
    String shader_get_code(RenderingEntity arg1) const override { return VSG::storage->shader_get_code(arg1); }

    void shader_get_param_list(RenderingEntity arg1, Vector<PropertyInfo> * arg2) const override { VSG::storage->shader_get_param_list(arg1, arg2); }

    void shader_set_default_texture_param(RenderingEntity arg1, const StringName & arg2, RenderingEntity arg3) override { DISPLAY_CHANGED VSG::storage->shader_set_default_texture_param(arg1, arg2, arg3); }
    RenderingEntity shader_get_default_texture_param(RenderingEntity arg1, const StringName & arg2) const override { return VSG::storage->shader_get_default_texture_param(arg1, arg2); }

    void shader_add_custom_define(RenderingEntity arg1, StringView arg2) override { VSG::storage->shader_add_custom_define(arg1, arg2); }
    void shader_get_custom_defines(RenderingEntity arg1, Vector<StringView> * arg2) const override { VSG::storage->shader_get_custom_defines(arg1, arg2); }
    void shader_remove_custom_define(RenderingEntity arg1, StringView p_define) override { VSG::storage->shader_remove_custom_define(arg1,p_define); }
    void set_shader_async_hidden_forbidden(bool p_forbid) override { VSG::storage->set_shader_async_hidden_forbidden(p_forbid); }

    /* COMMON MATERIAL API */

    RenderingEntity material_create() override { return VSG::storage->material_create(); }

    void material_set_shader(RenderingEntity arg1, RenderingEntity arg2) override { DISPLAY_CHANGED VSG::storage->material_set_shader(arg1, arg2); }
    RenderingEntity material_get_shader(RenderingEntity arg1) const override { return VSG::storage->material_get_shader(arg1); }

    void material_set_param(RenderingEntity arg1, const StringName & arg2, const Variant & arg3) override { DISPLAY_CHANGED VSG::storage->material_set_param(arg1, arg2, arg3); }
    Variant material_get_param(RenderingEntity arg1, const StringName & arg2) const override { return VSG::storage->material_get_param(arg1, arg2); }
    Variant material_get_param_default(RenderingEntity arg1, const StringName & arg2) const override { return VSG::storage->material_get_param_default(arg1, arg2); }

    void material_set_render_priority(RenderingEntity arg1, int arg2) override { DISPLAY_CHANGED VSG::storage->material_set_render_priority(arg1, arg2); }
    void material_set_line_width(RenderingEntity arg1, float arg2) override { DISPLAY_CHANGED VSG::storage->material_set_line_width(arg1, arg2); }
    void material_set_next_pass(RenderingEntity arg1, RenderingEntity arg2) override { DISPLAY_CHANGED VSG::storage->material_set_next_pass(arg1, arg2); }

    /* MESH API */

    RenderingEntity mesh_create() override { return VSG::storage->mesh_create(); }

    //BIND10(mesh_add_surface, RenderingEntity, uint32_t, RS::PrimitiveType, const PoolVector<uint8_t> &, int, const PoolVector<uint8_t> &, int, const AABB &, const Vector<PoolVector<uint8_t> > &, const PoolVector<AABB> &)
    void mesh_add_surface(RenderingEntity arg1, uint32_t arg2, RS::PrimitiveType arg3, const PoolVector<uint8_t> & arg4, int arg5, const PoolVector<uint8_t> & arg6, int arg7, const AABB & arg8, const Vector<PoolVector<uint8_t> > & arg9, const PoolVector<AABB> & arg10) override {
        DISPLAY_CHANGED
        VSG::storage->mesh_add_surface(arg1, arg2, arg3, arg4.toSpan(), arg5, arg6.toSpan(), arg7, arg8, arg9, arg10.toSpan());
    }
    void mesh_set_blend_shape_count(RenderingEntity arg1, int arg2) override { DISPLAY_CHANGED VSG::storage->mesh_set_blend_shape_count(arg1, arg2); }
    int mesh_get_blend_shape_count(RenderingEntity arg1) const override { return VSG::storage->mesh_get_blend_shape_count(arg1); }

    void mesh_set_blend_shape_mode(RenderingEntity arg1, RS::BlendShapeMode arg2) override { DISPLAY_CHANGED VSG::storage->mesh_set_blend_shape_mode(arg1, arg2); }
    RS::BlendShapeMode mesh_get_blend_shape_mode(RenderingEntity arg1) const override { return VSG::storage->mesh_get_blend_shape_mode(arg1); }

    void mesh_surface_update_region(RenderingEntity arg1, int arg2, int arg3, const PoolVector<uint8_t> &arg4) override {
        DISPLAY_CHANGED
        VSG::storage->mesh_surface_update_region(arg1, arg2, arg3, arg4.toSpan());
    }


    BIND3(mesh_surface_set_material, RenderingEntity, int, RenderingEntity)
    BIND2RC(RenderingEntity, mesh_surface_get_material, RenderingEntity, int)

    BIND2RC(int, mesh_surface_get_array_len, RenderingEntity, int)
    BIND2RC(int, mesh_surface_get_array_index_len, RenderingEntity, int)

    BIND2RC(PoolVector<uint8_t>, mesh_surface_get_array, RenderingEntity, int)
    BIND2RC(PoolVector<uint8_t>, mesh_surface_get_index_array, RenderingEntity, int)

    BIND2RC(uint32_t, mesh_surface_get_format, RenderingEntity, int)
    BIND2RC(RS::PrimitiveType, mesh_surface_get_primitive_type, RenderingEntity, int)

    BIND2RC(AABB, mesh_surface_get_aabb, RenderingEntity, int)
    BIND2RC(Vector<Vector<uint8_t> >, mesh_surface_get_blend_shapes, RenderingEntity, int)
    BIND2RC(const Vector<AABB> &, mesh_surface_get_skeleton_aabb, RenderingEntity, int)

    void mesh_remove_surface(RenderingEntity arg1, int arg2) override { DISPLAY_CHANGED BINDBASE->mesh_remove_surface(arg1, arg2); }
    int mesh_get_surface_count(RenderingEntity arg1) const override { return BINDBASE->mesh_get_surface_count(arg1); }

    void mesh_set_custom_aabb(RenderingEntity arg1, const AABB & arg2) override { DISPLAY_CHANGED BINDBASE->mesh_set_custom_aabb(arg1, arg2); }
    AABB mesh_get_custom_aabb(RenderingEntity arg1) const override { return BINDBASE->mesh_get_custom_aabb(arg1); }

    void mesh_clear(RenderingEntity arg1) override { DISPLAY_CHANGED BINDBASE->mesh_clear(arg1); }

    /* MULTIMESH API */

    BIND0R(RenderingEntity, multimesh_create)

    BIND5(multimesh_allocate, RenderingEntity, int, RS::MultimeshTransformFormat, RS::MultimeshColorFormat, RS::MultimeshCustomDataFormat)
    BIND1RC(int, multimesh_get_instance_count, RenderingEntity)

    BIND2(multimesh_set_mesh, RenderingEntity, RenderingEntity)
    BIND3(multimesh_instance_set_transform, RenderingEntity, int, const Transform &)
    BIND3(multimesh_instance_set_transform_2d, RenderingEntity, int, const Transform2D &)
    BIND3(multimesh_instance_set_color, RenderingEntity, int, const Color &)
    BIND3(multimesh_instance_set_custom_data, RenderingEntity, int, const Color &)

    BIND1RC(RenderingEntity, multimesh_get_mesh, RenderingEntity)
    BIND1RC(AABB, multimesh_get_aabb, RenderingEntity)

    BIND2RC(Transform, multimesh_instance_get_transform, RenderingEntity, int)
    BIND2RC(Transform2D, multimesh_instance_get_transform_2d, RenderingEntity, int)
    BIND2RC(Color, multimesh_instance_get_color, RenderingEntity, int)
    BIND2RC(Color, multimesh_instance_get_custom_data, RenderingEntity, int)

    BIND2(multimesh_set_as_bulk_array, RenderingEntity, Span<const float>)

    BIND2(multimesh_set_visible_instances, RenderingEntity, int)
    BIND1RC(int, multimesh_get_visible_instances, RenderingEntity)

    /* IMMEDIATE API */

    BIND0R(RenderingEntity, immediate_create)
    BIND3(immediate_begin, RenderingEntity, RS::PrimitiveType, RenderingEntity)
    BIND2(immediate_vertex, RenderingEntity, const Vector3 &)
    BIND2(immediate_normal, RenderingEntity, const Vector3 &)
    BIND2(immediate_tangent, RenderingEntity, const Plane &)
    BIND2(immediate_color, RenderingEntity, const Color &)
    BIND2(immediate_uv, RenderingEntity, const Vector2 &)
    BIND2(immediate_uv2, RenderingEntity, const Vector2 &)
    BIND1(immediate_end, RenderingEntity)
    BIND1(immediate_clear, RenderingEntity)
    BIND2(immediate_set_material, RenderingEntity, RenderingEntity)
    BIND1RC(RenderingEntity, immediate_get_material, RenderingEntity)

    /* SKELETON API */

    BIND0R(RenderingEntity, skeleton_create)
    BIND3(skeleton_allocate, RenderingEntity, int, bool)
    BIND1RC(int, skeleton_get_bone_count, RenderingEntity)
    BIND3(skeleton_bone_set_transform, RenderingEntity, int, const Transform &)
    BIND2RC(Transform, skeleton_bone_get_transform, RenderingEntity, int)
    BIND3(skeleton_bone_set_transform_2d, RenderingEntity, int, const Transform2D &)
    BIND2RC(Transform2D, skeleton_bone_get_transform_2d, RenderingEntity, int)
    BIND2(skeleton_set_base_transform_2d, RenderingEntity, const Transform2D &)

    /* Light API */

    BIND0R(RenderingEntity, directional_light_create)
    BIND0R(RenderingEntity, omni_light_create)
    BIND0R(RenderingEntity, spot_light_create)

    BIND2(light_set_color, RenderingEntity, const Color &)
    BIND3(light_set_param, RenderingEntity, RS::LightParam, float)
    BIND2(light_set_shadow, RenderingEntity, bool)
    BIND2(light_set_shadow_color, RenderingEntity, const Color &)
    BIND2(light_set_projector, RenderingEntity, RenderingEntity)
    BIND2(light_set_negative, RenderingEntity, bool)
    BIND2(light_set_cull_mask, RenderingEntity, uint32_t)
    BIND2(light_set_reverse_cull_face_mode, RenderingEntity, bool)
    BIND2(light_set_use_gi, RenderingEntity, bool)
    BIND2(light_set_bake_mode, RenderingEntity, RS::LightBakeMode)

    BIND2(light_omni_set_shadow_mode, RenderingEntity, RS::LightOmniShadowMode)
    BIND2(light_omni_set_shadow_detail, RenderingEntity, RS::LightOmniShadowDetail)

    BIND2(light_directional_set_shadow_mode, RenderingEntity, RS::LightDirectionalShadowMode)
    BIND2(light_directional_set_blend_splits, RenderingEntity, bool)
    BIND2(light_directional_set_shadow_depth_range_mode, RenderingEntity, RS::LightDirectionalShadowDepthRangeMode)

    /* PROBE API */

    BIND0R(RenderingEntity, reflection_probe_create)

    BIND2(reflection_probe_set_update_mode, RenderingEntity, RS::ReflectionProbeUpdateMode)
    BIND2(reflection_probe_set_intensity, RenderingEntity, float)
    BIND2(reflection_probe_set_interior_ambient, RenderingEntity, const Color &)
    BIND2(reflection_probe_set_interior_ambient_energy, RenderingEntity, float)
    BIND2(reflection_probe_set_interior_ambient_probe_contribution, RenderingEntity, float)
    BIND2(reflection_probe_set_max_distance, RenderingEntity, float)
    BIND2(reflection_probe_set_extents, RenderingEntity, const Vector3 &)
    BIND2(reflection_probe_set_origin_offset, RenderingEntity, const Vector3 &)
    BIND2(reflection_probe_set_as_interior, RenderingEntity, bool)
    BIND2(reflection_probe_set_enable_box_projection, RenderingEntity, bool)
    BIND2(reflection_probe_set_enable_shadows, RenderingEntity, bool)
    BIND2(reflection_probe_set_cull_mask, RenderingEntity, uint32_t)
    BIND2(reflection_probe_set_resolution, RenderingEntity, int)

    /* BAKED LIGHT API */

    BIND0R(RenderingEntity, gi_probe_create)

    BIND2(gi_probe_set_bounds, RenderingEntity, const AABB &)
    BIND1RC(AABB, gi_probe_get_bounds, RenderingEntity)

    BIND2(gi_probe_set_cell_size, RenderingEntity, float)
    BIND1RC(float, gi_probe_get_cell_size, RenderingEntity)

    BIND2(gi_probe_set_to_cell_xform, RenderingEntity, const Transform &)
    BIND1RC(Transform, gi_probe_get_to_cell_xform, RenderingEntity)

    BIND2(gi_probe_set_dynamic_range, RenderingEntity, int)
    BIND1RC(int, gi_probe_get_dynamic_range, RenderingEntity)

    BIND2(gi_probe_set_energy, RenderingEntity, float)
    BIND1RC(float, gi_probe_get_energy, RenderingEntity)

    BIND2(gi_probe_set_bias, RenderingEntity, float)
    BIND1RC(float, gi_probe_get_bias, RenderingEntity)

    BIND2(gi_probe_set_normal_bias, RenderingEntity, float)
    BIND1RC(float, gi_probe_get_normal_bias, RenderingEntity)

    BIND2(gi_probe_set_propagation, RenderingEntity, float)
    BIND1RC(float, gi_probe_get_propagation, RenderingEntity)

    BIND2(gi_probe_set_interior, RenderingEntity, bool)
    BIND1RC(bool, gi_probe_is_interior, RenderingEntity)

    BIND2(gi_probe_set_dynamic_data, RenderingEntity, const PoolVector<int> &)
    BIND1RC(PoolVector<int>, gi_probe_get_dynamic_data, RenderingEntity)

    /* LIGHTMAP CAPTURE */

    BIND0R(RenderingEntity, lightmap_capture_create)

    BIND2(lightmap_capture_set_bounds, RenderingEntity, const AABB &)
    BIND1RC(AABB, lightmap_capture_get_bounds, RenderingEntity)

    BIND2(lightmap_capture_set_octree, RenderingEntity, const PoolVector<uint8_t> &)
    BIND1RC(PoolVector<uint8_t>, lightmap_capture_get_octree, RenderingEntity)

    BIND2(lightmap_capture_set_octree_cell_transform, RenderingEntity, const Transform &)
    BIND1RC(Transform, lightmap_capture_get_octree_cell_transform, RenderingEntity)
    BIND2(lightmap_capture_set_octree_cell_subdiv, RenderingEntity, int)
    BIND1RC(int, lightmap_capture_get_octree_cell_subdiv, RenderingEntity)

    BIND2(lightmap_capture_set_energy, RenderingEntity, float)
    BIND1RC(float, lightmap_capture_get_energy, RenderingEntity)

    BIND2(lightmap_capture_set_interior, RenderingEntity, bool)
    BIND1RC(bool, lightmap_capture_is_interior, RenderingEntity)
    /* PARTICLES */

    BIND0R(RenderingEntity, particles_create)

    BIND2(particles_set_emitting, RenderingEntity, bool)
    BIND1R(bool, particles_get_emitting, RenderingEntity)
    BIND2(particles_set_amount, RenderingEntity, int)
    BIND2(particles_set_lifetime, RenderingEntity, float)
    BIND2(particles_set_one_shot, RenderingEntity, bool)
    BIND2(particles_set_pre_process_time, RenderingEntity, float)
    BIND2(particles_set_explosiveness_ratio, RenderingEntity, float)
    BIND2(particles_set_randomness_ratio, RenderingEntity, float)
    BIND2(particles_set_custom_aabb, RenderingEntity, const AABB &)
    BIND2(particles_set_speed_scale, RenderingEntity, float)
    BIND2(particles_set_use_local_coordinates, RenderingEntity, bool)
    BIND2(particles_set_process_material, RenderingEntity, RenderingEntity)
    BIND2(particles_set_fixed_fps, RenderingEntity, int)
    BIND2(particles_set_fractional_delta, RenderingEntity, bool)
    BIND1R(bool, particles_is_inactive, RenderingEntity)
    BIND1(particles_request_process, RenderingEntity)
    BIND1(particles_restart, RenderingEntity)

    BIND2(particles_set_draw_order, RenderingEntity, RS::ParticlesDrawOrder)

    BIND2(particles_set_draw_passes, RenderingEntity, int)
    BIND3(particles_set_draw_pass_mesh, RenderingEntity, int, RenderingEntity)

    BIND1R(AABB, particles_get_current_aabb, RenderingEntity)
    BIND2(particles_set_emission_transform, RenderingEntity, const Transform &)

#undef BINDBASE

//from now on, calls forwarded to this singleton
#define BINDBASE VSG::viewport

    /* VIEWPORT TARGET API */

    BIND0R(RenderingEntity, viewport_create)

    BIND2(viewport_set_use_arvr, RenderingEntity, bool)
    BIND3(viewport_set_size, RenderingEntity, int, int)

    BIND2(viewport_set_active, RenderingEntity, bool)
    BIND2(viewport_set_parent_viewport, RenderingEntity, RenderingEntity)

    BIND2(viewport_set_clear_mode, RenderingEntity, RS::ViewportClearMode)

    BIND3(viewport_attach_to_screen, RenderingEntity, const Rect2 &, int)
    BIND1(viewport_detach, RenderingEntity)

    BIND2(viewport_set_update_mode, RenderingEntity, RS::ViewportUpdateMode)
    BIND2(viewport_set_vflip, RenderingEntity, bool)

    BIND1RC(RenderingEntity, viewport_get_texture, RenderingEntity)

    BIND2(viewport_set_hide_scenario, RenderingEntity, bool)
    BIND2(viewport_set_hide_canvas, RenderingEntity, bool)
    BIND2(viewport_set_disable_environment, RenderingEntity, bool)
    BIND2(viewport_set_disable_3d, RenderingEntity, bool)
    BIND2(viewport_set_keep_3d_linear, RenderingEntity, bool)

    BIND2(viewport_attach_camera, RenderingEntity, RenderingEntity)
    BIND2(viewport_set_scenario, RenderingEntity, RenderingEntity)
    BIND2(viewport_attach_canvas, RenderingEntity, RenderingEntity)

    BIND2(viewport_remove_canvas, RenderingEntity, RenderingEntity)
    BIND3(viewport_set_canvas_transform, RenderingEntity, RenderingEntity, const Transform2D &)
    BIND2(viewport_set_transparent_background, RenderingEntity, bool)

    BIND2(viewport_set_global_canvas_transform, RenderingEntity, const Transform2D &)
    BIND4(viewport_set_canvas_stacking, RenderingEntity, RenderingEntity, int, int)
    BIND2(viewport_set_shadow_atlas_size, RenderingEntity, int)
    BIND3(viewport_set_shadow_atlas_quadrant_subdivision, RenderingEntity, int, int)
    BIND2(viewport_set_msaa, RenderingEntity, RS::ViewportMSAA)
    BIND2(viewport_set_use_fxaa, RenderingEntity, bool)
    BIND2(viewport_set_use_debanding, RenderingEntity, bool)
    BIND2(viewport_set_sharpen_intensity, RenderingEntity, float)
    BIND2(viewport_set_hdr, RenderingEntity, bool)
    BIND2(viewport_set_use_32_bpc_depth, RenderingEntity, bool)
    BIND2(viewport_set_usage, RenderingEntity, RS::ViewportUsage)

    BIND2R(uint64_t, viewport_get_render_info, RenderingEntity, RS::ViewportRenderInfo)
    BIND2(viewport_set_debug_draw, RenderingEntity, RS::ViewportDebugDraw)

    /* ENVIRONMENT API */

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::scene_render

    BIND0R(RenderingEntity, environment_create)

    BIND2(environment_set_background, RenderingEntity, RS::EnvironmentBG)
    BIND2(environment_set_sky, RenderingEntity, RenderingEntity)
    BIND2(environment_set_sky_custom_fov, RenderingEntity, float)
    BIND2(environment_set_sky_orientation, RenderingEntity, const Basis &)
    BIND2(environment_set_bg_color, RenderingEntity, const Color &)
    BIND2(environment_set_bg_energy, RenderingEntity, float)
    BIND2(environment_set_canvas_max_layer, RenderingEntity, int)
    BIND4(environment_set_ambient_light, RenderingEntity, const Color &, float, float)
    BIND2(environment_set_camera_feed_id, RenderingEntity, int)
    BIND7(environment_set_ssr, RenderingEntity, bool, int, float, float, float, bool)
    BIND13(environment_set_ssao, RenderingEntity, bool, float, float, float, float, float, float, float, const Color &, RS::EnvironmentSSAOQuality, RS::EnvironmentSSAOBlur, float)

    BIND6(environment_set_dof_blur_near, RenderingEntity, bool, float, float, float, RS::EnvironmentDOFBlurQuality)
    BIND6(environment_set_dof_blur_far, RenderingEntity, bool, float, float, float, RS::EnvironmentDOFBlurQuality)
    BIND12(environment_set_glow, RenderingEntity, bool, int, float, float, float, RS::EnvironmentGlowBlendMode, float, float, float, bool, bool)

    BIND9(environment_set_tonemap, RenderingEntity, RS::EnvironmentToneMapper, float, float, bool, float, float, float, float)

    BIND6(environment_set_adjustment, RenderingEntity, bool, float, float, float, RenderingEntity)

    BIND5(environment_set_fog, RenderingEntity, bool, const Color &, const Color &, float)
    BIND7(environment_set_fog_depth, RenderingEntity, bool, float, float, float, bool, float)
    BIND5(environment_set_fog_height, RenderingEntity, bool, float, float, float)


#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::scene
    /* EVENT QUEUING */
    void tick() override { BINDBASE->tick(); }
    void pre_draw(bool v) override { BINDBASE->pre_draw(v); }

    /* CAMERA API */

    BIND0R(RenderingEntity, camera_create)
    BIND4(camera_set_perspective, RenderingEntity, float, float, float)
    BIND4(camera_set_orthogonal, RenderingEntity, float, float, float)
    BIND5(camera_set_frustum, RenderingEntity, float, Vector2, float, float)
    BIND2(camera_set_transform, RenderingEntity, const Transform &)
    BIND2(camera_set_cull_mask, RenderingEntity, uint32_t)
    BIND2(camera_set_environment, RenderingEntity, RenderingEntity)
    BIND2(camera_set_use_vertical_aspect, RenderingEntity, bool)


    /* SCENARIO API */
    BIND0R(RenderingEntity, scenario_create)

    BIND2(scenario_set_debug, RenderingEntity, RS::ScenarioDebugMode)
    BIND2(scenario_set_environment, RenderingEntity, RenderingEntity)
    BIND3(scenario_set_reflection_atlas_size, RenderingEntity, int, int)
    BIND2(scenario_set_fallback_environment, RenderingEntity, RenderingEntity)

    /* INSTANCING API */

    BIND0R(RenderingEntity, instance_create)

    BIND2(instance_set_base, RenderingEntity, RenderingEntity)
    BIND2(instance_set_scenario, RenderingEntity, RenderingEntity)
    BIND2(instance_set_layer_mask, RenderingEntity, uint32_t)
    BIND2(instance_set_transform, RenderingEntity, const Transform &)
    BIND2(instance_attach_object_instance_id, RenderingEntity, GameEntity)
    BIND3(instance_set_blend_shape_weight, RenderingEntity, int, float)
    BIND3(instance_set_surface_material, RenderingEntity, int, RenderingEntity)
    BIND2(instance_set_visible, RenderingEntity, bool)
    BIND5(instance_set_use_lightmap, RenderingEntity, RenderingEntity, RenderingEntity, int , const Rect2 &)

    BIND2(instance_set_custom_aabb, RenderingEntity, AABB)

    BIND2(instance_attach_skeleton, RenderingEntity, RenderingEntity)

    BIND2(instance_set_extra_visibility_margin, RenderingEntity, real_t)
    /* PORTALS */

    BIND2(instance_set_portal_mode, RenderingEntity, RS::InstancePortalMode)
    /* OCCLUDERS */
    BIND0R(RenderingEntity, occluder_instance_create)
    BIND2(occluder_instance_set_scenario, RenderingEntity, RenderingEntity)
    BIND2(occluder_instance_link_resource, RenderingEntity, RenderingEntity)
    BIND2(occluder_instance_set_transform, RenderingEntity, const Transform &)
    BIND2(occluder_instance_set_active, RenderingEntity, bool)
    BIND0R(RenderingEntity, occluder_resource_create)
    BIND2(occluder_resource_prepare, RenderingEntity, RS::OccluderType)
    BIND2(occluder_resource_spheres_update, RenderingEntity, const Vector<Plane> &)
    BIND2(occluder_resource_mesh_update, RenderingEntity, const OccluderMeshData &)
    BIND1(set_use_occlusion_culling, bool)
    BIND1RC(GeometryMeshData, occlusion_debug_get_current_polys, RenderingEntity)

    // Callbacks
    BIND1(callbacks_register, RenderingServerCallbacks *)

    // don't use these in a game!
    BIND2RC(Vector<GameEntity>, instances_cull_aabb, const AABB &, RenderingEntity)
    BIND3RC(Vector<GameEntity>, instances_cull_ray, const Vector3 &, const Vector3 &, RenderingEntity)
    BIND2RC(Vector<GameEntity>, instances_cull_convex,  Span<const Plane>, RenderingEntity)

    BIND3(instance_geometry_set_flag, RenderingEntity, RenderingServerEnums::InstanceFlags, bool)
    BIND2(instance_geometry_set_cast_shadows_setting, RenderingEntity, RS::ShadowCastingSetting)
    BIND2(instance_geometry_set_material_override, RenderingEntity, RenderingEntity)
    BIND2(instance_geometry_set_material_overlay, RenderingEntity, RenderingEntity)

    BIND5(instance_geometry_set_draw_range, RenderingEntity, float, float, float, float)
    BIND2(instance_geometry_set_as_instance_lod, RenderingEntity, RenderingEntity)

#undef BINDBASE
//from now on, calls forwarded to this singleton
#define BINDBASE VSG::canvas

    /* CANVAS (2D) */

    BIND0R(RenderingEntity, canvas_create)
    BIND3(canvas_set_item_mirroring, RenderingEntity, RenderingEntity, const Point2 &)
    BIND2(canvas_set_modulate, RenderingEntity, const Color &)
    BIND3(canvas_set_parent, RenderingEntity, RenderingEntity, float)
    BIND1(canvas_set_disable_scale, bool)

    BIND0R(RenderingEntity, canvas_item_create)
    BIND2(canvas_item_set_parent, RenderingEntity, RenderingEntity)

    BIND2(canvas_item_set_visible, RenderingEntity, bool)
    BIND2(canvas_item_set_light_mask, RenderingEntity, int)

    BIND2(canvas_item_set_update_when_visible, RenderingEntity, bool)

    BIND2(canvas_item_set_transform, RenderingEntity, const Transform2D &)
    BIND2(canvas_item_set_clip, RenderingEntity, bool)
    BIND2(canvas_item_set_distance_field_mode, RenderingEntity, bool)
    BIND3(canvas_item_set_custom_rect, RenderingEntity, bool, const Rect2 &)
    BIND2(canvas_item_set_modulate, RenderingEntity, const Color &)
    BIND2(canvas_item_set_self_modulate, RenderingEntity, const Color &)

    BIND2(canvas_item_set_draw_behind_parent, RenderingEntity, bool)

    BIND6(canvas_item_add_line, RenderingEntity, const Point2 &, const Point2 &, const Color &, float, bool)
    BIND5(canvas_item_add_polyline, RenderingEntity, Span<const Vector2>, Span<const Color>, float, bool)
    BIND5(canvas_item_add_multiline, RenderingEntity, Span<const Vector2>, Span<const Color>, float, bool)
    BIND3(canvas_item_add_rect, RenderingEntity, const Rect2 &, const Color &)
    BIND4(canvas_item_add_circle, RenderingEntity, const Point2 &, float, const Color &)
    BIND7(canvas_item_add_texture_rect, RenderingEntity, const Rect2 &, RenderingEntity, bool, const Color &, bool, RenderingEntity)
    BIND8(canvas_item_add_texture_rect_region, RenderingEntity, const Rect2 &, RenderingEntity, const Rect2 &, const Color &, bool, RenderingEntity, bool)
    BIND11(canvas_item_add_nine_patch, RenderingEntity, const Rect2 &, const Rect2 &, RenderingEntity, const Vector2 &, const Vector2 &, RS::NinePatchAxisMode, RS::NinePatchAxisMode, bool, const Color &, RenderingEntity)
    BIND7(canvas_item_add_primitive, RenderingEntity, Span<const Vector2>, Span<const Color>, const PoolVector<Point2> &, RenderingEntity, float, RenderingEntity)
    BIND7(canvas_item_add_polygon, RenderingEntity, Span<const Point2>, Span<const Color>, Span<const Point2>, RenderingEntity, RenderingEntity, bool)
    BIND12(canvas_item_add_triangle_array, RenderingEntity, Span<const int>, Span<const Point2>, Span<const Color>, Span<const Point2>, const PoolVector<int> &, const PoolVector<float> &, RenderingEntity, int, RenderingEntity, bool,bool)
    BIND6(canvas_item_add_mesh, RenderingEntity, RenderingEntity, const Transform2D &, const Color &, RenderingEntity, RenderingEntity)
    BIND4(canvas_item_add_multimesh, RenderingEntity, RenderingEntity, RenderingEntity, RenderingEntity)
    BIND4(canvas_item_add_particles, RenderingEntity, RenderingEntity, RenderingEntity, RenderingEntity)
    BIND2(canvas_item_add_set_transform, RenderingEntity, const Transform2D &)
    BIND2(canvas_item_add_clip_ignore, RenderingEntity, bool)
    BIND2(canvas_item_set_sort_children_by_y, RenderingEntity, bool)
    BIND2(canvas_item_set_z_index, RenderingEntity, int)
    BIND2(canvas_item_set_z_as_relative_to_parent, RenderingEntity, bool)
    BIND3(canvas_item_set_copy_to_backbuffer, RenderingEntity, bool, const Rect2 &)
    BIND2(canvas_item_attach_skeleton, RenderingEntity, RenderingEntity)

    BIND1(canvas_item_clear, RenderingEntity)
    BIND2(canvas_item_set_draw_index, RenderingEntity, int)

    BIND2(canvas_item_set_material, RenderingEntity, RenderingEntity)

    BIND2(canvas_item_set_use_parent_material, RenderingEntity, bool)

    BIND0R(RenderingEntity, canvas_light_create)
    BIND2(canvas_light_attach_to_canvas, RenderingEntity, RenderingEntity)
    BIND2(canvas_light_set_enabled, RenderingEntity, bool)
    BIND2(canvas_light_set_scale, RenderingEntity, float)
    BIND2(canvas_light_set_transform, RenderingEntity, const Transform2D &)
    BIND2(canvas_light_set_texture, RenderingEntity, RenderingEntity)
    BIND2(canvas_light_set_texture_offset, RenderingEntity, const Vector2 &)
    BIND2(canvas_light_set_color, RenderingEntity, const Color &)
    BIND2(canvas_light_set_height, RenderingEntity, float)
    BIND2(canvas_light_set_energy, RenderingEntity, float)
    BIND3(canvas_light_set_z_range, RenderingEntity, int, int)
    BIND3(canvas_light_set_layer_range, RenderingEntity, int, int)
    BIND2(canvas_light_set_item_cull_mask, RenderingEntity, int)
    BIND2(canvas_light_set_item_shadow_cull_mask, RenderingEntity, int)

    BIND2(canvas_light_set_mode, RenderingEntity, RS::CanvasLightMode)

    BIND2(canvas_light_set_shadow_enabled, RenderingEntity, bool)
    BIND2(canvas_light_set_shadow_buffer_size, RenderingEntity, int)
    BIND2(canvas_light_set_shadow_gradient_length, RenderingEntity, float)
    BIND2(canvas_light_set_shadow_filter, RenderingEntity, RS::CanvasLightShadowFilter)
    BIND2(canvas_light_set_shadow_color, RenderingEntity, const Color &)
    BIND2(canvas_light_set_shadow_smooth, RenderingEntity, float)

    BIND0R(RenderingEntity, canvas_light_occluder_create)
    BIND2(canvas_light_occluder_attach_to_canvas, RenderingEntity, RenderingEntity)
    BIND2(canvas_light_occluder_set_enabled, RenderingEntity, bool)
    BIND2(canvas_light_occluder_set_polygon, RenderingEntity, RenderingEntity)
    BIND2(canvas_light_occluder_set_transform, RenderingEntity, const Transform2D &)
    BIND2(canvas_light_occluder_set_light_mask, RenderingEntity, int)

    BIND0R(RenderingEntity, canvas_occluder_polygon_create)
    BIND3(canvas_occluder_polygon_set_shape, RenderingEntity, Span<const Vector2>, bool)
    BIND2(canvas_occluder_polygon_set_shape_as_lines, RenderingEntity, Span<const Vector2>)

    BIND2(canvas_occluder_polygon_set_cull_mode, RenderingEntity, RS::CanvasOccluderPolygonCullMode)

    /* BLACK BARS */

    void black_bars_set_margins(int p_left, int p_top, int p_right, int p_bottom) override;
    void black_bars_set_images(RenderingEntity p_left, RenderingEntity p_top, RenderingEntity p_right, RenderingEntity p_bottom) override;

    /* FREE */

    void free_rid(RenderingEntity p_rid) override; ///< free RIDs associated with the visual server

    /* EVENT QUEUING */

    void request_frame_drawn_callback(FrameDrawnCallback &&cb) override;

    void draw(bool p_swap_buffers, double frame_step) override;
    bool has_changed(RS::ChangedPriority p_priority = RS::CHANGED_PRIORITY_ANY) const override;
    void init() override;
    void finish() override;

    /* STATUS INFORMATION */

    uint64_t get_render_info(RS::RenderInfo p_info) override;
    const char * get_video_adapter_name() const override;
    const char * get_video_adapter_vendor() const override;

    /* TESTING */

    void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) override;
    void set_default_clear_color(const Color &p_color) override;
    void set_shader_time_scale(float p_scale) override;

    bool has_feature(RS::Features p_feature) const override;

    bool has_os_feature(const StringName &p_feature) const override;
    void set_debug_generate_wireframes(bool p_generate) override;

    void call_set_use_vsync(bool p_enable) override;

    // bool is_low_end() const override;

    GODOT_EXPORT RenderingServerRaster();
    GODOT_EXPORT ~RenderingServerRaster() override;
    static RenderingServerRaster *get() { return (RenderingServerRaster*)submission_thread_singleton;}

#undef DISPLAY_CHANGED
#undef BIND0R
#undef BIND1RC
#undef BIND2RC
#undef BIND3RC
#undef BIND4RC

#undef BIND1
#undef BIND2
#undef BIND3
#undef BIND4
#undef BIND5
#undef BIND6
#undef BIND7
#undef BIND8
#undef BIND9
#undef BIND10
};
