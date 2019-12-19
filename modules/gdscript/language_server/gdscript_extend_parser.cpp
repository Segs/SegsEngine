/*************************************************************************/
/*  gdscript_extend_parser.cpp                                           */
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

#include "gdscript_extend_parser.h"
#include "../gdscript.h"
#include "core/io/json.h"
#include "gdscript_language_protocol.h"
#include "gdscript_workspace.h"

void ExtendGDScriptParser::update_diagnostics() {

    diagnostics.clear();

    if (has_error()) {
        lsp::Diagnostic diagnostic;
        diagnostic.severity = lsp::DiagnosticSeverity::Error;
        diagnostic.message = get_error();
        diagnostic.source = "gdscript";
        diagnostic.code = -1;
        lsp::Range range;
        lsp::Position pos;
        int line = LINE_NUMBER_TO_INDEX(get_error_line());
        const se_string &line_text = get_lines()[line];
        pos.line = line;
        pos.character = line_text.length() - StringUtils::strip_edges(line_text,true, false).length();
        range.start = pos;
        range.end = range.start;
        range.end.character = StringUtils::strip_edges(line_text,false).length();
        diagnostic.range = range;
        diagnostics.push_back(diagnostic);
    }
#ifdef DEBUG_ENABLED
    const List<GDScriptWarning> &warnings = get_warnings();
    for (const List<GDScriptWarning>::Element *E = warnings.front(); E; E = E->next()) {
        const GDScriptWarning &warning = E->deref();
        lsp::Diagnostic diagnostic;
        diagnostic.severity = lsp::DiagnosticSeverity::Warning;
        diagnostic.message = warning.get_message();
        diagnostic.source = "gdscript";
        diagnostic.code = warning.code;
        lsp::Range range;
        lsp::Position pos;
        int line = LINE_NUMBER_TO_INDEX(warning.line);
        const se_string &line_text = get_lines()[line];
        pos.line = line;
        pos.character = line_text.length() - StringUtils::strip_edges(line_text,true, false).length();
        range.start = pos;
        range.end = pos;
        range.end.character = StringUtils::strip_edges(line_text,false).length();
        diagnostic.range = range;
        diagnostics.push_back(diagnostic);
    }
#endif
}

void ExtendGDScriptParser::update_symbols() {

    members.clear();

    const GDScriptParser::Node *head = get_parse_tree();
    if (const GDScriptParser::ClassNode *gdclass = dynamic_cast<const GDScriptParser::ClassNode *>(head)) {

        parse_class_symbol(gdclass, class_symbol);

        for (int i = 0; i < class_symbol.children.size(); i++) {
            const lsp::DocumentSymbol &symbol = class_symbol.children[i];
            members[symbol.name] = &symbol;

            // cache level one inner classes
            if (symbol.kind == lsp::SymbolKind::Class) {
                ClassMembers inner_class;
                for (int j = 0; j < symbol.children.size(); j++) {
                    const lsp::DocumentSymbol &s = symbol.children[j];
                    inner_class.emplace(s.name,&s);
                }
                inner_classes.emplace(symbol.name, inner_class);
            }
        }
    }
}

