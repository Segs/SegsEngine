/*************************************************************************/
/*  crypto.cpp                                                           */
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

#include "crypto.h"

#include "core/engine.h"
#include "core/crypto/hashing_context.h"
#include "core/io/compression.h"
#include "core/list.h"
#include "core/method_bind.h"
#include "core/pool_vector.h"

using namespace eastl;

IMPL_GDCLASS(CryptoKey)
IMPL_GDCLASS(X509Certificate)
IMPL_GDCLASS(Crypto)
IMPL_GDCLASS(HMACContext)

/// Resources

CryptoKey *(*CryptoKey::_create)() = nullptr;
CryptoKey *CryptoKey::create() {
    if (_create) {
        return _create();
    }
    return nullptr;
}

void CryptoKey::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("save", { "path", "public_only" }), &CryptoKey::save,{DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("load", { "path", "public_only" }), &CryptoKey::load,{DEFVAL(false)});
    SE_BIND_METHOD(CryptoKey,is_public_only);
    MethodBinder::bind_method(D_METHOD("save_to_string", {"public_only"}), &CryptoKey::save_to_string, {DEFVAL(false)});
    MethodBinder::bind_method(D_METHOD("load_from_string", {"string_key", "public_only"}), &CryptoKey::load_from_string, {DEFVAL(false)});
}

X509Certificate *(*X509Certificate::_create)() = nullptr;
X509Certificate *X509Certificate::create() {
    if (_create) {
        return _create();
    }
    return nullptr;
}

void X509Certificate::_bind_methods() {
    SE_BIND_METHOD(X509Certificate,save);
    SE_BIND_METHOD(X509Certificate,load);
}

/// HMACContext

void HMACContext::_bind_methods() {
    SE_BIND_METHOD(HMACContext,start);
    SE_BIND_METHOD(HMACContext,update);
    SE_BIND_METHOD(HMACContext,finish);
}

HMACContext *(*HMACContext::_create)() = nullptr;
HMACContext *HMACContext::create() {
    if (_create) {
        return _create();
    }
    ERR_FAIL_V_MSG(nullptr, "HMACContext is not available when the mbedtls module is disabled.");
}

/// Crypto

void (*Crypto::_load_default_certificates)(StringView p_path) = nullptr;
Crypto *(*Crypto::_create)() = nullptr;
Crypto *Crypto::create() {
    if (_create) {
        return _create();
    }
    ERR_FAIL_V_MSG(nullptr, "Crypto is not available when the mbedtls module is disabled.");
}

void Crypto::load_default_certificates(StringView p_path) {
    if (_load_default_certificates) {
        _load_default_certificates(p_path);
    }
}

PoolByteArray Crypto::hmac_digest(HashingContext::HashType p_hash_type, PoolByteArray p_key, PoolByteArray p_msg) {
    Ref<HMACContext> ctx = Ref<HMACContext>(HMACContext::create());
    ERR_FAIL_COND_V_MSG(!ctx, PoolByteArray(), "HMAC is not available without mbedtls module.");
    Error err = ctx->start(p_hash_type, p_key);
    ERR_FAIL_COND_V(err != OK, PoolByteArray());
    err = ctx->update(p_msg);
    ERR_FAIL_COND_V(err != OK, PoolByteArray());
    return ctx->finish();
}

// Compares two HMACS for equality without leaking timing information in order to prevent timing attakcs.
// @see: https://paragonie.com/blog/2015/11/preventing-timing-attacks-on-string-comparison-with-double-hmac-strategy
bool Crypto::constant_time_compare(PoolByteArray p_trusted, PoolByteArray p_received) {
    const uint8_t *t = p_trusted.read().ptr();
    const uint8_t *r = p_received.read().ptr();
    int tlen = p_trusted.size();
    int rlen = p_received.size();
    // If the lengths are different then nothing else matters.
    if (tlen != rlen) {
        return false;
    }

    uint8_t v = 0;
    for (int i = 0; i < tlen; i++) {
        v |= t[i] ^ r[i];
    }
    return v == 0;
}

