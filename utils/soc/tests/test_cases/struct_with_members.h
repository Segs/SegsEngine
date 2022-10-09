#pragma once

#include "core/reflection_macros.h"

struct Fielder {
    SE_CLASS(struct)
    int simple_field;
    String array_field[12];
    Ref<Texture> simple_templated_field;
    Vector<Ref<Resource>> nested_template_field;
    const Map<String *,Ref<Resource>> & const_ref_template_field;
};

