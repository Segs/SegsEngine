#pragma once

#include "core/resource.h"
#include "core/string.h"

class ResourceLoaderInterface {
public:
    virtual RES load(StringView p_path, StringView p_original_path = StringView(), Error *r_error = nullptr, bool p_no_subresource_cache = false) = 0;
    virtual void get_recognized_extensions(Vector<String> &p_extensions) const = 0;
    virtual bool handles_type(StringView p_type) const = 0;
    virtual String get_resource_type(StringView p_path) const = 0;
    virtual ~ResourceLoaderInterface() = default;
};
