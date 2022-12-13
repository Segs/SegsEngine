#include "rasterizer_particle_component.h"

#include "rasterizer_material_component.h"
#include "rasterizer_shader_component.h"
#include "rasterizer_storage_gles3.h"
#include "rasterizer_dependent_entities_component.h"
#include "rasterizer_texture_component.h"
#include "core/engine.h"
#include "servers/rendering/render_entity_getter.h"

struct ParticlesDirty {
};

RasterizerParticlesComponent::RasterizerParticlesComponent() :
    custom_aabb(AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8))) {
    particle_buffers.create();
    particle_vaos.create();

}

RasterizerParticlesComponent::~RasterizerParticlesComponent() = default;

RenderingEntity RasterizerStorageGLES3::particles_create() {
    RenderingEntity res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerParticlesComponent>(res);
    VSG::ecs->registry.emplace<RasterizerInstantiableComponent>(res);
    return res;
}

void RasterizerStorageGLES3::particles_set_emitting(RenderingEntity p_particles, bool p_emitting) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->emitting = p_emitting;
}

bool RasterizerStorageGLES3::particles_get_emitting(RenderingEntity p_particles) {
    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, false);

    return particles->emitting;
}

void RasterizerStorageGLES3::particles_set_amount(RenderingEntity p_particles, int p_amount) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->amount = p_amount;

    int floats = p_amount * 24;
    float *data = memnew_arr(float, floats);

    for (int i = 0; i < floats; i++) {
        data[i] = 0;
    }

    for (int i = 0; i < 2; i++) {

        glBindVertexArray(particles->particle_vaos[i]);

        glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[i]);
        glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), data, GL_STATIC_DRAW);

        for (int j = 0; j < 6; j++) {
            glEnableVertexAttribArray(j);
            glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
        }
    }

    if (particles->histories_enabled) {

        for (int i = 0; i < 2; i++) {
            glBindVertexArray(particles->particle_vao_histories[i]);

            glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[i]);
            glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), data, GL_DYNAMIC_COPY);

            for (int j = 0; j < 6; j++) {
                glEnableVertexAttribArray(j);
                glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
            }
            particles->particle_valid_histories[i] = false;
        }
    }

    glBindVertexArray(0);

    particles->prev_ticks = 0;
    particles->phase = 0;
    particles->prev_phase = 0;
    particles->clear = true;

    memdelete_arr(data);
}

void RasterizerStorageGLES3::particles_set_lifetime(RenderingEntity p_particles, float p_lifetime) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    particles->lifetime = p_lifetime;
}

void RasterizerStorageGLES3::particles_set_one_shot(RenderingEntity p_particles, bool p_one_shot) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    particles->one_shot = p_one_shot;
}

void RasterizerStorageGLES3::particles_set_pre_process_time(RenderingEntity p_particles, float p_time) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    particles->pre_process_time = p_time;
}
void RasterizerStorageGLES3::particles_set_explosiveness_ratio(RenderingEntity p_particles, float p_ratio) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    particles->explosiveness = p_ratio;
}
void RasterizerStorageGLES3::particles_set_randomness_ratio(RenderingEntity p_particles, float p_ratio) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    particles->randomness = p_ratio;
}

static void _particles_update_histories(RasterizerParticlesComponent *particles) {

    bool needs_histories = particles->draw_order == RS::PARTICLES_DRAW_ORDER_VIEW_DEPTH;

    if (needs_histories == particles->histories_enabled) {
        return;
    }

    particles->histories_enabled = needs_histories;

    int floats = particles->amount * 24;

    if (!needs_histories) {

        particles->particle_buffer_histories.release();
        particles->particle_vao_histories.release();

    } else {

        particles->particle_buffer_histories.create();
        particles->particle_vao_histories.create();

        for (int i = 0; i < 2; i++) {
            glBindVertexArray(particles->particle_vao_histories[i]);

            glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffer_histories[i]);
            glBufferData(GL_ARRAY_BUFFER, floats * sizeof(float), nullptr, GL_DYNAMIC_COPY);

            for (int j = 0; j < 6; j++) {
                glEnableVertexAttribArray(j);
                glVertexAttribPointer(j, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4 * 6, CAST_INT_TO_UCHAR_PTR(j * 16));
            }

            particles->particle_valid_histories[i] = false;
        }
    }

    particles->clear = true;
}

void RasterizerStorageGLES3::particles_set_custom_aabb(RenderingEntity p_particles, const AABB &p_aabb) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    auto *deps = VSG::ecs->registry.try_get<RasterizerInstantiableComponent>(p_particles);

    ERR_FAIL_COND(!particles||!deps);
    particles->custom_aabb = p_aabb;
    _particles_update_histories(particles);
    deps->instance_change_notify(true, false);
}

