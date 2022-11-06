/*************************************************************************/
/*  resource_importer_layered_texture.cpp                                */
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

#include "resource_importer_layered_texture.h"

#include "core/io/config_file.h"
#include "core/object.h"
#include "core/io/image_loader.h"
#include "core/project_settings.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "scene/resources/texture.h"

namespace {
constexpr const char *compression_formats[] = {
    "bptc",
    "s3tc",
    nullptr
};
}

const char *LayeredTextureImpl::get_importer_name() const {

    return is_3d ? "texture_3d" : "texture_array";
}

const char *LayeredTextureImpl::get_visible_name() const {

    return is_3d ? "Texture3D" : "TextureArray";
}
void LayeredTextureImpl::get_recognized_extensions(Vector<String> &p_extensions) const {

    ImageLoader::get_recognized_extensions(p_extensions);
}
StringName LayeredTextureImpl::get_save_extension() const {
    return is_3d ? StringName("tex3d") : StringName("texarr");
}

StringName LayeredTextureImpl::get_resource_type() const {

    return is_3d ? StringName("Texture3D") : StringName("TextureArray");
}

bool LayeredTextureImpl::get_option_visibility(const StringName & /*p_option*/, const HashMap<StringName, Variant> &/*p_options*/) const {

    return true;
}

int LayeredTextureImpl::get_preset_count() const {
    return 3;
}
StringName LayeredTextureImpl::get_preset_name(int p_idx) const {

    static const char *preset_names[] = {
        "3D",
        "2D",
        "ColorCorrect"
    };

    return StaticCString(preset_names[p_idx],true);
}

void LayeredTextureImpl::get_import_options(Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {

    r_options->emplace_back(PropertyInfo(VariantType::INT, "compress/mode", PropertyHint::Enum, "Lossless (PNG),Video RAM (S3TC/ETC/BPTC),Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == PRESET_3D ? 1 : 0);
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "compress/no_bptc_if_rgb"), false);
    r_options->emplace_back(PropertyInfo(VariantType::INT, "flags/repeat", PropertyHint::Enum, "Disabled,Enabled,Mirrored"), 0);
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "flags/filter"), true);
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "flags/mipmaps"), p_preset == PRESET_COLOR_CORRECT ? 0 : 1);
    r_options->emplace_back(PropertyInfo(VariantType::BOOL, "flags/anisotropic"), false);
    r_options->emplace_back(PropertyInfo(VariantType::INT, "flags/srgb", PropertyHint::Enum, "Disable,Enable"), p_preset == PRESET_3D ? 1 : 0);
    r_options->emplace_back(PropertyInfo(VariantType::INT, "slices/horizontal", PropertyHint::Range, "1,256,1"), p_preset == PRESET_COLOR_CORRECT ? 16 : 8);
    r_options->emplace_back(PropertyInfo(VariantType::INT, "slices/vertical", PropertyHint::Range, "1,256,1"), p_preset == PRESET_COLOR_CORRECT ? 1 : 8);
}

