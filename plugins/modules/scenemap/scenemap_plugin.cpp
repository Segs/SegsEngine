/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#include "scenemap_plugin.h"

#include "scene_map.h"

#include "core/class_db.h"
#ifdef TOOLS_ENABLED
#include "scene_map_editor_plugin.h"
#include "editor/editor_plugin.h"
#endif

bool SceneMapModule::register_module()
{
#ifndef _3D_DISABLED
    ClassDB::register_class<SceneMap>();
#ifdef TOOLS_ENABLED
    Q_INIT_RESOURCE(scenemap);
    SceneMapEditor::initialize_class();
    EditorPlugins::add_by_type<SceneMapEditorPlugin>();
#endif
#endif

    return false;
}

void SceneMapModule::unregister_module()
{

}

