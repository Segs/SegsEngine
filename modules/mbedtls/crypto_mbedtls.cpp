/*************************************************************************/
/*  crypto_mbedtls.cpp                                                   */
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

#include "crypto_mbedtls.h"

#include "core/os/file_access.h"

#include "core/engine.h"
#include "core/print_string.h"
#include "core/io/compression.h"
#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/pool_vector.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif
#define PEM_BEGIN_CRT "-----BEGIN CERTIFICATE-----\n"
#define PEM_END_CRT "-----END CERTIFICATE-----\n"

#include <mbedtls/debug.h>
#include <mbedtls/md.h>
#include <mbedtls/pem.h>

#include <QFile>

CryptoKey *CryptoKeyMbedTLS::create() {
    return memnew(CryptoKeyMbedTLS);
}

Error CryptoKeyMbedTLS::load(StringView p_path, bool p_public_only) {
    ERR_FAIL_COND_V_MSG(locks, ERR_ALREADY_IN_USE, "Key is in use");

    PoolByteArray out;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, ERR_INVALID_PARAMETER, "Cannot open CryptoKeyMbedTLS file '" + String(p_path) + "'.");

    uint64_t flen = f->get_len();
    out.resize(flen + 1);
    {
        PoolByteArray::Write w = out.write();
        f->get_buffer(w.ptr(), flen);
        w[flen] = 0; //end f string
    }
    memdelete(f);

    int ret = 0;
    if (p_public_only) {
        ret = mbedtls_pk_parse_public_key(&pkey, out.read().ptr(), out.size());
    } else {
        ret = mbedtls_pk_parse_key(&pkey, out.read().ptr(), out.size(), nullptr, 0);
    }
    // We MUST zeroize the memory for safety!
    mbedtls_platform_zeroize(out.write().ptr(), out.size());
    ERR_FAIL_COND_V_MSG(ret, FAILED, "Error parsing key '" + itos(ret) + "'.");

    public_only = p_public_only;
    return OK;
}

Error CryptoKeyMbedTLS::save(StringView p_path, bool p_public_only) {
    FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!f, ERR_INVALID_PARAMETER, "Cannot save CryptoKeyMbedTLS file '" + String(p_path) + "'.");

    unsigned char w[16000];
    memset(w, 0, sizeof(w));

    int ret = 0;
    if (p_public_only) {
        ret = mbedtls_pk_write_pubkey_pem(&pkey, w, sizeof(w));
    } else {
        ret = mbedtls_pk_write_key_pem(&pkey, w, sizeof(w));
    }
    if (ret != 0) {
        memdelete(f);
        mbedtls_platform_zeroize(w, sizeof(w)); // Zeroize anything we might have written.
        ERR_FAIL_V_MSG(FAILED, "Error writing key '" + itos(ret) + "'.");
    }

    size_t len = strlen((char *)w);
    f->store_buffer(w, len);
    memdelete(f);
    mbedtls_platform_zeroize(w, sizeof(w)); // Zeroize temporary buffer.
    return OK;
}

Error CryptoKeyMbedTLS::load_from_string(StringView p_string_key, bool p_public_only) {
    int ret = 0;
    if (p_public_only) {
        ret = mbedtls_pk_parse_public_key(&pkey, (unsigned char *)p_string_key.data(), p_string_key.size());
    } else {
        ret = mbedtls_pk_parse_key(&pkey, (unsigned char *)p_string_key.data(), p_string_key.size(), nullptr, 0);
    }
    ERR_FAIL_COND_V_MSG(ret, FAILED, "Error parsing key '" + itos(ret) + "'.");

    public_only = p_public_only;
    return OK;
}

String CryptoKeyMbedTLS::save_to_string(bool p_public_only) {
    unsigned char w[16000];
    memset(w, 0, sizeof(w));

    int ret = 0;
    if (p_public_only) {
        ret = mbedtls_pk_write_pubkey_pem(&pkey, w, sizeof(w));
    } else {
        ret = mbedtls_pk_write_key_pem(&pkey, w, sizeof(w));
    }
    if (ret != 0) {
        mbedtls_platform_zeroize(w, sizeof(w));
        ERR_FAIL_V_MSG("", "Error saving key '" + itos(ret) + "'.");
    }
    String s = String((char *)w);
    return s;
}

