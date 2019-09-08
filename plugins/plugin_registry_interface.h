#pragma once
struct ResolverInterface
{
    /**
     * @brief newPluginDetected this method is called with a QObject providing an unkown set of plugin interfaces,
     * the role of this function is to register all interfaces in their proper places in the engine/editor/game/etc.
     * @return true if at least one of the interfaces provided by the plugin was resolved
     */
    virtual bool new_plugin_detected(class QObject *)=0;
    virtual void plugin_removed(class QObject *)=0;
    virtual ~ResolverInterface() = default;
};
/**
 * @brief add_plugin_resolver will provide the plugin system with a functor that will be called for all known plugins
 * When a new resolver is registered for the first time, it will be called with all already known plugin instances. And for all plugins detected afterwards.
 * @param r is a pointer to the resolver, plugin registry takes ownership of this object
 */
void add_plugin_resolver(ResolverInterface *r);

void load_all_plugins(const char *plugin_paths);
void unload_plugins();
