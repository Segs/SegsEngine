/*************************************************************************/
/*  crypto.h                                                             */
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

#include "core/reference.h"
#include "core/resource.h"

#include "core/io/resource_format_loader.h"
#include "core/io/resource_saver.h"

class CryptoKey : public Resource {
    GDCLASS(CryptoKey, Resource)

protected:
    static void _bind_methods();
    static CryptoKey *(*_create)();

public:
    static CryptoKey *create();
    virtual Error load(StringView p_path) = 0;
    virtual Error save(StringView p_path) = 0;
};

class X509Certificate : public Resource {
    GDCLASS(X509Certificate, Resource)

protected:
    static void _bind_methods();
    static X509Certificate *(*_create)();

public:
    static X509Certificate *create();
    virtual Error load(StringView p_path) = 0;
    virtual Error load_from_memory(const uint8_t *p_buffer, int p_len) = 0;
    virtual Error save(StringView p_path) = 0;
};

class Crypto : public RefCounted {
    GDCLASS(Crypto, RefCounted)

protected:
    static void _bind_methods();
    static Crypto *(*_create)();
    static void (*_load_default_certificates)(StringView p_path);

public:
    static Crypto *create();
    static void load_default_certificates(StringView p_path);

    virtual PoolByteArray generate_random_bytes(int p_bytes);
    virtual Ref<CryptoKey> generate_rsa(int p_bytes);
    virtual Ref<X509Certificate> generate_self_signed_certificate(Ref<CryptoKey> p_key, StringView p_issuer_name, StringView p_not_before, StringView p_not_after);

    Crypto();
};

class ResourceFormatLoaderCrypto : public ResourceFormatLoader {
    GDCLASS(ResourceFormatLoaderCrypto, ResourceFormatLoader)

public:
    RES load(StringView p_path, StringView p_original_path = StringView (), Error *r_error = nullptr) override;
    void get_recognized_extensions(Vector<String> &p_extensions) const override;
    bool handles_type(StringView p_type) const override;
    String get_resource_type(StringView p_path) const override;
};

class ResourceFormatSaverCrypto : public ResourceFormatSaver {
    GDCLASS(ResourceFormatSaverCrypto, ResourceFormatSaver)

public:
    Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0) override;
    void get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const override;
    bool recognize(const RES &p_resource) const override;
};
