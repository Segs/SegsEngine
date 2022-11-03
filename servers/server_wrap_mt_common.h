/*************************************************************************/
/*  server_wrap_mt_common.h                                              */
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
#define FUNC0R(m_r, m_type)                                                     \
    m_r m_type() override {                                                     \
        assert(Thread::get_caller_id() != server_thread);                       \
        m_r ret;                                                                \
        command_queue.push_and_sync( [&ret]() { ret = submission_thread_singleton->m_type();});\
        SYNC_DEBUG                                                              \
        return ret;                                                             \
    }

#define FUNCENT(m_type)                                                                    \
    Vector<RenderingEntity> m_type##_id_pool;                                              \
    void m_type##_free_cached_ids() {                                                      \
        assert(Thread::get_caller_id() != server_thread);                       \
        command_queue.push_and_sync( [this]() {                                            \
            for(auto v : m_type##_id_pool) {                                               \
                submission_thread_singleton->free_rid(v);                                  \
            }                                                                              \
        });                                                                                \
        m_type##_id_pool.clear();                                                          \
    }                                                                                      \
    RenderingEntity m_type##_create() override {                                           \
        assert(Thread::get_caller_id() != server_thread);                                  \
        RenderingEntity rid;                                                               \
        MutexLock lock(alloc_mutex);                                                       \
        if (m_type##_id_pool.empty()) {                                                    \
            command_queue.push_and_sync([this]() {                                         \
                for (int i = 0; i < pool_max_size; i++) {                                  \
                    m_type##_id_pool.emplace_back(submission_thread_singleton->m_type##_create()); \
                }                                                                          \
            });                                                                            \
            SYNC_DEBUG                                                                 \
        }                                                                              \
        rid = m_type##_id_pool.back();                                                 \
        m_type##_id_pool.pop_back();                                                   \
        return rid;                                                                    \
    }

#define FUNCRID(m_type)                                                                    \
    Vector<RID> m_type##_id_pool;                                                          \
    void m_type##_free_cached_ids() {                                                      \
        assert(Thread::get_caller_id() != server_thread);                       \
        command_queue.push_and_sync( [this]() {                                            \
            for(auto v : m_type##_id_pool) {                                               \
                submission_thread_singleton->free_rid(v);                                  \
            }                                                                              \
        });                                                                                \
        m_type##_id_pool.clear();                                                          \
    }                                                                                      \
    RID m_type##_create() override {                                                       \
        assert(Thread::get_caller_id() != server_thread);                                  \
        RID rid;                                                                           \
        MutexLock lock(alloc_mutex);                                                       \
        if (m_type##_id_pool.empty()) {                                                    \
            command_queue.push_and_sync([this]() {                                         \
                for (int i = 0; i < pool_max_size; i++) {                                  \
                    m_type##_id_pool.emplace_back(submission_thread_singleton->m_type##_create()); \
                }                                                                          \
            });                                                                            \
            SYNC_DEBUG                                                                 \
        }                                                                              \
        rid = m_type##_id_pool.back();                                                 \
        m_type##_id_pool.pop_back();                                                   \
        return rid;                                                                    \
    }

#define FUNC0RC(m_r, m_type)                                                    \
    m_r m_type() const override {                                               \
        assert(Thread::get_caller_id() != server_thread);                       \
        m_r ret;                                                            \
        command_queue.push_and_sync( [&ret]() { ret = submission_thread_singleton->m_type();});\
        SYNC_DEBUG                                                          \
        return ret;                                                         \
    }

#define FUNC0(m_type)                                               \
    void m_type() override {                                        \
        assert(Thread::get_caller_id() != server_thread);           \
        command_queue.push( []() { submission_thread_singleton->m_type();});\
    }

#define FUNC0C(m_type)                                            \
    void m_type() const override {                                 \
        assert(Thread::get_caller_id() != server_thread);           \
        command_queue.push( []() { submission_thread_singleton->m_type();});\
    }

