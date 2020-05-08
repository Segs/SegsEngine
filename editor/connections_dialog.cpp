/*************************************************************************/
/*  connections_dialog.cpp                                               */
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

#include "connections_dialog.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/translation_helpers.h"
#include "core/string_formatter.h"
#include "editor_node.h"
#include "editor/editor_help.h"
#include "editor/editor_scale.h"
#include "editor/scene_tree_dock.h"
#include "editor_settings.h"
#include "plugins/script_editor_plugin.h"
#include "scene/gui/label.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/rich_text_label.h"
#include "scene/resources/style_box.h"
#include "scene/main/scene_tree.h"

#include "EASTL/sort.h"

IMPL_GDCLASS(ConnectDialog)
IMPL_GDCLASS(ConnectionsDock)

static Node *_find_first_script(Node *p_root, Node *p_node) {
    if (p_node != p_root && p_node->get_owner() != p_root) {
        return nullptr;
    }
    if (!p_node->get_script().is_null()) {
        return p_node;
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {

        Node *ret = _find_first_script(p_root, p_node->get_child(i));
        if (ret) {
            return ret;
        }
    }

    return nullptr;
}

class ConnectDialogBinds : public Object {

    GDCLASS(ConnectDialogBinds,Object)

public:
    Vector<Variant> params;

    bool _set(const StringName &p_name, const Variant &p_value) {

        if (StringUtils::begins_with(p_name,"bind/")) {
            int which = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1)) - 1;
            ERR_FAIL_INDEX_V(which, params.size(), false);
            params[which] = p_value;
        } else
            return false;

        return true;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {

        if (StringUtils::begins_with(p_name,"bind/")) {
            int which = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1)) - 1;
            ERR_FAIL_INDEX_V(which, params.size(), false);
            r_ret = params[which];
        } else
            return false;

        return true;
    }

    void _get_property_list(Vector<PropertyInfo> *p_list) const {

        for (size_t i = 0; i < params.size(); i++) {
            p_list->push_back(PropertyInfo(params[i].get_type(), StringName("bind/" + itos(i + 1))));
        }
    }

    void notify_changed() {

        Object_change_notify(this);
    }

    ConnectDialogBinds() {
    }
};
IMPL_GDCLASS(ConnectDialogBinds)

void register_connection_dialog_classes()
{
    ConnectDialogBinds::initialize_class();
}
/*
 * Signal automatically called by parent dialog.
*/
void ConnectDialog::ok_pressed() {

    if (dst_method->get_text_ui().isEmpty()) {
        error->set_text(TTR("Method in target node must be specified."));
        error->popup_centered_minsize();
        return;
    }
    Node *target = tree->get_selected();

    if(!target)
        return; // Nothing selected in the tree, not an error.

    if (target->get_script().is_null()) {
        if (!target->has_method(StringName(dst_method->get_text()))) {
            error->set_text(TTR("Target method not found. Specify a valid method or attach a script to the target node."));
            error->popup_centered_minsize();
            return;
        }
    }
    emit_signal("connected");
    hide();
}

void ConnectDialog::_cancel_pressed() {

    hide();
}

/*
 * Called each time a target node is selected within the target node tree.
*/
void ConnectDialog::_tree_node_selected() {

    Node *current = tree->get_selected();

    if (!current)
        return;

    dst_path = source->get_path_to(current);
    _update_ok_enabled();
}

/*
 * Adds a new parameter bind to connection.
*/
void ConnectDialog::_add_bind() {

    if (cdbinds->params.size() >= VARIANT_ARG_MAX)
        return;
    VariantType vt = (VariantType)type_list->get_item_id(type_list->get_selected());

    Variant value;

    switch (vt) {
        case VariantType::BOOL: value = false; break;
        case VariantType::INT: value = 0; break;
        case VariantType::FLOAT: value = 0.0; break;
        case VariantType::STRING: value = ""; break;
        case VariantType::VECTOR2: value = Vector2(); break;
        case VariantType::RECT2: value = Rect2(); break;
        case VariantType::VECTOR3: value = Vector3(); break;
        case VariantType::PLANE: value = Plane(); break;
        case VariantType::QUAT: value = Quat(); break;
        case VariantType::AABB: value = AABB(); break;
        case VariantType::BASIS: value = Basis(); break;
        case VariantType::TRANSFORM: value = Transform(); break;
        case VariantType::COLOR: value = Color(); break;
        default: {
            ERR_FAIL();
        } break;
    }

    ERR_FAIL_COND(value.get_type() == VariantType::NIL);

    cdbinds->params.push_back(value);
    cdbinds->notify_changed();
}