void ExtendGDScriptParser::update_document_links(se_string_view p_code) {
    document_links.clear();

    GDScriptTokenizerText tokenizer;
    FileAccessRef fs = FileAccess::create(FileAccess::ACCESS_RESOURCES);
    tokenizer.set_code(p_code);
    while (true) {
        if (tokenizer.get_token() == GDScriptTokenizer::TK_EOF) {
            break;
        } else if (tokenizer.get_token() == GDScriptTokenizer::TK_CONSTANT) {
            Variant const_val = tokenizer.get_token_constant();
            if (const_val.get_type() == VariantType::STRING) {
                se_string path = const_val;
                bool exists = fs->file_exists(path);
                if (!exists) {
                    path = PathUtils::get_base_dir(get_path()) + "/" + path;
                    exists = fs->file_exists(path);
                }
                if (exists) {
                    se_string value = const_val;
                    lsp::DocumentLink link;
                    link.target = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_uri(path);
                    link.range.start.line = LINE_NUMBER_TO_INDEX(tokenizer.get_token_line());
                    link.range.end.line = link.range.start.line;
                    link.range.end.character = LINE_NUMBER_TO_INDEX(tokenizer.get_token_column());
                    link.range.start.character = link.range.end.character - value.length();
                    document_links.push_back(link);
                }
            }
        }
        tokenizer.advance();
    }
}
void ExtendGDScriptParser::parse_class_symbol(const GDScriptParser::ClassNode *p_class, lsp::DocumentSymbol &r_symbol) {

    const se_string uri = get_uri();

    r_symbol.uri = uri;
    r_symbol.script_path = path;
    r_symbol.children.clear();
    r_symbol.name = p_class->name.asCString();
    if (r_symbol.name.empty())
        r_symbol.name = PathUtils::get_file(path);
    r_symbol.kind = lsp::SymbolKind::Class;
    r_symbol.deprecated = false;
    r_symbol.range.start.line = LINE_NUMBER_TO_INDEX(p_class->line);
    r_symbol.range.start.character = p_class->column;
    r_symbol.range.end.line = LINE_NUMBER_TO_INDEX(p_class->end_line);
    r_symbol.selectionRange.start.line = r_symbol.range.start.line;
    r_symbol.detail = "class " + r_symbol.name;
    bool is_root_class = &r_symbol == &class_symbol;
    r_symbol.documentation = parse_documentation(is_root_class ? 0 : LINE_NUMBER_TO_INDEX(p_class->line), is_root_class);

    for (int i = 0; i < p_class->variables.size(); ++i) {

        const GDScriptParser::ClassNode::Member &m = p_class->variables[i];

        lsp::DocumentSymbol symbol;
        symbol.name = m.identifier;
        symbol.kind = lsp::SymbolKind::Variable;
        symbol.deprecated = false;
        const int line = LINE_NUMBER_TO_INDEX(m.line);
        symbol.range.start.line = line;
        symbol.range.start.character = lines[line].length() - StringUtils::strip_edges(lines[line],true, false).length();
        symbol.range.end.line = line;
        symbol.range.end.character = lines[line].length();
        symbol.selectionRange.start.line = symbol.range.start.line;
        if (m._export.type != VariantType::NIL) {
            symbol.detail += ("export ");
        }
        symbol.detail += se_string("var ") + m.identifier;
        if (m.data_type.kind != GDScriptParser::DataType::UNRESOLVED) {
            symbol.detail += ": " + m.data_type.to_string();
        }
        if (m.default_value.get_type() != VariantType::NIL) {
            symbol.detail += " = " + JSON::print(m.default_value);
        }

        symbol.documentation = parse_documentation(line);
        symbol.uri = uri;
        symbol.script_path = path;

        r_symbol.children.push_back(symbol);
    }

    for (int i = 0; i < p_class->_signals.size(); ++i) {
        const GDScriptParser::ClassNode::Signal &signal = p_class->_signals[i];

        lsp::DocumentSymbol symbol;
        symbol.name = signal.name;
        symbol.kind = lsp::SymbolKind::Event;
        symbol.deprecated = false;
        const int line = LINE_NUMBER_TO_INDEX(signal.line);
        symbol.range.start.line = line;
        symbol.range.start.character = lines[line].length() - StringUtils::strip_edges(lines[line],true, false).length();
        symbol.range.end.line = symbol.range.start.line;
        symbol.range.end.character = lines[line].length();
        symbol.selectionRange.start.line = symbol.range.start.line;
        symbol.documentation = parse_documentation(line);
        symbol.uri = uri;
        symbol.script_path = path;
        symbol.detail = "signal " + se_string(signal.name) + "(";
        for (int j = 0; j < signal.arguments.size(); j++) {
            if (j > 0) {
                symbol.detail += (", ");
            }
            symbol.detail += signal.arguments[j];
        }
        symbol.detail += (")");

        r_symbol.children.push_back(symbol);
    }

    for (const eastl::pair<StringName, GDScriptParser::ClassNode::Constant> &E : p_class->constant_expressions) {
        lsp::DocumentSymbol symbol;
        const GDScriptParser::ClassNode::Constant &c = E.second;
        const GDScriptParser::ConstantNode *node = dynamic_cast<const GDScriptParser::ConstantNode *>(c.expression);
        ERR_FAIL_COND(!node);
        symbol.name = E.first;
        symbol.kind = lsp::SymbolKind::Constant;
        symbol.deprecated = false;
        const int line = LINE_NUMBER_TO_INDEX(E.second.expression->line);
        symbol.range.start.line = line;
        symbol.range.start.character = E.second.expression->column;
        symbol.range.end.line = symbol.range.start.line;
        symbol.range.end.character = lines[line].length();
        symbol.selectionRange.start.line = symbol.range.start.line;
        symbol.documentation = parse_documentation(line);
        symbol.uri = uri;
        symbol.script_path = path;

        symbol.detail = "const " + symbol.name;
        if (c.type.kind != GDScriptParser::DataType::UNRESOLVED) {
            symbol.detail += ": " + c.type.to_string();
        }

        se_string value_text;
        if (node->value.get_type() == VariantType::OBJECT) {
            RES res(node->value);
            if (res && !res->get_path().empty()) {
                value_text = "preload(\"" + res->get_path() + "\")";
                if (symbol.documentation.empty()) {
                    auto S = GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts.find(res->get_path());
                    if (S!=GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts.end()) {
                        symbol.documentation = S->second->class_symbol.documentation;
                    }
                }
            } else {
                value_text = JSON::print(node->value);
            }
        } else {
            value_text = JSON::print(node->value);
        }
        if (!value_text.empty()) {
            symbol.detail += " = " + value_text;
        }

        r_symbol.children.push_back(symbol);
    }

    for (int i = 0; i < p_class->functions.size(); ++i) {
        const GDScriptParser::FunctionNode *func = p_class->functions[i];
        lsp::DocumentSymbol symbol;
        parse_function_symbol(func, symbol);
        r_symbol.children.push_back(symbol);
    }

    for (int i = 0; i < p_class->static_functions.size(); ++i) {
        const GDScriptParser::FunctionNode *func = p_class->static_functions[i];
        lsp::DocumentSymbol symbol;
        parse_function_symbol(func, symbol);
        r_symbol.children.push_back(symbol);
    }

    for (int i = 0; i < p_class->subclasses.size(); ++i) {
        const GDScriptParser::ClassNode *subclass = p_class->subclasses[i];
        lsp::DocumentSymbol symbol;
        parse_class_symbol(subclass, symbol);
        r_symbol.children.push_back(symbol);
    }
}

