/*************************************************************************/
/*  editor_import_plugin.h                                               */
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

#ifndef EDITOR_IMPORT_PLUGIN_H
#define EDITOR_IMPORT_PLUGIN_H

#include "core/io/resource_importer.h"

class EditorImportPlugin : public ResourceImporter {
    GDCLASS(EditorImportPlugin,ResourceImporter)

protected:
    static void _bind_methods();

public:
    EditorImportPlugin();
    StringName get_importer_name() const override;
    StringName get_visible_name() const override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    StringName get_preset_name(int p_idx) const override;
    int get_preset_count() const override;
    StringName get_save_extension() const override;
    StringName get_resource_type() const override;
    float get_priority() const override;
    int get_import_order() const override;
    void get_import_options(List<ImportOption> *r_options, int p_preset) const override;
    bool get_option_visibility(const StringName &p_option, const Map<StringName, Variant> &p_options) const override;
    Error import(se_string_view p_source_file, se_string_view p_save_path, const Map<StringName, Variant> &p_options, Vector<String>
            *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};

#endif //EDITOR_IMPORT_PLUGIN_H
