/*************************************************************************/
/*  sky.cpp                                                              */
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

#include "sky.h"

#include "core/io/image_loader.h"
#include "core/math/basis.h"
#include "core/method_bind.h"
#include "core/os/thread.h"
#include "servers/rendering_server.h"
#include "scene/resources/texture.h"


IMPL_GDCLASS(Sky)
IMPL_GDCLASS(PanoramaSky)
IMPL_GDCLASS(ProceduralSky)

VARIANT_ENUM_CAST(Sky::RadianceSize);
VARIANT_ENUM_CAST(ProceduralSky::TextureSize);

void Sky::set_radiance_size(RadianceSize p_size) {
    ERR_FAIL_INDEX(p_size, RADIANCE_SIZE_MAX);

    radiance_size = p_size;
    _radiance_changed();
}

Sky::RadianceSize Sky::get_radiance_size() const {

    return radiance_size;
}

void Sky::_bind_methods() {

    SE_BIND_METHOD(Sky,set_radiance_size);
    SE_BIND_METHOD(Sky,get_radiance_size);

    // Don't expose 1024 and 2048 in the property hint as these sizes will cause GPU hangs on many systems.
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "radiance_size", PropertyHint::Enum, "32,64,128,256,512"), "set_radiance_size", "get_radiance_size");

    BIND_ENUM_CONSTANT(RADIANCE_SIZE_32);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_64);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_128);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_256);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_512);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_1024);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_2048);
    BIND_ENUM_CONSTANT(RADIANCE_SIZE_MAX);
}

Sky::Sky() {
    radiance_size = RADIANCE_SIZE_128;
}

/////////////////////////////////////////

void PanoramaSky::_radiance_changed() {

    if (panorama) {
        static const int size[RADIANCE_SIZE_MAX] = {
            32, 64, 128, 256, 512, 1024, 2048
        };
        RenderingServer::get_singleton()->sky_set_texture(sky, panorama->get_rid(), size[get_radiance_size()]);
    }
}

void PanoramaSky::set_panorama(const Ref<Texture> &p_panorama) {

    panorama = p_panorama;

    if (panorama) {

        _radiance_changed();

    } else {
        RenderingServer::get_singleton()->sky_set_texture(sky, entt::null, 0);
    }
}

Ref<Texture> PanoramaSky::get_panorama() const {

    return panorama;
}

RenderingEntity PanoramaSky::get_rid() const {

    return sky;
}

void PanoramaSky::_bind_methods() {

    SE_BIND_METHOD(PanoramaSky,set_panorama);
    SE_BIND_METHOD(PanoramaSky,get_panorama);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "panorama", PropertyHint::ResourceType, "Texture"), "set_panorama", "get_panorama");
}

PanoramaSky::PanoramaSky() {

    sky = RenderingServer::get_singleton()->sky_create();
}

PanoramaSky::~PanoramaSky() {

    RenderingServer::get_singleton()->free_rid(sky);
}
//////////////////////////////////

void ProceduralSky::_radiance_changed() {

    if (update_queued)
        return; //do nothing yet

    static const int size[RADIANCE_SIZE_MAX] = {
        32, 64, 128, 256, 512, 1024, 2048
    };
    RenderingServer::get_singleton()->sky_set_texture(sky, texture, size[get_radiance_size()]);
}

