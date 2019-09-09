#pragma once

#include "core/property_info.h"
#include "core/map.h"

class String;
template <class T>
class Vector;
class EditorServiceInterface;

class ResourceImporterInterface {
protected:
    EditorServiceInterface *m_editor_interface;

public:
    struct ImportOption {
        PropertyInfo option;
        Variant default_value;

        ImportOption(const PropertyInfo &p_info, const Variant &p_default) : option(p_info), default_value(p_default) {}
        ImportOption() {}
    };
    void set_editor_interface(EditorServiceInterface *i) { m_editor_interface = i; }

    virtual String get_importer_name() const = 0;
    virtual String get_visible_name() const = 0;
    virtual void get_recognized_extensions(Vector<String> *p_extensions) const = 0;
    virtual String get_save_extension() const = 0;
    virtual String get_resource_type() const = 0;
    virtual float get_priority() const = 0;
    virtual int get_import_order() const = 0;
    virtual int get_preset_count() const = 0;
    virtual String get_preset_name(int /*p_idx*/) const = 0;
    virtual void get_import_options(List<ImportOption> *r_options, int p_preset = 0) const = 0;
    virtual bool get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const = 0;
    virtual String get_option_group_file() const = 0;
    virtual Error import(const String &p_source_file, const String &p_save_path,
            const Map<StringName, Variant> &p_options, List<String> *r_platform_variants,
            List<String> *r_gen_files = nullptr, Variant *r_metadata = nullptr) = 0;
    virtual Error import_group_file(const String &p_group_file,
            const Map<String, Map<StringName, Variant>> &p_source_file_options,
            const Map<String, String> &p_base_paths) = 0;
    virtual bool are_import_settings_valid(const String &p_path) const = 0;
    virtual String get_import_settings_string() const = 0;
    // Currently only implemented by ResourceImporterTexture
    /**
     * @brief build_reconfigured_list will use the resource's configuration and current state of the object as set by user
     * and build a list of resources that need to be reimported.
     * @param tgt will contain a vector of all resource names that need to be reimported
     * @note this method should not be called until editor ends it's current scan/import proces
     */
    virtual void build_reconfigured_list(Vector<String> & /*tgt*/) {}
    virtual ~ResourceImporterInterface() = default;
};
