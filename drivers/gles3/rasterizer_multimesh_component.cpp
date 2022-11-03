#include "rasterizer_multimesh_component.h"

#include "rasterizer_mesh_component.h"
#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

struct MultimeshDirtyMarker {
    bool dirty_aabb;
    bool dirty_data;
};

static void multimesh_allocate(RasterizerMultiMeshComponent *multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data_format=RS::MULTIMESH_CUSTOM_DATA_NONE);

void update_dirty_multimeshes() {
    auto vw(VSG::ecs->registry.view<RasterizerMultiMeshComponent, MultimeshDirtyMarker>());
    vw.each([](RenderingEntity en, RasterizerMultiMeshComponent &multimesh,MultimeshDirtyMarker &dirty) {
        auto &deps(VSG::ecs->registry.get<RasterizerInstantiableComponent>(en));
        if(!multimesh.size) {
            deps.instance_change_notify(true, false);
            return;
        }
        if (dirty.dirty_data) {
            glBindBuffer(GL_ARRAY_BUFFER, multimesh.buffer);
            uint32_t buffer_size = multimesh.data.size() * sizeof(float);
            glBufferData(GL_ARRAY_BUFFER, buffer_size, multimesh.data.read().ptr(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        if (dirty.dirty_aabb) {
            AABB mesh_aabb;

            if (multimesh.mesh!=entt::null) {
                mesh_aabb = mesh_get_aabb(get<RasterizerMeshComponent>(multimesh.mesh), entt::null);
            } else {
                mesh_aabb.size += Vector3(0.001f, 0.001f, 0.001f);
            }

            int stride = multimesh.color_floats + multimesh.xform_floats + multimesh.custom_data_floats;
            int count = multimesh.data.size();
            auto multimesh_wr(multimesh.data.write());

            float *data = &multimesh_wr[0];

            AABB aabb;

            if (multimesh.transform_format == RS::MULTIMESH_TRANSFORM_2D) {
                for (int i = 0; i < count; i += stride) {
                    float *dataptr = &data[i];
                    Transform xform;
                    xform.basis[0][0] = dataptr[0];
                    xform.basis[0][1] = dataptr[1];
                    xform.origin[0] = dataptr[3];
                    xform.basis[1][0] = dataptr[4];
                    xform.basis[1][1] = dataptr[5];
                    xform.origin[1] = dataptr[7];

                    AABB laabb = xform.xform(mesh_aabb);
                    if (i == 0)
                        aabb = laabb;
                    else
                        aabb.merge_with(laabb);
                }
            } else {
                for (int i = 0; i < count; i += stride) {
                    float *dataptr = &data[i];
                    Transform xform;

                    xform.basis.elements[0][0] = dataptr[0];
                    xform.basis.elements[0][1] = dataptr[1];
                    xform.basis.elements[0][2] = dataptr[2];
                    xform.origin.x = dataptr[3];
                    xform.basis.elements[1][0] = dataptr[4];
                    xform.basis.elements[1][1] = dataptr[5];
                    xform.basis.elements[1][2] = dataptr[6];
                    xform.origin.y = dataptr[7];
                    xform.basis.elements[2][0] = dataptr[8];
                    xform.basis.elements[2][1] = dataptr[9];
                    xform.basis.elements[2][2] = dataptr[10];
                    xform.origin.z = dataptr[11];

                    AABB laabb = xform.xform(mesh_aabb);
                    if (i == 0)
                        aabb = laabb;
                    else
                        aabb.merge_with(laabb);
                }
            }

            multimesh.aabb = aabb;
        }
        deps.instance_change_notify(true, false);
    });
    VSG::ecs->registry.clear<MultimeshDirtyMarker>();
}
//! If this multimesh has a source mesh, tell the mesh we're no longer using it.
void RasterizerMultiMeshComponent::unregister_from_mesh()
{
    if (mesh == entt::null) {
        return;
    }

    auto *belongs_to_mesh = VSG::ecs->try_get<RasterizerMeshComponent>(mesh);
    if (belongs_to_mesh) {
        belongs_to_mesh->multimeshes.erase_first_unsorted(self);
    }

    // asume this!=&from
    auto inst = VSG::ecs->try_get<RasterizerInstantiableComponent>(self);
    if(inst)
        inst->instance_remove_deps();

    RasterizerMeshComponent* mesh_cm = get<RasterizerMeshComponent>(mesh);
    if (mesh_cm) {
        mesh_cm->multimeshes.erase_first_unsorted(self);
    }

    multimesh_allocate(this, 0, RS::MULTIMESH_TRANSFORM_2D, RS::MULTIMESH_COLOR_NONE); //frees multimesh

}


static void multimesh_allocate(RasterizerMultiMeshComponent *multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data_format) {

    if (multimesh->size == p_instances && multimesh->transform_format == p_transform_format && multimesh->color_format == p_color_format && multimesh->custom_data_format == p_data_format) {
        return;
    }

    if (multimesh->buffer) {
        multimesh->buffer.release();
        multimesh->data.resize(0);
    }

    multimesh->size = p_instances;
    multimesh->transform_format = p_transform_format;
    multimesh->color_format = p_color_format;
    multimesh->custom_data_format = p_data_format;

    if (multimesh->size) {

        if (multimesh->transform_format == RS::MULTIMESH_TRANSFORM_2D) {
            multimesh->xform_floats = 8;
        } else {
            multimesh->xform_floats = 12;
        }

        if (multimesh->color_format == RS::MULTIMESH_COLOR_8BIT) {
            multimesh->color_floats = 1;
        } else if (multimesh->color_format == RS::MULTIMESH_COLOR_FLOAT) {
            multimesh->color_floats = 4;
        } else {
            multimesh->color_floats = 0;
        }

        if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_8BIT) {
            multimesh->custom_data_floats = 1;
        } else if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_FLOAT) {
            multimesh->custom_data_floats = 4;
        } else {
            multimesh->custom_data_floats = 0;
        }

        int format_floats = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;

        multimesh->data.resize(format_floats * p_instances);
        auto multimesh_wr(multimesh->data.write());

        float *dataptr = multimesh_wr.ptr();

        for (int i = 0; i < p_instances * format_floats; i += format_floats) {

            int color_from;
            int custom_data_from;

            if (multimesh->transform_format == RS::MULTIMESH_TRANSFORM_2D) {
                dataptr[i + 0] = 1.0;
                dataptr[i + 1] = 0.0;
                dataptr[i + 2] = 0.0;
                dataptr[i + 3] = 0.0;
                dataptr[i + 4] = 0.0;
                dataptr[i + 5] = 1.0;
                dataptr[i + 6] = 0.0;
                dataptr[i + 7] = 0.0;
                color_from = 8;
                custom_data_from = 8;
            } else {
                dataptr[i + 0] = 1.0;
                dataptr[i + 1] = 0.0;
                dataptr[i + 2] = 0.0;
                dataptr[i + 3] = 0.0;
                dataptr[i + 4] = 0.0;
                dataptr[i + 5] = 1.0;
                dataptr[i + 6] = 0.0;
                dataptr[i + 7] = 0.0;
                dataptr[i + 8] = 0.0;
                dataptr[i + 9] = 0.0;
                dataptr[i + 10] = 1.0;
                dataptr[i + 11] = 0.0;
                color_from = 12;
                custom_data_from = 12;
            }

            if (multimesh->color_format == RS::MULTIMESH_COLOR_NONE) {
                //none
            } else if (multimesh->color_format == RS::MULTIMESH_COLOR_8BIT) {

                union {
                    uint32_t colu;
                    float colf;
                } cu;

                cu.colu = 0xFFFFFFFF;
                dataptr[i + color_from + 0] = cu.colf;
                custom_data_from = color_from + 1;

            } else if (multimesh->color_format == RS::MULTIMESH_COLOR_FLOAT) {
                dataptr[i + color_from + 0] = 1.0;
                dataptr[i + color_from + 1] = 1.0;
                dataptr[i + color_from + 2] = 1.0;
                dataptr[i + color_from + 3] = 1.0;
                custom_data_from = color_from + 4;
            }

            if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_NONE) {
                //none
            } else if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_8BIT) {

                union {
                    uint32_t colu;
                    float colf;
                } cu;

                cu.colu = 0;
                dataptr[i + custom_data_from + 0] = cu.colf;

            } else if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_FLOAT) {
                dataptr[i + custom_data_from + 0] = 0.0;
                dataptr[i + custom_data_from + 1] = 0.0;
                dataptr[i + custom_data_from + 2] = 0.0;
                dataptr[i + custom_data_from + 3] = 0.0;
            }
        }

        multimesh->buffer.create();
        glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
        glBufferData(GL_ARRAY_BUFFER, multimesh->data.size() * sizeof(float), nullptr, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(multimesh->self,true,true);
}

