/*************************************************************************/
/*  audio_effect_reverb.cpp                                              */
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

#include "audio_effect_reverb.h"
#include "servers/audio_server.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AudioEffectReverbInstance)
IMPL_GDCLASS(AudioEffectReverb)

void AudioEffectReverbInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {

    for (int i = 0; i < 2; i++) {
        Reverb &r = reverb[i];

        r.set_predelay(base->predelay);
        r.set_predelay_feedback(base->predelay_fb);
        r.set_highpass(base->hpf);
        r.set_room_size(base->room_size);
        r.set_damp(base->damping);
        r.set_extra_spread(base->spread);
        r.set_wet(base->wet);
        r.set_dry(base->dry);
    }

    int todo = p_frame_count;
    int offset = 0;

    while (todo) {

        int to_mix = MIN(todo, Reverb::INPUT_BUFFER_MAX_SIZE);

        for (int j = 0; j < to_mix; j++) {
            tmp_src[j] = p_src_frames[offset + j].l;
        }

        reverb[0].process(tmp_src, tmp_dst, to_mix);

        for (int j = 0; j < to_mix; j++) {
            p_dst_frames[offset + j].l = tmp_dst[j];
            tmp_src[j] = p_src_frames[offset + j].r;
        }

        reverb[1].process(tmp_src, tmp_dst, to_mix);

        for (int j = 0; j < to_mix; j++) {
            p_dst_frames[offset + j].r = tmp_dst[j];
        }

        offset += to_mix;
        todo -= to_mix;
    }
}

AudioEffectReverbInstance::AudioEffectReverbInstance() {

    reverb[0].set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
    reverb[0].set_extra_spread_base(0);
    reverb[1].set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
    reverb[1].set_extra_spread_base(0.000521); //for stereo effect
}

Ref<AudioEffectInstance> AudioEffectReverb::instance() {
    Ref<AudioEffectReverbInstance> ins(make_ref_counted<AudioEffectReverbInstance>());
    ins->base = Ref<AudioEffectReverb>(this);
    return ins;
}

void AudioEffectReverb::set_predelay_msec(float p_msec) {

    predelay = p_msec;
}

void AudioEffectReverb::set_predelay_feedback(float p_feedback) {

    predelay_fb = CLAMP(p_feedback, 0.0f, 0.98f);
}
void AudioEffectReverb::set_room_size(float p_size) {

    room_size = p_size;
}
void AudioEffectReverb::set_damping(float p_damping) {

    damping = p_damping;
}
void AudioEffectReverb::set_spread(float p_spread) {

    spread = p_spread;
}

void AudioEffectReverb::set_dry(float p_dry) {

    dry = p_dry;
}
void AudioEffectReverb::set_wet(float p_wet) {

    wet = p_wet;
}
void AudioEffectReverb::set_hpf(float p_hpf) {

    hpf = p_hpf;
}

float AudioEffectReverb::get_predelay_msec() const {

    return predelay;
}
float AudioEffectReverb::get_predelay_feedback() const {

    return predelay_fb;
}
float AudioEffectReverb::get_room_size() const {

    return room_size;
}
float AudioEffectReverb::get_damping() const {

    return damping;
}
float AudioEffectReverb::get_spread() const {

    return spread;
}
float AudioEffectReverb::get_dry() const {

    return dry;
}
float AudioEffectReverb::get_wet() const {

    return wet;
}
float AudioEffectReverb::get_hpf() const {

    return hpf;
}

void AudioEffectReverb::_bind_methods() {

    SE_BIND_METHOD(AudioEffectReverb,set_predelay_msec);
    SE_BIND_METHOD(AudioEffectReverb,get_predelay_msec);

    SE_BIND_METHOD(AudioEffectReverb,set_predelay_feedback);
    SE_BIND_METHOD(AudioEffectReverb,get_predelay_feedback);

    SE_BIND_METHOD(AudioEffectReverb,set_room_size);
    SE_BIND_METHOD(AudioEffectReverb,get_room_size);

    SE_BIND_METHOD(AudioEffectReverb,set_damping);
    SE_BIND_METHOD(AudioEffectReverb,get_damping);

    SE_BIND_METHOD(AudioEffectReverb,set_spread);
    SE_BIND_METHOD(AudioEffectReverb,get_spread);

    SE_BIND_METHOD(AudioEffectReverb,set_dry);
    SE_BIND_METHOD(AudioEffectReverb,get_dry);

    SE_BIND_METHOD(AudioEffectReverb,set_wet);
    SE_BIND_METHOD(AudioEffectReverb,get_wet);

    SE_BIND_METHOD(AudioEffectReverb,set_hpf);
    SE_BIND_METHOD(AudioEffectReverb,get_hpf);

    ADD_GROUP("Predelay", "predelay_");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "predelay_msec", PropertyHint::Range, "20,500,1"), "set_predelay_msec", "get_predelay_msec");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "predelay_feedback", PropertyHint::Range, "0,0.98,0.01"), "set_predelay_feedback", "get_predelay_feedback");
    ADD_GROUP("", "");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "room_size", PropertyHint::Range, "0,1,0.01"), "set_room_size", "get_room_size");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "damping", PropertyHint::Range, "0,1,0.01"), "set_damping", "get_damping");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "spread", PropertyHint::Range, "0,1,0.01"), "set_spread", "get_spread");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "hipass", PropertyHint::Range, "0,1,0.01"), "set_hpf", "get_hpf");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "dry", PropertyHint::Range, "0,1,0.01"), "set_dry", "get_dry");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "wet", PropertyHint::Range, "0,1,0.01"), "set_wet", "get_wet");
}

AudioEffectReverb::AudioEffectReverb() {
    predelay = 150;
    predelay_fb = 0.4;
    hpf = 0;
    room_size = 0.8;
    damping = 0.5;
    spread = 1.0;
    dry = 1.0;
    wet = 0.5;
}
