#include "rasterizer_mesh_component.h"

#include "core/project_settings.h"
#include "rasterizer_common_geometry_component.h"
#include "rasterizer_multimesh_component.h"
#include "rasterizer_skeleton_component.h"
#include "rasterizer_surface_component.h"
#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_material_component.h"
#include "rasterizer_storage_gles3.h"

#include "servers/rendering/render_entity_getter.h"



void MeshComponent_clear(RasterizerMeshComponent *s) {
    for(auto surf : s->surfaces) {
        auto * surface=get<RasterizerSurfaceComponent>(surf);
        auto * geom=get<RasterizerCommonGeometryComponent>(surf);
        assert(surface && geom);

        if (geom->material!=entt::null) {
            material_remove_geometry(geom->material, surf);
            geom->material = entt::null;
        }

        get_rasterizer_storage_info().vertex_mem -= surface->total_data_size;
        VSG::ecs->registry.destroy(surf);
    }

    s->surfaces.clear();
}

void mesh_remove_surface(RasterizerMeshComponent *mesh, int p_surface) {
    ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());
    auto surf = mesh->surfaces[p_surface];
    auto *surface = get<RasterizerSurfaceComponent>(surf);
    auto *geom = get<RasterizerCommonGeometryComponent>(surf);

    if (geom->material != entt::null) {
        material_remove_geometry(geom->material, surf);
        geom->material = entt::null;
    }
    get_rasterizer_storage_info().vertex_mem -= surface->total_data_size;
    mesh->surfaces.erase_at(p_surface);
    VSG::ecs->registry.destroy(surf);

    auto *inst = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(mesh->self);
    // TODO: consider less-hacky solution for component deletion-order dependencies.
    // this could be called during entity destruction - RasterizerInstantiableComponent could be already deleted
    if (inst)
        inst->instance_change_notify(true, true);
}

void RasterizerMeshComponent::update_multimeshes() {

    for(RenderingEntity re: multimeshes) {
        VSG::ecs->registry.get<RasterizerInstantiableComponent>(re).instance_change_notify(false, true);
    }
}

RasterizerMeshComponent &RasterizerMeshComponent::operator=(RasterizerMeshComponent &&from) noexcept
{
    MeshComponent_clear(this);
    for(RenderingEntity mm : multimeshes) {
        multimesh_remove_base_mesh(mm);
    }
    multimeshes.clear();

    self= eastl::move(from.self);
    active=from.active;
    surfaces = eastl::move(from.surfaces);
    custom_aabb = eastl::move(from.custom_aabb);
    multimeshes = eastl::move(from.multimeshes);
    blend_shape_values = eastl::move(from.blend_shape_values);
    last_pass=from.last_pass;
    blend_shape_count= from.blend_shape_count;
    blend_shape_mode=from.blend_shape_mode;

    from.active = false;
    from.last_pass = 0;
    from.blend_shape_count = 0;
    return *this;
}

RasterizerMeshComponent::~RasterizerMeshComponent()
{
    while (!surfaces.empty()) {
        mesh_remove_surface(this, 0);
    }
    for(RenderingEntity re: multimeshes) {
        RasterizerMultiMeshComponent &multimesh = VSG::ecs->registry.get<RasterizerMultiMeshComponent>(re);
        multimesh.mesh = entt::null;
        RenderingEntity mm[1] = {re};
        mark_multimeshes_dirty(mm);
    }
    multimeshes.clear();

}

/* MESH API */

RenderingEntity RasterizerStorageGLES3::mesh_create() {

    RenderingEntity res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerMeshComponent>(res).self=res;
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);
    return res;
}

