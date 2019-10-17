#include "core/vector.h"
#include "core/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) eastl::vector<uint8_t,wrap_allocator>;

template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) Vector<String>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) Vector<struct Vector2>;
template class EXPORT_TEMPLATE_DEFINE(GODOT_EXPORT) Vector<struct Vector3>;
