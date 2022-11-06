/*************************************************************************/
/*  stream_peer.cpp                                                      */
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

#include "stream_peer.h"

#include "core/io/marshalls.h"
#include "core/method_bind.h"
#include "core/string_utils.inl"
#include "EASTL/unique_ptr.h"

IMPL_GDCLASS(StreamPeer)
IMPL_GDCLASS(StreamPeerBuffer)

Error StreamPeer::_put_data(Span<const uint8_t> p_data) {

    int len = p_data.size();
    if (len == 0)
        return OK;

    return put_data(p_data.data(), p_data.size());
}

Array StreamPeer::_put_partial_data(Span<const uint8_t> p_data) {

    Array ret;

    size_t len = p_data.size();
    if (len == 0) {
        ret.push_back(OK);
        ret.push_back(0);
        return ret;
    }

    int sent;
    Error err = put_partial_data(p_data.data(), len, sent);

    if (err != OK) {
        sent = 0;
    }
    ret.push_back(err);
    ret.push_back(sent);
    return ret;
}

Array StreamPeer::_get_data(int p_bytes) {

    Array ret;

    if (p_bytes>= (1U<<25)) { // if request is for more than 512MB something went wrong somewhere

        ret.push_back(ERR_OUT_OF_MEMORY);
        ret.push_back(PoolVector<uint8_t>());
        return ret;
    }
    auto holder = Vector<uint8_t>(p_bytes);

    Error err = get_data(holder.data(), p_bytes);
    ret.emplace_back(err);
    ret.emplace_back(eastl::move(holder));
    return ret;
}

Array StreamPeer::_get_partial_data(int p_bytes) {

    Array ret;

    PoolVector<uint8_t> data;
    data.resize(p_bytes);
    if (data.size() != p_bytes) {

        ret.push_back(ERR_OUT_OF_MEMORY);
        ret.push_back(PoolVector<uint8_t>());
        return ret;
    }

    PoolVector<uint8_t>::Write w = data.write();
    int received;
    Error err = get_partial_data(&w[0], p_bytes, received);
    w.release();

    if (err != OK) {
        data.resize(0);
    } else if (received != data.size()) {

        data.resize(received);
    }

    ret.push_back(err);
    ret.push_back(data);
    return ret;
}

void StreamPeer::put_u8(uint8_t p_val) {
    put_data((const uint8_t *)&p_val, 1);
}

void StreamPeer::put_8(int8_t p_val) {

    put_data((const uint8_t *)&p_val, 1);
}
void StreamPeer::put_u16(uint16_t p_val) {

    uint8_t buf[2];
    encode_uint16(p_val, buf);
    put_data(buf, 2);
}
void StreamPeer::put_16(int16_t p_val) {

    uint8_t buf[2];
    encode_uint16(p_val, buf);
    put_data(buf, 2);
}
void StreamPeer::put_u32(uint32_t p_val) {

    uint8_t buf[4];
    encode_uint32(p_val, buf);
    put_data(buf, 4);
}
void StreamPeer::put_32(int32_t p_val) {

    uint8_t buf[4];
    encode_uint32(p_val, buf);
    put_data(buf, 4);
}
void StreamPeer::put_u64(uint64_t p_val) {

    uint8_t buf[8];
    encode_uint64(p_val, buf);
    put_data(buf, 8);
}
void StreamPeer::put_64(int64_t p_val) {

    uint8_t buf[8];
    encode_uint64(p_val, buf);
    put_data(buf, 8);
}
void StreamPeer::put_float(float p_val) {

    uint8_t buf[4];

    encode_float(p_val, buf);

    put_data(buf, 4);
}
void StreamPeer::put_double(double p_val) {

    uint8_t buf[8];
    encode_double(p_val, buf);
    put_data(buf, 8);
}
void StreamPeer::put_string(StringView p_string) {

    put_u32(p_string.length());
    put_data((const uint8_t *)p_string.data(), p_string.length());
}
void StreamPeer::put_utf8_string(StringView p_string) {

    put_u32(p_string.length());
    put_data((const uint8_t *)p_string.data(), p_string.length());
}
void StreamPeer::put_var(const Variant &p_variant, bool p_full_objects) {

    int len = 0;
    Vector<uint8_t> buf;
    encode_variant(p_variant, nullptr, len, p_full_objects);
    buf.resize(len);
    put_32(len);
    encode_variant(p_variant, buf.data(), len, p_full_objects);
    put_data(buf.data(), buf.size());
}

