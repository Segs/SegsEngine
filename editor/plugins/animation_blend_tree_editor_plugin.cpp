/*************************************************************************/
/*  animation_blend_tree_editor_plugin.cpp                               */
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

#include "animation_blend_tree_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "core/translation_helpers.h"
#include "editor/editor_inspector.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/progress_bar.h"
#include "scene/main/viewport.h"
#include "scene/resources/style_box.h"

#include "EASTL/deque.h"

IMPL_GDCLASS(AnimationNodeBlendTreeEditor)

AnimationNodeBlendTreeEditor::AddOption::AddOption(String p_name, String p_type, int p_input_port_count) :
        name(eastl::move(p_name)),
        type(eastl::move(p_type)),
        input_port_count(p_input_port_count) {
}

void AnimationNodeBlendTreeEditor::add_custom_type(StringView p_name, const Ref<Script> &p_script) {

    for (int i = 0; i < add_options.size(); i++) {
        ERR_FAIL_COND(add_options[i].script == p_script);
    }

    AddOption ao;
    ao.name = p_name;
    ao.script = p_script;
    add_options.push_back(ao);

    _update_options_menu();
}

void AnimationNodeBlendTreeEditor::remove_custom_type(const Ref<Script> &p_script) {
    for (int i = 0; i < add_options.size(); i++) {
        if (add_options[i].script == p_script) {
            add_options.erase_at(i);
            return;
        }
    }

    _update_options_menu();
}

void AnimationNodeBlendTreeEditor::_update_options_menu(bool p_has_input_ports) {

    add_node->get_popup()->clear();
    add_node->get_popup()->set_size(Size2i(-1, -1));

    for (int i = 0; i < add_options.size(); i++) {
        if (p_has_input_ports && add_options[i].input_port_count == 0) {
            continue;
        }
        add_node->get_popup()->add_item(StringName(add_options[i].name), i);
    }

    Ref<AnimationNode> clipb = dynamic_ref_cast<AnimationNode>(EditorSettings::get_singleton()->get_resource_clipboard());
    if (clipb) {
        add_node->get_popup()->add_separator();
        add_node->get_popup()->add_item(TTR("Paste"), MENU_PASTE);
    }
    add_node->get_popup()->add_separator();
    add_node->get_popup()->add_item(TTR("Load..."), MENU_LOAD_FILE);
    use_popup_menu_position = false;
}

Size2 AnimationNodeBlendTreeEditor::get_minimum_size() const {

    return Size2(10, 200);
}

void AnimationNodeBlendTreeEditor::_property_changed(const StringName &p_property, const Variant &p_value, StringView p_field, bool p_changing) {

    AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_tree();
    updating = true;
    undo_redo->create_action(TTR("Parameter Changed:") + " " + p_property, UndoRedo::MERGE_ENDS);
    undo_redo->add_do_property(tree, p_property, p_value);
    undo_redo->add_undo_property(tree, p_property, tree->get(p_property));
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
    updating = false;
}

