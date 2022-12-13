/*************************************************************************/
/*  video_player.cpp                                                     */
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

#include "video_player.h"
#include "scene/scene_string_names.h"
#include "core/method_bind.h"

#include "core/os/os.h"
#include "servers/audio_server.h"

IMPL_GDCLASS(VideoPlayer)

int VideoPlayer::sp_get_channel_count() const {

    if (not playback) {
        return 0;
    }

    return playback->get_channels();
}

bool VideoPlayer::mix(AudioFrame *p_buffer, int p_frames) {

    // Check the amount resampler can really handle.
    // If it cannot, wait "wait_resampler_phase_limit" times.
    // This mechanism contributes to smoother pause/unpause operation.
    if (p_frames <= resampler.get_num_of_ready_frames() ||
            wait_resampler_limit <= wait_resampler) {
        wait_resampler = 0;
        return resampler.mix(p_buffer, p_frames);
    }
    wait_resampler++;
    return false;
}

// Called from main thread (eg VideoStreamPlaybackWebm::update)
int VideoPlayer::_audio_mix_callback(void *p_udata, const float *p_data, int p_frames) {
    ERR_FAIL_NULL_V(p_udata, 0);
    ERR_FAIL_NULL_V(p_data, 0);

    VideoPlayer *vp = (VideoPlayer *)p_udata;

    int todo = MIN(vp->resampler.get_writer_space(), p_frames);

    float *wb = vp->resampler.get_write_buffer();
    int c = vp->resampler.get_channel_count();

    for (int i = 0; i < todo * c; i++) {
        wb[i] = p_data[i];
    }
    vp->resampler.write(todo);

    return todo;
}
void VideoPlayer::_mix_audios(void *p_self) {

    ERR_FAIL_NULL(p_self);
    reinterpret_cast<VideoPlayer *>(p_self)->_mix_audio();
}
// Called from audio thread
void VideoPlayer::_mix_audio() {

    if (!stream) {
        return;
    }
    if (!playback || !playback->is_playing() || playback->is_paused()) {
        return;
    }

    AudioFrame *buffer = mix_buffer.data();
    int buffer_size = mix_buffer.size();

    // Resample
    if (!mix(buffer, buffer_size))
        return;

    AudioFrame vol = AudioFrame(volume, volume);

    int cc = AudioServer::get_singleton()->get_channel_count();

    if (cc == 1) {
        AudioFrame *target = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, 0);
        ERR_FAIL_COND(!target);

        for (int j = 0; j < buffer_size; j++) {

            target[j] += buffer[j] * vol;
        }

    } else {
        AudioFrame *targets[4];

        for (int k = 0; k < cc; k++) {
            targets[k] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, k);
            ERR_FAIL_COND(!targets[k]);
        }

        for (int j = 0; j < buffer_size; j++) {

            AudioFrame frame = buffer[j] * vol;
            for (int k = 0; k < cc; k++) {
                targets[k][j] += frame;
            }
        }
    }
}

void VideoPlayer::_notification(int p_notification) {

    switch (p_notification) {

        case NOTIFICATION_ENTER_TREE: {

            AudioServer::get_singleton()->add_callback(_mix_audios, this);

            if (stream && autoplay && !Engine::get_singleton()->is_editor_hint()) {
                play();
            }

        } break;

        case NOTIFICATION_EXIT_TREE: {

            AudioServer::get_singleton()->remove_callback(_mix_audios, this);

        } break;

        case NOTIFICATION_INTERNAL_PROCESS: {

            bus_index = AudioServer::get_singleton()->thread_find_bus_index(bus);

            if (not stream || paused || !playback || !playback->is_playing())
                return;

            double audio_time = USEC_TO_SEC(OS::get_singleton()->get_ticks_usec());

            double delta = last_audio_time == 0 ? 0 : audio_time - last_audio_time;
            last_audio_time = audio_time;

            if (delta == 0)
                return;

            playback->update(delta); // playback->is_playing() returns false in the last video frame

            if (!playback->is_playing()) {
                emit_signal(SceneStringNames::finished);
            }

        } break;

        case NOTIFICATION_DRAW: {

            if (not texture)
                return;
            if (texture->get_width() == 0)
                return;

            Size2 s = expand ? get_size() : texture->get_size();
            draw_texture_rect(texture, Rect2(Point2(), s), false);

        } break;
    }
}

