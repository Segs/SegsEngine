/*************************************************************************/
/*  noise_texture.cpp                                                    */
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

#include "noise_texture.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(NoiseTexture)

NoiseTexture::NoiseTexture() {
    update_queued = false;
    regen_queued = false;
    first_time = true;

    size = Vector2i(512, 512);
    seamless = false;
    as_normalmap = false;
    bump_strength = 8.0;
    flags = FLAGS_DEFAULT;

    noise = Ref<OpenSimplexNoise>();

    texture = RenderingServer::get_singleton()->texture_create();

    _queue_update();
}

NoiseTexture::~NoiseTexture() {
    RenderingServer::get_singleton()->free_rid(texture);
    if (noise_thread.is_started()) {
        noise_thread.wait_to_finish();
    }
}

void NoiseTexture::_bind_methods() {

    SE_BIND_METHOD(NoiseTexture,set_width);
    SE_BIND_METHOD(NoiseTexture,set_height);

    SE_BIND_METHOD(NoiseTexture,set_noise);
    SE_BIND_METHOD(NoiseTexture,get_noise);

    SE_BIND_METHOD(NoiseTexture,set_noise_offset);
    SE_BIND_METHOD(NoiseTexture,get_noise_offset);

    SE_BIND_METHOD(NoiseTexture,set_seamless);
    SE_BIND_METHOD(NoiseTexture,get_seamless);

    SE_BIND_METHOD(NoiseTexture,set_as_normalmap);
    SE_BIND_METHOD(NoiseTexture,is_normalmap);

    SE_BIND_METHOD(NoiseTexture,set_bump_strength);
    SE_BIND_METHOD(NoiseTexture,get_bump_strength);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PropertyHint::Range, "1,2048,1,or_greater"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "height", PropertyHint::Range, "1,2048,1,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "seamless"), "set_seamless", "get_seamless");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "as_normalmap"), "set_as_normalmap", "is_normalmap");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "bump_strength", PropertyHint::Range, "0,32,0.1,or_greater"), "set_bump_strength", "get_bump_strength");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "noise", PropertyHint::ResourceType, "OpenSimplexNoise"), "set_noise", "get_noise");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "noise_offset"), "set_noise_offset", "get_noise_offset");
}

void NoiseTexture::_validate_property(PropertyInfo &property) const {

    if (property.name == "bump_strength") {
        if (!as_normalmap) {
            property.usage = PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL;
        }
    }
}

void NoiseTexture::_set_texture_data(const Ref<Image> &p_image) {
    data = p_image;
    if (data) {
        RenderingServer::get_singleton()->texture_allocate(texture, size.x, size.y, 0, p_image->get_format(), RS::TEXTURE_TYPE_2D, flags);
        RenderingServer::get_singleton()->texture_set_data(texture, p_image);
    }
    emit_changed();
}

void NoiseTexture::_thread_done(const Ref<Image> &p_image) {

    _set_texture_data(p_image);

    noise_thread.wait_to_finish();
    if (regen_queued) {
        noise_thread.start(_thread_function, this);
        regen_queued = false;
    }
}

void NoiseTexture::_thread_function(void *p_ud) {
    NoiseTexture *tex = (NoiseTexture *)p_ud;
    tex->call_deferred([tex,img=eastl::move(tex->_generate_texture())]() { tex->_thread_done(img); });
}

void NoiseTexture::_queue_update() {

    if (update_queued) {
        return;
    }

    update_queued = true;
    call_deferred([this](){ _update_texture();});
}

Ref<Image> NoiseTexture::_generate_texture() {

    // Prevent memdelete due to unref() on other thread.
    Ref<OpenSimplexNoise> ref_noise = noise;

    if (not ref_noise)
        return Ref<Image>();

    Ref<Image> image;

    if (seamless) {
        image = ref_noise->get_seamless_image(size.x);
    } else {
        image = ref_noise->get_image(size.x, size.y, noise_offset);
    }

    if (as_normalmap) {
        image->bumpmap_to_normalmap(bump_strength);
    }

    return image;
}

void NoiseTexture::_update_texture() {
    bool use_thread = true;
    if (first_time) {
        use_thread = false;
        first_time = false;
    }
    if (use_thread) {

        if (!noise_thread.is_started()) {
            noise_thread.start(_thread_function, this);
            regen_queued = false;
        } else {
            regen_queued = true;
        }

    } else {
        Ref<Image> image = _generate_texture();
        _set_texture_data(image);
    }
    update_queued = false;

}

void NoiseTexture::set_noise(Ref<OpenSimplexNoise> p_noise) {
    if (p_noise == noise)
        return;
    if (noise) {
        noise->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &NoiseTexture::_queue_update));
    }
    noise = p_noise;
    if (noise) {
        noise->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &NoiseTexture::_queue_update));
    }
    _queue_update();
}

Ref<OpenSimplexNoise> NoiseTexture::get_noise() {
    return noise;
}

void NoiseTexture::set_width(int p_width) {
    if (p_width == size.x) {
        return;
    }
    size.x = p_width;
    _queue_update();
}

void NoiseTexture::set_height(int p_height) {
    if (p_height == size.y) {
        return;
    }
    size.y = p_height;
    _queue_update();
}

void NoiseTexture::set_noise_offset(Vector2 p_noise_offset) {
    if (noise_offset == p_noise_offset) {
        return;
    }
    noise_offset = p_noise_offset;
    _queue_update();
}

void NoiseTexture::set_seamless(bool p_seamless) {
    if (p_seamless == seamless) {
        return;
    }
    seamless = p_seamless;
    _queue_update();
}

bool NoiseTexture::get_seamless() {
    return seamless;
}

void NoiseTexture::set_as_normalmap(bool p_as_normalmap) {
    if (p_as_normalmap == as_normalmap) return;
    as_normalmap = p_as_normalmap;
    _queue_update();
    Object_change_notify(this);
}

bool NoiseTexture::is_normalmap() {
    return as_normalmap;
}

void NoiseTexture::set_bump_strength(float p_bump_strength) {

    if (p_bump_strength == bump_strength) return;
    bump_strength = p_bump_strength;
    if (as_normalmap)
        _queue_update();
}

float NoiseTexture::get_bump_strength() {

    return bump_strength;
}

int NoiseTexture::get_width() const {

    return size.x;
}

int NoiseTexture::get_height() const {

    return size.y;
}

Vector2 NoiseTexture::get_noise_offset() const {
    return noise_offset;
}

void NoiseTexture::set_flags(uint32_t p_flags) {
    flags = p_flags;
    RenderingServer::get_singleton()->texture_set_flags(texture, flags);
}

uint32_t NoiseTexture::get_flags() const {
    return flags;
}

Ref<Image> NoiseTexture::get_data() const {

    return data;
}