void AnimationNodeBlendTreeEditor::_update_graph() {

    if (updating || !blend_tree)
        return;

    visible_properties.clear();

    graph->set_scroll_ofs(blend_tree->get_graph_offset() * EDSCALE);

    graph->clear_connections();
    //erase all nodes
    for (int i = 0; i < graph->get_child_count(); i++) {

        if (object_cast<GraphNode>(graph->get_child(i))) {
            memdelete(graph->get_child(i));
            i--;
        }
    }

    animations.clear();

    List<StringName> nodes;
    blend_tree->get_node_list(&nodes);

    for (const StringName &E : nodes) {

        GraphNode *node = memnew(GraphNode);
        graph->add_child(node);

        Ref<AnimationNode> agnode = blend_tree->get_node(E);

        node->set_offset(blend_tree->get_node_position(E) * EDSCALE);

        node->set_title(agnode->get_caption());
        node->set_name(E);

        int base = 0;
        if (E != StringView("output")) {
            LineEdit *name = memnew(LineEdit);
            name->set_text(E);
            name->set_expand_to_text_length(true);
            node->add_child(name);
            node->set_slot(0, false, 0, Color(), true, 0, get_theme_color("font_color", "Label"));
            auto lambda=[this,agnode_cp(agnode)](StringView s) { _node_renamed(s,agnode_cp); };
            auto focus_lambda = [this,name,agnode_cp(agnode)]() {_node_renamed_focus_out(name,agnode_cp); };
            name->connect("text_entered",callable_gen(this,lambda),ObjectNS::CONNECT_QUEUED);
            name->connect("focus_exited",callable_gen(this, focus_lambda), ObjectNS::CONNECT_QUEUED);
            base = 1;
            node->set_show_close_button(true);
            node->connectF("close_request",this,[this,E] { _delete_request(E); },ObjectNS::CONNECT_QUEUED);
        }

        for (int i = 0; i < agnode->get_input_count(); i++) {
            Label *in_name = memnew(Label);
            node->add_child(in_name);
            in_name->set_text(StringName(agnode->get_input_name(i)));
            node->set_slot(base + i, true, 0, get_theme_color("font_color", "Label"), false, 0, Color());
        }

        Vector<PropertyInfo> pinfo;
        agnode->get_parameter_list(&pinfo);
        for (const PropertyInfo &F : pinfo) {

            if (!(F.usage & PROPERTY_USAGE_EDITOR)) {
                continue;
            }
            String base_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E) + "/" + F.name;
            EditorProperty *prop = EditorInspector::instantiate_property_editor(AnimationTreeEditor::get_singleton()->get_tree(), F.type, base_path, F.hint, F.hint_string, F.usage);
            if (prop) {
                prop->set_object_and_property(AnimationTreeEditor::get_singleton()->get_tree(), StringName(base_path));
                prop->update_property();
                prop->set_name_split_ratio(0);
                prop->connect("property_changed",callable_mp(this, &ClassName::_property_changed));
                node->add_child(prop);
                visible_properties.push_back(prop);
            }
        }
        auto lambda_drag = [this,E](const Vector2 &p_from, const Vector2 &p_to) { _node_dragged(p_from,p_to,E);};
        node->connect("dragged",callable_gen(this, lambda_drag));

        if (AnimationTreeEditor::get_singleton()->can_edit(agnode)) {
            node->add_child(memnew(HSeparator));
            Button *open_in_editor = memnew(Button);
            open_in_editor->set_text(TTR("Open Editor"));
            open_in_editor->set_button_icon(get_theme_icon("Edit", "EditorIcons"));
            node->add_child(open_in_editor);
            open_in_editor->connectF("pressed",this,[this,E]() { _open_in_editor(E); },ObjectNS::CONNECT_QUEUED);
            open_in_editor->set_h_size_flags(SIZE_SHRINK_CENTER);
        }

        if (agnode->has_filter()) {

            node->add_child(memnew(HSeparator));
            Button *edit_filters = memnew(Button);
            edit_filters->set_text(TTR("Edit Filters"));
            edit_filters->set_button_icon(get_theme_icon("AnimationFilter", "EditorIcons"));
            node->add_child(edit_filters);
            edit_filters->connectF("pressed",this,[this,E]() { _edit_filters(E); },ObjectNS::CONNECT_QUEUED);
            edit_filters->set_h_size_flags(SIZE_SHRINK_CENTER);
        }

        Ref<AnimationNodeAnimation> anim = dynamic_ref_cast<AnimationNodeAnimation>(agnode);
        if (anim) {

            MenuButton *mb = memnew(MenuButton);
            mb->set_text(anim->get_animation());
            mb->set_button_icon(get_theme_icon("Animation", "EditorIcons"));
            Array options;

            node->add_child(memnew(HSeparator));
            node->add_child(mb);

            ProgressBar *pb = memnew(ProgressBar);

            AnimationTree *player = AnimationTreeEditor::get_singleton()->get_tree();
            if (player->has_node(player->get_animation_player())) {
                AnimationPlayer *ap = object_cast<AnimationPlayer>(player->get_node(player->get_animation_player()));
                if (ap) {
                    Vector<StringName> anims(ap->get_animation_list());

                    for (const StringName &F : anims) {
                        mb->get_popup()->add_item(F);
                        options.push_back(F);
                    }

                    if (ap->has_animation(anim->get_animation())) {
                        pb->set_max(ap->get_animation(anim->get_animation())->get_length());
                    }
                }
            }

            pb->set_percent_visible(false);
            pb->set_custom_minimum_size(Vector2(0, 14) * EDSCALE);
            animations[E] = pb;
            node->add_child(pb);
            auto lambda = [this,options,E](int idx) {
              _anim_selected(idx,options,E);
            };
            mb->get_popup()->connect("index_pressed", callable_gen(this, lambda), ObjectNS::CONNECT_QUEUED);
        }

        if (EditorSettings::get_singleton()->getT<bool>("interface/theme/use_graph_node_headers")) {
            Ref<StyleBoxFlat> sb = dynamic_ref_cast<StyleBoxFlat>(node->get_theme_stylebox("frame", "GraphNode"));
            Color c = sb->get_border_color();
            Color mono_color = (c.r + c.g + c.b) / 3 < 0.7 ? Color(1.0, 1.0, 1.0) : Color(0.0, 0.0, 0.0);
            mono_color.a = 0.85f;
            c = mono_color;

            node->add_theme_color_override("title_color", c);
            c.a = 0.7f;
            node->add_theme_color_override("close_color", c);
            node->add_theme_color_override("resizer_color", c);
        }
    }

    Vector<AnimationNodeBlendTree::NodeConnection> connections(blend_tree->get_node_connections());

    for (const AnimationNodeBlendTree::NodeConnection& E : connections) {

        graph->connect_node(E.output_node, 0, E.input_node, E.input_index);
    }
    float graph_minimap_opacity = EditorSettings::get_singleton()->getT<float>("editors/visual_editors/minimap_opacity");
    graph->set_minimap_opacity(graph_minimap_opacity);

}

