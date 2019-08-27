#pragma once

#include "core/list.h"
#include "core/plugin_interfaces/load_params.h"
#include "core/service_interfaces/CoreInterface.h"
#include "core/image_data.h"
#include <QObject>

template <class T>
class Ref;
template <class T>
class PoolVector;
class Image;
enum Error : int;
class FileAccess;
class String;
struct ImageData;

class ImageFormatLoader {
    friend class ImageLoader;
    friend class ResourceFormatLoaderImage;

public:
    virtual Error load_image(ImageData &p_image, FileAccess *p_fileaccess, LoadParams params={}) = 0;
	//! Default implementation that operates on raw memory, can be overriden by the plugin to provide a more efficient implementation.
	virtual Error load_image(ImageData &p_image, const uint8_t *data, int size, LoadParams params={})
	{
		FileAccess *fa = getCoreInterface()->wrapMemoryAsFileAccess(data,size);
		if(!fa)
			return ERR_CANT_OPEN;
		Error res = this->load_image(p_image,fa,params);
		getCoreInterface()->releaseFileAccess(fa);
		return res;
	}
    virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;

public:
	virtual ~ImageFormatLoader() {}
};
#define ImageFormatLoader_iid "org.godot.ImageFormatLoader"

Q_DECLARE_INTERFACE(ImageFormatLoader, ImageFormatLoader_iid)

class ImageFormatSaver {
    friend class ImageSaver;
public:
    virtual Error save_image(const ImageData &p_image, PoolVector<uint8_t> &tgt, SaveParams params) = 0;
    virtual Error save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params) = 0;
    virtual bool can_save(const String &extension)=0; // support for multi-format plugins
    virtual void get_saved_extensions(List<String> *p_extensions) const = 0;
public:
	virtual ~ImageFormatSaver() {}
};
#define ImageFormatSaver_iid "org.godot.ImageFormatSaver"

Q_DECLARE_INTERFACE(ImageFormatSaver, ImageFormatSaver_iid)