void RasterizerStorageGLES3::particles_set_speed_scale(RenderingEntity p_particles, float p_scale) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->speed_scale = p_scale;
}
void RasterizerStorageGLES3::particles_set_use_local_coordinates(RenderingEntity p_particles, bool p_enable) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->use_local_coords = p_enable;
}

void RasterizerStorageGLES3::particles_set_fixed_fps(RenderingEntity p_particles, int p_fps) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->fixed_fps = p_fps;
}

void RasterizerStorageGLES3::particles_set_fractional_delta(RenderingEntity p_particles, bool p_enable) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->fractional_delta = p_enable;
}

void RasterizerStorageGLES3::particles_set_process_material(RenderingEntity p_particles, RenderingEntity p_material) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->process_material = p_material;
}

void RasterizerStorageGLES3::particles_set_draw_order(RenderingEntity p_particles, RS::ParticlesDrawOrder p_order) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->draw_order = p_order;
    _particles_update_histories(particles);
}

void RasterizerStorageGLES3::particles_set_draw_passes(RenderingEntity p_particles, int p_passes) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->draw_passes.resize(p_passes);
}

void RasterizerStorageGLES3::particles_set_draw_pass_mesh(RenderingEntity p_particles, int p_pass, RenderingEntity p_mesh) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);
    ERR_FAIL_INDEX(p_pass, particles->draw_passes.size());
    particles->draw_passes[p_pass] = p_mesh;
}

void RasterizerStorageGLES3::particles_restart(RenderingEntity p_particles) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->restart_request = true;
}

void RasterizerStorageGLES3::particles_request_process(RenderingEntity p_particles) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    VSG::ecs->registry.emplace_or_replace<ParticlesDirty>(p_particles);
}

AABB RasterizerStorageGLES3::particles_get_current_aabb(RenderingEntity p_particles) {

    const auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, AABB());

    const float *data;
    glBindBuffer(GL_ARRAY_BUFFER, particles->particle_buffers[0]);


    PoolVector<uint8_t> vector;
    vector.resize(particles->amount * 16 * 6);
    {
        PoolVector<uint8_t>::Write w = vector.write();
        glGetBufferSubData(GL_ARRAY_BUFFER, 0, particles->amount * 16 * 6, w.ptr());
    }
    PoolVector<uint8_t>::Read r = vector.read();
    data = reinterpret_cast<const float *>(r.ptr());

    AABB aabb;

    Transform inv = particles->emission_transform.affine_inverse();

    for (int i = 0; i < particles->amount; i++) {
        int ofs = i * 24;
        Vector3 pos = Vector3(data[ofs + 15], data[ofs + 19], data[ofs + 23]);
        if (!particles->use_local_coords) {
            pos = inv.xform(pos);
        }
        if (i == 0) {
            aabb.position = pos;
        } else {
            aabb.expand_to(pos);
        }
    }

    r.release();
    vector = PoolVector<uint8_t>();

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    float longest_axis = 0;
    for (int i = 0; i < particles->draw_passes.size(); i++) {
        if (particles->draw_passes[i]!=entt::null) {
            AABB maabb = mesh_get_aabb(particles->draw_passes[i], entt::null);
            longest_axis = M_MAX(maabb.get_longest_axis_size(), longest_axis);
        }
    }

    aabb.grow_by(longest_axis);

    return aabb;
}

AABB RasterizerStorageGLES3::particles_get_aabb(RenderingEntity p_particles) const {

    const auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, AABB());

    return particles->custom_aabb;
}

void RasterizerStorageGLES3::particles_set_emission_transform(RenderingEntity p_particles, const Transform &p_transform) {

    auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND(!particles);

    particles->emission_transform = p_transform;
}

int RasterizerStorageGLES3::particles_get_draw_passes(RenderingEntity p_particles) const {

    const auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, 0);

    return particles->draw_passes.size();
}

RenderingEntity RasterizerStorageGLES3::particles_get_draw_pass_mesh(RenderingEntity p_particles, int p_pass) const {

    const auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, entt::null);
    ERR_FAIL_INDEX_V(p_pass, particles->draw_passes.size(), entt::null);

    return particles->draw_passes[p_pass];
}