Ref<Image> ProceduralSky::_generate_sky() {

    update_queued = false;

    PoolVector<uint8_t> imgdata;

    static const int size[TEXTURE_SIZE_MAX] = {
        256, 512, 1024, 2048, 4096
    };

    int w = size[texture_size];
    int h = w / 2;

    imgdata.resize(w * h * 4); //RGBE

    {
        PoolVector<uint8_t>::Write dataw = imgdata.write();

        uint32_t *ptr = (uint32_t *)dataw.ptr();

        Color sky_top_linear = sky_top_color.to_linear();
        Color sky_horizon_linear = sky_horizon_color.to_linear();

        Color ground_bottom_linear = ground_bottom_color.to_linear();
        Color ground_horizon_linear = ground_horizon_color.to_linear();

        Color sun_linear;
        sun_linear.r = sun_color.r * sun_energy;
        sun_linear.g = sun_color.g * sun_energy;
        sun_linear.b = sun_color.b * sun_energy;

        Vector3 sun(0, 0, -1);

        sun = Basis(Vector3(1, 0, 0), Math::deg2rad(sun_latitude)).xform(sun);
        sun = Basis(Vector3(0, 1, 0), Math::deg2rad(sun_longitude)).xform(sun);

        sun.normalize();

        for (int i = 0; i < w; i++) {

            float u = float(i) / (w - 1);
            float phi = u * 2.0f * Math_PI;

            for (int j = 0; j < h; j++) {

                float v = float(j) / (h - 1);
                float theta = v * Math_PI;

                Vector3 normal(
                        Math::sin(phi) * Math::sin(theta) * -1.0f,
                        Math::cos(theta),
                        Math::cos(phi) * Math::sin(theta) * -1.0f);

                normal.normalize();

                float v_angle = Math::acos(CLAMP(normal.y, -1.0f, 1.0f));

                Color color;

                if (normal.y < 0) {
                    //ground

                    float c = (v_angle - (Math_PI * 0.5f)) / (Math_PI * 0.5f);
                    color = ground_horizon_linear.linear_interpolate(ground_bottom_linear, Math::ease(c, ground_curve));
                    color.r *= ground_energy;
                    color.g *= ground_energy;
                    color.b *= ground_energy;
                } else {
                    float c = v_angle / (Math_PI * 0.5f);
                    color = sky_horizon_linear.linear_interpolate(sky_top_linear, Math::ease(1.0f - c, sky_curve));
                    color.r *= sky_energy;
                    color.g *= sky_energy;
                    color.b *= sky_energy;

                    float sun_angle = Math::rad2deg(Math::acos(CLAMP(sun.dot(normal), -1.0f, 1.0f)));

                    if (sun_angle < sun_angle_min) {
                        color = color.blend(sun_linear);
                    } else if (sun_angle < sun_angle_max) {

                        float c2 = (sun_angle - sun_angle_min) / (sun_angle_max - sun_angle_min);
                        c2 = Math::ease(c2, sun_curve);

                        color = color.blend(sun_linear).linear_interpolate(color, c2);
                    }
                }

                ptr[j * w + i] = color.to_rgbe9995();
            }
        }
    }

    Ref<Image> image(make_ref_counted<Image>());
    image->create(w, h, false, ImageData::FORMAT_RGBE9995, imgdata);

    return image;
}

void ProceduralSky::set_sky_top_color(const Color &p_sky_top) {

    sky_top_color = p_sky_top;
    _queue_update();
}

Color ProceduralSky::get_sky_top_color() const {

    return sky_top_color;
}

void ProceduralSky::set_sky_horizon_color(const Color &p_sky_horizon) {

    sky_horizon_color = p_sky_horizon;
    _queue_update();
}
Color ProceduralSky::get_sky_horizon_color() const {

    return sky_horizon_color;
}

void ProceduralSky::set_sky_curve(float p_curve) {

    sky_curve = p_curve;
    _queue_update();
}
float ProceduralSky::get_sky_curve() const {

    return sky_curve;
}

void ProceduralSky::set_sky_energy(float p_energy) {

    sky_energy = p_energy;
    _queue_update();
}
float ProceduralSky::get_sky_energy() const {

    return sky_energy;
}

void ProceduralSky::set_ground_bottom_color(const Color &p_ground_bottom) {

    ground_bottom_color = p_ground_bottom;
    _queue_update();
}
Color ProceduralSky::get_ground_bottom_color() const {

    return ground_bottom_color;
}

void ProceduralSky::set_ground_horizon_color(const Color &p_ground_horizon) {

    ground_horizon_color = p_ground_horizon;
    _queue_update();
}
Color ProceduralSky::get_ground_horizon_color() const {

    return ground_horizon_color;
}

void ProceduralSky::set_ground_curve(float p_curve) {

    ground_curve = p_curve;
    _queue_update();
}
float ProceduralSky::get_ground_curve() const {

    return ground_curve;
}

void ProceduralSky::set_ground_energy(float p_energy) {

    ground_energy = p_energy;
    _queue_update();
}
float ProceduralSky::get_ground_energy() const {

    return ground_energy;
}

void ProceduralSky::set_sun_color(const Color &p_sun) {

    sun_color = p_sun;
    _queue_update();
}
Color ProceduralSky::get_sun_color() const {

    return sun_color;
}

void ProceduralSky::set_sun_latitude(float p_angle) {

    sun_latitude = p_angle;
    _queue_update();
}
float ProceduralSky::get_sun_latitude() const {

    return sun_latitude;
}

void ProceduralSky::set_sun_longitude(float p_angle) {

    sun_longitude = p_angle;
    _queue_update();
}
float ProceduralSky::get_sun_longitude() const {

    return sun_longitude;
}

void ProceduralSky::set_sun_angle_min(float p_angle) {

    sun_angle_min = p_angle;
    _queue_update();
}
float ProceduralSky::get_sun_angle_min() const {

    return sun_angle_min;
}

