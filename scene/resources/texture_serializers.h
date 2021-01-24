#pragma once


#include "core/io/resource_format_loader.h"

class ResourceFormatLoaderStreamTexture : public ResourceFormatLoader {
public:
    RES load(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(StringView p_type) const override;
    String get_resource_type(StringView p_path) const override;
};

class ResourceFormatLoaderTextureLayered : public ResourceFormatLoader {
public:
    RES load(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(StringView p_type) const override;
    String get_resource_type(StringView p_path) const override;
};