X509Certificate *X509CertificateMbedTLS::create() {
    return memnew(X509CertificateMbedTLS);
}

Error X509CertificateMbedTLS::load(StringView p_path) {
    ERR_FAIL_COND_V_MSG(locks, ERR_ALREADY_IN_USE, "Certificate is in use");

    PoolByteArray out;
    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, ERR_INVALID_PARAMETER, "Cannot open X509CertificateMbedTLS file '" + String(p_path) + "'.");

    uint64_t flen = f->get_len();
    out.resize(flen + 1);
    {
        PoolByteArray::Write w = out.write();
        f->get_buffer(w.ptr(), flen);
        w[flen] = 0; //end f string
    }
    memdelete(f);

    int ret = mbedtls_x509_crt_parse(&cert, out.read().ptr(), out.size());
    ERR_FAIL_COND_V_MSG(ret, FAILED, "Error parsing some certificates: " + itos(ret));

    return OK;
}

Error X509CertificateMbedTLS::load_from_memory(const uint8_t *p_buffer, int p_len) {
    ERR_FAIL_COND_V_MSG(locks, ERR_ALREADY_IN_USE, "Certificate is in use");

    int ret = mbedtls_x509_crt_parse(&cert, p_buffer, p_len);
    ERR_FAIL_COND_V_MSG(ret, FAILED, "Error parsing certificates: " + itos(ret));
    return OK;
}

Error X509CertificateMbedTLS::save(StringView p_path) {
    FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!f, ERR_INVALID_PARAMETER, "Cannot save X509CertificateMbedTLS file '" + String(p_path) + "'.");

    mbedtls_x509_crt *crt = &cert;
    while (crt) {
        unsigned char w[4096];
        size_t wrote = 0;
        int ret = mbedtls_pem_write_buffer(PEM_BEGIN_CRT, PEM_END_CRT, cert.raw.p, cert.raw.len, w, sizeof(w), &wrote);
        if (ret != 0 || wrote == 0) {
            memdelete(f);
            ERR_FAIL_V_MSG(FAILED, "Error writing certificate '" + itos(ret) + "'.");
        }

        f->store_buffer(w, wrote - 1); // don't write the string terminator
        crt = crt->next;
    }
    memdelete(f);
    return OK;
}

bool HMACContextMbedTLS::is_md_type_allowed(mbedtls_md_type_t p_md_type) {
    switch (p_md_type) {
        case MBEDTLS_MD_SHA1:
        case MBEDTLS_MD_SHA256:
            return true;
        default:
            return false;
    }
}

HMACContext *HMACContextMbedTLS::create() {
    return memnew(HMACContextMbedTLS);
}

Error HMACContextMbedTLS::start(HashingContext::HashType p_hash_type, PoolByteArray p_key) {
    ERR_FAIL_COND_V_MSG(ctx != nullptr, ERR_FILE_ALREADY_IN_USE, "HMACContext already started.");

    // HMAC keys can be any size.
    ERR_FAIL_COND_V_MSG(p_key.empty(), ERR_INVALID_PARAMETER, "Key must not be empty.");

    hash_type = p_hash_type;
    mbedtls_md_type_t ht = CryptoMbedTLS::md_type_from_hashtype(p_hash_type, hash_len);

    bool allowed = HMACContextMbedTLS::is_md_type_allowed(ht);
    ERR_FAIL_COND_V_MSG(!allowed, ERR_INVALID_PARAMETER, "Unsupported hash type.");

    ctx = memalloc(sizeof(mbedtls_md_context_t));
    mbedtls_md_init((mbedtls_md_context_t *)ctx);

    mbedtls_md_setup((mbedtls_md_context_t *)ctx, mbedtls_md_info_from_type((mbedtls_md_type_t)ht), 1);
    int ret = mbedtls_md_hmac_starts((mbedtls_md_context_t *)ctx, (const uint8_t *)p_key.read().ptr(), (size_t)p_key.size());
    return ret ? FAILED : OK;
}

