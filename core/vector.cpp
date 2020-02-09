#include "core/vector.h"
#include "core/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/variant.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<uint8_t,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<int,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<float,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Vector2,wrap_allocator>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<Vector3,wrap_allocator>;

const PODVector<Variant> null_variant_pvec;
const PODVector<Vector2> null_vec2_pvec;
const PODVector<Vector3> null_vec3_pvec;
const PODVector<int> null_int_pvec;
const PODVector<float> null_float_pvec;
