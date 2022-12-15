#pragma once

#include "core/forward_decls.h"
#include "core/string_name.h"

class Object;
class IObjectTooling;
class Variant;
struct PropertyInfo;
class RefPtr;
struct ClassDB_ClassInfo;
struct MethodInfo;
class PHashTranslation;
class Translation;
template <class T>
class Ref;
class Resource;

/**
 * Tooling interface that replaces usages of macros with calls to functionality implemented 'outside' of engine dll
 */
namespace Tooling
{

GODOT_EXPORT bool class_can_instance_cb(ClassDB_ClassInfo *ti, const StringName &string_name);
GODOT_EXPORT void add_virtual_method(const StringName & string_name, const MethodInfo & method_info);
GODOT_EXPORT void generate_phash_translation(PHashTranslation &tgt, const Ref<Translation> &p_from);
GODOT_EXPORT bool tooling_log();
GODOT_EXPORT void importer_load(const Ref<Resource> & res, const String & path);
GODOT_EXPORT bool check_resource_manager_load(StringView p_path);

}

// Internal tooling helpers.
#ifdef TOOLS_ENABLED
    GODOT_EXPORT void Object_change_notify(Object *self,const StringName &p_property = StringName());
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
    inline void Object_set_edited(Object * /*self*/,bool /*p_edited*/,bool /*increment_version*/=true) {}
    inline constexpr bool Object_set_fallback(Object * /*self*/,const StringName &/*p_name*/, const Variant & /*p_value*/) {return false;}
    inline Variant Object_get_fallback(const Object * /*self*/, const StringName &/*p_name*/, bool & r_valid) { r_valid=false; return {};}
    inline constexpr void Object_add_tool_properties(Vector<PropertyInfo> *) {}
    inline constexpr bool Object_script_signal_validate(const RefPtr & /*self*/) { return false; }
    inline constexpr bool Object_allow_disconnect(uint32_t /*f*/) { return true; }
    inline constexpr void Object_add_tooling_methods() {}
#endif

class GODOT_EXPORT IObjectTooling {
    friend void Object_set_edited(Object *self,bool p_edited,bool increment_version);

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


