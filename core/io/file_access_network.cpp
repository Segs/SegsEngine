/*************************************************************************/
/*  file_access_network.cpp                                              */
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

#include "file_access_network.h"

#include "core/io/ip.h"
#include "core/io/marshalls.h"
#include "core/io/stream_peer_tcp.h"
#include "core/os/os.h"
#include "core/os/semaphore.h"
#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/reference.h"
#include "core/string.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/string_formatter.h"
#include "core/project_settings.h"
#include "core/map.h"

#include "EASTL/deque.h"

//#define DEBUG_PRINT(m_p) print_line(m_p)
//#define DEBUG_TIME(m_what) printf("MS: %s - %lli\n",m_what,OS::get_singleton()->get_ticks_usec());
#define DEBUG_PRINT(m_p)
#define DEBUG_TIME(m_what)
namespace {
struct FileAccessNetworkClient_priv {
    struct BlockRequest {

        uint64_t offset;
        int32_t id;
        int32_t size;
    };
    Ref<StreamPeerTCP> client=make_ref_counted<StreamPeerTCP>();

    Dequeue<BlockRequest> block_requests;
    Map<int, FileAccessNetwork *> accesses;

};

struct FileAccessNetwork_priv {
    mutable int waiting_on_page;
    struct Page {
        Vector<uint8_t> buffer;
        int activity = 0;
        bool queued = false;
    };

    mutable Vector<Page> pages;

    mutable Error response;

};

}
#define D_PRIV() ((FileAccessNetwork_priv *)m_priv)
#define C_PRIV() ((FileAccessNetworkClient_priv *)m_priv)

void FileAccessNetworkClient::lock_mutex() {
    mutex.lock();
}

void FileAccessNetworkClient::unlock_mutex() {
    mutex.unlock();
}

void FileAccessNetworkClient::put_32(int32_t p_32) {

    uint8_t buf[4];
    encode_uint32(p_32, buf);
    C_PRIV()->client->put_data(buf, 4);
    DEBUG_PRINT("put32: " + itos(p_32));
}

void FileAccessNetworkClient::put_64(int64_t p_64) {

    uint8_t buf[8];
    encode_uint64(p_64, buf);
    C_PRIV()->client->put_data(buf, 8);
    DEBUG_PRINT("put64: " + itos(p_64));
}

int32_t FileAccessNetworkClient::get_32() {

    uint8_t buf[4];
    C_PRIV()->client->get_data(buf, 4);
    return decode_uint32(buf);
}

int64_t FileAccessNetworkClient::get_64() {

    uint8_t buf[8];
    C_PRIV()->client->get_data(buf, 8);
    return decode_uint64(buf);
}

void FileAccessNetworkClient::_thread_func() {
    auto priv = C_PRIV();
    priv->client->set_no_delay(true);
    while (!quit) {

        DEBUG_PRINT("SEM WAIT - " + itos(sem.get()));
        sem.wait();

        DEBUG_TIME("sem_unlock");
        //DEBUG_PRINT("semwait returned "+itos(werr));
        //DEBUG_PRINT("MUTEX LOCK " + itos(lockcount));
        lock_mutex();
        DEBUG_PRINT("MUTEX PASS");

        {
            MutexLock guard(blockrequest_mutex);
            while (!priv->block_requests.empty()) {
                put_32(priv->block_requests.front().id);
                put_32(FileAccessNetwork::COMMAND_READ_BLOCK);
                put_64(priv->block_requests.front().offset);
                put_32(priv->block_requests.front().size);
                priv->block_requests.pop_front();
            }
        }

        DEBUG_PRINT("THREAD ITER");

        DEBUG_TIME("sem_read");
        int id = get_32();

        int response = get_32();
        DEBUG_PRINT("GET RESPONSE: " + itos(response));

        FileAccessNetwork *fa = nullptr;

        if (response != FileAccessNetwork::RESPONSE_DATA) {
            if (!priv->accesses.contains(id)) {
                unlock_mutex();
                ERR_FAIL_COND(!priv->accesses.contains(id));
            }
        }
        auto iter=priv->accesses.find(id);
        if (iter!=priv->accesses.end()) {
            fa = iter->second;
        }

        switch (response) {

            case FileAccessNetwork::RESPONSE_OPEN: {

                DEBUG_TIME("sem_open");
                int status = get_32();
                if (status != OK) {
                    fa->_respond(0, Error(status));
                } else {
                    uint64_t len = get_64();
                    fa->_respond(len, Error(status));
                }

                fa->sem.post();

            } break;
            case FileAccessNetwork::RESPONSE_DATA: {

                int64_t offset = get_64();
                uint32_t len = get_32();

                Vector<uint8_t> block; // TODO: replace with Temporary-allocated vector
                block.resize(len);
                priv->client->get_data(block.data(), len);

                if (fa) //may have been queued
                    fa->_set_block(offset, block);

            } break;
            case FileAccessNetwork::RESPONSE_FILE_EXISTS: {

                int status = get_32();
                if(fa) {
                    fa->exists_modtime = status != 0;
                    fa->sem.post();
                }

            } break;
            case FileAccessNetwork::RESPONSE_GET_MODTIME: {

                uint64_t status = get_64();
                fa->exists_modtime = status;
                fa->sem.post();

            } break;
        }

        unlock_mutex();
    }
}

