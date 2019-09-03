#pragma once

struct LoadParams
{
    float p_scale = 1.0;
    bool p_force_linear = false;
    bool p_will_upsample = false; // used by svg import
    bool p_convert_colors = false; // used by svg import
};

struct SaveParams
{
    float p_quality = 1.0;
    bool p_greyscale; // used by exr saver
};
