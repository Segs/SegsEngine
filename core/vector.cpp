#include "core/vector.h"
#include "core/ustring.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"

template class GODOT_EXPORT_TEMPLATE_B eastl::vector<uint8_t,wrap_allocator>;
GODOT_TEMPLATE_EXT_DEFINE(Vector<String>)
GODOT_TEMPLATE_EXT_DEFINE(Vector<Vector2>)
GODOT_TEMPLATE_EXT_DEFINE(Vector<Vector3>)
