#include "rasterizer_shadow_atlas_component.h"

#include "core/os/os.h"
#include "drivers/gles3/rasterizer_light_instance_component.h"
#include "rasterizer_scene_gles3.h"
#include "servers/rendering/render_entity_getter.h"
#include "core/project_settings.h"

bool _shadow_atlas_find_shadow(RasterizerShadowAtlasComponent *shadow_atlas, int *p_in_quadrants, int p_quadrant_count,
        int p_current_subdiv, uint64_t p_tick, int &r_quadrant, int &r_shadow, uint64_t scene_pass,
        uint64_t shadow_atlas_realloc_tolerance_msec) {
    for (int i = p_quadrant_count - 1; i >= 0; i--) {

        int qidx = p_in_quadrants[i];

        if (shadow_atlas->quadrants[qidx].subdivision == (uint32_t)p_current_subdiv) {
            return false;
        }

             //look for an empty space
        int sc = shadow_atlas->quadrants[qidx].shadows.size();
        RasterizerShadowAtlasComponent::Quadrant::Shadow *sarr = shadow_atlas->quadrants[qidx].shadows.data();

        int found_free_idx = -1; //found a free one
        int found_used_idx = -1; //found existing one, must steal it
        uint64_t min_pass = 0; // pass of the existing one, try to use the least recently used one (LRU fashion)

        for (int j = 0; j < sc; j++) {
            if (sarr[j].owner==entt::null) {
                found_free_idx = j;
                break;
            }

            auto *sli = get<RasterizerLightInstanceComponent>(sarr[j].owner);
            ERR_CONTINUE(!sli);

            if (sli->last_scene_pass != scene_pass) {

                     //was just allocated, don't kill it so soon, wait a bit..
                if (p_tick - sarr[j].alloc_tick < shadow_atlas_realloc_tolerance_msec) {
                    continue;
                }

                if (found_used_idx == -1 || sli->last_scene_pass < min_pass) {
                    found_used_idx = j;
                    min_pass = sli->last_scene_pass;
                }
            }
        }

        if (found_free_idx == -1 && found_used_idx == -1) {
            continue; //nothing found
        }

        if (found_free_idx == -1 && found_used_idx != -1) {
            found_free_idx = found_used_idx;
        }

        r_quadrant = qidx;
        r_shadow = found_free_idx;

        return true;
    }

    return false;
}

/* SHADOW ATLAS API */

RenderingEntity RasterizerSceneGLES3::shadow_atlas_create() {
    auto res = VSG::ecs->create();
    auto & shadow_atlas(VSG::ecs->registry.emplace<RasterizerShadowAtlasComponent>(res));
    shadow_atlas.self = res;
    shadow_atlas.size = 0;
    shadow_atlas.smallest_subdiv = 0;

    for (int i = 0; i < 4; i++) {
        shadow_atlas.size_order[i] = i;
    }

    return res;
}

void RasterizerSceneGLES3::shadow_atlas_set_size(RenderingEntity p_atlas, int p_size) {

    auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_atlas);
    ERR_FAIL_COND(!shadow_atlas);
    shadow_atlas->set_size(p_atlas,p_size);
}

void RasterizerSceneGLES3::shadow_atlas_set_quadrant_subdivision(RenderingEntity p_atlas, int p_quadrant, int p_subdivision) {

    auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_atlas);
    ERR_FAIL_COND(!shadow_atlas);
    ERR_FAIL_INDEX(p_quadrant, 4);
    ERR_FAIL_INDEX(p_subdivision, 16384);

    uint32_t subdiv = next_power_of_2(p_subdivision);
    if (subdiv & 0xaaaaaaaa) { //sqrt(subdiv) must be integer
        subdiv <<= 1;
    }

    subdiv = int(Math::sqrt((float)subdiv));

         //obtain the number that will be x*x

    if (shadow_atlas->quadrants[p_quadrant].subdivision == subdiv) {
        return;
    }

         //erase all data from quadrant
    for (int i = 0; i < shadow_atlas->quadrants[p_quadrant].shadows.size(); i++) {

        if (shadow_atlas->quadrants[p_quadrant].shadows[i].owner!=entt::null) {
            shadow_atlas->shadow_owners.erase(shadow_atlas->quadrants[p_quadrant].shadows[i].owner);
            auto *li = get<RasterizerLightInstanceShadowAtlasesComponent>(shadow_atlas->quadrants[p_quadrant].shadows[i].owner);
            ERR_CONTINUE(!li);
            li->shadow_atlases.erase(p_atlas);
        }
    }

    shadow_atlas->quadrants[p_quadrant].shadows.resize(0);
    shadow_atlas->quadrants[p_quadrant].shadows.resize(subdiv * subdiv);
    shadow_atlas->quadrants[p_quadrant].subdivision = subdiv;

         //cache the smallest subdiv (for faster allocation in light update)

    shadow_atlas->smallest_subdiv = 1 << 30;

    for (int i = 0; i < 4; i++) {
        if (shadow_atlas->quadrants[i].subdivision) {
            shadow_atlas->smallest_subdiv = MIN(shadow_atlas->smallest_subdiv, shadow_atlas->quadrants[i].subdivision);
        }
    }

    if (shadow_atlas->smallest_subdiv == 1 << 30) {
        shadow_atlas->smallest_subdiv = 0;
    }

         //resort the size orders, simple bublesort for 4 elements..

    int swaps = 0;
    do {
        swaps = 0;

        for (int i = 0; i < 3; i++) {
            if (shadow_atlas->quadrants[shadow_atlas->size_order[i]].subdivision < shadow_atlas->quadrants[shadow_atlas->size_order[i + 1]].subdivision) {
                SWAP(shadow_atlas->size_order[i], shadow_atlas->size_order[i + 1]);
                swaps++;
            }
        }
    } while (swaps > 0);
}