void ExtendGDScriptParser::parse_function_symbol(const GDScriptParser::FunctionNode *p_func, lsp::DocumentSymbol &r_symbol) {

    const se_string uri = get_uri();

    r_symbol.name = p_func->name;
    r_symbol.kind = lsp::SymbolKind::Function;
    r_symbol.detail = se_string("func ") + p_func->name + "(";
    r_symbol.deprecated = false;
    const int line = LINE_NUMBER_TO_INDEX(p_func->line);
    r_symbol.range.start.line = line;
    r_symbol.range.start.character = p_func->column;
    r_symbol.range.end.line = MAX(p_func->body->end_line - 2, p_func->body->line);
    r_symbol.range.end.character = lines[r_symbol.range.end.line].length();
    r_symbol.selectionRange.start.line = r_symbol.range.start.line;
    r_symbol.documentation = parse_documentation(line);
    r_symbol.uri = uri;
    r_symbol.script_path = path;

    se_string arguments;
    for (int i = 0; i < p_func->arguments.size(); i++) {
        lsp::DocumentSymbol symbol;
        symbol.kind = lsp::SymbolKind::Variable;
        symbol.name = p_func->arguments[i];
        symbol.range.start.line = LINE_NUMBER_TO_INDEX(p_func->body->line);
        symbol.range.start.character = p_func->body->column;
        symbol.range.end = symbol.range.start;
        symbol.uri = uri;
        symbol.script_path = path;
        r_symbol.children.push_back(symbol);
        if (i > 0) {
            arguments += (", ");
        }
        arguments += (p_func->arguments[i]);
        if (p_func->argument_types[i].kind != GDScriptParser::DataType::UNRESOLVED) {
            arguments += ": " + p_func->argument_types[i].to_string();
        }
        int default_value_idx = i - (p_func->arguments.size() - p_func->default_values.size());
        if (default_value_idx >= 0) {
            const GDScriptParser::ConstantNode *const_node = dynamic_cast<const GDScriptParser::ConstantNode *>(p_func->default_values[default_value_idx]);
            if (const_node == nullptr) {
                const GDScriptParser::OperatorNode *operator_node = dynamic_cast<const GDScriptParser::OperatorNode *>(p_func->default_values[default_value_idx]);
                if (operator_node) {
                    const_node = dynamic_cast<const GDScriptParser::ConstantNode *>(operator_node->next);
                }
            }

            if (const_node) {
                se_string value = JSON::print(const_node->value);
                arguments += " = " + value;
            }
        }
    }
    r_symbol.detail += arguments + ")";
    if (p_func->return_type.kind != GDScriptParser::DataType::UNRESOLVED) {
        r_symbol.detail += " -> " + p_func->return_type.to_string();
    }

    for (const eastl::pair<const StringName, LocalVarNode *> &E : p_func->body->variables) {
        lsp::DocumentSymbol symbol;
        const GDScriptParser::LocalVarNode *var = E.second;
        symbol.name = E.first;
        symbol.kind = lsp::SymbolKind::Variable;
        symbol.range.start.line = LINE_NUMBER_TO_INDEX(E.second->line);
        symbol.range.start.character = E.second->column;
        symbol.range.end.line = symbol.range.start.line;
        symbol.range.end.character = lines[symbol.range.end.line].length();
        symbol.uri = uri;
        symbol.script_path = path;
        symbol.detail = "var " + symbol.name;
        if (var->datatype.kind != GDScriptParser::DataType::UNRESOLVED) {
            symbol.detail += ": " + var->datatype.to_string();
        }
        symbol.documentation = parse_documentation(line);
        r_symbol.children.push_back(symbol);
    }
}

