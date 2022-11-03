#include "curve_texture.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/image_enum_casters.h"
#include "core/io/image_loader.h"
#include "core/io/image_saver.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/os.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(CurveTexture)

void CurveTexture::_bind_methods() {

    SE_BIND_METHOD(CurveTexture,set_width);

    SE_BIND_METHOD(CurveTexture,set_curve);
    SE_BIND_METHOD(CurveTexture,get_curve);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PropertyHint::Range, "1,4096"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "curve", PropertyHint::ResourceType, "Curve"), "set_curve", "get_curve");
}

void CurveTexture::set_width(int p_width) {

    ERR_FAIL_COND(p_width < 32 || p_width > 4096);
    _width = p_width;
    _update();
}

int CurveTexture::get_width() const {

    return _width;
}

void CurveTexture::ensure_default_setup(float p_min, float p_max) {
    if (not _curve) {
        Ref<Curve> curve(make_ref_counted<Curve>());
        curve->add_point(Vector2(0, 1));
        curve->add_point(Vector2(1, 1));
        curve->set_min_value(p_min);
        curve->set_max_value(p_max);
        set_curve(curve);
        // Min and max is 0..1 by default
    }
}

void CurveTexture::set_curve(const Ref<Curve>& p_curve) {
    if (_curve != p_curve) {
        if (_curve) {
            _curve->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &CurveTexture::_update));
        }
        _curve = p_curve;
        if (_curve) {
            _curve->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &CurveTexture::_update));
        }
        _update();
    }
}

void CurveTexture::_update() {

    PoolVector<uint8_t> data;
    data.resize(_width * sizeof(float));

    // The array is locked in that scope
    {
        PoolVector<uint8_t>::Write wd8 = data.write();
        float *wd = (float *)wd8.ptr();

        if (_curve) {
            Curve &curve = *_curve;
            for (int i = 0; i < _width; ++i) {
                float t = i / static_cast<float>(_width);
                wd[i] = curve.interpolate_baked(t);
            }

        } else {
            for (int i = 0; i < _width; ++i) {
                wd[i] = 0;
            }
        }
    }

    Ref<Image> image(make_ref_counted<Image>(_width, 1, false, ImageData::FORMAT_RF, data));

    RenderingServer::get_singleton()->texture_allocate(_texture, _width, 1, 0, ImageData::FORMAT_RF, RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER);
    RenderingServer::get_singleton()->texture_set_data(_texture, image);

    emit_changed();
}

Ref<Curve> CurveTexture::get_curve() const {

    return _curve;
}

RenderingEntity CurveTexture::get_rid() const {

    return _texture;
}

CurveTexture::CurveTexture() {
    _width = 2048;
    _texture = RenderingServer::get_singleton()->texture_create();
}
CurveTexture::~CurveTexture() {
    RenderingServer::get_singleton()->free_rid(_texture);
}
