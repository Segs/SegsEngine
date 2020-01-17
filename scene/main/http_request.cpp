/*************************************************************************/
/*  http_request.cpp                                                     */
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

#include "http_request.h"

#include "core/os/os.h"
#include "core/method_bind.h"
#include "core/os/thread.h"
#include "core/pool_vector.h"
#include "core/os/file_access.h"
#include "scene/main/timer.h"
#include "core/string_utils.h"

#include <atomic>

IMPL_GDCLASS(HTTPRequest)

VARIANT_ENUM_CAST(HTTPRequest::Result);
//TODO: SEGS: duplicating HTTPClient enums here
VARIANT_ENUM_CAST(HTTPClient::Method);
VARIANT_ENUM_CAST(HTTPClient::Status);

namespace {
struct HTTPRequestData
{
    PODVector<String> headers;
    PoolByteArray body;
    PoolVector<String> response_headers;
    String request_string;
    String url;
    String request_data;
    String download_to_file;
    Ref<HTTPClient> client;
    FileAccess *file;
    Thread *thread;
    std::atomic<int> downloaded;
    int port;
    int response_code;
    int body_len;
    int body_size_limit;
    int redirections;
    int max_redirects;
    int timeout;
    HTTPClient::Method method;
    std::atomic<bool> use_threads;
    std::atomic<bool> thread_done;
    std::atomic<bool> thread_request_quit;
    bool requesting;
    bool validate_ssl;
    bool use_ssl;
    bool request_sent;
    bool got_response;
};
void initialize(HTTPRequestData &d) {
    d.thread = nullptr;

    d.port = 80;
    d.redirections = 0;
    d.max_redirects = 8;
    d.body_len = -1;
    d.got_response = false;
    d.validate_ssl = false;
    d.use_ssl = false;
    d.response_code = 0;
    d.request_sent = false;
    d.requesting = false;
    d.client = make_ref_counted<HTTPClient>();
    d.use_threads = false;
    d.thread_done = false;
    d.downloaded = 0;
    d.body_size_limit = -1;
    d.file = nullptr;
    d.timeout = 0;

}
static Error _request(HTTPRequestData &impl) {

    return impl.client->connect_to_host(impl.url, impl.port, impl.use_ssl, impl.validate_ssl);
}

Error _parse_url(HTTPRequestData &impl,se_string_view p_url) {

    impl.url = p_url;
    impl.use_ssl = false;

    impl.request_string = "";
    impl.port = 80;
    impl.request_sent = false;
    impl.got_response = false;
    impl.body_len = -1;
    impl.body.resize(0);
    impl.downloaded = 0;
    impl.redirections = 0;

    String url_lower(StringUtils::to_lower(impl.url));
    if (StringUtils::begins_with(url_lower,"http://")) {
        impl.url = StringUtils::substr(impl.url,7);
    } else if (StringUtils::begins_with(url_lower,"https://")) {
        impl.url = StringUtils::substr(impl.url,8);
        impl.use_ssl = true;
        impl.port = 443;
    } else {
        ERR_FAIL_V_MSG(ERR_INVALID_PARAMETER, "Malformed URL: " + impl.url + ".")
    }

    ERR_FAIL_COND_V_MSG(impl.url.length() < 1, ERR_INVALID_PARAMETER, "URL too short: " + impl.url + ".")

    int slash_pos = StringUtils::find(impl.url,"/");

    if (slash_pos != -1) {
        impl.request_string = StringUtils::substr(impl.url,slash_pos);
        impl.url = StringUtils::substr(impl.url,0, slash_pos);
    } else {
        impl.request_string = "/";
    }

    int colon_pos = StringUtils::find(impl.url,":");
    if (colon_pos != -1) {
        impl.port = StringUtils::to_int(StringUtils::substr(impl.url,colon_pos + 1));
        impl.url = StringUtils::substr(impl.url,0, colon_pos);
        ERR_FAIL_COND_V(impl.port < 1 || impl.port > 65535, ERR_INVALID_PARAMETER)
    }

    return OK;
}

bool _handle_response(HTTPRequestData &impl,HTTPRequest *tgt,bool *ret_value) {
    using namespace StringUtils;

    if (!impl.client->has_response()) {
        tgt->call_deferred("_request_done", HTTPRequest::RESULT_NO_RESPONSE, 0, PoolSeStringArray(), PoolByteArray());
        *ret_value = true;
        return true;
    }

    impl.got_response = true;
    impl.response_code = impl.client->get_response_code();
    List<String> rheaders;
    impl.client->get_response_headers(&rheaders);
    impl.response_headers.resize(0);
    impl.downloaded = 0;
    for (List<String>::Element *E = rheaders.front(); E; E = E->next()) {
        impl.response_headers.push_back(E->deref());
    }

    if (impl.response_code == 301 || impl.response_code == 302) {
        // Handle redirect

        if (impl.max_redirects >= 0 && impl.redirections >= impl.max_redirects) {

            tgt->call_deferred("_request_done", HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED, impl.response_code, impl.response_headers, PoolByteArray());
            *ret_value = true;
            return true;
        }

        String new_request;

        for (List<String>::Element *E = rheaders.front(); E; E = E->next()) {
            if (StringUtils::contains(StringUtils::to_lower(E->deref()),"location: ") ) {
                new_request = (strip_edges(substr(E->deref(),9))).data();
            }
        }

        if (!new_request.empty()) {
            // Process redirect
            impl.client->close();
            int new_redirs = impl.redirections + 1; // Because _request() will clear it
            Error err;
            if (StringUtils::begins_with(new_request,"http")) {
                // New url, request all again
                _parse_url(impl,new_request);
            } else {
                impl.request_string = new_request;
            }

            err = _request(impl);
            if (err == OK) {
                impl.request_sent = false;
                impl.got_response = false;
                impl.body_len = -1;
                impl.body.resize(0);
                impl.downloaded = 0;
                impl.redirections = new_redirs;
                *ret_value = false;
                return true;
            }
        }
    }

    return false;
}

bool _update_connection(HTTPRequestData &impl,HTTPRequest *tgt) {

    switch (impl.client->get_status()) {
        case HTTPClient::STATUS_DISCONNECTED: {
            tgt->call_deferred("_request_done", HTTPRequest::RESULT_CANT_CONNECT, 0, PoolSeStringArray(), PoolByteArray());
            return true; // End it, since it's doing something
        }
        case HTTPClient::STATUS_RESOLVING: {
            impl.client->poll();
            // Must wait
            return false;
        }
        case HTTPClient::STATUS_CANT_RESOLVE: {
            tgt->call_deferred("_request_done", HTTPRequest::RESULT_CANT_RESOLVE, 0, PoolSeStringArray(), PoolByteArray());
            return true;

        }
        case HTTPClient::STATUS_CONNECTING: {
            impl.client->poll();
            // Must wait
            return false;
        } // Connecting to IP
        case HTTPClient::STATUS_CANT_CONNECT: {

            tgt->call_deferred("_request_done", HTTPRequest::RESULT_CANT_CONNECT, 0, PoolSeStringArray(), PoolByteArray());
            return true;

        }
        case HTTPClient::STATUS_CONNECTED: {

            if (impl.request_sent) {

                if (!impl.got_response) {

                    // No body

                    bool ret_value;

                    if (_handle_response(impl,tgt,&ret_value))
                        return ret_value;

                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_SUCCESS, impl.response_code, impl.response_headers, PoolByteArray());
                    return true;
                }
                if (impl.body_len < 0) {
                    // Chunked transfer is done
                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_SUCCESS,impl.response_code, impl.response_headers, impl.body);
                    return true;
                }

                tgt->call_deferred("_request_done", HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH,impl.response_code, impl.response_headers, PoolByteArray());
                return true;
                // Request migh have been done
            } else {
                // Did not request yet, do request

                Error err = impl.client->request(impl.method, (impl.request_string), impl.headers, impl.request_data);
                if (err != OK) {
                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_CONNECTION_ERROR, 0, PoolSeStringArray(), PoolByteArray());
                    return true;
                }

                impl.request_sent = true;
                return false;
            }
        } // Connected: break requests only accepted here
        case HTTPClient::STATUS_REQUESTING: {
            // Must wait, still requesting
            impl.client->poll();
            return false;

        } // Request in progress
        case HTTPClient::STATUS_BODY: {

            if (!impl.got_response) {

                bool ret_value;

                if (_handle_response(impl,tgt,&ret_value))
                    return ret_value;

                if (!impl.client->is_response_chunked() && impl.client->get_response_body_length() == 0) {

                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_SUCCESS, impl.response_code, impl.response_headers, PoolByteArray());
                    return true;
                }

                // No body len (-1) if chunked or no content-length header was provided.
                // Change your webserver configuration if you want body len.
                impl.body_len = impl.client->get_response_body_length();

                if (impl.body_size_limit >= 0 && impl.body_len > impl.body_size_limit) {
                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED, impl.response_code, impl.response_headers, PoolByteArray());
                    return true;
                }

                if (!impl.download_to_file.empty()) {
                    impl.file = FileAccess::open(impl.download_to_file, FileAccess::WRITE);
                    if (!impl.file) {

                        tgt->call_deferred("_request_done", HTTPRequest::RESULT_DOWNLOAD_FILE_CANT_OPEN, impl.response_code, impl.response_headers, PoolByteArray());
                        return true;
                    }
                }
            }

            impl.client->poll();

            PoolByteArray chunk = impl.client->read_response_body_chunk();
            impl.downloaded += chunk.size();

            if (impl.file) {
                PoolByteArray::Read r = chunk.read();
                impl.file->store_buffer(r.ptr(), chunk.size());
                if (impl.file->get_error() != OK) {
                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_DOWNLOAD_FILE_WRITE_ERROR, impl.response_code, impl.response_headers, PoolByteArray());
                    return true;
                }
            } else {
                impl.body.append_array(chunk);
            }

            if (impl.body_size_limit >= 0 && impl.downloaded > impl.body_size_limit) {
                tgt->call_deferred("_request_done", HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED, impl.response_code, impl.response_headers, PoolByteArray());
                return true;
            }

            if (impl.body_len >= 0) {

                if (impl.downloaded == impl.body_len) {
                    tgt->call_deferred("_request_done", HTTPRequest::RESULT_SUCCESS, impl.response_code, impl.response_headers,impl.body);
                    return true;
                }
            } else if (impl.client->get_status() == HTTPClient::STATUS_DISCONNECTED) {
                // We read till EOF, with no errors. Request is done.
                tgt->call_deferred("_request_done", HTTPRequest::RESULT_SUCCESS, impl.response_code, impl.response_headers, impl.body);
            }

            return false;

        } // Request resulted in body: break which must be read
        case HTTPClient::STATUS_CONNECTION_ERROR: {
            tgt->call_deferred("_request_done", HTTPRequest::RESULT_CONNECTION_ERROR, 0, PoolSeStringArray(), PoolByteArray());
            return true;
        }
        case HTTPClient::STATUS_SSL_HANDSHAKE_ERROR: {
            tgt->call_deferred("_request_done", HTTPRequest::RESULT_SSL_HANDSHAKE_ERROR, 0, PoolSeStringArray(), PoolByteArray());
            return true;
        }
    }

    ERR_FAIL_V(false)
}

