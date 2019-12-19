#pragma once

#include "core/plugin_interfaces/PluginDeclarations.h"

class PackedSourcePCK : public QObject, public PackSourceInterface {
    Q_PLUGIN_METADATA(IID "org.godot.PackSourcePCK")
    Q_INTERFACES(PackSourceInterface)
    Q_OBJECT
public:
    bool try_open_pack(se_string_view p_path, bool p_replace_files) override;
    FileAccess *get_file(se_string_view p_path, PackedDataFile *p_file) override;
};