void RasterizerStorageGLES3::mesh_add_surface(RenderingEntity p_mesh, uint32_t p_format, RS::PrimitiveType p_primitive,
        Span<const uint8_t> p_array, int p_vertex_count, Span<const uint8_t> p_index_array, int p_index_count,
        const AABB &p_aabb, const Vector<PoolVector<uint8_t>> &p_blend_shapes, Span<const AABB> p_bone_aabbs) {

    Span<const uint8_t> array = p_array;
    Vector<uint8_t> converted_array;
    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_mesh);
    ERR_FAIL_COND(!mesh||!deps);


    ERR_FAIL_COND(!(p_format & RS::ARRAY_FORMAT_VERTEX));

         //must have index and bones, both.
    {
        uint32_t bones_weight = RS::ARRAY_FORMAT_BONES | RS::ARRAY_FORMAT_WEIGHTS;
        ERR_FAIL_COND_MSG((p_format & bones_weight) && (p_format & bones_weight) != bones_weight, "Array must have both bones and weights in format or none.");
    }

    //bool has_morph = p_blend_shapes.size();
    bool use_split_stream = T_GLOBAL_GET<bool>("rendering/misc/mesh_storage/split_stream") && !(p_format & RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE);

    RasterizerSurfaceComponent::Attrib attribs[RS::ARRAY_MAX];

    int attributes_base_offset = 0;
    int attributes_stride = 0;
    int positions_stride = 0;

    for (uint8_t i = 0; i < RS::ARRAY_MAX; i++) {

        attribs[i].index = i;

        if (!(p_format & (1 << i))) {
            attribs[i].enabled = false;
            attribs[i].integer = false;
            continue;
        }

        attribs[i].enabled = true;
        attribs[i].offset = attributes_base_offset + attributes_stride;
        attribs[i].integer = false;

        switch (i) {

            case RS::ARRAY_VERTEX: {

                if (p_format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                    attribs[i].size = 2;
                } else {
                    attribs[i].size = (p_format & RS::ARRAY_COMPRESS_VERTEX) ? 4 : 3;
                }

                if (p_format & RS::ARRAY_COMPRESS_VERTEX) {
                    attribs[i].type = GL_HALF_FLOAT;
                    positions_stride += attribs[i].size * 2;
                } else {
                    attribs[i].type = GL_FLOAT;
                    positions_stride += attribs[i].size * 4;
                }

                attribs[i].normalized = GL_FALSE;

                if (use_split_stream) {
                    attributes_base_offset = positions_stride * p_vertex_count;
                } else {
                    attributes_base_offset = positions_stride;
                }
            } break;
            case RS::ARRAY_NORMAL: {

            if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                // Always pack normal and tangent into vec4
                // normal will be xy tangent will be zw
                // normal will always be oct32 (4 byte) encoded
                // UNLESS tangent exists and is also compressed
                // then it will be oct16 encoded along with tangent
                attribs[i].normalized = GL_TRUE;
                attribs[i].size = 2;
                attribs[i].type = GL_SHORT;
                attributes_stride += 4;
                // Storing normal/tangent in the tangent attrib makes it easier to ubershaderify the scene shader
                attribs[i].index = RS::ARRAY_TANGENT;
            } else {
                attribs[i].size = 3;

                if (p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    attribs[i].type = GL_BYTE;
                    attributes_stride += 4; //pad extra byte
                    attribs[i].normalized = GL_TRUE;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 12;
                    attribs[i].normalized = GL_FALSE;
                }
                }

            } break;
            case RS::ARRAY_TANGENT: {

            if (p_format & RS::ARRAY_FLAG_USE_OCTAHEDRAL_COMPRESSION) {
                attribs[i].enabled = false;
                attribs[RS::ARRAY_NORMAL].size = 4;
                if (p_format & RS::ARRAY_COMPRESS_TANGENT && p_format & RS::ARRAY_COMPRESS_NORMAL) {
                    // normal and tangent will each be oct16 (2 bytes each)
                    // pack into single vec4<GL_BYTE> for memory bandwidth
                    // savings while keeping 4 byte alignment
                    attribs[RS::ARRAY_NORMAL].type = GL_BYTE;
                } else {
                    // normal and tangent will each be oct32 (4 bytes each)
                    attributes_stride += 4;
                }
            } else {
                attribs[i].size = 4;

                if (p_format & RS::ARRAY_COMPRESS_TANGENT) {
                    attribs[i].type = GL_BYTE;
                    attributes_stride += 4;
                    attribs[i].normalized = GL_TRUE;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 16;
                    attribs[i].normalized = GL_FALSE;
                }
                }

            } break;
            case RS::ARRAY_COLOR: {

                attribs[i].size = 4;

                if (p_format & RS::ARRAY_COMPRESS_COLOR) {
                    attribs[i].type = GL_UNSIGNED_BYTE;
                    attributes_stride += 4;
                    attribs[i].normalized = GL_TRUE;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 16;
                    attribs[i].normalized = GL_FALSE;
                }

            } break;
            case RS::ARRAY_TEX_UV: {

                attribs[i].size = 2;

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV) {
                    attribs[i].type = GL_HALF_FLOAT;
                    attributes_stride += 4;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 8;
                }

                attribs[i].normalized = GL_FALSE;

            } break;
            case RS::ARRAY_TEX_UV2: {

                attribs[i].size = 2;

                if (p_format & RS::ARRAY_COMPRESS_TEX_UV2) {
                    attribs[i].type = GL_HALF_FLOAT;
                    attributes_stride += 4;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 8;
                }
                attribs[i].normalized = GL_FALSE;

            } break;
            case RS::ARRAY_BONES: {

                attribs[i].size = 4;

                if (p_format & RS::ARRAY_FLAG_USE_16_BIT_BONES) {
                    attribs[i].type = GL_UNSIGNED_SHORT;
                    attributes_stride += 8;
                } else {
                    attribs[i].type = GL_UNSIGNED_BYTE;
                    attributes_stride += 4;
                }

                attribs[i].normalized = GL_FALSE;
                attribs[i].integer = true;

            } break;
            case RS::ARRAY_WEIGHTS: {

                attribs[i].size = 4;

                if (p_format & RS::ARRAY_COMPRESS_WEIGHTS) {

                    attribs[i].type = GL_UNSIGNED_SHORT;
                    attributes_stride += 8;
                    attribs[i].normalized = GL_TRUE;
                } else {
                    attribs[i].type = GL_FLOAT;
                    attributes_stride += 16;
                    attribs[i].normalized = GL_FALSE;
                }

            } break;
            case RS::ARRAY_INDEX: {

                attribs[i].size = 1;

                if (p_vertex_count >= (1 << 16)) {
                    attribs[i].type = GL_UNSIGNED_INT;
                    attribs[i].stride = 4;
                } else {
                    attribs[i].type = GL_UNSIGNED_SHORT;
                    attribs[i].stride = 2;
                }

                attribs[i].normalized = GL_FALSE;

            } break;
        }
    }

    if (use_split_stream) {
        attribs[RS::ARRAY_VERTEX].stride = positions_stride;
        for (int i = 1; i < RS::ARRAY_MAX - 1; i++) {
            attribs[i].stride = attributes_stride;
        }
    } else {
    for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {
            attribs[i].stride = positions_stride + attributes_stride;
        }
    }

         //validate sizes

    int stride = positions_stride + attributes_stride;
    int array_size = stride * p_vertex_count;
    int index_array_size = 0;
    if (array.size() != array_size && array.size() + p_vertex_count * 2 == array_size) {
        //old format, convert
        converted_array.resize(p_array.size() + p_vertex_count * 2);
        array = converted_array;

        uint8_t *w = converted_array.data();
        const uint8_t *r = p_array.data();

        uint16_t *w16 = (uint16_t *)w;
        const uint16_t *r16 = (uint16_t *)r;

        uint16_t one = Math::make_half_float(1);

        for (int i = 0; i < p_vertex_count; i++) {

            *w16++ = *r16++;
            *w16++ = *r16++;
            *w16++ = *r16++;
            *w16++ = one;
            for (int j = 0; j < (stride / 2) - 4; j++) {
                *w16++ = *r16++;
            }
        }
    }

    ERR_FAIL_COND(array.size() != array_size);

    if (p_format & RS::ARRAY_FORMAT_INDEX) {

        index_array_size = attribs[RS::ARRAY_INDEX].stride * p_index_count;
    }

    ERR_FAIL_COND(p_index_array.size() != index_array_size);

    ERR_FAIL_COND(p_blend_shapes.size() != mesh->blend_shape_count);

    for (int i = 0; i < p_blend_shapes.size(); i++) {
        ERR_FAIL_COND(p_blend_shapes[i].size() != array_size);
    }

         //ok all valid, create stuff
    RenderingEntity surface_ent=VSG::ecs->create();

    auto &surface(VSG::ecs->registry.emplace<RasterizerSurfaceComponent>(surface_ent));
    VSG::ecs->registry.emplace<RasterizerCommonGeometryComponent>(surface_ent,RasterizerCommonGeometryComponent::GEOMETRY_SURFACE);
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(surface_ent);
    surface.active = true;
    surface.array_len = p_vertex_count;
    surface.index_array_len = p_index_count;
    surface.array_byte_size = array.size();
    surface.index_array_byte_size = p_index_array.size();
    surface.primitive = p_primitive;
    surface.mesh = p_mesh;
    surface.format = p_format;
    surface.skeleton_bone_aabb.assign(p_bone_aabbs.begin(),p_bone_aabbs.end());
    surface.skeleton_bone_used.resize(surface.skeleton_bone_aabb.size(),false); // mark all unused
    surface.aabb = p_aabb;
    surface.max_bone = p_bone_aabbs.size();
    surface.total_data_size += surface.array_byte_size + surface.index_array_byte_size;

    for (int i = 0; i < surface.skeleton_bone_used.size(); i++) {
        if (surface.skeleton_bone_aabb[i].size.x >= 0 && surface.skeleton_bone_aabb[i].size.y >= 0 && surface.skeleton_bone_aabb[i].size.z >= 0)
            surface.skeleton_bone_used[i] = true;
    }

    for (int i = 0; i < RS::ARRAY_MAX; i++) {
        surface.attribs[i] = attribs[i];
    }

    {

        const uint8_t *vr = array.data();

        surface.vertex_id.create();
        glBindBuffer(GL_ARRAY_BUFFER, surface.vertex_id);
        glBufferData(GL_ARRAY_BUFFER, array_size, vr, (p_format & RS::ARRAY_FLAG_USE_DYNAMIC_UPDATE) ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

        if (p_format & RS::ARRAY_FORMAT_INDEX) {

            const uint8_t *ir = p_index_array.data();

            surface.index_id.create();
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface.index_id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_array_size, ir, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind
        }

             //generate arrays for faster state switching

        for (int ai = 0; ai < 2; ai++) {

            if (ai == 0) {
                //for normal draw
                surface.array_id.create();
                glBindVertexArray(surface.array_id);
                glBindBuffer(GL_ARRAY_BUFFER, surface.vertex_id);
            } else if (ai == 1) {
                //for instancing draw (can be changed and no one cares)
                surface.instancing_array_id.create();
                glBindVertexArray(surface.instancing_array_id);
                glBindBuffer(GL_ARRAY_BUFFER, surface.vertex_id);
            }

            for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {

                if (!attribs[i].enabled)
                    continue;

                if (attribs[i].integer) {
                    glVertexAttribIPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
                } else {
                    glVertexAttribPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
                }
                glEnableVertexAttribArray(attribs[i].index);
            }

            if (surface.index_id) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface.index_id);
            }

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

#ifdef DEBUG_ENABLED

        if (config.generate_wireframes && p_primitive == RS::PRIMITIVE_TRIANGLES) {
            //generate wireframes, this is used mostly by editor
            Vector<uint32_t> wf_indices;
            int index_count;

            if (p_format & RS::ARRAY_FORMAT_INDEX) {

                index_count = p_index_count * 2;
                wf_indices.resize(index_count);

                uint32_t *wr = wf_indices.data();

                if (p_vertex_count < (1 << 16)) {
                    //read 16 bit indices
                    const uint16_t *src_idx = (const uint16_t *)p_index_array.data();
                    for (int i = 0; i + 5 < index_count; i += 6) {

                        wr[i + 0] = src_idx[i / 2];
                        wr[i + 1] = src_idx[i / 2 + 1];
                        wr[i + 2] = src_idx[i / 2 + 1];
                        wr[i + 3] = src_idx[i / 2 + 2];
                        wr[i + 4] = src_idx[i / 2 + 2];
                        wr[i + 5] = src_idx[i / 2];
                    }

                } else {

                         //read 16 bit indices
                    const uint32_t *src_idx = (const uint32_t *)p_index_array.data();
                    for (int i = 0; i + 5 < index_count; i += 6) {

                        wr[i + 0] = src_idx[i / 2];
                        wr[i + 1] = src_idx[i / 2 + 1];
                        wr[i + 2] = src_idx[i / 2 + 1];
                        wr[i + 3] = src_idx[i / 2 + 2];
                        wr[i + 4] = src_idx[i / 2 + 2];
                        wr[i + 5] = src_idx[i / 2];
                    }
                }

            } else {

                index_count = p_vertex_count * 2;
                wf_indices.resize(index_count);
                uint32_t *wr = wf_indices.data();
                for (int i = 0; i + 5 < index_count; i += 6) {

                    wr[i + 0] = i / 2;
                    wr[i + 1] = i / 2 + 1;
                    wr[i + 2] = i / 2 + 1;
                    wr[i + 3] = i / 2 + 2;
                    wr[i + 4] = i / 2 + 2;
                    wr[i + 5] = i / 2;
                }
            }
            {
                const uint32_t *ir = wf_indices.data();

                surface.index_wireframe_id.create();
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface.index_wireframe_id);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), ir, GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind

                surface.index_wireframe_len = index_count;
            }

            for (int ai = 0; ai < 2; ai++) {

                if (ai == 0) {
                    //for normal draw
                    surface.array_wireframe_id.create();
                    glBindVertexArray(surface.array_wireframe_id);
                    glBindBuffer(GL_ARRAY_BUFFER, surface.vertex_id);
                } else if (ai == 1) {
                    //for instancing draw (can be changed and no one cares)
                    surface.instancing_array_wireframe_id.create();
                    glBindVertexArray(surface.instancing_array_wireframe_id);
                    glBindBuffer(GL_ARRAY_BUFFER, surface.vertex_id);
                }

                for (int i = 0; i < RS::ARRAY_MAX - 1; i++) {

                    if (!attribs[i].enabled)
                        continue;

                    if (attribs[i].integer) {
                        glVertexAttribIPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
                    } else {
                        glVertexAttribPointer(attribs[i].index, attribs[i].size, attribs[i].type, attribs[i].normalized, attribs[i].stride, CAST_INT_TO_UCHAR_PTR(attribs[i].offset));
                    }
                    glEnableVertexAttribArray(attribs[i].index);
                }

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface.index_wireframe_id);

                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

