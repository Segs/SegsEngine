#pragma once
#include "scene/resources/texture.h"
#include "servers/camera_server.h"

class GODOT_EXPORT CameraTexture : public Texture {
    GDCLASS(CameraTexture,Texture)
private:
    int camera_feed_id;
    CameraServer::FeedImage which_feed;

protected:
    static void _bind_methods();

public:
    int get_width() const override;
    int get_height() const override;
    RenderingEntity get_rid() const override;
    bool has_alpha() const override;

    void set_flags(uint32_t p_flags) override;
    uint32_t get_flags() const override;

    Ref<Image> get_data() const override;

    void set_camera_feed_id(int p_new_id);
    int get_camera_feed_id() const;

    void set_which_feed(CameraServer::FeedImage p_which);
    CameraServer::FeedImage get_which_feed() const;

    void set_camera_active(bool p_active);
    bool get_camera_active() const;

    CameraTexture();
    ~CameraTexture() override;
};
