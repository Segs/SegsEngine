#pragma once

#include "core/plugin_interfaces/PluginDeclarations.h"

class PackedSourcePCK : public QObject, public PackSourceInterface {
    Q_PLUGIN_METADATA(IID "org.segs_engine.PackSourcePCK")
    Q_INTERFACES(PackSourceInterface)
    Q_OBJECT
public:
    bool try_open_pack(StringView p_path, bool p_replace_files, StringView p_destination="") override;
    FileAccess *get_file(StringView p_path, PackedDataFile *p_file) override;
};