void FileAccessNetworkClient::_thread_func(void *s) {

    FileAccessNetworkClient *self = (FileAccessNetworkClient *)s;

    self->_thread_func();
}

Error FileAccessNetworkClient::connect(const String &p_host, int p_port, const String &p_password) {

    IP_Address ip;

    if (StringUtils::is_valid_ip_address(p_host)) {
        ip = IP_Address(p_host.c_str());
    } else {
        ip = IP::get_singleton()->resolve_hostname(p_host);
    }

    DEBUG_PRINT("IP: " + String(ip) + " port " + ::to_string(p_port));
    Error err = C_PRIV()->client->connect_to_host(ip, p_port);
    ERR_FAIL_COND_V_MSG(err != OK, err, FormatVE("Cannot connect to host with IP: %s and port: %d",String(ip).c_str(),p_port));
    while (C_PRIV()->client->get_status() == StreamPeerTCP::STATUS_CONNECTING) {
        //DEBUG_PRINT("trying to connect....");
        OS::get_singleton()->delay_usec(1000);
    }

    if (C_PRIV()->client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
        return ERR_CANT_CONNECT;
    }

    String cs = p_password;
    put_32(cs.length());
    C_PRIV()->client->put_data((const uint8_t *)cs.data(), cs.length());

    int e = get_32();

    if (e != OK) {
        return ERR_INVALID_PARAMETER;
    }

    thread.start(_thread_func, this);

    return OK;
}

FileAccessNetworkClient *FileAccessNetworkClient::singleton = nullptr;

FileAccessNetworkClient::FileAccessNetworkClient() {
    m_priv = memnew(FileAccessNetworkClient_priv);
    quit = false;
    singleton = this;
    last_id = 0;
}

FileAccessNetworkClient::~FileAccessNetworkClient() {

    quit = true;
    sem.post();
    thread.wait_to_finish();
    memdelete((FileAccessNetworkClient_priv *)m_priv);
}
void FileAccessNetworkClient::add_block_request(int id,int page_size,int page_offset) {
    using Priv = FileAccessNetworkClient_priv;

    Priv::BlockRequest br;
    br.offset = size_t(page_offset) * page_size;
    br.id = id;
    br.size = page_size;
    ((Priv *)m_priv)->block_requests.emplace_back(br);

}

int FileAccessNetworkClient::record_access_source(FileAccessNetwork *from)
{
    using Priv = FileAccessNetworkClient_priv;
    lock_mutex();
    auto id = last_id++;
    ((Priv *)m_priv)->accesses[id] = from;
    unlock_mutex();
    return id;
}

bool FileAccessNetworkClient::is_my_token_valid(int source_id, FileAccessNetwork *from)
{
    using Priv = FileAccessNetworkClient_priv;

    auto iter=((Priv *)m_priv)->accesses.find(source_id);
    if(((Priv *)m_priv)->accesses.end()==iter) {
        return false;
    }
    return iter->second==from;
}

void FileAccessNetworkClient::finish_access(int id, FileAccessNetwork *from)
{
    using Priv = FileAccessNetworkClient_priv;
    MutexLock guard(mutex);

    auto iter=((Priv *)m_priv)->accesses.find(id);
    ERR_FAIL_COND(((Priv *)m_priv)->accesses.end()==iter);
    ERR_FAIL_COND(from!=iter->second);
    ((Priv *)m_priv)->accesses.erase(iter);
    unlock_mutex();
}
void FileAccessNetwork::_set_block(uint64_t p_offset, const Vector<uint8_t> &p_block) {

    int page = p_offset / page_size;
    ERR_FAIL_INDEX(page, D_PRIV()->pages.size());
    if (page < D_PRIV()->pages.size() - 1) {
        ERR_FAIL_COND(p_block.size() != page_size);
    } else {
        ERR_FAIL_COND((p_block.size() != (int)(total_size % page_size)));
    }

    {
        MutexLock guard(buffer_mutex);
        D_PRIV()->pages[page].buffer = p_block;
        D_PRIV()->pages[page].queued = false;
    }

    if (D_PRIV()->waiting_on_page == page) {
        D_PRIV()->waiting_on_page = -1;
        page_sem.post();
    }
}

