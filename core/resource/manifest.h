/* http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License.
 * See LICENSE.md for details.
*/
#pragma once

#include "core/resource.h"
#include "core/map.h"
#include "core/string.h"

struct ManifestEntry {
    String uuid;
    String file_path;
};

class GODOT_EXPORT ResourceManifest : public Resource {

    GDCLASS(ResourceManifest,Resource)

    RES_BASE_EXTENSION("manifest")

    String m_path;
    Vector<ManifestEntry> m_entries;
public:

    // Resource interface
    void reload_from_file() override;
protected:
    Error load_manifest(StringView p_path);
};