void mark_multimeshes_dirty(Span<RenderingEntity> meshes)
{
    VSG::ecs->registry.insert(meshes.begin(),meshes.end(),MultimeshDirtyMarker{true,false});
}


/* MULTIMESH API */

RenderingEntity RasterizerStorageGLES3::multimesh_create() {
    auto res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerMultiMeshComponent>(res,res);
    return res;
}


void RasterizerStorageGLES3::multimesh_allocate(RenderingEntity p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, RS::MultimeshColorFormat p_color_format, RS::MultimeshCustomDataFormat p_data_format) {

    RasterizerMultiMeshComponent *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ::multimesh_allocate(multimesh,p_instances, p_transform_format, p_color_format, p_data_format);
}

int RasterizerStorageGLES3::multimesh_get_instance_count(RenderingEntity p_multimesh) const {

    auto *multimesh = VSG::ecs->try_get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, 0);

    return multimesh->size;
}

void RasterizerStorageGLES3::multimesh_set_mesh(RenderingEntity p_multimesh, RenderingEntity p_mesh) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
#ifdef DEBUG_ENABLED
    if(multimesh->mesh==p_multimesh) {
        WARN_PRINT("Multimesh set the same mesh multiple times.");
    }
#endif
    if (multimesh->mesh!=entt::null) {
        auto *mesh = VSG::ecs->try_get<RasterizerMeshComponent>(multimesh->mesh);
        if (mesh) {
            mesh->multimeshes.erase_first_unsorted(p_multimesh);
        }
    }

    multimesh->mesh = p_mesh;

    if (multimesh->mesh!=entt::null) {
        auto *mesh = get<RasterizerMeshComponent>(p_mesh);
        if (mesh) {
            mesh->multimeshes.push_back(p_multimesh);
        }
    }
    VSG::ecs->registry.get_or_emplace<MultimeshDirtyMarker>(p_multimesh).dirty_aabb = true;
}

