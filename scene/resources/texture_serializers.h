#pragma once


#include "core/io/resource_format_loader.h"

class ResourceFormatLoaderStreamTexture : public ResourceFormatLoader {
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};

class ResourceFormatLoaderTextureLayered : public ResourceFormatLoader {
public:
    enum Compression {
        COMPRESSION_LOSSLESS,
        COMPRESSION_VRAM,
        COMPRESSION_UNCOMPRESSED
    };

    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};