void AnimationNodeBlendTreeEditor::_file_opened(StringView p_file) {

    file_loaded = dynamic_ref_cast<AnimationNode>(gResourceManager().load(p_file));
    if (file_loaded) {
        _add_node(MENU_LOAD_FILE_CONFIRM);
    }
}

void AnimationNodeBlendTreeEditor::_add_node(int p_idx) {

    Ref<AnimationNode> anode;

    String base_name;

    if (p_idx == MENU_LOAD_FILE) {

        open_file->clear_filters();
        Vector<String> filters;
        gResourceManager().get_recognized_extensions_for_type("AnimationNode", filters);
        for (const String &E : filters) {
            open_file->add_filter("*." + E);
        }
        open_file->popup_centered_ratio();
        return;
    } else if (p_idx == MENU_LOAD_FILE_CONFIRM) {
        anode = file_loaded;
        file_loaded.unref();
        base_name = anode->get_class();
    } else if (p_idx == MENU_PASTE) {

        anode = dynamic_ref_cast<AnimationNode>(EditorSettings::get_singleton()->get_resource_clipboard());
        ERR_FAIL_COND(not anode);
        base_name = anode->get_class();
    } else if (!add_options[p_idx].type.empty()) {
        AnimationNode *an = object_cast<AnimationNode>(ClassDB::instance(StringName(add_options[p_idx].type)));
        ERR_FAIL_COND(!an);
        anode = Ref<AnimationNode>(an,DoNotAddRef);
        base_name = add_options[p_idx].name;
    } else {
        ERR_FAIL_COND(not add_options[p_idx].script);
        StringName base_type(add_options[p_idx].script->get_instance_base_type());
        AnimationNode *an = object_cast<AnimationNode>(ClassDB::instance(base_type));
        ERR_FAIL_COND(!an);
        anode = Ref<AnimationNode>(an,DoNotAddRef);
        anode->set_script(add_options[p_idx].script.get_ref_ptr());
        base_name = add_options[p_idx].name;
    }

    Ref<AnimationNodeOutput> out = dynamic_ref_cast<AnimationNodeOutput>(anode);
    if (out) {
        EditorNode::get_singleton()->show_warning(TTR("Output node can't be added to the blend tree."));
        return;
    }

    if (!from_node.empty() && anode->get_input_count() == 0) {
        from_node = "";
        return;
    }

    Point2 instance_pos = graph->get_scroll_ofs();
    if (use_popup_menu_position) {
        instance_pos += popup_menu_position;
    } else {
        instance_pos += graph->get_size() * 0.5;
    }

    instance_pos /= graph->get_zoom();

    int base = 1;
    String name(base_name);
    while (blend_tree->has_node(StringName(name))) {
        base++;
        name = base_name + " " + itos(base);
    }

    undo_redo->create_action(TTR("Add Node to BlendTree"));
    undo_redo->add_do_method(blend_tree.get(), "add_node", name, anode, instance_pos / EDSCALE);
    undo_redo->add_undo_method(blend_tree.get(), "remove_node", name);
    if (!from_node.empty()) {
        undo_redo->add_do_method(blend_tree.get(), "connect_node", name, 0, from_node);
        from_node = "";
    }
    if (!to_node.empty() && to_slot != -1) {
        undo_redo->add_do_method(blend_tree.get(), "connect_node", to_node, to_slot, name);
        to_node = "";
        to_slot = -1;
    }
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

}

void AnimationNodeBlendTreeEditor::_popup(bool p_has_input_ports, const Vector2 &p_popup_position, const Vector2 &p_node_position) {
    _update_options_menu(p_has_input_ports);
    use_popup_menu_position = true;
    popup_menu_position = p_popup_position;
    add_node->get_popup()->set_position(p_node_position);
    add_node->get_popup()->popup();
}

void AnimationNodeBlendTreeEditor::_popup_request(const Vector2 &p_position) {
    _popup(false, graph->get_local_mouse_position(), p_position);
}

void AnimationNodeBlendTreeEditor::_connection_to_empty(const StringName &p_from, int p_from_slot, const Vector2 &p_release_position) {
    Ref<AnimationNode> node = blend_tree->get_node(p_from);
    if (node) {
        from_node = p_from;
        _popup(true, p_release_position, graph->get_global_mouse_position());
    }
}

