#pragma once

#include "core/godot_export.h"

#cmakedefine OPTION_PRECISE_MATH_CHECKS

#ifdef DEBUG_ENABLED
#define MATH_CHECKS
#endif
