#include "reflection_walker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QBuffer>
#include <functional>

extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }
extern "C" int __lsan_is_turned_off() { return 1; }

QStringList loadModuleDefinition(ModuleConfig& tgt, const QString &srcfile) {
    QStringList top_directories;
    QFile src(srcfile);

    if (!src.open(QFile::ReadOnly)) {
        return top_directories;
    }

    QByteArray data(src.readAll());
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return top_directories;
    QJsonObject root(doc.object());
    tgt.module_name = root["name"].toString().toUtf8();
    tgt.version = root["version"].toString().toUtf8();
    tgt.api_version = root["api_version"].toString().toUtf8();
    QJsonArray dirs = root["directories"].toArray();
    for (const auto& dir : dirs) {
        top_directories.append(dir.toString().toUtf8());
    }
    return top_directories;
}
bool processModuleDef(QString path,QString default_ns) {
    ModuleConfig mod;
    QStringList top_directories= loadModuleDefinition(mod, path);
    if (top_directories.empty()) {
        return false;
    }
    QString file_path = QFileInfo(path).path();
    QDir::setCurrent(file_path);
    auto zz = QDir::currentPath();
    for (const QString& root : top_directories) {
        QDirIterator iter(root, QStringList({ "*.h" }), QDir::NoFilter, QDirIterator::Subdirectories);
        while (iter.hasNext()) {
            QFile fl(iter.next());
            if (!fl.open(QFile::ReadOnly)) {
                qCritical() << "Failed to open file" << fl;
                continue;
            }
            if (!processHeader(fl.fileName(), &fl)) {
                qCritical() << "Error while processing file" << fl.fileName();
                return false;
            }
        }
    }

    mod.default_ns = default_ns;
    setConfig(mod);

    return true;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("soc");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Segs Object Compiler");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("source", "Module definition file or a single header.");


    parser.addOptions({
        {{"n", "namespace"}, "Use the provided namespace as default when no other is provided/defined.", "namespace"},
        {{"o", "output_dir"}, "Put generated files in the provided directory.", "output_dir"},
        {{"j", "json"}, "Produce reflection interchange file."},
        {{"c", "cpp"}, "Produce helper cpp."},
    });

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.empty()) {
        return 0;
    }

    initContext();
    bool produce_json = parser.isSet("j");
    bool produce_cpp = parser.isSet("c");

    QString default_ns = parser.value("namespace");
    if(default_ns.isEmpty()) {
        default_ns = "Godot";
    }
    QString output_dir = parser.value("output_dir");
    if(output_dir.isEmpty()) {
        output_dir = ".";
    }


    ModuleConfig config;
    config.default_ns = default_ns;
    setConfig(config);
    //NOTE: Simplified parser doesn't handle '{' and '}' embedded within strings, check if input contains such
    for(const QString& arg : args ) {
        if(arg.endsWith("json")) {
            if(!processModuleDef(arg,default_ns))
                return -1;
        }
        if(arg.endsWith(".h")) {
            if(!QFile::exists(arg))
                return -1;
            QFile src_file(arg);
            if (!src_file.open(QFile::ReadOnly)) {
                qCritical() << "Failed to open source file:" << arg;
                return -1;
            }
            if(!processHeader(arg,&src_file)) {
                qDebug() << arg << false;
                return -1;
            }
        }
        if(produce_cpp)
        {
            QFile tgtFile(output_dir+"/"+QFileInfo(arg).baseName() + "_soc.cpp");
            if (!tgtFile.open(QIODevice::WriteOnly | QIODevice::Text))
                return -1;
            exportCpp(&tgtFile);
        }
        if(produce_json)
        {
            QFile tgtFile(output_dir+"/"+QFileInfo(arg).baseName() + "_rfl.json");
            if (!tgtFile.open(QIODevice::WriteOnly | QIODevice::Text))
                return -1;
            exportJson(&tgtFile);
        }
    }
    return 0;
}
