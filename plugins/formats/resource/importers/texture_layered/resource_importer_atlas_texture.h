#pragma once

#include "layered_texture_impl.h"

class ResourceImporter3DTexture : public QObject, public LayeredTextureImpl {
    Q_PLUGIN_METADATA(IID "org.godot.Texture3DImporter")
    Q_INTERFACES(ResourceImporterInterface)
    Q_OBJECT
public:
    ResourceImporter3DTexture()
    {
        set_3d(true);
    }

    float get_priority() const override { return 13.0f; }
};
