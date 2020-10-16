#pragma once
class QJsonObject;
class QObject;

struct ResolverInterface
{
    /**
     * @brief new_plugin_detected is called with a QObject providing an unknown set of plugin interfaces,
     * the role of this function is to register all interfaces in their proper places in the engine/editor/game/etc.
     * \a metadata contains additional plugin information ( version/dependencies etc. )
     * \a path optional parameter containing full path to the plugin.
     * @return true if at least one of the interfaces provided by the plugin was resolved
     */
    virtual bool new_plugin_detected(QObject *, const QJsonObject &metadata, const char *path=nullptr)=0;
    virtual void plugin_removed(QObject *)=0;
    virtual ~ResolverInterface() = default;
};
/**
 * @brief add_plugin_resolver registers a new resolver object with the plugin system.
 * On registration, the resolver will be informed about all currently registered plugins and in the future,
 * it will be notified about plugin removals/additions.
 * @param r is a pointer to the resolver, plugin registry takes ownership of this object
 */
void add_plugin_resolver(ResolverInterface *r);

void load_all_plugins(const char *plugin_paths);
void unload_plugins();
void remove_all_resolvers();
