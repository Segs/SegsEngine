/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2022 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#pragma once


#include "editor/plugin_interfaces/PluginDeclarations.h"

class OsxProjectExportPlugin : public QObject, public EditorPlatformExportInterface {

    Q_PLUGIN_METADATA(IID "org.segs_engine.editor.OsxProjectExportPlugin")
    Q_INTERFACES(EditorPlatformExportInterface)
    Q_OBJECT

    EditorExportPlatform* m_platform = nullptr;
public:
    ~OsxProjectExportPlugin() override {}
    bool is_supported() override;
    bool create_and_register_exporter(EditorExportPlatform*) override;
    EditorExportPlatform* platform() override { return m_platform; }
    void unregister_exporter(EditorExportPlatform*) override;
};