void FileAccessNetwork::_respond(uint64_t p_len, Error p_status) {

    DEBUG_PRINT("GOT RESPONSE - len: " + itos(p_len) + " status: " + itos(p_status));
    D_PRIV()->response = p_status;
    if (D_PRIV()->response != OK)
        return;
    opened = true;
    total_size = p_len;
    int pc = ((total_size - 1) / page_size) + 1;
    D_PRIV()->pages.resize(pc);
}

Error FileAccessNetwork::_open(StringView p_path, int p_mode_flags) {

    ERR_FAIL_COND_V(p_mode_flags != READ, ERR_UNAVAILABLE);
    if (opened)
        close();
    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
    DEBUG_PRINT("open: " + p_path);

    DEBUG_TIME("open_begin");

    nc->lock_mutex();
    ERR_FAIL_COND_V(!nc->is_my_token_valid(id,this), ERR_UNAVAILABLE); //Network access was somehow replaced in client map ?
    nc->put_32(id);
    nc->put_32(COMMAND_OPEN_FILE);
    nc->put_32(p_path.length());
    FileAccessNetworkClient_priv *priv = (FileAccessNetworkClient_priv *)nc->m_priv;
    priv->client->put_data((const uint8_t *)p_path.data(), p_path.length());
    pos = 0;
    eof_flag = false;
    last_page = -1;
    last_page_buff = nullptr;

    //buffers.clear();
    nc->unlock_mutex();
    DEBUG_PRINT("OPEN POST");
    DEBUG_TIME("open_post");
    nc->sem.post(); //awaiting answer
    DEBUG_PRINT("WAIT...");
    sem.wait();
    DEBUG_TIME("open_end");
    DEBUG_PRINT("WAIT ENDED...");

    return D_PRIV()->response;
}

void FileAccessNetwork::close() {

    if (!opened) {
        return;
    }

    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;

    DEBUG_PRINT("CLOSE");
    nc->lock_mutex();
    nc->put_32(id);
    nc->put_32(COMMAND_CLOSE);
    D_PRIV()->pages.clear();
    opened = false;
    nc->unlock_mutex();
}
bool FileAccessNetwork::is_open() const {

    return opened;
}

void FileAccessNetwork::seek(size_t p_position) {

    ERR_FAIL_COND_MSG(!opened, "File must be opened before use.");
    eof_flag = p_position > total_size;

    if (p_position >= total_size) {
        p_position = total_size;
    }

    pos = p_position;
}

void FileAccessNetwork::seek_end(int64_t p_position) {

    seek(total_size + p_position);
}
size_t FileAccessNetwork::get_position() const {

    ERR_FAIL_COND_V_MSG(!opened, 0, "File must be opened before use.");
    return pos;
}
size_t FileAccessNetwork::get_len() const {

    ERR_FAIL_COND_V_MSG(!opened, 0, "File must be opened before use.");
    return total_size;
}

bool FileAccessNetwork::eof_reached() const {

    ERR_FAIL_COND_V_MSG(!opened, false, "File must be opened before use.");
    return eof_flag;
}

uint8_t FileAccessNetwork::get_8() const {

    uint8_t v=0;
    get_buffer(&v, 1);
    return v;
}

void FileAccessNetwork::_queue_page(int32_t p_page) const {

    if (p_page >= D_PRIV()->pages.size())
        return;
    if (D_PRIV()->pages[p_page].buffer.empty() && !D_PRIV()->pages[p_page].queued) {

        FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
        {
            MutexLock guard(nc->blockrequest_mutex);
            nc->add_block_request(id,page_size,p_page);
            D_PRIV()->pages[p_page].queued = true;
        }
        DEBUG_PRINT("QUEUE PAGE POST");
        nc->sem.post();
        DEBUG_PRINT("queued " + itos(p_page));
    }
}

