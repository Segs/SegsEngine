/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2022 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#include "osx_exporter_plugin.h"

bool OsxProjectExportPlugin::is_supported()
{
    return true;
}

void OsxProjectExportPlugin::unregister_exporter(EditorExportPlatform* platform) {
#ifdef TOOLS_ENABLED
    Q_CLEANUP_RESOURCE(osx_exporter);
#endif
}

bool OsxProjectExportPlugin::create_and_register_exporter(EditorExportPlatform* platform) {
    m_platform = platform;
#ifdef TOOLS_ENABLED
    Q_INIT_RESOURCE(osx_exporter);
#endif
    return true;
}


