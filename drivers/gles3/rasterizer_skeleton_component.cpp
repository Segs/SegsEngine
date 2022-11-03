#include "rasterizer_skeleton_component.h"

#include "rasterizer_storage_gles3.h"
#include "servers/rendering/render_entity_getter.h"

namespace {
struct RasterizerSkeletonDirty {};

void skeleton_allocate(RasterizerSkeletonComponent *skeleton, int p_bones, bool p_2d_skeleton) {
    ERR_FAIL_COND(!skeleton);
    ERR_FAIL_COND(p_bones < 0);

    if (skeleton->size == p_bones && skeleton->use_2d == p_2d_skeleton) {
        return;
    }

    skeleton->size = p_bones;
    skeleton->use_2d = p_2d_skeleton;

    int height = p_bones / 256;
    if (p_bones % 256) {
        height++;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skeleton->texture);

    if (skeleton->use_2d) {
        skeleton->skel_texture.resize(256 * height * 2 * 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, height * 2, 0, GL_RGBA, GL_FLOAT, nullptr);
    } else {
        skeleton->skel_texture.resize(256 * height * 3 * 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 256, height * 3, 0, GL_RGBA, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
} // namespace

RasterizerSkeletonComponent::RasterizerSkeletonComponent() = default;

//! remove this skeleton from all instances referencing it
void RasterizerSkeletonComponent::unregister_from_instances()
{
    auto inst_view(VSG::ecs->registry.view<RenderingInstanceComponent>());
    for (RenderingEntity E : instances) {
        assert(VSG::ecs->registry.all_of<RenderingInstanceComponent>(E));
        auto &inst = inst_view.get<RenderingInstanceComponent>(E);
        inst.skeleton = entt::null;
    }
    instances.clear();
}
RasterizerSkeletonComponent::~RasterizerSkeletonComponent()
{
   unregister_from_instances();
}

void mark_skeleton_dirty(RenderingEntity e)
{
    assert(VSG::ecs->valid(e));
    VSG::ecs->registry.emplace_or_replace<RasterizerSkeletonDirty>(e);
}


void update_dirty_skeletons()
{
    glActiveTexture(GL_TEXTURE0);

    auto grp=VSG::ecs->registry.group<RasterizerSkeletonDirty,RasterizerSkeletonComponent>();
    grp.each([](auto ent,RasterizerSkeletonComponent &skeleton) {

        if (skeleton.size) {

            int height = skeleton.size / 256;
            if (skeleton.size % 256)
                height++;

            glBindTexture(GL_TEXTURE_2D, skeleton.texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, height * (skeleton.use_2d ? 2 : 3), GL_RGBA, GL_FLOAT, skeleton.skel_texture.data());
        }

        for (auto E : skeleton.instances) {
            getUnchecked<RenderingInstanceComponent>(E)->base_changed(true, false);
        }
    });
    VSG::ecs->registry.clear<RasterizerSkeletonDirty>();
}

/* SKELETON API */

RenderingEntity RasterizerStorageGLES3::skeleton_create() {

    auto res = VSG::ecs->create();
    auto &skeleton(VSG::ecs->registry.emplace<RasterizerSkeletonComponent>(res));

    skeleton.texture.create();

    return res;
}

void RasterizerStorageGLES3::skeleton_allocate(RenderingEntity p_skeleton, int p_bones, bool p_2d_skeleton) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);
    ::skeleton_allocate(skeleton,p_bones,p_2d_skeleton);
    VSG::ecs->registry.emplace_or_replace<RasterizerSkeletonDirty>(p_skeleton);
}

int RasterizerStorageGLES3::skeleton_get_bone_count(RenderingEntity p_skeleton) const {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);
    ERR_FAIL_COND_V(!skeleton, 0);

    return skeleton->size;
}

void RasterizerStorageGLES3::skeleton_bone_set_transform(RenderingEntity p_skeleton, int p_bone, const Transform &p_transform) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);

    ERR_FAIL_COND(!skeleton);
    ERR_FAIL_INDEX(p_bone, skeleton->size);
    ERR_FAIL_COND(skeleton->use_2d);

    float *texture = skeleton->skel_texture.data();

    int base_ofs = ((p_bone / 256) * 256) * 3 * 4 + (p_bone % 256) * 4;

    texture[base_ofs + 0] = p_transform.basis[0].x;
    texture[base_ofs + 1] = p_transform.basis[0].y;
    texture[base_ofs + 2] = p_transform.basis[0].z;
    texture[base_ofs + 3] = p_transform.origin.x;
    base_ofs += 256 * 4;
    texture[base_ofs + 0] = p_transform.basis[1].x;
    texture[base_ofs + 1] = p_transform.basis[1].y;
    texture[base_ofs + 2] = p_transform.basis[1].z;
    texture[base_ofs + 3] = p_transform.origin.y;
    base_ofs += 256 * 4;
    texture[base_ofs + 0] = p_transform.basis[2].x;
    texture[base_ofs + 1] = p_transform.basis[2].y;
    texture[base_ofs + 2] = p_transform.basis[2].z;
    texture[base_ofs + 3] = p_transform.origin.z;

    VSG::ecs->registry.emplace_or_replace<RasterizerSkeletonDirty>(p_skeleton);
}

