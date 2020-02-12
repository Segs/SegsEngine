/*************************************************************************/
/*  gdscript_workspace.cpp                                               */
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

#include "gdscript_workspace.h"
#include "gdscript_language_protocol.h"
#include "../gdscript.h"
#include "../gdscript_parser.h"
#include "core/os/dir_access.h"
#include "core/project_settings.h"
#include "core/method_bind.h"
#include "core/script_language.h"
#include "editor/editor_help.h"

IMPL_GDCLASS(GDScriptWorkspace)

void GDScriptWorkspace::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("symbol"), &GDScriptWorkspace::symbol);
    MethodBinder::bind_method(D_METHOD("parse_script", {"p_path", "p_content"}), &GDScriptWorkspace::parse_script);
    MethodBinder::bind_method(D_METHOD("parse_local_script", {"p_path"}), &GDScriptWorkspace::parse_local_script);
    MethodBinder::bind_method(D_METHOD("get_file_path", {"p_uri"}), &GDScriptWorkspace::get_file_path);
    MethodBinder::bind_method(D_METHOD("get_file_uri", {"p_path"}), &GDScriptWorkspace::get_file_uri);
    MethodBinder::bind_method(D_METHOD("publish_diagnostics", {"p_path"}), &GDScriptWorkspace::publish_diagnostics);
    MethodBinder::bind_method(D_METHOD("generate_script_api", {"p_path"}), &GDScriptWorkspace::generate_script_api);
}

void GDScriptWorkspace::remove_cache_parser(se_string_view p_path) {
    auto parser = parse_results.find_as(p_path);
    auto script = scripts.find_as(p_path);
    if (parser!=parse_results.end() && script!=scripts.end()) {
        if (script->second && script->second == script->second) {
            memdelete(script->second);
        } else {
            memdelete(script->second);
            memdelete(parser->second);
        }
        parse_results.erase(parser);
        scripts.erase(script);
    } else if (parser!=parse_results.end()) {
        memdelete(parser->second);
        parse_results.erase(parser);
    } else if (script!=scripts.end()) {
        memdelete(script->second);
        scripts.erase(script);
    }
}

const lsp::DocumentSymbol *GDScriptWorkspace::get_native_symbol(se_string_view p_class, se_string_view p_member) const {

    StringName class_name = StringName(p_class);
    StringName empty;

    while (class_name != empty) {
        const auto E = native_symbols.find(class_name);
        if (E!=native_symbols.end()) {
            const lsp::DocumentSymbol &class_symbol = E->second;

            if (p_member.empty()) {
                return &class_symbol;
            } else {
                for (int i = 0; i < class_symbol.children.size(); i++) {
                    const lsp::DocumentSymbol &symbol = class_symbol.children[i];
                    if (symbol.name == p_member) {
                        return &symbol;
                    }
                }
            }
        }
        class_name = ClassDB::get_parent_class(class_name);
    }

    return nullptr;
}

const lsp::DocumentSymbol *GDScriptWorkspace::get_script_symbol(se_string_view p_path) const {
    const auto S = scripts.find_as(p_path);
    if (S!=scripts.end()) {
        return &(S->second->get_symbols());
    }
    return nullptr;
}

void GDScriptWorkspace::reload_all_workspace_scripts() {
    List<String> paths;
    list_script_files("res://", paths);
    for (const String &path : paths) {
        Error err;
        String content = FileAccess::get_file_as_string(path, &err);
        ERR_CONTINUE(err != OK);
        err = parse_script(path, content);

        if (err != OK) {
            auto S = parse_results.find(path);
            String err_msg = "Failed parse script " + path;
            if (S!=parse_results.end()) {
                err_msg += "\n" + S->second->get_error();
            }
            ERR_CONTINUE_MSG(err != OK,err_msg); 
        }
    }
}

