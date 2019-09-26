#pragma once

#include "core/plugin_interfaces/PluginDeclarations.h"

class PackedSourcePCK : public QObject, public PackSourceInterface {
    Q_PLUGIN_METADATA(IID "org.godot.PackSourcePCK")
    Q_INTERFACES(PackSourceInterface)
    Q_OBJECT
public:
    bool try_open_pack(const String &p_path) override;
    FileAccess *get_file(const String &p_path, PackedDataFile *p_file) override;
};
