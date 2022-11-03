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

void load_all_plugins(const char *plugin_paths)
{
    print_line("Retrieving statically linked plugins");
    get_static_plugins(s_common_plugins);
    print_line("Finding dynamically loadable plugins");

    QString base_path(plugin_paths); //auto base_path = QFileInfo(OS::get_singleton()->get_executable_path().c_str()).path();

    QCoreApplication::addLibraryPath( base_path);
    print_line(qPrintable(QString("Retrieving dynamically linked plugins from:")+base_path));

    QDir plugins_dir(base_path);
    QDirIterator iter(base_path,QDir::NoDot|QDir::AllEntries,QDirIterator::Subdirectories);

    while(iter.hasNext()) {
        QString filename = iter.next();
        QFileInfo fi=iter.fileInfo();
        if(!fi.isFile())
            continue;

        if(fi.suffix()!="dll" && fi.suffix()!="so")
            continue;

        s_common_plugins.add_plugin(plugins_dir.absoluteFilePath(filename));
    }
    s_common_plugins.resolve_plugins();
}
void unload_plugins()
{
    s_common_plugins.unloadAll();
}

void remove_all_resolvers()
{
    s_common_plugins.removeAllResolvers();
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
        used |= r->new_plugin_detected(ob,plugin_loader->metaData(),qPrintable(path));
    }
    // if(!used) {
    //     qDebug().noquote() << "Plugin loaded but no resolver can use it." << path;
    // }
    return true;
}

void PluginRegistry::add_resolver(ResolverInterface *r)
{
    m_plugin_resolvers.push_back(r);

    resolve_plugins(r);
}

void PluginRegistry::resolve_plugins(ResolverInterface *specific) {
    if (!specific) {
        for (auto r : m_plugin_resolvers) {
            resolve_plugins(r); // we recursively call ourselves with specific resolver.
        }
        return;
    }

    ERR_FAIL_COND(!m_plugin_resolvers.contains(specific));
    for (const QStaticPlugin &plug : static_plugins) {
        QObject *ob = plug.instance();
        if (m_loaded.contains({ specific, ob }))
            continue;
        specific->new_plugin_detected(ob, plug.metaData());
        m_loaded[{ specific, ob }] = true;
    }
    for (QPluginLoader *ob : dynamic_plugin_loaders) {
        if (m_loaded.contains({ specific, ob->instance() }))
            continue;
        specific->new_plugin_detected(ob->instance(), ob->metaData(),qPrintable(ob->fileName()));
        m_loaded[{ specific, ob->instance() }] = true;
    }
}

void PluginRegistry::removeAllResolvers() {
    unloadAll();
    for (auto r : m_plugin_resolvers)
        delete r;
    m_plugin_resolvers.clear();
}
