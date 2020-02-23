/*************************************************************************/
/*  gdscript_language_protocol.cpp                                       */
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

#include "gdscript_language_protocol.h"
#include "core/io/json.h"
#include "core/project_settings.h"
#include "editor/editor_node.h"
#include "core/method_bind.h"

IMPL_GDCLASS(GDScriptLanguageProtocol)

GDScriptLanguageProtocol *GDScriptLanguageProtocol::singleton = nullptr;

void GDScriptLanguageProtocol::on_data_received(int p_id) {
    lastest_client_id = p_id;
    Ref<WebSocketPeer> peer = server->get_peer(p_id);
    PoolByteArray data;
    if (OK == peer->get_packet_buffer(data)) {
        String message((const char *)data.read().ptr(), data.size());
        if (StringUtils::begins_with(message,"Content-Length:")) return;
        String output = process_message(message);
        if (!output.empty()) {
            peer->put_packet((const uint8_t *)output.data(), output.length());
        }
    }
}

void GDScriptLanguageProtocol::on_client_connected(int p_id, se_string_view p_protocal) {
    clients[p_id] = server->get_peer(p_id);
}

void GDScriptLanguageProtocol::on_client_disconnected(int p_id, bool p_was_clean_close) {
    clients.erase(p_id);
}

String GDScriptLanguageProtocol::process_message(const String &p_text) {
    String ret = process_string(p_text);
    if (ret.empty()) {
        return ret;
    } else {
        return format_output(ret);
    }
}

String GDScriptLanguageProtocol::format_output(se_string_view p_text) {

    String header("Content-Length: ");
    size_t len = p_text.length();
    header += itos(len);
    header += "\r\n\r\n";

    return header + p_text;
}