se_string ExtendGDScriptParser::parse_documentation(int p_line, bool p_docs_down) {
    ERR_FAIL_INDEX_V(p_line, lines.size(), se_string())

    List<se_string_view> doc_lines;

    if (!p_docs_down) { // inline comment
        se_string_view inline_comment = lines[p_line];
        auto comment_start = StringUtils::find(inline_comment,"#");
        if (comment_start != se_string::npos) {
            inline_comment = StringUtils::strip_edges(StringUtils::substr(inline_comment,comment_start, inline_comment.length()));
            if (inline_comment.length() > 1) {
                doc_lines.push_back(StringUtils::substr(inline_comment,1, inline_comment.length()));
            }
        }
    }

    int step = p_docs_down ? 1 : -1;
    int start_line = p_docs_down ? p_line : p_line - 1;
    for (int i = start_line; true; i += step) {

        if (i < 0 || i >= lines.size()) break;

        se_string_view line_comment = StringUtils::strip_edges(lines[i],true, false);
        if (StringUtils::begins_with(line_comment,"#")) {
            line_comment = StringUtils::substr(line_comment,1, line_comment.length());
            if (p_docs_down) {
                doc_lines.push_back(line_comment);
            } else {
                doc_lines.push_front(line_comment);
            }
        } else {
            break;
        }
    }

    se_string doc;
    for (List<se_string_view>::Element *E = doc_lines.front(); E; E = E->next()) {
        doc.append(E->deref());
        doc.push_back('\n');
    }
    return doc;
}

se_string ExtendGDScriptParser::get_text_for_completion(const lsp::Position &p_cursor) const {

    se_string longthing;
    int len = lines.size();
    for (int i = 0; i < len; i++) {

        if (i == p_cursor.line) {
            longthing += StringUtils::substr(lines[i],0, p_cursor.character);
            longthing += c_cursor_marker; //not utf8, represents the cursor
            longthing += StringUtils::substr(lines[i],p_cursor.character, lines[i].size());
        } else {

            longthing += lines[i];
        }

        if (i != len - 1)
            longthing += '\n';
    }

    return longthing;
}

se_string ExtendGDScriptParser::get_text_for_lookup_symbol(const lsp::Position &p_cursor, se_string_view p_symbol, bool p_func_requred) const {
    se_string longthing;
    int len = lines.size();
    for (int i = 0; i < len; i++) {

        if (i == p_cursor.line) {
            se_string_view line = lines[i];
            se_string first_part(StringUtils::substr(line,0, p_cursor.character));
            se_string last_part(StringUtils::substr(line,p_cursor.character + 1));
            if (!p_symbol.empty()) {
                se_string_view left_cursor_text;
                for (int c = p_cursor.character - 1; c >= 0; c--) {
                    left_cursor_text = StringUtils::substr(line,c, p_cursor.character - c);
                    if (StringUtils::begins_with(p_symbol,left_cursor_text)) {
                        first_part = StringUtils::substr(line,0, c);
                        first_part += p_symbol;
                        break;
                    }
                }
            }

            longthing += first_part;
            longthing += c_cursor_marker; //not utf8, represents the cursor
            if (p_func_requred) {
                longthing += '('; // tell the parser this is a function call
            }
            longthing += last_part;
        } else {

            longthing += lines[i];
        }

        if (i != len - 1)
            longthing += '\n';
    }

    return longthing;
}

