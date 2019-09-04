#pragma once

#include "core/service_interfaces/CoreInterface.h"

class Image;
enum Error : int;
class FileAccess;
class String;
class DefaultAllocator;
class Node;
struct Animation;
struct ImageData;

template <class T>
class Ref;
template <class T>
class PoolVector;
template <class T, class A>
class List;

class EditorSceneImporterInterface {
    friend class ImageLoader;
    friend class ResourceFormatLoaderImage;

public:
    virtual uint32_t get_import_flags() const=0;
    virtual void get_extensions(List<String,DefaultAllocator> *p_extensions) const = 0;
    virtual Node *import_scene(const String &p_path, uint32_t p_flags, int p_bake_fps, List<String,DefaultAllocator> *r_missing_deps, Error *r_err = nullptr)=0;
    virtual Ref<Animation> import_animation(const String &p_path, uint32_t p_flags, int p_bake_fps)=0;

public:
	virtual ~EditorSceneImporterInterface() {}
};


class EditorSceneExporterInterface {
//    friend class ImageSaver;
public:
    virtual bool can_save(const String &extension)=0; // support for multi-format plugins
    virtual void get_extensions(List<String,DefaultAllocator> *p_extensions) const = 0;
public:
	virtual ~EditorSceneExporterInterface() {}
};
