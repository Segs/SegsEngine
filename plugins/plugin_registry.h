#pragma once
#include "plugin_registry_interface.h"
#include <QPluginLoader>
#include <QVector>

class PluginRegistry {
    QVector<ResolverInterface *> m_plugin_resolvers;
    QVector<QPluginLoader *> dynamic_plugin_loaders;
    QVector<QObject *> static_plugins;
    QHash<QPair<ResolverInterface *,QObject *>,bool> m_loaded;
public:
    ~PluginRegistry();
    void unloadAll();
    bool add_plugin(const QString &path);
    void add_static_plugin(QObject *plug) { assert(!static_plugins.contains(plug)); static_plugins.push_back(plug); }
    void add_resolver(ResolverInterface *);
    void resolve_plugins();
};
