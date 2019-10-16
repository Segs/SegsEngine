/*************************************************************************/
/*  crypto.cpp                                                           */
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

#include "crypto.h"

#include "core/engine.h"
#include "core/list.h"
#include "core/io/compression.h"
#include "core/method_bind.h"

IMPL_GDCLASS(CryptoKey)
IMPL_GDCLASS(X509Certificate)
IMPL_GDCLASS(Crypto)
IMPL_GDCLASS(ResourceFormatLoaderCrypto)
IMPL_GDCLASS(ResourceFormatSaverCrypto)

/// Resources

CryptoKey *(*CryptoKey::_create)() = nullptr;
CryptoKey *CryptoKey::create() {
    if (_create)
        return _create();
    return nullptr;
}

void CryptoKey::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("save", {"path"}), &CryptoKey::save);
    MethodBinder::bind_method(D_METHOD("load", {"path"}), &CryptoKey::load);
}

X509Certificate *(*X509Certificate::_create)() = nullptr;
X509Certificate *X509Certificate::create() {
    if (_create)
        return _create();
    return nullptr;
}

void X509Certificate::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("save", {"path"}), &X509Certificate::save);
    MethodBinder::bind_method(D_METHOD("load", {"path"}), &X509Certificate::load);
}

/// Crypto

void (*Crypto::_load_default_certificates)(String p_path) = nullptr;
Crypto *(*Crypto::_create)() = nullptr;
Crypto *Crypto::create() {
    if (_create)
        return _create();
    return memnew(Crypto);
}

void Crypto::load_default_certificates(String p_path) {

    if (_load_default_certificates)
        _load_default_certificates(p_path);
}

void Crypto::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("generate_random_bytes", {"size"}), &Crypto::generate_random_bytes);
    MethodBinder::bind_method(D_METHOD("generate_rsa", {"size"}), &Crypto::generate_rsa);
    MethodBinder::bind_method(D_METHOD("generate_self_signed_certificate", {"key", "issuer_name", "not_before", "not_after"}),
            &Crypto::generate_self_signed_certificate,
            { DEFVAL("CN=myserver,O=myorganisation,C=IT"), DEFVAL("20140101000000"), DEFVAL("20340101000000") });
}

PoolByteArray Crypto::generate_random_bytes(int p_bytes) {
    ERR_FAIL_V_MSG(PoolByteArray(), "generate_random_bytes is not available when mbedtls module is disabled.")
}

Ref<CryptoKey> Crypto::generate_rsa(int p_bytes) {
    ERR_FAIL_V_MSG(Ref<CryptoKey>(), "generate_rsa is not available when mbedtls module is disabled.")
}

Ref<X509Certificate> Crypto::generate_self_signed_certificate(Ref<CryptoKey> p_key, String p_issuer_name, String p_not_before, String p_not_after) {
    ERR_FAIL_V_MSG(Ref<X509Certificate>(), "generate_self_signed_certificate is not available when mbedtls module is disabled.")
}

Crypto::Crypto() = default;

/// Resource loader/saver

RES ResourceFormatLoaderCrypto::load(const String &p_path, const String &p_original_path, Error *r_error) {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "crt") {
        X509Certificate *cert = X509Certificate::create();
        if (cert)
            cert->load(p_path);
        return RES(cert);
    } else if (el == "key") {
        CryptoKey *key = CryptoKey::create();
        if (key)
            key->load(p_path);
        return RES(key);
    }
    return RES();
}

void ResourceFormatLoaderCrypto::get_recognized_extensions(ListPOD<String> *p_extensions) const {

    p_extensions->push_back("crt");
    p_extensions->push_back("key");
}

bool ResourceFormatLoaderCrypto::handles_type(const String &p_type) const {

    return p_type == "X509Certificate" || p_type == "CryptoKey";
}

String ResourceFormatLoaderCrypto::get_resource_type(const String &p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "crt")
        return "X509Certificate";
    else if (el == "key")
        return "CryptoKey";
    return "";
}

Error ResourceFormatSaverCrypto::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {

    Error err;
    Ref<X509Certificate> cert = dynamic_ref_cast<X509Certificate>(p_resource);
    Ref<CryptoKey> key = dynamic_ref_cast<CryptoKey>(p_resource);
    if (cert) {
        err = cert->save(p_path);
    } else if (key) {
        err = key->save(p_path);
    } else {
        ERR_FAIL_V(ERR_INVALID_PARAMETER)
    }
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save Crypto resource to file '" + p_path + "'.")
    return OK;
}

void ResourceFormatSaverCrypto::get_recognized_extensions(const RES &p_resource, Vector<String> *p_extensions) const {

    const X509Certificate *cert = object_cast<X509Certificate>(p_resource.get());
    const CryptoKey *key = object_cast<CryptoKey>(p_resource.get());
    if (cert) {
        p_extensions->push_back("crt");
    }
    if (key) {
        p_extensions->push_back("key");
    }
}
bool ResourceFormatSaverCrypto::recognize(const RES &p_resource) const {

    return object_cast<X509Certificate>(p_resource.get()) || object_cast<CryptoKey>(p_resource.get());
}