static void _particles_process(RasterizerGLES3ShadersStorage &shaders,RasterizerParticlesComponent *p_particles, float p_delta) {

    float new_phase = Math::fmod((float)p_particles->phase + (p_delta / p_particles->lifetime) * p_particles->speed_scale, 1.0f);

    if (p_particles->clear) {
        p_particles->cycle_number = 0;
        p_particles->random_seed = Math::rand();
    } else if (new_phase < p_particles->phase) {
        if (p_particles->one_shot) {
            p_particles->emitting = false;
            shaders.particles.set_uniform(ParticlesShaderGLES3::EMITTING, false);
        }
        p_particles->cycle_number++;
    }

    shaders.particles.set_uniform(ParticlesShaderGLES3::SYSTEM_PHASE, new_phase);
    shaders.particles.set_uniform(ParticlesShaderGLES3::PREV_SYSTEM_PHASE, p_particles->phase);
    p_particles->phase = new_phase;

    shaders.particles.set_uniform(ParticlesShaderGLES3::DELTA, p_delta * p_particles->speed_scale);
    shaders.particles.set_uniform(ParticlesShaderGLES3::CLEAR, p_particles->clear);
    glUniform1ui(shaders.particles.get_uniform_location(ParticlesShaderGLES3::RANDOM_SEED), p_particles->random_seed);

    if (p_particles->use_local_coords) {
        shaders.particles.set_uniform(ParticlesShaderGLES3::EMISSION_TRANSFORM, Transform());
    } else {
        shaders.particles.set_uniform(ParticlesShaderGLES3::EMISSION_TRANSFORM, p_particles->emission_transform);
    }

    glUniform1ui(shaders.particles.get_uniform(ParticlesShaderGLES3::CYCLE), p_particles->cycle_number);

    p_particles->clear = false;

    glBindVertexArray(p_particles->particle_vaos[0]);

    glBindBuffer(GL_ARRAY_BUFFER, 0); // ensure this is unbound per WebGL2 spec
    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, p_particles->particle_buffers[1]);

         //        GLint size = 0;
         //        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);

    glBeginTransformFeedback(GL_POINTS);
    glDrawArrays(GL_POINTS, 0, p_particles->amount);
    glEndTransformFeedback();

    SWAP(p_particles->particle_buffers[0], p_particles->particle_buffers[1]);
    SWAP(p_particles->particle_vaos[0], p_particles->particle_vaos[1]);

    glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    glBindVertexArray(0);
#if 0
    // /* //debug particles :D
    glBindBuffer(GL_ARRAY_BUFFER, p_particles->particle_buffers[0]);

    float *data = (float *)glMapBufferRange(GL_ARRAY_BUFFER, 0, p_particles->amount * 16 * 6, GL_MAP_READ_BIT);
    for (int i = 0; i < p_particles->amount; i++) {
        int ofs = i * 24;
        print_line(itos(i) + ":");
        print_line("\tColor: " + (String)Color(data[ofs + 0], data[ofs + 1], data[ofs + 2], data[ofs + 3]));
        print_line("\tVelocity: " + (String)Vector3(data[ofs + 4], data[ofs + 5], data[ofs + 6]));
        print_line("\tActive: " + itos(data[ofs + 7]));
        print_line("\tCustom: " + (String)Color(data[ofs + 8], data[ofs + 9], data[ofs + 10], data[ofs + 11]));
        print_line("\tXF X: " + (String)Color(data[ofs + 12], data[ofs + 13], data[ofs + 14], data[ofs + 15]));
        print_line("\tXF Y: " + (String)Color(data[ofs + 16], data[ofs + 17], data[ofs + 18], data[ofs + 19]));
        print_line("\tXF Z: " + (String)Color(data[ofs + 20], data[ofs + 21], data[ofs + 22], data[ofs + 23]));
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif
    //*/
}

