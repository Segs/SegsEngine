/*************************************************************************/
/*  audio_effect_stereo_enhance.cpp                                      */
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

#include "audio_effect_stereo_enhance.h"
#include "servers/audio_server.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AudioEffectStereoEnhanceInstance)
IMPL_GDCLASS(AudioEffectStereoEnhance)

void AudioEffectStereoEnhanceInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {

    float intensity = base->pan_pullout;
    bool surround_mode = base->surround > 0;
    float surround_amount = base->surround;
    unsigned int delay_frames = (base->time_pullout / 1000.0) * AudioServer::get_singleton()->get_mix_rate();

    for (int i = 0; i < p_frame_count; i++) {

        float l = p_src_frames[i].l;
        float r = p_src_frames[i].r;

        float center = (l + r) / 2.0f;

        l = (center + (l - center) * intensity);
        r = (center + (r - center) * intensity);

        if (surround_mode) {

            float val = (l + r) / 2.0;

            delay_ringbuff[ringbuff_pos & ringbuff_mask] = val;

            float out = delay_ringbuff[(ringbuff_pos - delay_frames) & ringbuff_mask] * surround_amount;

            l += out;
            r += -out;
        } else {

            float val = r;

            delay_ringbuff[ringbuff_pos & ringbuff_mask] = val;

            //r is delayed
            r = delay_ringbuff[(ringbuff_pos - delay_frames) & ringbuff_mask];
            ;
        }

        p_dst_frames[i].l = l;
        p_dst_frames[i].r = r;
        ringbuff_pos++;
    }
}

AudioEffectStereoEnhanceInstance::~AudioEffectStereoEnhanceInstance() {

    memdelete_arr(delay_ringbuff);
}

Ref<AudioEffectInstance> AudioEffectStereoEnhance::instance() {
    Ref<AudioEffectStereoEnhanceInstance> ins(make_ref_counted<AudioEffectStereoEnhanceInstance>());

    ins->base = Ref<AudioEffectStereoEnhance>(this);

    float ring_buffer_max_size = AudioEffectStereoEnhanceInstance::MAX_DELAY_MS + 2;
    ring_buffer_max_size /= 1000.0; //convert to seconds
    ring_buffer_max_size *= AudioServer::get_singleton()->get_mix_rate();

    int ringbuff_size = (int)ring_buffer_max_size;

    int bits = 0;

    while (ringbuff_size > 0) {
        bits++;
        ringbuff_size /= 2;
    }

    ringbuff_size = 1 << bits;
    ins->ringbuff_mask = ringbuff_size - 1;
    ins->ringbuff_pos = 0;

    ins->delay_ringbuff = memnew_arr(float, ringbuff_size);

    return ins;
}

void AudioEffectStereoEnhance::set_pan_pullout(float p_amount) {

    pan_pullout = p_amount;
}

float AudioEffectStereoEnhance::get_pan_pullout() const {

    return pan_pullout;
}

void AudioEffectStereoEnhance::set_time_pullout(float p_amount) {

    time_pullout = p_amount;
}
float AudioEffectStereoEnhance::get_time_pullout() const {

    return time_pullout;
}

void AudioEffectStereoEnhance::set_surround(float p_amount) {

    surround = p_amount;
}
float AudioEffectStereoEnhance::get_surround() const {

    return surround;
}

void AudioEffectStereoEnhance::_bind_methods() {

    BIND_METHOD(AudioEffectStereoEnhance,set_pan_pullout);
    BIND_METHOD(AudioEffectStereoEnhance,get_pan_pullout);

    BIND_METHOD(AudioEffectStereoEnhance,set_time_pullout);
    BIND_METHOD(AudioEffectStereoEnhance,get_time_pullout);

    BIND_METHOD(AudioEffectStereoEnhance,set_surround);
    BIND_METHOD(AudioEffectStereoEnhance,get_surround);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pan_pullout", PropertyHint::Range, "0,4,0.01"), "set_pan_pullout", "get_pan_pullout");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "time_pullout_ms", PropertyHint::Range, "0,50,0.01"), "set_time_pullout", "get_time_pullout");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "surround", PropertyHint::Range, "0,1,0.01"), "set_surround", "get_surround");
}

AudioEffectStereoEnhance::AudioEffectStereoEnhance() {
    pan_pullout = 1;
    time_pullout = 0;
    surround = 0;
}
