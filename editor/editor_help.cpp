/*************************************************************************/
/*  editor_help.cpp                                                      */
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

#include "editor_help.h"

#include "core/callable_method_pointer.h"
#include "core/doc_support/doc_data.h"
#include "core/method_bind.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/resource/resource_manager.h"
#include "core/string.h"
#include "core/string_formatter.h"
#include "doc_data_compressed.gen.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "scene/gui/rich_text_label.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"

#include "EASTL/deque.h"
#include "EASTL/sort.h"

using namespace eastl;

IMPL_GDCLASS(FindBar)
IMPL_GDCLASS(EditorHelp)
IMPL_GDCLASS(EditorHelpBit)

#define CONTRIBUTE_URL "https://docs.godotengine.org/en/latest/community/contributing/updating_the_class_reference.html"

DocData *EditorHelp::doc = nullptr;
class EditorHelpPrivate {
public:
    Vector<Pair<String, int> > section_line;
    HashMap<String, int> method_line;
    HashMap<String, int> signal_line;
    HashMap<String, int> property_line;
    HashMap<String, int> theme_property_line;
    HashMap<String, int> constant_line;
    HashMap<String, int> enum_line;
    HashMap<String, HashMap<String, int> > enum_values_line;
};
class FindBarPrivate {
public:
    LineEdit *search_text;
    ToolButton *find_prev;
    ToolButton *find_next;
    Label *matches_label;
    TextureButton *hide_button;
    UIString prev_search;

    RichTextLabel *rich_text_label;
};

void EditorHelp::_init_colors() {

    title_color = get_theme_color("accent_color", "Editor");
    text_color = get_theme_color("default_color", "RichTextLabel");
    headline_color = get_theme_color("headline_color", "EditorHelp");
    base_type_color = title_color.linear_interpolate(text_color, 0.5);
    comment_color = text_color * Color(1, 1, 1, 0.6f);
    symbol_color = comment_color;
    value_color = text_color * Color(1, 1, 1, 0.6f);
    qualifier_color = text_color * Color(1, 1, 1, 0.8f);
    type_color = get_theme_color("accent_color", "Editor").linear_interpolate(text_color, 0.5);
    class_desc->add_theme_color_override("selection_color", get_theme_color("accent_color", "Editor") * Color(1, 1, 1, 0.4f));
    class_desc->add_constant_override("line_separation", Math::round(5 * EDSCALE));
}

void EditorHelp::_unhandled_key_input(const Ref<InputEvent> &p_ev) {

    if (!is_visible_in_tree())
        return;

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_ev);

    if (k && k->get_control() && k->get_keycode() == KEY_F) {

        search->grab_focus();
        search->select_all();
    }
}

void EditorHelp::_search(bool p_search_previous) {

    if (p_search_previous)
        find_bar->search_prev();
    else
        find_bar->search_next();
}

void EditorHelp::_class_list_select(StringView p_select) {

    _goto_desc(p_select);
}

void EditorHelp::_class_desc_select(StringView p_select) {

    if (StringUtils::begins_with(p_select,"$")) { //enum
        StringView select = StringUtils::substr(p_select,1, p_select.length());
        StringView class_name;
        if (StringUtils::find(select,".") != String::npos) {
            class_name = StringUtils::get_slice(select,".", 0);
            select = StringUtils::get_slice(select,".", 1);
        } else {
            class_name = "@GlobalScope";
        }
        emit_signal("go_to_help", String("class_enum:") + class_name + ":" + select);
        return;
    } else if (StringUtils::begins_with(p_select,"#")) {
        emit_signal("go_to_help", String("class_name:") + StringUtils::substr(p_select,1, p_select.length()));
        return;
    } else if (StringUtils::begins_with(p_select,"@")) {
        int tag_end = StringUtils::find(p_select," ");

        StringView tag = StringUtils::substr(p_select,1, tag_end - 1);
        StringView link = StringUtils::lstrip(StringUtils::substr(p_select,tag_end + 1, p_select.length())," ");

        String topic;
        HashMap<String, int> *table = nullptr;

        if (tag == "method"_sv) {
            topic = "class_method";
            table = &m_private->method_line;
        } else if (tag == "member"_sv) {
            topic = "class_property";
            table = &m_private->property_line;
        } else if (tag == "enum"_sv) {
            topic = "class_enum";
            table = &m_private->enum_line;
        } else if (tag == "signal"_sv) {
            topic = "class_signal";
            table = &m_private->signal_line;
        } else if (tag == "constant"_sv) {
            topic = "class_constant";
            table = &m_private->constant_line;
        } else {
            return;
        }
        StringName gscope("@GlobalScope");
        if (StringUtils::contains(link,".")) {
            emit_signal("go_to_help", topic + ":" + StringUtils::get_slice(link,".", 0) + ":" + StringUtils::get_slice(link,".", 1));
        } else {
            auto iter = table->find_as(link);
            if (iter!=table->end()) {
                // Found in the current page
                class_desc->scroll_to_line(iter->second);
            } else {
                if (topic == "class_enum") {
                    // Try to find the enum in @GlobalScope
                    const DocContents::ClassDoc &cd = doc->class_list[String(gscope)];

                    for (int i = 0; i < cd.constants.size(); i++) {
                        if (cd.constants[i].enumeration == link) {
                            // Found in @GlobalScope
                            emit_signal("go_to_help", String(topic + ":@GlobalScope:" + link));
                            break;
                        }
                    }
                } else if (topic == "class_constant") {
                    // Try to find the constant in @GlobalScope
                    const DocContents::ClassDoc &cd = doc->class_list[String(gscope)];

                    for (int i = 0; i < cd.constants.size(); i++) {
                        if (cd.constants[i].name == link) {
                            // Found in @GlobalScope
                            emit_signal("go_to_help", String(topic + ":@GlobalScope:" + link));
                            break;
                        }
                    }
                }
            }
        }
    } else if (StringUtils::begins_with(p_select,"http")) {
        OS::get_singleton()->shell_open(p_select);
    }
}

void EditorHelp::_class_desc_input(const Ref<InputEvent> &/*p_input*/) {
}

void EditorHelp::_class_desc_resized() {
    // Add extra horizontal margins for better readability.
    // The margins increase as the width of the editor help container increases.
    Ref<Font> doc_code_font = get_theme_font("doc_source", "EditorFonts");
    real_t char_width = doc_code_font->get_char_size('x').width;
    const int display_margin = M_MAX(30 * EDSCALE, get_parent_anchorable_rect().size.width - char_width * 120 * EDSCALE) * 0.5f;

    Ref<StyleBox> class_desc_stylebox = dynamic_ref_cast<StyleBox>(EditorNode::get_singleton()->get_theme_base()->get_theme_stylebox("normal", "RichTextLabel")->duplicate());
    class_desc_stylebox->set_default_margin(Margin::Left, display_margin);
    class_desc_stylebox->set_default_margin(Margin::Right, display_margin);
    class_desc->add_theme_style_override("normal", class_desc_stylebox);
}