#endif
    }

    {

             //blend shapes

        for (int i = 0; i < p_blend_shapes.size(); i++) {

            RasterizerSurfaceComponent::BlendShape mt;

            Span<const uint8_t> vr = p_blend_shapes[i].toSpan();

            surface.total_data_size += array_size;

            mt.vertex_id.create();
            glBindBuffer(GL_ARRAY_BUFFER, mt.vertex_id);
            glBufferData(GL_ARRAY_BUFFER, array_size, vr.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

            mt.array_id.create();
            glBindVertexArray(mt.array_id);
            glBindBuffer(GL_ARRAY_BUFFER, mt.vertex_id);

            for (int j = 0; j < RS::ARRAY_MAX - 1; j++) {

                if (!attribs[j].enabled)
                    continue;

                if (attribs[j].integer) {
                    glVertexAttribIPointer(attribs[j].index, attribs[j].size, attribs[j].type, attribs[j].stride, CAST_INT_TO_UCHAR_PTR(attribs[j].offset));
                } else {
                    glVertexAttribPointer(attribs[j].index, attribs[j].size, attribs[j].type, attribs[j].normalized, attribs[j].stride, CAST_INT_TO_UCHAR_PTR(attribs[j].offset));
                }
                glEnableVertexAttribArray(attribs[j].index);
            }

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

            surface.blend_shapes.emplace_back(eastl::move(mt));
        }
    }

    mesh->surfaces.push_back(surface_ent);
    deps->instance_change_notify(true, true);

    get_rasterizer_storage_info().vertex_mem += surface.total_data_size;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_count(RenderingEntity p_mesh, int p_amount) {

    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_mesh);
    ERR_FAIL_COND(!mesh||!deps);

    ERR_FAIL_COND(!mesh->surfaces.empty());
    ERR_FAIL_COND(p_amount < 0);

    mesh->blend_shape_count = p_amount;
    deps->instance_change_notify(true, false);
}
int RasterizerStorageGLES3::mesh_get_blend_shape_count(RenderingEntity p_mesh) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, 0);

    return mesh->blend_shape_count;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_mode(RenderingEntity p_mesh, RS::BlendShapeMode p_mode) {

    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);

    mesh->blend_shape_mode = p_mode;
}
RS::BlendShapeMode RasterizerStorageGLES3::mesh_get_blend_shape_mode(RenderingEntity p_mesh) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, RS::BLEND_SHAPE_MODE_NORMALIZED);

    return mesh->blend_shape_mode;
}

