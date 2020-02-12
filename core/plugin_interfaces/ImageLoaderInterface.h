#pragma once

#include "core/plugin_interfaces/load_params.h"
#include "core/service_interfaces/CoreInterface.h"
#include "core/error_list.h"
#include "core/forward_decls.h"

class Image;
enum Error : int;
class FileAccess;
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
    virtual void get_recognized_extensions(Vector<String> &p_extensions) const = 0;
    virtual void set_loader_option(int /*option_id*/,void * /*option_var*/) {}
public:
    virtual ~ImageFormatLoader() = default;
    ImageFormatLoader() = default;
    // Not copyable.
    ImageFormatLoader(const ImageFormatLoader &) = delete;
    ImageFormatLoader &operator=(const ImageFormatLoader &) = delete;
};

class ImageFormatSaver {
    friend class ImageSaver;
public:
    virtual Error save_image(const ImageData &p_image, Vector<uint8_t> &tgt, SaveParams params) = 0;
    virtual Error save_image(const ImageData &p_image, FileAccess *p_fileaccess, SaveParams params) = 0;
    virtual bool can_save(se_string_view extension)=0; // support for multi-format plugins
    virtual void get_saved_extensions(Vector<String> &p_extensions) const = 0;
public:
    virtual ~ImageFormatSaver() = default;
    ImageFormatSaver() = default;

    // Not copyable.
    ImageFormatSaver(const ImageFormatSaver &) = delete;
    ImageFormatSaver &operator=(const ImageFormatSaver &) = delete;
};

class ImageCodecInterface {
    friend class ImageSaver;
public:
    virtual Error compress_image(Image *p_image, CompressParams params) = 0;
    virtual Error decompress_image(Image *p_image) = 0;
    virtual void fill_modes(Vector<int> &) const = 0;
public:
    virtual ~ImageCodecInterface() = default;
    ImageCodecInterface() = default;

    // Not copyable.
    ImageCodecInterface(const ImageCodecInterface &) = delete;
    ImageCodecInterface &operator=(const ImageCodecInterface &) = delete;
};
