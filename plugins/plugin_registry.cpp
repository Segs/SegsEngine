#include "plugin_registry.h"
#include "plugin_registry_interface.h"

#include "core/os/os.h"
#include "core/io/resource_saver.h"
#include "core/plugin_interfaces/PluginDeclarations.h"
#include "core/print_string.h"

#include <QDirIterator>
#include <QDebug>
#include <QCoreApplication>
#include <QPluginLoader>

static PluginRegistry s_common_plugins;

extern void get_static_plugins(PluginRegistry &reg);

void add_plugin_resolver(ResolverInterface *r) {
    s_common_plugins.add_resolver(r);
}

static void load_all_plugins() {

    print_line("Retrieving statically linked plugins");
    get_static_plugins(s_common_plugins);

    print_line("Retrieving dynamically linked plugins");
    String exepath = OS::get_singleton()->get_executable_path();
    auto base_path = QFileInfo(exepath.m_str).path();
    QDir plugins_dir(base_path+"/plugins");
    QCoreApplication::addLibraryPath( base_path+"/plugins" );
    for (const QString &filename : plugins_dir.entryList(QDir::Files)) {
        qDebug() << "Filename: " << filename;

        if (!filename.contains(QLatin1String("plugin"),Qt::CaseInsensitive))
            continue;

        qDebug() << "Attempting to load: " << plugins_dir.absoluteFilePath(filename);
        s_common_plugins.add_plugin(plugins_dir.absoluteFilePath(filename));
    }
    s_common_plugins.resolve_plugins();

}
void load_all_plugins(const char *plugin_paths)
{
    load_all_plugins();
}
void unload_plugins()
{
    s_common_plugins.unloadAll();
}

PluginRegistry::~PluginRegistry() {
    unloadAll();
    for(auto r : m_plugin_resolvers)
        delete r;
    m_plugin_resolvers.clear();
}

void PluginRegistry::unloadAll()
{
    for(QPluginLoader *l : dynamic_plugin_loaders)
    {
        QObject *ob = l->instance();
        for(ResolverInterface * r : m_plugin_resolvers)
        {
            r->plugin_removed(ob);
        }
        l->unload();
        delete l;
    }
    dynamic_plugin_loaders.clear();
    m_loaded.clear();
}


bool PluginRegistry::add_plugin(const QString &path)
{
    QPluginLoader *plugin_loader = new QPluginLoader;

    plugin_loader->setLoadHints(QLibrary::ExportExternalSymbolsHint);
    plugin_loader->setFileName(path);

    QObject *ob = plugin_loader->instance();
    if(!ob)
    {
        QString problem = plugin_loader->errorString();
        qDebug() << "Plugin load problem: " << problem;
        delete plugin_loader;
        return false;
    }

    dynamic_plugin_loaders.push_back(plugin_loader);
    bool used=false;
    for(ResolverInterface *r : m_plugin_resolvers)
    {
        used |= r->new_plugin_detected(ob);
    }
    if(!used)
        qDebug() << "Plugin loaded but no resolver can use it." << path;
    return true;
}

void PluginRegistry::add_resolver(ResolverInterface *r)
{
    m_plugin_resolvers.push_back(r);

    resolve_plugins();
}

void PluginRegistry::resolve_plugins()
{
    for(auto r : m_plugin_resolvers)
    {
        for(QObject *ob : static_plugins)
        {
            if(m_loaded.contains({r,ob}))
                continue;
            r->new_plugin_detected(ob);
            m_loaded[{r,ob}]=true;
        }
        for(QPluginLoader *ob : dynamic_plugin_loaders)
        {
            if(m_loaded.contains({r,ob->instance()}))
                continue;
            r->new_plugin_detected(ob->instance());
            m_loaded[{r,ob->instance()}]=true;
        }
    }
}
