#pragma once

#include "core/image.h"
#include "core/plugin_interfaces/PluginDeclarations.h"

class StreamTexture;

class LayeredTextureImpl : public ResourceImporterInterface{

    bool is_3d;
    static const char *compression_formats[];

protected:
    static void _texture_reimport_srgb(const Ref<StreamTexture> &p_tex);
    static void _texture_reimport_3d(const Ref<StreamTexture> &p_tex);
    static void _texture_reimport_normal(const Ref<StreamTexture> &p_tex);

public:
    enum Preset {
        PRESET_3D,
        PRESET_2D,
        PRESET_COLOR_CORRECT,
    };

    enum CompressMode {
        COMPRESS_LOSSLESS,
        COMPRESS_VIDEO_RAM,
        COMPRESS_UNCOMPRESSED
    };
    StringName get_importer_name() const override;
    StringName get_visible_name() const override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    StringName get_save_extension() const override;
    StringName get_resource_type() const override;

    int get_preset_count() const override;
    StringName get_preset_name(int p_idx) const override;

    void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const override;
    bool get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const override;

    void _save_tex(const Vector<Ref<Image>> &p_images, StringView p_to_path, int p_compress_mode, ImageCompressMode p_vram_compression, bool p_mipmaps, int p_texture_flags);

    Error import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String>
            *r_platform_variants, Vector<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) override;

    void update_imports();

    bool are_import_settings_valid(StringView p_path) const override;
    String get_import_settings_string() const override;

    void set_3d(bool p_3d) { is_3d = p_3d; }
    LayeredTextureImpl();

    // ResourceImporterInterface defaults
public:
    int get_import_order() const override {return 0;}
    StringName get_option_group_file() const override { return StringName(); }
    Error import_group_file(StringView /*p_group_file*/, const Map<String, HashMap<StringName, Variant> > & /*p_source_file_options*/, const Map<String, String> & /*p_base_paths*/) override {
        return ERR_UNAVAILABLE;
    }
};