se_string ExtendGDScriptParser::get_identifier_under_position(const lsp::Position &p_position, Vector2i &p_offset) const {

    ERR_FAIL_INDEX_V(p_position.line, lines.size(), {})
    se_string_view line = lines[p_position.line];
    ERR_FAIL_INDEX_V(p_position.character, line.size(), {})

    int start_pos = p_position.character;
    for (int c = p_position.character; c >= 0; c--) {
        start_pos = c;
        char ch = line[c];
        bool valid_char = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
        if (!valid_char) {
            break;
        }
    }

    int end_pos = p_position.character;
    for (size_t c = p_position.character; c < line.length(); c++) {
        char ch = line[c];
        bool valid_char = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
        if (!valid_char) {
            break;
        }
        end_pos = c;
    }
    if (start_pos < end_pos) {
        p_offset.x = start_pos - p_position.character;
        p_offset.y = end_pos - p_position.character;
        return se_string(StringUtils::substr(line,start_pos + 1, end_pos - start_pos));
    }

    return null_se_string;
}

se_string  ExtendGDScriptParser::get_uri() const {
    return GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_uri(path);
}

const lsp::DocumentSymbol *ExtendGDScriptParser::search_symbol_defined_at_line(int p_line, const lsp::DocumentSymbol &p_parent) const {
    const lsp::DocumentSymbol *ret = nullptr;
    if (p_line < p_parent.range.start.line) {
        return ret;
    } else if (p_parent.range.start.line == p_line) {
        return &p_parent;
    } else {
        for (int i = 0; i < p_parent.children.size(); i++) {
            ret = search_symbol_defined_at_line(p_line, p_parent.children[i]);
            if (ret) {
                break;
            }
        }
    }
    return ret;
}

const lsp::DocumentSymbol *ExtendGDScriptParser::get_symbol_defined_at_line(int p_line) const {
    if (p_line <= 0) {
        return &class_symbol;
    }
    return search_symbol_defined_at_line(p_line, class_symbol);
}

const lsp::DocumentSymbol *ExtendGDScriptParser::get_member_symbol(se_string_view p_name, se_string_view p_subclass) const {

    if (p_subclass.empty()) {
        auto ptr = members.find_as(p_name);
        if (ptr!=members.end()) {
            return ptr->second;
        }
    } else {
        auto iter_class = inner_classes.find_as(p_subclass);
        if (iter_class != inner_classes.end()) {
            auto ptr = iter_class->second.find_as(p_name);
            if (ptr!=iter_class->second.end()) {
                return ptr->second;
            }
        }
    }

    return nullptr;
}

const List<lsp::DocumentLink> &ExtendGDScriptParser::get_document_links() const {
    return document_links;
}

const Array &ExtendGDScriptParser::get_member_completions() {

    if (member_completions.empty()) {

        for(const auto & entry : members) {

            const lsp::DocumentSymbol *symbol = entry.second;
            lsp::CompletionItem item = symbol->make_completion_item();
            item.data = JOIN_SYMBOLS(path, entry.first);
            member_completions.push_back(item.to_json());
        }

        for(const auto & entry : inner_classes) {

            const ClassMembers &inner_class = entry.second;
            for(const auto & member_name : inner_class) {
                const lsp::DocumentSymbol *symbol = member_name.second;
                lsp::CompletionItem item = symbol->make_completion_item();
                item.data = JOIN_SYMBOLS(path, JOIN_SYMBOLS(entry.first, member_name.first));
                member_completions.push_back(item.to_json());
            }
        }
    }

    return member_completions;
}

Dictionary ExtendGDScriptParser::dump_function_api(const GDScriptParser::FunctionNode *p_func) const {
    Dictionary func;
    ERR_FAIL_NULL_V(p_func, func)
    func["name"] = p_func->name;
    func["return_type"] = p_func->return_type.to_string();
    func["rpc_mode"] = (int)p_func->rpc_mode;
    Array arguments;
    for (int i = 0; i < p_func->arguments.size(); i++) {
        Dictionary arg;
        arg["name"] = p_func->arguments[i];
        arg["type"] = p_func->argument_types[i].to_string();
        int default_value_idx = i - (p_func->arguments.size() - p_func->default_values.size());
        if (default_value_idx >= 0) {
            const GDScriptParser::ConstantNode *const_node = dynamic_cast<const GDScriptParser::ConstantNode *>(p_func->default_values[default_value_idx]);
            if (const_node == nullptr) {
                const GDScriptParser::OperatorNode *operator_node = dynamic_cast<const GDScriptParser::OperatorNode *>(p_func->default_values[default_value_idx]);
                if (operator_node) {
                    const_node = dynamic_cast<const GDScriptParser::ConstantNode *>(operator_node->next);
                }
            }
            if (const_node) {
                arg["default_value"] = const_node->value;
            }
        }
        arguments.push_back(arg);
    }
    if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(p_func->line))) {
        func["signature"] = symbol->detail;
        func["description"] = symbol->documentation;
    }
    func["arguments"] = arguments;
    return func;
}

