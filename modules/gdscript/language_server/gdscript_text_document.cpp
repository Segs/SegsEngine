/*************************************************************************/
/*  gdscript_text_document.cpp                                           */
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

#include "gdscript_text_document.h"
#include "../gdscript.h"
#include "core/os/os.h"
#include "core/method_bind.h"
#include "editor/editor_settings.h"
#include "editor/plugins/script_text_editor.h"
#include "gdscript_extend_parser.h"
#include "gdscript_language_protocol.h"

IMPL_GDCLASS(GDScriptTextDocument)

void GDScriptTextDocument::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("didOpen"), &GDScriptTextDocument::didOpen);
    MethodBinder::bind_method(D_METHOD("didChange"), &GDScriptTextDocument::didChange);
    MethodBinder::bind_method(D_METHOD("nativeSymbol"), &GDScriptTextDocument::nativeSymbol);
    MethodBinder::bind_method(D_METHOD("documentSymbol"), &GDScriptTextDocument::documentSymbol);
    MethodBinder::bind_method(D_METHOD("completion"), &GDScriptTextDocument::completion);
    MethodBinder::bind_method(D_METHOD("resolve"), &GDScriptTextDocument::resolve);
    MethodBinder::bind_method(D_METHOD("foldingRange"), &GDScriptTextDocument::foldingRange);
    MethodBinder::bind_method(D_METHOD("codeLens"), &GDScriptTextDocument::codeLens);
    MethodBinder::bind_method(D_METHOD("documentLink"), &GDScriptTextDocument::documentLink);
    MethodBinder::bind_method(D_METHOD("colorPresentation"), &GDScriptTextDocument::colorPresentation);
    MethodBinder::bind_method(D_METHOD("hover"), &GDScriptTextDocument::hover);
    MethodBinder::bind_method(D_METHOD("definition"), &GDScriptTextDocument::definition);
    MethodBinder::bind_method(D_METHOD("declaration"), &GDScriptTextDocument::declaration);
    MethodBinder::bind_method(D_METHOD("signatureHelp"), &GDScriptTextDocument::signatureHelp);
    MethodBinder::bind_method(D_METHOD("show_native_symbol_in_editor"), &GDScriptTextDocument::show_native_symbol_in_editor);
}

void GDScriptTextDocument::didOpen(const Variant &p_param) {
    lsp::TextDocumentItem doc = load_document_item(p_param);
    sync_script_content(doc.uri, doc.text);
}

void GDScriptTextDocument::didChange(const Variant &p_param) {
    lsp::TextDocumentItem doc = load_document_item(p_param);
    Dictionary dict = p_param;
    Array contentChanges = dict["contentChanges"];
    for (int i = 0; i < contentChanges.size(); ++i) {
        lsp::TextDocumentContentChangeEvent evt;
        evt.load(contentChanges[i]);
        doc.text = evt.text;
    }
    sync_script_content(doc.uri, doc.text);
}

lsp::TextDocumentItem GDScriptTextDocument::load_document_item(const Variant &p_param) {
    lsp::TextDocumentItem doc;
    Dictionary params = p_param;
    doc.load(params["textDocument"]);
    return doc;
}

void GDScriptTextDocument::notify_client_show_symbol(const lsp::DocumentSymbol *symbol) {
    ERR_FAIL_NULL(symbol);
    GDScriptLanguageProtocol::get_singleton()->notify_client(("gdscript/show_native_symbol"), symbol->to_json(true));
}
void GDScriptTextDocument::initialize() {

    if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {

        const DefHashMap<StringName, ClassMembers> &native_members = GDScriptLanguageProtocol::get_singleton()->get_workspace()->native_members;

        for(const auto & class_p : native_members) {

            const ClassMembers &members = class_p.second;

            for(const auto &e : members) {

                const lsp::DocumentSymbol *symbol = e.second;
                lsp::CompletionItem item = symbol->make_completion_item();
                item.data = JOIN_SYMBOLS(String(class_p.first), e.first);
                native_member_completions.push_back(item.to_json());
            }
        }
    }
}

Variant GDScriptTextDocument::nativeSymbol(const Dictionary &p_params) {

    Variant ret;

    lsp::NativeSymbolInspectParams params;
    params.load(p_params);

    if (const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_native_symbol(params)) {
        ret = symbol->to_json(true);
        notify_client_show_symbol(symbol);
    }

    return ret;
}
Array GDScriptTextDocument::documentSymbol(const Dictionary &p_params) {
    Dictionary params = p_params["textDocument"];
    String uri = params["uri"].as<String>();
    String path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(uri);
    Array arr;
    const auto &scripts(GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts);
    const auto parser = scripts.find_as(path);
    if (parser == scripts.end()) {
        return arr;
    }

    Vector<lsp::DocumentedSymbolInformation> list;
    parser->second->get_symbols().symbol_tree_as_list(uri, list);
    for (int i = 0; i < list.size(); i++) {
        arr.push_back(list[i].to_json());
    }
    return arr;
}