void RasterizerStorageGLES3::multimesh_instance_set_transform(RenderingEntity p_multimesh, int p_index, const Transform &p_transform) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->size);
    ERR_FAIL_COND(multimesh->transform_format == RS::MULTIMESH_TRANSFORM_2D);

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float *dataptr = &multimesh_wr[stride * p_index];

    dataptr[0] = p_transform.basis.elements[0][0];
    dataptr[1] = p_transform.basis.elements[0][1];
    dataptr[2] = p_transform.basis.elements[0][2];
    dataptr[3] = p_transform.origin.x;
    dataptr[4] = p_transform.basis.elements[1][0];
    dataptr[5] = p_transform.basis.elements[1][1];
    dataptr[6] = p_transform.basis.elements[1][2];
    dataptr[7] = p_transform.origin.y;
    dataptr[8] = p_transform.basis.elements[2][0];
    dataptr[9] = p_transform.basis.elements[2][1];
    dataptr[10] = p_transform.basis.elements[2][2];
    dataptr[11] = p_transform.origin.z;

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(p_multimesh,true,true);

}

void RasterizerStorageGLES3::multimesh_instance_set_transform_2d(RenderingEntity p_multimesh, int p_index, const Transform2D &p_transform) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->size);
    ERR_FAIL_COND(multimesh->transform_format == RS::MULTIMESH_TRANSFORM_3D);

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());
    float *dataptr = &multimesh_wr[stride * p_index];

    dataptr[0] = p_transform.elements[0][0];
    dataptr[1] = p_transform.elements[1][0];
    dataptr[2] = 0;
    dataptr[3] = p_transform.elements[2][0];
    dataptr[4] = p_transform.elements[0][1];
    dataptr[5] = p_transform.elements[1][1];
    dataptr[6] = 0;
    dataptr[7] = p_transform.elements[2][1];

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(p_multimesh,true,true);

}
void RasterizerStorageGLES3::multimesh_instance_set_color(RenderingEntity p_multimesh, int p_index, const Color &p_color) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->size);
    ERR_FAIL_COND(multimesh->color_format == RS::MULTIMESH_COLOR_NONE);
    ERR_FAIL_INDEX(multimesh->color_format, RS::MULTIMESH_COLOR_MAX);


    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float *dataptr = &multimesh_wr[stride * p_index + multimesh->xform_floats];

    if (multimesh->color_format == RS::MULTIMESH_COLOR_8BIT) {

        uint8_t *data8 = (uint8_t *)dataptr;
        data8[0] = CLAMP(p_color.r * 255.0f, 0.0f, 255.0f);
        data8[1] = CLAMP(p_color.g * 255.0f, 0.0f, 255.0f);
        data8[2] = CLAMP(p_color.b * 255.0f, 0.0f, 255.0f);
        data8[3] = CLAMP(p_color.a * 255.0f, 0.0f, 255.0f);

    } else if (multimesh->color_format == RS::MULTIMESH_COLOR_FLOAT) {
        dataptr[0] = p_color.r;
        dataptr[1] = p_color.g;
        dataptr[2] = p_color.b;
        dataptr[3] = p_color.a;
    }

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(p_multimesh,true,true);
}