void GDScriptWorkspace::list_script_files(se_string_view p_root_dir, List<String> &r_files) {
    Error err;
    DirAccessRef dir = DirAccess::open(p_root_dir, &err);
    if (OK == err) {
        dir->list_dir_begin();
        String file_name = dir->get_next();
        while (file_name.length()) {
            if (dir->current_is_dir() && file_name != "." && file_name != ".." && file_name != "./") {
                list_script_files(PathUtils::plus_file(p_root_dir,file_name), r_files);
            } else if (StringUtils::ends_with(file_name,(".gd"))) {
                String script_file = PathUtils::plus_file(p_root_dir,file_name);
                r_files.push_back(script_file);
            }
            file_name = dir->get_next();
        }
    }
}

ExtendGDScriptParser *GDScriptWorkspace::get_parse_successed_script(se_string_view p_path) {
    auto S = scripts.find_as(p_path);
    if (S==scripts.end()) {
        parse_local_script(p_path);
        S = scripts.find_as(p_path);
    }
    if (S!=scripts.end()) {
        return S->second;
    }
    return nullptr;
}

ExtendGDScriptParser *GDScriptWorkspace::get_parse_result(se_string_view p_path) {
    auto S = parse_results.find_as(p_path);
    if (S==parse_results.end()) {
        parse_local_script(p_path);
        S = parse_results.find_as(p_path);
    }
    if (S!=parse_results.end()) {
        return S->second;
    }
    return nullptr;
}

Array GDScriptWorkspace::symbol(const Dictionary &p_params) {
    String query = p_params["query"].as<String>();
    Array arr;
    if (query.empty()) {
        return arr;
    }
    for (const auto &E : scripts) {
        Vector<lsp::DocumentedSymbolInformation> script_symbols;
        E.second->get_symbols().symbol_tree_as_list(E.first, script_symbols);
        for (size_t i = 0; i < script_symbols.size(); ++i) {
            if (StringUtils::is_subsequence_of(query,script_symbols[i].name,StringUtils::CaseInsensitive)) {
                arr.push_back(script_symbols[i].to_json());
            }
        }
    }
    return arr;
}

