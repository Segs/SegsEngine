#pragma once
#include "core/variant.h"
#include "core/ustring.h"
#include "typesystem_decls.h"

struct PropertyInfo {
private:
public:
    String name;
    String hint_string;
    StringName class_name; // for classes
    Variant::Type type = Variant::NIL;
    PropertyHint hint = PROPERTY_HINT_NONE;
    uint32_t usage = PROPERTY_USAGE_DEFAULT;

    _FORCE_INLINE_ PropertyInfo with_added_usage(int p_fl) const {
        PropertyInfo pi = *this;
        pi.usage |= p_fl;
        return pi;
    }

    operator Dictionary() const;

    static PropertyInfo from_dict(const Dictionary &p_dict);

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
    PropertyInfo() {
    }
    PropertyInfo(const PropertyInfo &oth) :
        name(oth.name),
        hint_string(oth.hint_string),
        class_name(oth.class_name),
        type(oth.type),
        hint(oth.hint),
        usage(oth.usage)
    {
    }
    PropertyInfo(Variant::Type p_type, const char *p_name, PropertyHint p_hint = PROPERTY_HINT_NONE,
            const char *p_hint_string = nullptr, uint32_t p_usage = PROPERTY_USAGE_DEFAULT,
            const StringName &p_class_name = StringName()) :
            name(p_name),
            hint_string(p_hint_string),
            type(p_type),
            hint(p_hint),
            usage(p_usage) {

        if (hint == PROPERTY_HINT_RESOURCE_TYPE) {
            class_name = StaticCString(p_hint_string,true);
        } else {
            class_name = p_class_name;
        }
    }
    PropertyInfo(Variant::Type p_type, String &&p_name, PropertyHint p_hint = PROPERTY_HINT_NONE,
            const StringName &p_hint_string = String::null_val, uint32_t p_usage = PROPERTY_USAGE_DEFAULT,
            const StringName &p_class_name = StringName()) :
            name(std::move(p_name)),
            hint_string(p_hint_string),
            type(p_type),
            hint(p_hint),
            usage(p_usage) {

        if (hint == PROPERTY_HINT_RESOURCE_TYPE) {
            class_name = hint_string;
        } else {
            class_name = p_class_name;
        }
    }

    PropertyInfo(const StringName &p_class_name) : class_name(p_class_name), type(Variant::OBJECT) {}
    PropertyInfo(const RawPropertyInfo &rp) :
        name(rp.name),
        hint_string(rp.hint_string),
        class_name(rp.class_name ? StaticCString(rp.class_name, true) : StringName()),
        type(Variant::Type(rp.type)),
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
        return StringUtils::compare(name,p_info.name)<0;
    }
    ~PropertyInfo() = default;
};
