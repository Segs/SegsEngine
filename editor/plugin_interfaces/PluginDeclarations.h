#pragma once

#include "editor/plugin_interfaces/EditorSceneImporterInterface.h"

#include <QObject>

#define EditorSceneImporterInterface_iid "org.godot.EditorSceneImporterInterface"

Q_DECLARE_INTERFACE(EditorSceneImporterInterface, EditorSceneImporterInterface_iid)

#define EditorSceneExporterInterface_iid "org.godot.EditorSceneExporterInterface"

Q_DECLARE_INTERFACE(EditorSceneExporterInterface, EditorSceneExporterInterface_iid)

