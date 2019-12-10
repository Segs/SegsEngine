/*************************************************************************/
/*  audio_driver_pulseaudio.h                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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
#ifndef PULSEAUDIO_ENABLED
#ewew
#endif
#ifdef PULSEAUDIO_ENABLED


#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "servers/audio_server.h"

#include <pulse/pulseaudio.h>

class AudioDriverPulseAudio : public AudioDriver {

    Thread *thread=nullptr;
    Mutex *mutex=nullptr;

    pa_mainloop *pa_ml=nullptr;
    pa_context *pa_ctx=nullptr;
    pa_stream *pa_str=nullptr;
    pa_stream *pa_rec_str=nullptr;
    pa_channel_map pa_map;
    pa_channel_map pa_rec_map;

    se_string device_name;
    se_string new_device;
    se_string default_device;

    se_string capture_device_name;
    se_string capture_new_device;
    se_string capture_default_device;

    Vector<int32_t> samples_in;
    Vector<int16_t> samples_out;

    unsigned int mix_rate=0;
    unsigned int buffer_frames=0;
    unsigned int pa_buffer_size=0;
    int channels=0;
    int pa_ready=0;
    int pa_status=0;
    Array pa_devices;
    Array pa_rec_devices;

    bool active=false;
    bool thread_exited=false;
    mutable bool exit_thread=false;

    float latency=0.0f;

    static void pa_state_cb(pa_context *c, void *userdata);
    static void pa_sink_info_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
    static void pa_source_info_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
    static void pa_server_info_cb(pa_context *c, const pa_server_info *i, void *userdata);
    static void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
    static void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);

    Error init_device();
    void finish_device();

    Error capture_init_device();
    void capture_finish_device();

    void detect_channels(bool capture = false);

    static void thread_func(void *p_udata);

public:
    const char *get_name() const override {
        return "PulseAudio";
    }

    Error init() override;
    void start() override;
    int get_mix_rate() const override;
    SpeakerMode get_speaker_mode() const override;

    Array get_device_list() override;
    se_string_view get_device() override;
    void set_device(se_string_view device) override;

    Array capture_get_device_list() override;
    void capture_set_device(se_string_view p_name) override;
    se_string capture_get_device() override;

    void lock() override;
    void unlock() override;
    void finish() override;

    float get_latency() override;

    Error capture_start() override;
    Error capture_stop() override;

    AudioDriverPulseAudio();
    ~AudioDriverPulseAudio() override;
};
#endif // PULSEAUDIO_ENABLED
