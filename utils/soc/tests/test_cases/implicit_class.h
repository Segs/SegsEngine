#pragma once

#include "core/reflection_macros.h"
#include "core/object.h"
#include "core/reference.h"

#include <stdint.h>
class EditorSelection;
class Object;
template<class T>
class Ref;

class Bar {
    SE_CLASS()

    SE_INVOCABLE EditorSelection *get_selection();
    SE_INVOCABLE void inspect_object(Object *p_obj, StringView p_for_property = StringView(), bool p_inspector_only = false);
    SE_INVOCABLE virtual void v_func();
    SE_INVOCABLE static void s_func();
};

