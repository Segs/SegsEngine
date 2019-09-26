#pragma once

#include "core/vector.h"
#include "core/pool_vector.h"

class ImageLoader;
class String;
class ImageFormatSaver;
class FileAccess;
class Image;
template<class T>
class Ref;
//TODO: SEGS- convert ImageSaver to singelton, so we can have a single initialization point at which to register plugin resolver
class GODOT_EXPORT ImageSaver {
public:
    static void register_plugin_resolver();
    static Error save_image(const String& p_file,const Ref<Image> &p_image, FileAccess *p_custom = nullptr, float p_quality = 1.0);
    static Error save_image(const String& ext, const Ref<Image> & p_image, PoolVector<uint8_t> &tgt, float p_quality = 1.0);

    static void get_recognized_extensions(Vector<String> *p_extensions);
    static ImageFormatSaver *recognize(const String &p_extension);

    static void add_image_format_saver(ImageFormatSaver *p_loader);
    static void remove_image_format_saver(ImageFormatSaver *p_loader);

    static const Vector<ImageFormatSaver *> &get_image_format_savers();

    static void cleanup();
};