/*
Remove parameter bind from connection.
*/
void ConnectDialog::_remove_bind() {

    const StringName &st(bind_editor->get_selected_path());
    if (st.empty())
        return;
    int idx = StringUtils::to_int(StringUtils::get_slice(st,"/", 1)) - 1;

    ERR_FAIL_INDEX(idx, cdbinds->params.size());
    cdbinds->params.erase_at(idx);
    cdbinds->notify_changed();
}

/*
 * Enables or disables the connect button. The connect button is enabled if a
 * node is selected and valid in the selected mode.
 */
void ConnectDialog::_update_ok_enabled() {

    Node *target = tree->get_selected();

    if (target == nullptr) {
        get_ok()->set_disabled(true);
        return;
    }

    if (!advanced->is_pressed() && target->get_script().is_null()) {
        get_ok()->set_disabled(true);
        return;
    }

    get_ok()->set_disabled(false);
}
void ConnectDialog::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {
        bind_editor->edit(cdbinds);
    }
}

void ConnectDialog::_bind_methods() {

    MethodBinder::bind_method("_advanced_pressed", &ConnectDialog::_advanced_pressed);
    MethodBinder::bind_method("_cancel", &ConnectDialog::_cancel_pressed);
    MethodBinder::bind_method("_tree_node_selected", &ConnectDialog::_tree_node_selected);

    MethodBinder::bind_method("_add_bind", &ConnectDialog::_add_bind);
    MethodBinder::bind_method("_remove_bind", &ConnectDialog::_remove_bind);
    MethodBinder::bind_method("_update_ok_enabled", &ConnectDialog::_update_ok_enabled);

    ADD_SIGNAL(MethodInfo("connected"));
}

Node *ConnectDialog::get_source() const {

    return source;
}

StringName ConnectDialog::get_signal_name() const {

    return signal;
}

NodePath ConnectDialog::get_dst_path() const {

    return dst_path;
}

void ConnectDialog::set_dst_node(Node *p_node) {

    tree->set_selected(p_node);
}

StringName ConnectDialog::get_dst_method_name() const {

    String txt = dst_method->get_text();
    if (StringUtils::contains(txt,'('))
        txt = StringUtils::strip_edges(StringUtils::left(txt,StringUtils::find(txt,"(")));
    return StringName(txt);
}

void ConnectDialog::set_dst_method(const StringName &p_method) {

    dst_method->set_text(p_method);
}

const Vector<Variant> &ConnectDialog::get_binds() const {

    return cdbinds->params;
}

bool ConnectDialog::get_deferred() const {

    return deferred->is_pressed();
}

bool ConnectDialog::get_oneshot() const {

    return oneshot->is_pressed();
}

/*
 * Returns true if ConnectDialog is being used to edit an existing connection.
*/
bool ConnectDialog::is_editing() const {

    return bEditMode;
}

/*
 * Initialize ConnectDialog and populate fields with expected data.
 * If creating a connection from scratch, sensible defaults are used.
 * If editing an existing connection, previous data is retained.
*/
void ConnectDialog::init(const Connection& c, bool bEdit) {

    set_hide_on_ok(false);

    source = static_cast<Node *>(c.source);
    signal = c.signal;

    tree->set_selected(nullptr);
    tree->set_marked(source, true);

    if (c.target) {
        set_dst_node(static_cast<Node *>(c.target));
        set_dst_method(c.method);
    }

    _update_ok_enabled();

    bool bDeferred = (c.flags & ObjectNS::CONNECT_QUEUED) == ObjectNS::CONNECT_QUEUED;
    bool bOneshot = (c.flags & ObjectNS::CONNECT_ONESHOT) == ObjectNS::CONNECT_ONESHOT;

    deferred->set_pressed(bDeferred);
    oneshot->set_pressed(bOneshot);

    cdbinds->params.clear();
    cdbinds->params = c.binds;
    cdbinds->notify_changed();

    bEditMode = bEdit;
}

void ConnectDialog::popup_dialog(const UIString &p_for_signal) {

    from_signal->set_text_uistring(p_for_signal);
    error_label->add_color_override("font_color", get_color("error_color", "Editor"));
    if (!advanced->is_pressed())
        error_label->set_visible(!_find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root()));

    popup_centered();
}

