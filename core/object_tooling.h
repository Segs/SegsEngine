#pragma once

#include "core/string_name.h"
#include "core/forward_decls.h"

#ifndef TOOLS_ENABLED
#include "core/variant.h"
#endif
class Object;
class IObjectTooling;
class Variant;
struct PropertyInfo;
class RefPtr;

// Internal tooling helpers.
#ifdef TOOLS_ENABLED
    GODOT_EXPORT void Object_change_notify(Object *self,StringName p_property = StringName());
    GODOT_EXPORT void Object_add_change_receptor(Object *self,Object *p_receptor);
    GODOT_EXPORT void Object_remove_change_receptor(Object *self,Object *p_receptor);
    GODOT_EXPORT void Object_set_edited(Object *self,bool p_edited,bool increment_version=true);
    GODOT_EXPORT bool Object_set_fallback(Object *self,const StringName &p_name,const Variant &p_value);
    // TODO: SEGS: consider using eastl::optional<Variant> as return type ?
    GODOT_EXPORT Variant Object_get_fallback(const Object *self, const StringName &p_name, bool &r_valid);
    GODOT_EXPORT IObjectTooling * create_tooling_for(Object *self);
    GODOT_EXPORT void relase_tooling(IObjectTooling *);
    GODOT_EXPORT bool Object_script_signal_validate(const RefPtr &self);
    GODOT_EXPORT bool Object_allow_disconnect(uint32_t f);
    GODOT_EXPORT void Object_add_tooling_methods();
#else
    inline constexpr void Object_change_notify(Object * /*self*/,StringView /*p_what*/ = {}) {}
    inline constexpr IObjectTooling * create_tooling_for(Object * /*self*/) { return nullptr; }
    inline constexpr void relase_tooling(IObjectTooling *) {}
    inline constexpr void Object_add_change_receptor(Object * /*self*/,Object * /*p_receptor*/) {}
    inline constexpr void Object_remove_change_receptor(Object * /*self*/,Object * /*p_receptor*/) {}
    inline constexpr void Object_set_edited(Object * /*self*/,bool /*p_edited*/,bool /*increment_version*/=true) {}
    inline constexpr bool Object_set_fallback(Object * /*self*/,const StringName &/*p_name*/, const Variant & /*p_value*/) {return false;}
    inline Variant Object_get_fallback(const Object * /*self*/, const StringName &/*p_name*/, bool & r_valid) { r_valid=false; return {};}
    inline constexpr void Object_add_tool_properties(Vector<PropertyInfo> *) {}
    inline constexpr bool Object_script_signal_validate(const RefPtr & /*self*/) { return false; }
    inline constexpr bool Object_allow_disconnect(uint32_t /*f*/) { return true; }
    inline constexpr void Object_add_tooling_methods() {}
#endif

class GODOT_EXPORT IObjectTooling {
#ifdef TOOLS_ENABLED
    friend void Object_set_edited(Object *self,bool p_edited,bool increment_version);
#else
    friend constexpr void Object_set_edited(Object *self,bool p_edited,bool increment_version);
#endif

public:
    virtual bool is_edited() const = 0;
    //! this function is used to check when something changed beyond a point, it's used mainly for generating previews
    virtual uint32_t get_edited_version() const =0;

    virtual void editor_set_section_unfold(StringView p_section, bool p_unfolded)=0;
    virtual bool editor_is_section_unfolded(StringView p_section) const = 0;
    virtual const Set<String> &editor_get_section_folding() const =0;
    virtual void editor_clear_section_folding()=0;
    virtual ~IObjectTooling() = default;
private:
    virtual void set_edited(bool p_edited,bool increment_version=true)=0;
};