static void _thread_func(void *p_userdata) {

    HTTPRequest *hr = (HTTPRequest *)p_userdata;
    HTTPRequestData *hrdat = (HTTPRequestData *)hr->m_impl;

    Error err = _request(*hrdat);

    if (err != OK) {
        hr->call_deferred("_request_done", HTTPRequest::RESULT_CANT_CONNECT, 0, PoolSeStringArray(), PoolByteArray());
    } else {
        while (!hrdat->thread_request_quit) {

            bool exit = _update_connection(*hrdat,hr);
            if (exit)
                break;
            OS::get_singleton()->delay_usec(1);
        }
    }

    hrdat->thread_done = true;
}
} // end of anonymous namespace
void HTTPRequest::_redirect_request(se_string_view /*p_new_url*/) {
}

#define IMPLD() ((HTTPRequestData*)m_impl)
Error HTTPRequest::request(se_string_view p_url, const PODVector<String> &p_custom_headers, bool p_ssl_validate_domain, HTTPClient::Method p_method, se_string_view p_request_data) {

    ERR_FAIL_COND_V(!is_inside_tree(), ERR_UNCONFIGURED)
    ERR_FAIL_COND_V_MSG(IMPLD()->requesting, ERR_BUSY, "HTTPRequest is processing a request. Wait for completion or cancel it before attempting a new one.")

    if (IMPLD()->timeout > 0) {
        timer->stop();
        timer->start(IMPLD()->timeout);
    }

    IMPLD()->method = p_method;

    Error err = _parse_url(*IMPLD(),p_url);
    if (err)
        return err;

    IMPLD()->validate_ssl = p_ssl_validate_domain;

    IMPLD()->headers = p_custom_headers;

    IMPLD()->request_data = p_request_data;

    IMPLD()->requesting = true;

    if (IMPLD()->use_threads) {

        IMPLD()->thread_done = false;
        IMPLD()->thread_request_quit = false;
        IMPLD()->client->set_blocking_mode(true);
        IMPLD()->thread = Thread::create(_thread_func, this);
    } else {
        IMPLD()->client->set_blocking_mode(false);
        err = _request(*IMPLD());
        if (err != OK) {
            call_deferred("_request_done", RESULT_CANT_CONNECT, 0, PoolSeStringArray(), PoolByteArray());
            return ERR_CANT_CONNECT;
        }

        set_process_internal(true);
    }

    return OK;
}