void RasterizerStorageGLES3::mesh_set_blend_shape_values(RenderingEntity p_mesh, Span<const float> p_values) {
    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);
    mesh->blend_shape_values.assign(p_values.begin(),p_values.end());
}

Vector<float> RasterizerStorageGLES3::mesh_get_blend_shape_values(RenderingEntity p_mesh) const {
    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh,{});
    return mesh->blend_shape_values;
}

void RasterizerStorageGLES3::mesh_surface_update_region(RenderingEntity p_mesh, int p_surface, int p_offset, Span<const uint8_t> p_data) {

    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);
    ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());

    int total_size = p_data.size();
    auto *surf=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);
    ERR_FAIL_COND(p_offset + total_size > surf->array_byte_size);

    glBindBuffer(GL_ARRAY_BUFFER, surf->vertex_id);
    glBufferSubData(GL_ARRAY_BUFFER, p_offset, total_size, p_data.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
}

void RasterizerStorageGLES3::mesh_surface_set_material(RenderingEntity p_mesh, int p_surface, RenderingEntity p_material) {

    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_mesh);
    ERR_FAIL_COND(!mesh||!deps);
    ERR_FAIL_INDEX(p_surface, mesh->surfaces.size());

    auto surf_ent=mesh->surfaces[p_surface];
    auto *selected_surface=getUnchecked<RasterizerCommonGeometryComponent>(surf_ent);
    if (selected_surface->material == p_material)
        return;

    if (selected_surface->material!=entt::null) {
        material_remove_geometry(selected_surface->material, surf_ent);
    }

    selected_surface->material = p_material;

    if (selected_surface->material!=entt::null) {
        _material_add_geometry(selected_surface->material, surf_ent);
    }

    deps->instance_change_notify(false, true);
}
RenderingEntity RasterizerStorageGLES3::mesh_surface_get_material(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, entt::null);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), entt::null);
    auto * selected_surface=get<RasterizerCommonGeometryComponent>(mesh->surfaces[p_surface]);

    return selected_surface->material;
}