void RasterizerStorageGLES3::update_particles() {

    glEnable(GL_RASTERIZER_DISCARD);
    auto dirty_particles_group = VSG::ecs -> registry.group<ParticlesDirty>(entt::get<RasterizerParticlesComponent,RasterizerInstantiableComponent>);
    auto ts=Engine::get_singleton()->get_time_scale();

    dirty_particles_group.each([=](auto ent,RasterizerParticlesComponent &particles,RasterizerInstantiableComponent &deps) {
        //use transform feedback to process particles

        if (particles.restart_request) {
            particles.prev_ticks = 0;
            particles.phase = 0;
            particles.prev_phase = 0;
            particles.clear = true;
            particles.particle_valid_histories[0] = false;
            particles.particle_valid_histories[1] = false;
            particles.restart_request = false;
        }

        if (particles.inactive && !particles.emitting) {
            return;
        }

        if (particles.emitting) {
            if (particles.inactive) {
                //restart system from scratch
                particles.prev_ticks = 0;
                particles.phase = 0;
                particles.prev_phase = 0;
                particles.clear = true;
                particles.particle_valid_histories[0] = false;
                particles.particle_valid_histories[1] = false;
            }
            particles.inactive = false;
            particles.inactive_time = 0;
        } else {
            particles.inactive_time += particles.speed_scale * frame.delta;
            if (particles.inactive_time > particles.lifetime * 1.2) {
                particles.inactive = true;
                return;
            }
        }

        auto *material = get<RasterizerMaterialComponent>(particles.process_material);
        auto *shader = material ? get<RasterizerShaderComponent>(material->shader) : nullptr;
        if (!material || !shader || shader->mode != RS::ShaderMode::PARTICLES) {

            shaders.particles.set_custom_shader(0);
        } else {
            shaders.particles.set_custom_shader(shader->custom_code_id);

            if (material->ubo_id) {

                glBindBufferBase(GL_UNIFORM_BUFFER, 0, material->ubo_id);
            }

            int tc = material->textures.size();
            RenderingEntity *textures = material->textures.data();
            ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader->texture_hints.data();

            for (int i = 0; i < tc; i++) {

                glActiveTexture(GL_TEXTURE0 + i);

                GLenum target;
                GLuint tex;

                RasterizerTextureComponent *t = get<RasterizerTextureComponent>(textures[i]);

                if (!t) {
                    //check hints
                    target = GL_TEXTURE_2D;

                    switch (texture_hints[i]) {
                        case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
                        case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
                            tex = resources.black_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_TRANSPARENT: {
                            tex = resources.transparent_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
                            tex = resources.aniso_tex;
                        } break;
                        case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
                            tex = resources.normal_tex;
                        } break;
                        default: {
                            tex = resources.white_tex;
                        } break;
                    }
                } else {

                    t = t->get_ptr(); //resolve for proxies
                    target = t->target;
                    tex = t->get_texture_id();
                }

                glBindTexture(target, tex);
            }
        }

        shaders.particles.set_conditional(ParticlesShaderGLES3::USE_FRACTIONAL_DELTA, particles.fractional_delta);

        shaders.particles.bind();

        shaders.particles.set_uniform(ParticlesShaderGLES3::TOTAL_PARTICLES, particles.amount);
        shaders.particles.set_uniform(ParticlesShaderGLES3::TIME, frame.time[0]);
        shaders.particles.set_uniform(ParticlesShaderGLES3::EXPLOSIVENESS, particles.explosiveness);
        shaders.particles.set_uniform(ParticlesShaderGLES3::LIFETIME, particles.lifetime);
        shaders.particles.set_uniform(ParticlesShaderGLES3::ATTRACTOR_COUNT, 0);
        shaders.particles.set_uniform(ParticlesShaderGLES3::EMITTING, particles.emitting);
        shaders.particles.set_uniform(ParticlesShaderGLES3::RANDOMNESS, particles.randomness);

        bool zero_time_scale = ts <= 0.0f;

        if (particles.clear && particles.pre_process_time > 0.0f) {

            float frame_time;
            const float fps_divisor = particles.fixed_fps>0 ? particles.fixed_fps : 30.0f;
            frame_time = 1.0f / fps_divisor;

            float todo = particles.pre_process_time;

            while (todo >= 0) {
                _particles_process(shaders,&particles, frame_time);
                todo -= frame_time;
            }
        }

        if (particles.fixed_fps > 0) {
            float frame_time;
            float decr;
            if (zero_time_scale) {
                frame_time = 0.0;
                decr = 1.0f / particles.fixed_fps;
            } else {
                frame_time = 1.0f / particles.fixed_fps;
                decr = frame_time;
            }
            float delta = frame.delta;
            if (delta > 0.1f) { //avoid recursive stalls if fps goes below 10
                delta = 0.1f;
            } else if (delta <= 0.0f) { //unlikely but..
                delta = 0.001f;
            }
            float todo = particles.frame_remainder + delta;

            while (todo >= frame_time) {
                _particles_process(shaders,&particles, frame_time);
                todo -= decr;
            }

            particles.frame_remainder = todo;

        } else {
            _particles_process(shaders, &particles, zero_time_scale ? 0.0 : frame.delta);
        }

        if (particles.histories_enabled) {

            SWAP(particles.particle_buffer_histories[0], particles.particle_buffer_histories[1]);
            SWAP(particles.particle_vao_histories[0], particles.particle_vao_histories[1]);
            SWAP(particles.particle_valid_histories[0], particles.particle_valid_histories[1]);

                 //copy
            glBindBuffer(GL_COPY_READ_BUFFER, particles.particle_buffers[0]);
            glBindBuffer(GL_COPY_WRITE_BUFFER, particles.particle_buffer_histories[0]);
            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, particles.amount * 24 * sizeof(float));

            particles.particle_valid_histories[0] = true;
        }

        deps.instance_change_notify(true, false); //make sure shadows are updated
    });

    glDisable(GL_RASTERIZER_DISCARD);
    VSG::ecs->registry.clear<ParticlesDirty>();
}

bool RasterizerStorageGLES3::particles_is_inactive(RenderingEntity p_particles) const {

    const auto *particles = VSG::ecs->try_get<RasterizerParticlesComponent>(p_particles);
    ERR_FAIL_COND_V(!particles, false);
    return !particles->emitting && particles->inactive;
}