void ConnectDialog::_advanced_pressed() {

    if (advanced->is_pressed()) {
        set_custom_minimum_size(Size2(900, 500) * EDSCALE);
        connect_to_label->set_text(TTR("Connect to Node:"));
        tree->set_connect_to_script_mode(false);

        vbc_right->show();
        error_label->hide();
    } else {
        set_custom_minimum_size(Size2(600, 500) * EDSCALE);
        set_size(Size2());
        connect_to_label->set_text(TTR("Connect to Script:"));
        tree->set_connect_to_script_mode(true);

        vbc_right->hide();
        error_label->set_visible(!_find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root()));
    }

    _update_ok_enabled();

    set_position((get_viewport_rect().size - get_custom_minimum_size()) / 2);
}

ConnectDialog::ConnectDialog() {

    set_custom_minimum_size(Size2(600, 500) * EDSCALE);

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);

    HBoxContainer *main_hb = memnew(HBoxContainer);
    vbc->add_child(main_hb);
    main_hb->set_v_size_flags(SIZE_EXPAND_FILL);

    VBoxContainer *vbc_left = memnew(VBoxContainer);
    main_hb->add_child(vbc_left);
    vbc_left->set_h_size_flags(SIZE_EXPAND_FILL);

    from_signal = memnew(LineEdit);
    from_signal->set_editable(false);
    vbc_left->add_margin_child(TTR("From Signal:"), from_signal);

    tree = memnew(SceneTreeEditor(false));
    tree->set_connecting_signal(true);
    tree->get_scene_tree()->connect("item_activated", this, "_ok");
    tree->connect("node_selected", this, "_tree_node_selected");
    tree->set_connect_to_script_mode(true);

    Node *mc = vbc_left->add_margin_child(TTR("Connect to Script:"), tree, true);
    connect_to_label = object_cast<Label>(vbc_left->get_child(mc->get_index() - 1));

    error_label = memnew(Label);
    error_label->set_text(TTR("Scene does not contain any script."));
    vbc_left->add_child(error_label);
    error_label->hide();

    vbc_right = memnew(VBoxContainer);
    main_hb->add_child(vbc_right);
    vbc_right->set_h_size_flags(SIZE_EXPAND_FILL);
    vbc_right->hide();

    HBoxContainer *add_bind_hb = memnew(HBoxContainer);

    type_list = memnew(OptionButton);
    type_list->set_h_size_flags(SIZE_EXPAND_FILL);
    add_bind_hb->add_child(type_list);
    type_list->add_item("bool", (int)VariantType::BOOL);
    type_list->add_item("int", (int)VariantType::INT);
    type_list->add_item("float", (int)VariantType::FLOAT);
    type_list->add_item("String", (int)VariantType::STRING);
    type_list->add_item("Vector2", (int)VariantType::VECTOR2);
    type_list->add_item("Rect2", (int)VariantType::RECT2);
    type_list->add_item("Vector3", (int)VariantType::VECTOR3);
    type_list->add_item("Plane", (int)VariantType::PLANE);
    type_list->add_item("Quat", (int)VariantType::QUAT);
    type_list->add_item("AABB", (int)VariantType::AABB);
    type_list->add_item("Basis", (int)VariantType::BASIS);
    type_list->add_item("Transform", (int)VariantType::TRANSFORM);
    type_list->add_item("Color", (int)VariantType::COLOR);
    type_list->select(0);

    Button *add_bind = memnew(Button);
    add_bind->set_text(TTR("Add"));
    add_bind_hb->add_child(add_bind);
    add_bind->connect("pressed", this, "_add_bind");

    Button *del_bind = memnew(Button);
    del_bind->set_text(TTR("Remove"));
    add_bind_hb->add_child(del_bind);
    del_bind->connect("pressed", this, "_remove_bind");

    vbc_right->add_margin_child(TTR("Add Extra Call Argument:"), add_bind_hb);

    bind_editor = memnew(EditorInspector);

    vbc_right->add_margin_child(TTR("Extra Call Arguments:"), bind_editor, true);

    HBoxContainer *dstm_hb = memnew(HBoxContainer);
    vbc_left->add_margin_child("Receiver Method:", dstm_hb);

    dst_method = memnew(LineEdit);
    dst_method->set_h_size_flags(SIZE_EXPAND_FILL);
    dst_method->connect("text_entered", this, "_builtin_text_entered");
    dstm_hb->add_child(dst_method);

    advanced = memnew(CheckButton);
    dstm_hb->add_child(advanced);
    advanced->set_text(TTR("Advanced"));
    advanced->connect("pressed", this, "_advanced_pressed");

    // Add spacing so the tree and inspector are the same size.
    Control *spacing = memnew(Control);
    spacing->set_custom_minimum_size(Size2(0, 4) * EDSCALE);
    vbc_right->add_child(spacing);

    deferred = memnew(CheckBox);
    deferred->set_h_size_flags(0);
    deferred->set_text(TTR("Deferred"));
    deferred->set_tooltip(TTR("Defers the signal, storing it in a queue and only firing it at idle time."));
    vbc_right->add_child(deferred);

    oneshot = memnew(CheckBox);
    oneshot->set_h_size_flags(0);
    oneshot->set_text(TTR("Oneshot"));
    oneshot->set_tooltip(TTR("Disconnects the signal after its first emission."));
    vbc_right->add_child(oneshot);

    set_as_toplevel(true);

    cdbinds = memnew(ConnectDialogBinds);

    error = memnew(AcceptDialog);
    add_child(error);
    error->set_title(TTR("Cannot connect signal"));
    error->get_ok()->set_text(TTR("Close"));
    get_ok()->set_text(TTR("Connect"));
}

