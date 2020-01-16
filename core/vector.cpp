#include "core/vector.h"
#include "core/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/variant.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<uint8_t,wrap_allocator>;

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) Vector<UIString>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) Vector<Variant>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<struct Vector2>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<struct Vector3>;

const Vector<Variant> null_variant_vec;
const PODVector<Variant> null_variant_pvec;
