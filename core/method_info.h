#pragma once
#include "core/variant.h"
#include "core/vector.h"
#include "core/property_info.h"

struct GODOT_EXPORT MethodInfo {

    StringName name;
    PropertyInfo return_val;
    uint32_t flags;
    int id = 0;
    PODVector<PropertyInfo> arguments;
    PODVector<Variant> default_arguments;

    bool operator==(const MethodInfo &p_method) const { return id == p_method.id; }
    bool operator<(const MethodInfo &p_method) const { return id == p_method.id ? (StringUtils::compare(name,p_method.name)<0) : (id < p_method.id); }

    operator Dictionary() const;

    static MethodInfo from_dict(const Dictionary &p_dict);
    MethodInfo();
    MethodInfo(const char *p_name);
    MethodInfo(const char *p_name, const PropertyInfo &p_param1);
    MethodInfo(const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
    MethodInfo(const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
    MethodInfo(const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
    MethodInfo(const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
    MethodInfo(VariantType ret);
    MethodInfo(VariantType ret, const char *p_name);
    MethodInfo(VariantType ret, const char *p_name, const PropertyInfo &p_param1);
    MethodInfo(VariantType ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
    MethodInfo(VariantType ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
    MethodInfo(VariantType ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
    MethodInfo(VariantType ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
};