void Crypto::_bind_methods() {
    SE_BIND_METHOD(Crypto,generate_random_bytes);
    SE_BIND_METHOD(Crypto,generate_rsa);
    MethodBinder::bind_method(
            D_METHOD("generate_self_signed_certificate", { "key", "issuer_name", "not_before", "not_after" }),
            &Crypto::generate_self_signed_certificate,
            { Variant("CN=myserver,O=myorganisation,C=IT"), Variant("20140101000000"), Variant("20340101000000") });
    SE_BIND_METHOD(Crypto,sign);
    SE_BIND_METHOD(Crypto,verify);
    SE_BIND_METHOD(Crypto,encrypt);
    SE_BIND_METHOD(Crypto,decrypt);
    SE_BIND_METHOD(Crypto,hmac_digest);
    SE_BIND_METHOD(Crypto,constant_time_compare);
}

PoolByteArray Crypto::generate_random_bytes(int p_bytes) {
    ERR_FAIL_V_MSG(PoolByteArray(), "generate_random_bytes is not available when mbedtls module is disabled.");
}

Ref<CryptoKey> Crypto::generate_rsa(int p_bytes) {
    ERR_FAIL_V_MSG(Ref<CryptoKey>(), "generate_rsa is not available when mbedtls module is disabled.");
}

Ref<X509Certificate> Crypto::generate_self_signed_certificate(
        Ref<CryptoKey> p_key, StringView p_issuer_name, StringView p_not_before, StringView p_not_after) {
    ERR_FAIL_V_MSG(Ref<X509Certificate>(),
            "generate_self_signed_certificate is not available when mbedtls module is disabled.");
}

Crypto::Crypto() = default;

/// Resource loader/saver

RES ResourceFormatLoaderCrypto::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {
    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "crt") {
        X509Certificate *cert = X509Certificate::create();
        if (cert) {
            cert->load(p_path);
        }
        return RES(cert);
    } else if (el == "key") {
        CryptoKey *key = CryptoKey::create();
        if (key) {
            key->load(p_path, false);
        }
        return RES(key);
    } else if (el == "pub") {
        CryptoKey *key = CryptoKey::create();
        if (key) {
            key->load(p_path, true);
        }
        return RES(key);
    }
    return RES();
}

void ResourceFormatLoaderCrypto::get_recognized_extensions(Vector<String> &p_extensions) const {
    p_extensions.push_back("crt");
    p_extensions.push_back("key");
    p_extensions.push_back("pub");
}

bool ResourceFormatLoaderCrypto::handles_type(StringView p_type) const {
    return p_type == "X509Certificate"_sv || p_type == "CryptoKey"_sv;
}

String ResourceFormatLoaderCrypto::get_resource_type(StringView p_path) const {
    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "crt") {
        return "X509Certificate";
    } else if (el == "key" || el == "pub") {
        return "CryptoKey";
    }
    return String();
}

Error ResourceFormatSaverCrypto::save(StringView p_path, const RES &p_resource, uint32_t p_flags) {
    Error err;
    Ref<X509Certificate> cert = dynamic_ref_cast<X509Certificate>(p_resource);
    Ref<CryptoKey> key = dynamic_ref_cast<CryptoKey>(p_resource);
    if (cert) {
        err = cert->save(p_path);
    } else if (key) {
        err = key->save(p_path,PathUtils::get_extension(p_path)=="pub");
    } else {
        ERR_FAIL_V(ERR_INVALID_PARAMETER);
    }
    ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot save Crypto resource to file '" + String(p_path) + "'.");
    return OK;
}

void ResourceFormatSaverCrypto::get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const {
    const X509Certificate *cert = object_cast<X509Certificate>(p_resource.get());
    const CryptoKey *key = object_cast<CryptoKey>(p_resource.get());
    if (cert) {
        p_extensions.push_back("crt");
    }
    if (key) {
        if (!key->is_public_only()) {
        p_extensions.push_back("key");
        }
        p_extensions.push_back("pub");
    }
}

bool ResourceFormatSaverCrypto::recognize(const RES &p_resource) const {
    return object_cast<X509Certificate>(p_resource.get()) || object_cast<CryptoKey>(p_resource.get());
}