int RasterizerStorageGLES3::mesh_surface_get_array_len(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, 0);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);
    auto * selected_surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    return selected_surface->array_len;
}
int RasterizerStorageGLES3::mesh_surface_get_array_index_len(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, 0);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);
    auto * selected_surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    return selected_surface->index_array_len;
}

PoolVector<uint8_t> RasterizerStorageGLES3::mesh_surface_get_array(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    PoolVector<uint8_t> ret;
    ret.resize(surface->array_byte_size);
    glBindBuffer(GL_ARRAY_BUFFER, surface->vertex_id);

    {
        PoolVector<uint8_t>::Write w = ret.write();
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, surface->array_byte_size, w.ptr());
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return ret;
}

PoolVector<uint8_t> RasterizerStorageGLES3::mesh_surface_get_index_array(RenderingEntity p_mesh, int p_surface) const {
    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, PoolVector<uint8_t>());
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), PoolVector<uint8_t>());

    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    PoolVector<uint8_t> ret;
    ret.resize(surface->index_array_byte_size);

    if (surface->index_array_byte_size > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->index_id);

        {
            PoolVector<uint8_t>::Write w = ret.write();
            glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, surface->index_array_byte_size, w.ptr());
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    return ret;
}

uint32_t RasterizerStorageGLES3::mesh_surface_get_format(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);

    ERR_FAIL_COND_V(!mesh, 0);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), 0);

    auto * selected_surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);
    return selected_surface->format;
}