bool RasterizerSceneGLES3::shadow_atlas_update_light(RenderingEntity p_atlas, RenderingEntity p_light_intance, float p_coverage, uint64_t p_light_version) {

    auto *shadow_atlas = get<RasterizerShadowAtlasComponent>(p_atlas);
    ERR_FAIL_COND_V(!shadow_atlas, false);

    auto *li = get<RasterizerLightInstanceShadowAtlasesComponent>(p_light_intance);
    ERR_FAIL_COND_V(!li, false);

    if (shadow_atlas->size == 0 || shadow_atlas->smallest_subdiv == 0) {
        return false;
    }

    uint32_t quad_size = shadow_atlas->size >> 1;
    int desired_fit = MIN(quad_size / shadow_atlas->smallest_subdiv, next_power_of_2(quad_size * p_coverage));

    int valid_quadrants[4];
    int valid_quadrant_count = 0;
    int best_size = -1; //best size found
    int best_subdiv = -1; //subdiv for the best size

         //find the quadrants this fits into, and the best possible size it can fit into
    for (int i = 0; i < 4; i++) {
        int q = shadow_atlas->size_order[i];
        int sd = shadow_atlas->quadrants[q].subdivision;
        if (sd == 0) {
            continue; //unused
        }

        int max_fit = quad_size / sd;

        if (best_size != -1 && max_fit > best_size) {
            break; //too large
        }

        valid_quadrants[valid_quadrant_count++] = q;
        best_subdiv = sd;

        if (max_fit >= desired_fit) {
            best_size = max_fit;
        }
    }

    ERR_FAIL_COND_V(valid_quadrant_count == 0, false);

    uint64_t tick = OS::get_singleton()->get_ticks_msec();

         //see if it already exists

    if (shadow_atlas->shadow_owners.contains(p_light_intance)) {
        //it does!
        uint32_t key = shadow_atlas->shadow_owners[p_light_intance];
        uint32_t q = (key >> RasterizerShadowAtlasComponent::QUADRANT_SHIFT) & 0x3;
        uint32_t s = key & RasterizerShadowAtlasComponent::SHADOW_INDEX_MASK;

        bool should_realloc = shadow_atlas->quadrants[q].subdivision != (uint32_t)best_subdiv && (shadow_atlas->quadrants[q].shadows[s].alloc_tick - tick > shadow_atlas_realloc_tolerance_msec);
        bool should_redraw = shadow_atlas->quadrants[q].shadows[s].version != p_light_version;

        if (!should_realloc) {
            shadow_atlas->quadrants[q].shadows[s].version = p_light_version;
            //already existing, see if it should redraw or it's just OK
            return should_redraw;
        }

        int new_quadrant, new_shadow;

             //find a better place
        if (_shadow_atlas_find_shadow(shadow_atlas, valid_quadrants, valid_quadrant_count,
                    shadow_atlas->quadrants[q].subdivision, tick, new_quadrant, new_shadow, scene_pass,
                    shadow_atlas_realloc_tolerance_msec)) {
            // found a better place!
            auto *sh = &shadow_atlas->quadrants[new_quadrant].shadows[new_shadow];
            if (sh->owner!=entt::null) {
                //is taken, but is invalid, erasing it
                shadow_atlas->shadow_owners.erase(sh->owner);
                auto *sli = get<RasterizerLightInstanceShadowAtlasesComponent>(sh->owner);
                sli->shadow_atlases.erase(p_atlas);
            }

                 //erase previous
            shadow_atlas->quadrants[q].shadows[s].version = 0;
            shadow_atlas->quadrants[q].shadows[s].owner = entt::null;

            sh->owner = p_light_intance;
            sh->alloc_tick = tick;
            sh->version = p_light_version;
            li->shadow_atlases.insert(p_atlas);

                 //make new key
            key = new_quadrant << RasterizerShadowAtlasComponent::QUADRANT_SHIFT;
            key |= new_shadow;
            //update it in map
            shadow_atlas->shadow_owners[p_light_intance] = key;
            //make it dirty, as it should redraw anyway
            return true;
        }

             //no better place for this shadow found, keep current

             //already existing, see if it should redraw or it's just OK

        shadow_atlas->quadrants[q].shadows[s].version = p_light_version;

        return should_redraw;
    }

    int new_quadrant, new_shadow;

         //find a better place
    if (_shadow_atlas_find_shadow(shadow_atlas, valid_quadrants, valid_quadrant_count, -1, tick, new_quadrant,
                new_shadow, scene_pass, shadow_atlas_realloc_tolerance_msec)) {
        // found a better place!
        RasterizerShadowAtlasComponent::Quadrant::Shadow *sh = &shadow_atlas->quadrants[new_quadrant].shadows[new_shadow];
        if (sh->owner!=entt::null) {
            //is taken, but is invalid, erasing it
            shadow_atlas->shadow_owners.erase(sh->owner);
            auto *sli = get<RasterizerLightInstanceShadowAtlasesComponent>(sh->owner);
            sli->shadow_atlases.erase(p_atlas);
        }

        sh->owner = p_light_intance;
        sh->alloc_tick = tick;
        sh->version = p_light_version;
        li->shadow_atlases.insert(p_atlas);

             //make new key
        uint32_t key = new_quadrant << RasterizerShadowAtlasComponent::QUADRANT_SHIFT;
        key |= new_shadow;
        //update it in map
        shadow_atlas->shadow_owners[p_light_intance] = key;
        //make it dirty, as it should redraw anyway

        return true;
    }

         //no place to allocate this light, apologies

    return false;
}
void RasterizerShadowAtlasComponent::set_size(RenderingEntity self, int p_size) {

    ERR_FAIL_COND(p_size < 0);

    p_size = next_power_of_2(p_size);

    if (p_size == size) {
        return;
    }

    // erasing atlas
    depth.release();
    fbo.release();
    for (int i = 0; i < 4; i++) {
        //clear subdivisions
        quadrants[i].shadows.resize(0);
        quadrants[i].shadows.resize(1ULL << quadrants[i].subdivision);
    }

         //erase shadow atlas reference from lights
    for (eastl::pair<const RenderingEntity,uint32_t> &E : shadow_owners) {
        auto *li = VSG::ecs->try_get<RasterizerLightInstanceShadowAtlasesComponent>(E.first);
        ERR_CONTINUE(!li);
        li->shadow_atlases.erase(self);
    }

         //clear owners
    shadow_owners.clear();

    size = p_size;

    if (size) {
        fbo.create();
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

             // Create a texture for storing the depth
        glActiveTexture(GL_TEXTURE0);
        depth.create();
        glBindTexture(GL_TEXTURE_2D, depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D, depth, 0);

        glViewport(0, 0, size, size);
        glClearDepth(0.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void RasterizerSceneGLES3::set_directional_shadow_count(int p_count) {

    directional_shadow.light_count = p_count;
    directional_shadow.current_light = 0;
}
void RasterizerSceneGLES3::directional_shadow_create()
{
    //directional light shadow
    directional_shadow.light_count = 0;
    directional_shadow.size = next_power_of_2(T_GLOBAL_GET<uint32_t>("rendering/quality/directional_shadow/size"));
    directional_shadow.fbo.create();
    glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow.fbo);
    directional_shadow.depth.create();
    glBindTexture(GL_TEXTURE_2D, directional_shadow.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, directional_shadow.size, directional_shadow.size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, directional_shadow.depth, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ERR_PRINT("Directional shadow framebuffer status invalid");
    }
}
//! remove ourselves from all lights that reference us
void RasterizerShadowAtlasComponent::unregister_from_lights()
{
    //erase shadow atlas reference from lights
    for (eastl::pair<const RenderingEntity,uint32_t> &E : shadow_owners) {
        auto *li = get<RasterizerLightInstanceShadowAtlasesComponent>(E.first);
        ERR_CONTINUE(!li);

        li->shadow_atlases.erase(self);
    }
    shadow_owners.clear();
}

RasterizerShadowAtlasComponent::~RasterizerShadowAtlasComponent()
{
    if (0 == size || self==entt::null)
        return;

    // erasing atlas
    depth.release();
    fbo.release();
    unregister_from_lights();
}