Error HMACContextMbedTLS::update(PoolByteArray p_data) {
    ERR_FAIL_COND_V_MSG(ctx == nullptr, ERR_INVALID_DATA, "Start must be called before update.");

    ERR_FAIL_COND_V_MSG(p_data.empty(), ERR_INVALID_PARAMETER, "Src must not be empty.");

    int ret = mbedtls_md_hmac_update((mbedtls_md_context_t *)ctx, (const uint8_t *)p_data.read().ptr(), (size_t)p_data.size());
    return ret ? FAILED : OK;
}

PoolByteArray HMACContextMbedTLS::finish() {
    ERR_FAIL_COND_V_MSG(ctx == nullptr, PoolByteArray(), "Start must be called before finish.");
    ERR_FAIL_COND_V_MSG(hash_len == 0, PoolByteArray(), "Unsupported hash type.");

    PoolByteArray out;
    out.resize(hash_len);

    unsigned char *out_ptr = (unsigned char *)out.write().ptr();
    int ret = mbedtls_md_hmac_finish((mbedtls_md_context_t *)ctx, out_ptr);

    mbedtls_md_free((mbedtls_md_context_t *)ctx);
    memfree((mbedtls_md_context_t *)ctx);
    ctx = nullptr;
    hash_len = 0;

    ERR_FAIL_COND_V_MSG(ret, PoolByteArray(), "Error received while finishing HMAC");
    return out;
}

HMACContextMbedTLS::~HMACContextMbedTLS() {
    if (ctx != nullptr) {
        mbedtls_md_free((mbedtls_md_context_t *)ctx);
        memfree((mbedtls_md_context_t *)ctx);
    }
}

Crypto *CryptoMbedTLS::create() {
    return memnew(CryptoMbedTLS);
}

void CryptoMbedTLS::initialize_crypto() {

#ifdef DEBUG_ENABLED
    mbedtls_debug_set_threshold(1);
#endif

    Crypto::_create = create;
    Crypto::_load_default_certificates = load_default_certificates;
    X509CertificateMbedTLS::make_default();
    CryptoKeyMbedTLS::make_default();
    HMACContextMbedTLS::make_default();
}

void CryptoMbedTLS::finalize_crypto() {
    Crypto::_create = nullptr;
    Crypto::_load_default_certificates = nullptr;
    memdelete(default_certs);
    default_certs = nullptr;
    X509CertificateMbedTLS::finalize();
    CryptoKeyMbedTLS::finalize();
    HMACContextMbedTLS::finalize();
}

CryptoMbedTLS::CryptoMbedTLS() {
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
    if (ret != 0) {
        ERR_PRINT(" failed\n  ! mbedtls_ctr_drbg_seed returned an error" + itos(ret));
    }
}

CryptoMbedTLS::~CryptoMbedTLS() {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}

X509CertificateMbedTLS *CryptoMbedTLS::default_certs = nullptr;

X509CertificateMbedTLS *CryptoMbedTLS::get_default_certificates() {
    return default_certs;
}

void CryptoMbedTLS::load_default_certificates(StringView p_path) {
    ERR_FAIL_COND(default_certs != nullptr);

    default_certs = memnew(X509CertificateMbedTLS);
    ERR_FAIL_COND(default_certs == nullptr);

    if (!p_path.empty()) {
        // Use certs defined in project settings.
        default_certs->load(p_path);
    }
#if 1
    else {
        // Use builtin certs only if user did not override it in project settings.
        PoolByteArray out;
        QFile fl(":/binary/ca-certificates.crt");
        fl.open(QFile::ReadOnly);
        QByteArray contents=fl.readAll();
        out.resize(contents.size() + 1);
        {
            PoolByteArray::Write w = out.write();
            memcpy(w.ptr(),contents.data(),contents.size());
            w[contents.size()] = 0; // Make sure it ends with string terminator
        }
#ifdef DEBUG_ENABLED
        print_verbose("Loaded builtin certs");
#endif
        default_certs->load_from_memory(out.read().ptr(), out.size());
    }
#endif
}

