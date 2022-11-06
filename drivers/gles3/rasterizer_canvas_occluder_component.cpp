#include "rasterizer_canvas_occluder_component.h"
#include "rasterizer_storage_gles3.h"

/* LIGHT SHADOW MAPPING */

RenderingEntity RasterizerStorageGLES3::canvas_light_occluder_create() {
    auto res = VSG::ecs->create();
    RasterizerCanvasOccluderComponent &co(VSG::ecs->registry.emplace<RasterizerCanvasOccluderComponent>(res));
    co.len = 0;
    co.array_id.create();

    return res;
}

void RasterizerStorageGLES3::canvas_light_occluder_set_polylines(RenderingEntity p_occluder, Span<const Vector2> p_lines) {

    ERR_FAIL_COND(!VSG::ecs->registry.any_of<RasterizerCanvasOccluderComponent>(p_occluder));
    RasterizerCanvasOccluderComponent &co = VSG::ecs->registry.get<RasterizerCanvasOccluderComponent>(p_occluder);

    co.lines.assign(p_lines.begin(),p_lines.end());

    if (p_lines.size() != co.len) {

        co.index_id.release();
        co.vertex_id.release();
        co.len = 0;
    }

    if (p_lines.size()) {

        PoolVector<float> geometry;
        PoolVector<uint16_t> indices;
        int lc = p_lines.size();

        geometry.resize(lc * 6);
        indices.resize(lc * 3);

        PoolVector<float>::Write vw = geometry.write();
        PoolVector<uint16_t>::Write iw = indices.write();

        const int POLY_HEIGHT = 16384;

        for (int i = 0; i < lc / 2; i++) {

            vw[i * 12 + 0] = p_lines[i * 2 + 0].x;
            vw[i * 12 + 1] = p_lines[i * 2 + 0].y;
            vw[i * 12 + 2] = POLY_HEIGHT;

            vw[i * 12 + 3] = p_lines[i * 2 + 1].x;
            vw[i * 12 + 4] = p_lines[i * 2 + 1].y;
            vw[i * 12 + 5] = POLY_HEIGHT;

            vw[i * 12 + 6] = p_lines[i * 2 + 1].x;
            vw[i * 12 + 7] = p_lines[i * 2 + 1].y;
            vw[i * 12 + 8] = -POLY_HEIGHT;

            vw[i * 12 + 9] = p_lines[i * 2 + 0].x;
            vw[i * 12 + 10] = p_lines[i * 2 + 0].y;
            vw[i * 12 + 11] = -POLY_HEIGHT;

            iw[i * 6 + 0] = i * 4 + 0;
            iw[i * 6 + 1] = i * 4 + 1;
            iw[i * 6 + 2] = i * 4 + 2;

            iw[i * 6 + 3] = i * 4 + 2;
            iw[i * 6 + 4] = i * 4 + 3;
            iw[i * 6 + 5] = i * 4 + 0;
        }

             //if same buffer len is being set, just use BufferSubData to avoid a pipeline flush

        if (!co.vertex_id.is_initialized()) {
            co.vertex_id.create();
            glBindBuffer(GL_ARRAY_BUFFER, co.vertex_id);
            glBufferData(GL_ARRAY_BUFFER, lc * 6 * sizeof(real_t), vw.ptr(), GL_STATIC_DRAW);
        } else {

            glBindBuffer(GL_ARRAY_BUFFER, co.vertex_id);
            glBufferSubData(GL_ARRAY_BUFFER, 0, lc * 6 * sizeof(real_t), vw.ptr());
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

        if (!co.index_id.is_initialized()) {

            co.index_id.create();
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co.index_id);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, lc * 3 * sizeof(uint16_t), iw.ptr(), GL_DYNAMIC_DRAW);
        } else {

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co.index_id);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, lc * 3 * sizeof(uint16_t), iw.ptr());
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); //unbind

        co.len = lc;
        glBindVertexArray(co.array_id);
        glBindBuffer(GL_ARRAY_BUFFER, co.vertex_id);
        glEnableVertexAttribArray(RS::ARRAY_VERTEX);
        glVertexAttribPointer(RS::ARRAY_VERTEX, 3, GL_FLOAT, false, 0, nullptr);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, co.index_id);
        glBindVertexArray(0);
    }

}
