/*
 * SEGS - Super Entity Game Server Engine
 * http://www.segs.dev/
 * Copyright (c) 2006 - 2020 SEGS Team (see AUTHORS.md)
 * This software is licensed under the terms of the 3-clause BSD License. See LICENSE_SEGS.md for details.
 */
#pragma once


#include "core/plugin_interfaces/PluginDeclarations.h"

class SceneMapModule : public QObject, public ModuleInterface {

    Q_PLUGIN_METADATA(IID "org.segs_engine.SceneMap")
    Q_INTERFACES(ModuleInterface)
    Q_OBJECT

public:
    ~SceneMapModule() override {}

    // ModuleInterface interface
public:
    bool register_module() override;
    void unregister_module() override;
};
