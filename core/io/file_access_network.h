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
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/string.h"

class Thread;
class FileAccessNetwork;
class SemaphoreOld;
class StreamPeerTCP;

using Mutex = std::recursive_mutex;
class Semaphore;

class FileAccessNetworkClient {

    void *m_priv;
    Vector<uint8_t> block;
    Semaphore sem;
    Thread thread;
    Mutex mutex;
    Mutex blockrequest_mutex;
    int32_t last_id;

    bool quit;

    void _thread_func();
    static void _thread_func(void *s);

    void put_32(int32_t p_32);
    void put_64(int64_t p_64);
    int32_t get_32();
    int64_t get_64();

    void lock_mutex();
    void unlock_mutex();

    friend class FileAccessNetwork;
    static FileAccessNetworkClient *singleton;
    void add_block_request(int id, int page_size, int page_offset);
    int record_access_source(FileAccessNetwork *from);
    bool is_my_token_valid(int source_id,FileAccessNetwork *from);
    void finish_access(int id,FileAccessNetwork *from);
public:
    static FileAccessNetworkClient *get_singleton() { return singleton; }

    Error connect(const String &p_host, int p_port, const String &p_password = String());

    FileAccessNetworkClient();
    ~FileAccessNetworkClient();
};

class FileAccessNetwork : public FileAccess {

    Semaphore sem;
    mutable Semaphore page_sem;
    mutable Mutex buffer_mutex;
    bool opened;
    uint64_t total_size;
    mutable uint64_t pos;
    int32_t id;
    mutable bool eof_flag;
    mutable int32_t last_page;
    mutable uint8_t *last_page_buff;

    int32_t page_size;
    int32_t read_ahead;
    uint64_t exists_modtime;
    void *m_priv;

    friend class FileAccessNetworkClient;
    void _queue_page(int32_t p_page) const;
    void _respond(uint64_t p_len, Error p_status);
    void _set_block(uint64_t p_offset, const Vector<uint8_t> &p_block);

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

    Error _open(StringView p_path, int p_mode_flags) override; ///< open a file
    void close() override; ///< close a file
    bool is_open() const override; ///< true when file is open

    void seek(size_t p_position) override; ///< seek to a given position
    void seek_end(int64_t p_position = 0) override; ///< seek from the end of file
    size_t get_position() const override; ///< get position in the file
    size_t get_len() const override; ///< get size of the file

    bool eof_reached() const override; ///< reading passed EOF

    uint8_t get_8() const override; ///< get a byte
    uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

    Error get_error() const override; ///< get last error

    void flush() override;
    void store_8(uint8_t p_dest) override; ///< store a byte

    bool file_exists(StringView p_path) override; ///< return true if a file exists

    uint64_t _get_modified_time(StringView p_file) override;
    uint32_t _get_unix_permissions(StringView p_file) override;
    Error _set_unix_permissions(StringView p_file, uint32_t p_permissions) override;

    static void configure();

    FileAccessNetwork();
    ~FileAccessNetwork() override;
};