Size2 VideoPlayer::get_minimum_size() const {

    if (!expand && texture)
        return texture->get_size();
    else
        return Size2();
}

void VideoPlayer::set_expand(bool p_expand) {

    expand = p_expand;
    update();
    minimum_size_changed();
}

bool VideoPlayer::has_expand() const {

    return expand;
}

void VideoPlayer::set_stream(const Ref<VideoStream> &p_stream) {

    stop();
    AudioServer::get_singleton()->lock();
    mix_buffer.resize(AudioServer::get_singleton()->thread_get_mix_buffer_size());

    stream = p_stream;
    if (stream) {
        stream->set_audio_track(audio_track);
        playback = stream->instance_playback();
    } else {
        playback = Ref<VideoStreamPlayback>();
    }
    AudioServer::get_singleton()->unlock();

    if (playback) {
        playback->set_loop(loops);
        playback->set_paused(paused);
        texture = dynamic_ref_cast<ImageTexture>(playback->get_texture());

        const int channels = playback->get_channels();

        AudioServer::get_singleton()->lock();
        if (channels > 0)
            resampler.setup(channels, playback->get_mix_rate(), AudioServer::get_singleton()->get_mix_rate(), buffering_ms, 0);
        else
            resampler.clear();
        AudioServer::get_singleton()->unlock();

        if (channels > 0)
            playback->set_mix_callback(_audio_mix_callback, this);

    } else {
        texture.unref();
        AudioServer::get_singleton()->lock();
        resampler.clear();
        AudioServer::get_singleton()->unlock();
    }

    update();
    if (!expand) {
        minimum_size_changed();
    }

}

Ref<VideoStream> VideoPlayer::get_stream() const {

    return stream;
}

void VideoPlayer::play() {

    ERR_FAIL_COND(!is_inside_tree());
    if (not playback)
        return;
    playback->stop();
    playback->play();
    set_process_internal(true);
    //    AudioServer::get_singleton()->stream_set_active(stream_rid,true);
    //    AudioServer::get_singleton()->stream_set_volume_scale(stream_rid,volume);
    last_audio_time = 0;
}

void VideoPlayer::stop() {

    if (!is_inside_tree())
        return;
    if (not playback)
        return;

    playback->stop();
    //    AudioServer::get_singleton()->stream_set_active(stream_rid,false);
    resampler.flush();
    set_process_internal(false);
    last_audio_time = 0;
}

bool VideoPlayer::is_playing() const {

    if (not playback)
        return false;

    return playback->is_playing();
}

void VideoPlayer::set_paused(bool p_paused) {

    paused = p_paused;
    if (playback) {
        playback->set_paused(p_paused);
        set_process_internal(!p_paused);
    }
    last_audio_time = 0;
}

bool VideoPlayer::is_paused() const {

    return paused;
}

void VideoPlayer::set_buffering_msec(int p_msec) {

    buffering_ms = p_msec;
}

int VideoPlayer::get_buffering_msec() const {

    return buffering_ms;
}

void VideoPlayer::set_audio_track(int p_track) {
    audio_track = p_track;
}

int VideoPlayer::get_audio_track() const {

    return audio_track;
}

void VideoPlayer::set_volume(float p_vol) {

    volume = p_vol;
}

float VideoPlayer::get_volume() const {

    return volume;
}

void VideoPlayer::set_volume_db(float p_db) {

    if (p_db < -79)
        set_volume(0);
    else
        set_volume(Math::db2linear(p_db));
}

float VideoPlayer::get_volume_db() const {

    if (volume == 0) {
        return -80;
    } else {
        return Math::linear2db(volume);
    }
}

StringName VideoPlayer::get_stream_name() const {

    if (not stream) {
        return StringName("<No Stream>");
    }
    return StringName(stream->get_name());
}

float VideoPlayer::get_stream_position() const {

    if (not playback) {
        return 0;
    }
    return playback->get_playback_position();
}

void VideoPlayer::set_stream_position(float p_position) {

    if (playback) {
        playback->seek(p_position);
    }
}

Ref<Texture> VideoPlayer::get_video_texture() const {

    if (playback)
        return playback->get_texture();

    return Ref<Texture>();
}

void VideoPlayer::set_autoplay(bool p_enable) {

    autoplay = p_enable;
}

bool VideoPlayer::has_autoplay() const {

    return autoplay;
}

