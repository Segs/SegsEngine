/*************************************************************************/
/*  http_request.h                                                       */
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

#include "core/io/http_client.h"
#include "scene/main/node.h"

class Timer;
class HTTPRequest : public Node {

    GDCLASS(HTTPRequest,Node)

public:
    enum Result {
        RESULT_SUCCESS,
        RESULT_CHUNKED_BODY_SIZE_MISMATCH,
        RESULT_CANT_CONNECT,
        RESULT_CANT_RESOLVE,
        RESULT_CONNECTION_ERROR,
        RESULT_SSL_HANDSHAKE_ERROR,
        RESULT_NO_RESPONSE,
        RESULT_BODY_SIZE_LIMIT_EXCEEDED,
        RESULT_REQUEST_FAILED,
        RESULT_DOWNLOAD_FILE_CANT_OPEN,
        RESULT_DOWNLOAD_FILE_WRITE_ERROR,
        RESULT_REDIRECT_LIMIT_REACHED,
        RESULT_TIMEOUT

    };

private:

    void _redirect_request(se_string_view p_new_url);
    void _request_done(int p_status, int p_code, const PoolSeStringArray &headers, const PoolByteArray &p_data);

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    //! connects to a full url and perform request
    Error request(se_string_view p_url, const PODVector<String> &p_custom_headers = {}, bool p_ssl_validate_domain = true, HTTPClient::Method p_method = HTTPClient::METHOD_GET, se_string_view p_request_data = {});
    void cancel_request();
    HTTPClient::Status get_http_client_status() const;

    void set_use_threads(bool p_use);
    bool is_using_threads() const;

    void set_download_file(se_string_view p_file);
    const String &get_download_file() const;

    void set_download_chunk_size(int p_chunk_size);
    int get_download_chunk_size() const;

    void set_body_size_limit(int p_bytes);
    int get_body_size_limit() const;

    void set_max_redirects(int p_max);
    int get_max_redirects() const;

    Timer *timer;
    void *m_impl = nullptr; // made public to allow implementation methods to access it.

    void set_timeout(int p_timeout);
    int get_timeout();

    void _timeout();

    int get_downloaded_bytes() const;
    int get_body_size() const;

    HTTPRequest();
    ~HTTPRequest() override;
};
