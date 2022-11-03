/*************************************************************************/
/*  resource_importer_texture.cpp                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "resource_importer_texture.h"

#include "core/dictionary.h"
#include "core/print_string.h"
#include "core/io/config_file.h"
#include "core/io/image_loader.h"
#include "core/io/resource_importer.h"
#include "core/os/mutex.h"
#include "core/project_settings.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "editor/service_interfaces/EditorServiceInterface.h"
#include "scene/resources/texture.h"

void ResourceImporterTexture::_texture_reimport_srgb(StringName p_tex_path) {

    MutexGuard guard(singleton->mutex);

    if (!singleton->make_flags.contains(p_tex_path)) {
        singleton->make_flags[p_tex_path] = 0;
    }

    singleton->make_flags[p_tex_path] |= MAKE_SRGB_FLAG;

}

void ResourceImporterTexture::_texture_reimport_3d(StringName p_tex_path) {

    MutexGuard guard(singleton->mutex);

    if (!singleton->make_flags.contains(p_tex_path)) {
        singleton->make_flags[p_tex_path] = 0;
    }

    singleton->make_flags[p_tex_path] |= MAKE_3D_FLAG;

}

void ResourceImporterTexture::_texture_reimport_normal(StringName p_tex_path) {

    MutexGuard guard(singleton->mutex);

    if (!singleton->make_flags.contains(p_tex_path)) {
        singleton->make_flags[p_tex_path] = 0;
    }

    singleton->make_flags[p_tex_path] |= MAKE_NORMAL_FLAG;

}

void ResourceImporterTexture::build_reconfigured_list(Vector<String> &to_reimport) {

    MutexGuard guard(mutex);

    if (make_flags.empty()) {
        return;
    }

    for (eastl::pair<const StringName, int> &E : make_flags) {

        Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());

        String src_path = String(E.first) + ".import";

        Error err = cf->load(src_path);
        ERR_CONTINUE(err != OK);

        bool changed = false;
        if (E.second & MAKE_SRGB_FLAG && cf->get_value("params", "flags/srgb").as<int>() == 2) {
            cf->set_value("params", "flags/srgb", 1);
            changed = true;
        }

        if (E.second & MAKE_NORMAL_FLAG && cf->get_value("params", "compress/normal_map").as<int>() == 0) {
            print_line(FormatVE(TTR("%s: Texture detected as used as a normal map in 3D. Enabling red-green texture compression to reduce memory usage (blue channel is discarded).").asCString(), E.first.asCString()));

            cf->set_value("params", "compress/normal_map", 1);
            changed = true;
        }

        if (E.second & MAKE_3D_FLAG && cf->get_value("params", "detect_3d").as<bool>()) {
            print_line(FormatVE(TTR("%s: Texture detected as used in 3D. Enabling filter, repeat, mipmap generation and VRAM texture compression.").asCString(), E.first.asCString()));

            cf->set_value("params", "detect_3d", false);
            cf->set_value("params", "compress/mode", 2);
            cf->set_value("params", "flags/repeat", true);
            cf->set_value("params", "flags/filter", true);
            cf->set_value("params", "flags/mipmaps", true);
            changed = true;
        }

        if (changed) {
            cf->save(src_path);
            to_reimport.push_back(String(E.first));
        }
    }

    make_flags.clear();


}

const char *ResourceImporterTexture::get_importer_name() const {

    return "texture";
}

const char *ResourceImporterTexture::get_visible_name() const {

    return "Texture";
}
void ResourceImporterTexture::get_recognized_extensions(Vector<String> &p_extensions) const {

    ImageLoader::get_recognized_extensions(p_extensions);
}
StringName ResourceImporterTexture::get_save_extension() const {
    return "stex";
}

StringName ResourceImporterTexture::get_resource_type() const {

    return "StreamTexture";
}

bool ResourceImporterTexture::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {

    if (p_option == "compress/lossy_quality") {
        int compress_mode = p_options.at("compress/mode").as<int>();
        if (compress_mode != COMPRESS_LOSSY && compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
    } else if (p_option == "compress/hdr_mode") {
        int compress_mode= p_options.at("compress/mode").as<int>();
        if (compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
    } else if (p_option == "compress/normal_map") {
        int compress_mode = p_options.at("compress/mode").as<int>();
        if (compress_mode == COMPRESS_LOSSLESS) {
            return false;
        }
    } else if (p_option == "compress/bptc_ldr") {
        int compress_mode= p_options.at("compress/mode").as<int>();
        if (compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
        if (!ProjectSettings::get_singleton()->getT<bool>("rendering/vram_compression/import_bptc")) {
            return false;
        }
    }

    return true;
}

int ResourceImporterTexture::get_preset_count() const {
    return 4;
}
StringName ResourceImporterTexture::get_preset_name(int p_idx) const {

    static const char *preset_names[] = {
        "2D, Detect 3D",
        "2D",
        "2D Pixel",
        "3D"
    };

    return StaticCString(preset_names[p_idx],true);
}

void ResourceImporterTexture::get_import_options(Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {

    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "compress/mode", PropertyHint::Enum, "Lossless,Lossy,Video RAM,Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == PRESET_3D ? 2 : 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::FLOAT, "compress/lossy_quality", PropertyHint::Range, "0,1,0.01"), 0.7));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "compress/hdr_mode", PropertyHint::Enum, "Enabled,Force RGBE"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "compress/bptc_ldr", PropertyHint::Enum, "Enabled,RGBA Only"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "compress/normal_map", PropertyHint::Enum, "Detect,Enable,Disabled"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "flags/repeat", PropertyHint::Enum, "Disabled,Enabled,Mirrored"), p_preset == PRESET_3D ? 1 : 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/filter"), p_preset != PRESET_2D_PIXEL));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/mipmaps"), p_preset == PRESET_3D));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/anisotropic"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "flags/srgb", PropertyHint::Enum, "Disable,Enable,Detect"), 2));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/fix_alpha_border"), p_preset != PRESET_3D));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/premult_alpha"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/HDR_as_SRGB"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/invert_color"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/normal_map_invert_y"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "stream"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "size_limit", PropertyHint::Range, "0,4096,1"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "detect_3d"), p_preset == PRESET_DETECT));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::FLOAT, "svg/scale", PropertyHint::Range, "0.001,100,0.001"), 1.0));
}

void ResourceImporterTexture::_save_stex(const Ref<Image> &p_image, StringView p_to_path, int p_compress_mode,
        float p_lossy_quality, ImageCompressMode p_vram_compression, bool p_mipmaps, int p_texture_flags,
        bool p_streamable, bool p_detect_3d, bool p_detect_srgb, bool p_force_rgbe, bool p_detect_normal,
        bool p_force_normal, bool p_force_po2_for_compressed) {

    FileAccess *f = FileAccess::open(p_to_path, FileAccess::WRITE);
    ERR_FAIL_NULL(f);
    f->store_8('G');
    f->store_8('D');
    f->store_8('S');
    f->store_8('T'); //godot streamable texture

    bool resize_to_po2 = false;

    if (p_compress_mode == COMPRESS_VIDEO_RAM && p_force_po2_for_compressed && (p_mipmaps || p_texture_flags & Texture::FLAG_REPEAT)) {
        resize_to_po2 = true;
        f->store_16(next_power_of_2(p_image->get_width()));
        f->store_16(p_image->get_width());
        f->store_16(next_power_of_2(p_image->get_height()));
        f->store_16(p_image->get_height());
    } else {
        f->store_16(p_image->get_width());
        f->store_16(0);
        f->store_16(p_image->get_height());
        f->store_16(0);
    }
    f->store_32(p_texture_flags);

    uint32_t format = 0;

    if (p_streamable)
        format |= StreamTexture::FORMAT_BIT_STREAM;
    if (p_mipmaps)
        format |= StreamTexture::FORMAT_BIT_HAS_MIPMAPS; //mipmaps bit
    if (p_detect_3d)
        format |= StreamTexture::FORMAT_BIT_DETECT_3D;
    if (p_detect_srgb)
        format |= StreamTexture::FORMAT_BIT_DETECT_SRGB;
    if (p_detect_normal)
        format |= StreamTexture::FORMAT_BIT_DETECT_NORMAL;

    if ((p_compress_mode == COMPRESS_LOSSLESS || p_compress_mode == COMPRESS_LOSSY) && p_image->get_format() > ImageData::FORMAT_RGBA8) {
        p_compress_mode = COMPRESS_UNCOMPRESSED; //these can't go as lossy
    }

    switch (p_compress_mode) {
        case COMPRESS_LOSSLESS: {
            bool lossless_force_png = ProjectSettings::get_singleton()->get("rendering/misc/lossless_compression/force_png").as<bool>();
            bool use_webp = !lossless_force_png && p_image->get_width() <= 16383 && p_image->get_height() <= 16383; // WebP has a size limit

            Ref<Image> image = dynamic_ref_cast<Image>(p_image->duplicate());
            if (p_mipmaps) {
                image->generate_mipmaps();
            } else {
                image->clear_mipmaps();
            }

            int mmc = image->get_mipmap_count() + 1;

            if (use_webp) {
                format |= StreamTexture::FORMAT_BIT_WEBP;
            } else {
                format |= StreamTexture::FORMAT_BIT_PNG;
            }
            format |= StreamTexture::FORMAT_BIT_PNG;
            f->store_32(format);
            f->store_32(mmc);

            for (int i = 0; i < mmc; i++) {

                if (i > 0) {
                    image->shrink_x2();
                }

                Vector<uint8_t> data = Image::lossless_packer(image);
                int data_len = data.size();
                f->store_32(data_len);

                f->store_buffer(data.data(), data_len);
            }

        } break;
        case COMPRESS_LOSSY: {
            Ref<Image> image(dynamic_ref_cast<Image>(p_image->duplicate()));
            if (p_mipmaps) {
                image->generate_mipmaps();
            } else {
                image->clear_mipmaps();
            }

            int mmc = image->get_mipmap_count() + 1;

            format |= StreamTexture::FORMAT_BIT_WEBP;
            f->store_32(format);
            f->store_32(mmc);

            for (int i = 0; i < mmc; i++) {

                if (i > 0) {
                    image->shrink_x2();
                }

                Vector<uint8_t> data = Image::lossy_packer(image, p_lossy_quality);
                int data_len = data.size();
                f->store_32(data_len);

                f->store_buffer(data.data(), data_len);
            }
        } break;
        case COMPRESS_VIDEO_RAM: {

            Ref<Image> image(dynamic_ref_cast<Image>(p_image->duplicate()));
            if (resize_to_po2) {
                image->resize_to_po2();
            }
            if (p_mipmaps) {
                image->generate_mipmaps(p_force_normal);
            }

            if (p_force_rgbe && image->get_format() >= ImageData::FORMAT_R8 && image->get_format() <= ImageData::FORMAT_RGBE9995) {
                image->convert(ImageData::FORMAT_RGBE9995);
            } else {
                ImageCompressSource csource = ImageCompressSource::COMPRESS_SOURCE_GENERIC;
                if (p_force_normal) {
                    csource = ImageCompressSource::COMPRESS_SOURCE_NORMAL;
                } else if (p_texture_flags & RS::TEXTURE_FLAG_CONVERT_TO_LINEAR) {
                    csource = ImageCompressSource::COMPRESS_SOURCE_SRGB;
                }

                image->compress(p_vram_compression, csource, p_lossy_quality);
            }

            format |= image->get_format();

            f->store_32(format);

            PoolVector<uint8_t> data = image->get_data();
            int dl = data.size();
            PoolVector<uint8_t>::Read r = data.read();
            f->store_buffer(r.ptr(), dl);
        } break;
        case COMPRESS_UNCOMPRESSED: {

            Ref<Image> image = dynamic_ref_cast<Image>(p_image->duplicate());
            if (p_mipmaps) {
                image->generate_mipmaps();
            } else {
                image->clear_mipmaps();
            }

            format |= image->get_format();
            f->store_32(format);

            PoolVector<uint8_t> data = image->get_data();
            int dl = data.size();
            PoolVector<uint8_t>::Read r = data.read();

            f->store_buffer(r.ptr(), dl);

        } break;
    }

    memdelete(f);
}

Error ResourceImporterTexture::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
                                      Vector<String> *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    int compress_mode= p_options.at("compress/mode").as<int>();
    float lossy= p_options.at("compress/lossy_quality").as<float>();
    int repeat= p_options.at("flags/repeat").as<int>();
    bool filter = p_options.at("flags/filter").as<bool>();
    bool mipmaps = p_options.at("flags/mipmaps").as<bool>();
    bool anisotropic = p_options.at("flags/anisotropic").as<bool>();
    int srgb= p_options.at("flags/srgb").as<int>();
    bool fix_alpha_border = p_options.at("process/fix_alpha_border").as<bool>();
    bool premult_alpha = p_options.at("process/premult_alpha").as<bool>();
    bool invert_color = p_options.at("process/invert_color").as<bool>();
    bool normal_map_invert_y = p_options.at("process/normal_map_invert_y").as<bool>();
    bool stream = p_options.at("stream").as<bool>();
    int size_limit= p_options.at("size_limit").as<int>();
    bool hdr_as_srgb = p_options.at("process/HDR_as_SRGB").as<bool>();
    int normal= p_options.at("compress/normal_map").as<int>();
    float scale= p_options.at("svg/scale").as<float>();
    bool force_rgbe = p_options.at("compress/hdr_mode").as<bool>();
    int bptc_ldr= p_options.at("compress/bptc_ldr").as<int>();

    Ref<Image> image(make_ref_counted<Image>());

    Error err = ImageLoader::load_image(p_source_file, image, nullptr, {scale,hdr_as_srgb});
    if (err != OK)
        return err;

    PoolStringArray formats_imported;

    int tex_flags = 0;
    if (repeat > 0)
        tex_flags |= Texture::FLAG_REPEAT;
    if (repeat == 2)
        tex_flags |= Texture::FLAG_MIRRORED_REPEAT;
    if (filter)
        tex_flags |= Texture::FLAG_FILTER;
    if (mipmaps || compress_mode == COMPRESS_VIDEO_RAM)
        tex_flags |= Texture::FLAG_MIPMAPS;
    if (anisotropic)
        tex_flags |= Texture::FLAG_ANISOTROPIC_FILTER;
    if (srgb == 1)
        tex_flags |= Texture::FLAG_CONVERT_TO_LINEAR;

    if (size_limit > 0 && (image->get_width() > size_limit || image->get_height() > size_limit)) {
        //limit size
        if (image->get_width() >= image->get_height()) {
            int new_width = size_limit;
            int new_height = image->get_height() * new_width / image->get_width();

            image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
        } else {

            int new_height = size_limit;
            int new_width = image->get_width() * new_height / image->get_height();

            image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
        }

        if (normal == 1) {
            image->normalize();
        }
    }

    if (fix_alpha_border) {
        image->fix_alpha_edges();
    }

    if (premult_alpha) {
        image->premultiply_alpha();
    }

    if (invert_color) {
        int height = image->get_height();
        int width = image->get_width();

        image->lock();
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                image->set_pixel(i, j, image->get_pixel(i, j).inverted());
            }
        }
        image->unlock();
    }

    if (normal_map_invert_y) {
        // Inverting the green channel can be used to flip a normal map's direction.
        // There's no standard when it comes to normal map Y direction, so this is
        // sometimes needed when using a normal map exported from another program.
        // See <http://wiki.polycount.com/wiki/Normal_Map_Technical_Details#Common_Swizzle_Coordinates>.
        const int height = image->get_height();
        const int width = image->get_width();

        image->lock();
        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                const Color color = image->get_pixel(i, j);
                image->set_pixel(i, j, Color(color.r, 1 - color.g, color.b));
            }
        }
        image->unlock();
    }
    bool detect_3d = p_options.at("detect_3d").as<bool>();
    bool detect_srgb = srgb == 2;
    bool detect_normal = normal == 0;
    bool force_normal = normal == 1;

    if (compress_mode == COMPRESS_VIDEO_RAM) {
        //must import in all formats, in order of priority (so platform choses the best supported one. IE, etc2 over etc).
        //Android, GLES 2.x

        bool ok_on_pc = false;
        bool is_hdr = (image->get_format() >= ImageData::FORMAT_RF && image->get_format() <= ImageData::FORMAT_RGBE9995);
        bool is_ldr = (image->get_format() >= ImageData::FORMAT_L8 && image->get_format() <= ImageData::FORMAT_RGB565);
        bool can_bptc = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_bptc").as<bool>();
        bool can_s3tc = ProjectSettings::get_singleton()->get("rendering/vram_compression/import_s3tc").as<bool>();

        if (can_bptc) {
            ImageUsedChannels channels = image->detect_used_channels();
            if (is_hdr) {

                if (channels == ImageUsedChannels::USED_CHANNELS_LA || channels == ImageUsedChannels::USED_CHANNELS_RGBA) {
                    can_bptc = false;
                }
            } else if (is_ldr) {

                //handle "RGBA Only" setting
                if (bptc_ldr == 1 && channels != ImageUsedChannels::USED_CHANNELS_LA && channels != ImageUsedChannels::USED_CHANNELS_RGBA) {
                    can_bptc = false;
                }
            }

            formats_imported.push_back("bptc");
        }

        if (!can_bptc && is_hdr && !force_rgbe) {
            //convert to ldr if this can't be stored hdr
            image->convert(ImageData::FORMAT_RGBA8);
        }

        if (can_bptc || can_s3tc) {
            _save_stex(image, String(p_save_path) + ".s3tc.stex", compress_mode, lossy, can_bptc ? ImageCompressMode::COMPRESS_BPTC : ImageCompressMode::COMPRESS_S3TC, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, false);
            r_platform_variants->push_back("s3tc");
            formats_imported.push_back("s3tc");
            ok_on_pc = true;
        }

        if (!ok_on_pc) {
            m_editor_interface->reportError(TTR("Warning, no suitable PC VRAM compression enabled in Project Settings. This texture "
                                              "will not display correctly on PC."));
        }
    } else {
        //import normally
        _save_stex(image, String(p_save_path) + ".stex", compress_mode, lossy, ImageCompressMode::COMPRESS_S3TC /*this is ignored */, mipmaps, tex_flags, stream, detect_3d, detect_srgb, force_rgbe, detect_normal, force_normal, false);
    }

    if (r_metadata) {
        Dictionary metadata;
        metadata["vram_texture"] = compress_mode == COMPRESS_VIDEO_RAM;
        if (!formats_imported.empty()) {
            metadata["imported_formats"] = formats_imported;
        }
        *r_metadata = metadata;
    }
    return OK;
}