void HTTPRequest::cancel_request() {

    timer->stop();

    if (!IMPLD()->requesting)
        return;

    if (!IMPLD()->use_threads) {
        set_process_internal(false);
    } else {
        IMPLD()->thread_request_quit = true;
        Thread::wait_to_finish(IMPLD()->thread);
        memdelete(IMPLD()->thread);
        IMPLD()->thread = nullptr;
    }

    if (IMPLD()->file) {
        memdelete(IMPLD()->file);
        IMPLD()->file = nullptr;
    }
    IMPLD()->client->close();
    IMPLD()->body.resize(0);
    IMPLD()->got_response = false;
    IMPLD()->response_code = -1;
    IMPLD()->request_sent = false;
    IMPLD()->requesting = false;
}

void HTTPRequest::_request_done(int p_status, int p_code, const PoolSeStringArray &headers, const PoolByteArray &p_data) {

    cancel_request();
    emit_signal("request_completed", p_status, p_code, headers, p_data);
}

void HTTPRequest::_notification(int p_what) {

    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {

        if (IMPLD()->use_threads)
            return;
        bool done = _update_connection(*IMPLD(),this);
        if (done) {

            set_process_internal(false);
            // cancel_request(); called from _request done now
        }
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {
        if (IMPLD()->requesting) {
            cancel_request();
        }
    }
}

void HTTPRequest::set_use_threads(bool p_use) {

    ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED)
    IMPLD()->use_threads = p_use;
}