RS::PrimitiveType RasterizerStorageGLES3::mesh_surface_get_primitive_type(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, RS::PRIMITIVE_MAX);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), RS::PRIMITIVE_MAX);
    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    return surface->primitive;
}

AABB RasterizerStorageGLES3::mesh_surface_get_aabb(RenderingEntity p_mesh, int p_surface) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, AABB());
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), AABB());
    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    return surface->aabb;
}

Vector<Vector<uint8_t>> RasterizerStorageGLES3::mesh_surface_get_blend_shapes(RenderingEntity p_mesh, int p_surface) const {

    Vector<Vector<uint8_t> > bsarr;
    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, bsarr);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), bsarr);
    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    bsarr.reserve(surface->blend_shapes.size());
    for (size_t i = 0; i < surface->blend_shapes.size(); i++) {

        Vector<uint8_t> ret;
        ret.resize(surface->array_byte_size);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surface->blend_shapes[i].vertex_id);

        {
            glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, surface->array_byte_size, ret.data());
        }

        bsarr.emplace_back(eastl::move(ret));
    }

    return bsarr;
}

const Vector<AABB> &RasterizerStorageGLES3::mesh_surface_get_skeleton_aabb(RenderingEntity p_mesh, int p_surface) const {
    static const Vector<AABB> null_aabb_pvec;

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, null_aabb_pvec);
    ERR_FAIL_INDEX_V(p_surface, mesh->surfaces.size(), null_aabb_pvec);
    auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[p_surface]);

    return surface->skeleton_bone_aabb;
}

void RasterizerStorageGLES3::mesh_remove_surface(RenderingEntity p_mesh, int p_surface) {

    RasterizerMeshComponent *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);
    ::mesh_remove_surface(mesh,p_surface);
}

int RasterizerStorageGLES3::mesh_get_surface_count(RenderingEntity p_mesh) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, 0);
    return mesh->surfaces.size();
}

void RasterizerStorageGLES3::mesh_set_custom_aabb(RenderingEntity p_mesh, const AABB &p_aabb) {

    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);

    mesh->custom_aabb = p_aabb;
    getUnchecked<RasterizerInstantiableComponent>(p_mesh)->instance_change_notify(true, false);
}

AABB RasterizerStorageGLES3::mesh_get_custom_aabb(RenderingEntity p_mesh) const {

    const auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, AABB());

    return mesh->custom_aabb;
}
AABB RasterizerStorageGLES3::mesh_get_aabb(RenderingEntity p_mesh, RenderingEntity p_skeleton) const {

    RasterizerMeshComponent *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND_V(!mesh, AABB());
    return ::mesh_get_aabb(mesh,p_skeleton);
}

