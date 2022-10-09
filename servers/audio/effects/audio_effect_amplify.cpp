/*************************************************************************/
/*  audio_effect_amplify.cpp                                             */
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

#include "audio_effect_amplify.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AudioEffectAmplifyInstance)
IMPL_GDCLASS(AudioEffectAmplify)

void AudioEffectAmplifyInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {

    //multiply volume interpolating to avoid clicks if this changes
    float volume_db = base->volume_db;
    float vol = Math::db2linear(mix_volume_db);
    float vol_inc = (Math::db2linear(volume_db) - vol) / float(p_frame_count);

    for (int i = 0; i < p_frame_count; i++) {
        p_dst_frames[i] = p_src_frames[i] * vol;
        vol += vol_inc;
    }
    //set volume for next mix
    mix_volume_db = volume_db;
}

Ref<AudioEffectInstance> AudioEffectAmplify::instance() {
    Ref<AudioEffectAmplifyInstance> ins(make_ref_counted<AudioEffectAmplifyInstance>());
    ins->base = Ref<AudioEffectAmplify>(this);
    ins->mix_volume_db = volume_db;
    return ins;
}

void AudioEffectAmplify::set_volume_db(float p_volume) {
    volume_db = p_volume;
}

float AudioEffectAmplify::get_volume_db() const {

    return volume_db;
}

void AudioEffectAmplify::_bind_methods() {

    BIND_METHOD(AudioEffectAmplify,set_volume_db);
    BIND_METHOD(AudioEffectAmplify,get_volume_db);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "volume_db", PropertyHint::Range, "-80,24,0.01"), "set_volume_db", "get_volume_db");
}

AudioEffectAmplify::AudioEffectAmplify() {
    volume_db = 0;
}