bool HTTPRequest::is_using_threads() const {

    return IMPLD()->use_threads;
}

void HTTPRequest::set_body_size_limit(int p_bytes) {

    ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED)

    IMPLD()->body_size_limit = p_bytes;
}

int HTTPRequest::get_body_size_limit() const {

    return IMPLD()->body_size_limit;
}

void HTTPRequest::set_download_file(se_string_view p_file) {

    ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED)

    IMPLD()->download_to_file = p_file;
}

const String &HTTPRequest::get_download_file() const {

    return IMPLD()->download_to_file;
}

void HTTPRequest::set_download_chunk_size(int p_chunk_size) {

    ERR_FAIL_COND(get_http_client_status() != HTTPClient::STATUS_DISCONNECTED);

    IMPLD()->client->set_read_chunk_size(p_chunk_size);
}

int HTTPRequest::get_download_chunk_size() const {
    return IMPLD()->client->get_read_chunk_size();
}

HTTPClient::Status HTTPRequest::get_http_client_status() const {
    return IMPLD()->client->get_status();
}

void HTTPRequest::set_max_redirects(int p_max) {

    IMPLD()->max_redirects = p_max;
}

int HTTPRequest::get_max_redirects() const {

    return IMPLD()->max_redirects;
}

int HTTPRequest::get_downloaded_bytes() const {

    return IMPLD()->downloaded;
}
int HTTPRequest::get_body_size() const {
    return IMPLD()->body_len;
}

void HTTPRequest::set_timeout(int p_timeout) {

    ERR_FAIL_COND(p_timeout < 0)
    IMPLD()->timeout = p_timeout;
}

int HTTPRequest::get_timeout() {

    return IMPLD()->timeout;
}

void HTTPRequest::_timeout() {

    cancel_request();
    call_deferred("_request_done", RESULT_TIMEOUT, 0, PoolSeStringArray(), PoolByteArray());
}