void ProceduralSky::set_sun_angle_max(float p_angle) {

    sun_angle_max = p_angle;
    _queue_update();
}
float ProceduralSky::get_sun_angle_max() const {

    return sun_angle_max;
}

void ProceduralSky::set_sun_curve(float p_curve) {

    sun_curve = p_curve;
    _queue_update();
}
float ProceduralSky::get_sun_curve() const {

    return sun_curve;
}

void ProceduralSky::set_sun_energy(float p_energy) {

    sun_energy = p_energy;
    _queue_update();
}
float ProceduralSky::get_sun_energy() const {

    return sun_energy;
}

void ProceduralSky::set_texture_size(TextureSize p_size) {
    ERR_FAIL_INDEX(p_size, TEXTURE_SIZE_MAX);

    texture_size = p_size;
    _queue_update();
}
ProceduralSky::TextureSize ProceduralSky::get_texture_size() const {
    return texture_size;
}

Ref<Image> ProceduralSky::get_data() const {
    return static_ref_cast<Image>(panorama->duplicate());
}

RenderingEntity ProceduralSky::get_rid() const {
    return sky;
}

void ProceduralSky::_update_sky() {

    bool use_thread = true;
    if (first_time) {
        use_thread = false;
        first_time = false;
    }
    if (use_thread) {

        if (!sky_thread.is_started()) {
            sky_thread.start(_thread_function, this);
            regen_queued = false;
        } else {
            regen_queued = true;
        }

    } else {
        panorama = _generate_sky();
        RenderingServer::get_singleton()->texture_allocate(texture, panorama->get_width(), panorama->get_height(), 0,
                ImageData::FORMAT_RGBE9995, RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER | RS::TEXTURE_FLAG_REPEAT);
        RenderingServer::get_singleton()->texture_set_data(texture, panorama);
        _radiance_changed();
    }
}

void ProceduralSky::_queue_update() {

    if (update_queued)
        return;

    update_queued = true;
    call_deferred([this](){ _update_sky();});

}

void ProceduralSky::_thread_done(const Ref<Image> &p_image) {
    ERR_FAIL_COND(!p_image);

    panorama = p_image;
    RenderingServer::get_singleton()->texture_allocate(texture, panorama->get_width(), panorama->get_height(), 0,
            ImageData::FORMAT_RGBE9995, RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER | RS::TEXTURE_FLAG_REPEAT);
    RenderingServer::get_singleton()->texture_set_data(texture, panorama);
    _radiance_changed();
    sky_thread.wait_to_finish();
    if (regen_queued) {
        sky_thread.start(_thread_function, this);
        regen_queued = false;
    }
}

void ProceduralSky::_thread_function(void *p_ud) {

    ProceduralSky *psky = (ProceduralSky *)p_ud;
    psky->call_deferred([psky,gen=psky->_generate_sky()]() { psky->_thread_done(gen); });
}