Dictionary ExtendGDScriptParser::dump_class_api(const GDScriptParser::ClassNode *p_class) const {
    Dictionary class_api;

    ERR_FAIL_NULL_V(p_class, class_api)

    class_api["name"] = p_class->name;
    class_api["path"] = path;
    Array extends_class;
    for (int i = 0; i < p_class->extends_class.size(); i++) {
        extends_class.append(p_class->extends_class[i]);
    }
    class_api["extends_class"] = extends_class;
    class_api["extends_file"] = p_class->extends_file;
    class_api["icon"] = p_class->icon_path;

    if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(p_class->line))) {
        class_api["signature"] = symbol->detail;
        class_api["description"] = symbol->documentation;
    }

    Array subclasses;
    for (int i = 0; i < p_class->subclasses.size(); i++) {
        subclasses.push_back(dump_class_api(p_class->subclasses[i]));
    }
    class_api["sub_classes"] = subclasses;

    Array constants;
    for (const auto &E : p_class->constant_expressions) {

        const GDScriptParser::ClassNode::Constant &c = E.second;
        const GDScriptParser::ConstantNode *node = dynamic_cast<const GDScriptParser::ConstantNode *>(c.expression);
        ERR_FAIL_COND_V(!node, class_api)

        Dictionary api;
        api["name"] = E.first;
        api["value"] = node->value;
        api["data_type"] = node->datatype.to_string();
        if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(node->line))) {
            api["signature"] = symbol->detail;
            api["description"] = symbol->documentation;
        }
        constants.push_back(api);
    }
    class_api["constants"] = constants;

    Array members;
    for (int i = 0; i < p_class->variables.size(); ++i) {
        const GDScriptParser::ClassNode::Member &m = p_class->variables[i];
        Dictionary api;
        api["name"] = m.identifier;
        api["data_type"] = m.data_type.to_string();
        api["default_value"] = m.default_value;
        api["setter"] = (m.setter);
        api["getter"] = (m.getter);
        api["export"] = m._export.type != VariantType::NIL;
        if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.line))) {
            api["signature"] = symbol->detail;
            api["description"] = symbol->documentation;
        }
        members.push_back(api);
    }
    class_api["members"] = members;

    Array defined_signals;
    for (int i = 0; i < p_class->_signals.size(); ++i) {
        const GDScriptParser::ClassNode::Signal &signal = p_class->_signals[i];
        Dictionary api;
        api["name"] = signal.name;
        Array args;
        for (int j = 0; j < signal.arguments.size(); j++) {
            args.append(signal.arguments[j]);
        }
        api["arguments"] = args;
        if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(signal.line))) {
            api["signature"] = symbol->detail;
            api["description"] = symbol->documentation;
        }
        defined_signals.push_back(api);
    }
    class_api["signals"] = defined_signals;

    Array methods;
    for (int i = 0; i < p_class->functions.size(); ++i) {
        methods.append(dump_function_api(p_class->functions[i]));
    }
    class_api["methods"] = methods;

    Array static_functions;
    for (int i = 0; i < p_class->static_functions.size(); ++i) {
        static_functions.append(dump_function_api(p_class->functions[i]));
    }
    class_api["static_functions"] = static_functions;

    return class_api;
}

Dictionary ExtendGDScriptParser::generate_api() const {

    Dictionary api;
    const GDScriptParser::Node *head = get_parse_tree();
    if (const GDScriptParser::ClassNode *gdclass = dynamic_cast<const GDScriptParser::ClassNode *>(head)) {
        api = dump_class_api(gdclass);
    }
    return api;
}

Error ExtendGDScriptParser::parse(se_string_view p_code, se_string_view p_path) {
    path = p_path;
    se_string::split_ref(lines,p_code,'\n');

    Error err = GDScriptParser::parse(p_code, PathUtils::get_base_dir(p_path), false, p_path, false, nullptr, false);
    update_diagnostics();
    update_symbols();
    update_document_links(p_code);
    return err;
}
