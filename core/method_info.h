#pragma once
#include "core/variant.h"
#include "core/vector.h"
#include "core/property_info.h"

struct GODOT_EXPORT MethodInfo {
private:
    void set_args(PropertyInfo &&p_param1) {
        arguments.emplace_back(eastl::move(p_param1));
    }
    void set_args(PropertyInfo &&p_param1, PropertyInfo &&p_param2) {
        arguments.emplace_back(eastl::move(p_param1));
        arguments.emplace_back(eastl::move(p_param2));
    }
    void set_args(PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3) {
        arguments.emplace_back(eastl::move(p_param1));
        arguments.emplace_back(eastl::move(p_param2));
        arguments.emplace_back(eastl::move(p_param3));
    }
    void set_args(PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4) {
        arguments.emplace_back(eastl::move(p_param1));
        arguments.emplace_back(eastl::move(p_param2));
        arguments.emplace_back(eastl::move(p_param3));
        arguments.emplace_back(eastl::move(p_param4));
    }
    void set_args(PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4, PropertyInfo &&p_param5) {
        arguments.emplace_back(eastl::move(p_param1));
        arguments.emplace_back(eastl::move(p_param2));
        arguments.emplace_back(eastl::move(p_param3));
        arguments.emplace_back(eastl::move(p_param4));
        arguments.emplace_back(eastl::move(p_param5));
    }

public:
    StringName name;
    PropertyInfo return_val;
    uint32_t flags=METHOD_FLAG_NORMAL;
    int id = 0;
    Vector<PropertyInfo> arguments;
    Vector<Variant> default_arguments;

    bool operator==(const MethodInfo &p_method) const { return id == p_method.id; }
    bool operator<(const MethodInfo &p_method) const { return id == p_method.id ? (StringView(name).compare(p_method.name)<0) : (id < p_method.id); }

    operator Dictionary() const;

    static MethodInfo from_dict(const Dictionary &p_dict);
    MethodInfo() = default;
    MethodInfo(MethodInfo &&) = default;
    MethodInfo(const MethodInfo &) = default;
    MethodInfo & operator=(MethodInfo &&) noexcept = default;
    MethodInfo & operator=(const MethodInfo &) = default;

    MethodInfo(const char *p_name) :
        name(p_name) {
    }

    MethodInfo(const StringName &p_name) noexcept : name(p_name) {    }

    MethodInfo(StringName p_name, PropertyInfo &&p_param1) :
        name(eastl::move(p_name)) {
        set_args(eastl::move(p_param1));
    }
    MethodInfo(const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2) :
        name(p_name) {
        set_args(eastl::move(p_param1),eastl::move(p_param2));
    }
    MethodInfo(const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3) :
        name(p_name) {
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3));
    }
    MethodInfo(const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4) :
        name(p_name) {
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3),eastl::move(p_param4));
    }
    MethodInfo(const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4, PropertyInfo &&p_param5) :
        name(p_name) {
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3),eastl::move(p_param4),eastl::move(p_param5));
    }
    MethodInfo(VariantType ret, const char *p_name) :
        name(p_name) {
        return_val.type = ret;
    }
    MethodInfo(VariantType ret, const char *p_name, PropertyInfo &&p_param1) :
        name(p_name) {
        return_val.type = ret;
        set_args(eastl::move(p_param1));
    }
    MethodInfo(VariantType ret, const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2) :
        name(p_name) {
        return_val.type = ret;
        set_args(eastl::move(p_param1),eastl::move(p_param2));
    }
    MethodInfo(VariantType ret, const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3) :
        name(p_name) {
        return_val.type = ret;
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3));
    }
    MethodInfo(VariantType ret, const char *p_name,PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4) :
        name(p_name) {
        return_val.type = ret;
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3),eastl::move(p_param4));
    }
    MethodInfo(VariantType ret, const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2, PropertyInfo &&p_param3, PropertyInfo &&p_param4, PropertyInfo &&p_param5) :
        name(p_name) {
        return_val.type = ret;
        set_args(eastl::move(p_param1),eastl::move(p_param2),eastl::move(p_param3),eastl::move(p_param4),eastl::move(p_param5));
    }
    MethodInfo(const PropertyInfo &p_ret, const char *p_name) :
        name(p_name),
        return_val(p_ret) {
    }
    MethodInfo(PropertyInfo &&p_ret, const char *p_name, PropertyInfo &&p_param1) :
        name(p_name),
        return_val(eastl::move(p_ret)) {
        set_args(eastl::move(p_param1));
    }
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, PropertyInfo &&p_param1, PropertyInfo &&p_param2) :
        name(p_name),
        return_val(p_ret) {
        set_args(eastl::move(p_param1),eastl::move(p_param2));
    }
};
