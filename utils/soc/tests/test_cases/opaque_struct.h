#pragma once

#include "core/reflection_macros.h"
#include "core/object.h"
#include "core/reference.h"

#include <stdint.h>

struct OpaqueStruct {
    int data;
};
SE_OPAQUE_TYPE(OpaqueStruct)
