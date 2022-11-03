#pragma once

#include "glad/glad.h"

#include <cstring> // for memset
#include <stdint.h>


using GLuint = uint32_t;

struct GLBufferImpl {
    static void release(int count,GLuint *data) noexcept {
        if(count && data[0]) {
            glDeleteBuffers(count,data);
            memset(data,0,sizeof(GLuint)*count);
        }
    }
    static void create(int count,GLuint *data) noexcept {
        release(count,data);
        glGenBuffers(count,data);
    }
};

struct GLVAOImpl {
    static void release(int count,GLuint *data) noexcept {
        if(count && data[0]) {
            glDeleteVertexArrays(count,data);
            memset(data,0,sizeof(GLuint)*count);
        }
    }
    static void create(int count,GLuint *data) noexcept {
        release(count,data);
        glGenVertexArrays(count,data);
    }
};

struct GLTextureImpl {
    static void release(int count,GLuint *data) noexcept {
        if(count && data[0]) {
            glDeleteTextures(count,data);
            memset(data,0,sizeof(GLuint)*count);
        }
    }
    static void create(int count,GLuint *data) noexcept {
        release(count,data);
        glGenTextures(count,data);
    }
};

struct GLFramebufferImpl {
    static void release(int count,GLuint *data) noexcept {
        if(count && data[0]) {
            glDeleteFramebuffers(count,data);
            memset(data,0,sizeof(GLuint)*count);
        }
    }
    static void create(int count,GLuint *data) noexcept {
        release(count,data);
        glGenFramebuffers(count,data);
    }
};

struct GLRenderBufferImpl {
    static void release(int count,GLuint *data) noexcept {
        if(count && data[0]) {
            glDeleteRenderbuffers(count,data);
            memset(data,0,sizeof(GLuint)*count);
        }
    }
    static void create(int count,GLuint *data) noexcept {
        release(count,data);
        glGenRenderbuffers(count,data);
    }
};


template<uint32_t count,typename ResourceImpl>
struct GLMultiHandle {
    GLuint value[count];

    // auto conversion operator, for single entry handles returns the stored value, otherwise return the pointer to storage
    operator auto() const {
        if constexpr(count==1)
            return value[0];
        else
            return value;
    }
    void release() noexcept {
        ResourceImpl::release(count,value);
    }
    void create() noexcept {
        release();
        ResourceImpl::create(count,value);
    }
    [[nodiscard]] bool is_initialized() const { return value[0]!=0; }

    constexpr bool operator!=(GLuint e) const { static_assert(count==1); return value[0]!=e; }
    constexpr bool operator==(GLuint e) const { static_assert(count == 1); return value[0]==e; }


    GLMultiHandle(const GLMultiHandle &) = delete;
    GLMultiHandle &operator=(const GLMultiHandle &) = delete;

    GLMultiHandle(GLMultiHandle &&f) noexcept {
        memcpy(value,f.value,sizeof(GLuint)*count);
        memset(f.value,0,sizeof(GLuint)*count);
    }
    GLMultiHandle &operator=(GLMultiHandle &&f) noexcept {
        ResourceImpl::release(count,value);
        if(this!=&f) // do we actually need to copy anything ?  NOTE: This conditional might be removed if it results in shorter code
            memcpy(value,f.value,sizeof(GLuint)*count);

        memset(f.value,0,sizeof(GLuint)*count);
        return *this;
    }
    GLuint operator[](uint32_t idx) const {
        return value[idx];
    }
    GLuint &operator[](uint32_t idx) {
        return value[idx];
    }
    constexpr GLMultiHandle() {
        value[0] = 0;
    }
    ~GLMultiHandle()  noexcept {
        release();
    }
};



template<uint32_t count>
using GLMultiBufferHandle = GLMultiHandle<count, GLBufferImpl>;
template<uint32_t count>
using GLMultiTextureHandle = GLMultiHandle<count, GLTextureImpl>;
template<uint32_t count>
using GLMultiVAOHandle = GLMultiHandle<count, GLVAOImpl>;
template<uint32_t count>
using GLMultiFBOHandle = GLMultiHandle<count, GLFramebufferImpl>;

using GLBufferHandle= GLMultiHandle<1, GLBufferImpl>;
using GLTextureHandle = GLMultiHandle<1, GLTextureImpl>;
using GLFBOHandle = GLMultiHandle<1, GLFramebufferImpl>;
using GLRenderBufferHandle = GLMultiHandle<1, GLRenderBufferImpl>;
using GLVAOHandle = GLMultiHandle<1, GLVAOImpl>;

struct GLNonOwningHandle {
    GLuint value{0};

         // auto conversion operator, for single entry handles returns the stored value, otherwise return the pointer to storage
    operator auto() const {
        return value;
    }
    [[nodiscard]] constexpr bool is_initialized() const { return value!=0; }

    constexpr bool operator!=(GLuint e) const { return value!=e; }
    constexpr bool operator==(GLuint e) const { return value==e; }

    GLNonOwningHandle(const GLNonOwningHandle &) = delete;
    GLNonOwningHandle &operator=(const GLNonOwningHandle &) = delete;

    GLNonOwningHandle &operator=(const GLTextureHandle &f) {
        value = f;
        return *this;
    }

    GLNonOwningHandle(GLNonOwningHandle &&f) noexcept {
        value = f.value;
        f.value = 0;
    }

    GLNonOwningHandle &operator=(GLNonOwningHandle &&f) noexcept {
        value = f.value;
        f.value = 0; // this is set to 0 to help with debugging when moved-from handle is used
        return *this;
    }

    constexpr GLNonOwningHandle() = default;
    constexpr GLNonOwningHandle(GLuint v) : value(v) {}
    ~GLNonOwningHandle()  noexcept = default;
};
