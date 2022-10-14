#pragma once

#include "core/reflection_macros.h"
#include "core/object.h"
#include "core/reference.h"

#include <stdint.h>

struct Frame {
    SE_CLASS(struct)
    Ref<Texture> texture;
    float delay_sec;

    Frame() {
        delay_sec = 0;
    }
};

class Propertied {
    SE_CLASS()
    enum {
        MAX_BLEND_POINTS = 64
    };
    struct BlendPoint {
        SE_CLASS(struct)

        StringName name;
        Ref<AnimationRootNode> node;
        float position;
    };

    BlendPoint blend_points[MAX_BLEND_POINTS];
    // read only property
    SE_PROPERTY(Vector<int> val1 READ get_val1)
    // read/write property
    SE_PROPERTY(String label READ get_label WRITE set_label)
    // read/write with usage specification
    SE_PROPERTY(int tester READ get_tester WRITE set_tester USAGE STORAGE|EDITOR|INTERNAL|DO_NOT_SHARE_ON_DUPLICATE)

    SE_PROPERTY(bool areas READ is_clip_to_areas_enabled WRITE set_clip_to_areas GROUP clip_to)
    SE_PROPERTY(bool bodies READ is_clip_to_bodies_enabled WRITE set_clip_to_bodies GROUP clip_to)

    SE_PROPERTY(Frame[256] frames READ get_frame WRITE set_frame)
    SE_PROPERTY(BlendPoint[64] blend_points READ get_point WRITE set_point)
};
