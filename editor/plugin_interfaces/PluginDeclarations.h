#pragma once

#include "editor/plugin_interfaces/EditorSceneImporterInterface.h"
#include "editor/plugin_interfaces/EditorExportPlatformInterface.h"
#include <QObject>

#define EditorSceneImporterInterface_iid "org.segs_engine.editor.SceneImporterInterface"

Q_DECLARE_INTERFACE(EditorSceneImporterInterface, EditorSceneImporterInterface_iid)

#define EditorSceneExporterInterface_iid "org.segs_engine.editor.SceneExporterInterface"

Q_DECLARE_INTERFACE(EditorSceneExporterInterface, EditorSceneExporterInterface_iid)
#define EditorPlatformExportInterface_iid "org.segs_engine.editor.PlatformExportInterface"

Q_DECLARE_INTERFACE(EditorPlatformExportInterface, EditorPlatformExportInterface_iid)