///////////////////////////////////////////////

#define FUNC1R(m_r, m_type, m_arg1)                                                 \
    m_r m_type(m_arg1 p1) override {                                                 \
        assert(Thread::get_caller_id() != server_thread);                             \
        m_r ret;                                                                \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1); }); \
        SYNC_DEBUG                                                              \
        return ret;                                                             \
    }

#define FUNC1RC(m_r, m_type, m_arg1)                                                \
    m_r m_type(m_arg1 p1) const override {                                           \
        assert(Thread::get_caller_id() != server_thread);                             \
        m_r ret;                                                                \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1); }); \
        SYNC_DEBUG                                                              \
        return ret;                                                             \
    }

#define FUNC1S(m_type, m_arg1)                                                 \
    void m_type(m_arg1 p1) override {                                           \
        assert(Thread::get_caller_id() != server_thread);                        \
        command_queue.push_and_sync( [=]() { submission_thread_singleton->m_type(p1); }); \
        SYNC_DEBUG                                                         \
    }

#define FUNC1(m_type, m_arg1)                                         \
    void m_type(m_arg1 p1) override {								  \
        assert(Thread::get_caller_id() != server_thread);               \
        command_queue.push( [p1]() { submission_thread_singleton->m_type(p1); }); \
    }

#define FUNC2R(m_r, m_type, m_arg1, m_arg2)                                             \
    m_r m_type(m_arg1 p1, m_arg2 p2) override {                                          \
        assert(Thread::get_caller_id() != server_thread);                                 \
        m_r ret;                                                                    \
        command_queue.push_and_sync( [p1,p2,&ret]() { ret=submission_thread_singleton->m_type(p1, p2); }); \
        SYNC_DEBUG                                                                  \
        return ret;                                                                 \
    }

#define FUNC2RC(m_r, m_type, m_arg1, m_arg2)                                            \
    m_r m_type(m_arg1 p1, m_arg2 p2) const override {                                    \
        assert(Thread::get_caller_id() != server_thread);                                 \
        m_r ret;                                                                    \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2); }); \
        SYNC_DEBUG                                                                  \
        return ret;                                                                 \
    }

#define FUNC2S(m_type, m_arg1, m_arg2)                                             \
    void m_type(m_arg1 p1, m_arg2 p2) override {                                   \
        assert(Thread::get_caller_id() != server_thread);                            \
        command_queue.push_and_sync( [=]() { submission_thread_singleton->m_type(p1, p2); });  \
        SYNC_DEBUG                                                             \
    }

#define FUNC2SC(m_type, m_arg1, m_arg2)                                            \
    void m_type(m_arg1 p1, m_arg2 p2) const override {                              \
        assert(Thread::get_caller_id() != server_thread);                            \
        command_queue.push_and_sync( [=]() { submission_thread_singleton->m_type(p1, p2); }); \
        SYNC_DEBUG                                                             \
    }

#define FUNC2(m_type, m_arg1, m_arg2)                                     \
    void m_type(m_arg1 p1, m_arg2 p2) override {						  \
        assert(Thread::get_caller_id() != server_thread);                   \
        command_queue.push([p1,p2]() {submission_thread_singleton->m_type(p1, p2);}); \
    }

#define FUNC3R(m_r, m_type, m_arg1, m_arg2, m_arg3)                                         \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3) override {                                   \
        assert(Thread::get_caller_id() != server_thread);                                     \
        m_r ret;                                                                        \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2, p3); }); \
        SYNC_DEBUG                                                                      \
        return ret;                                                                     \
    }

#define FUNC3RC(m_r, m_type, m_arg1, m_arg2, m_arg3)                                        \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3) const override {                            \
        assert(Thread::get_caller_id() != server_thread);                                     \
        m_r ret;                                                                        \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2, p3); }); \
        SYNC_DEBUG                                                                      \
        return ret;                                                                     \
    }

