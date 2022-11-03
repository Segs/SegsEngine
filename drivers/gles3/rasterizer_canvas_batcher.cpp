#include "rasterizer_canvas_batcher.h"

#include "core/ecs_registry.h"
#include "servers/rendering/rendering_server_globals.h"


RasterizerTextureComponent *RasterizerCanvasBatcherBaseClass::_get_canvas_texture(RenderingEntity p_texture) {
    if (p_texture==entt::null) {
        return nullptr;
    }
        RasterizerTextureComponent*texture = get<RasterizerTextureComponent>(p_texture);
    assert(texture);
        if(!texture) {
            return nullptr;
        }
        // could be a proxy texture (e.g. animated)
        if (texture->proxy!=entt::null) {
            // take care to prevent infinite loop
            int count = 0;
            while (texture->proxy!=entt::null) {
                texture = getUnchecked<RasterizerTextureComponent>(texture->proxy);
                count++;
                ERR_FAIL_COND_V_MSG(count == 16, nullptr, "Texture proxy infinite loop detected.");
            }
        }

        return texture->get_ptr();
    }
void RasterizerCanvasBatcherBaseClass::batch_canvas_begin() {
    // diagnose_frame?
    bdata.frame_string = ""; // just in case, always set this as we don't want a string leak in release...
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
    if (bdata.settings_diagnose_frame) {
        bdata.diagnose_frame = false;

        uint32_t tick = OS::get_singleton()->get_ticks_msec();
        uint64_t frame = Engine::get_singleton()->get_frames_drawn();

        if (tick >= bdata.next_diagnose_tick) {
            bdata.next_diagnose_tick = tick + 10000;

            // the plus one is prevent starting diagnosis half way through frame
            bdata.diagnose_frame_number = frame + 1;
        }

        if (frame == bdata.diagnose_frame_number) {
            bdata.diagnose_frame = true;
            bdata.reset_stats();
        }

        if (bdata.diagnose_frame) {
            bdata.frame_string = "canvas_begin FRAME " + itos(frame) + "\n";
        }
    }
#endif
}

void RasterizerCanvasBatcherBaseClass::batch_canvas_end() {
#if defined(TOOLS_ENABLED) && defined(DEBUG_ENABLED)
    if (bdata.diagnose_frame) {
        bdata.frame_string += "canvas_end\n";
        if (bdata.stats_items_sorted) {
            bdata.frame_string += "\titems reordered: " + itos(bdata.stats_items_sorted) + "\n";
        }
        if (bdata.stats_light_items_joined) {
            bdata.frame_string += "\tlight items joined: " + itos(bdata.stats_light_items_joined) + "\n";
        }

        print_line(bdata.frame_string);
    }
#endif
}

void RasterizerCanvasBatcherBaseClass::batch_constructor() {
    bdata.settings_use_batching = false;

    use_nvidia_rect_workaround = T_GLOBAL_GET<bool>("rendering/2d/options/use_nvidia_rect_flicker_workaround");
}

RasterizerCanvasBatcherBaseClass::BatchData::BatchData() {
    reset_flush();
    reset_joined_item();

    gl_vertex_buffer = 0;
    gl_index_buffer = 0;
    max_quads = 0;
    vertex_buffer_size_units = 0;
    vertex_buffer_size_bytes = 0;
    index_buffer_size_units = 0;
    index_buffer_size_bytes = 0;

    use_colored_vertices = false;

    settings_use_batching = false;
    settings_max_join_item_commands = 0;
    settings_colored_vertex_format_threshold = 0.0f;
    settings_batch_buffer_num_verts = 0;
    scissor_threshold_area = 0.0f;
    joined_item_batch_flags = 0;
    diagnose_frame = false;
    next_diagnose_tick = 10000;
    diagnose_frame_number = 9999999999; // some high number
    join_across_z_indices = true;
    settings_item_reordering_lookahead = 0;

    settings_use_batching_original_choice = false;
    settings_flash_batching = false;
    settings_diagnose_frame = false;
    settings_scissor_lights = false;
    settings_scissor_threshold = -1.0f;
    settings_use_single_rect_fallback = false;
    settings_use_software_skinning = true;
    settings_ninepatch_mode = 0; // default
    settings_light_max_join_items = 16;

    settings_uv_contract = false;
    settings_uv_contract_amount = 0.0f;

    buffer_mode_batch_upload_send_null = true;
    buffer_mode_batch_upload_flag_stream = false;

    stats_items_sorted = 0;
    stats_light_items_joined = 0;
}

void RasterizerCanvasBatcherBaseClass::BatchData::reset_flush() {
    batches.reset();
    batch_textures.clear();

    vertices.reset();
    light_angles.reset();
    vertex_colors.reset();
    vertex_modulates.reset();
    vertex_transforms.reset();

    total_quads = 0;
    total_verts = 0;
    total_color_changes = 0;

    use_light_angles = false;
    use_modulate = false;
    use_large_verts = false;
    fvf = RasterizerStorageCommon::FVF_REGULAR;
}

String RasterizerCanvasBatcherBaseClass::BatchColor::to_string() const {
    String sz = "{";
    const float *data = get_data();
    for (int c = 0; c < 4; c++) {
        float f = data[c];
        int val = int((f * 255.0f) + 0.5f);
        sz += String(Variant(val)) + " ";
    }
    sz += "}";
    return sz;
}