const char *ResourceImporterTexture::compression_formats[] = {
    "bptc",
    "s3tc",
    nullptr
};
String ResourceImporterTexture::get_import_settings_string() const {

    String s;

    int index = 0;
    while (compression_formats[index]) {
        String setting_path = "rendering/vram_compression/import_" + String(compression_formats[index]);
        bool test = ProjectSettings::get_singleton()->get(StringName(setting_path)).as<bool>();
        if (test) {
            s += String(compression_formats[index]);
        }
        index++;
    }

    return s;
}

bool ResourceImporterTexture::are_import_settings_valid(StringView p_path) const {

    //will become invalid if formats are missing to import
    Dictionary metadata = ResourceFormatImporter::get_singleton()->get_resource_metadata(p_path).as<Dictionary>();

    if (!metadata.has("vram_texture")) {
        return false;
    }

    bool vram = metadata["vram_texture"].as<bool>();
    if (!vram) {
        return true; //do not care about non vram
    }

    Vector<String> formats_imported;
    if (metadata.has("imported_formats")) {
        formats_imported = metadata["imported_formats"].as<Vector<String>>();
    }

    int index = 0;
    bool valid = true;
    while (compression_formats[index]) {
        String setting_path = "rendering/vram_compression/import_" + String(compression_formats[index]);
        bool test = ProjectSettings::get_singleton()->get(StringName(setting_path)).as<bool>();
        if (test) {
            auto iter= eastl::find(formats_imported.begin(),formats_imported.end(),compression_formats[index]);
            if (iter==formats_imported.end()) {
                valid = false;
                break;
            }
        }
        index++;
    }

    return valid;
}

ResourceImporterTexture *ResourceImporterTexture::singleton = nullptr;

ResourceImporterTexture::ResourceImporterTexture() {

    singleton = this;
    StreamTexture::request_3d_callback = _texture_reimport_3d;
    StreamTexture::request_srgb_callback = _texture_reimport_srgb;
    StreamTexture::request_normal_callback = _texture_reimport_normal;
}

