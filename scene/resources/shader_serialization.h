#pragma once
#include "core/io/resource_format_loader.h"
#include "core/io/resource_saver.h"

class ResourceFormatLoaderShader : public ResourceFormatLoader {
public:
    RES load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr) override;
    void get_recognized_extensions(ListPOD<String> *p_extensions) const override;
    bool handles_type(const String &p_type) const override;
    String get_resource_type(const String &p_path) const override;
};

class ResourceFormatSaverShader : public ResourceFormatSaver {
public:
    Error save(const String &p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    void get_recognized_extensions(const RES &p_resource, Vector<String> *p_extensions) const override;
    bool recognize(const RES &p_resource) const override;
};

