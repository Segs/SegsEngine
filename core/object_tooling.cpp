#include "core/object_tooling.h"

#include "core/class_db.h"
#include "core/engine.h"
#include "core/list.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/os/memory.h"
#include "core/property_info.h"
#include "core/script_language.h"
#include "core/se_string.h"
#include "core/set.h"

#ifdef TOOLS_ENABLED
struct ObjectToolingImpl final : public IObjectTooling {

    Set<String> editor_section_folding;
    Set<Object *> change_receptors;
    uint32_t _edited_version;
    bool _edited;

    // IObjectTooling interface
public:
    void set_edited(bool p_edited,bool increment_version=true) final {
        _edited = p_edited;
        if(increment_version)
            _edited_version++;
    }
    bool is_edited() const final {
        return _edited;
    }
    uint32_t get_edited_version() const final {
        return _edited_version;
    }
    void editor_set_section_unfold(se_string_view p_section, bool p_unfolded) final {
        set_edited(true);
        if (p_unfolded)
            editor_section_folding.insert(p_section);
        else {
            auto iter=editor_section_folding.find_as(p_section);
            if(iter!=editor_section_folding.end())
                editor_section_folding.erase(iter);
        }
    }
    [[nodiscard]] bool editor_is_section_unfolded(se_string_view p_section) const final {
        return editor_section_folding.contains_as(p_section);
    }
    [[nodiscard]] const Set<String> &editor_get_section_folding() const final {
        return editor_section_folding;
    }
    void editor_clear_section_folding() final {
        editor_section_folding.clear();
    }
    ObjectToolingImpl() {
        _edited = false;
        _edited_version = 0;
    }
};
void Object_change_notify(Object *self,StringName p_property) {
    auto tooling_iface=(ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->set_edited(true,false);
    for (Object *E : tooling_iface->change_receptors)
        E->_changed_callback(self, p_property);
}

void relase_tooling(IObjectTooling *s)
{
    memdelete(s);
}

IObjectTooling *create_tooling_for(Object *self)
{
    return memnew(ObjectToolingImpl);
}

void Object_add_change_receptor(Object *self, Object *p_receptor)
{
    auto tooling_iface=(ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->change_receptors.insert(p_receptor);
}
void Object_remove_change_receptor(Object *self, Object *p_receptor)
{
    auto tooling_iface=(ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->change_receptors.erase(p_receptor);
}

void Object_set_edited(Object *self, bool p_edited, bool increment_version)
{
    auto tooling_iface=(ObjectToolingImpl *)self->get_tooling_interface();
    tooling_iface->set_edited(p_edited,increment_version);
}

bool Object_set_fallback(Object *self,const StringName &p_name,const Variant &p_value)
{
    bool valid=false;
    auto si = self->get_script_instance();
    if (si) {
        si->property_set_fallback(p_name, p_value, &valid);
    }
    return valid;
}

Variant Object_get_fallback(const Object *self,const StringName &p_name,bool &r_valid)
{
    auto si = self->get_script_instance();
    Variant ret;
    if (si) {
        bool valid;
        ret = si->property_get_fallback(p_name, &valid);
        if (valid) {
            r_valid = true;
            return ret;
        }
    }
    r_valid=false;
    return ret;
}

void Object_add_tool_properties(Vector<PropertyInfo> *p_list)
{
    p_list->push_back(PropertyInfo(VariantType::NIL, "Script", PropertyHint::None, "", PROPERTY_USAGE_GROUP));
}

bool Object_script_signal_validate(RefPtr script)
{
    //allow connecting signals anyway if script is invalid, see issue #17070
    if (!refFromRefPtr<Script>(script)->is_valid()) {
        return true;
    }
    return false;
}

bool Object_allow_disconnect(uint32_t f) {
    if ((f & ObjectNS::CONNECT_PERSIST) && Engine::get_singleton()->is_editor_hint()) {
        // this signal was connected from the editor, and is being edited. just don't disconnect for now
        return false;
    }
    return true;
}

void Object_add_tooling_methods()
{
    MethodInfo miget("_get", PropertyInfo(VariantType::STRING, "property"));
    miget.return_val.name = "Variant";
    miget.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
    ClassDB::add_virtual_method(Object::get_class_static_name(), miget);

    MethodInfo plget(VariantType::ARRAY,"_get_property_list");
    ClassDB::add_virtual_method(Object::get_class_static_name(), plget);
}

#endif
