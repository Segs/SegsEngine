#pragma once

#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/plugin_interfaces/ResourceImporterInterface.h"
#include "core/plugin_interfaces/ResourceLoaderInterface.h"
#include "core/plugin_interfaces/PackSourceInterface.h"

#define ImageFormatLoader_iid "org.godot.ImageFormatLoader"

Q_DECLARE_INTERFACE(ImageFormatLoader, ImageFormatLoader_iid)

#define ImageFormatSaver_iid "org.godot.ImageFormatSaver"

Q_DECLARE_INTERFACE(ImageFormatSaver, ImageFormatSaver_iid)

#define ResourceImporterInterface_iid "org.godot.editor.ResourceImporterInterface"

Q_DECLARE_INTERFACE(ResourceImporterInterface, ResourceImporterInterface_iid)

#define ResourceLoaderInterface_iid "org.godot.editor.ResourceLoaderInterface"

Q_DECLARE_INTERFACE(ResourceLoaderInterface, ResourceLoaderInterface_iid)

#define PackSourceInterface_iid "org.godot.editor.PackSourceInterface"

Q_DECLARE_INTERFACE(PackSourceInterface, PackSourceInterface_iid)
