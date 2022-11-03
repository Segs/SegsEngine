#include "rasterizer_reflection_atlas_component.h"
#include "drivers/gles3/rasterizer_reflection_probe_component.h"
#include "rasterizer_scene_gles3.h"

RenderingEntity reflection_atlas_create() {
    RenderingEntity  res = VSG::ecs->create();
    VSG::ecs->registry.emplace<RasterizerReflectionAtlasComponent>(res);
    return res;
}

void RasterizerReflectionAtlasComponent::set_size(int p_size) {
    int n_size = next_power_of_2(p_size);

    if (size == n_size)
        return;


    if (size) {
        fbo.release();
        color.release();
    }

    size = n_size;

    for (int i = 0; i < reflections.size(); i++) {
        //erase probes reference to this
        if (reflections[i].owner!= entt::null) {
            auto *reflection_probe_instance = VSG::ecs->try_get<RasterizerReflectionProbeInstanceComponent>(reflections[i].owner);
            reflections[i].owner = entt::null;

            ERR_CONTINUE(!reflection_probe_instance);
            reflection_probe_instance->reflection_atlas_index = -1;
            reflection_probe_instance->atlas = entt::null;
            reflection_probe_instance->render_step = -1;
        }
    }

    if (size) {

        GLenum internal_format = GL_RGBA16F;
        GLenum format = GL_RGBA;
        GLenum type = GL_HALF_FLOAT;

             // Create a texture for storing the color
        glActiveTexture(GL_TEXTURE0);
        color.create();
        glBindTexture(GL_TEXTURE_2D, color);

        int mmsize = size;
        glTexStorage2DCustom(GL_TEXTURE_2D, 6, internal_format, mmsize, mmsize, format, type);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 5);

        fbo.create();
        for (int i = 0; i < 6; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, i);

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            ERR_CONTINUE(status != GL_FRAMEBUFFER_COMPLETE);

            glDisable(GL_SCISSOR_TEST);
            glViewport(0, 0, mmsize, mmsize);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT); //it needs to be cleared, to avoid generating garbage

            mmsize >>= 1;
        }
    }
}

void reflection_atlas_set_size(RenderingEntity p_ref_atlas, int p_size) {

    auto *reflection_atlas = VSG::ecs->registry.try_get<RasterizerReflectionAtlasComponent>(p_ref_atlas);
    ERR_FAIL_COND(!reflection_atlas);
    reflection_atlas->set_size(p_size);

}

void reflection_atlas_set_subdivision(RenderingEntity p_ref_atlas, int p_subdiv) {

    auto *reflection_atlas = VSG::ecs->registry.try_get<RasterizerReflectionAtlasComponent>(p_ref_atlas);
    ERR_FAIL_COND(!reflection_atlas);

    int subdiv = next_power_of_2(p_subdiv);
    if (subdiv & 0xaaaaaaaa) { //sqrt(subdiv) must be integer
        subdiv <<= 1;
    }

    subdiv = int(Math::sqrt((float)subdiv));

    if (reflection_atlas->subdiv == subdiv) {
        return;
    }

    if (subdiv) {

        for (int i = 0; i < reflection_atlas->reflections.size(); i++) {
            //erase probes reference to this
            if (reflection_atlas->reflections[i].owner!= entt::null) {
                auto *reflection_probe_instance = VSG::ecs->try_get<RasterizerReflectionProbeInstanceComponent>(reflection_atlas->reflections[i].owner);
                reflection_atlas->reflections[i].owner = entt::null;

                ERR_CONTINUE(!reflection_probe_instance);
                reflection_probe_instance->reflection_atlas_index = -1;
                reflection_probe_instance->atlas = entt::null;
                reflection_probe_instance->render_step = -1;
            }
        }
    }

    reflection_atlas->subdiv = subdiv;

    reflection_atlas->reflections.resize(subdiv * subdiv);
}

//! remove ourselves from all probes that reference us
void RasterizerReflectionAtlasComponent::unregister_from_reflection_probes()
{
    for (Reflection & refl : reflections) {
        // TODO: verify refl.owner==self ??
        if (refl.owner==entt::null) {
            continue;
        }
        auto *reflection_probe_instance = VSG::ecs->try_get<RasterizerReflectionProbeInstanceComponent>(refl.owner);
        refl.owner = entt::null;

        ERR_CONTINUE(!reflection_probe_instance);
        reflection_probe_instance->reflection_atlas_index = -1;
        reflection_probe_instance->atlas = entt::null;
        reflection_probe_instance->render_step = -1;
    }
    reflections.clear();
}

RasterizerReflectionAtlasComponent::~RasterizerReflectionAtlasComponent()
{
    unregister_from_reflection_probes();
}