uint8_t StreamPeer::get_u8() {

    uint8_t buf[1];
    get_data(buf, 1);
    return buf[0];
}
int8_t StreamPeer::get_8() {

    uint8_t buf[1];
    get_data(buf, 1);
    return buf[0];
}
uint16_t StreamPeer::get_u16() {

    uint8_t buf[2];
    get_data(buf, 2);
    uint16_t r = decode_uint16(buf);
    return r;
}
int16_t StreamPeer::get_16() {

    uint8_t buf[2];
    get_data(buf, 2);
    uint16_t r = decode_uint16(buf);
    return r;
}
uint32_t StreamPeer::get_u32() {

    uint8_t buf[4];
    get_data(buf, 4);
    uint32_t r = decode_uint32(buf);
    return r;
}
int32_t StreamPeer::get_32() {

    uint8_t buf[4];
    get_data(buf, 4);
    uint32_t r = decode_uint32(buf);
    return r;
}
uint64_t StreamPeer::get_u64() {

    uint8_t buf[8];
    get_data(buf, 8);
    uint64_t r = decode_uint64(buf);
    return r;
}
int64_t StreamPeer::get_64() {

    uint8_t buf[8];
    get_data(buf, 8);
    uint64_t r = decode_uint64(buf);
    return r;
}
float StreamPeer::get_float() {

    uint8_t buf[4];
    get_data(buf, 4);


    return decode_float(buf);
}

double StreamPeer::get_double() {

    uint8_t buf[8];
    get_data(buf, 8);


    return decode_double(buf);
}
String StreamPeer::get_string(int p_bytes) {

    if (p_bytes < 0)
        p_bytes = get_u32();
    ERR_FAIL_COND_V(p_bytes < 0, String());

    String buf;
    buf.resize(p_bytes + 1);
    Error err = get_data((uint8_t *)&buf[0], p_bytes);
    ERR_FAIL_COND_V(err != OK, String());
    buf[p_bytes] = 0;
    return buf;
}
Variant StreamPeer::get_var(bool p_allow_objects) {

    int len = get_32();
    ERR_FAIL_COND_V(len >= 512*1024*1024, Variant());
    Vector<uint8_t> var;
    var.resize(len);
    Error err = get_data(var.data(), len);
    ERR_FAIL_COND_V(err != OK, Variant());

    Variant ret;
    err = decode_variant(ret, var.data(), len, nullptr, p_allow_objects);
    ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to decode Variant.");

    return ret;
}

void StreamPeer::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("put_data", {"data"}), &StreamPeer::_put_data);
    MethodBinder::bind_method(D_METHOD("put_partial_data", {"data"}), &StreamPeer::_put_partial_data);

    MethodBinder::bind_method(D_METHOD("get_data", {"bytes"}), &StreamPeer::_get_data);
    MethodBinder::bind_method(D_METHOD("get_partial_data", {"bytes"}), &StreamPeer::_get_partial_data);

    SE_BIND_METHOD(StreamPeer,get_available_bytes);

    SE_BIND_METHOD(StreamPeer,put_8);
    SE_BIND_METHOD(StreamPeer,put_u8);
    SE_BIND_METHOD(StreamPeer,put_16);
    SE_BIND_METHOD(StreamPeer,put_u16);
    SE_BIND_METHOD(StreamPeer,put_32);
    SE_BIND_METHOD(StreamPeer,put_u32);
    SE_BIND_METHOD(StreamPeer,put_64);
    SE_BIND_METHOD(StreamPeer,put_u64);
    SE_BIND_METHOD(StreamPeer,put_float);
    SE_BIND_METHOD(StreamPeer,put_double);
    SE_BIND_METHOD(StreamPeer,put_string);
    SE_BIND_METHOD(StreamPeer,put_utf8_string);
    MethodBinder::bind_method(D_METHOD("put_var", {"value", "full_objects"}), &StreamPeer::put_var, {DEFVAL(false)});

    SE_BIND_METHOD(StreamPeer,get_8);
    SE_BIND_METHOD(StreamPeer,get_u8);
    SE_BIND_METHOD(StreamPeer,get_16);
    SE_BIND_METHOD(StreamPeer,get_u16);
    SE_BIND_METHOD(StreamPeer,get_32);
    SE_BIND_METHOD(StreamPeer,get_u32);
    SE_BIND_METHOD(StreamPeer,get_64);
    SE_BIND_METHOD(StreamPeer,get_u64);
    SE_BIND_METHOD(StreamPeer,get_float);
    SE_BIND_METHOD(StreamPeer,get_double);
    MethodBinder::bind_method(D_METHOD("get_string", {"bytes"}), &StreamPeer::get_string, {DEFVAL(-1)});
    //MethodBinder::bind_method(D_METHOD("get_utf8_string", {"bytes"}), &StreamPeer::get_utf8_string, {DEFVAL(-1)});
    MethodBinder::bind_method(D_METHOD("get_var", {"allow_objects"}), &StreamPeer::get_var, {DEFVAL(false)});

}
////////////////////////////////