Array GDScriptTextDocument::completion(const Dictionary &p_params) {

    Array arr;

    lsp::CompletionParams params;
    params.load(p_params);
    Dictionary request_data = params.to_json();

    Vector<ScriptCodeCompletionOption> options;
    GDScriptLanguageProtocol::get_singleton()->get_workspace()->completion(params, &options);

    if (!options.empty()) {

        int i = 0;
        arr.resize(options.size());

        for (const ScriptCodeCompletionOption& option : options) {

            lsp::CompletionItem item;
            item.label = option.display;
            item.data = request_data;

            switch (option.kind) {
                case ScriptCodeCompletionOption::KIND_ENUM:
                    item.kind = lsp::CompletionItemKind::Enum;
                    break;
                case ScriptCodeCompletionOption::KIND_CLASS:
                    item.kind = lsp::CompletionItemKind::Class;
                    break;
                case ScriptCodeCompletionOption::KIND_MEMBER:
                    item.kind = lsp::CompletionItemKind::Property;
                    break;
                case ScriptCodeCompletionOption::KIND_FUNCTION:
                    item.kind = lsp::CompletionItemKind::Method;
                    break;
                case ScriptCodeCompletionOption::KIND_SIGNAL:
                    item.kind = lsp::CompletionItemKind::Event;
                    break;
                case ScriptCodeCompletionOption::KIND_CONSTANT:
                    item.kind = lsp::CompletionItemKind::Constant;
                    break;
                case ScriptCodeCompletionOption::KIND_VARIABLE:
                    item.kind = lsp::CompletionItemKind::Variable;
                    break;
                case ScriptCodeCompletionOption::KIND_FILE_PATH:
                    item.kind = lsp::CompletionItemKind::File;
                    break;
                case ScriptCodeCompletionOption::KIND_NODE_PATH:
                    item.kind = lsp::CompletionItemKind::Snippet;
                    break;
                case ScriptCodeCompletionOption::KIND_PLAIN_TEXT:
                    item.kind = lsp::CompletionItemKind::Text;
                    break;
            }

            arr[i] = item.to_json();
            i++;
        }
    } else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {

        arr = native_member_completions.duplicate();

        for (const auto &E : GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts) {

            ExtendGDScriptParser *script = E.second;
            const Array &items = script->get_member_completions();

            const int start_size = arr.size();
            arr.resize(start_size + items.size());
            for (int i = start_size; i < arr.size(); i++) {
                arr[i] = items[i - start_size];
            }
        }
    }
    return arr;
}

Dictionary GDScriptTextDocument::resolve(const Dictionary &p_params) {

    lsp::CompletionItem item;
    item.load(p_params);

    lsp::CompletionParams params;
    Variant data = p_params["data"];
    auto ws(GDScriptLanguageProtocol::get_singleton()->get_workspace());
    const lsp::DocumentSymbol *symbol = nullptr;

    if (data.get_type() == VariantType::DICTIONARY) {

        params.load(p_params["data"]);
        symbol = ws->resolve_symbol(params, item.label, item.kind == lsp::CompletionItemKind::Method || item.kind == lsp::CompletionItemKind::Function);

    } else if (data.get_type() == VariantType::STRING) {

        String query = data;

        Vector<se_string_view> param_symbols = StringUtils::split(query,(SYMBOL_SEPERATOR), false);

        if (param_symbols.size() >= 2) {

            se_string_view class_ = param_symbols[0];
            StringName class_name = StringName(class_);
            se_string_view member_name = param_symbols[param_symbols.size() - 1];
            se_string_view inner_class_name;
            if (param_symbols.size() >= 3) {
                inner_class_name = param_symbols[1];
            }
            const auto members = ws->native_members.find_as(se_string_view(class_name));
            if (members == ws->native_members.end()) {
                const auto member = members->second.find_as(se_string_view(member_name));
                if (member!=members->second.end()) {
                    symbol = member->second;
                }
            }

            if (!symbol) {
                const auto E = ws->scripts.find_as(class_name);
                if (E!=ws->scripts.end()) {
                    symbol = E->second->get_member_symbol(member_name, inner_class_name);
                }
            }
        }
    }

    if (symbol) {
        item.documentation = symbol->render();
    }

    if ((item.kind == lsp::CompletionItemKind::Method || item.kind == lsp::CompletionItemKind::Function) &&
            !StringUtils::ends_with(item.label, "):")) {
        item.insertText = item.label + "(";
        if (symbol && symbol->children.empty()) {
            item.insertText += ')';
        }
    } else if (item.kind == lsp::CompletionItemKind::Event) {
        if (params.context.triggerKind == lsp::CompletionTriggerKind::TriggerCharacter && (params.context.triggerCharacter == "(")) {
            const char * quote_style(EDITOR_DEF(("text_editor/completion/use_single_quotes"), false) ? "'" : "\"");
            item.insertText = quote_style + item.label + quote_style;
        }
    }

    return item.to_json(true);
}

Array GDScriptTextDocument::foldingRange(const Dictionary &p_params) {
    Array arr;
    return arr;
}