AABB mesh_get_aabb(const RasterizerMeshComponent *mesh, RenderingEntity p_skeleton) {

    if (mesh->custom_aabb != AABB()) {
        return mesh->custom_aabb;
    }

    const RasterizerSkeletonComponent *sk = get<RasterizerSkeletonComponent>(p_skeleton);

    AABB aabb;

    if (sk && sk->size != 0) {

        for (size_t i = 0; i < mesh->surfaces.size(); i++) {

            AABB laabb;
            auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[i]);
            if ((surface->format & RS::ARRAY_FORMAT_BONES) && !surface->skeleton_bone_aabb.empty()) {

                int bs = surface->skeleton_bone_aabb.size();
                const AABB *skbones = surface->skeleton_bone_aabb.data();
                const bool *skused = surface->skeleton_bone_used.data();

                int sbs = sk->size;
                ERR_CONTINUE(bs > sbs);
                const float *texture = sk->skel_texture.data();

                bool first = true;
                if (sk->use_2d) {
                    for (int j = 0; j < bs; j++) {

                        if (!skused[j])
                            continue;

                        int base_ofs = ((j / 256) * 256) * 2 * 4 + (j % 256) * 4;

                        Transform mtx;

                        mtx.basis[0].x = texture[base_ofs + 0];
                        mtx.basis[0].y = texture[base_ofs + 1];
                        mtx.origin.x = texture[base_ofs + 3];
                        base_ofs += 256 * 4;
                        mtx.basis[1].x = texture[base_ofs + 0];
                        mtx.basis[1].y = texture[base_ofs + 1];
                        mtx.origin.y = texture[base_ofs + 3];

                        AABB baabb = mtx.xform(skbones[j]);

                        if (first) {
                            laabb = baabb;
                            first = false;
                        } else {
                            laabb.merge_with(baabb);
                        }
                    }
                } else {
                    for (int j = 0; j < bs; j++) {

                        if (!skused[j])
                            continue;

                        int base_ofs = ((j / 256) * 256) * 3 * 4 + (j % 256) * 4;

                        Transform mtx;

                        mtx.basis[0].x = texture[base_ofs + 0];
                        mtx.basis[0].y = texture[base_ofs + 1];
                        mtx.basis[0].z = texture[base_ofs + 2];
                        mtx.origin.x = texture[base_ofs + 3];
                        base_ofs += 256 * 4;
                        mtx.basis[1].x = texture[base_ofs + 0];
                        mtx.basis[1].y = texture[base_ofs + 1];
                        mtx.basis[1].z = texture[base_ofs + 2];
                        mtx.origin.y = texture[base_ofs + 3];
                        base_ofs += 256 * 4;
                        mtx.basis[2].x = texture[base_ofs + 0];
                        mtx.basis[2].y = texture[base_ofs + 1];
                        mtx.basis[2].z = texture[base_ofs + 2];
                        mtx.origin.z = texture[base_ofs + 3];

                        AABB baabb = mtx.xform(skbones[j]);
                        if (first) {
                            laabb = baabb;
                            first = false;
                        } else {
                            laabb.merge_with(baabb);
                        }
                    }
                }

            } else {

                laabb = surface->aabb;
            }

            if (i == 0)
                aabb = laabb;
            else
                aabb.merge_with(laabb);
        }
    } else {

        for (int i = 0; i < mesh->surfaces.size(); i++) {

            auto * surface=get<RasterizerSurfaceComponent>(mesh->surfaces[i]);
            if (i == 0)
                aabb = surface->aabb;
            else
                aabb.merge_with(surface->aabb);
        }
    }

    return aabb;
}



void RasterizerStorageGLES3::mesh_clear(RenderingEntity p_mesh) {
    auto *mesh = get<RasterizerMeshComponent>(p_mesh);
    ERR_FAIL_COND(!mesh);

    while (!mesh->surfaces.empty()) {
        mesh_remove_surface(p_mesh, 0);
    }
}

