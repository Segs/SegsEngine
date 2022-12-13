/*************************************************************************/
/*  arvr_interface_gdnative.h                                            */
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

#ifndef ARVR_INTERFACE_GDNATIVE_H
#define ARVR_INTERFACE_GDNATIVE_H

#include "modules/gdnative/gdnative.h"
#include "servers/arvr/arvr_interface.h"

/**
    @authors Hinsbart & Karroffel & Mux213

    This subclass of our AR/VR interface forms a bridge to GDNative.
*/

class ARVRInterfaceGDNative : public ARVRInterface {
    GDCLASS(ARVRInterfaceGDNative,ARVRInterface)

    void cleanup();

protected:
    const godot_arvr_interface_gdnative *interface;
    void *data;

    static void _bind_methods();

public:
    /** general interface information **/
    ARVRInterfaceGDNative();
    ~ARVRInterfaceGDNative() override;

    void set_interface(const godot_arvr_interface_gdnative *p_interface);

    StringName get_name() const override;
    int get_capabilities() const override;

    bool is_initialized() const override;
    bool initialize() override;
    void uninitialize() override;

    /** specific to AR **/
    bool get_anchor_detection_is_enabled() const override;
    void set_anchor_detection_is_enabled(bool p_enable) override;
    int get_camera_feed_id() override;

    /** rendering and internal **/
    Size2 get_render_targetsize() override;
    bool is_stereo() override;
    Transform get_transform_for_eye(ARVREyes p_eye, const Transform &p_cam_transform) override;

    // we expose a PoolVector<float> version of this function to GDNative
    PoolVector<float> _get_projection_for_eye(ARVREyes p_eye, real_t p_aspect, real_t p_z_near, real_t p_z_far);

    // and a CameraMatrix version to ARVRServer
    CameraMatrix get_projection_for_eye(ARVREyes p_eye, real_t p_aspect, real_t p_z_near, real_t p_z_far) override;

    unsigned int get_external_texture_for_eye(ARVREyes p_eye) override;
    void commit_for_eye(ARVREyes p_eye, RID p_render_target, const Rect2 &p_screen_rect) override;

    void process() override;
    void notification(int p_what) override;
};

#endif // ARVR_INTERFACE_GDNATIVE_H
