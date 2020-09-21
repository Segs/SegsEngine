#pragma once
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/plugin_interfaces/ResourceImporterInterface.h"
#include "core/plugin_interfaces/ResourceLoaderInterface.h"
#include "core/plugin_interfaces/PackSourceInterface.h"
#include "core/plugin_interfaces/ModuleInterface.h"
#include "core/plugin_interfaces/ScriptingGlueInterface.h"
#include <QtCore/QObject>

#define ImageFormatLoader_iid "org.segs_engine.ImageFormatLoader"

Q_DECLARE_INTERFACE(ImageFormatLoader, ImageFormatLoader_iid)

#define ImageFormatSaver_iid "org.segs_engine.ImageFormatSaver"

Q_DECLARE_INTERFACE(ImageFormatSaver, ImageFormatSaver_iid)

#define ImageCodecInterface_iid "org.segs_engine.ImageCodec"

Q_DECLARE_INTERFACE(ImageCodecInterface, ImageCodecInterface_iid)

#define ResourceImporterInterface_iid "org.segs_engine.editor.ResourceImporterInterface"

Q_DECLARE_INTERFACE(ResourceImporterInterface, ResourceImporterInterface_iid)

#define ResourceLoaderInterface_iid "org.segs_engine.editor.ResourceLoaderInterface"

Q_DECLARE_INTERFACE(ResourceLoaderInterface, ResourceLoaderInterface_iid)

#define PackSourceInterface_iid "org.segs_engine.editor.PackSourceInterface"

Q_DECLARE_INTERFACE(PackSourceInterface, PackSourceInterface_iid)

#define Module_iid "org.segs_engine.Module"

Q_DECLARE_INTERFACE(ModuleInterface, Module_iid)

#define ScriptingGlue_iid "org.segs_engine.ScriptingGlue"

Q_DECLARE_INTERFACE(ScriptingGlueInterface, ScriptingGlue_iid)
