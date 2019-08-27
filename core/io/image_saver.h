#pragma once

#include "core/image.h"
#include "core/io/resource_loader.h"
#include "core/list.h"
#include "core/os/file_access.h"


class ImageLoader;
class String;
class ImageFormatSaver;

class ImageSaver {

    static Vector<ImageFormatSaver *> savers;

protected:
public:
    static Error save_image(String p_file,const Ref<Image> &p_image, FileAccess *p_custom = nullptr, float p_quality = 1.0);
    static Error save_image(String ext, const Ref<Image> & p_image, PoolVector<uint8_t> &tgt, float p_quality = 1.0);

    static void get_recognized_extensions(List<String> *p_extensions);
    static ImageFormatSaver *recognize(const String &p_extension);

    static void add_image_format_saver(ImageFormatSaver *p_loader);
    static void remove_image_format_saver(ImageFormatSaver *p_loader);

    static const Vector<ImageFormatSaver *> &get_image_format_savers();

    static void cleanup();
};
