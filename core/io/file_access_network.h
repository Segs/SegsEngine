/*************************************************************************/
/*  file_access_network.h                                                */
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

#include "core/os/file_access.h"
#include "core/reference.h"
#include "core/se_string.h"
#include "core/list.h"
#include "core/map.h"

class Thread;
class FileAccessNetwork;
class Semaphore;
class StreamPeerTCP;
namespace std {
class recursive_mutex;
}
using Mutex = std::recursive_mutex;

class FileAccessNetworkClient {

    struct BlockRequest {

        uint64_t offset;
        int id;
        int size;
    };

    List<BlockRequest> block_requests;

    Semaphore *sem;
    Thread *thread;
    bool quit;
    Mutex *mutex;
    Mutex *blockrequest_mutex;
    Map<int, FileAccessNetwork *> accesses;
    Ref<StreamPeerTCP> client;
    int last_id;

    PODVector<uint8_t> block;

    void _thread_func();
    static void _thread_func(void *s);

    void put_32(int p_32);
    void put_64(int64_t p_64);
    int get_32();
    int64_t get_64();
    int lockcount;
    void lock_mutex();
    void unlock_mutex();

    friend class FileAccessNetwork;
    static FileAccessNetworkClient *singleton;

public:
    static FileAccessNetworkClient *get_singleton() { return singleton; }

    Error connect(const String &p_host, int p_port, const String &p_password = String());

    FileAccessNetworkClient();
    ~FileAccessNetworkClient();
};

class FileAccessNetwork : public FileAccess {

    Semaphore *sem;
    Semaphore *page_sem;
    Mutex *buffer_mutex;
    bool opened;
    size_t total_size;
    mutable size_t pos;
    int id;
    mutable bool eof_flag;
    mutable int last_page;
    mutable uint8_t *last_page_buff;

    int page_size;
    int read_ahead;
    uint64_t exists_modtime;
    void *m_priv;

    friend class FileAccessNetworkClient;
    void _queue_page(int p_page) const;
    void _respond(size_t p_len, Error p_status);
    void _set_block(int p_offset, const PODVector<uint8_t> &p_block);

public:
    enum Command {
        COMMAND_OPEN_FILE,
        COMMAND_READ_BLOCK,
        COMMAND_CLOSE,
        COMMAND_FILE_EXISTS,
        COMMAND_GET_MODTIME,
    };

    enum Response {
        RESPONSE_OPEN,
        RESPONSE_DATA,
        RESPONSE_FILE_EXISTS,
        RESPONSE_GET_MODTIME,
    };

    Error _open(se_string_view p_path, int p_mode_flags) override; ///< open a file
    void close() override; ///< close a file
    bool is_open() const override; ///< true when file is open

    void seek(size_t p_position) override; ///< seek to a given position
    void seek_end(int64_t p_position = 0) override; ///< seek from the end of file
    size_t get_position() const override; ///< get position in the file
    size_t get_len() const override; ///< get size of the file

    bool eof_reached() const override; ///< reading passed EOF

    uint8_t get_8() const override; ///< get a byte
    int get_buffer(uint8_t *p_dst, int p_length) const override;

    Error get_error() const override; ///< get last error

    void flush() override;
    void store_8(uint8_t p_dest) override; ///< store a byte

    bool file_exists(se_string_view p_path) override; ///< return true if a file exists

    uint64_t _get_modified_time(se_string_view p_file) override;
    uint32_t _get_unix_permissions(se_string_view p_file) override;
    Error _set_unix_permissions(se_string_view p_file, uint32_t p_permissions) override;

    static void configure();

    FileAccessNetwork();
    ~FileAccessNetwork() override;
};
