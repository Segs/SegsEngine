/*************************************************************************/
/*  resource_importer_texture.cpp                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "core/io/config_file.h"
#include "core/io/image_loader.h"
#include "core/io/resource_importer.h"
#include "core/os/mutex.h"
#include "core/project_settings.h"
#include "editor/service_interfaces/EditorServiceInterface.h"
#include "scene/resources/texture.h"

namespace {
#pragma pack(push, 1)
struct TexFileHdr {
    int header_size;
    int file_size;
    int wdth;
    int hght;
    int flags;
    int fade[2];
    uint8_t alpha;
    char magic[3];
};
#pragma pack(pop)
} // end of anonymous namespace

void ResourceImporterCoHTexture::build_reconfigured_list(Vector<String> &to_reimport) {

    mutex->lock();

    if (make_flags.empty()) {
        mutex->unlock();
        return;
    }

    for (eastl::pair<const StringName, int> &E : make_flags) {

        Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());

        String src_path = String(E.first) + ".import";

        Error err = cf->load(src_path);
        ERR_CONTINUE(err != OK)

        bool changed = false;
        if (E.second & MAKE_SRGB_FLAG && int(cf->get_value("params", "flags/srgb")) == 2) {
            cf->set_value("params", "flags/srgb", 1);
            changed = true;
        }

        if (E.second & MAKE_NORMAL_FLAG && int(cf->get_value("params", "compress/normal_map")) == 0) {
            cf->set_value("params", "compress/normal_map", 1);
            changed = true;
        }

        if (E.second & MAKE_3D_FLAG && bool(cf->get_value("params", "detect_3d"))) {
            cf->set_value("params", "detect_3d", false);
            cf->set_value("params", "compress/mode", 2);
            cf->set_value("params", "flags/repeat", true);
            cf->set_value("params", "flags/filter", true);
            cf->set_value("params", "flags/mipmaps", true);
            changed = true;
        }

        if (changed) {
            cf->save(src_path);
            to_reimport.push_back(E.first);
        }
    }

    make_flags.clear();

    mutex->unlock();
}

String ResourceImporterCoHTexture::get_importer_name() const {

    return "coh_texture";
}

String ResourceImporterCoHTexture::get_visible_name() const {

    return "Texture";
}
void ResourceImporterCoHTexture::get_recognized_extensions(Vector<String> *p_extensions) const {
    p_extensions->push_back("texture");
}
String ResourceImporterCoHTexture::get_save_extension() const {
    return "dds";
}

String ResourceImporterCoHTexture::get_resource_type() const {

    return "StreamTexture";
}

bool ResourceImporterCoHTexture::get_option_visibility(
        const String &p_option, const Map<StringName, Variant> &p_options) const {

    if (p_option == "compress/lossy_quality") {
        int compress_mode = int(p_options.at("compress/mode"));
        if (compress_mode != COMPRESS_LOSSY && compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
    } else if (p_option == "compress/hdr_mode") {
        int compress_mode = int(p_options.at("compress/mode"));
        if (compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
    } else if (p_option == "compress/bptc_ldr") {
        int compress_mode = int(p_options.at("compress/mode"));
        if (compress_mode != COMPRESS_VIDEO_RAM) {
            return false;
        }
        if (!ProjectSettings::get_singleton()->get("rendering/vram_compression/import_bptc")) {
            return false;
        }
    }

    return true;
}

int ResourceImporterCoHTexture::get_preset_count() const {
    return 4;
}
String ResourceImporterCoHTexture::get_preset_name(int p_idx) const {

    static const char *preset_names[] = { "2D, Detect 3D", "2D", "2D Pixel", "3D" };

    return preset_names[p_idx];
}

void ResourceImporterCoHTexture::get_import_options(ListPOD<ImportOption> *r_options, int p_preset) const {

    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,Video RAM,Uncompressed",
                    PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            p_preset == PRESET_3D ? 2 : 0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::REAL, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "compress/hdr_mode", PROPERTY_HINT_ENUM, "Enabled,Force RGBE"), 0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "compress/bptc_ldr", PROPERTY_HINT_ENUM, "Enabled,RGBA Only"), 0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
    r_options->push_back(ImportOption(
            PropertyInfo(VariantType::INT, "flags/repeat", PROPERTY_HINT_ENUM, "Disabled,Enabled,Mirrored"),
            p_preset == PRESET_3D ? 1 : 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/filter"), p_preset != PRESET_2D_PIXEL));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/mipmaps"), p_preset == PRESET_3D));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "flags/anisotropic"), false));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::INT, "flags/srgb", PROPERTY_HINT_ENUM, "Disable,Enable,Detect"), 2));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::BOOL, "process/fix_alpha_border"), p_preset != PRESET_3D));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/premult_alpha"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/HDR_as_SRGB"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "process/invert_color"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "stream"), false));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::INT, "size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "detect_3d"), p_preset == PRESET_DETECT));
    r_options->push_back(
            ImportOption(PropertyInfo(VariantType::REAL, "svg/scale", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 1.0));
}

struct FileDeleter {
    void operator()(FileAccess *fa) const {
        memdelete(fa);
    }
};

Error ResourceImporterCoHTexture::import(const String &p_source_file, const String &p_save_path,
        const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files,
        Variant *r_metadata) {

//    int compress_mode = p_options.at("compress/mode");
//    float lossy = p_options.at("compress/lossy_quality");
//    int repeat = p_options.at("flags/repeat");
//    bool filter = p_options.at("flags/filter").as<bool>();
//    bool mipmaps = p_options.at("flags/mipmaps").as<bool>();
//    bool anisotropic = p_options.at("flags/anisotropic").as<bool>();
//    int srgb = p_options.at("flags/srgb");
//    bool fix_alpha_border = p_options.at("process/fix_alpha_border").as<bool>();
//    bool premult_alpha = p_options.at("process/premult_alpha").as<bool>();
//    bool invert_color = p_options.at("process/invert_color").as<bool>();
//    bool stream = p_options.at("stream").as<bool>();
//    int size_limit = p_options.at("size_limit");
//    bool hdr_as_srgb = p_options.at("process/HDR_as_SRGB").as<bool>();
//    int normal = p_options.at("compress/normal_map");
//    float scale = p_options.at("svg/scale");
//    bool force_rgbe = p_options.at("compress/hdr_mode").as<bool>();
//    int bptc_ldr = p_options.at("compress/bptc_ldr");

    Ref<Image> image(make_ref_counted<Image>());
    eastl::unique_ptr<FileAccess,FileDeleter> src_tex(FileAccess::open(p_source_file, FileAccess::READ));

    if (!src_tex) return ERR_FILE_CANT_OPEN;

    TexFileHdr hdr;
    src_tex->get_buffer((uint8_t *)&hdr, sizeof(TexFileHdr));

    if (0 != memcmp(hdr.magic, "TX2", 3)) return ERR_FILE_UNRECOGNIZED;

    int name_bytes = hdr.header_size - sizeof(TexFileHdr);
    CharString data;
    data.resize(name_bytes);

    if (name_bytes != src_tex->get_buffer((uint8_t *)data.data(), name_bytes)) return ERR_FILE_UNRECOGNIZED;

    QString originalname = QString::fromLatin1(data);

    eastl::unique_ptr<FileAccess,FileDeleter> dest_tex(FileAccess::open(p_save_path+'.'+PathUtils::get_extension(originalname), FileAccess::WRITE));
    if (!dest_tex) return ERR_FILE_CANT_OPEN;
    uint8_t buf[4096*4];
    while(true) {
        int total = src_tex->get_buffer(buf,4096*4);
        if(total>0)
            dest_tex->store_buffer(buf,total);
        if(total!=4096*4)
            break;
    }

    if (r_metadata) {
        Dictionary metadata;
        metadata["coh_texture_data"] = "blorb";
        *r_metadata = metadata;
    }
    return OK;
}

const char *ResourceImporterCoHTexture::compression_formats[] = { "bptc", "s3tc", "etc", "etc2", "pvrtc", nullptr };
String ResourceImporterCoHTexture::get_import_settings_string() const {

    String s;

    int index = 0;
    while (compression_formats[index]) {
        String setting_path = "rendering/vram_compression/import_" + String(compression_formats[index]);
        bool test = ProjectSettings::get_singleton()->get(setting_path).as<bool>();
        if (test) {
            s += String(compression_formats[index]);
        }
        index++;
    }

    return s;
}

bool ResourceImporterCoHTexture::are_import_settings_valid(const String &p_path) const {

    // will become invalid if formats are missing to import
    Dictionary metadata = ResourceFormatImporter::get_singleton()->get_resource_metadata(p_path);

    if (!metadata.has("coh_texture_data")) {
        return false;
    }
    return true;
}

ResourceImporterCoHTexture *ResourceImporterCoHTexture::singleton = nullptr;

ResourceImporterCoHTexture::ResourceImporterCoHTexture() {

    singleton = this;
    mutex = memnew(Mutex);
}

ResourceImporterCoHTexture::~ResourceImporterCoHTexture() {

    memdelete(mutex);
}
