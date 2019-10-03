#pragma once
#include "core/io/resource_format_loader.h"

class ResourceFormatLoaderBMFont : public ResourceFormatLoader {
public:
    RES load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr) override;
    void get_recognized_extensions(ListPOD<String> *p_extensions) const override;
    bool handles_type(const String &p_type) const override;
    String get_resource_type(const String &p_path) const override;
};

/////////////

class ResourceFormatLoaderDynamicFont : public ResourceFormatLoader {
public:
    RES load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr) override;
    void get_recognized_extensions(ListPOD<String> *p_extensions) const override;
    bool handles_type(const String &p_type) const override;
    String get_resource_type(const String &p_path) const override;
};
