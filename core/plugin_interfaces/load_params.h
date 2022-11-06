#pragma once
#include <stdint.h>

enum class ImageUsedChannels : int8_t;

struct LoadParams {
    float p_scale = 1.0;
    bool p_force_linear = false;
    bool p_will_upsample = false; // used by svg import
    bool p_convert_colors = false; // used by svg import
};

struct SaveParams {
    float p_quality = 1.0;
    bool p_greyscale = false; // used by exr saver
    bool p_lossless = false;
};
enum ImageCompressMode : int8_t {
    COMPRESS_S3TC,
    COMPRESS_BPTC,
    COMPRESS_MAX
};
struct CompressParams {
    float p_quality = 1.0;
    ImageCompressMode mode = COMPRESS_S3TC;
    ImageUsedChannels used_channels = ImageUsedChannels(0);
};
