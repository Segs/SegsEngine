/* http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License.
 * See LICENSE.md for details.
*/
#pragma once

#include "core/resource.h"
#include "core/map.h"
#include "core/string.h"

class GODOT_EXPORT ResourceManifest : public Resource {

    GDCLASS(ResourceManifest,Resource)

    RES_BASE_EXTENSION("manifest")
public:
};