void LayeredTextureImpl::_save_tex(const Vector<Ref<Image>> &p_images, StringView p_to_path, int p_compress_mode, ImageCompressMode p_vram_compression, bool p_mipmaps, int p_texture_flags) {

    FileAccess *f = FileAccess::open(p_to_path, FileAccess::WRITE);
    f->store_8('G');
    f->store_8('D');
    if (is_3d) {
        f->store_8('3');
    } else {
        f->store_8('A');
    }
    f->store_8('T'); //godot streamable texture

    f->store_32(p_images[0]->get_width());
    f->store_32(p_images[0]->get_height());
    f->store_32(p_images.size()); //depth
    f->store_32(p_texture_flags);
    if (p_compress_mode != COMPRESS_VIDEO_RAM) {
        //vram needs to do a first compression to tell what the format is, for the rest its ok
        f->store_32(p_images[0]->get_format());
        f->store_32(p_compress_mode); // 0 - lossless (PNG), 1 - vram, 2 - uncompressed
    }

    if ((p_compress_mode == COMPRESS_LOSSLESS) && p_images[0]->get_format() > ImageData::FORMAT_RGBA8) {
        p_compress_mode = COMPRESS_UNCOMPRESSED; //these can't go as lossy
    }

    for (int i = 0; i < p_images.size(); i++) {

        switch (p_compress_mode) {
            case COMPRESS_LOSSLESS: {

                Ref<Image> image = dynamic_ref_cast<Image>(p_images[i]->duplicate());
                if (p_mipmaps) {
                    image->generate_mipmaps();
                } else {
                    image->clear_mipmaps();
                }

                int mmc = image->get_mipmap_count() + 1;
                f->store_32(mmc);

                for (int j = 0; j < mmc; j++) {

                    if (j > 0) {
                        image->shrink_x2();
                    }

                    Vector<uint8_t> data = Image::lossless_packer(image);
                    int data_len = data.size();
                    f->store_32(data_len);
                    f->store_buffer(data.data(), data_len);
                }

            } break;
            case COMPRESS_VIDEO_RAM: {

                Ref<Image> image = dynamic_ref_cast<Image>(p_images[i]->duplicate());
                image->generate_mipmaps(false);

                ImageCompressSource csource = ImageCompressSource::COMPRESS_SOURCE_GENERIC; //ImageCompressSource::COMPRESS_SOURCE_LAYERED;
                image->compress(p_vram_compression, csource, 0.7f);

                if (i == 0) {
                    //hack so we can properly tell the format
                    f->store_32(image->get_format());
                    f->store_32(p_compress_mode); // 0 - lossless (PNG), 1 - vram, 2 - uncompressed
                }

                PoolVector<uint8_t> data = image->get_data();
                int dl = data.size();

                PoolVector<uint8_t>::Read r = data.read();
                f->store_buffer(r.ptr(), dl);
            } break;
            case COMPRESS_UNCOMPRESSED: {

                Ref<Image> image = dynamic_ref_cast<Image>(p_images[i]->duplicate());

                if (p_mipmaps) {
                    image->generate_mipmaps();
                } else {
                    image->clear_mipmaps();
                }

                PoolVector<uint8_t> data = image->get_data();
                int dl = data.size();

                PoolVector<uint8_t>::Read r = data.read();

                f->store_buffer(r.ptr(), dl);

            } break;
        }
    }

    memdelete(f);
}