Error GDScriptWorkspace::initialize() {
    if (initialized) return OK;

    DocData *doc = EditorHelp::get_doc_data();
    for (const auto &E : doc->class_list) {

        const DocData::ClassDoc &class_data = E.second;
        lsp::DocumentSymbol class_symbol;
        StringName class_name(E.first);
        class_symbol.name = class_name;
        class_symbol.native_class = class_name;
        class_symbol.kind = lsp::SymbolKind::Class;
        class_symbol.detail = String("<Native> class ") + class_name;
        if (!class_data.inherits.empty()) {
            class_symbol.detail += String(" extends ") + class_data.inherits;
        }
        class_symbol.documentation = class_data.brief_description + "\n" + class_data.description;

        for (int i = 0; i < class_data.constants.size(); i++) {
            const DocData::ConstantDoc &const_data = class_data.constants[i];
            lsp::DocumentSymbol symbol;
            symbol.name = const_data.name;
            symbol.native_class = class_name;
            symbol.kind = lsp::SymbolKind::Constant;
            symbol.detail = "const " + String(class_name) + "." + const_data.name;
            if (const_data.enumeration.length()) {
                symbol.detail += ": " + const_data.enumeration;
            }
            symbol.detail += " = " + const_data.value;
            symbol.documentation = const_data.description;
            class_symbol.children.push_back(symbol);
        }

        Vector<DocData::PropertyDoc> properties;
        properties.push_back(class_data.properties);
        const int theme_prop_start_idx = properties.size();
        properties.push_back(class_data.theme_properties);

        for (size_t i = 0; i < class_data.properties.size(); i++) {
            const DocData::PropertyDoc &data = class_data.properties[i];
            lsp::DocumentSymbol symbol;
            symbol.name = data.name;
            symbol.native_class = class_name;
            symbol.kind = lsp::SymbolKind::Property;
            symbol.detail = String(i >= theme_prop_start_idx ? "<Theme> var" : "var") + " " + class_name + "." + data.name;
            if (data.enumeration.length()) {
                symbol.detail += ": " + data.enumeration;
            } else {
                symbol.detail += String(": ") + data.type;
            }
            symbol.documentation = data.description;
            class_symbol.children.push_back(symbol);
        }

        Vector<DocData::MethodDoc> methods_signals;
        methods_signals.push_back(class_data.methods);
        const int signal_start_idx = methods_signals.size();
        methods_signals.push_back(class_data.defined_signals);

        for (int i = 0; i < methods_signals.size(); i++) {
            const DocData::MethodDoc &data = methods_signals[i];

            lsp::DocumentSymbol symbol;
            symbol.name = data.name;
            symbol.native_class = class_name;
            symbol.kind = i >= signal_start_idx ? lsp::SymbolKind::Event : lsp::SymbolKind::Method;

            String params;
            bool arg_default_value_started = false;
            for (int j = 0; j < data.arguments.size(); j++) {
                const DocData::ArgumentDoc &arg = data.arguments[j];
                lsp::DocumentSymbol symbol_arg;
                symbol_arg.name = arg.name;
                symbol_arg.kind = lsp::SymbolKind::Variable;
                symbol_arg.detail = arg.type;

                if (!arg_default_value_started && !arg.default_value.empty()) {
                    arg_default_value_started = true;
                }
                String arg_str = arg.name + ": " + arg.type;
                if (arg_default_value_started) {
                    arg_str += " = " + arg.default_value;
                }
                if (j < data.arguments.size() - 1) {
                    arg_str += (", ");
                }
                params += arg_str;
                symbol.children.emplace_back(eastl::move(symbol_arg));
            }
            if (StringUtils::contains(data.qualifiers,"vararg")) {
                params += (params.empty() ? "..." : ", ...");
            }

            String return_type = data.return_type;
            if (return_type.empty()) {
                return_type = "void";
            }
            symbol.detail = String("func ") + class_name + "." + data.name + "(" + params + ") -> " + return_type;

            symbol.documentation = data.description;
            class_symbol.children.push_back(symbol);
        }

        native_symbols.emplace(class_name, class_symbol);
    }

    reload_all_workspace_scripts();

    if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
        for (auto & E : native_symbols) {
            ClassMembers members;
            const lsp::DocumentSymbol &class_symbol = E.second;
            for (int i = 0; i < class_symbol.children.size(); i++) {
                const lsp::DocumentSymbol &symbol = class_symbol.children[i];
                members.emplace(symbol.name, &symbol);
            }
            native_members[E.first] = members;
        }

        // cache member completions
        for (const auto &S : scripts) {
            S.second->get_member_completions();
        }
    }

    return OK;
}

Error GDScriptWorkspace::parse_script(se_string_view p_path, se_string_view p_content) {

    ExtendGDScriptParser *parser = memnew(ExtendGDScriptParser);
    Error err = parser->parse(p_content, p_path);
    auto last_parser = parse_results.find_as(p_path);
    auto last_script = scripts.find_as(p_path);

    if (err == OK) {

        remove_cache_parser(p_path);
        parse_results[String(p_path)] = parser;
        scripts[String(p_path)] = parser;

    } else {
        if (last_parser!=parse_results.end() && last_script!=scripts.end() && last_parser->second != last_script->second) {
            memdelete(last_parser->second);
        }
        parse_results[String(p_path)] = parser;
    }

    publish_diagnostics(p_path);

    return err;
}

Error GDScriptWorkspace::parse_local_script(se_string_view p_path) {
    Error err;
    String content = FileAccess::get_file_as_string(p_path, &err);
    if (err == OK) {
        err = parse_script(p_path, content);
    }
    return err;
}

String GDScriptWorkspace::get_file_path(se_string_view p_uri) const {
    String path = StringUtils::replace(p_uri,root_uri + "/", ("res://"));
    path = StringUtils::http_unescape(path);
    return path;
}

String GDScriptWorkspace::get_file_uri(se_string_view p_path) const {
    String uri(p_path);
    uri.replace("res://", root_uri + "/");
    return uri;
}