void EditorHelp::_add_type(StringView p_type, StringView p_enum) {

    StringView t = p_type;
    if (t.empty())
        t = "void";
    bool can_ref = t != "void"_sv || !p_enum.empty();

    if (!p_enum.empty()) {
        if (StringUtils::get_slice_count(p_enum,'.') > 1) {
            t = StringUtils::get_slice(p_enum,'.', 1);
        } else {
            t = StringUtils::get_slice(p_enum,'.', 0);
        }
    }
    const Color text_color = get_theme_color("default_color", "RichTextLabel");
    const Color type_color = get_theme_color("accent_color", "Editor").linear_interpolate(text_color, 0.5);
    class_desc->push_color(type_color);
    if (can_ref) {
        if (p_enum.empty()) {
            class_desc->push_meta(String("#") + t); //class
        } else {
            class_desc->push_meta(String("$") + p_enum); //class
        }
    }
    class_desc->add_text(t);
    if (can_ref)
        class_desc->pop();
    class_desc->pop();
}

StringView _fix_constant(StringView p_constant) {

    if (StringUtils::strip_edges(p_constant) == "4294967295"_sv) {
        return "0xFFFFFFFF";
    }

    if (StringUtils::strip_edges(p_constant) == "2147483647"_sv) {
        return "0x7FFFFFFF";
    }

    if (StringUtils::strip_edges(p_constant) == "1048575"_sv) {
        return "0xFFFFF";
    }

    return p_constant;
}

void EditorHelp::_add_method(const DocContents::MethodDoc &p_method, bool p_overview) {

    m_private->method_line[String(p_method.name)] = class_desc->get_line_count() - 2; //gets overridden if description

    const bool is_vararg = p_method.qualifiers.contains("vararg");

    if (p_overview) {
        class_desc->push_cell();
        class_desc->push_align(RichTextLabel::ALIGN_RIGHT);
    }

    _add_type(p_method.return_type, p_method.return_enum);

    if (p_overview) {
        class_desc->pop(); //align
        class_desc->pop(); //cell
        class_desc->push_cell();
    } else {
        class_desc->add_text(" ");
    }

    if (p_overview && !p_method.description.empty()) {
        class_desc->push_meta(String("@method ") + p_method.name);
    }

    class_desc->push_color(headline_color);
    _add_text(p_method.name);
    class_desc->pop();

    if (p_overview && !p_method.description.empty()) {
        class_desc->pop(); //meta
    }

    class_desc->push_color(symbol_color);
    class_desc->add_text("(");
    class_desc->pop();

    for (int j = 0; j < p_method.arguments.size(); j++) {
        class_desc->push_color(text_color);
        if (j > 0)
            class_desc->add_text(", ");
        _add_text(p_method.arguments[j].name);
        class_desc->add_text(": ");
        _add_type(p_method.arguments[j].type, p_method.arguments[j].enumeration);
        if (not p_method.arguments[j].default_value.empty()) {

            class_desc->push_color(symbol_color);
            class_desc->add_text(" = ");
            class_desc->pop();
            class_desc->push_color(value_color);
            _add_text(_fix_constant(p_method.arguments[j].default_value));
            class_desc->pop();
        }

        class_desc->pop();
    }

    if (is_vararg) {
        class_desc->push_color(text_color);
        if (!p_method.arguments.empty())
            class_desc->add_text(", ");
        class_desc->push_color(symbol_color);
        class_desc->add_text("...");
        class_desc->pop();
        class_desc->pop();
    }

    class_desc->push_color(symbol_color);
    class_desc->add_text(")");
    class_desc->pop();
    if (!p_method.qualifiers.empty()) {

        class_desc->push_color(qualifier_color);
        class_desc->add_text(" ");
        _add_text(p_method.qualifiers);
        class_desc->pop();
    }

    if (p_overview)
        class_desc->pop(); //cell
}
Error EditorHelp::_goto_desc(StringView p_class, int p_vscr) {

    if (!doc->class_list.contains_as(p_class))
        return ERR_DOES_NOT_EXIST;

    select_locked = true;

    class_desc->show();

    description_line = 0;

    if (edited_class == p_class)
        return OK; //already there

    edited_class = StringName(p_class);
    _update_doc();
    return OK;
}

