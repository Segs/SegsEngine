#pragma once
#include "core/variant.h"
#include "core/ustring.h"

#include "core/property_info.h"

struct MethodInfo {

    String name;
    PropertyInfo return_val;
    uint32_t flags;
    int id = 0;
    List<PropertyInfo> arguments;
    Vector<Variant> default_arguments;

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
    MethodInfo(Variant::Type ret);
    MethodInfo(Variant::Type ret, const char *p_name);
    MethodInfo(Variant::Type ret, const char *p_name, const PropertyInfo &p_param1);
    MethodInfo(Variant::Type ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
    MethodInfo(Variant::Type ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
    MethodInfo(Variant::Type ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
    MethodInfo(Variant::Type ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
    MethodInfo(const PropertyInfo &p_ret, const char *p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
};