uint64_t FileAccessNetwork::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
    ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);

    //bool eof=false;
    if (pos + p_length > total_size) {
        eof_flag = true;
    }
    if (pos + p_length >= total_size) {
        p_length = total_size - pos;
    }

    //FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;

    uint8_t *buff = last_page_buff;

    for (int i = 0; i < p_length; i++) {

        int page = pos / page_size;

        if (page != last_page) {
            buffer_mutex.lock();
            if (D_PRIV()->pages[page].buffer.empty()) {
                D_PRIV()->waiting_on_page = page;
                for (int j = 0; j < read_ahead; j++) {

                    _queue_page(page + j);
                }
                buffer_mutex.unlock();
                DEBUG_PRINT("wait");
                page_sem.wait();
                DEBUG_PRINT("done");
            } else {

                for (int j = 0; j < read_ahead; j++) {

                    _queue_page(page + j);
                }
                //queue pages
                buffer_mutex.unlock();
            }

            buff = D_PRIV()->pages[page].buffer.data();
            last_page_buff = buff;
            last_page = page;
        }

        p_dst[i] = buff[pos - uint64_t(page) * page_size];
        pos++;
    }

    return p_length;
}

Error FileAccessNetwork::get_error() const {

    return pos == total_size ? ERR_FILE_EOF : OK;
}

void FileAccessNetwork::flush() {
    ERR_FAIL();
}

void FileAccessNetwork::store_8(uint8_t p_dest) {

    ERR_FAIL();
}

bool FileAccessNetwork::file_exists(StringView p_path) {

    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
    {
        MutexLock guard(nc->mutex);
        nc->put_32(id);
        nc->put_32(COMMAND_FILE_EXISTS);
        nc->put_32(p_path.length());
        FileAccessNetworkClient_priv *priv = (FileAccessNetworkClient_priv *)nc->m_priv;

        priv->client->put_data((const uint8_t *)p_path.data(), p_path.length());
    }
    DEBUG_PRINT("FILE EXISTS POST");
    nc->sem.post();
    sem.wait();

    return exists_modtime != 0;
}

uint64_t FileAccessNetwork::_get_modified_time(StringView p_file) {

    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
    FileAccessNetworkClient_priv *priv = (FileAccessNetworkClient_priv *)nc->m_priv;
    {
        MutexLock guard(nc->mutex);
        nc->put_32(id);
        nc->put_32(COMMAND_GET_MODTIME);
        nc->put_32(p_file.length());
        priv->client->put_data((const uint8_t *)p_file.data(), p_file.length());
    }
    DEBUG_PRINT("MODTIME POST");
    nc->sem.post();
    sem.wait();

    return exists_modtime;
}

uint32_t FileAccessNetwork::_get_unix_permissions(StringView p_file) {
    ERR_PRINT("Getting UNIX permissions from network drives is not implemented yet");
    return 0;
}

Error FileAccessNetwork::_set_unix_permissions(StringView p_file, uint32_t p_permissions) {
    ERR_PRINT("Setting UNIX permissions on network drives is not implemented yet");
    return ERR_UNAVAILABLE;
}

void FileAccessNetwork::configure() {

    GLOBAL_DEF("network/remote_fs/page_size", 65536);
    ProjectSettings::get_singleton()->set_custom_property_info("network/remote_fs/page_size", PropertyInfo(VariantType::INT, "network/remote_fs/page_size", PropertyHint::Range, "1,65536,1,or_greater")); //is used as denominator and can't be zero
    GLOBAL_DEF("network/remote_fs/page_read_ahead", 4);
    ProjectSettings::get_singleton()->set_custom_property_info("network/remote_fs/page_read_ahead", PropertyInfo(VariantType::INT, "network/remote_fs/page_read_ahead", PropertyHint::Range, "0,8,1,or_greater"));
}

FileAccessNetwork::FileAccessNetwork() {

    m_priv = new FileAccessNetwork_priv;

    eof_flag = false;
    opened = false;
    pos = 0;
    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
    id = nc->record_access_source(this);
    page_size = GLOBAL_GET("network/remote_fs/page_size").as<int>();
    read_ahead = GLOBAL_GET("network/remote_fs/page_read_ahead").as<int>();
    D_PRIV()->waiting_on_page = -1;
    last_page = -1;
}

FileAccessNetwork::~FileAccessNetwork() {

    FileAccessNetwork::close();

    FileAccessNetworkClient *nc = FileAccessNetworkClient::singleton;
    nc->finish_access(id,this);
    delete D_PRIV();
    m_priv = nullptr;
}
#undef D_PRIV
#undef C_PRIV