void GDScriptLanguageProtocol::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("initialize", {"params"}), &GDScriptLanguageProtocol::initialize);
    MethodBinder::bind_method(D_METHOD("initialized", {"params"}), &GDScriptLanguageProtocol::initialized);
    MethodBinder::bind_method(D_METHOD("on_data_received"), &GDScriptLanguageProtocol::on_data_received);
    MethodBinder::bind_method(D_METHOD("on_client_connected"), &GDScriptLanguageProtocol::on_client_connected);
    MethodBinder::bind_method(D_METHOD("on_client_disconnected"), &GDScriptLanguageProtocol::on_client_disconnected);
    MethodBinder::bind_method(D_METHOD("notify_all_clients", {"p_method", "p_params"}), &GDScriptLanguageProtocol::notify_all_clients, {DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("notify_client", {"p_method", "p_params", "p_client"}), &GDScriptLanguageProtocol::notify_client, {DEFVAL(Variant()), DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("is_smart_resolve_enabled"), &GDScriptLanguageProtocol::is_smart_resolve_enabled);
    MethodBinder::bind_method(D_METHOD("get_text_document"), &GDScriptLanguageProtocol::get_text_document);
    MethodBinder::bind_method(D_METHOD("get_workspace"), &GDScriptLanguageProtocol::get_workspace);
    MethodBinder::bind_method(D_METHOD("is_initialized"), &GDScriptLanguageProtocol::is_initialized);
}

Dictionary GDScriptLanguageProtocol::initialize(const Dictionary &p_params) {

    lsp::InitializeResult ret;

    String root_uri = p_params["rootUri"];
    String root = p_params["rootPath"];
    bool is_same_workspace;
#ifndef WINDOWS_ENABLED
    is_same_workspace = StringUtils::to_lower(root) == StringUtils::to_lower(workspace->root);
#else
    is_same_workspace = StringUtils::to_lower(StringUtils::replace(root,"\\", "/")) == StringUtils::to_lower(workspace->root);
#endif

    if (root_uri.length() && is_same_workspace) {
        workspace->root_uri = root_uri;
    } else {

        workspace->root_uri = "file://" + workspace->root;

        Dictionary params;
        params["path"] = workspace->root;
        Dictionary request = make_notification("gdscrip_client/changeWorkspace", params);
        auto peer = clients.find(lastest_client_id);
        if (peer!=clients.end()) {
            String msg = JSON::print(request);
            msg = format_output(msg);
            peer->second->put_packet((const uint8_t *)msg.data(), msg.length());
        }
    }

    if (!_initialized) {
        workspace->initialize();
        text_document->initialize();
        _initialized = true;
    }

    return ret.to_json();
}

void GDScriptLanguageProtocol::initialized(const Variant &p_params) {

    lsp::GodotCapabilities capabilities;

    DocData *doc = EditorHelp::get_doc_data();
    for (auto &E : doc->class_list) {

        lsp::GodotNativeClassInfo gdclass;
        gdclass.name = E.second.name;
        gdclass.class_doc = &E.second;
        auto iter = ClassDB::classes.find(StringName(E.second.name));
        gdclass.class_info = iter!=ClassDB::classes.end() ? &iter->second : nullptr;
        capabilities.native_classes.push_back(gdclass);
    }

    notify_client("gdscript/capabilities", capabilities.to_json());
}

void GDScriptLanguageProtocol::poll() {
    server->poll();
}

Error GDScriptLanguageProtocol::start(int p_port, const IP_Address &p_bind_ip) {
    if (server == nullptr) {
        server = dynamic_cast<WebSocketServer *>(ClassDB::instance("WebSocketServer"));
        ERR_FAIL_COND_V(!server, FAILED);
        server->set_buffers(8192, 1024, 8192, 1024); // 8mb should be way more than enough
        server->connect("data_received", this, "on_data_received");
        server->connect("client_connected", this, "on_client_connected");
        server->connect("client_disconnected", this, "on_client_disconnected");
    }
    server->set_bind_ip(p_bind_ip);
    return server->listen(p_port);
}

void GDScriptLanguageProtocol::stop() {

    for(const auto &cl : clients) {
        cl.second->close();
    }
    server->stop();
    clients.clear();
}

void GDScriptLanguageProtocol::notify_all_clients(se_string_view p_method, const Variant &p_params) {

    Dictionary message = make_notification(p_method, p_params);
    String msg = JSON::print(message);
    msg = format_output(msg);

    for(const auto &cl : clients) {
        Ref<WebSocketPeer> peer = cl.second;
        peer->put_packet((const uint8_t *)msg.data(), msg.length());
    }
}

void GDScriptLanguageProtocol::notify_client(se_string_view p_method, const Variant &p_params, int p_client) {

    if (p_client == -1) {
        p_client = lastest_client_id;
    }

    auto peer_iter = clients.find(p_client);
    ERR_FAIL_COND(peer_iter == clients.end());

    Dictionary message = make_notification(p_method, p_params);
    String msg = JSON::print(message);
    msg = format_output(msg);

    peer_iter->second->put_packet((const uint8_t *)msg.data(), msg.length());
}

bool GDScriptLanguageProtocol::is_smart_resolve_enabled() const {
    return bool(_EDITOR_GET(("network/language_server/enable_smart_resolve")));
}

bool GDScriptLanguageProtocol::is_goto_native_symbols_enabled() const {
    return bool(_EDITOR_GET(("network/language_server/show_native_symbols_in_editor")));
}

GDScriptLanguageProtocol::GDScriptLanguageProtocol() {
    server = nullptr;
    singleton = this;
    _initialized = false;
    workspace = make_ref_counted<GDScriptWorkspace>();
    text_document = make_ref_counted<GDScriptTextDocument>();
    set_scope(("textDocument"), text_document.get());
    set_scope(("completionItem"), text_document.get());
    set_scope(("workspace"), workspace.get());
    workspace->root = ProjectSettings::get_singleton()->get_resource_path();
}

GDScriptLanguageProtocol::~GDScriptLanguageProtocol() {
    memdelete(server);
    server = nullptr;
}
