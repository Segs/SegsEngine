#pragma once

#include "core/pool_vector.h"

struct ImageData
{
    enum Format : uint8_t {

        FORMAT_L8=0, //luminance
        FORMAT_LA8, //luminance-alpha
        FORMAT_R8,
        FORMAT_RG8,
        FORMAT_RGB8,
        FORMAT_RGBA8,
        FORMAT_RGBA4444,
        FORMAT_RGB565,
        FORMAT_RF, //float
        FORMAT_RGF,
        FORMAT_RGBF,
        FORMAT_RGBAF,
        FORMAT_RH, //half float
        FORMAT_RGH,
        FORMAT_RGBH,
        FORMAT_RGBAH,
        FORMAT_RGBE9995,
        FORMAT_DXT1, //s3tc bc1
        FORMAT_DXT3, //bc2
        FORMAT_DXT5, //bc3
        FORMAT_RGTC_R,
        FORMAT_RGTC_RG,
        FORMAT_BPTC_RGBA, //btpc bc7
        FORMAT_BPTC_RGBF, //float bc6h
        FORMAT_BPTC_RGBFU, //unsigned float bc6hu
        FORMAT_PVRTC2, //pvrtc
        FORMAT_PVRTC2A,
        FORMAT_PVRTC4,
        FORMAT_PVRTC4A,
        FORMAT_ETC, //etc1
        FORMAT_ETC2_R11, //etc2
        FORMAT_ETC2_R11S, //signed, NOT srgb.
        FORMAT_ETC2_RG11,
        FORMAT_ETC2_RG11S,
        FORMAT_ETC2_RGB8,
        FORMAT_ETC2_RGBA8,
        FORMAT_ETC2_RGB8A1,
        FORMAT_ETC2_RA_AS_RG, //used to make basis universal happy
        FORMAT_DXT5_RA_AS_RG, //used to make basis universal happy
        FORMAT_MAX
    };
    enum {
        MAX_WIDTH = 16384, // force a limit somehow
        MAX_HEIGHT = 16384, // force a limit somehow
        MAX_PIXELS = 268435456
    };

    PoolVector<uint8_t> data;
    int width, height;
    Format format;
    bool mipmaps;
};
