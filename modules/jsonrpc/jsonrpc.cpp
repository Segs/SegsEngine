/*************************************************************************/
/*  jsonrpc.cpp                                                          */
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

#include "jsonrpc.h"

#include "core/dictionary.h"
#include "core/io/json.h"
#include "core/method_bind.h"

VARIANT_ENUM_CAST(JSONRPC::ErrorCode);

JSONRPC::JSONRPC() {
}

JSONRPC::~JSONRPC() {
}

IMPL_GDCLASS(JSONRPC)

void JSONRPC::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_scope", {"scope", "target"}), &JSONRPC::set_scope);
    MethodBinder::bind_method(D_METHOD("process_action", {"action", "recurse"}), &JSONRPC::process_action, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("process_string", {"action"}), &JSONRPC::process_string);

    MethodBinder::bind_method(D_METHOD("make_request", {"method", "params", "id"}), &JSONRPC::make_request);
    MethodBinder::bind_method(D_METHOD("make_response", {"result", "id"}), &JSONRPC::make_response);
    MethodBinder::bind_method(D_METHOD("make_notification", {"method", "params"}), &JSONRPC::make_notification);
    MethodBinder::bind_method(D_METHOD("make_response_error", {"code", "message", "id"}), &JSONRPC::make_response_error, {DEFVAL(Variant())});

    BIND_ENUM_CONSTANT(PARSE_ERROR);
    BIND_ENUM_CONSTANT(INVALID_REQUEST);
    BIND_ENUM_CONSTANT(METHOD_NOT_FOUND);
    BIND_ENUM_CONSTANT(INVALID_PARAMS);
    BIND_ENUM_CONSTANT(INTERNAL_ERROR);
}

Dictionary JSONRPC::make_response_error(int p_code, StringView p_message, const Variant &p_id) const {
    Dictionary dict;
    dict["jsonrpc"] = "2.0";

    Dictionary err;
    err["code"] = p_code;
    err["message"] = p_message;

    dict["error"] = err;
    dict["id"] = p_id;

    return dict;
}

Dictionary JSONRPC::make_response(const Variant &p_value, const Variant &p_id) {
    Dictionary dict;
    dict["jsonrpc"] = "2.0";
    dict["id"] = p_id;
    dict["result"] = p_value;
    return dict;
}

Dictionary JSONRPC::make_notification(StringView p_method, const Variant &p_params) {
    Dictionary dict;
    dict["jsonrpc"] = "2.0";
    dict["method"] = p_method;
    dict["params"] = p_params;
    return dict;
}

Dictionary JSONRPC::make_request(StringView p_method, const Variant &p_params, const Variant &p_id) {
    Dictionary dict;
    dict["jsonrpc"] = "2.0";
    dict["method"] = p_method;
    dict["params"] = p_params;
    dict["id"] = p_id;
    return dict;
}

Variant JSONRPC::process_action(const Variant &p_action, bool p_process_arr_elements) {
    Variant ret;
    if (p_action.get_type() == VariantType::DICTIONARY) {
        Dictionary dict = p_action.as<Dictionary>();
        String method = dict.get("method", "").as<String>();
        if (method.starts_with("$/")) {
            return ret;
        }
        Array args;
        if (dict.has("params")) {
            Variant params = dict.get("params", Variant());
            if (params.get_type() == VariantType::ARRAY) {
                args = params.as<Array>();
            } else {
                args.push_back(params);
            }
        }

        Object *object = this;
        if (method_scopes.contains(PathUtils::get_base_dir(method))) {
            object = method_scopes[PathUtils::get_base_dir(method)];
            method = PathUtils::get_file(method);
        }

        Variant id;
        if (dict.has("id")) {
            id = dict["id"];
        }

        if (object == nullptr || !object->has_method(StringName(method))) {
            ret = make_response_error(JSONRPC::METHOD_NOT_FOUND, ("Method not found") + method, id);
        } else {
            const Variant** argptrs = nullptr;
            int argc = args.size();
            if (argc > 0) {
                argptrs = (const Variant**)alloca(sizeof(Variant*) * argc);
                for (int i = 0; i < argc; i++) {
                    argptrs[i] = &args[i];
                }
            }
            Callable::CallError ce;
            Variant call_ret = object->call(StringName(method), argptrs,argc,ce);
            if (id.get_type() != VariantType::NIL) {
                ret = make_response(call_ret, id);
            }
        }
    } else if (p_action.get_type() == VariantType::ARRAY && p_process_arr_elements) {
        Array arr = p_action.as<Array>();
        int size = arr.size();
        if (size) {
            Array arr_ret;
            for (int i = 0; i < size; i++) {
                const Variant &var = arr.get(i);
                arr_ret.push_back(process_action(var));
            }
            ret = arr_ret;
        } else {
            ret = make_response_error(JSONRPC::INVALID_REQUEST, ("Invalid Request"));
        }
    } else {
        ret = make_response_error(JSONRPC::INVALID_REQUEST, ("Invalid Request"));
    }
    return ret;
}

String JSONRPC::process_string(const String &p_input) {

    if (p_input.empty()) return null_string;

    Variant ret;
    Variant input;
    String err_message;
    int err_line;
    if (OK != JSON::parse(p_input, input, err_message, err_line)) {
        ret = make_response_error(JSONRPC::PARSE_ERROR, "Parse error");
    } else {
        ret = process_action(input, true);
    }

    if (ret.get_type() == VariantType::NIL) {
        return null_string;
    }
    return JSON::print(ret);
}

void JSONRPC::set_scope(const String &p_scope, Object *p_obj) {
    method_scopes[p_scope] = p_obj;
}