Error LayeredTextureImpl::import(StringView p_source_file, StringView _save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
    Vector<String> * r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    String p_save_path(_save_path);
    int compress_mode= p_options.at("compress/mode").as<int>();
    int no_bptc_if_rgb= p_options.at("compress/no_bptc_if_rgb").as<int>();
    int repeat= p_options.at("flags/repeat").as<int>();
    bool filter = p_options.at("flags/filter").as<bool>();
    bool mipmaps = p_options.at("flags/mipmaps").as<bool>();
    bool anisotropic = p_options.at("flags/anisotropic").as<bool>();
    int srgb= p_options.at("flags/srgb").as<int>();
    int hslices= p_options.at("slices/horizontal").as<int>();
    int vslices= p_options.at("slices/vertical").as<int>();

    Ref<Image> image(make_ref_counted<Image>());

    Error err = ImageLoader::load_image(p_source_file, image, nullptr, {1.0,false});
    if (err != OK)
        return err;

    int tex_flags = 0;
    if (repeat > 0) {
        tex_flags |= Texture::FLAG_REPEAT;
    }
    if (repeat == 2) {
        tex_flags |= Texture::FLAG_MIRRORED_REPEAT;
    }
    if (filter) {
        tex_flags |= Texture::FLAG_FILTER;
    }
    if (mipmaps || compress_mode == COMPRESS_VIDEO_RAM) {
        tex_flags |= Texture::FLAG_MIPMAPS;
    }
    if (anisotropic) {
        tex_flags |= Texture::FLAG_ANISOTROPIC_FILTER;
    }
    if (srgb == 1) {
        tex_flags |= Texture::FLAG_CONVERT_TO_LINEAR;
    }

    Vector<Ref<Image> > slices;

    int slice_w = image->get_width() / hslices;
    int slice_h = image->get_height() / vslices;

    //optimize
    if (compress_mode == COMPRESS_VIDEO_RAM) {
        //if using video ram, optimize
        if (srgb) {
            //remove alpha if not needed, so compression is more efficient
            if (image->get_format() == ImageData::FORMAT_RGBA8 && !image->detect_alpha()) {
                image->convert(ImageData::FORMAT_RGB8);
            }
        } else {
            image->optimize_channels();
        }
    }

    for (int i = 0; i < vslices; i++) {
        for (int j = 0; j < hslices; j++) {
            int x = slice_w * j;
            int y = slice_h * i;
            Ref<Image> slice = image->get_rect(Rect2(x, y, slice_w, slice_h));
            ERR_CONTINUE(not slice || slice->is_empty());
            if (slice->get_width() != slice_w || slice->get_height() != slice_h) {
                slice->resize(slice_w, slice_h);
            }
            slices.emplace_back(eastl::move(slice));
        }
    }

    StringName extension = get_save_extension();
    Array formats_imported;

    if (compress_mode == COMPRESS_VIDEO_RAM) {
        //must import in all formats, in order of priority (so platform choses the best supported one. IE, etc2 over etc).
        //Android, GLES 2.x

        bool ok_on_pc = false;
        bool encode_bptc = false;

        if (ProjectSettings::get_singleton()->getT<bool>("rendering/vram_compression/import_bptc")) {

            encode_bptc = true;

            if (no_bptc_if_rgb) {
                ImageUsedChannels channels = image->detect_used_channels();
                if (channels != ImageUsedChannels::USED_CHANNELS_LA && channels != ImageUsedChannels::USED_CHANNELS_RGBA) {
                    encode_bptc = false;
                }
            }

            formats_imported.emplace_back("bptc");
        }

        if (encode_bptc) {

            _save_tex(slices, p_save_path + ".bptc." + extension, compress_mode, ImageCompressMode::COMPRESS_BPTC, mipmaps, tex_flags);
            r_platform_variants->emplace_back("bptc");
            ok_on_pc = true;
        }

        if (ProjectSettings::get_singleton()->getT<bool>("rendering/vram_compression/import_s3tc")) {

            _save_tex(slices, p_save_path + ".s3tc." + extension, compress_mode, ImageCompressMode::COMPRESS_S3TC, mipmaps, tex_flags);
            r_platform_variants->emplace_back("s3tc");
            ok_on_pc = true;
            formats_imported.emplace_back("s3tc");
        }

#ifdef TOOLS_ENABLED
        if (!ok_on_pc) {
            EditorNode::add_io_error("Warning, no suitable PC VRAM compression enabled in Project Settings. This texture will not display correctly on PC.");
        }
#endif
    } else {
        //import normally
        _save_tex(slices, p_save_path + "." + extension, compress_mode, ImageCompressMode::COMPRESS_S3TC /*this is ignored */, mipmaps, tex_flags);
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

String LayeredTextureImpl::get_import_settings_string() const {

    String s;

    int index = 0;
    while (compression_formats[index]) {
        StringName setting_path("rendering/vram_compression/import_" + String(compression_formats[index]));
        bool test = ProjectSettings::get_singleton()->get(setting_path).as<bool>();
        if (test) {
            s += String(compression_formats[index]);
        }
        index++;
    }

    return s;
}

bool LayeredTextureImpl::are_import_settings_valid(StringView p_path) const {

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
        StringName setting_path("rendering/vram_compression/import_" + String(compression_formats[index]));
        bool test = ProjectSettings::get_singleton()->get(setting_path).as<bool>();
        if (test) {
            auto iter = eastl::find(formats_imported.begin(),formats_imported.end(),compression_formats[index]);
            if (iter==formats_imported.end()) {
                valid = false;
                break;
            }
        }
        index++;
    }

    return valid;
}

LayeredTextureImpl::LayeredTextureImpl() {

    is_3d = true;
}