Ref<CryptoKey> CryptoMbedTLS::generate_rsa(int p_bytes) {
    Ref<CryptoKeyMbedTLS> out(make_ref_counted<CryptoKeyMbedTLS>());
    int ret = mbedtls_pk_setup(&(out->pkey), mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    ERR_FAIL_COND_V(ret != 0, Ref<CryptoKey>());
    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(out->pkey), mbedtls_ctr_drbg_random, &ctr_drbg, p_bytes, 65537);
    out->public_only = false;
    ERR_FAIL_COND_V(ret != 0, Ref<CryptoKey>());
    return out;
}

Ref<X509Certificate> CryptoMbedTLS::generate_self_signed_certificate(Ref<CryptoKey> p_key, StringView p_issuer_name, StringView p_not_before, StringView p_not_after) {
    Ref<CryptoKeyMbedTLS> key = dynamic_ref_cast<CryptoKeyMbedTLS>(p_key);
    ERR_FAIL_COND_V_MSG(not key, Ref<X509Certificate>(), "Invalid private key argument.");

    assert(key);

    mbedtls_x509write_cert crt;
    mbedtls_x509write_crt_init(&crt);

    mbedtls_x509write_crt_set_subject_key(&crt, &(key->pkey));
    mbedtls_x509write_crt_set_issuer_key(&crt, &(key->pkey));
    mbedtls_x509write_crt_set_subject_name(&crt, String(p_issuer_name).c_str());
    mbedtls_x509write_crt_set_issuer_name(&crt, String(p_issuer_name).c_str());
    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

    mbedtls_mpi serial;
    mbedtls_mpi_init(&serial);
    uint8_t rand_serial[20];
    mbedtls_ctr_drbg_random(&ctr_drbg, rand_serial, 20);
    ERR_FAIL_COND_V(mbedtls_mpi_read_binary(&serial, rand_serial, 20), Ref<X509Certificate>());
    mbedtls_x509write_crt_set_serial(&crt, &serial);

    mbedtls_x509write_crt_set_validity(&crt, String(p_not_before).data(), String(p_not_after).data());
    mbedtls_x509write_crt_set_basic_constraints(&crt, 1, -1);
    mbedtls_x509write_crt_set_basic_constraints(&crt, 1, 0);

    unsigned char buf[4096];
    memset(buf, 0, 4096);
    int ret = mbedtls_x509write_crt_pem(&crt, buf, 4096, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_mpi_free(&serial);
    mbedtls_x509write_crt_free(&crt);
    ERR_FAIL_COND_V_MSG(ret != 0, {}, "Failed to generate certificate: " + itos(ret));
    buf[4095] = '\0'; // Make sure strlen can't fail.

    Ref<X509CertificateMbedTLS> out(make_ref_counted<X509CertificateMbedTLS>());
    out->load_from_memory(buf, strlen((char *)buf) + 1); // Use strlen to find correct output size.
    return out;
}

PoolByteArray CryptoMbedTLS::generate_random_bytes(int p_bytes) {
    PoolByteArray out;
    out.resize(p_bytes);
    mbedtls_ctr_drbg_random(&ctr_drbg, out.write().ptr(), p_bytes);
    return out;
}

mbedtls_md_type_t CryptoMbedTLS::md_type_from_hashtype(HashingContext::HashType p_hash_type, int &r_size) {
    switch (p_hash_type) {
        case HashingContext::HASH_MD5:
            r_size = 16;
            return MBEDTLS_MD_MD5;
        case HashingContext::HASH_SHA1:
            r_size = 20;
            return MBEDTLS_MD_SHA1;
        case HashingContext::HASH_SHA256:
            r_size = 32;
            return MBEDTLS_MD_SHA256;
        default:
            r_size = 0;
            ERR_FAIL_V_MSG(MBEDTLS_MD_NONE, "Invalid hash type.");
    }
}

Vector<uint8_t> CryptoMbedTLS::sign(HashingContext::HashType p_hash_type, Vector<uint8_t> p_hash, const Ref<CryptoKey> &p_key) {
    int size;
    mbedtls_md_type_t type = CryptoMbedTLS::md_type_from_hashtype(p_hash_type, size);
    ERR_FAIL_COND_V_MSG(type == MBEDTLS_MD_NONE, Vector<uint8_t>(), "Invalid hash type.");
    ERR_FAIL_COND_V_MSG(p_hash.size() != size, Vector<uint8_t>(), "Invalid hash provided. Size must be " + itos(size));
    Ref<CryptoKeyMbedTLS> key = static_ref_cast<CryptoKeyMbedTLS>(p_key);
    ERR_FAIL_COND_V_MSG(!key, Vector<uint8_t>(), "Invalid key provided.");
    ERR_FAIL_COND_V_MSG(key->is_public_only(), Vector<uint8_t>(), "Invalid key provided. Cannot sign with public_only keys.");
    size_t sig_size = 0;
    unsigned char buf[MBEDTLS_MPI_MAX_SIZE];
    Vector<uint8_t> out;
    int ret = mbedtls_pk_sign(&(key->pkey), type, p_hash.data(), size, buf, &sig_size, mbedtls_ctr_drbg_random, &ctr_drbg);
    ERR_FAIL_COND_V_MSG(ret, out, "Error while signing: " + itos(ret));
    out.resize(sig_size);
    memcpy(out.data(), buf, sig_size);
    return out;
}

bool CryptoMbedTLS::verify(HashingContext::HashType p_hash_type, Vector<uint8_t> p_hash, Vector<uint8_t> p_signature, const Ref<CryptoKey> &p_key) {
    int size;
    mbedtls_md_type_t type = CryptoMbedTLS::md_type_from_hashtype(p_hash_type, size);
    ERR_FAIL_COND_V_MSG(type == MBEDTLS_MD_NONE, false, "Invalid hash type.");
    ERR_FAIL_COND_V_MSG(p_hash.size() != size, false, "Invalid hash provided. Size must be " + itos(size));
    Ref<CryptoKeyMbedTLS> key = static_ref_cast<CryptoKeyMbedTLS>(p_key);
    ERR_FAIL_COND_V_MSG(!key, false, "Invalid key provided.");
    return mbedtls_pk_verify(&(key->pkey), type, p_hash.data(), size, p_signature.data(), p_signature.size()) == 0;
}

Vector<uint8_t> CryptoMbedTLS::encrypt(const Ref<CryptoKey> &p_key, Vector<uint8_t> p_plaintext) {
    Ref<CryptoKeyMbedTLS> key = static_ref_cast<CryptoKeyMbedTLS>(p_key);
    ERR_FAIL_COND_V_MSG(!key, Vector<uint8_t>(), "Invalid key provided.");
    uint8_t buf[1024];
    size_t size;
    Vector<uint8_t> out;
    int ret = mbedtls_pk_encrypt(&(key->pkey), p_plaintext.data(), p_plaintext.size(), buf, &size, sizeof(buf), mbedtls_ctr_drbg_random, &ctr_drbg);
    ERR_FAIL_COND_V_MSG(ret, out, "Error while encrypting: " + itos(ret));
    out.resize(size);
    memcpy(out.data(), buf, size);
    return out;
}

Vector<uint8_t> CryptoMbedTLS::decrypt(const Ref<CryptoKey> &p_key, Vector<uint8_t> p_ciphertext) {
    Ref<CryptoKeyMbedTLS> key = static_ref_cast<CryptoKeyMbedTLS>(p_key);
    ERR_FAIL_COND_V_MSG(!key, Vector<uint8_t>(), "Invalid key provided.");
    ERR_FAIL_COND_V_MSG(key->is_public_only(), Vector<uint8_t>(), "Invalid key provided. Cannot decrypt using a public_only key.");
    uint8_t buf[2048];
    size_t size;
    Vector<uint8_t> out;
    int ret = mbedtls_pk_decrypt(&(key->pkey), p_ciphertext.data(), p_ciphertext.size(), buf, &size, sizeof(buf), mbedtls_ctr_drbg_random, &ctr_drbg);
    ERR_FAIL_COND_V_MSG(ret, out, "Error while decrypting: " + itos(ret));
    out.resize(size);
    memcpy(out.data(), buf, size);
    return out;
}