void GDScriptWorkspace::publish_diagnostics(se_string_view p_path) {
    Dictionary params;
    Array errors;
    const auto ele = parse_results.find_as(p_path);
    if (ele!=parse_results.end()) {
        const Vector<lsp::Diagnostic> &list = ele->second->get_diagnostics();
        errors.resize(list.size());
        for (int i = 0; i < list.size(); ++i) {
            errors[i] = list[i].to_json();
        }
    }
    params["diagnostics"] = errors;
    params["uri"] = get_file_uri(p_path);
    GDScriptLanguageProtocol::get_singleton()->notify_client(("textDocument/publishDiagnostics"), params);
}

void GDScriptWorkspace::completion(const lsp::CompletionParams &p_params, Vector<ScriptCodeCompletionOption> *r_options) {

    String path = get_file_path(p_params.textDocument.uri);
    String call_hint;
    bool forced = false;

    if (const ExtendGDScriptParser *parser = get_parse_result(path)) {
        String code = parser->get_text_for_completion(p_params.position);
        GDScriptLanguage::get_singleton()->complete_code(code, path, nullptr, r_options, forced, call_hint);
    }
}

const lsp::DocumentSymbol *GDScriptWorkspace::resolve_symbol(const lsp::TextDocumentPositionParams &p_doc_pos, se_string_view p_symbol_name, bool p_func_requred) {

    const lsp::DocumentSymbol *symbol = nullptr;

    String path = get_file_path(p_doc_pos.textDocument.uri);
    if (const ExtendGDScriptParser *parser = get_parse_result(path)) {

        se_string_view symbol_identifier = p_symbol_name;
        Vector<se_string_view> identifier_parts = StringUtils::split(symbol_identifier,'(');
        if (identifier_parts.size()) {
            symbol_identifier = identifier_parts[0];
        }

        lsp::Position pos = p_doc_pos.position;
        if (symbol_identifier.empty()) {
            Vector2i offset;
            symbol_identifier = parser->get_identifier_under_position(p_doc_pos.position, offset);
            pos.character += offset.y;
        }

        if (!symbol_identifier.empty()) {

            if (ScriptServer::is_global_class(StringName(symbol_identifier))) {

                se_string_view class_path = ScriptServer::get_global_class_path(StringName(symbol_identifier));
                symbol = get_script_symbol(class_path);

            } else {

                ScriptLanguage::LookupResult ret;
                if (OK == GDScriptLanguage::get_singleton()->lookup_code(parser->get_text_for_lookup_symbol(pos, symbol_identifier, p_func_requred), symbol_identifier, path, nullptr, ret)) {

                    if (ret.type == ScriptLanguage::LookupResult::RESULT_SCRIPT_LOCATION) {

                        String target_script_path = path;
                        if (ret.script) {
                            target_script_path = ret.script->get_path();
                        }

                        if (const ExtendGDScriptParser *target_parser = get_parse_result(target_script_path)) {
                            symbol = target_parser->get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(ret.location));
                        }

                    } else {

                        String member = ret.class_member;
                        if (member.empty() && symbol_identifier != se_string_view(ret.class_name)) {
                            member = symbol_identifier;
                        }
                        symbol = get_native_symbol(ret.class_name, member);
                    }
                } else {
                    symbol = parser->get_member_symbol(symbol_identifier);
                }
            }
        }
    }

    return symbol;
}

void GDScriptWorkspace::resolve_related_symbols(const lsp::TextDocumentPositionParams &p_doc_pos, List<const lsp::DocumentSymbol *> &r_list) {

    String path = get_file_path(p_doc_pos.textDocument.uri);
    if (const ExtendGDScriptParser *parser = get_parse_result(path)) {

        Vector2i offset;
        String symbol_identifier(parser->get_identifier_under_position(p_doc_pos.position, offset));

        for (const auto & e : native_members) {
            const ClassMembers &members = e.second;
            auto symbol = members.find(symbol_identifier);
            if (symbol!=members.end()) {
                r_list.push_back(symbol->second);
            }
        }

        for (const auto &E : scripts) {
            const ExtendGDScriptParser *script = E.second;
            const ClassMembers &members = script->get_members();
            auto symbol = members.find(symbol_identifier);
            if (symbol != members.end()) {
                r_list.push_back(symbol->second);
            }

            const DefHashMap<String, ClassMembers> &inner_classes = script->get_inner_classes();
            for(const auto &_class : inner_classes) {

                const ClassMembers &inner_class = _class.second;
                auto symbol = inner_class.find(symbol_identifier);
                if (symbol!=inner_class.end()) {
                    r_list.push_back(symbol->second);
                }
            }
        }
    }
}