#define FUNC3(m_type, m_arg1, m_arg2, m_arg3)                                 \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3) override {                   \
        assert(Thread::get_caller_id() != server_thread);                       \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3); }); \
    }

#define FUNC4R(m_r, m_type, m_arg1, m_arg2, m_arg3, m_arg4)                                     \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4) override {                           \
        assert(Thread::get_caller_id() != server_thread);                                         \
        m_r ret;                                                                            \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2, p3, p4); }); \
        SYNC_DEBUG                                                                          \
        return ret;                                                                         \
    }

#define FUNC4(m_type, m_arg1, m_arg2, m_arg3, m_arg4)                             \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4) override {             \
        assert(Thread::get_caller_id() != server_thread);                           \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4); }); \
    }

#define FUNC4C(m_type, m_arg1, m_arg2, m_arg3, m_arg4)                            \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4) const override {       \
        assert(Thread::get_caller_id() != server_thread);                           \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4); }); \
    }

#define FUNC5R(m_r, m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5)                                 \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5) override {                     \
        assert(Thread::get_caller_id() != server_thread);                                             \
        m_r ret;                                                                                \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2, p3, p4, p5); }); \
        SYNC_DEBUG                                                                              \
        return ret;                                                                             \
    }

#define FUNC5(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5)                         \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5) override {      \
        assert(Thread::get_caller_id() != server_thread);                               \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5); }); \
    }

#define FUNC6(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6)                       \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6) override { \
        assert(Thread::get_caller_id() != server_thread);                                     \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6); });   \
    }

#define FUNC7(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7)                          \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7) override { \
        assert(Thread::get_caller_id() != server_thread);                                             \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7); });          \
    }

#define FUNC7R(m_r, m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7)                      \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7) override { \
        assert(Thread::get_caller_id() != server_thread);                                                          \
        m_r ret;                                                                                             \
        command_queue.push_and_sync( [&]() { ret=submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7); });  \
        SYNC_DEBUG                                                                                           \
        return ret;                                                                                          \
    }
#define FUNC8R(m_r, m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8)                      \
    m_r m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8) override { \
        assert(Thread::get_caller_id() != server_thread);                                                          \
        m_r ret;                                                                                             \
        command_queue.push_and_sync( [=,&ret]() { ret=submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8); });  \
        SYNC_DEBUG                                                                                           \
        return ret;                                                                                          \
    }

#define FUNC8(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8)                             \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8) override { \
        assert(Thread::get_caller_id() != server_thread);                                                           \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8); });                 \
    }

#define FUNC9(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9)                                \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8, m_arg9 p9) override { \
        assert(Thread::get_caller_id() != server_thread);                                                                      \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8, p9); });                        \
    }

#define FUNC10(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10)                                   \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8, m_arg9 p9, m_arg10 p10) override { \
        assert(Thread::get_caller_id() != server_thread);                                                                                   \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10); });                                \
    }

#define FUNC11(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11)                                       \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8, m_arg9 p9, m_arg10 p10, m_arg11 p11) override { \
        assert(Thread::get_caller_id() != server_thread);                                                                                                \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11); });                                        \
    }

#define FUNC12(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12)                                           \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8, m_arg9 p9, m_arg10 p10, m_arg11 p11, m_arg12 p12) override { \
        assert(Thread::get_caller_id() != server_thread);                                                                                                             \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12); });                                                \
    }

#define FUNC13(m_type, m_arg1, m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_arg9, m_arg10, m_arg11, m_arg12, m_arg13)                                               \
    void m_type(m_arg1 p1, m_arg2 p2, m_arg3 p3, m_arg4 p4, m_arg5 p5, m_arg6 p6, m_arg7 p7, m_arg8 p8, m_arg9 p9, m_arg10 p10, m_arg11 p11, m_arg12 p12, m_arg13 p13) override { \
        assert(Thread::get_caller_id() != server_thread);                                                                                                                          \
        command_queue.push( [=]() { submission_thread_singleton->m_type(p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13); });                                                        \
    }
