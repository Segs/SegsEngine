#pragma once

#include "rasterizer_gl_unique_handle.h"
#include "servers/rendering/render_entity_helpers.h"
#include "servers/rendering_server_enums.h"
#include "core/image.h"
#include "core/math/aabb.h"
#include "core/string.h"
#include "core/vector.h"

struct RasterizerMeshComponent;

struct RasterizerSurfaceComponent {

    struct Attrib {

        GLuint index;
        GLint size;
        uint32_t offset;
        GLenum type;
        GLsizei stride;
        bool enabled;
        bool integer;
        GLboolean normalized;
    };
    struct BlendShape {
        GLBufferHandle vertex_id;
        GLVAOHandle array_id;
        BlendShape() = default;
        BlendShape(BlendShape &&) = default;
        BlendShape &operator=(BlendShape &&) = default;
    };

    Attrib attribs[RS::ARRAY_MAX];
    Vector<AABB> skeleton_bone_aabb;
    Vector<bool> skeleton_bone_used;
    Vector<BlendShape> blend_shapes;

    AABB aabb;
    RenderingEntity mesh=entt::null; //RasterizerMeshComponent *
    uint32_t format=0;

    GLVAOHandle array_id;
    GLVAOHandle instancing_array_id;
    GLBufferHandle vertex_id;
    GLBufferHandle index_id;

    GLBufferHandle index_wireframe_id;
    GLVAOHandle array_wireframe_id;
    GLVAOHandle instancing_array_wireframe_id;
    int index_wireframe_len=0;

    int array_len=0;
    int index_array_len=0;
    int max_bone;

    int array_byte_size=0;
    int index_array_byte_size=0;

    RS::PrimitiveType primitive = RS::PRIMITIVE_POINTS;

    int total_data_size=0;
    bool active=false;

    void material_changed_notify();

    RasterizerSurfaceComponent(const RasterizerSurfaceComponent &) = delete;
    RasterizerSurfaceComponent &operator=(const RasterizerSurfaceComponent &) = delete;

    RasterizerSurfaceComponent(RasterizerSurfaceComponent &&) = default;
    RasterizerSurfaceComponent &operator=(RasterizerSurfaceComponent &&) = default;

    RasterizerSurfaceComponent();
};