void EditorHelp::_update_doc() {
    using namespace StringUtils;

    if (!doc->class_list.contains_as(edited_class))
        return;

    scroll_locked = true;

    class_desc->clear();
    m_private->method_line.clear();
    m_private->section_line.clear();

    _init_colors();

    DocContents::ClassDoc cd = doc->class_list[String(edited_class)]; //make a copy, so we can sort without worrying

    Ref<Font> doc_font = get_theme_font("doc", "EditorFonts");
    Ref<Font> doc_bold_font = get_theme_font("doc_bold", "EditorFonts");
    Ref<Font> doc_title_font = get_theme_font("doc_title", "EditorFonts");
    Ref<Font> doc_code_font = get_theme_font("doc_source", "EditorFonts");
    String link_color_text = title_color.to_html(false);

    // Class name
    m_private->section_line.push_back(Pair<String, int>(TTR("Top").asCString(), 0));
    class_desc->push_font(doc_title_font);
    class_desc->push_color(title_color);
    class_desc->add_text_uistring((TTR("Class:") + " ").asString());
    class_desc->push_color(headline_color);
    _add_text(edited_class);
    class_desc->pop();
    class_desc->pop();
    class_desc->pop();
    class_desc->add_newline();

    // Inheritance tree

    // Ascendents
    if (!cd.inherits.empty()) {

        class_desc->push_color(title_color);
        class_desc->push_font(doc_font);
        class_desc->add_text_uistring((TTR("Inherits:") + " ").asString());
        class_desc->pop();

        String inherits(cd.inherits);

        while (!inherits.empty()) {
            _add_type(inherits);

            inherits = doc->class_list[inherits].inherits;

            if (!inherits.empty()) {
                class_desc->add_text(" < ");
            }
        }

        class_desc->pop();
        class_desc->add_newline();
    }

    // Descendents
    if (ClassDB::class_exists(StringName(cd.name))) {

        bool found = false;
        bool prev = false;

        for (const eastl::pair<const String,DocContents::ClassDoc> &E : doc->class_list) {

            if (E.second.inherits == cd.name) {

                if (!found) {
                    class_desc->push_color(title_color);
                    class_desc->push_font(doc_font);
                    class_desc->add_text_uistring((TTR("Inherited by:") + " ").asString());
                    class_desc->pop();
                    found = true;
                }

                if (prev) {

                    class_desc->add_text(" , ");
                }

                _add_type(E.second.name);
                prev = true;
            }
        }

        if (found) {
            class_desc->pop();
            class_desc->add_newline();
        }
    }

    class_desc->add_newline();
    class_desc->add_newline();

    // Brief description
    if (!cd.brief_description.empty()) {

        class_desc->push_color(text_color);
        class_desc->push_font(doc_bold_font);
        class_desc->push_indent(1);
        _add_text(cd.brief_description);
        class_desc->pop();
        class_desc->pop();
        class_desc->pop();
        class_desc->add_newline();
        class_desc->add_newline();
        class_desc->add_newline();
    }

    // Class description
    if (!cd.description.empty()) {

        m_private->section_line.push_back(Pair<String, int>(String(TTR("Description")), class_desc->get_line_count() - 2));
        description_line = class_desc->get_line_count() - 2;
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text(TTR("Description"));
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->add_newline();
        class_desc->push_color(text_color);
        class_desc->push_font(doc_font);
        class_desc->push_indent(1);
        _add_text(cd.description);
        class_desc->pop();
        class_desc->pop();
        class_desc->pop();
        class_desc->add_newline();
        class_desc->add_newline();
        class_desc->add_newline();
    }
    // Online tutorials
    if (cd.tutorials.size()) {
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text(TTR("Online Tutorials"));
        class_desc->pop();
        class_desc->pop();

        class_desc->push_indent(1);
        class_desc->push_font(doc_code_font);
        class_desc->add_newline();

        for (int i = 0; i < cd.tutorials.size(); i++) {
            const String &link = cd.tutorials[i];
            String linktxt = link;
            const auto seppos = linktxt.find("//");
            if (seppos != String::npos) {
                linktxt = link.right(seppos + 2);
            }

            class_desc->push_color(symbol_color);
            class_desc->append_bbcode(String("[url=") + link + "]" + linktxt + "[/url]");
            class_desc->pop();
            class_desc->add_newline();
        }

        class_desc->pop();
        class_desc->pop();
        class_desc->add_newline();
        class_desc->add_newline();
    }
    // Properties overview
    HashSet<String> skip_methods;
    bool property_descr = false;

    if (!cd.properties.empty()) {

        m_private->section_line.push_back(Pair<String, int>(TTR("Properties").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Properties").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->push_font(doc_code_font);
        class_desc->push_indent(1);
        class_desc->push_table(2);
        class_desc->set_table_column_expand(1, true);

        for (size_t i = 0; i < cd.properties.size(); i++) {
            m_private->property_line[cd.properties[i].name] = class_desc->get_line_count() - 2; //gets overridden if description

            class_desc->push_cell();
            class_desc->push_align(RichTextLabel::ALIGN_RIGHT);
            class_desc->push_font(doc_code_font);
            _add_type(cd.properties[i].type, cd.properties[i].enumeration);
            class_desc->pop();
            class_desc->pop();
            class_desc->pop();

            bool describe = false;

            if (!cd.properties[i].setter.empty()) {
                skip_methods.insert(cd.properties[i].setter);
                describe = true;
            }
            if (!cd.properties[i].getter.empty()) {
                skip_methods.insert(cd.properties[i].getter);
                describe = true;
            }

            if (!cd.properties[i].description.empty()) {
                describe = true;
            }

            if (cd.properties[i].overridden) {
                describe = false;
            }

            class_desc->push_cell();
            class_desc->push_font(doc_code_font);
            class_desc->push_color(headline_color);

            if (describe) {
                class_desc->push_meta(String("@member ") + cd.properties[i].name);
            }

            _add_text(cd.properties[i].name);

            if (describe) {
                class_desc->pop();
                property_descr = true;
            }

            if (!cd.properties[i].default_value.empty()) {
                class_desc->push_color(symbol_color);
                class_desc->add_text(cd.properties[i].overridden ? String(" [") + TTR("override:") + " " : String(" [") + TTR("default:") + " ");
                class_desc->pop();
                class_desc->push_color(value_color);
                _add_text(_fix_constant(cd.properties[i].default_value));
                class_desc->pop();
                class_desc->push_color(symbol_color);
                class_desc->add_text("]");
                class_desc->pop();
            }

            class_desc->pop();
            class_desc->pop();

            class_desc->pop();
        }

        class_desc->pop(); //table
        class_desc->pop();
        class_desc->pop(); // font
        class_desc->add_newline();
        class_desc->add_newline();
    }

    // Methods overview
    bool method_descr = false;
    bool sort_methods = EditorSettings::get_singleton()->getT<bool>("text_editor/help/sort_functions_alphabetically");

    Vector<DocContents::MethodDoc> methods;

    for (size_t i = 0; i < cd.methods.size(); i++) {
        if (skip_methods.contains_as(cd.methods[i].name)) {
            if (cd.methods[i].arguments.empty() /* getter */ ||
                    cd.methods[i].arguments.size() == 1 && cd.methods[i].return_type == "void" /* setter */) {
                continue;
            }
        }
        methods.push_back(cd.methods[i]);
    }
    if (!methods.empty()) {

        if (sort_methods)
            eastl::sort(methods.begin(), methods.end());

        m_private->section_line.push_back(Pair<String, int>(TTR("Methods").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Methods").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->push_font(doc_code_font);
        class_desc->push_indent(1);
        class_desc->push_table(2);
        class_desc->set_table_column_expand(1, true);

        bool any_previous = false;
        for (int pass = 0; pass < 2; pass++) {
            Vector<DocContents::MethodDoc> m;

            for (const DocContents::MethodDoc &dm : methods) {
                StringView q = dm.qualifiers;
                if (pass == 0 && q.contains("virtual") || pass == 1 && !q.contains("virtual")) {
                    m.push_back(dm);
                }
            }

            if (any_previous && !m.empty()) {
                class_desc->push_cell();
                class_desc->pop(); //cell
                class_desc->push_cell();
                class_desc->pop(); //cell
            }

            StringView group_prefix;
            for (size_t i = 0; i < m.size(); i++) {
                const StringView new_prefix = m[i].name.substr(0, 3);
                bool is_new_group = false;

                if (i < m.size() - 1 && new_prefix == StringUtils::substr(m[i + 1].name,0, 3) && new_prefix != group_prefix) {
                    is_new_group = i > 0;
                    group_prefix = new_prefix;
                } else if (!group_prefix.empty() && new_prefix != group_prefix) {
                    is_new_group = true;
                    group_prefix = {};
                }

                if (is_new_group && pass == 1) {
                    class_desc->push_cell();
                    class_desc->pop(); //cell
                    class_desc->push_cell();
                    class_desc->pop(); //cell
                }

                if (!m[i].description.empty()) {
                    method_descr = true;
                }

                _add_method(m[i], true);
            }

            any_previous = !m.empty();
        }

        class_desc->pop(); //table
        class_desc->pop();
        class_desc->pop(); // font
        class_desc->add_newline();
        class_desc->add_newline();
    }

    // Theme properties
    if (!cd.theme_properties.empty()) {

        m_private->section_line.push_back(Pair<String, int>(TTR("Theme Properties").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Theme Properties").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->push_indent(1);
        class_desc->push_table(2);
        class_desc->set_table_column_expand(1, true);

        for (size_t i = 0; i < cd.theme_properties.size(); i++) {

            m_private->theme_property_line[cd.theme_properties[i].name] = class_desc->get_line_count() - 2; //gets overridden if description

            class_desc->push_cell();
            class_desc->push_align(RichTextLabel::ALIGN_RIGHT);
            class_desc->push_font(doc_code_font);
            _add_type(cd.theme_properties[i].type);
            class_desc->pop();
            class_desc->pop();
            class_desc->pop();

            class_desc->push_cell();
            class_desc->push_font(doc_code_font);
            class_desc->push_color(headline_color);
            _add_text(cd.theme_properties[i].name);
            class_desc->pop();

            if (!cd.theme_properties[i].default_value.empty()) {
                class_desc->push_color(symbol_color);
                class_desc->add_text(String(" [") + TTR("default:") + " ");
                class_desc->pop();
                class_desc->push_color(value_color);
                _add_text(_fix_constant(cd.theme_properties[i].default_value));
                class_desc->pop();
                class_desc->push_color(symbol_color);
                class_desc->add_text("]");
                class_desc->pop();
            }

            class_desc->pop();

            if (!cd.theme_properties[i].description.empty()) {
                class_desc->push_font(doc_font);
                class_desc->add_text("  ");
                class_desc->push_color(comment_color);
                _add_text(cd.theme_properties[i].description);
                class_desc->pop();
                class_desc->pop();
            }
            class_desc->pop(); // cell
        }

        class_desc->pop(); // table
        class_desc->pop();
        class_desc->add_newline();
        class_desc->add_newline();
    }

    // Signals
    if (!cd.defined_signals.empty()) {

        if (sort_methods) {
            eastl::sort(cd.defined_signals.begin(),cd.defined_signals.end());
        }

        m_private->section_line.push_back(Pair<String, int>(TTR("Signals").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Signals").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->add_newline();

        class_desc->push_indent(1);

        for (size_t i = 0; i < cd.defined_signals.size(); i++) {

            m_private->signal_line[cd.defined_signals[i].name] = class_desc->get_line_count() - 2; //gets overridden if description
            class_desc->push_font(doc_code_font); // monofont
            class_desc->push_color(headline_color);
            _add_text(cd.defined_signals[i].name);
            class_desc->pop();
            class_desc->push_color(symbol_color);
            class_desc->add_text("(");
            class_desc->pop();
            for (size_t j = 0; j < cd.defined_signals[i].arguments.size(); j++) {
                class_desc->push_color(text_color);
                if (j > 0)
                    class_desc->add_text(", ");
                _add_text(cd.defined_signals[i].arguments[j].name);
                class_desc->add_text(": ");
                _add_type(cd.defined_signals[i].arguments[j].type);
                if (!cd.defined_signals[i].arguments[j].default_value.empty()) {

                    class_desc->push_color(symbol_color);
                    class_desc->add_text(" = ");
                    class_desc->pop();
                    _add_text(cd.defined_signals[i].arguments[j].default_value);
                }

                class_desc->pop();
            }

            class_desc->push_color(symbol_color);
            class_desc->add_text(")");
            class_desc->pop();
            class_desc->pop(); // end monofont
            if (!cd.defined_signals[i].description.empty()) {

                class_desc->push_font(doc_font);
                class_desc->push_color(comment_color);
                class_desc->push_indent(1);
                _add_text(cd.defined_signals[i].description);
                class_desc->pop(); // indent
                class_desc->pop();
                class_desc->pop(); // font
            }
            class_desc->add_newline();
            class_desc->add_newline();
        }

        class_desc->pop();
        class_desc->add_newline();
    }

    // Constants and enums
    if (!cd.constants.empty()) {

        Map<String, Vector<DocContents::ConstantDoc> > enums;
        Vector<DocContents::ConstantDoc> constants;

        for (size_t i = 0; i < cd.constants.size(); i++) {

            if (not cd.constants[i].enumeration.empty()) {
                enums[cd.constants[i].enumeration].push_back(cd.constants[i]);
            } else {

                constants.push_back(cd.constants[i]);
            }
        }

        // Enums
        if (!enums.empty()) {

            m_private->section_line.push_back(Pair<String, int>(TTR("Enumerations").asCString(), class_desc->get_line_count() - 2));
            class_desc->push_color(title_color);
            class_desc->push_font(doc_title_font);
            class_desc->add_text_uistring(TTR("Enumerations").asCString());
            class_desc->pop();
            class_desc->pop();
            class_desc->push_indent(1);

            class_desc->add_newline();

            for (auto E = enums.begin(); E!=enums.end(); ++E) {

                m_private->enum_line[E->first] = class_desc->get_line_count() - 2;

                class_desc->push_color(title_color);
                class_desc->add_text_uistring("enum  ");
                class_desc->pop();
                class_desc->push_font(doc_code_font);
                auto e = E->first;
                auto parts = E->first.split('.');
                if (parts.size()>1 && edited_class.asCString()==parts[0]) {
                    e = parts[1];
                }

                class_desc->push_color(headline_color);
                class_desc->add_text(e);
                class_desc->pop();
                class_desc->pop();
                class_desc->push_color(symbol_color);
                class_desc->add_text(":");
                class_desc->pop();
                class_desc->add_newline();

                class_desc->push_indent(1);
                Vector<DocContents::ConstantDoc> enum_list = E->second;

                HashMap<String, int> enumValuesContainer;
                int enumStartingLine = m_private->enum_line[E->first];

                for (size_t i = 0; i < enum_list.size(); i++) {
                    if (cd.name == "@GlobalScope")
                        enumValuesContainer[enum_list[i].name] = enumStartingLine;

                    // Add the enum constant line to the constant_line map so we can locate it as a constant
                    m_private->constant_line[enum_list[i].name] = class_desc->get_line_count() - 2;

                    class_desc->push_font(doc_code_font);
                    class_desc->push_color(headline_color);
                    _add_text(enum_list[i].name);
                    class_desc->pop();
                    class_desc->push_color(symbol_color);
                    class_desc->add_text(" = ");
                    class_desc->pop();
                    class_desc->push_color(value_color);
                    _add_text(_fix_constant(enum_list[i].value));
                    class_desc->pop();
                    class_desc->pop();
                    if (!enum_list[i].description.empty()) {
                        class_desc->push_font(doc_font);
                        //class_desc->add_text("  ");
                        class_desc->push_indent(1);
                        class_desc->push_color(comment_color);
                        _add_text(enum_list[i].description);
                        class_desc->pop();
                        class_desc->pop();
                        class_desc->pop(); // indent
                        class_desc->add_newline();
                    }

                    class_desc->add_newline();
                }

                if (cd.name == "@GlobalScope")
                    m_private->enum_values_line[E->first] = enumValuesContainer;

                class_desc->pop();

                class_desc->add_newline();
            }

            class_desc->pop();
            class_desc->add_newline();
        }

        // Constants
        if (!constants.empty()) {

            m_private->section_line.push_back(Pair<String, int>(TTR("Constants").asCString(), class_desc->get_line_count() - 2));
            class_desc->push_color(title_color);
            class_desc->push_font(doc_title_font);
            class_desc->add_text_uistring(TTR("Constants").asCString());
            class_desc->pop();
            class_desc->pop();
            class_desc->push_indent(1);

            class_desc->add_newline();

            for (int i = 0; i < constants.size(); i++) {

                m_private->constant_line[constants[i].name] = class_desc->get_line_count() - 2;
                class_desc->push_font(doc_code_font);
                auto cval=constants[i].value;
                if (StringUtils::begins_with(cval,"Color(") && StringUtils::ends_with(cval,")")) {

                    String stripped = cval.replaced(" ", "").replaced("Color(", "").replaced(")", "");
                    Vector<float> color = StringUtils::split_floats(stripped,",");
                    if (color.size() >= 3) {
                        class_desc->push_color(Color(color[0], color[1], color[2]));
                        static const CharType prefix[3] = { 0x25CF /* filled circle */, ' ', 0 };
                        class_desc->add_text_uistring(UIString(prefix));
                        class_desc->pop();
                    }
                }

                class_desc->push_color(headline_color);
                _add_text(constants[i].name);
                class_desc->pop();
                class_desc->push_color(symbol_color);
                class_desc->add_text(" = ");
                class_desc->pop();
                class_desc->push_color(value_color);
                _add_text(_fix_constant(cval));
                class_desc->pop();

                class_desc->pop();
                if (!constants[i].description.empty()) {
                    class_desc->push_font(doc_font);
                    class_desc->push_indent(1);
                    class_desc->push_color(comment_color);
                    _add_text(constants[i].description);
                    class_desc->pop();
                    class_desc->pop();
                    class_desc->pop(); // indent
                    class_desc->add_newline();
                }

                class_desc->add_newline();
            }

            class_desc->pop();
            class_desc->add_newline();
        }
    }

    // Property descriptions
    if (property_descr) {

        m_private->section_line.push_back(Pair<String, int>(TTR("Property Descriptions").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Property Descriptions").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->add_newline();

        for (int i = 0; i < cd.properties.size(); i++) {
            if (cd.properties[i].overridden)
                continue;

            m_private->property_line[cd.properties[i].name] = class_desc->get_line_count() - 2;

            class_desc->push_table(2);
            class_desc->set_table_column_expand(1, true);

            class_desc->push_cell();
            class_desc->push_font(doc_code_font);
            _add_type(cd.properties[i].type, cd.properties[i].enumeration);
            class_desc->add_text(" ");
            class_desc->pop(); // font
            class_desc->pop(); // cell

            class_desc->push_cell();
            class_desc->push_font(doc_code_font);
            class_desc->push_color(headline_color);
            _add_text(cd.properties[i].name);
            class_desc->pop(); // color

            if (!cd.properties[i].default_value.empty()) {
                class_desc->push_color(symbol_color);
                class_desc->add_text(String(" [") + TTR("default:") + " ");
                class_desc->pop(); // color

                class_desc->push_color(value_color);
                _add_text(_fix_constant(cd.properties[i].default_value));
                class_desc->pop(); // color

                class_desc->push_color(symbol_color);
                class_desc->add_text("]");
                class_desc->pop(); // color
            }

            class_desc->pop(); // font
            class_desc->pop(); // cell

            if (!cd.properties[i].setter.empty()) {

                class_desc->push_cell();
                class_desc->pop(); // cell

                class_desc->push_cell();
                class_desc->push_font(doc_code_font);
                class_desc->push_color(text_color);
                class_desc->add_text(cd.properties[i].setter + TTR("(value)"));
                class_desc->pop(); // color
                class_desc->push_color(comment_color);
                class_desc->add_text(" setter");
                class_desc->pop(); // color
                class_desc->pop(); // font
                class_desc->pop(); // cell
                m_private->method_line[cd.properties[i].setter] = m_private->property_line[cd.properties[i].name];
            }

            if (!cd.properties[i].getter.empty()) {

                class_desc->push_cell();
                class_desc->pop(); // cell

                class_desc->push_cell();
                class_desc->push_font(doc_code_font);
                class_desc->push_color(text_color);
                class_desc->add_text(cd.properties[i].getter + "()");
                class_desc->pop(); //color
                class_desc->push_color(comment_color);
                class_desc->add_text(" getter");
                class_desc->pop(); //color
                class_desc->pop(); //font
                class_desc->pop(); //cell
                m_private->method_line[cd.properties[i].getter] = m_private->property_line[cd.properties[i].name];
            }

            class_desc->pop(); // table

            class_desc->add_newline();
            class_desc->add_newline();

            class_desc->push_color(text_color);
            class_desc->push_font(doc_font);
            class_desc->push_indent(1);
            if (not StringUtils::strip_edges(cd.properties[i].description).empty()) {
                _add_text(cd.properties[i].description);
            }
            else {
                class_desc->add_image(get_theme_icon("Error", "EditorIcons"));
                class_desc->add_text(" ");
                class_desc->push_color(comment_color);
                auto translated = TTR("There is currently no description for this property. Please help us by "
                        "[color=$color][url=$url]contributing one[/url][/color]!");
                class_desc->append_bbcode(replace(replace(translated, "$url", CONTRIBUTE_URL), "$color", link_color_text));
                class_desc->pop();
            }
            class_desc->pop();
            class_desc->pop();
            class_desc->pop();
            class_desc->add_newline();
            class_desc->add_newline();
            class_desc->add_newline();
        }
    }

    // Method descriptions
    if (method_descr) {

        m_private->section_line.push_back(Pair<String, int>(TTR("Method Descriptions").asCString(), class_desc->get_line_count() - 2));
        class_desc->push_color(title_color);
        class_desc->push_font(doc_title_font);
        class_desc->add_text_uistring(TTR("Method Descriptions").asCString());
        class_desc->pop();
        class_desc->pop();

        class_desc->add_newline();
        class_desc->add_newline();

        for (int pass = 0; pass < 2; pass++) {
            Vector<DocContents::MethodDoc> methods_filtered;

            for (size_t i = 0; i < methods.size(); i++) {
                auto q = methods[i].qualifiers;
                if (pass == 0 && q.contains("virtual") || pass == 1 && not q.contains("virtual")) {
                    methods_filtered.push_back(methods[i]);
                }
            }

            for (size_t i = 0; i < methods_filtered.size(); i++) {
                class_desc->push_font(doc_code_font);
                _add_method(methods_filtered[i], false);
                class_desc->pop();

                class_desc->add_newline();
                class_desc->add_newline();

                class_desc->push_color(text_color);
                class_desc->push_font(doc_font);
                class_desc->push_indent(1);
                if (not StringUtils::strip_edges(methods_filtered[i].description).empty()) {
                    _add_text(methods_filtered[i].description);
                } else {
                    class_desc->add_image(get_theme_icon("Error", "EditorIcons"));
                    class_desc->add_text(" ");
                    class_desc->push_color(comment_color);
                    auto translated = TTR("There is currently no description for this method. Please help us by "
                                            "[color=$color][url=$url]contributing one[/url][/color]!");
                    class_desc->append_bbcode(
                            replace(replace(translated, "$url", CONTRIBUTE_URL), "$color", link_color_text));
                    class_desc->pop();
                }

                class_desc->pop();
                class_desc->pop();
                class_desc->pop();
                class_desc->add_newline();
                class_desc->add_newline();
                class_desc->add_newline();
            }
        }
    }
    scroll_locked = false;
}

void EditorHelp::_request_help(StringView p_string) {
    Error err = _goto_desc(p_string);
    if (err == OK) {
        EditorNode::get_singleton()->set_visible_editor(EditorNode::EDITOR_SCRIPT);
    }
    //100 palabras
}

void EditorHelp::_help_callback(StringView p_topic) {

    StringView what = StringUtils::get_slice(p_topic,":", 0);
    StringView clss = StringUtils::get_slice(p_topic,":", 1);
    String name;
    if (StringUtils::get_slice_count(p_topic,':') == 3)
        name = StringUtils::get_slice(p_topic,":", 2);

    _request_help(clss); //first go to class

    int line = 0;

    if (what == "class_desc"_sv) {
        line = description_line;
    } else if (what == "class_signal"_sv) {
        if (m_private->signal_line.contains(name))
            line = m_private->signal_line[name];
    } else if (what == "class_method"_sv || what == "class_method_desc"_sv) {
        if (m_private->method_line.contains(name))
            line = m_private->method_line[name];
    } else if (what == "class_property"_sv) {
        if (m_private->property_line.contains(name))
            line = m_private->property_line[name];
    } else if (what == "class_enum"_sv) {
        if (m_private->enum_line.contains(name))
            line = m_private->enum_line[name];
    } else if (what == "class_theme_item"_sv) {
        if (m_private->theme_property_line.contains(name))
            line = m_private->theme_property_line[name];
    } else if (what == "class_constant"_sv) {
        if (m_private->constant_line.contains(name))
            line = m_private->constant_line[name];
    } else if (what == "class_global"_sv) {
        if (m_private->constant_line.contains(name))
            line = m_private->constant_line[name];
        else {
            for(const auto &e : m_private->enum_values_line) {
                if (e.second.contains(name)) {
                    line = e.second.at(name);
                    break;
                }
            }
        }
    }

    class_desc->call_deferred("scroll_to_line", line);
}

static void _add_text_to_rt(StringView p_bbcode, RichTextLabel *p_rt) {
    using namespace StringUtils;

    DocData *doc = EditorHelp::get_doc_data();
    String base_path;

    Ref<Font> doc_font = p_rt->get_theme_font("doc", "EditorFonts");
    Ref<Font> doc_bold_font = p_rt->get_theme_font("doc_bold", "EditorFonts");
    Ref<Font> doc_code_font = p_rt->get_theme_font("doc_source", "EditorFonts");
    Color font_color_hl = p_rt->get_theme_color("headline_color", "EditorHelp");
    Color accent_color = p_rt->get_theme_color("accent_color", "Editor");
    Color link_color = accent_color.linear_interpolate(font_color_hl, 0.8f);
    Color code_color = accent_color.linear_interpolate(font_color_hl, 0.6f);

    String bbcode(strip_edges(replace(replace(dedent(p_bbcode),"\t", ""),"\r", "")));

    // remove extra new lines around code blocks
    bbcode = bbcode.replaced("[codeblock]\n", "[codeblock]");
    bbcode = bbcode.replaced("\n[/codeblock]", "[/codeblock]");

    Dequeue<StringView> tag_stack;
    bool code_tag = false;

    size_t pos = 0;
    while (pos < bbcode.length()) {

        size_t brk_pos = StringUtils::find(bbcode,"[", pos);

        if (brk_pos == String::npos)
            brk_pos = bbcode.length();

        if (brk_pos > pos) {
            String text(StringUtils::substr(bbcode,pos, brk_pos - pos));
            if (!code_tag)
                text = replace(text,"\n", "\n\n");
            p_rt->add_text(text);
        }

        if (brk_pos == bbcode.length())
            break; //nothing else to add

        size_t brk_end = StringUtils::find(bbcode,"]", brk_pos + 1);

        if (brk_end == String::npos) {

            String text(StringUtils::substr(bbcode,brk_pos, bbcode.length() - brk_pos));
            if (!code_tag)
                text = replace(text,"\n", "\n\n");
            p_rt->add_text(text);

            break;
        }

        StringView tag = StringUtils::substr(bbcode,brk_pos + 1, brk_end - brk_pos - 1);

        if (StringUtils::begins_with(tag,"/")) {
            bool tag_ok = !tag_stack.empty() && tag_stack.front() == StringUtils::substr(tag,1, tag.length());

            if (!tag_ok) {

                p_rt->add_text("[");
                pos = brk_pos + 1;
                continue;
            }

            tag_stack.pop_front();
            pos = brk_end + 1;
            if (tag != "/img"_sv) {
                p_rt->pop();
                if (code_tag) {
                    p_rt->pop();
                }
            }
            code_tag = false;
        } else if (code_tag) {

            p_rt->add_text("[");
            pos = brk_pos + 1;

        } else if (StringUtils::begins_with(tag,"method ") || StringUtils::begins_with(tag,"member ") || StringUtils::begins_with(tag,"signal ") || StringUtils::begins_with(tag,"enum ") || StringUtils::begins_with(tag,"constant ")) {

            auto tag_end = StringUtils::find(tag," ");

            StringView link_tag = StringUtils::substr(tag,0, tag_end);
            StringView link_target = StringUtils::lstrip(StringUtils::substr(tag,tag_end + 1, tag.length())," ");

            p_rt->push_color(link_color);
            p_rt->push_meta(String("@") + link_tag + " " + link_target);
            p_rt->add_text(String(link_target) + (StringUtils::begins_with(tag,"method ") ? "()" : ""));
            p_rt->pop();
            p_rt->pop();
            pos = brk_end + 1;

        } else if (doc->class_list.contains_as(tag)) {

            p_rt->push_color(link_color);
            p_rt->push_meta(String("#") + tag);
            p_rt->add_text(tag);
            p_rt->pop();
            p_rt->pop();
            pos = brk_end + 1;

        } else if (tag == "b"_sv) {

            //use bold font
            p_rt->push_font(doc_bold_font);
            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (tag == "i"_sv) {

            //use italics font
            p_rt->push_color(font_color_hl);
            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (tag == "code"_sv || tag == "codeblock"_sv) {

            //use monospace font
            p_rt->push_font(doc_code_font);
            p_rt->push_color(code_color);
            code_tag = true;
            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (tag == "center"_sv) {

            //align to center
            p_rt->push_align(RichTextLabel::ALIGN_CENTER);
            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (tag == "br"_sv) {

            //force a line break
            p_rt->add_newline();
            pos = brk_end + 1;
        } else if (tag == "u"_sv) {

            //use underline
            p_rt->push_underline();
            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (tag == "s"_sv) {

            //use strikethrough
            p_rt->push_strikethrough();
            pos = brk_end + 1;
            tag_stack.push_front(tag);

        } else if (tag == "url"_sv) {

            size_t end = StringUtils::find(bbcode,"[", brk_end);
            if (end == String::npos)
                end = bbcode.length();
            StringView url = StringUtils::substr(bbcode,brk_end + 1, end - brk_end - 1);
            p_rt->push_meta(url);

            pos = brk_end + 1;
            tag_stack.push_front(tag);
        } else if (StringUtils::begins_with(tag,"url=")) {

            StringView url = StringUtils::substr(tag,4, tag.length());
            p_rt->push_meta(url);
            pos = brk_end + 1;
            tag_stack.push_front("url");
        } else if (tag == "img"_sv) {

            auto end = StringUtils::find(bbcode,"[", brk_end);
            if (end == String::npos)
                end = bbcode.length();
            StringView image = StringUtils::substr(bbcode,brk_end + 1, end - brk_end - 1);

            Ref<Texture> texture = dynamic_ref_cast<Texture>(gResourceManager().load(PathUtils::plus_file(base_path,image), "Texture"));
            if (texture)
                p_rt->add_image(texture);

            pos = end;
            tag_stack.push_front(tag);
        } else if (StringUtils::begins_with(tag,"color=")) {

            StringView col = StringUtils::substr(tag,6, tag.length());
            Color color;

            if (StringUtils::begins_with(col,"#"))
                color = Color::html(col);
            else if (col == "aqua"_sv)
                color = Color(0, 1, 1);
            else if (col == "black"_sv)
                color = Color(0, 0, 0);
            else if (col == "blue"_sv)
                color = Color(0, 0, 1);
            else if (col == "fuchsia"_sv)
                color = Color(1, 0, 1);
            else if (col == "gray"_sv || col == "grey"_sv)
                color = Color(0.5, 0.5, 0.5);
            else if (col == "green"_sv)
                color = Color(0, 0.5, 0);
            else if (col == "lime"_sv)
                color = Color(0, 1, 0);
            else if (col == "maroon"_sv)
                color = Color(0.5, 0, 0);
            else if (col == "navy"_sv)
                color = Color(0, 0, 0.5);
            else if (col == "olive"_sv)
                color = Color(0.5, 0.5, 0);
            else if (col == "purple"_sv)
                color = Color(0.5, 0, 0.5);
            else if (col == "red"_sv)
                color = Color(1, 0, 0);
            else if (col == "silver"_sv)
                color = Color(0.75, 0.75, 0.75);
            else if (col == "teal"_sv)
                color = Color(0, 0.5, 0.5);
            else if (col == "white"_sv)
                color = Color(1, 1, 1);
            else if (col == "yellow"_sv)
                color = Color(1, 1, 0);
            else
                color = Color(0, 0, 0); //base_color;

            p_rt->push_color(color);
            pos = brk_end + 1;
            tag_stack.push_front("color");

        } else if (StringUtils::begins_with(tag,"font=")) {

            StringView fnt = StringUtils::substr(tag,5, tag.length());

            Ref<Font> font = dynamic_ref_cast<Font>(gResourceManager().load(PathUtils::plus_file(base_path,fnt), "Font"));
            if (font)
                p_rt->push_font(font);
            else {
                p_rt->push_font(doc_font);
            }

            pos = brk_end + 1;
            tag_stack.push_front("font");

        } else {

            p_rt->add_text("["); //ignore
            pos = brk_pos + 1;
        }
    }
}

void EditorHelp::_add_text(StringView p_bbcode) {

    _add_text_to_rt(p_bbcode, class_desc);
}
void EditorHelp::_add_text(const UIString &p_bbcode) {

    _add_text_to_rt(qPrintable(p_bbcode), class_desc);
}

void EditorHelp::generate_doc() {

    doc = memnew(DocData);
    generate_docs_from_running_program(*doc,true);
    DocData compdoc;
    compdoc.load_compressed(_doc_data_compressed, _doc_data_compressed_size, _doc_data_uncompressed_size);
    doc->merge_from(compdoc); //ensure all is up to date
}

void EditorHelp::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_READY:
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {

            _update_doc();
        } break;
        case NOTIFICATION_THEME_CHANGED: {
            if (is_visible_in_tree()) {
                _class_desc_resized();
            }
        } break;
        default: break;
    }
}

void EditorHelp::go_to_help(StringView p_help) {

    _help_callback(p_help);
}

void EditorHelp::go_to_class(StringView p_class, int p_scroll) {

    _goto_desc(p_class, p_scroll);
}

Vector<Pair<String, int> > EditorHelp::get_sections() {
    Vector<Pair<String, int> > sections;

    for (int i = 0; i < m_private->section_line.size(); i++) {
        sections.emplace_back(m_private->section_line[i].first, i);
    }
    return sections;
}

void EditorHelp::scroll_to_section(int p_section_index) {
    int line = m_private->section_line[p_section_index].second;
    class_desc->scroll_to_line(line);
}

void EditorHelp::popup_search() {

    find_bar->popup_search();
}

const char *EditorHelp::get_class() const {
    return edited_class.asCString();
}

void EditorHelp::set_focused() {
    class_desc->grab_focus();
}

void EditorHelp::search_again(bool p_search_previous) {
    _search(p_search_previous);
}

int EditorHelp::get_scroll() const {

    return class_desc->get_v_scroll()->get_value();
}
void EditorHelp::set_scroll(int p_scroll) {

    class_desc->get_v_scroll()->set_value(p_scroll);
}

void EditorHelp::_bind_methods() {
    MethodBinder::bind_method("_unhandled_key_input", &EditorHelp::_unhandled_key_input);

    ADD_SIGNAL(MethodInfo("go_to_help"));
}

EditorHelp::EditorHelp() {
    m_private = memnew(EditorHelpPrivate);
    set_custom_minimum_size(Size2(150 * EDSCALE, 0));

    EDITOR_DEF("text_editor/help/sort_functions_alphabetically", true);

    class_desc = memnew(RichTextLabel);
    add_child(class_desc);
    class_desc->set_v_size_flags(SIZE_EXPAND_FILL);
    class_desc->add_theme_color_override("selection_color", get_theme_color("accent_color", "Editor") * Color(1, 1, 1, 0.4f));
    class_desc->connect("meta_clicked",callable_mp(this, &ClassName::_class_desc_select));
    class_desc->connect("gui_input",callable_mp(this, &ClassName::_class_desc_input));
    class_desc->connect("resized",callable_mp(this, &ClassName::_class_desc_resized));
    _class_desc_resized();

    // Added second so it opens at the bottom so it won't offset the entire widget.
    find_bar = memnew(FindBar);
    add_child(find_bar);
    find_bar->hide();
    find_bar->set_rich_text_label(class_desc);

    class_desc->set_selection_enabled(true);

    scroll_locked = false;
    select_locked = false;
    class_desc->hide();
}

EditorHelp::~EditorHelp() {
    memdelete(m_private);
}

void EditorHelpBit::_go_to_help(const StringName& p_what) {

    EditorNode::get_singleton()->set_visible_editor(EditorNode::EDITOR_SCRIPT);
    ScriptEditor::get_singleton()->goto_help(p_what);
    emit_signal("request_hide");
}

void EditorHelpBit::_meta_clicked(StringView p_select) {

    if (StringUtils::begins_with(p_select,"$")) { //enum

        StringView select = StringUtils::substr(p_select,1, p_select.length());
        StringView class_name;
        if (StringUtils::contains(select,".")) {
            class_name = StringUtils::get_slice(select,".", 0);
        } else {
            class_name = "@Global";
        }
        _go_to_help(StringName(String("class_enum:") + class_name + ":" + select));
        return;
    } else if (StringUtils::begins_with(p_select,"#")) {

        _go_to_help(StringName(String("class_name:") + StringUtils::substr(p_select,1, p_select.length())));
        return;
    } else if (StringUtils::begins_with(p_select,"@")) {

        StringView m = StringUtils::substr(p_select,1, p_select.length());

        if (StringUtils::contains(m,'.'))
            _go_to_help(StringName(String("class_method:") + StringUtils::get_slice(m, ".", 0) + ":" +
                                   StringUtils::get_slice(m, ".", 0))); // must go somewhere else
    }
}

void EditorHelpBit::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_text", {"text"}), &EditorHelpBit::set_text);
    ADD_SIGNAL(MethodInfo("request_hide"));
}

void EditorHelpBit::_notification(int p_what) {

    switch (p_what) {
        case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {

            rich_text->add_theme_color_override("selection_color", get_theme_color("accent_color", "Editor") * Color(1, 1, 1, 0.4f));
        } break;
        default: break;
    }
}
void EditorHelpBit::set_text(StringView p_text) {

    rich_text->clear();
    _add_text_to_rt(p_text, rich_text);
}

void EditorHelpBit::set_text_ui(const UIString &p_text) {

    rich_text->clear();
    _add_text_to_rt(qPrintable(p_text), rich_text);
}

EditorHelpBit::EditorHelpBit() {

    rich_text = memnew(RichTextLabel);
    add_child(rich_text);
    rich_text->connect("meta_clicked",callable_mp(this, &ClassName::_meta_clicked));
    rich_text->add_theme_color_override("selection_color", get_theme_color("accent_color", "Editor") * Color(1, 1, 1, 0.4f));
    rich_text->set_override_selected_font_color(false);
    set_custom_minimum_size(Size2(0, 70 * EDSCALE));
}

FindBar::FindBar() {
    m_private = memnew(FindBarPrivate);
    m_private->search_text = memnew(LineEdit);
    add_child(m_private->search_text);
    m_private->search_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
    m_private->search_text->set_h_size_flags(SIZE_EXPAND_FILL);
    m_private->search_text->connect("text_changed",callable_mp(this, &ClassName::_search_text_changed));
    m_private->search_text->connect("text_entered",callable_mp(this, &ClassName::_search_text_entered));

    m_private->matches_label = memnew(Label);
    add_child(m_private->matches_label);
    m_private->matches_label->hide();

    m_private->find_prev = memnew(ToolButton);
    add_child(m_private->find_prev);
    m_private->find_prev->set_focus_mode(FOCUS_NONE);
    m_private->find_prev->connect("pressed",callable_mp(this, &ClassName::search_prev));

    m_private->find_next = memnew(ToolButton);
    add_child(m_private->find_next);
    m_private->find_next->set_focus_mode(FOCUS_NONE);
    m_private->find_next->connect("pressed",callable_mp(this, &ClassName::search_next));

    Control *space = memnew(Control);
    add_child(space);
    space->set_custom_minimum_size(Size2(4, 0) * EDSCALE);

    m_private->hide_button = memnew(TextureButton);
    add_child(m_private->hide_button);
    m_private->hide_button->set_focus_mode(FOCUS_NONE);
    m_private->hide_button->set_expand(true);
    m_private->hide_button->set_stretch_mode(TextureButton::STRETCH_KEEP_CENTERED);
    m_private->hide_button->connect("pressed",callable_mp(this, &ClassName::_hide_bar));
}

FindBar::~FindBar()
{
    memdelete(m_private);
}

void FindBar::popup_search() {

    show();
    bool grabbed_focus = false;
    if (!m_private->search_text->has_focus()) {
        m_private->search_text->grab_focus();
        grabbed_focus = true;
    }

    if (!m_private->search_text->get_text_ui().isEmpty()) {
        m_private->search_text->select_all();
        m_private->search_text->set_cursor_position(m_private->search_text->get_text_ui().length());
        if (grabbed_focus) {
            _search();
        }
    }
}

void FindBar::_notification(int p_what) {

    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
        case NOTIFICATION_THEME_CHANGED: {

            m_private->find_prev->set_button_icon(get_theme_icon("MoveUp", "EditorIcons"));
            m_private->find_next->set_button_icon(get_theme_icon("MoveDown", "EditorIcons"));
            m_private->hide_button->set_normal_texture(get_theme_icon("Close", "EditorIcons"));
            m_private->hide_button->set_hover_texture(get_theme_icon("Close", "EditorIcons"));
            m_private->hide_button->set_pressed_texture(get_theme_icon("Close", "EditorIcons"));
            m_private->hide_button->set_custom_minimum_size(m_private->hide_button->get_normal_texture()->get_size());
            m_private->matches_label->add_theme_color_override("font_color", results_count > 0 ? get_theme_color("font_color", "Label") : get_theme_color("error_color", "Editor"));
        } break;
        case NOTIFICATION_VISIBILITY_CHANGED: {

            set_process_unhandled_input(is_visible_in_tree());
        } break;
    }
}

void FindBar::_bind_methods() {

    MethodBinder::bind_method("_unhandled_input", &FindBar::_unhandled_input);

    ADD_SIGNAL(MethodInfo("search"));
}

void FindBar::set_rich_text_label(RichTextLabel *p_rich_text_label) {

    m_private->rich_text_label = p_rich_text_label;
}

bool FindBar::search_next() {

    return _search();
}

bool FindBar::search_prev() {

    return _search(true);
}

bool FindBar::_search(bool p_search_previous) {

    UIString stext = m_private->search_text->get_text_ui();
    bool keep = m_private->prev_search == stext;

    bool ret = m_private->rich_text_label->search(stext, keep, p_search_previous);
    if (!ret) {
        ret = m_private->rich_text_label->search(stext, false, p_search_previous);
    }

    m_private->prev_search = stext;

    if (ret) {
        _update_results_count();
    } else {
        results_count = 0;
    }
    _update_matches_label();

    return ret;
}

void FindBar::_update_results_count() {

    results_count = 0;

    UIString searched = m_private->search_text->get_text_ui();
    if (searched.isEmpty()) return;

    UIString full_text = StringUtils::from_utf8(m_private->rich_text_label->get_text());

    int from_pos = 0;

    while (true) {
        int pos = StringUtils::find(full_text,searched, from_pos);
        if (pos == -1)
            break;

        results_count++;
        from_pos = pos + searched.length();
    }
}

void FindBar::_update_matches_label() {

    if (m_private->search_text->get_text().empty() || results_count == -1) {
        m_private->matches_label->hide();
    } else {
        m_private->matches_label->show();

        m_private->matches_label->add_theme_color_override("font_color", results_count > 0 ? get_theme_color("font_color", "Label") : get_theme_color("error_color", "Editor"));
        m_private->matches_label->set_text(FormatSN((results_count == 1 ? TTR("%d match.") : TTR("%d matches.")).asCString(), results_count));
    }
}

void FindBar::_hide_bar() {

    if (m_private->search_text->has_focus())
        m_private->rich_text_label->grab_focus();

    hide();
}

void FindBar::_unhandled_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);
    if (k) {

        if (k->is_pressed() && (m_private->rich_text_label->has_focus() || is_a_parent_of(get_focus_owner()))) {

            bool accepted = true;

            switch (k->get_keycode()) {

                case KEY_ESCAPE: {

                    _hide_bar();
                } break;
                default: {

                    accepted = false;
                } break;
            }

            if (accepted) {
                accept_event();
            }
        }
    }
}

void FindBar::_search_text_changed(StringView /*p_text*/) {

    search_next();
}

void FindBar::_search_text_entered(StringView /*p_text*/) {

    if (Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
        search_prev();
    } else {
    search_next();
    }
}
