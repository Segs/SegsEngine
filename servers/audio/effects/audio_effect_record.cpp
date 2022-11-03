/*************************************************************************/
/*  audio_effect_record.cpp                                              */
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

#include "audio_effect_record.h"
#include "core/method_bind.h"

IMPL_GDCLASS(AudioEffectRecordInstance)
IMPL_GDCLASS(AudioEffectRecord)
//TODO: SEGS: AudioStreamSample::Format used.
VARIANT_ENUM_CAST(AudioStreamSample::Format);
namespace  {
    // copied over from ResourceImporterWAV to remove dependency on optional plugin.
    void WAV_compress_ima_adpcm(Span<const float> p_data, Vector<uint8_t>& dst_data)
    {
        /*p_sample_data->data = (void*)malloc(len);
        xm_s8 *dataptr=(xm_s8*)p_sample_data->data;*/

        static const int16_t _ima_adpcm_step_table[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
            19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
            50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
            130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
            337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
            876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
            2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
            5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
            15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
        };

        static const int8_t _ima_adpcm_index_table[16] = {
            -1, -1, -1, -1, 2, 4, 6, 8,
            -1, -1, -1, -1, 2, 4, 6, 8
        };

        int datalen = p_data.size();
        int datamax = datalen;
        if (datalen & 1)
            datalen++;

        dst_data.resize(datalen / 2 + 4);

        int step_idx = 0, prev = 0;
        uint8_t* out = dst_data.data();
        //int16_t xm_prev=0;
        const float* in = p_data.data();

        /* initial value is zero */
        *out++ = 0;
        *out++ = 0;
        /* Table index initial value */
        *out++ = 0;
        /* unused */
        *out++ = 0;

        for (int i = 0; i < datalen; i++)
        {
            uint8_t nibble;
            int16_t xm_sample;

            if (i >= datamax)
                xm_sample = 0;
            else
            {
                xm_sample = CLAMP(in[i] * 32767.0f, -32768.0f, 32767.0f);
                /*
                if (xm_sample==32767 || xm_sample==-32768)
                    printf("clippy!\n",xm_sample);
                */
            }

            //xm_sample=xm_sample+xm_prev;
            //xm_prev=xm_sample;

            int diff = (int)xm_sample - prev;

            nibble = 0;
            int step = _ima_adpcm_step_table[step_idx];
            int vpdiff = step >> 3;
            if (diff < 0)
            {
                nibble = 8;
                diff = -diff;
            }
            int mask = 4;
            while (mask)
            {
                if (diff >= step)
                {
                    nibble |= mask;
                    diff -= step;
                    vpdiff += step;
                }

                step >>= 1;
                mask >>= 1;
            }

            if (nibble & 8)
                prev -= vpdiff;
            else
                prev += vpdiff;

            if (prev > 32767)
            {
                //printf("%i,xms %i, prev %i,diff %i, vpdiff %i, clip up %i\n",i,xm_sample,prev,diff,vpdiff,prev);
                prev = 32767;
            }
            else if (prev < -32768)
            {
                //printf("%i,xms %i, prev %i,diff %i, vpdiff %i, clip down %i\n",i,xm_sample,prev,diff,vpdiff,prev);
                prev = -32768;
            }

            step_idx += _ima_adpcm_index_table[nibble];
            if (step_idx < 0)
                step_idx = 0;
            else if (step_idx > 88)
                step_idx = 88;

            if (i & 1)
            {
                *out |= nibble << 4;
                out++;
            }
            else
            {
                *out = nibble;
            }
            /*dataptr[i]=prev>>8;*/
        }
    }
}
void AudioEffectRecordInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
    if (!is_recording) {
        for (int i = 0; i < p_frame_count; i++) {
            p_dst_frames[i] = p_src_frames[i];
        }
        return;
    }

    //Add incoming audio frames to the IO ring buffer
    const AudioFrame *src = p_src_frames;
    AudioFrame *rb_buf = ring_buffer.data();
    for (int i = 0; i < p_frame_count; i++) {
        p_dst_frames[i] = p_src_frames[i];
        rb_buf[ring_buffer_pos & ring_buffer_mask] = src[i];
        ring_buffer_pos++;
    }
}

void AudioEffectRecordInstance::_update_buffer() {
    //Case: Frames are remaining in the buffer
    while (ring_buffer_read_pos < ring_buffer_pos) {
        //Read from the buffer into recording_data
        _io_store_buffer();
    }
}

void AudioEffectRecordInstance::_update(void *userdata) {
    AudioEffectRecordInstance *ins = (AudioEffectRecordInstance *)userdata;
    ins->_update_buffer();
}

bool AudioEffectRecordInstance::process_silence() const {
    return true;
}

void AudioEffectRecordInstance::_io_thread_process() {
    thread_active = true;

    while (is_recording) {
        //Check: The current recording has been requested to stop
        if (!base->recording_active) {
            is_recording = false;
        }

        _update_buffer();

        if (is_recording) {
            //Wait to avoid too much busy-wait
            OS::get_singleton()->delay_usec(500);
        }
    }

    thread_active = false;
}

void AudioEffectRecordInstance::_io_store_buffer() {
    int to_read = ring_buffer_pos - ring_buffer_read_pos;

    AudioFrame *rb_buf = ring_buffer.data();

    while (to_read) {
        AudioFrame buffered_frame = rb_buf[ring_buffer_read_pos & ring_buffer_mask];
        recording_data.push_back(buffered_frame.l);
        recording_data.push_back(buffered_frame.r);

        ring_buffer_read_pos++;
        to_read--;
    }
}

void AudioEffectRecordInstance::_thread_callback(void *_instance) {

    AudioEffectRecordInstance *aeri = reinterpret_cast<AudioEffectRecordInstance *>(_instance);

    aeri->_io_thread_process();
}