ConnectDialog::~ConnectDialog() {

    memdelete(cdbinds);
}

//////////////////////////////////////////

// Originally copied and adapted from EditorProperty, try to keep style in sync.
Control *ConnectionsDockTree::make_custom_tooltip(StringView p_text) const {

    EditorHelpBit *help_bit = memnew(EditorHelpBit);
    help_bit->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));
    help_bit->get_rich_text()->set_fixed_size_to_width(360 * EDSCALE);

    FixedVector<StringView,16,true> parts;

    String::split_ref(parts,p_text,"::");

    String text(String(TTR("Signal:")) + " [u][b]" + parts[0] + "[/b][/u]");
    text += String(StringUtils::strip_edges(parts[1])) + "\n";
    text += StringUtils::strip_edges(parts[2]);

    help_bit->call_deferred("set_text", text); //hack so it uses proper theme once inside scene
    return help_bit;
}

struct _ConnectionsDockMethodInfoSort {

    _FORCE_INLINE_ bool operator()(const MethodInfo &a, const MethodInfo &b) const {
        return a.name < b.name;
    }
};

/*
 * Post-ConnectDialog callback for creating/editing connections.
 * Creates or edits connections based on state of the ConnectDialog when "Connect" is pressed.
*/
void ConnectionsDock::_make_or_edit_connection() {

    TreeItem *it = tree->get_selected();
    ERR_FAIL_COND(!it);

    NodePath dst_path = connect_dialog->get_dst_path();
    Node *target = selectedNode->get_node(dst_path);
    ERR_FAIL_COND(!target);

    Connection cToMake;
    cToMake.source = connect_dialog->get_source();
    cToMake.target = target;
    cToMake.signal = connect_dialog->get_signal_name();
    cToMake.method = connect_dialog->get_dst_method_name();
    cToMake.binds = connect_dialog->get_binds();
    bool defer = connect_dialog->get_deferred();
    bool oshot = connect_dialog->get_oneshot();
    cToMake.flags = ObjectNS::CONNECT_PERSIST | (defer ? ObjectNS::CONNECT_QUEUED : 0) | (oshot ? ObjectNS::CONNECT_ONESHOT : 0);

    // Conditions to add function: must have a script and must not have the method already
    // (in the class, the script itself, or inherited).
    bool add_script_function = false;
    Ref<Script> script = refFromRefPtr<Script>(target->get_script());
    if (script && !ClassDB::has_method(target->get_class_name(), cToMake.method)) {
        // There is a chance that the method is inherited from another script.
        bool found_inherited_function = false;
        Ref<Script> inherited_script = script->get_base_script();
        while (inherited_script) {
            int line = inherited_script->get_language()->find_function(cToMake.method, inherited_script->get_source_code());
            if (line != -1) {
                found_inherited_function = true;
                break;
            }

            inherited_script = inherited_script->get_base_script();
        }

        add_script_function = !found_inherited_function;
    }
    PoolVector<String> script_function_args;
    if (add_script_function) {
        // Pick up args here before "it" is deleted by update_tree.
        script_function_args = it->get_metadata(0).as<Dictionary>()["args"].as<PoolVector<String>>();
        for (int i = 0; i < cToMake.binds.size(); i++) {
            script_function_args.append("extra_arg_" + itos(i) + ":" + Variant::get_type_name(cToMake.binds[i].get_type()));
        }
    }

    if (connect_dialog->is_editing()) {
        _disconnect(*it);
        _connect(cToMake);
    } else {
        _connect(cToMake);
    }

    // IMPORTANT NOTE: _disconnect and _connect cause an update_tree, which will delete the object "it" is pointing to.
    it = nullptr;

    if (add_script_function) {
        editor->emit_signal("script_add_function_request", Variant(target), cToMake.method, script_function_args);
        hide();
    }

    update_tree();
}

