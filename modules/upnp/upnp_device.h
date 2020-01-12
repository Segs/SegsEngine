/*************************************************************************/
/*  upnp_device.h                                                        */
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

#include "core/reference.h"
#include "core/method_arg_casters.h"
#include "core/method_enum_caster.h"

class UPNPDevice : public RefCounted {

    GDCLASS(UPNPDevice,RefCounted)

public:
    enum IGDStatus {

        IGD_STATUS_OK,
        IGD_STATUS_HTTP_ERROR,
        IGD_STATUS_HTTP_EMPTY,
        IGD_STATUS_NO_URLS,
        IGD_STATUS_NO_IGD,
        IGD_STATUS_DISCONNECTED,
        IGD_STATUS_UNKNOWN_DEVICE,
        IGD_STATUS_INVALID_CONTROL,
        IGD_STATUS_MALLOC_ERROR,
        IGD_STATUS_UNKNOWN_ERROR,
    };

    void set_description_url(se_string url);
    const se_string &get_description_url() const;

    void set_service_type(se_string type);
    const se_string &get_service_type() const;

    void set_igd_control_url(se_string url);
    const se_string &get_igd_control_url() const;

    void set_igd_service_type(se_string type);
    const se_string &get_igd_service_type() const;

    void set_igd_our_addr(se_string addr);
    const se_string &get_igd_our_addr() const;

    void set_igd_status(IGDStatus status);
    IGDStatus get_igd_status() const;

    bool is_valid_gateway() const;
    se_string query_external_address() const;
    int add_port_mapping(int port, int port_internal = 0, const se_string &desc = {}, const se_string &proto = "UDP", int duration = 0) const;
    int delete_port_mapping(int port, se_string_view proto = "UDP") const;

    UPNPDevice();
    ~UPNPDevice() override;

protected:
    static void _bind_methods();

private:
    se_string description_url;
    se_string service_type;
    se_string igd_control_url;
    se_string igd_service_type;
    se_string igd_our_addr;
    IGDStatus igd_status;
};


