/*************************************************************************/
/*  pluginscript_loader.h                                                */
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

// Godot imports
#include "core/io/resource_format_loader.h"
#include "core/io/resource_saver.h"
#include "core/script_language.h"

class PluginScriptLanguage;

class ResourceFormatLoaderPluginScript : public ResourceFormatLoader {

    PluginScriptLanguage *_language;

public:
    ResourceFormatLoaderPluginScript(PluginScriptLanguage *language);
    RES load(se_string_view p_path, const String &p_original_path = String(), Error *r_error = nullptr) override;
    void get_recognized_extensions(PODVector<String> &p_extensions) const override;
    bool handles_type(const String &p_type) const override;
    String get_resource_type(se_string_view p_path) const override;
};

class ResourceFormatSaverPluginScript : public ResourceFormatSaver {

    PluginScriptLanguage *_language;

public:
    ResourceFormatSaverPluginScript(PluginScriptLanguage *language);
    Error save(se_string_view p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    void get_recognized_extensions(const RES &p_resource, PODVector<String> *p_extensions) const override;
    bool recognize(const RES &p_resource) const override;
};