/*
 * Creates single connection w/ undo-redo functionality.
*/
void ConnectionsDock::_connect(const Connection& cToMake) {

    Node *source = static_cast<Node *>(cToMake.source);
    Node *target = static_cast<Node *>(cToMake.target);

    if (!source || !target)
        return;
    String translated_fmt(TTR("Connect '%s' to '%s'"));
    undo_redo->create_action(FormatVE(translated_fmt.c_str(), cToMake.signal.asCString(), cToMake.method.asCString()));

    undo_redo->add_do_method(source, "connect", cToMake.signal, Variant(target), cToMake.method,
            Variant::fromVector(Span<const Variant>(cToMake.binds)), cToMake.flags);
    undo_redo->add_undo_method(source, "disconnect", cToMake.signal, Variant(target), cToMake.method);
    undo_redo->add_do_method(this, "update_tree");
    undo_redo->add_undo_method(this, "update_tree");
    undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree"); //to force redraw of scene tree
    undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

    undo_redo->commit_action();
}

/*
 * Break single connection w/ undo-redo functionality.
*/
void ConnectionsDock::_disconnect(TreeItem &item) {

    Connection c = item.get_metadata(0);
    ERR_FAIL_COND(c.source != selectedNode); // Shouldn't happen but... Bugcheck.

    String translated_fmt(TTR("Disconnect '%s' to '%s'"));
    undo_redo->create_action(FormatVE(translated_fmt.c_str(), c.signal.asCString(), c.method.asCString()));

    undo_redo->add_do_method(selectedNode, "disconnect", c.signal,Variant(c.target), c.method);
    undo_redo->add_undo_method(selectedNode, "connect", c.signal, Variant(c.target), c.method, Variant::fromVector(Span<const Variant>(c.binds)), c.flags);
    undo_redo->add_do_method(this, "update_tree");
    undo_redo->add_undo_method(this, "update_tree");
    undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree"); // To force redraw of scene tree.
    undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

    undo_redo->commit_action();
}

/*
 * Break all connections of currently selected signal.
 * Can undo-redo as a single action.
*/
void ConnectionsDock::_disconnect_all() {

    TreeItem *item = tree->get_selected();

    if (!_is_item_signal(*item))
        return;

    TreeItem *child = item->get_children();
    String signalName = item->get_metadata(0).operator Dictionary()["name"];
    String translated_fmt(TTR("Disconnect all from signal: '%s'"));
    undo_redo->create_action(FormatVE(translated_fmt.c_str(), signalName.c_str()));

    while (child) {
        Connection c = child->get_metadata(0);
        undo_redo->add_do_method(selectedNode, "disconnect", c.signal, Variant(c.target), c.method);
        undo_redo->add_undo_method(selectedNode, "connect", c.signal, Variant(c.target), c.method, Variant::fromVector(Span<const Variant>(c.binds)), c.flags);
        child = child->get_next();
    }

    undo_redo->add_do_method(this, "update_tree");
    undo_redo->add_undo_method(this, "update_tree");
    undo_redo->add_do_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");
    undo_redo->add_undo_method(EditorNode::get_singleton()->get_scene_tree_dock()->get_tree_editor(), "update_tree");

    undo_redo->commit_action();
}

void ConnectionsDock::_tree_item_selected() {

    TreeItem *item = tree->get_selected();
    if (!item) { // Unlikely. Disable button just in case.
        connect_button->set_text(TTR("Connect..."));
        connect_button->set_disabled(true);
    } else if (_is_item_signal(*item)) {
        connect_button->set_text(TTR("Connect..."));
        connect_button->set_disabled(false);
    } else {
        connect_button->set_text(TTR("Disconnect"));
        connect_button->set_disabled(false);
    }
}

void ConnectionsDock::_tree_item_activated() { // "Activation" on double-click.

    TreeItem *item = tree->get_selected();

    if (!item)
        return;

    if (_is_item_signal(*item)) {
        _open_connection_dialog(*item);
    } else {
        _go_to_script(*item);
    }
}

