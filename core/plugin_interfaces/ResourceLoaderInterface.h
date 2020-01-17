#pragma once

#include "core/resource.h"
#include "core/se_string.h"

class ResourceLoaderInterface {
public:
    virtual RES load(se_string_view p_path, se_string_view p_original_path = se_string_view (), Error *r_error = nullptr)  = 0;
    virtual void get_recognized_extensions(PODVector<String> &p_extensions) const = 0;
    virtual bool handles_type(se_string_view p_type) const = 0;
    virtual String get_resource_type(se_string_view p_path) const = 0;
    virtual ~ResourceLoaderInterface() = default;
};
