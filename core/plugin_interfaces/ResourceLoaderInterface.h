#pragma once

#include "core/resource.h"
#include "core/ustring.h"

class ResourceLoaderInterface {
public:
    virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr)  = 0;
    virtual void get_recognized_extensions(ListPOD<String> *p_extensions) const = 0;
    virtual bool handles_type(const String &p_type) const = 0;
    virtual String get_resource_type(const String &p_path) const = 0;
    virtual ~ResourceLoaderInterface() = default;
};