bool ConnectionsDock::_is_item_signal(TreeItem &item) {

    return item.get_parent() == tree->get_root() || item.get_parent()->get_parent() == tree->get_root();
}

/*
Open connection dialog with TreeItem data to CREATE a brand-new connection.
*/
void ConnectionsDock::_open_connection_dialog(TreeItem &item) {

    String signal = item.get_metadata(0).operator Dictionary()["name"];
    const String &signalname = signal;
    String midname(selectedNode->get_name());
    for (size_t i = 0; i < midname.length(); i++) { //TODO: Regex filter may be cleaner.
        char c = midname[i];
        if (!(isalpha(c) || isdigit(c) || c == '_')) {
            if (c == ' ') {
                // Replace spaces with underlines.
                c = '_';
            } else {
                // Remove any other characters.
                StringUtils::erase(midname,i,midname.size()-i);
                i--;
                continue;
            }
        }
        midname[i]=c;
    }

    Node *dst_node = selectedNode->get_owner() ? selectedNode->get_owner() : selectedNode;
    if (!dst_node || dst_node->get_script().is_null()) {
        dst_node = _find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root());
    }

    StringName dst_method("_on_" + midname + "_" + signal);

    Connection c;
    c.source = selectedNode;
    c.signal = StringName(signalname);
    c.target = dst_node;
    c.method = dst_method;
    connect_dialog->popup_dialog(StringUtils::from_utf8(signalname));
    connect_dialog->init(c);
    connect_dialog->set_title(TTR("Connect a Signal to a Method"));
}

/*
 * Open connection dialog with Connection data to EDIT an existing connection.
*/
void ConnectionsDock::_open_connection_dialog(const Connection& cToEdit) {

    Node *src = static_cast<Node *>(cToEdit.source);
    Node *dst = static_cast<Node *>(cToEdit.target);

    if (src && dst) {
        connect_dialog->set_title(TTR("Edit Connection:") + cToEdit.signal);
        connect_dialog->popup_centered();
        connect_dialog->init(cToEdit, true);
    }
}

/*
 * Open slot method location in script editor.
*/
void ConnectionsDock::_go_to_script(TreeItem &item) {

    if (_is_item_signal(item))
        return;

    Connection c = item.get_metadata(0);
    ERR_FAIL_COND(c.source != selectedNode); //shouldn't happen but...bugcheck

    if (!c.target)
        return;

    Ref<Script> script = refFromRefPtr<Script>(c.target->get_script());

    if (not script)
        return;

    if (script && ScriptEditor::get_singleton()->script_goto_method(script, c.method)) {
        editor->call_va("_editor_select", EditorNode::EDITOR_SCRIPT);
    }
}

void ConnectionsDock::_handle_signal_menu_option(int option) {

    TreeItem *item = tree->get_selected();

    if (!item)
        return;

    switch (option) {
        case CONNECT: {
            _open_connection_dialog(*item);
        } break;
        case DISCONNECT_ALL: {
            StringName signal_name = item->get_metadata(0).operator Dictionary()["name"];
            disconnect_all_dialog->set_text(FormatSN(TTR("Are you sure you want to remove all connections from the \"%s\" signal?").asCString(), signal_name.asCString()));
            disconnect_all_dialog->popup_centered();
        } break;
    }
}

void ConnectionsDock::_handle_slot_menu_option(int option) {

    TreeItem *item = tree->get_selected();

    if (!item)
        return;

    switch (option) {
        case EDIT: {
            Connection c = item->get_metadata(0);
            _open_connection_dialog(c);
        } break;
        case GO_TO_SCRIPT: {
            _go_to_script(*item);
        } break;
        case DISCONNECT: {
            _disconnect(*item);
            update_tree();
        } break;
    }
}

void ConnectionsDock::_rmb_pressed(Vector2 position) {

    TreeItem *item = tree->get_selected();

    if (!item)
        return;

    Vector2 global_position = tree->get_global_position() + position;

    if (_is_item_signal(*item)) {
        signal_menu->set_position(global_position);
        signal_menu->popup();
    } else {
        slot_menu->set_position(global_position);
        slot_menu->popup();
    }
}

void ConnectionsDock::_close() {

    hide();
}

void ConnectionsDock::_connect_pressed() {

    TreeItem *item = tree->get_selected();
    if (!item) {
        connect_button->set_disabled(true);
        return;
    }

    if (_is_item_signal(*item)) {
        _open_connection_dialog(*item);
    } else {
        _disconnect(*item);
        update_tree();
    }
}