void RasterizerStorageGLES3::multimesh_instance_set_custom_data(RenderingEntity p_multimesh, int p_index, const Color &p_custom_data) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->size);
    ERR_FAIL_COND(multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_NONE);
    ERR_FAIL_INDEX(multimesh->custom_data_format, RS::MULTIMESH_CUSTOM_DATA_MAX);

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float *dataptr = &multimesh_wr[stride * p_index + multimesh->xform_floats + multimesh->color_floats];

    if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_8BIT) {

        uint8_t *data8 = (uint8_t *)dataptr;
        data8[0] = CLAMP(p_custom_data.r * 255.0f, 0.0f, 255.0f);
        data8[1] = CLAMP(p_custom_data.g * 255.0f, 0.0f, 255.0f);
        data8[2] = CLAMP(p_custom_data.b * 255.0f, 0.0f, 255.0f);
        data8[3] = CLAMP(p_custom_data.a * 255.0f, 0.0f, 255.0f);

    } else if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_FLOAT) {
        dataptr[0] = p_custom_data.r;
        dataptr[1] = p_custom_data.g;
        dataptr[2] = p_custom_data.b;
        dataptr[3] = p_custom_data.a;
    }

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(p_multimesh,true,true);
}
RenderingEntity RasterizerStorageGLES3::multimesh_get_mesh(RenderingEntity p_multimesh) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, entt::null);

    return multimesh->mesh;
}

Transform RasterizerStorageGLES3::multimesh_instance_get_transform(RenderingEntity p_multimesh, int p_index) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, Transform());
    ERR_FAIL_INDEX_V(p_index, multimesh->size, Transform());
    ERR_FAIL_COND_V(multimesh->transform_format == RS::MULTIMESH_TRANSFORM_2D, Transform());

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float *dataptr = &multimesh_wr[stride * p_index];

    Transform xform;

    xform.basis.elements[0][0] = dataptr[0];
    xform.basis.elements[0][1] = dataptr[1];
    xform.basis.elements[0][2] = dataptr[2];
    xform.origin.x = dataptr[3];
    xform.basis.elements[1][0] = dataptr[4];
    xform.basis.elements[1][1] = dataptr[5];
    xform.basis.elements[1][2] = dataptr[6];
    xform.origin.y = dataptr[7];
    xform.basis.elements[2][0] = dataptr[8];
    xform.basis.elements[2][1] = dataptr[9];
    xform.basis.elements[2][2] = dataptr[10];
    xform.origin.z = dataptr[11];

    return xform;
}
Transform2D RasterizerStorageGLES3::multimesh_instance_get_transform_2d(RenderingEntity p_multimesh, int p_index) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, Transform2D());
    ERR_FAIL_INDEX_V(p_index, multimesh->size, Transform2D());
    ERR_FAIL_COND_V(multimesh->transform_format == RS::MULTIMESH_TRANSFORM_3D, Transform2D());

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float* dataptr = &multimesh_wr[stride * p_index];

    Transform2D xform;

    xform.elements[0][0] = dataptr[0];
    xform.elements[1][0] = dataptr[1];
    xform.elements[2][0] = dataptr[3];
    xform.elements[0][1] = dataptr[4];
    xform.elements[1][1] = dataptr[5];
    xform.elements[2][1] = dataptr[7];

    return xform;
}

