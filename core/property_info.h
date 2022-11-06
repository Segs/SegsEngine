#pragma once
#include <utility>

#include "core/string.h"
#include "core/string_name.h"
#include "typesystem_decls.h"

enum class VariantType : int8_t;
class Dictionary;

struct GODOT_EXPORT PropertyInfo {
public:
    StringName name;
    String hint_string;
    StringName class_name; // for classes
    uint32_t usage = PROPERTY_USAGE_DEFAULT;
    int16_t element_count = 0; // used by array property 'header' to mark the number of array entries
    //TODO: consider negative PropertyInfo::element_count for dynamic arrays ?
    VariantType type = VariantType(0);
    PropertyHint hint = PropertyHint::None;


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
    PropertyInfo(PropertyInfo &&) noexcept = default;
    PropertyInfo(const PropertyInfo &oth) = default;

    PropertyInfo(VariantType p_type, StringName &&p_name, PropertyHint p_hint = PropertyHint::None,
            StringView p_hint_string=StringView(), uint32_t p_usage = PROPERTY_USAGE_DEFAULT,
            const StringName &p_class_name = StringName()) :
            name(eastl::move(p_name)),
            hint_string(p_hint_string),
            usage(p_usage),
            type(p_type),
            hint(p_hint) {

        if (hint == PropertyHint::ResourceType) {
            class_name = StringName(p_hint_string);
        } else {
            class_name = p_class_name;
        }
    }
    // For property array 'head' entry
    PropertyInfo(StringName &&p_name,int count, const StringName &p_array_prefix = StringName()) :
            name(eastl::move(p_name)),
            hint_string(p_array_prefix),
            usage(PROPERTY_USAGE_ARRAY| PROPERTY_USAGE_INTERNAL),
            element_count(count) {
    }

    PropertyInfo(StringName &&p_class_name, VariantType t) : class_name(eastl::move(p_class_name)), type(t) {}
    PropertyInfo(const RawPropertyInfo &rp) :
        name(rp.name ? StaticCString(rp.name, true) : StringName()),
        hint_string(rp.hint_string ? rp.hint_string : ""),
        usage(rp.usage),
        type(VariantType(rp.type)),
        hint(rp.hint)
    {
        // Handles ClassName::NestedType -> ClassName.NestedType conversion
        bool has_class_spec = rp.class_name && StringView(rp.class_name).contains(StringView("::"));
        if(has_class_spec) {
            class_name = StringName(String(rp.class_name).replaced("::","."));
        }
        else
            class_name = StringName(rp.class_name ? StaticCString(rp.class_name, true) : StringName());
    }

    bool operator==(const PropertyInfo &p_info) const {
        return ((type == p_info.type) &&
                (name == p_info.name) &&
                (class_name == p_info.class_name) &&
                (hint == p_info.hint) &&
                (hint_string == p_info.hint_string) &&
                (usage == p_info.usage));
    }

    bool operator<(const PropertyInfo &p_info) const {
        return StringView(name).compare(p_info.name)<0;
    }
    ~PropertyInfo() = default;
};