void ConnectionsDock::_notification(int p_what) {

    if (p_what == EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED) {
        update_tree();
    }
}

void ConnectionsDock::_bind_methods() {

    MethodBinder::bind_method("_make_or_edit_connection", &ConnectionsDock::_make_or_edit_connection);
    MethodBinder::bind_method("_disconnect_all", &ConnectionsDock::_disconnect_all);
    MethodBinder::bind_method("_tree_item_selected", &ConnectionsDock::_tree_item_selected);
    MethodBinder::bind_method("_tree_item_activated", &ConnectionsDock::_tree_item_activated);
    MethodBinder::bind_method("_handle_signal_menu_option", &ConnectionsDock::_handle_signal_menu_option);
    MethodBinder::bind_method("_handle_slot_menu_option", &ConnectionsDock::_handle_slot_menu_option);
    MethodBinder::bind_method("_rmb_pressed", &ConnectionsDock::_rmb_pressed);
    MethodBinder::bind_method("_close", &ConnectionsDock::_close);
    MethodBinder::bind_method("_connect_pressed", &ConnectionsDock::_connect_pressed);
    MethodBinder::bind_method("update_tree", &ConnectionsDock::update_tree);
}

void ConnectionsDock::set_node(Node *p_node) {

    selectedNode = p_node;
    update_tree();
}

void ConnectionsDock::update_tree() {

    tree->clear();

    if (!selectedNode)
        return;

    TreeItem *root = tree->create_item();

    Vector<MethodInfo> node_signals;

    selectedNode->get_signal_list(&node_signals);

    bool did_script = false;
    StringName base = selectedNode->get_class_name();

    while (base) {

        Vector<MethodInfo> node_signals2;
        Ref<Texture> icon;
        String name;

        if (!did_script) {

            Ref<Script> scr(refFromRefPtr<Script>(selectedNode->get_script()));
            if (scr) {
                scr->get_script_signal_list(&node_signals2);
                if (PathUtils::is_resource_file(scr->get_path()))
                    name = PathUtils::get_file(scr->get_path());
                else
                    name = scr->get_class();

                if (has_icon(scr->get_class_name(), "EditorIcons")) {
                    icon = get_icon(scr->get_class_name(), "EditorIcons");
                }
            }

        } else {

            ClassDB::get_signal_list(base, &node_signals2, true);
            if (has_icon(base, "EditorIcons")) {
                icon = get_icon(base, "EditorIcons");
            }
            name = base;
        }

        TreeItem *pitem = nullptr;

        if (!node_signals2.empty()) {
            pitem = tree->create_item(root);
            pitem->set_text_utf8(0, name);
            pitem->set_icon(0, icon);
            pitem->set_selectable(0, false);
            pitem->set_editable(0, false);
            pitem->set_custom_bg_color(0, get_color("prop_subsection", "Editor"));
            eastl::sort(node_signals2.begin(), node_signals2.end());
        }

        for (MethodInfo &mi : node_signals2) {

            StringName signal_name = mi.name;
            String signaldesc("(");
            PoolVector<String> argnames;
            if (!mi.arguments.empty()) {
                int idx=0;
                for (PropertyInfo &pi : mi.arguments) {
                    if (0==idx)
                        signaldesc += ", ";

                    String tname("var");
                    if (pi.type == VariantType::OBJECT && pi.class_name != StringName()) {
                        tname = pi.class_name;
                    } else if (pi.type != VariantType::NIL) {
                        tname = Variant::get_type_name(pi.type);
                    }
                    signaldesc += String(pi.name.empty() ? StringName("arg " + itos(idx++)) : pi.name) + ": " + tname;
                    argnames.push_back(String(pi.name + String(":") + tname));
                }
            }
            signaldesc += ')';

            TreeItem *item = tree->create_item(pitem);
            item->set_text_utf8(0, String(signal_name) + signaldesc);
            Dictionary sinfo;
            sinfo["name"] = signal_name;
            sinfo["args"] = argnames;
            item->set_metadata(0, sinfo);
            item->set_icon(0, get_icon("Signal", "EditorIcons"));

            // Set tooltip with the signal's documentation.
            {
                String descr;
                bool found = false;

                Map<StringName, Map<StringName, String> >::iterator G = descr_cache.find(base);
                if (G!=descr_cache.end()) {
                    Map<StringName, String>::iterator F = G->second.find(signal_name);
                    if (F!=G->second.end()) {
                        found = true;
                        descr = F->second;
                    }
                }

                if (!found) {
                    DocData *dd = EditorHelp::get_doc_data();
                    auto F = dd->class_list.find(base);
                    while (F!=dd->class_list.end() && descr.empty()) {
                        for (size_t i = 0; i < F->second.defined_signals.size(); i++) {
                            if (F->second.defined_signals[i].name == signal_name.asCString()) {
                                descr = StringUtils::strip_edges(F->second.defined_signals[i].description);
                                break;
                            }
                        }
                        if (!F->second.inherits.empty()) {
                            F = dd->class_list.find(F->second.inherits);
                        } else {
                            break;
                        }
                    }
                    descr_cache[base][signal_name] = descr;
                }

                // "::" separators used in make_custom_tooltip for formatting.
                item->set_tooltip(0, StringName(String(signal_name) + "::" + signaldesc + "::" + descr));
            }

            // List existing connections
            List<Object::Connection> connections;
            selectedNode->get_signal_connection_list(signal_name, &connections);

            for (Object::Connection &c : connections) {
                if (!(c.flags & ObjectNS::CONNECT_PERSIST))
                    continue;

                Node *target = object_cast<Node>(c.target);
                if (!target)
                    continue;

                String path = String(selectedNode->get_path_to(target)) + " :: " + c.method + "()";
                if (c.flags & ObjectNS::CONNECT_QUEUED)
                    path += " (deferred)";
                if (c.flags & ObjectNS::CONNECT_ONESHOT)
                    path += " (oneshot)";
                if (!c.binds.empty()) {

                    path += " binds( ";
                    for (int i = 0; i < c.binds.size(); i++) {

                        if (i > 0)
                            path += ", ";
                        path += c.binds[i].as<String>();
                    }
                    path += " )";
                }

                TreeItem *item2 = tree->create_item(item);
                item2->set_text_utf8(0, path);
                item2->set_metadata(0, c);
                item2->set_icon(0, get_icon("Slot", "EditorIcons"));
            }
        }

        if (!did_script) {
            did_script = true;
        } else {
            base = ClassDB::get_parent_class(base);
        }
    }

    connect_button->set_text(TTR("Connect..."));
    connect_button->set_disabled(true);
}

