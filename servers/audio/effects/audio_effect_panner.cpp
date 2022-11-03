/*************************************************************************/
/*  audio_effect_panner.cpp                                              */
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

#include "audio_effect_panner.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AudioEffectPannerInstance)
IMPL_GDCLASS(AudioEffectPanner)

void AudioEffectPannerInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {

    float lvol = CLAMP(1.0f - base->pan, 0.0f, 1.0f);
    float rvol = CLAMP(1.0f + base->pan, 0.0f, 1.0f);

    for (int i = 0; i < p_frame_count; i++) {

        p_dst_frames[i].l = p_src_frames[i].l * lvol + p_src_frames[i].r * (1.0f - rvol);
        p_dst_frames[i].r = p_src_frames[i].r * rvol + p_src_frames[i].l * (1.0f - lvol);
    }
}

Ref<AudioEffectInstance> AudioEffectPanner::instance() {
    Ref<AudioEffectPannerInstance> ins(make_ref_counted<AudioEffectPannerInstance>());
    ins->base = Ref<AudioEffectPanner>(this);
    return ins;
}

void AudioEffectPanner::set_pan(float p_cpanume) {
    pan = p_cpanume;
}

float AudioEffectPanner::get_pan() const {

    return pan;
}

void AudioEffectPanner::_bind_methods() {

    SE_BIND_METHOD(AudioEffectPanner,set_pan);
    SE_BIND_METHOD(AudioEffectPanner,get_pan);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pan", PropertyHint::Range, "-1,1,0.01"), "set_pan", "get_pan");
}

AudioEffectPanner::AudioEffectPanner() {
    pan = 0;
}
