#pragma once
#include <utility>

#include "core/se_string.h"
#include "typesystem_decls.h"

enum class VariantType : int8_t;
class Dictionary;

struct GODOT_EXPORT PropertyInfo {
public:
    StringName name;
    se_string hint_string;
    StringName class_name; // for classes
    VariantType type = VariantType(0);
    PropertyHint hint = PROPERTY_HINT_NONE;
    uint32_t usage = PROPERTY_USAGE_DEFAULT;

    PropertyInfo with_added_usage(int p_fl) const {
        PropertyInfo pi = *this;
        pi.usage |= p_fl;
        return pi;
    }

    operator Dictionary() const;

    static PropertyInfo from_dict(const Dictionary &p_dict);

    PropertyInfo& operator=(PropertyInfo &&oth) noexcept = default;

    PropertyInfo& operator=(const PropertyInfo&oth) {
        if(this==&oth)
            return *this;
        name=oth.name;
        hint_string=oth.hint_string;
        class_name=oth.class_name;
        type=oth.type;
        hint=oth.hint;
        usage=oth.usage;
        return *this;
    }
    PropertyInfo() = default;

    PropertyInfo(const PropertyInfo &oth) = default;

//    PropertyInfo(VariantType p_type, StringName p_name, PropertyHint p_hint = PROPERTY_HINT_NONE,
//            const char *p_hint_string = nullptr, uint32_t p_usage = PROPERTY_USAGE_DEFAULT,
//            const StringName &p_class_name = StringName()) :
//            name(p_name),
//            hint_string(p_hint_string ? p_hint_string : se_string()),
//            type(p_type),
//            hint(p_hint),
//            usage(p_usage) {

//        if (hint == PROPERTY_HINT_RESOURCE_TYPE) {
//            class_name = StaticCString(p_hint_string,true);
//        } else {
//            class_name = p_class_name;
//        }
//    }
    PropertyInfo(VariantType p_type, StringName p_name, PropertyHint p_hint = PROPERTY_HINT_NONE,
            se_string_view p_hint_string=se_string_view(), uint32_t p_usage = PROPERTY_USAGE_DEFAULT,
            const StringName &p_class_name = StringName()) :
            name(eastl::move(p_name)),
            hint_string(p_hint_string),
            type(p_type),
            hint(p_hint),
            usage(p_usage) {

        if (hint == PROPERTY_HINT_RESOURCE_TYPE) {
            class_name = StringName(p_hint_string);
        } else {
            class_name = p_class_name;
        }
    }


    PropertyInfo(StringName p_class_name, VariantType t) : class_name(std::move(p_class_name)), type(t) {}
    PropertyInfo(const RawPropertyInfo &rp) :
        name(rp.name),
        hint_string(rp.hint_string),
        class_name(rp.class_name ? StaticCString(rp.class_name, true) : StringName()),
        type(VariantType(rp.type)),
        hint(rp.hint),
        usage(rp.usage)
    {}

    bool operator==(const PropertyInfo &p_info) const {
        return ((type == p_info.type) &&
                (name == p_info.name) &&
                (class_name == p_info.class_name) &&
                (hint == p_info.hint) &&
                (hint_string == p_info.hint_string) &&
                (usage == p_info.usage));
    }

    bool operator<(const PropertyInfo &p_info) const {
        return se_string_view(name).compare(p_info.name)<0;
    }
    ~PropertyInfo() = default;
};
