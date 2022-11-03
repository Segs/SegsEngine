#pragma once

#include "servers/rendering_server_enums.h"
#include "servers/rendering/render_entity_helpers.h"
#include "drivers/gles3/rasterizer_gl_unique_handle.h"

#include "core/engine_entities.h"
#include "core/math/aabb.h"
#include "core/math/transform.h"


struct RasterizerParticlesComponent  {

    Vector<RenderingEntity> draw_passes;
    MoveOnlyEntityHandle process_material;
    AABB custom_aabb;
    Transform emission_transform;

    float inactive_time=0.0f;
    int amount=0;
    float lifetime=1.0f;
    float pre_process_time=0.0f;
    float explosiveness=0.0f;
    float randomness=0.0f;
    float phase;
    float prev_phase;
    uint64_t prev_ticks=0;
    uint32_t random_seed=0;

    uint32_t cycle_number=0;

    float speed_scale=1.0f;

    int fixed_fps=0;
    float frame_remainder=0;

    RS::ParticlesDrawOrder draw_order=RS::PARTICLES_DRAW_ORDER_INDEX;


    GLMultiBufferHandle<2> particle_buffers;
    GLMultiVAOHandle<2> particle_vaos;

    GLMultiBufferHandle<2> particle_buffer_histories;
    GLMultiVAOHandle<2> particle_vao_histories;
    bool particle_valid_histories[2];
    bool histories_enabled = false;

    bool inactive = true;
    bool emitting = false;
    bool one_shot = false;
    bool restart_request = false;
    bool use_local_coords = true;
    bool fractional_delta = false;
    bool clear = true;

    RasterizerParticlesComponent(const RasterizerParticlesComponent&) = delete;
    RasterizerParticlesComponent &operator=(const RasterizerParticlesComponent&) = delete;

    RasterizerParticlesComponent(RasterizerParticlesComponent &&) = default;
    RasterizerParticlesComponent& operator=(RasterizerParticlesComponent&&) = default;

    RasterizerParticlesComponent();
    ~RasterizerParticlesComponent();
};
