/*************************************************************************/
/*  audio_stream_mp3.h                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "core/io/resource_loader.h"
#include "servers/audio/audio_stream.h"

class AudioStreamMP3;
struct mp3dec_ex_t;


class GODOT_EXPORT AudioStreamPlaybackMP3 : public AudioStreamPlaybackResampled {
    GDCLASS(AudioStreamPlaybackMP3, AudioStreamPlaybackResampled);

    mp3dec_ex_t *mp3d = nullptr;
    uint32_t frames_mixed = 0;
    bool active = false;
    int loops = 0;

    friend class AudioStreamMP3;

    Ref<AudioStreamMP3> mp3_stream;

protected:
    void _mix_internal(AudioFrame *p_buffer, int p_frames) override;
    float get_stream_sampling_rate() override;

public:
    void start(float p_from_pos = 0.0) override;
    void stop() override;
    bool is_playing() const override;

    int get_loop_count() const override; //times it looped

    float get_playback_position() const override;
    void seek(float p_time) override;

    AudioStreamPlaybackMP3() = default;
    ~AudioStreamPlaybackMP3() override;
};

class GODOT_EXPORT AudioStreamMP3 : public AudioStream {
    GDCLASS(AudioStreamMP3, AudioStream);
    OBJ_SAVE_TYPE(AudioStream) //children are all saved as AudioStream, so they can be exchanged
    RES_BASE_EXTENSION("mp3str");

    friend class AudioStreamPlaybackMP3;

    void *data = nullptr;
    uint32_t data_len = 0;

    float sample_rate = 1;
    int channels = 1;
    float length = 0;
    bool loop = false;
    float loop_offset = 0;
    void clear_data();

protected:
    static void _bind_methods();

public:
    void set_loop(bool p_enable);
    bool has_loop() const;

    void set_loop_offset(float p_seconds);
    float get_loop_offset() const;

    Ref<AudioStreamPlayback> instance_playback() override;
    String get_stream_name() const override;

    void set_data(const PoolVector<uint8_t> &p_data);
    PoolVector<uint8_t> get_data() const;

    float get_length() const override;

    AudioStreamMP3();
    ~AudioStreamMP3() override;
};
