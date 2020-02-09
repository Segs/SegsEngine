/*************************************************************************/
/*  gdscript_workspace.h                                                 */
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

#ifndef GDSCRIPT_WORKSPACE_H
#define GDSCRIPT_WORKSPACE_H

#include "../gdscript_parser.h"
#include "core/variant.h"
#include "gdscript_extend_parser.h"
#include "lsp.hpp"

class GDScriptWorkspace : public RefCounted {
    GDCLASS(GDScriptWorkspace,RefCounted)

protected:
    static void _bind_methods();
    void remove_cache_parser(se_string_view p_path);
    bool initialized = false;
    Map<StringName, lsp::DocumentSymbol> native_symbols;

    const lsp::DocumentSymbol *get_native_symbol(se_string_view p_class, se_string_view p_member = {}) const;
    const lsp::DocumentSymbol *get_script_symbol(se_string_view p_path) const;

    void reload_all_workspace_scripts();

    ExtendGDScriptParser *get_parse_successed_script(se_string_view p_path);
    ExtendGDScriptParser *get_parse_result(se_string_view p_path);

    void list_script_files(se_string_view p_root_dir, ListPOD<String> &r_files);

public:
    String root;
    String root_uri;

    Map<String, ExtendGDScriptParser *> scripts;
    Map<String, ExtendGDScriptParser *> parse_results;
    DefHashMap<StringName, ClassMembers> native_members;

public:
    Array symbol(const Dictionary &p_params);

public:
    Error initialize();

    Error parse_script(se_string_view p_path, se_string_view p_content);
    Error parse_local_script(se_string_view p_path);

    String get_file_path(se_string_view p_uri) const;
    String get_file_uri(se_string_view p_path) const;

    void publish_diagnostics(se_string_view p_path);
    void completion(const lsp::CompletionParams &p_params, PODVector<ScriptCodeCompletionOption> *r_options);

    const lsp::DocumentSymbol *resolve_symbol(const lsp::TextDocumentPositionParams &p_doc_pos, se_string_view p_symbol_name = {}, bool p_func_requred = false);
    void resolve_related_symbols(const lsp::TextDocumentPositionParams &p_doc_pos, ListPOD<const lsp::DocumentSymbol *> &r_list);

    const lsp::DocumentSymbol *resolve_native_symbol(const lsp::NativeSymbolInspectParams &p_params);
    void resolve_document_links(se_string_view p_uri, PODVector<lsp::DocumentLink> &r_list);
    Dictionary generate_script_api(se_string_view p_path);
    Error resolve_signature(const lsp::TextDocumentPositionParams &p_doc_pos, lsp::SignatureHelp &r_signature);

    GDScriptWorkspace();
    ~GDScriptWorkspace() override;
};

#endif