Array GDScriptTextDocument::codeLens(const Dictionary &p_params) {
    Array arr;
    return arr;
}

Array GDScriptTextDocument::documentLink(const Dictionary &p_params) {
    Array ret;

    lsp::DocumentLinkParams params;
    params.load(p_params);

    Vector<lsp::DocumentLink> links;
    GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_document_links(params.textDocument.uri, links);
    for (const lsp::DocumentLink &E : links) {
        ret.push_back(E.to_json());
    }
    return ret;
}

Array GDScriptTextDocument::colorPresentation(const Dictionary &p_params) {
    Array arr;
    return arr;
}

Variant GDScriptTextDocument::hover(const Dictionary &p_params) {

    lsp::TextDocumentPositionParams params;
    params.load(p_params);

    const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params);
    if (symbol) {

        lsp::Hover hover;
        hover.contents = symbol->render();
        return hover.to_json();

    } else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {

        Dictionary ret;
        Array contents;
        List<const lsp::DocumentSymbol *> list;
        GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(params, list);
        for (const lsp::DocumentSymbol *s : list) {
            if ( s ) {
                contents.push_back(s->render().value);
            }
        }
        ret["contents"] = contents;
        return ret;
    }

    return Variant();
}
Array GDScriptTextDocument::definition(const Dictionary &p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);
    List<const lsp::DocumentSymbol *> symbols;
    Array arr = this->find_symbols(params, symbols);
    return arr;
}
Variant GDScriptTextDocument::declaration(const Dictionary &p_params) {
    lsp::TextDocumentPositionParams params;
    params.load(p_params);
    List<const lsp::DocumentSymbol *> symbols;
    Array arr = this->find_symbols(params, symbols);
    if (arr.empty() && !symbols.empty() && !symbols.front()->native_class.empty()) { // Find a native symbol
        const lsp::DocumentSymbol *symbol = symbols.front();
        if (GDScriptLanguageProtocol::get_singleton()->is_goto_native_symbols_enabled()) {
            String id;
            switch (symbol->kind) {
                case lsp::SymbolKind::Class:
                    id = "class_name:" + symbol->name;
                    break;
                case lsp::SymbolKind::Constant:
                    id = "class_constant:" + symbol->native_class + ":" + symbol->name;
                    break;
                case lsp::SymbolKind::Property:
                case lsp::SymbolKind::Variable:
                    id = "class_property:" + symbol->native_class + ":" + symbol->name;
                    break;
                case lsp::SymbolKind::Enum:
                    id = "class_enum:" + symbol->native_class + ":" + symbol->name;
                    break;
                case lsp::SymbolKind::Method:
                case lsp::SymbolKind::Function:
                    id = "class_method:" + symbol->native_class + ":" + symbol->name;
                    break;
                default:
                    id = "class_global:" + symbol->native_class + ":" + symbol->name;
                    break;
            }
            call_deferred("show_native_symbol_in_editor", id);
        } else {
            notify_client_show_symbol(symbol);
        }
    }
    return arr;
}
Variant GDScriptTextDocument::signatureHelp(const Dictionary &p_params) {
    Variant ret;

    lsp::TextDocumentPositionParams params;
    params.load(p_params);

    lsp::SignatureHelp s;
    if (OK == GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_signature(params, s)) {
        ret = s.to_json();
    }

    return ret;
}
GDScriptTextDocument::GDScriptTextDocument() {
    file_checker = FileAccess::create(FileAccess::ACCESS_RESOURCES);
}

GDScriptTextDocument::~GDScriptTextDocument() {
    memdelete(file_checker);
}

void GDScriptTextDocument::sync_script_content(se_string_view p_path, se_string_view p_content) {
    auto wp = GDScriptLanguageProtocol::get_singleton()->get_workspace();
    String path = wp->get_file_path(p_path);
    wp->parse_script(path, p_content);
}

void GDScriptTextDocument::show_native_symbol_in_editor(se_string_view p_symbol_id) {
    ScriptEditor::get_singleton()->call_deferred("_help_class_goto", p_symbol_id);
    OS::get_singleton()->move_window_to_foreground();
}
Array GDScriptTextDocument::find_symbols(const lsp::TextDocumentPositionParams &p_location, List<const lsp::DocumentSymbol *> &r_list) {
    Array arr;
    const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(p_location);
    if (symbol) {
        lsp::Location location;
        location.uri = symbol->uri;
        location.range = symbol->range;
        const String &path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(symbol->uri);
        if (file_checker->file_exists(path)) {
            arr.push_back(location.to_json());
        }
        r_list.push_back(symbol);
    } else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
        List<const lsp::DocumentSymbol *> list;
        GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(p_location, list);
        for (const lsp::DocumentSymbol *s : list) {
            if (s) {
                if (!s->uri.empty()) {
                    lsp::Location location;
                    location.uri = s->uri;
                    location.range = s->range;
                    arr.push_back(location.to_json());
                    r_list.push_back(s);
                }
            }
        }
    }
    return arr;
}
