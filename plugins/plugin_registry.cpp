#include "plugin_registry.h"

#include "core/io/image_loader.h"
#include "core/io/image_saver.h"
#include "core/os/os.h"
#include "core/io/resource_saver.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/print_string.h"

#include "static_plugin_registry.inc"

#include <QDirIterator>
#include <QDebug>
#include <QCoreApplication>

static Ref<ResourceFormatSaver> s_generic_saver;
struct PluginHolder {
    QList<QPluginLoader *> s_plugins;
    ~PluginHolder() {
        unloadAll();
    }
    void unloadAll()
    {
        for(QPluginLoader *l : s_plugins)
        {
            QObject *ob = l->instance();
            auto image_loader_interface = qobject_cast<ImageFormatLoader *>(ob);
            if (image_loader_interface) {
                ImageLoader::remove_image_format_loader(image_loader_interface);
            }
            auto image_saver_interface = qobject_cast<ImageFormatSaver *>(ob);
            if(image_saver_interface) {
                ImageSaver::remove_image_format_saver(image_saver_interface);
            }
            l->unload();
            delete l;
        }
        s_plugins.clear();
    }
    bool addPlugin(const QString &path)
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
        s_plugins.push_back(plugin_loader);
        auto image_loader_interface = qobject_cast<ImageFormatLoader *>(ob);
        if (image_loader_interface) {
            print_line(String("Adding image loader:")+ob->metaObject()->className());
            ImageLoader::add_image_format_loader(image_loader_interface);
        }
        auto image_saver_interface = qobject_cast<ImageFormatSaver *>(ob);
        if(image_saver_interface) {
            print_line(String("Adding image saver:")+ob->metaObject()->className());
            ImageSaver::add_image_format_saver(image_saver_interface);
        }
        return true;
    }
};
static PluginHolder s_plugins;

static void load_image_loader_plugins() {
    print_line("Retrieving statically linked plugins");
    auto ents = QPluginLoader::staticPlugins();
    for (QObject *ob : QPluginLoader::staticInstances()) {
        auto image_loader_interface = qobject_cast<ImageFormatLoader *>(ob);
        if (image_loader_interface) {
            print_line(ob->metaObject()->className());
            ImageLoader::add_image_format_loader(image_loader_interface);
        }
        auto image_saver_interface = qobject_cast<ImageFormatSaver *>(ob);
        if(image_saver_interface) {
            ImageSaver::add_image_format_saver(image_saver_interface);
        }
    }
    String exepath = OS::get_singleton()->get_executable_path();
    auto z = QFileInfo(exepath).path();
    QDir plugins_dir(z+"/plugins");
    QCoreApplication::addLibraryPath( z+"/plugins" );
    for (QString filename : plugins_dir.entryList(QDir::Files)) {
        qDebug() << "Filename: " << filename;

        if (!filename.contains("plugin",Qt::CaseInsensitive))
            continue;

        qDebug() << "Attempting to load: " << plugins_dir.absoluteFilePath(filename);

        s_plugins.addPlugin(plugins_dir.absoluteFilePath(filename));
    }
}

void load_all_plugins(const char *plugin_paths)
{
    load_image_loader_plugins();

    // add generic resource saver, based on ImageSaver and friends
    if(!s_generic_saver.is_valid())
    {
        s_generic_saver.instance();
    }
    ResourceSaver::add_resource_format_saver(s_generic_saver);
}
void unload_plugins()
{
    // NOOP for now
    ResourceSaver::remove_resource_format_saver(s_generic_saver);
    s_generic_saver.unref();
    s_plugins.unloadAll();
}