const lsp::DocumentSymbol *GDScriptWorkspace::resolve_native_symbol(const lsp::NativeSymbolInspectParams &p_params) {

    auto E = native_symbols.find(StringName(p_params.native_class));
    if (E!=native_symbols.end()) {
        const lsp::DocumentSymbol &symbol = E->second;
        if (p_params.symbol_name.empty() || p_params.symbol_name == symbol.name) {
            return &symbol;
        }

        for (int i = 0; i < symbol.children.size(); ++i) {
            if (symbol.children[i].name == p_params.symbol_name) {
                return &(symbol.children[i]);
            }
        }
    }

    return nullptr;
}

void GDScriptWorkspace::resolve_document_links(se_string_view p_uri, Vector<lsp::DocumentLink> &r_list) {
    if (const ExtendGDScriptParser *parser = get_parse_successed_script(get_file_path(p_uri))) {
        const Vector<lsp::DocumentLink> &links = parser->get_document_links();
        for (const lsp::DocumentLink &E : links) {
            r_list.push_back(E);
        }
    }
}
Dictionary GDScriptWorkspace::generate_script_api(se_string_view p_path) {
    Dictionary api;
    if (const ExtendGDScriptParser *parser = get_parse_successed_script(p_path)) {
        api = parser->generate_api();
    }
    return api;
}
Error GDScriptWorkspace::resolve_signature(const lsp::TextDocumentPositionParams &p_doc_pos, lsp::SignatureHelp &r_signature) {
    if (const ExtendGDScriptParser *parser = get_parse_result(get_file_path(p_doc_pos.textDocument.uri))) {

        lsp::TextDocumentPositionParams text_pos;
        text_pos.textDocument = p_doc_pos.textDocument;

        if (parser->get_left_function_call(p_doc_pos.position, text_pos.position, r_signature.activeParameter) == OK) {

            List<const lsp::DocumentSymbol *> symbols;

            if (const lsp::DocumentSymbol *symbol = resolve_symbol(text_pos)) {
                symbols.push_back(symbol);
            } else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
                GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(text_pos, symbols);
            }

            for (const lsp::DocumentSymbol *symbol : symbols) {
                if (symbol->kind == lsp::SymbolKind::Method || symbol->kind == lsp::SymbolKind::Function) {

                    lsp::SignatureInformation signature_info;
                    signature_info.label = symbol->detail;
                    signature_info.documentation = symbol->render();

                    for (int i = 0; i < symbol->children.size(); i++) {
                        const lsp::DocumentSymbol &arg = symbol->children[i];
                        lsp::ParameterInformation arg_info;
                        arg_info.label = arg.name;
                        signature_info.parameters.push_back(arg_info);
                    }
                    r_signature.signatures.push_back(signature_info);
                    break;
                }
            }

            if (r_signature.signatures.size()) {
                return OK;
            }
        }
    }
    return ERR_METHOD_NOT_FOUND;
}
GDScriptWorkspace::GDScriptWorkspace() {
    ProjectSettings::get_singleton()->get_resource_path();
}

GDScriptWorkspace::~GDScriptWorkspace() {
    Set<se_string_view> cached_parsers;

    for (const auto & E : parse_results) {
        cached_parsers.insert(E.first);
    }

    for (const auto & E : scripts) {
        cached_parsers.insert(E.first);
    }

    for (const se_string_view &E : cached_parsers) {
        remove_cache_parser(E);
    }
}
