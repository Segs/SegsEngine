#include "camera_texture.h"

#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "servers/camera/camera_feed.h"
#include "servers/camera_server_enum_casters.h"

IMPL_GDCLASS(CameraTexture)

void CameraTexture::_bind_methods() {
    SE_BIND_METHOD(CameraTexture,set_camera_feed_id);
    SE_BIND_METHOD(CameraTexture,get_camera_feed_id);

    SE_BIND_METHOD(CameraTexture,set_which_feed);
    SE_BIND_METHOD(CameraTexture,get_which_feed);

    SE_BIND_METHOD(CameraTexture,set_camera_active);
    SE_BIND_METHOD(CameraTexture,get_camera_active);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "camera_feed_id"), "set_camera_feed_id", "get_camera_feed_id");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "which_feed"), "set_which_feed", "get_which_feed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "camera_is_active"), "set_camera_active", "get_camera_active");
}



int CameraTexture::get_height() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_base_height();
    } else {
        return 0;
    }
}

bool CameraTexture::has_alpha() const {
    return false;
}

RenderingEntity CameraTexture::get_rid() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_texture(which_feed);
    } else {
        return entt::null;
    }
}

void CameraTexture::set_flags(uint32_t p_flags) {
    // not supported
}

uint32_t CameraTexture::get_flags() const {
    // not supported
    return 0;
}

Ref<Image> CameraTexture::get_data() const {
    // not (yet) supported
    return Ref<Image>();
}

void CameraTexture::set_camera_feed_id(int p_new_id) {
    camera_feed_id = p_new_id;
    Object_change_notify(this);
}

int CameraTexture::get_camera_feed_id() const {
    return camera_feed_id;
}

void CameraTexture::set_which_feed(CameraServer::FeedImage p_which) {
    which_feed = p_which;
    Object_change_notify(this);
}

CameraServer::FeedImage CameraTexture::get_which_feed() const {
    return which_feed;
}

void CameraTexture::set_camera_active(bool p_active) {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        feed->set_active(p_active);
        Object_change_notify(this);
    }
}

bool CameraTexture::get_camera_active() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->is_active();
    } else {
        return false;
    }
}

CameraTexture::CameraTexture() {
    camera_feed_id = 0;
    which_feed = CameraServer::FEED_RGBA_IMAGE;
}

CameraTexture::~CameraTexture() {
    // nothing to do here yet
}

int CameraTexture::get_width() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_base_width();
    } else {
        return 0;
    }
}
