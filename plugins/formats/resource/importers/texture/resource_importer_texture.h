/*************************************************************************/
/*  resource_importer_texture.h                                          */
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

#pragma once

#include "core/image.h"
#include "core/plugin_interfaces/PluginDeclarations.h"

class StringName;

class ResourceImporterTexture : public QObject, public ResourceImporterInterface {

    Q_PLUGIN_METADATA(IID "org.godot.TextureImporter")
    Q_INTERFACES(ResourceImporterInterface)
    Q_OBJECT

protected:
    enum {
        MAKE_3D_FLAG = 1,
        MAKE_SRGB_FLAG = 2,
        MAKE_NORMAL_FLAG = 4
    };

    Mutex *mutex;
    Map<StringName, int> make_flags;

    static void _texture_reimport_srgb(StringName p_tex);
    static void _texture_reimport_3d(StringName p_tex_path);
    static void _texture_reimport_normal(StringName p_tex_path);

    static ResourceImporterTexture *singleton;
    static const char *compression_formats[];

public:
    static ResourceImporterTexture *get_singleton() { return singleton; }
    StringName get_importer_name() const override;
    StringName get_visible_name() const override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;
    StringName get_save_extension() const override;
    StringName get_resource_type() const override;

    enum Preset {
        PRESET_DETECT,
        PRESET_2D,
        PRESET_2D_PIXEL,
        PRESET_3D,
    };

    enum CompressMode { COMPRESS_LOSSLESS, COMPRESS_LOSSY, COMPRESS_VIDEO_RAM, COMPRESS_UNCOMPRESSED };

    int get_preset_count() const override;
    StringName get_preset_name(int p_idx) const override;

    void get_import_options(ListPOD<ImportOption> *r_options, int p_preset = 0) const override;
    bool get_option_visibility(const StringName &p_option, const Map<StringName, Variant> &p_options) const override;

    void _save_stex(const Ref<Image> &p_image, se_string_view p_to_path, int p_compress_mode, float p_lossy_quality,
            ImageCompressMode p_vram_compression, bool p_mipmaps, int p_texture_flags, bool p_streamable,
            bool p_detect_3d, bool p_detect_srgb, bool p_force_rgbe, bool p_detect_normal, bool p_force_normal,
            bool p_force_po2_for_compressed);

    Error import(se_string_view p_source_file, se_string_view p_save_path, const Map<StringName, Variant> &p_options,
            DefList<String> *r_platform_variants, DefList<String> *r_gen_files = nullptr,
            Variant *r_metadata = nullptr) override;

    void build_reconfigured_list(PODVector<String> &editor_is_scanning_or_importing) override;

    bool are_import_settings_valid(se_string_view p_path) const override;
    String get_import_settings_string() const override;

    // ResourceImporterInterface defaults
public:
    float get_priority() const override { return 14.0f; }
    int get_import_order() const override { return 0; }
    StringName get_option_group_file() const override { return StringName(); }
    Error import_group_file(se_string_view /*p_group_file*/,
            const Map<String, Map<StringName, Variant>> & /*p_source_file_options*/,
            const Map<String, String> & /*p_base_paths*/) override {
        return ERR_UNAVAILABLE;
    }

public:
    ResourceImporterTexture();
    ~ResourceImporterTexture() override;
};