Transform RasterizerStorageGLES3::skeleton_bone_get_transform(RenderingEntity p_skeleton, int p_bone) const {

    const auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);

    ERR_FAIL_COND_V(!skeleton, Transform());
    ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform());
    ERR_FAIL_COND_V(skeleton->use_2d, Transform());

    const float* texture = skeleton->skel_texture.data();

    Transform ret;

    int base_ofs = ((p_bone / 256) * 256) * 3 * 4 + (p_bone % 256) * 4;

    ret.basis[0].x = texture[base_ofs + 0];
    ret.basis[0].y = texture[base_ofs + 1];
    ret.basis[0].z = texture[base_ofs + 2];
    ret.origin.x = texture[base_ofs + 3];
    base_ofs += 256 * 4;
    ret.basis[1].x = texture[base_ofs + 0];
    ret.basis[1].y = texture[base_ofs + 1];
    ret.basis[1].z = texture[base_ofs + 2];
    ret.origin.y = texture[base_ofs + 3];
    base_ofs += 256 * 4;
    ret.basis[2].x = texture[base_ofs + 0];
    ret.basis[2].y = texture[base_ofs + 1];
    ret.basis[2].z = texture[base_ofs + 2];
    ret.origin.z = texture[base_ofs + 3];

    return ret;
}
void RasterizerStorageGLES3::skeleton_bone_set_transform_2d(RenderingEntity p_skeleton, int p_bone, const Transform2D &p_transform) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);

    ERR_FAIL_COND(!skeleton);
    ERR_FAIL_INDEX(p_bone, skeleton->size);
    ERR_FAIL_COND(!skeleton->use_2d);

    float* texture = skeleton->skel_texture.data();

    int base_ofs = ((p_bone / 256) * 256) * 2 * 4 + (p_bone % 256) * 4;

    texture[base_ofs + 0] = p_transform[0][0];
    texture[base_ofs + 1] = p_transform[1][0];
    texture[base_ofs + 2] = 0;
    texture[base_ofs + 3] = p_transform[2][0];
    base_ofs += 256 * 4;
    texture[base_ofs + 0] = p_transform[0][1];
    texture[base_ofs + 1] = p_transform[1][1];
    texture[base_ofs + 2] = 0;
    texture[base_ofs + 3] = p_transform[2][1];

    skeleton->revision++;
    VSG::ecs->registry.emplace_or_replace<RasterizerSkeletonDirty>(p_skeleton);
}
Transform2D RasterizerStorageGLES3::skeleton_bone_get_transform_2d(RenderingEntity p_skeleton, int p_bone) const {

    const auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);

    ERR_FAIL_COND_V(!skeleton, Transform2D());
    ERR_FAIL_INDEX_V(p_bone, skeleton->size, Transform2D());
    ERR_FAIL_COND_V(!skeleton->use_2d, Transform2D());

    const float* texture = skeleton->skel_texture.data();

    Transform2D ret;

    int base_ofs = ((p_bone / 256) * 256) * 2 * 4 + (p_bone % 256) * 4;

    ret[0][0] = texture[base_ofs + 0];
    ret[1][0] = texture[base_ofs + 1];
    ret[2][0] = texture[base_ofs + 3];
    base_ofs += 256 * 4;
    ret[0][1] = texture[base_ofs + 0];
    ret[1][1] = texture[base_ofs + 1];
    ret[2][1] = texture[base_ofs + 3];

    return ret;
}

void RasterizerStorageGLES3::skeleton_set_base_transform_2d(RenderingEntity p_skeleton, const Transform2D &p_base_transform) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);

    ERR_FAIL_COND(!skeleton->use_2d);

    skeleton->base_transform_2d = p_base_transform;
}

uint32_t RasterizerStorageGLES3::skeleton_get_revision(RenderingEntity p_skeleton) const {
    const auto *skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);
    ERR_FAIL_COND_V(!skeleton, 0);
    return skeleton->revision;
}

void RasterizerStorageGLES3::update_dirty_skeletons() {
    ::update_dirty_skeletons();
}


// instancing support

void RasterizerStorageGLES3::instance_add_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);
    ERR_FAIL_COND(!skeleton);

    skeleton->instances.insert(p_instance);
}

void RasterizerStorageGLES3::instance_remove_skeleton(RenderingEntity p_skeleton, RenderingEntity p_instance) {

    auto * skeleton = VSG::ecs->try_get<RasterizerSkeletonComponent>(p_skeleton);
    ERR_FAIL_COND(!skeleton);

    skeleton->instances.erase(p_instance);
}

