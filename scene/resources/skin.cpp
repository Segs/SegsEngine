#include "skin.h"

#include "core/ustring.h"
#include "core/list.h"

#include "core/method_bind_interface.h"
#include "core/method_bind.h"


IMPL_GDCLASS(Skin)

void Skin::set_bind_count(int p_size) {
    ERR_FAIL_COND(p_size < 0)
    binds.resize(p_size);
    binds_ptr = binds.ptrw();
    bind_count = p_size;
    emit_changed();
}

void Skin::add_bind(int p_bone, const Transform &p_pose) {
    uint32_t index = bind_count;
    set_bind_count(bind_count + 1);
    set_bind_bone(index, p_bone);
    set_bind_pose(index, p_pose);
}

void Skin::set_bind_bone(int p_index, int p_bone) {
    ERR_FAIL_INDEX(p_index, bind_count)
    binds_ptr[p_index].bone = p_bone;
    emit_changed();
}

void Skin::set_bind_pose(int p_index, const Transform &p_pose) {
    ERR_FAIL_INDEX(p_index, bind_count)
    binds_ptr[p_index].pose = p_pose;
    emit_changed();
}

void Skin::clear_binds() {
    binds.clear();
    binds_ptr = nullptr;
    bind_count = 0;
    emit_changed();
}

bool Skin::_set(const StringName &p_name, const Variant &p_value) {
    using namespace StringUtils;
    String name = p_name;
    if (name == "bind_count") {
        set_bind_count(p_value);
        return true;
    } else if (begins_with(name,"bind/")) {
        int index = to_int(get_slice(name,'/', 1));
        String what = get_slice(name,'/', 2);
        if (what == "bone") {
            set_bind_bone(index, p_value);
            return true;
        } else if (what == "pose") {
            set_bind_pose(index, p_value);
            return true;
        }
    }
    return false;
}

bool Skin::_get(const StringName &p_name, Variant &r_ret) const {
    using namespace StringUtils;

    String name = p_name;
    if (name == "bind_count") {
        r_ret = get_bind_count();
        return true;
    } else if (begins_with(name,"bind/")) {
        int index = to_int(get_slice(name,'/', 1));
        String what = get_slice(name,'/', 2);
        if (what == "bone") {
            r_ret = get_bind_bone(index);
            return true;
        } else if (what == "pose") {
            r_ret = get_bind_pose(index);
            return true;
        }
    }
    return false;
}
void Skin::_get_property_list(ListPOD<PropertyInfo> *p_list) const {
    p_list->push_back(PropertyInfo(VariantType::INT, "bind_count", PROPERTY_HINT_RANGE, "0,16384,1,or_greater"));
    for (int i = 0; i < get_bind_count(); i++) {
        p_list->push_back(PropertyInfo(VariantType::INT, "bind/" + itos(i) + "/bone", PROPERTY_HINT_RANGE, "0,16384,1,or_greater"));
        p_list->push_back(PropertyInfo(VariantType::TRANSFORM, "bind/" + itos(i) + "/pose"));
    }
}

void Skin::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_bind_count", {"bind_count"}), &Skin::set_bind_count);
    MethodBinder::bind_method(D_METHOD("get_bind_count"), &Skin::get_bind_count);

    MethodBinder::bind_method(D_METHOD("add_bind", {"bone", "pose"}), &Skin::add_bind);

    MethodBinder::bind_method(D_METHOD("set_bind_pose", {"bind_index", "pose"}), &Skin::set_bind_pose);
    MethodBinder::bind_method(D_METHOD("get_bind_pose", {"bind_index"}), &Skin::get_bind_pose);

    MethodBinder::bind_method(D_METHOD("set_bind_bone", {"bind_index", "bone"}), &Skin::set_bind_bone);
    MethodBinder::bind_method(D_METHOD("get_bind_bone", {"bind_index"}), &Skin::get_bind_bone);

    MethodBinder::bind_method(D_METHOD("clear_binds"), &Skin::clear_binds);
}

Skin::Skin() {
    bind_count = 0;
    binds_ptr = nullptr;
}