void AudioEffectRecordInstance::init() {
    //Reset recorder status
    ring_buffer_pos = 0;
    ring_buffer_read_pos = 0;

    //We start a new recording
    recording_data.resize(0); //Clear data completely and reset length
    is_recording = true;

    io_thread.start(_thread_callback, this);
}

void AudioEffectRecordInstance::finish() {

    if (thread_active) {
        io_thread.wait_to_finish();
    }
}

AudioEffectRecordInstance::~AudioEffectRecordInstance() {

    finish();
}

Ref<AudioEffectInstance> AudioEffectRecord::instance() {
    Ref<AudioEffectRecordInstance> ins(make_ref_counted<AudioEffectRecordInstance>());
    ins->base = Ref<AudioEffectRecord>(this);
    ins->is_recording = false;

    //Re-using the buffer size calculations from audio_effect_delay.cpp
    float ring_buffer_max_size = IO_BUFFER_SIZE_MS;
    ring_buffer_max_size /= 1000.0; //convert to seconds
    ring_buffer_max_size *= AudioServer::get_singleton()->get_mix_rate();

    int ringbuff_size = ring_buffer_max_size;

    int bits = 0;

    while (ringbuff_size > 0) {
        bits++;
        ringbuff_size /= 2;
    }

    ringbuff_size = 1 << bits;
    ins->ring_buffer_mask = ringbuff_size - 1;
    ins->ring_buffer_pos = 0;

    ins->ring_buffer.resize(ringbuff_size);

    ins->ring_buffer_read_pos = 0;

    ensure_thread_stopped();
    current_instance = ins;
    if (recording_active) {
        ins->init();
    }

    return ins;
}

void AudioEffectRecord::ensure_thread_stopped() {
    recording_active = false;
    if (current_instance != nullptr) {
        current_instance->finish();
    }
}

void AudioEffectRecord::set_recording_active(bool p_record) {
    if (p_record) {
        if (current_instance == nullptr) {
            WARN_PRINT("Recording should not be set as active before Godot has initialized.");
            recording_active = false;
            return;
        }

        ensure_thread_stopped();
        recording_active = true;
        current_instance->init();
    } else {
        recording_active = false;
    }
}

bool AudioEffectRecord::is_recording_active() const {
    return recording_active;
}

void AudioEffectRecord::set_format(AudioStreamSample::Format p_format) {
    format = p_format;
}

AudioStreamSample::Format AudioEffectRecord::get_format() const {
    return format;
}

Ref<AudioStreamSample> AudioEffectRecord::get_recording() const {
    AudioStreamSample::Format dst_format = format;
    bool stereo = true; //forcing mono is not implemented

    Vector<uint8_t> dst_data;

    ERR_FAIL_COND_V(not current_instance, Ref<AudioStreamSample>());
    ERR_FAIL_COND_V(current_instance->recording_data.empty(), Ref<AudioStreamSample>());

    if (dst_format == AudioStreamSample::FORMAT_8_BITS) {
        int data_size = current_instance->recording_data.size();
        dst_data.resize(data_size);

        for (int i = 0; i < data_size; i++) {
            int8_t v = CLAMP<float>(current_instance->recording_data[i] * 128, -128, 127);
            dst_data[i] = v;
        }
    } else if (dst_format == AudioStreamSample::FORMAT_16_BITS) {
        int data_size = current_instance->recording_data.size();
        dst_data.resize(data_size * 2);

        for (int i = 0; i < data_size; i++) {
            int16_t v = CLAMP<float>(current_instance->recording_data[i] * 32768, -32768, 32767);
            encode_uint16(v, &dst_data[i * 2]);
        }
    } else if (dst_format == AudioStreamSample::FORMAT_IMA_ADPCM) {
        //byte interleave
        Vector<float> left;
        Vector<float> right;

        int tframes = current_instance->recording_data.size() / 2;
        left.resize(tframes);
        right.resize(tframes);

        for (int i = 0; i < tframes; i++) {
            left[i] = current_instance->recording_data[i * 2 + 0];
            right[i] = current_instance->recording_data[i * 2 + 1];
        }

        Vector<uint8_t> bleft;
        Vector<uint8_t> bright;

        WAV_compress_ima_adpcm(left, bleft);
        WAV_compress_ima_adpcm(right, bright);

        int dl = bleft.size();
        dst_data.resize(dl * 2);

        for (int i = 0; i < dl; i++) {
            dst_data[i * 2 + 0] = bleft[i];
            dst_data[i * 2 + 1] = bright[i];
        }
    } else {
        ERR_PRINT("Format not implemented.");
    }

    Ref<AudioStreamSample> sample(make_ref_counted<AudioStreamSample>());
    sample->set_data(dst_data);
    sample->set_format(dst_format);
    sample->set_mix_rate(AudioServer::get_singleton()->get_mix_rate());
    sample->set_loop_mode(AudioStreamSample::LOOP_DISABLED);
    sample->set_loop_begin(0);
    sample->set_loop_end(0);
    sample->set_stereo(stereo);

    return sample;
}

void AudioEffectRecord::_bind_methods() {
    SE_BIND_METHOD(AudioEffectRecord,set_recording_active);
    SE_BIND_METHOD(AudioEffectRecord,is_recording_active);
    SE_BIND_METHOD(AudioEffectRecord,set_format);
    SE_BIND_METHOD(AudioEffectRecord,get_format);
    SE_BIND_METHOD(AudioEffectRecord,get_recording);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "format", PropertyHint::Enum, "8-Bit,16-Bit,IMA-ADPCM"), "set_format", "get_format");
}

AudioEffectRecord::AudioEffectRecord() {
    format = AudioStreamSample::FORMAT_16_BITS;
    recording_active = false;
}