void VideoPlayer::set_bus(const StringName &p_bus) {

    //if audio is active, must lock this
    AudioServer::get_singleton()->lock();
    bus = p_bus;
    AudioServer::get_singleton()->unlock();
}

StringName VideoPlayer::get_bus() const {

    for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
        if (AudioServer::get_singleton()->get_bus_name(i) == bus) {
            return bus;
        }
    }
    return "Master";
}

void VideoPlayer::_validate_property(PropertyInfo &p_property) const {

    if (p_property.name == "bus") {

        String options;
        for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
            if (i > 0)
                options += ',';
            StringName name = AudioServer::get_singleton()->get_bus_name(i);
            options += name;
        }

        p_property.hint_string = options;
    }
}

void VideoPlayer::_bind_methods() {

    SE_BIND_METHOD(VideoPlayer,set_stream);
    SE_BIND_METHOD(VideoPlayer,get_stream);

    SE_BIND_METHOD(VideoPlayer,play);
    SE_BIND_METHOD(VideoPlayer,stop);

    SE_BIND_METHOD(VideoPlayer,is_playing);

    SE_BIND_METHOD(VideoPlayer,set_paused);
    SE_BIND_METHOD(VideoPlayer,is_paused);

    SE_BIND_METHOD(VideoPlayer,set_volume);
    SE_BIND_METHOD(VideoPlayer,get_volume);

    SE_BIND_METHOD(VideoPlayer,set_volume_db);
    SE_BIND_METHOD(VideoPlayer,get_volume_db);

    SE_BIND_METHOD(VideoPlayer,set_audio_track);
    SE_BIND_METHOD(VideoPlayer,get_audio_track);

    SE_BIND_METHOD(VideoPlayer,get_stream_name);

    SE_BIND_METHOD(VideoPlayer,set_stream_position);
    SE_BIND_METHOD(VideoPlayer,get_stream_position);

    SE_BIND_METHOD(VideoPlayer,set_autoplay);
    SE_BIND_METHOD(VideoPlayer,has_autoplay);

    SE_BIND_METHOD(VideoPlayer,set_expand);
    SE_BIND_METHOD(VideoPlayer,has_expand);

    SE_BIND_METHOD(VideoPlayer,set_buffering_msec);
    SE_BIND_METHOD(VideoPlayer,get_buffering_msec);

    SE_BIND_METHOD(VideoPlayer,set_bus);
    SE_BIND_METHOD(VideoPlayer,get_bus);

    SE_BIND_METHOD(VideoPlayer,get_video_texture);

    ADD_SIGNAL(MethodInfo("finished"));

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "audio_track", PropertyHint::Range, "0,128,1"), "set_audio_track", "get_audio_track");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "stream", PropertyHint::ResourceType, "VideoStream"), "set_stream", "get_stream");
    //ADD_PROPERTY( PropertyInfo(VariantType::BOOL, "stream/loop"), "set_loop", "has_loop") ;
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "volume_db", PropertyHint::Range, "-80,24,0.01"), "set_volume_db", "get_volume_db");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "volume", PropertyHint::ExpRange, "0,15,0.01", 0), "set_volume", "get_volume");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "autoplay"), "set_autoplay", "has_autoplay");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "paused"), "set_paused", "is_paused");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "expand"), "set_expand", "has_expand");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "buffering_msec", PropertyHint::Range, "10,1000"), "set_buffering_msec", "get_buffering_msec");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "stream_position", PropertyHint::Range, "0,1280000,0.1", 0), "set_stream_position", "get_stream_position");

    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "bus", PropertyHint::Enum, ""), "set_bus", "get_bus");
}

VideoPlayer::VideoPlayer() {

    volume = 1;
    loops = false;
    paused = false;
    autoplay = false;
    expand = true;

    audio_track = 0;
    bus_index = 0;

    buffering_ms = 500;

    //    internal_stream.player=this;
    //    stream_rid=AudioServer::get_singleton()->audio_stream_create(&internal_stream);
    last_audio_time = 0;

    wait_resampler = 0;
    wait_resampler_limit = 2;
}

VideoPlayer::~VideoPlayer() {

    //    if (stream_rid.is_valid())
    //        AudioServer::get_singleton()->free(stream_rid);
    resampler.clear(); //Not necessary here, but make in consistent with other "stream_player" classes
}