void RasterizerStorageGLES3::mesh_render_blend_shapes(RasterizerSurfaceComponent *s, const float *p_weights) {
    glBindVertexArray(s->array_id);

    static constexpr const BlendShapeShaderGLES3::Conditionals cond[RS::ARRAY_MAX - 1] = {
        BlendShapeShaderGLES3::ENABLE_NORMAL, // will be ignored
        BlendShapeShaderGLES3::ENABLE_NORMAL,
        BlendShapeShaderGLES3::ENABLE_TANGENT,
        BlendShapeShaderGLES3::ENABLE_COLOR,
        BlendShapeShaderGLES3::ENABLE_UV,
        BlendShapeShaderGLES3::ENABLE_UV2,
        BlendShapeShaderGLES3::ENABLE_SKELETON,
        BlendShapeShaderGLES3::ENABLE_SKELETON,
    };

    int stride = 0;

    if (s->format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
        stride = 2 * 4;
    } else {
        stride = 3 * 4;
    }

    static const int sizes[RS::ARRAY_MAX - 1] = {
        3 * 4,
        3 * 4,
        4 * 4,
        4 * 4,
        2 * 4,
        2 * 4,
        4 * 4,
        4 * 4
    };

    for (int i = 1; i < RS::ARRAY_MAX - 1; i++) {
        shaders.blend_shapes.set_conditional(cond[i], s->format & (1 << i)); //enable conditional for format
        if (s->format & (1 << i)) {
            stride += sizes[i];
        }
    }

         //copy all first
    float base_weight = 1.0;

    size_t mtc = s->blend_shapes.size();

    if (VSG::ecs->registry.get<RasterizerMeshComponent>(s->mesh).blend_shape_mode == RS::BLEND_SHAPE_MODE_NORMALIZED) {

        for (int i = 0; i < mtc; i++) {
            base_weight -= p_weights[i];
        }
    }

    shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::ENABLE_BLEND, false); //first pass does not blend
    shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::USE_2D_VERTEX, s->format & RS::ARRAY_FLAG_USE_2D_VERTICES); //use 2D vertices if needed

    shaders.blend_shapes.bind();

    shaders.blend_shapes.set_uniform(BlendShapeShaderGLES3::BLEND_AMOUNT, base_weight);
    glEnable(GL_RASTERIZER_DISCARD);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, resources.transform_feedback_buffers[0]);
    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, s->array_len);
    glEndTransformFeedback();

    shaders.blend_shapes.set_conditional(BlendShapeShaderGLES3::ENABLE_BLEND, true); //first pass does not blend
    shaders.blend_shapes.bind();

    for (int ti = 0; ti < mtc; ti++) {
        float weight = p_weights[ti];

        if (Math::is_zero_approx(weight)) {
            //not bother with this one
            continue;
        }

        glBindVertexArray(s->blend_shapes[ti].array_id);
        glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[0]);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, resources.transform_feedback_buffers[1]);

        shaders.blend_shapes.set_uniform(BlendShapeShaderGLES3::BLEND_AMOUNT, weight);

        int ofs = 0;
        for (uint8_t i = 0; i < RS::ARRAY_MAX - 1; i++) {

            if (s->format & (1 << i)) {
                glEnableVertexAttribArray(i + 8);
                switch (i) {

                    case RS::ARRAY_VERTEX: {
                        if (s->format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                            glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                            ofs += 2 * 4;
                        } else {
                            glVertexAttribPointer(i + 8, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                            ofs += 3 * 4;
                        }
                    } break;
                    case RS::ARRAY_NORMAL: {
                        glVertexAttribPointer(i + 8, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 3 * 4;
                    } break;
                    case RS::ARRAY_TANGENT: {
                        glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 4 * 4;

                    } break;
                    case RS::ARRAY_COLOR: {
                        glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 4 * 4;

                    } break;
                    case RS::ARRAY_TEX_UV: {
                        glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 2 * 4;

                    } break;
                    case RS::ARRAY_TEX_UV2: {
                        glVertexAttribPointer(i + 8, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 2 * 4;

                    } break;
                    case RS::ARRAY_BONES: {
                        glVertexAttribIPointer(i + 8, 4, GL_UNSIGNED_INT, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 4 * 4;

                    } break;
                    case RS::ARRAY_WEIGHTS: {
                        glVertexAttribPointer(i + 8, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 4 * 4;

                    } break;
                }

            } else {
                glDisableVertexAttribArray(i + 8);
            }
        }

        glBeginTransformFeedback(GL_POINTS);
        glDrawArrays(GL_POINTS, 0, s->array_len);
        glEndTransformFeedback();

        SWAP(resources.transform_feedback_buffers[0], resources.transform_feedback_buffers[1]);
    }

    glDisable(GL_RASTERIZER_DISCARD);
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);

    glBindVertexArray(resources.transform_feedback_array);
    glBindBuffer(GL_ARRAY_BUFFER, resources.transform_feedback_buffers[0]);

    int ofs = 0;
    for (uint8_t i = 0; i < RS::ARRAY_MAX - 1; i++) {

        if (s->format & (1 << i)) {
            glEnableVertexAttribArray(i);
            switch (i) {

                case RS::ARRAY_VERTEX: {
                    if (s->format & RS::ARRAY_FLAG_USE_2D_VERTICES) {
                        glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 2 * 4;
                    } else {
                        glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                        ofs += 3 * 4;
                    }
                } break;
                case RS::ARRAY_NORMAL: {
                    glVertexAttribPointer(i, 3, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 3 * 4;
                } break;
                case RS::ARRAY_TANGENT: {
                    glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 4 * 4;

                } break;
                case RS::ARRAY_COLOR: {
                    glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 4 * 4;

                } break;
                case RS::ARRAY_TEX_UV: {
                    glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 2 * 4;

                } break;
                case RS::ARRAY_TEX_UV2: {
                    glVertexAttribPointer(i, 2, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 2 * 4;

                } break;
                case RS::ARRAY_BONES: {
                    glVertexAttribIPointer(i, 4, GL_UNSIGNED_INT, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 4 * 4;

                } break;
                case RS::ARRAY_WEIGHTS: {
                    glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, stride, CAST_INT_TO_UCHAR_PTR(ofs));
                    ofs += 4 * 4;

                } break;
            }

        } else {
            glDisableVertexAttribArray(i);
        }
    }

    if (s->index_array_len) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_id);
    }
}