Color RasterizerStorageGLES3::multimesh_instance_get_color(RenderingEntity p_multimesh, int p_index) const {
    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, Color());
    ERR_FAIL_INDEX_V(p_index, multimesh->size, Color());
    ERR_FAIL_COND_V(multimesh->color_format == RS::MULTIMESH_COLOR_NONE, Color());
    ERR_FAIL_INDEX_V(multimesh->color_format, RS::MULTIMESH_COLOR_MAX, Color());

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float* dataptr = &multimesh_wr[stride * p_index + multimesh->xform_floats];

    if (multimesh->color_format == RS::MULTIMESH_COLOR_8BIT) {
        union {
            uint32_t colu;
            float colf;
        } cu;

        cu.colf = dataptr[0];

        return Color::hex(BSWAP32(cu.colu));

    } else if (multimesh->color_format == RS::MULTIMESH_COLOR_FLOAT) {
        Color c;
        c.r = dataptr[0];
        c.g = dataptr[1];
        c.b = dataptr[2];
        c.a = dataptr[3];

        return c;
    }

    return Color();
}

Color RasterizerStorageGLES3::multimesh_instance_get_custom_data(RenderingEntity p_multimesh, int p_index) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, Color());
    ERR_FAIL_INDEX_V(p_index, multimesh->size, Color());
    ERR_FAIL_COND_V(multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_NONE, Color());
    ERR_FAIL_INDEX_V(multimesh->custom_data_format, RS::MULTIMESH_CUSTOM_DATA_MAX, Color());

    int stride = multimesh->color_floats + multimesh->xform_floats + multimesh->custom_data_floats;
    auto multimesh_wr(multimesh->data.write());

    float* dataptr = &multimesh_wr[stride * p_index + multimesh->xform_floats + multimesh->color_floats];

    if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_8BIT) {
        union {
            uint32_t colu;
            float colf;
        } cu;

        cu.colf = dataptr[0];

        return Color::hex(BSWAP32(cu.colu));

    } else if (multimesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_FLOAT) {
        Color c;
        c.r = dataptr[0];
        c.g = dataptr[1];
        c.b = dataptr[2];
        c.a = dataptr[3];

        return c;
    }

    return Color();
}

void RasterizerStorageGLES3::multimesh_set_as_bulk_array(RenderingEntity p_multimesh, Span<const float> p_array) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);
    ERR_FAIL_COND(multimesh->data.empty());

    int dsize = multimesh->data.size();

    ERR_FAIL_COND(dsize != p_array.size());

    memcpy(multimesh->data.write().ptr(), p_array.data(), dsize * sizeof(float));

    VSG::ecs->registry.emplace_or_replace<MultimeshDirtyMarker>(p_multimesh,true,true);
}

void RasterizerStorageGLES3::multimesh_set_visible_instances(RenderingEntity p_multimesh, int p_visible) {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);

    multimesh->visible_instances = p_visible;
}
int RasterizerStorageGLES3::multimesh_get_visible_instances(RenderingEntity p_multimesh) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, -1);

    return multimesh->visible_instances;
}

AABB RasterizerStorageGLES3::multimesh_get_aabb(RenderingEntity p_multimesh) const {

    auto *multimesh = get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND_V(!multimesh, AABB());

    update_dirty_multimeshes(); //update pending AABBs

    return multimesh->aabb;
}

RasterizerMultiMeshComponent::RasterizerMultiMeshComponent(RenderingEntity s) : self(s) {
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(s);
}

RasterizerMultiMeshComponent &RasterizerMultiMeshComponent::operator=(RasterizerMultiMeshComponent &&from) {

    unregister_from_mesh();

    mesh = eastl::move(from.mesh);
    size = from.size;
    transform_format = from.transform_format;
    color_format = from.color_format;
    custom_data_format = from.custom_data_format;
    data = eastl::move(from.data);
    aabb = from.aabb;
    self = eastl::move(from.self);
    from.self = entt::null;
    buffer= eastl::move(from.buffer);
    visible_instances=from.visible_instances;

    xform_floats=from.xform_floats;
    color_floats=from.color_floats;
    custom_data_floats=from.custom_data_floats;

    return *this;
}

RasterizerMultiMeshComponent::~RasterizerMultiMeshComponent() {
    unregister_from_mesh();
}

void multimesh_remove_base_mesh(RenderingEntity p_multimesh)
{
    auto *multimesh = VSG::ecs->try_get<RasterizerMultiMeshComponent>(p_multimesh);
    ERR_FAIL_COND(!multimesh);

    multimesh->mesh = entt::null;
    VSG::ecs->registry.get_or_emplace<MultimeshDirtyMarker>(p_multimesh).dirty_aabb=true;
}

