/*************************************************************************/
/*  crypto_mbedtls.h                                                     */
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

#include "core/crypto/crypto.h"
#include "core/resource.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>

class CryptoMbedTLS;
class SSLContextMbedTLS;
class CryptoKeyMbedTLS : public CryptoKey {

private:
	mbedtls_pk_context pkey;
	int locks;

public:
	static CryptoKey *create();
	static void make_default() { CryptoKey::_create = create; }
	static void finalize() { CryptoKey::_create = nullptr; }

    Error load(se_string_view p_path) override;
    Error save(se_string_view p_path) override;

	CryptoKeyMbedTLS() {
		mbedtls_pk_init(&pkey);
		locks = 0;
	}
	~CryptoKeyMbedTLS() override {
		mbedtls_pk_free(&pkey);
	}

	_FORCE_INLINE_ void lock() { locks++; }
	_FORCE_INLINE_ void unlock() { locks--; }

	friend class CryptoMbedTLS;
	friend class SSLContextMbedTLS;
};

class X509CertificateMbedTLS : public X509Certificate {

private:
	mbedtls_x509_crt cert;
	int locks;

public:
	static X509Certificate *create();
	static void make_default() { X509Certificate::_create = create; }
	static void finalize() { X509Certificate::_create = nullptr; }

    Error load(se_string_view p_path) override;
	Error load_from_memory(const uint8_t *p_buffer, int p_len) override;
    Error save(se_string_view p_path) override;

	X509CertificateMbedTLS() {
		mbedtls_x509_crt_init(&cert);
		locks = 0;
	}
	~X509CertificateMbedTLS() override {
		mbedtls_x509_crt_free(&cert);
	}

	_FORCE_INLINE_ void lock() { locks++; }
	_FORCE_INLINE_ void unlock() { locks--; }

	friend class CryptoMbedTLS;
	friend class SSLContextMbedTLS;
};

class CryptoMbedTLS : public Crypto {

private:
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	static X509CertificateMbedTLS *default_certs;

public:
	static Crypto *create();
	static void initialize_crypto();
	static void finalize_crypto();
	static X509CertificateMbedTLS *get_default_certificates();
    static void load_default_certificates(se_string_view p_path);

	PoolByteArray generate_random_bytes(int p_bytes) override;
	Ref<CryptoKey> generate_rsa(int p_bytes) override;
    Ref<X509Certificate> generate_self_signed_certificate(Ref<CryptoKey> p_key, se_string_view p_issuer_name, se_string_view p_not_before, se_string_view p_not_after) override;

	CryptoMbedTLS();
	~CryptoMbedTLS() override;
};
