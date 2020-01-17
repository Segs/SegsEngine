#pragma once
#include "core/io/resource_format_loader.h"

class ResourceFormatLoaderBMFont : public ResourceFormatLoader {
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};

/////////////

class ResourceFormatLoaderDynamicFont : public ResourceFormatLoader {
public:
    RES load(se_string_view p_path, se_string_view p_original_path = se_string_view(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;
    bool handles_type(se_string_view p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};
