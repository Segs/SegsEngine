#include "core/vector.h"
#include "core/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/variant.h"
#include "core/list.h"
#include "core/string_name.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<uint8_t,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<int,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<float,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Vector2,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Vector3,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<StringName,wrap_allocator>;

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::list<StringName,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::list<se_string_view,wrap_allocator>;

const Vector<Variant> null_variant_pvec;
const Vector<Vector2> null_vec2_pvec;
const Vector<Vector3> null_vec3_pvec;
const Vector<int> null_int_pvec;
const Vector<float> null_float_pvec;
