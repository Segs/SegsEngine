/*************************************************************************/
/*  resource_importer_csv.cpp                                            */
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

#include "resource_importer_csv.h"

#include "core/io/resource_saver.h"
#include "core/os/file_access.h"

StringName ResourceImporterCSV::get_importer_name() const {

    return "csv";
}

StringName ResourceImporterCSV::get_visible_name() const {

    return "CSV";
}
void ResourceImporterCSV::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back("csv");
}

StringName ResourceImporterCSV::get_save_extension() const {
    return ""; //does not save a single resource
}

StringName ResourceImporterCSV::get_resource_type() const {

    return "TextFile";
}

bool ResourceImporterCSV::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {

    return true;
}

int ResourceImporterCSV::get_preset_count() const {
    return 0;
}
StringName ResourceImporterCSV::get_preset_name(int p_idx) const {

    return "";
}

void ResourceImporterCSV::get_import_options(Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {
}

Error ResourceImporterCSV::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String>
        *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {
    return OK;
}

ResourceImporterCSV::ResourceImporterCSV() {
}