void ProceduralSky::_bind_methods() {

    SE_BIND_METHOD(ProceduralSky,set_sky_top_color);
    SE_BIND_METHOD(ProceduralSky,get_sky_top_color);

    SE_BIND_METHOD(ProceduralSky,set_sky_horizon_color);
    SE_BIND_METHOD(ProceduralSky,get_sky_horizon_color);

    SE_BIND_METHOD(ProceduralSky,set_sky_curve);
    SE_BIND_METHOD(ProceduralSky,get_sky_curve);

    SE_BIND_METHOD(ProceduralSky,set_sky_energy);
    SE_BIND_METHOD(ProceduralSky,get_sky_energy);

    SE_BIND_METHOD(ProceduralSky,set_ground_bottom_color);
    SE_BIND_METHOD(ProceduralSky,get_ground_bottom_color);

    SE_BIND_METHOD(ProceduralSky,set_ground_horizon_color);
    SE_BIND_METHOD(ProceduralSky,get_ground_horizon_color);

    SE_BIND_METHOD(ProceduralSky,set_ground_curve);
    SE_BIND_METHOD(ProceduralSky,get_ground_curve);

    SE_BIND_METHOD(ProceduralSky,set_ground_energy);
    SE_BIND_METHOD(ProceduralSky,get_ground_energy);

    SE_BIND_METHOD(ProceduralSky,set_sun_color);
    SE_BIND_METHOD(ProceduralSky,get_sun_color);

    SE_BIND_METHOD(ProceduralSky,set_sun_latitude);
    SE_BIND_METHOD(ProceduralSky,get_sun_latitude);

    SE_BIND_METHOD(ProceduralSky,set_sun_longitude);
    SE_BIND_METHOD(ProceduralSky,get_sun_longitude);

    SE_BIND_METHOD(ProceduralSky,set_sun_angle_min);
    SE_BIND_METHOD(ProceduralSky,get_sun_angle_min);

    SE_BIND_METHOD(ProceduralSky,set_sun_angle_max);
    SE_BIND_METHOD(ProceduralSky,get_sun_angle_max);

    SE_BIND_METHOD(ProceduralSky,set_sun_curve);
    SE_BIND_METHOD(ProceduralSky,get_sun_curve);

    SE_BIND_METHOD(ProceduralSky,set_sun_energy);
    SE_BIND_METHOD(ProceduralSky,get_sun_energy);

    SE_BIND_METHOD(ProceduralSky,set_texture_size);
    SE_BIND_METHOD(ProceduralSky,get_texture_size);

    ADD_GROUP("Sky", "sky_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "sky_top_color"), "set_sky_top_color", "get_sky_top_color");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "sky_horizon_color"), "set_sky_horizon_color", "get_sky_horizon_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sky_curve", PropertyHint::ExpEasing), "set_sky_curve", "get_sky_curve");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sky_energy", PropertyHint::Range, "0,64,0.01"), "set_sky_energy", "get_sky_energy");

    ADD_GROUP("Ground", "ground_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "ground_bottom_color"), "set_ground_bottom_color", "get_ground_bottom_color");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "ground_horizon_color"), "set_ground_horizon_color", "get_ground_horizon_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "ground_curve", PropertyHint::ExpEasing), "set_ground_curve", "get_ground_curve");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "ground_energy", PropertyHint::Range, "0,64,0.01"), "set_ground_energy", "get_ground_energy");

    ADD_GROUP("Sun", "sun_");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "sun_color"), "set_sun_color", "get_sun_color");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_latitude", PropertyHint::Range, "-180,180,0.01"), "set_sun_latitude", "get_sun_latitude");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_longitude", PropertyHint::Range, "-180,180,0.01"), "set_sun_longitude", "get_sun_longitude");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_angle_min", PropertyHint::Range, "0,360,0.01"), "set_sun_angle_min", "get_sun_angle_min");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_angle_max", PropertyHint::Range, "0,360,0.01"), "set_sun_angle_max", "get_sun_angle_max");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_curve", PropertyHint::ExpEasing), "set_sun_curve", "get_sun_curve");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "sun_energy", PropertyHint::Range, "0,64,0.01"), "set_sun_energy", "get_sun_energy");

    ADD_GROUP("Texture", "texture_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "texture_size", PropertyHint::Enum, "256,512,1024,2048,4096"), "set_texture_size", "get_texture_size");

    BIND_ENUM_CONSTANT(TEXTURE_SIZE_256);
    BIND_ENUM_CONSTANT(TEXTURE_SIZE_512);
    BIND_ENUM_CONSTANT(TEXTURE_SIZE_1024);
    BIND_ENUM_CONSTANT(TEXTURE_SIZE_2048);
    BIND_ENUM_CONSTANT(TEXTURE_SIZE_4096);
    BIND_ENUM_CONSTANT(TEXTURE_SIZE_MAX);
}

ProceduralSky::ProceduralSky(bool p_desaturate) {

    sky = RenderingServer::get_singleton()->sky_create();
    texture = RenderingServer::get_singleton()->texture_create();

    update_queued = false;
    sky_top_color = Color::hex(0xa5d6f1ff);
    sky_horizon_color = Color::hex(0xd6eafaff);
    sky_curve = 0.09f;
    sky_energy = 1;

    ground_bottom_color = Color::hex(0x282f36ff);
    ground_horizon_color = Color::hex(0x6c655fff);
    ground_curve = 0.02f;
    ground_energy = 1;

    if (p_desaturate) {
        sky_top_color.set_hsv(sky_top_color.get_h(), 0, sky_top_color.get_v());
        sky_horizon_color.set_hsv(sky_horizon_color.get_h(), 0, sky_horizon_color.get_v());
        ground_bottom_color.set_hsv(ground_bottom_color.get_h(), 0, ground_bottom_color.get_v());
        ground_horizon_color.set_hsv(ground_horizon_color.get_h(), 0, ground_horizon_color.get_v());
    }
    sun_color = Color(1, 1, 1);
    sun_latitude = 35;
    sun_longitude = 0;
    sun_angle_min = 1;
    sun_angle_max = 100;
    sun_curve = 0.05f;
    sun_energy = 1;

    texture_size = TEXTURE_SIZE_1024;
    regen_queued = false;
    first_time = true;

    _queue_update();
}

ProceduralSky::~ProceduralSky() {

    if (sky_thread.is_started()) {
        sky_thread.wait_to_finish();
    }
    RenderingServer::get_singleton()->free_rid(sky);
    RenderingServer::get_singleton()->free_rid(texture);
}