void StreamPeerBuffer::_bind_methods() {

    SE_BIND_METHOD(StreamPeerBuffer,seek);
    SE_BIND_METHOD(StreamPeerBuffer,get_size);
    SE_BIND_METHOD(StreamPeerBuffer,get_position);
    SE_BIND_METHOD(StreamPeerBuffer,resize);
    SE_BIND_METHOD(StreamPeerBuffer,set_data_array);
    SE_BIND_METHOD(StreamPeerBuffer,get_data_array);
    SE_BIND_METHOD(StreamPeerBuffer,clear);
    SE_BIND_METHOD(StreamPeerBuffer,duplicate);

    ADD_PROPERTY(PropertyInfo(VariantType::POOL_BYTE_ARRAY, "data_array"), "set_data_array", "get_data_array");
}

Error StreamPeerBuffer::put_data(const uint8_t *p_data, int p_bytes) {

    if (p_bytes <= 0)
        return OK;

    if (pointer + p_bytes > data.size()) {
        data.resize(pointer + p_bytes);
    }

    memcpy(data.data()+pointer, p_data, p_bytes);

    pointer += p_bytes;
    return OK;
}

Error StreamPeerBuffer::put_partial_data(const uint8_t *p_data, int p_bytes, int &r_sent) {

    r_sent = p_bytes;
    return put_data(p_data, p_bytes);
}

Error StreamPeerBuffer::get_data(uint8_t *p_buffer, int p_bytes) {

    int recv;
    get_partial_data(p_buffer, p_bytes, recv);
    if (recv != p_bytes)
        return ERR_INVALID_PARAMETER;

    return OK;
}

Error StreamPeerBuffer::get_partial_data(uint8_t *p_buffer, int p_bytes, int &r_received) {

    if (pointer + p_bytes > data.size()) {
        r_received = data.size() - pointer;
        if (r_received <= 0) {
            r_received = 0;
            return OK; //you got 0
        }
    } else {
        r_received = p_bytes;
    }

    memcpy(p_buffer, data.data() + pointer, r_received);

    pointer += r_received;
    // FIXME: return what? OK or ERR_*
    // return OK for now so we don't maybe return garbage
    return OK;
}

int StreamPeerBuffer::get_available_bytes() const {

    return data.size() - pointer;
}

void StreamPeerBuffer::seek(int p_pos) {

    ERR_FAIL_COND(p_pos < 0);
    ERR_FAIL_COND(p_pos > data.size());
    pointer = p_pos;
}
int StreamPeerBuffer::get_size() const {

    return data.size();
}

int StreamPeerBuffer::get_position() const {

    return pointer;
}

void StreamPeerBuffer::resize(int p_size) {

    data.resize(p_size);
}

void StreamPeerBuffer::set_data_array(Vector<uint8_t> &&p_data) {

    data = eastl::move(p_data);
    pointer = 0;
}

void StreamPeerBuffer::clear() {

    data.resize(0);
    pointer = 0;
}

Ref<StreamPeerBuffer> StreamPeerBuffer::duplicate() const {

    Ref<StreamPeerBuffer> spb(make_ref_counted<StreamPeerBuffer>());
    spb->data = data;
    return spb;
}


