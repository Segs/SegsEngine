/*************************************************************************/
/*  audio_driver_alsa.h                                                  */
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

#include "servers/audio_server.h"

#ifdef ALSA_ENABLED

#include "core/os/thread.h"

#include <alsa/asoundlib.h>

class GODOT_EXPORT AudioDriverALSA : public AudioDriver {

    Thread *thread=nullptr;
    Mutex *mutex=nullptr;

    snd_pcm_t *pcm_handle=nullptr;

    se_string device_name;
    se_string new_device;

    PODVector<int32_t> samples_in;
    PODVector<int16_t> samples_out;

    Error init_device();
    void finish_device();

    static void thread_func(void *p_udata);

    unsigned int mix_rate;
    SpeakerMode speaker_mode;

    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    int channels;

    bool active;
    bool thread_exited;
    mutable bool exit_thread;

public:
    const char *get_name() const override {
        return "ALSA";
    }

    Error init() override;
    void start() override;
    int get_mix_rate() const override;
    SpeakerMode get_speaker_mode() const override;
    Array get_device_list() override;
    se_string_view get_device() override;
    void set_device(se_string_view device) override;
    void lock() override;
    void unlock() override;
    void finish() override;

    AudioDriverALSA();
    ~AudioDriverALSA() override;
};

#endif