void HTTPRequest::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("request", {"url", "custom_headers", "ssl_validate_domain", "method", "request_data"}), &HTTPRequest::request, {DEFVAL(PoolSeStringArray()), DEFVAL(true), DEFVAL(HTTPClient::METHOD_GET), DEFVAL(se_string_view())});
    MethodBinder::bind_method(D_METHOD("cancel_request"), &HTTPRequest::cancel_request);

    MethodBinder::bind_method(D_METHOD("get_http_client_status"), &HTTPRequest::get_http_client_status);

    MethodBinder::bind_method(D_METHOD("set_use_threads", {"enable"}), &HTTPRequest::set_use_threads);
    MethodBinder::bind_method(D_METHOD("is_using_threads"), &HTTPRequest::is_using_threads);

    MethodBinder::bind_method(D_METHOD("set_body_size_limit", {"bytes"}), &HTTPRequest::set_body_size_limit);
    MethodBinder::bind_method(D_METHOD("get_body_size_limit"), &HTTPRequest::get_body_size_limit);

    MethodBinder::bind_method(D_METHOD("set_max_redirects", {"amount"}), &HTTPRequest::set_max_redirects);
    MethodBinder::bind_method(D_METHOD("get_max_redirects"), &HTTPRequest::get_max_redirects);

    MethodBinder::bind_method(D_METHOD("set_download_file", {"path"}), &HTTPRequest::set_download_file);
    MethodBinder::bind_method(D_METHOD("get_download_file"), &HTTPRequest::get_download_file);

    MethodBinder::bind_method(D_METHOD("get_downloaded_bytes"), &HTTPRequest::get_downloaded_bytes);
    MethodBinder::bind_method(D_METHOD("get_body_size"), &HTTPRequest::get_body_size);

    MethodBinder::bind_method(D_METHOD("_redirect_request"), &HTTPRequest::_redirect_request);
    MethodBinder::bind_method(D_METHOD("_request_done"), &HTTPRequest::_request_done);

    MethodBinder::bind_method(D_METHOD("set_timeout", {"timeout"}), &HTTPRequest::set_timeout);
    MethodBinder::bind_method(D_METHOD("get_timeout"), &HTTPRequest::get_timeout);

    MethodBinder::bind_method(D_METHOD("set_download_chunk_size"), &HTTPRequest::set_download_chunk_size);
    MethodBinder::bind_method(D_METHOD("get_download_chunk_size"), &HTTPRequest::get_download_chunk_size);

    MethodBinder::bind_method(D_METHOD("_timeout"), &HTTPRequest::_timeout);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "download_file", PROPERTY_HINT_FILE), "set_download_file", "get_download_file");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "download_chunk_size", PROPERTY_HINT_RANGE, "256,16777216"), "set_download_chunk_size", "get_download_chunk_size");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_threads"), "set_use_threads", "is_using_threads");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "body_size_limit", PROPERTY_HINT_RANGE, "-1,2000000000"), "set_body_size_limit", "get_body_size_limit");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "max_redirects", PROPERTY_HINT_RANGE, "-1,64"), "set_max_redirects", "get_max_redirects");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "timeout", PROPERTY_HINT_RANGE, "0,86400"), "set_timeout", "get_timeout");

    ADD_SIGNAL(MethodInfo("request_completed", PropertyInfo(VariantType::INT, "result"), PropertyInfo(VariantType::INT, "response_code"), PropertyInfo(VariantType::POOL_STRING_ARRAY, "headers"), PropertyInfo(VariantType::POOL_BYTE_ARRAY, "body")));

    BIND_ENUM_CONSTANT(RESULT_SUCCESS)
    //BIND_ENUM_CONSTANT( RESULT_NO_BODY )
    BIND_ENUM_CONSTANT(RESULT_CHUNKED_BODY_SIZE_MISMATCH)
    BIND_ENUM_CONSTANT(RESULT_CANT_CONNECT)
    BIND_ENUM_CONSTANT(RESULT_CANT_RESOLVE)
    BIND_ENUM_CONSTANT(RESULT_CONNECTION_ERROR)
    BIND_ENUM_CONSTANT(RESULT_SSL_HANDSHAKE_ERROR)
    BIND_ENUM_CONSTANT(RESULT_NO_RESPONSE)
    BIND_ENUM_CONSTANT(RESULT_BODY_SIZE_LIMIT_EXCEEDED)
    BIND_ENUM_CONSTANT(RESULT_REQUEST_FAILED)
    BIND_ENUM_CONSTANT(RESULT_DOWNLOAD_FILE_CANT_OPEN)
    BIND_ENUM_CONSTANT(RESULT_DOWNLOAD_FILE_WRITE_ERROR)
    BIND_ENUM_CONSTANT(RESULT_REDIRECT_LIMIT_REACHED)
    BIND_ENUM_CONSTANT(RESULT_TIMEOUT)
}

HTTPRequest::HTTPRequest() {
    timer = memnew(Timer);
    timer->set_one_shot(true);
    timer->connect("timeout", this, "_timeout");
    add_child(timer);
    HTTPRequestData *dat = memnew(HTTPRequestData);
    m_impl = dat;
    initialize(*dat);
}

HTTPRequest::~HTTPRequest() {
    if(IMPLD()) {
        if (IMPLD()->file)
            memdelete(IMPLD()->file);
        memdelete(IMPLD());
    }
}
#undef IMPLD
