#pragma once

#include "core/plugin_interfaces/ImageLoaderInterface.h"

#define ImageFormatLoader_iid "org.godot.ImageFormatLoader"

Q_DECLARE_INTERFACE(ImageFormatLoader, ImageFormatLoader_iid)

#define ImageFormatSaver_iid "org.godot.ImageFormatSaver"

Q_DECLARE_INTERFACE(ImageFormatSaver, ImageFormatSaver_iid)