ConnectionsDock::ConnectionsDock(EditorNode *p_editor) {

    editor = p_editor;
    set_name(TTR("Signals"));

    VBoxContainer *vbc = this;

    tree = memnew(ConnectionsDockTree);
    tree->set_columns(1);
    tree->set_select_mode(Tree::SELECT_ROW);
    tree->set_hide_root(true);
    vbc->add_child(tree);
    tree->set_v_size_flags(SIZE_EXPAND_FILL);
    tree->set_allow_rmb_select(true);

    connect_button = memnew(Button);
    HBoxContainer *hb = memnew(HBoxContainer);
    vbc->add_child(hb);
    hb->add_spacer();
    hb->add_child(connect_button);
    connect_button->connect("pressed", this, "_connect_pressed");

    connect_dialog = memnew(ConnectDialog);
    connect_dialog->set_as_toplevel(true);
    add_child(connect_dialog);

    disconnect_all_dialog = memnew(ConfirmationDialog);
    disconnect_all_dialog->set_as_toplevel(true);
    add_child(disconnect_all_dialog);
    disconnect_all_dialog->connect("confirmed", this, "_disconnect_all");
    disconnect_all_dialog->set_text(TTR("Are you sure you want to remove all connections from this signal?"));

    signal_menu = memnew(PopupMenu);
    add_child(signal_menu);
    signal_menu->connect("id_pressed", this, "_handle_signal_menu_option");
    signal_menu->add_item(TTR("Connect..."), CONNECT);
    signal_menu->add_item(TTR("Disconnect All"), DISCONNECT_ALL);

    slot_menu = memnew(PopupMenu);
    add_child(slot_menu);
    slot_menu->connect("id_pressed", this, "_handle_slot_menu_option");
    slot_menu->add_item(TTR("Edit..."), EDIT);
    slot_menu->add_item(TTR("Go To Method"), GO_TO_SCRIPT);
    slot_menu->add_item(TTR("Disconnect"), DISCONNECT);

    connect_dialog->connect("connected", this, "_make_or_edit_connection");
    tree->connect("item_selected", this, "_tree_item_selected");
    tree->connect("item_activated", this, "_tree_item_activated");
    tree->connect("item_rmb_selected", this, "_rmb_pressed");

    add_constant_override("separation", 3 * EDSCALE);
}

ConnectionsDock::~ConnectionsDock() {
}