void AnimationNodeBlendTreeEditor::_connection_from_empty(const StringName &p_to, int p_to_slot, const Vector2 &p_release_position) {
    Ref<AnimationNode> node = blend_tree->get_node(p_to);
    if (node) {
        to_node = p_to;
        to_slot = p_to_slot;
        _popup(false, p_release_position, graph->get_global_mouse_position());
    }
}
void AnimationNodeBlendTreeEditor::_node_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_which) {

    updating = true;
    undo_redo->create_action(TTR("Node Moved"));
    undo_redo->add_do_method(blend_tree.get(), "set_node_position", p_which, p_to / EDSCALE);
    undo_redo->add_undo_method(blend_tree.get(), "set_node_position", p_which, p_from / EDSCALE);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
    updating = false;
}

void AnimationNodeBlendTreeEditor::_connection_request(StringView p_from, int p_from_index, StringView p_to, int p_to_index) {

    AnimationNodeBlendTree::ConnectionError err = blend_tree->can_connect_node(StringName(p_to), p_to_index, StringName(p_from));

    if (err != AnimationNodeBlendTree::CONNECTION_OK) {
        EditorNode::get_singleton()->show_warning(TTR("Unable to connect, port may be in use or connection may be invalid."));
        return;
    }

    undo_redo->create_action(TTR("Nodes Connected"));
    undo_redo->add_do_method(blend_tree.get(), "connect_node", p_to, p_to_index, p_from);
    undo_redo->add_undo_method(blend_tree.get(), "disconnect_node", p_to, p_to_index);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_disconnection_request(StringView p_from, int p_from_index, StringView p_to, int p_to_index) {

    graph->disconnect_node(StringName(p_from), p_from_index, StringName(p_to), p_to_index);

    updating = true;
    undo_redo->create_action(TTR("Nodes Disconnected"));
    undo_redo->add_do_method(blend_tree.get(), "disconnect_node", p_to, p_to_index);
    undo_redo->add_undo_method(blend_tree.get(), "connect_node", p_to, p_to_index, p_from);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
    updating = false;
}

void AnimationNodeBlendTreeEditor::_anim_selected(int p_index, Array p_options, const StringName &p_node) {

    String option = p_options[p_index].as<String>();

    Ref<AnimationNodeAnimation> anim = dynamic_ref_cast<AnimationNodeAnimation>(blend_tree->get_node(p_node));
    ERR_FAIL_COND(not anim);

    undo_redo->create_action(TTR("Set Animation"));
    undo_redo->add_do_method(anim.get(), "set_animation", option);
    undo_redo->add_undo_method(anim.get(), "set_animation", anim->get_animation());
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_request(const StringName & p_which) {

    undo_redo->create_action(TTR("Delete Node"));
    undo_redo->add_do_method(blend_tree.get(), "remove_node", p_which);
    undo_redo->add_undo_method(blend_tree.get(), "add_node", p_which, blend_tree->get_node(p_which), blend_tree->get_node_position(p_which));

    Vector<AnimationNodeBlendTree::NodeConnection> conns(blend_tree->get_node_connections());

    for (AnimationNodeBlendTree::NodeConnection &E : conns) {
        if (E.output_node == p_which || E.input_node == p_which) {
            undo_redo->add_undo_method(blend_tree.get(), "connect_node", E.input_node, E.input_index, E.output_node);
        }
    }

    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_nodes_request() {

    Vector<StringName> to_erase;
    to_erase.reserve(graph->get_child_count());
    for (int i = 0; i < graph->get_child_count(); i++) {
        GraphNode *gn = object_cast<GraphNode>(graph->get_child(i));
        if (gn) {
            if (gn->is_selected() && gn->is_close_button_visible()) {
                to_erase.push_back(gn->get_name());
            }
        }
    }

    if (to_erase.empty())
        return;

    undo_redo->create_action(TTR("Delete Node(s)"));

    for (const StringName &F : to_erase) {
        _delete_request(F);
    }

    undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_node_selected(Object *p_node) {

    GraphNode *gn = object_cast<GraphNode>(p_node);
    ERR_FAIL_COND(!gn);

    const StringName &name = gn->get_name();

    Ref<AnimationNode> anode = blend_tree->get_node(name);
    ERR_FAIL_COND(not anode);

    EditorNode::get_singleton()->push_item(anode.get(), {}, true);
}

void AnimationNodeBlendTreeEditor::_open_in_editor(const StringName &p_which) {

    Ref<AnimationNode> an = blend_tree->get_node(p_which);
    ERR_FAIL_COND(not an);
    AnimationTreeEditor::get_singleton()->enter_editor(p_which);
}

void AnimationNodeBlendTreeEditor::_filter_toggled() {

    updating = true;
    undo_redo->create_action(TTR("Toggle Filter On/Off"));
    undo_redo->add_do_method(_filter_edit.get(), "set_filter_enabled", filter_enabled->is_pressed());
    undo_redo->add_undo_method(_filter_edit.get(), "set_filter_enabled", _filter_edit->is_filter_enabled());
    undo_redo->add_do_method(this, "_update_filters", _filter_edit);
    undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
    undo_redo->commit_action();
    updating = false;
}

void AnimationNodeBlendTreeEditor::_filter_edited() {

    TreeItem *edited = filters->get_edited();
    ERR_FAIL_COND(!edited);

    NodePath edited_path = edited->get_metadata(0).as<NodePath>();
    bool filtered = edited->is_checked(0);

    updating = true;
    undo_redo->create_action(TTR("Change Filter"));
    undo_redo->add_do_method(_filter_edit.get(), "set_filter_path", edited_path, filtered);
    undo_redo->add_undo_method(_filter_edit.get(), "set_filter_path", edited_path, _filter_edit->is_path_filtered(edited_path));
    undo_redo->add_do_method(this, "_update_filters", _filter_edit);
    undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
    undo_redo->commit_action();
    updating = false;
}

bool AnimationNodeBlendTreeEditor::_update_filters(const Ref<AnimationNode> &anode) {

    if (updating || _filter_edit != anode)
        return false;

    NodePath player_path = AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();

    if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(player_path)) {
        EditorNode::get_singleton()->show_warning(TTR("No animation player set, so unable to retrieve track names."));
        return false;
    }

    AnimationPlayer *player = object_cast<AnimationPlayer>(AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
    if (!player) {
        EditorNode::get_singleton()->show_warning(TTR("Player path set is invalid, so unable to retrieve track names."));
        return false;
    }

    Node *base = player->get_node(player->get_root());

    if (!base) {
        EditorNode::get_singleton()->show_warning(TTR("Animation player has no valid root node path, so unable to retrieve track names."));
        return false;
    }

    updating = true;

    Set<String> paths;
    HashMap<String, Set<String> > types;
    {
        Vector<StringName> player_animations(player->get_animation_list());

        for (const StringName &E : player_animations) {

            Ref<Animation> anim = player->get_animation(E);
            for (int i = 0; i < anim->get_track_count(); i++) {
                String track_path = (String)anim->track_get_path(i);
                paths.insert(track_path);

                StringName track_type_name;
                Animation::TrackType track_type = anim->track_get_type(i);
                switch (track_type) {
                    case Animation::TrackType::TYPE_ANIMATION: {
                        track_type_name = TTR("Anim Clips");
                    } break;
                    case Animation::TrackType::TYPE_AUDIO: {
                        track_type_name = TTR("Audio Clips");
                    } break;
                    case Animation::TrackType::TYPE_METHOD: {
                        track_type_name = TTR("Functions");
                    } break;
                    default: {
                    } break;
                }
                if (!track_type_name.empty()) {
                    types[track_path].insert(track_type_name);
                }
            }
        }
    }

    filter_enabled->set_pressed(anode->is_filter_enabled());
    filters->clear();
    TreeItem *root = filters->create_item();

    Map<String, TreeItem *> parenthood;

    for (const String &E : paths) {

        NodePath path(E);
        TreeItem *ti = nullptr;
        String accum;
        for (int i = 0; i < path.get_name_count(); i++) {
            StringName name(path.get_name(i));
            if (!accum.empty()) {
                accum += "/";
            }
            accum += name;
            if (!parenthood.contains(accum)) {
                if (ti) {
                    ti = filters->create_item(ti);
                } else {
                    ti = filters->create_item(root);
                }
                parenthood[accum] = ti;
                ti->set_text(0, name);
                ti->set_selectable(0, false);
                ti->set_editable(0, false);

                if (base->has_node(NodePath(accum))) {
                    Node *node = base->get_node(NodePath(accum));
                    ti->set_icon(0, EditorNode::get_singleton()->get_object_icon(node, "Node"));
                }

            } else {
                ti = parenthood[accum];
            }
        }

        Node *node = nullptr;
        if (base->has_node(NodePath(accum))) {
            node = base->get_node(NodePath(accum));
        }
        if (!node)
            continue; //no node, can't edit

        if (path.get_subname_count()) {

            StringName concat(path.get_concatenated_subnames());

            Skeleton *skeleton = object_cast<Skeleton>(node);
            if (skeleton && skeleton->find_bone(concat) != -1) {
                //path in skeleton
                const StringName &bone = concat;
                int idx = skeleton->find_bone(bone);
                Dequeue<String> bone_path;
                while (idx != -1) {
                    bone_path.push_front(skeleton->get_bone_name(idx));
                    idx = skeleton->get_bone_parent(idx);
                }

                accum += ":";

                for (const String &F : bone_path) {
                    if ((void *)F.data() != (void *)bone_path.front().data()) {
                        accum += "/";
                    }

                    accum += F;
                    if (!parenthood.contains(accum)) {
                        ti = filters->create_item(ti);
                        parenthood[accum] = ti;
                        ti->set_text_utf8(0, F);
                        ti->set_selectable(0, false);
                        ti->set_editable(0, false);
                        ti->set_icon(0, get_theme_icon("BoneAttachment3D", "EditorIcons"));
                    } else {
                        ti = parenthood[accum];
                    }
                }

                ti->set_editable(0, true);
                ti->set_selectable(0, true);
                ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
                ti->set_text_utf8(0, concat);
                ti->set_checked(0, anode->is_path_filtered(path));
                ti->set_icon(0, get_theme_icon("BoneAttachment3D", "EditorIcons"));
                ti->set_metadata(0, path);

            } else {
                //just a property
                ti = filters->create_item(ti);
                ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
                ti->set_text_utf8(0, concat);
                ti->set_editable(0, true);
                ti->set_selectable(0, true);
                ti->set_checked(0, anode->is_path_filtered(path));
                ti->set_metadata(0, path);
            }
        } else {
            if (ti) {
                //just a node, not a property track
                String types_text = "[";
                if (types.contains((String)path)) {
                    auto & c(types[(String)path]);
                    auto F = types[(String)path].begin();
                    types_text += *F;
                    ++F;
                    for (;F != c.end(); ++F) {
                        types_text += " / " + *F;
                    }
                }
                types_text += "]";
                ti = filters->create_item(ti);
                ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
                ti->set_text_utf8(0, types_text);
                ti->set_editable(0, true);
                ti->set_selectable(0, true);
                ti->set_checked(0, anode->is_path_filtered(path));
                ti->set_metadata(0, path);
            }
        }
    }

    updating = false;

    return true;
}

void AnimationNodeBlendTreeEditor::_edit_filters(const StringName &p_which) {

    Ref<AnimationNode> anode = blend_tree->get_node(p_which);
    ERR_FAIL_COND(not anode);

    _filter_edit = anode;
    if (!_update_filters(anode))
        return;

    filter_dialog->popup_centered_minsize(Size2(500, 500) * EDSCALE);
}

void AnimationNodeBlendTreeEditor::_removed_from_graph() {
    if (is_visible()) {
        EditorNode::get_singleton()->edit_item(nullptr);
    }
}

void AnimationNodeBlendTreeEditor::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {

        error_panel->add_theme_style_override("panel", get_theme_stylebox("bg", "Tree"));
        error_label->add_theme_color_override("font_color", get_theme_color("error_color", "Editor"));

        if (p_what == NOTIFICATION_THEME_CHANGED && is_visible_in_tree())
            _update_graph();
    }

    if (p_what == NOTIFICATION_PROCESS) {

        String error;

        if (!AnimationTreeEditor::get_singleton()->get_tree()->is_active()) {
            error = TTR("AnimationTree is inactive.\nActivate to enable playback, check node warnings if activation fails.");
        } else if (AnimationTreeEditor::get_singleton()->get_tree()->is_state_invalid()) {
            error = AnimationTreeEditor::get_singleton()->get_tree()->get_invalid_state_reason();
        }

        if (error != error_label->get_text()) {
            error_label->set_text(StringName(error));
            if (!error.empty()) {
                error_panel->show();
            } else {
                error_panel->hide();
            }
        }

        Vector<AnimationNodeBlendTree::NodeConnection> conns(blend_tree->get_node_connections());

        for (AnimationNodeBlendTree::NodeConnection& E : conns) {
            float activity = 0;
            StringName path(AnimationTreeEditor::get_singleton()->get_base_path() + E.input_node);
            if (AnimationTreeEditor::get_singleton()->get_tree() && !AnimationTreeEditor::get_singleton()->get_tree()->is_state_invalid()) {
                activity = AnimationTreeEditor::get_singleton()->get_tree()->get_connection_activity(path, E.input_index);
            }
            graph->set_connection_activity(E.output_node, 0, E.input_node, E.input_index, activity);
        }

        AnimationTree *graph_player = AnimationTreeEditor::get_singleton()->get_tree();
        AnimationPlayer *player = nullptr;
        if (graph_player->has_node(graph_player->get_animation_player())) {
            player = object_cast<AnimationPlayer>(graph_player->get_node(graph_player->get_animation_player()));
        }

        if (player) {
            for (eastl::pair<const StringName,ProgressBar *> &E : animations) {
                Ref<AnimationNodeAnimation> an = dynamic_ref_cast<AnimationNodeAnimation>(blend_tree->get_node(E.first));
                if (an) {
                    if (player->has_animation(an->get_animation())) {
                        Ref<Animation> anim = player->get_animation(an->get_animation());
                        if (anim) {
                            E.second->set_max(anim->get_length());
                            //StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + E->get().input_node;
                            StringName time_path(AnimationTreeEditor::get_singleton()->get_base_path() + E.first + "/time");
                            E.second->set_value(AnimationTreeEditor::get_singleton()->get_tree()->getT<float>(time_path));
                        }
                    }
                }
            }
        }

        for (int i = 0; i < visible_properties.size(); i++) {
            visible_properties[i]->update_property();
        }
    }

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        set_process(is_visible_in_tree());
    }
}

void AnimationNodeBlendTreeEditor::_scroll_changed(const Vector2 &p_scroll) {
    if (updating)
        return;
    updating = true;
    blend_tree->set_graph_offset(p_scroll / EDSCALE);
    updating = false;
}

void AnimationNodeBlendTreeEditor::_bind_methods() {

    MethodBinder::bind_method("_update_graph", &AnimationNodeBlendTreeEditor::_update_graph);
    MethodBinder::bind_method("_update_filters", &AnimationNodeBlendTreeEditor::_update_filters);
    MethodBinder::bind_method("_update_options_menu", &AnimationNodeBlendTreeEditor::_update_options_menu);

    MethodBinder::bind_method("_anim_selected", &AnimationNodeBlendTreeEditor::_anim_selected);
}

AnimationNodeBlendTreeEditor *AnimationNodeBlendTreeEditor::singleton = nullptr;

void AnimationNodeBlendTreeEditor::_node_renamed(StringView p_text, const Ref<AnimationNode>& p_node) {
    if (!blend_tree) {
        WARN_PRINT_ONCE("AnimationNodeBlendTreeEditor::_node_renamed with null blend_tree");
        return;
    }
    StringName prev_name = blend_tree->get_node_name(p_node);
    ERR_FAIL_COND(prev_name.empty());
    GraphNode *gn = object_cast<GraphNode>(graph->get_node(NodePath(prev_name)));
    ERR_FAIL_COND(!gn);

    ERR_FAIL_COND(p_text.empty() || StringUtils::contains(p_text,".") || StringUtils::contains(p_text,"/"));

    if (prev_name == p_text) {
        return; //nothing to do
    }

    int base = 1;
    String name(p_text);
    while (blend_tree->has_node(StringName(name))) {
        base++;
        name = String(p_text) + " " + itos(base);
    }

    String base_path(AnimationTreeEditor::get_singleton()->get_base_path());

    updating = true;
    undo_redo->create_action(TTR("Node Renamed"));
    undo_redo->add_do_method(blend_tree.get(), "rename_node", prev_name, name);
    undo_redo->add_undo_method(blend_tree.get(), "rename_node", name, prev_name);
    undo_redo->add_do_method(AnimationTreeEditor::get_singleton()->get_tree(), "rename_parameter", String(base_path + prev_name), String(base_path + name));
    undo_redo->add_undo_method(AnimationTreeEditor::get_singleton()->get_tree(), "rename_parameter", String(base_path + name), String(base_path + prev_name));
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
    updating = false;
    gn->set_name(p_text);
    gn->set_size(gn->get_minimum_size());

    //change editors accordingly
    for (auto &property : visible_properties) {
        StringName pname(property->get_edited_property());
        if (StringUtils::begins_with(pname,base_path + prev_name)) {
            StringName new_name2(StringUtils::replace_first(pname,base_path + prev_name, base_path + name));
            property->set_object_and_property(property->get_edited_object(), new_name2);
        }
    }

    //recreate connections
    graph->clear_connections();

    Vector<AnimationNodeBlendTree::NodeConnection> connections(blend_tree->get_node_connections());

    for (const AnimationNodeBlendTree::NodeConnection &E : connections) {

        graph->connect_node(E.output_node, 0, E.input_node, E.input_index);
    }

    //update animations
    for (eastl::pair<const StringName,ProgressBar *> &E : animations) {
        if (E.first == prev_name) {
            animations[StringName(p_text)] = animations[prev_name];
            animations.erase(prev_name);
            break;
        }
    }

    _update_graph(); // Needed to update the signal connections with the new name.
}

void AnimationNodeBlendTreeEditor::_node_renamed_focus_out(Node *le, const Ref<AnimationNode>& p_node) {
    if (le == nullptr) {
        return; // The text_submitted signal triggered the graph update and freed the LineEdit.
    }

    _node_renamed(le->call_va("get_text").as<String>(), p_node);
}

bool AnimationNodeBlendTreeEditor::can_edit(const Ref<AnimationNode> &p_node) {
    Ref<AnimationNodeBlendTree> bt = dynamic_ref_cast<AnimationNodeBlendTree>(p_node);
    return bt;
}

void AnimationNodeBlendTreeEditor::edit(const Ref<AnimationNode> &p_node) {

    if (blend_tree) {
        blend_tree->disconnect("removed_from_graph",callable_mp(this, &ClassName::_removed_from_graph));
    }

    blend_tree = dynamic_ref_cast<AnimationNodeBlendTree>(p_node);

    if (not blend_tree) {
        hide();
    } else {
        blend_tree->connect("removed_from_graph",callable_mp(this, &ClassName::_removed_from_graph));

        _update_graph();
    }
}

AnimationNodeBlendTreeEditor::AnimationNodeBlendTreeEditor() {

    singleton = this;
    updating = false;
    use_popup_menu_position = false;

    graph = memnew(GraphEdit);
    add_child(graph);
    graph->add_valid_right_disconnect_type(0);
    graph->add_valid_left_disconnect_type(0);
    graph->set_v_size_flags(SIZE_EXPAND_FILL);
    graph->connect("connection_request",callable_mp(this, &ClassName::_connection_request), ObjectNS::CONNECT_QUEUED);
    graph->connect("disconnection_request",callable_mp(this, &ClassName::_disconnection_request), ObjectNS::CONNECT_QUEUED);
    graph->connect("node_selected",callable_mp(this, &ClassName::_node_selected));
    graph->connect("scroll_offset_changed",callable_mp(this, &ClassName::_scroll_changed));
    graph->connect("delete_nodes_request",callable_mp(this, &ClassName::_delete_nodes_request));
    graph->connect("popup_request",callable_mp(this, &ClassName::_popup_request));

    graph->connect("connection_to_empty",callable_mp(this, &ClassName::_connection_to_empty));
    graph->connect("connection_from_empty",callable_mp(this, &ClassName::_connection_from_empty));

    float graph_minimap_opacity = EditorSettings::get_singleton()->getT<float>("editors/visual_editors/minimap_opacity");
    graph->set_minimap_opacity(graph_minimap_opacity);

    VSeparator *vs = memnew(VSeparator);
    graph->get_zoom_hbox()->add_child(vs);
    graph->get_zoom_hbox()->move_child(vs, 0);

    add_node = memnew(MenuButton);
    graph->get_zoom_hbox()->add_child(add_node);
    add_node->set_text(TTR("Add Node..."));
    graph->get_zoom_hbox()->move_child(add_node, 0);
    add_node->get_popup()->connect("id_pressed",callable_mp(this, &ClassName::_add_node));
    auto update_opts_lambda = [this]() {
        _update_options_menu(false);
    };
    add_node->connect("about_to_show",callable_gen(this, update_opts_lambda));

    add_options.emplace_back("Animation", "AnimationNodeAnimation");
    add_options.emplace_back("OneShot", "AnimationNodeOneShot", 2);
    add_options.emplace_back("Add2", "AnimationNodeAdd2", 2);
    add_options.emplace_back("Add3", "AnimationNodeAdd3", 3);
    add_options.emplace_back("Blend2", "AnimationNodeBlend2", 2);
    add_options.emplace_back("Blend3", "AnimationNodeBlend3", 3);
    add_options.emplace_back("Seek", "AnimationNodeTimeSeek", 1);
    add_options.emplace_back("TimeScale", "AnimationNodeTimeScale", 1);
    add_options.emplace_back("Transition", "AnimationNodeTransition");
    add_options.emplace_back("BlendTree", "AnimationNodeBlendTree");
    add_options.emplace_back("BlendSpace1D", "AnimationNodeBlendSpace1D");
    add_options.emplace_back("BlendSpace2D", "AnimationNodeBlendSpace2D");
    add_options.emplace_back("StateMachine", "AnimationNodeStateMachine");
    _update_options_menu();

    error_panel = memnew(PanelContainer);
    add_child(error_panel);
    error_label = memnew(Label);
    error_panel->add_child(error_label);
    error_label->set_text("eh");

    filter_dialog = memnew(AcceptDialog);
    add_child(filter_dialog);
    filter_dialog->set_title(TTR("Edit Filtered Tracks:"));

    VBoxContainer *filter_vbox = memnew(VBoxContainer);
    filter_dialog->add_child(filter_vbox);

    filter_enabled = memnew(CheckBox);
    filter_enabled->set_text(TTR("Enable Filtering"));
    filter_enabled->connect("pressed",callable_mp(this, &ClassName::_filter_toggled));
    filter_vbox->add_child(filter_enabled);

    filters = memnew(Tree);
    filter_vbox->add_child(filters);
    filters->set_v_size_flags(SIZE_EXPAND_FILL);
    filters->set_hide_root(true);
    filters->connect("item_edited",callable_mp(this, &ClassName::_filter_edited));

    open_file = memnew(EditorFileDialog);
    add_child(open_file);
    open_file->set_title(TTR("Open Animation Node"));
    open_file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    open_file->connect("file_selected",callable_mp(this, &ClassName::_file_opened));
    undo_redo = EditorNode::get_undo_redo();
}

