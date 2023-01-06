/*************************************************************************/
/*  audio_server.cpp                                                     */
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

#include "audio_server.h"

#include "core/debugger/script_debugger.h"
#include "core/external_profiler.h"
#include "core/io/resource_loader.h"
#include "core/method_bind.h"
#include "core/method_enum_caster.h"
#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/resource/resource_manager.h"
#include "core/script_language.h"
#include "scene/resources/audio_stream_sample.h"

#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio/effects/audio_effect_compressor.h"

using namespace eastl; // for string view suffix

#ifdef TOOLS_ENABLED

#define MARK_EDITED Object_set_edited(this,true);

#else

#define MARK_EDITED

#endif

IMPL_GDCLASS(AudioServer)
IMPL_GDCLASS(AudioBusLayout)
VARIANT_ENUM_CAST(AudioServer::SpeakerMode);

struct AudioServerBus {

    StringName name;
    bool solo;
    bool mute;
    bool bypass;

    bool soloed;

    //Each channel is a stereo pair.
    struct Channel {
        bool used;
        bool active;
        AudioFrame peak_volume;
        Vector<AudioFrame> buffer;
        Vector<Ref<AudioEffectInstance> > effect_instances;
        uint64_t last_mix_with_audio;
        Channel() {
            last_mix_with_audio = 0;
            used = false;
            active = false;
            peak_volume = AudioFrame(AUDIO_MIN_PEAK_DB, AUDIO_MIN_PEAK_DB);
        }
    };

    Vector<Channel> channels;

    struct Effect {
        Ref<AudioEffect> effect;
        bool enabled;
#ifdef DEBUG_ENABLED
        uint64_t prof_time;
#endif
    };

    Vector<Effect> effects;
    float volume_db;
    StringName send;
    int index_cache;
};


AudioDriver *AudioDriver::singleton = nullptr;
AudioDriver *AudioDriver::get_singleton() {

    return singleton;
}

void AudioDriver::set_singleton() {

    singleton = this;
}

void AudioDriver::audio_server_process(int p_frames, int32_t *p_buffer, bool p_update_mix_time) {

    if (p_update_mix_time) {
        update_mix_time(p_frames);
    }

    if (AudioServer::get_singleton()) {
        AudioServer::get_singleton()->_driver_process(p_frames, p_buffer);
    }
}

void AudioDriver::update_mix_time(int p_frames) {

    _last_mix_frames = p_frames;
    if (OS::get_singleton()) {
        _last_mix_time = OS::get_singleton()->get_ticks_usec();
    }
}

double AudioDriver::get_time_since_last_mix() {
    lock();
    uint64_t last_mix_time = _last_mix_time;
    unlock();
    return (OS::get_singleton()->get_ticks_usec() - last_mix_time) / 1000000.0;
}

double AudioDriver::get_time_to_next_mix() {
    lock();
    uint64_t last_mix_time = _last_mix_time;
    uint64_t last_mix_frames = _last_mix_frames;
    unlock();
    double total = (OS::get_singleton()->get_ticks_usec() - last_mix_time) / 1000000.0;
    double mix_buffer = last_mix_frames / (double)get_mix_rate();
    return mix_buffer - total;
}

void AudioDriver::input_buffer_init(int driver_buffer_frames) {

    const int input_buffer_channels = 2;
    input_buffer.resize(driver_buffer_frames * input_buffer_channels * 4);
    input_position = 0;
    input_size = 0;
}

void AudioDriver::input_buffer_write(int32_t sample) {

    if ((int)input_position < input_buffer.size()) {
        input_buffer.write()[input_position++] = sample;
        if ((int)input_position >= input_buffer.size()) {
            input_position = 0;
        }
        if ((int)input_size < input_buffer.size()) {
            input_size++;
        }
    } else {
        WARN_PRINT("input_buffer_write: Invalid input_position=" + itos(input_position) + " input_buffer.size()=" + itos(input_buffer.size()));
    }
}

AudioDriver::SpeakerMode AudioDriver::get_speaker_mode_by_total_channels(int p_channels) const {
    switch (p_channels) {
        case 4:
            return SPEAKER_SURROUND_31;
        case 6:
            return SPEAKER_SURROUND_51;
        case 8:
            return SPEAKER_SURROUND_71;
    }

    // Default to STEREO
    return SPEAKER_MODE_STEREO;
}

int AudioDriver::get_total_channels_by_speaker_mode(AudioDriver::SpeakerMode p_mode) const {
    switch (p_mode) {
        case SPEAKER_MODE_STEREO:
            return 2;
        case SPEAKER_SURROUND_31:
            return 4;
        case SPEAKER_SURROUND_51:
            return 6;
        case SPEAKER_SURROUND_71:
            return 8;
    }

    ERR_FAIL_V(2);
}

Array AudioDriver::get_device_list() {
    Array list;

    list.push_back("Default");

    return list;
}

StringView AudioDriver::get_device() {
    return "Default";
}

Array AudioDriver::capture_get_device_list() {
    Array list;

    list.push_back("Default");

    return list;
}

AudioDriver::AudioDriver() {

    _last_mix_time = 0;
    _last_mix_frames = 0;
    input_position = 0;
    input_size = 0;

#ifdef DEBUG_ENABLED
    prof_time = 0;
#endif
}

AudioDriverDummy AudioDriverManager::dummy_driver;
AudioDriver *AudioDriverManager::drivers[MAX_DRIVERS] = {
    &AudioDriverManager::dummy_driver,
};
int AudioDriverManager::driver_count = 1;

void AudioDriverManager::add_driver(AudioDriver *p_driver) {

    ERR_FAIL_COND(driver_count >= MAX_DRIVERS);
    drivers[driver_count - 1] = p_driver;

    // Last driver is always our dummy driver
    drivers[driver_count++] = &AudioDriverManager::dummy_driver;
}

int AudioDriverManager::get_driver_count() {

    return driver_count;
}

void AudioDriverManager::initialize(int p_driver) {
    GLOBAL_DEF_RST("audio/enable_audio_input", false);
    GLOBAL_DEF_RST("audio/mix_rate", DEFAULT_MIX_RATE);
    GLOBAL_DEF_RST("audio/output_latency", DEFAULT_OUTPUT_LATENCY);

    int failed_driver = -1;

    // Check if there is a selected driver
    if (p_driver >= 0 && p_driver < driver_count) {
        if (drivers[p_driver]->init() == OK) {
            drivers[p_driver]->set_singleton();
            return;
        } else {
            failed_driver = p_driver;
        }
    }

    // No selected driver, try them all in order
    for (int i = 0; i < driver_count; i++) {
        // Don't re-init the driver if it failed above
        if (i == failed_driver) {
            continue;
        }

        if (drivers[i]->init() == OK) {
            drivers[i]->set_singleton();
            break;
        }
    }

    if (driver_count > 1 && StringView(AudioDriver::get_singleton()->get_name()) == StringView("Dummy")) {
        WARN_PRINT("All audio drivers failed, falling back to the dummy driver.");
    }
}

AudioDriver *AudioDriverManager::get_driver(int p_driver) {

    ERR_FAIL_INDEX_V(p_driver, driver_count, nullptr);
    return drivers[p_driver];
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

void AudioServer::_driver_process(int p_frames, int32_t *p_buffer) {

    int todo = p_frames;

#ifdef DEBUG_ENABLED
    uint64_t prof_ticks = OS::get_singleton()->get_ticks_usec();
#endif

    if (channel_count != get_channel_count()) {
        // Amount of channels changed due to a device change
        // reinitialize the buses channels and buffers
        init_channels_and_buffers();
    }

    ERR_FAIL_COND_MSG(buses.empty() && todo, "AudioServer bus count is less than 1.");
    while (todo) {

        if (to_mix == 0) {
            _mix_step();
        }

        int to_copy = MIN(to_mix, todo);

        AudioServerBus *master = buses[0];

        int from = buffer_size - to_mix;
        int from_buf = p_frames - todo;

        //master master, send to output
        int cs = master->channels.size();
        for (int k = 0; k < cs; k++) {

            if (master->channels[k].active) {

                const AudioFrame *buf = master->channels[k].buffer.data();

                for (int j = 0; j < to_copy; j++) {

                    float l = CLAMP(buf[from + j].l, -1.0f, 1.0f);
                    int32_t vl = l * ((1 << 20) - 1);
                    int32_t vl2 = (vl < 0 ? -1 : 1) * (ABS(vl) << 11);
                    p_buffer[(from_buf + j) * (cs * 2) + k * 2 + 0] = vl2;

                    float r = CLAMP(buf[from + j].r, -1.0f, 1.0f);
                    int32_t vr = r * ((1 << 20) - 1);
                    int32_t vr2 = (vr < 0 ? -1 : 1) * (ABS(vr) << 11);
                    p_buffer[(from_buf + j) * (cs * 2) + k * 2 + 1] = vr2;
                }

            } else {
                for (int j = 0; j < to_copy; j++) {

                    p_buffer[(from_buf + j) * (cs * 2) + k * 2 + 0] = 0;
                    p_buffer[(from_buf + j) * (cs * 2) + k * 2 + 1] = 0;
                }
            }
        }

        todo -= to_copy;
        to_mix -= to_copy;
    }

#ifdef DEBUG_ENABLED
    prof_time += OS::get_singleton()->get_ticks_usec() - prof_ticks;
#endif
}

void AudioServer::_mix_step() {

    bool solo_mode = false;

    for (int i = 0; i < buses.size(); i++) {
        AudioServerBus *bus = buses[i];
        bus->index_cache = i; //might be moved around by editor, so..
        for (int k = 0; k < bus->channels.size(); k++) {

            bus->channels[k].used = false;
        }

        if (bus->solo) {
            //solo chain
            solo_mode = true;
            bus->soloed = true;
            do {

                if (bus != buses[0]) {
                    //everything has a send save for master bus
                    if (!bus_map.contains(bus->send)) {
                        bus = buses[0]; //send to master
                    } else {
                        int prev_index_cache = bus->index_cache;
                        bus = bus_map[bus->send];
                        if (prev_index_cache >= bus->index_cache) { //invalid, send to master
                            bus = buses[0];
                        }
                    }

                    bus->soloed = true;
                } else {
                    bus = nullptr;
                }

            } while (bus);
        } else {
            bus->soloed = false;
        }
    }

    //make callbacks for mixing the audio
    for (const CallbackItem &E : callbacks) {

        E.callback(E.userdata);
    }

    for (int i = buses.size() - 1; i >= 0; i--) {
        //go bus by bus
        AudioServerBus *bus = buses[i];

        for (int k = 0; k < bus->channels.size(); k++) {

            if (bus->channels[k].active && !bus->channels[k].used) {
                //buffer was not used, but it's still active, so it must be cleaned
                AudioFrame *buf = bus->channels[k].buffer.data();

                for (uint32_t j = 0; j < buffer_size; j++) {

                    buf[j] = AudioFrame(0, 0);
                }
            }
        }

        //process effects
        if (!bus->bypass) {
            for (int j = 0; j < bus->effects.size(); j++) {

                if (!bus->effects[j].enabled)
                    continue;

#ifdef DEBUG_ENABLED
                uint64_t ticks = OS::get_singleton()->get_ticks_usec();
#endif

                for (int k = 0; k < bus->channels.size(); k++) {

                    if (!(bus->channels[k].active || bus->channels[k].effect_instances[j]->process_silence())) {
                        continue;
                    }
                    bus->channels[k].effect_instances[j]->process(bus->channels[k].buffer.data(), temp_buffer[k].data(), buffer_size);
                }

                //swap buffers, so internal buffer always has the right data
                for (int k = 0; k < bus->channels.size(); k++) {

                    if (!(buses[i]->channels[k].active || bus->channels[k].effect_instances[j]->process_silence())) {
                        continue;
                    }
                    SWAP(bus->channels[k].buffer, temp_buffer[k]);
                }

#ifdef DEBUG_ENABLED
                bus->effects[j].prof_time += OS::get_singleton()->get_ticks_usec() - ticks;
#endif
            }
        }

        //process send

        AudioServerBus *send = nullptr;

        if (i > 0) {
            //everything has a send save for master bus
            if (!bus_map.contains(bus->send)) {
                send = buses[0];
            } else {
                send = bus_map[bus->send];
                if (send->index_cache >= bus->index_cache) { //invalid, send to master
                    send = buses[0];
                }
            }
        }

        for (int k = 0; k < bus->channels.size(); k++) {

            if (!bus->channels[k].active) {
                bus->channels[k].peak_volume = AudioFrame(AUDIO_MIN_PEAK_DB, AUDIO_MIN_PEAK_DB);
                continue;
            }

            AudioFrame *buf = bus->channels[k].buffer.data();

            AudioFrame peak = AudioFrame(0, 0);

            float volume = Math::db2linear(bus->volume_db);

            if (solo_mode) {
                if (!bus->soloed) {
                    volume = 0.0;
                }
            } else {
                if (bus->mute) {
                    volume = 0.0;
                }
            }

            //apply volume and compute peak
            for (uint32_t j = 0; j < buffer_size; j++) {

                buf[j] *= volume;

                float l = ABS(buf[j].l);
                if (l > peak.l) {
                    peak.l = l;
                }
                float r = ABS(buf[j].r);
                if (r > peak.r) {
                    peak.r = r;
                }
            }

            bus->channels[k].peak_volume = AudioFrame(Math::linear2db(peak.l + AUDIO_PEAK_OFFSET), Math::linear2db(peak.r + AUDIO_PEAK_OFFSET));

            if (!bus->channels[k].used) {
                //see if any audio is contained, because channel was not used

                if (M_MAX(peak.r, peak.l) > Math::db2linear(channel_disable_threshold_db)) {
                    bus->channels[k].last_mix_with_audio = mix_frames;
                } else if (mix_frames - bus->channels[k].last_mix_with_audio > channel_disable_frames) {
                    bus->channels[k].active = false;
                    continue; //went inactive, don't mix.
                }
            }

            if (send) {
                //if not master bus, send
                AudioFrame *target_buf = thread_get_channel_mix_buffer(send->index_cache, k);

                for (uint32_t j = 0; j < buffer_size; j++) {
                    target_buf[j] += buf[j];
                }
            }
        }
    }

    mix_frames += buffer_size;
    to_mix = buffer_size;
}

bool AudioServer::thread_has_channel_mix_buffer(int p_bus, int p_buffer) const {
    if (p_bus < 0 || p_bus >= buses.size())
        return false;
    if (p_buffer < 0 || p_buffer >= buses[p_bus]->channels.size())
        return false;
    return true;
}

AudioFrame *AudioServer::thread_get_channel_mix_buffer(int p_bus, int p_buffer) {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), nullptr);
    ERR_FAIL_INDEX_V(p_buffer, buses[p_bus]->channels.size(), nullptr);

    AudioFrame *data = buses[p_bus]->channels[p_buffer].buffer.data();

    if (!buses[p_bus]->channels[p_buffer].used) {
        buses[p_bus]->channels[p_buffer].used = true;
        buses[p_bus]->channels[p_buffer].active = true;
        buses[p_bus]->channels[p_buffer].last_mix_with_audio = mix_frames;
        for (uint32_t i = 0; i < buffer_size; i++) {
            data[i] = AudioFrame(0, 0);
        }
    }

    return data;
}

int AudioServer::thread_get_mix_buffer_size() const {

    return buffer_size;
}

int AudioServer::thread_find_bus_index(const StringName &p_name) {

    auto iter = bus_map.find(p_name);
    if (iter!=bus_map.end()) {
        return iter->second->index_cache;
    } else {
        return 0;
    }
}

void AudioServer::set_bus_count(int p_count) {

    ERR_FAIL_COND(p_count < 1);
    ERR_FAIL_INDEX(p_count, 256);

    Object_set_edited(this,true);

    lock();
    int cb = buses.size();

    if (p_count < buses.size()) {
        for (int i = p_count; i < buses.size(); i++) {
            bus_map.erase(buses[i]->name);
            memdelete(buses[i]);
        }
    }

    buses.resize(p_count);

    for (int i = cb; i < buses.size(); i++) {

        String attempt("New Bus");
        int attempts = 1;
        while (true) {

            bool name_free = true;
            for (int j = 0; j < i; j++) {

                if (buses[j]->name == attempt) {
                    name_free = false;
                    break;
                }
            }

            if (!name_free) {
                attempts++;
                attempt = "New Bus " + itos(attempts);
            } else {
                break;
            }
        }

        buses[i] = memnew(AudioServerBus);
        buses[i]->channels.resize(channel_count);
        for (int j = 0; j < channel_count; j++) {
            buses[i]->channels[j].buffer.resize(buffer_size);
        }
        StringName attempt_sn(attempt);
        buses[i]->name = attempt_sn;
        buses[i]->solo = false;
        buses[i]->mute = false;
        buses[i]->bypass = false;
        buses[i]->volume_db = 0;
        if (i > 0) {
            buses[i]->send = "Master";
        }

        bus_map[attempt_sn] = buses[i];
    }

    unlock();

    emit_signal("bus_layout_changed");
}

void AudioServer::remove_bus(int p_index) {

    ERR_FAIL_INDEX(p_index, buses.size());
    ERR_FAIL_COND(p_index == 0);

    Object_set_edited(this,true);

    lock();
    bus_map.erase(buses[p_index]->name);
    memdelete(buses[p_index]);
    buses.erase_at(p_index);
    unlock();

    emit_signal("bus_layout_changed");
}

void AudioServer::add_bus(int p_at_pos) {

    Object_set_edited(this,true);

    if (p_at_pos >= buses.size()) {
        p_at_pos = -1;
    } else if (p_at_pos == 0) {
        if (buses.size() > 1)
            p_at_pos = 1;
        else
            p_at_pos = -1;
    }

    StringName attempt("New Bus");
    int attempts = 1;
    while (true) {

        bool name_free = true;
        for (int j = 0; j < buses.size(); j++) {

            if (buses[j]->name == attempt) {
                name_free = false;
                break;
            }
        }

        if (!name_free) {
            attempts++;
            attempt = StringName("New Bus " + itos(attempts));
        } else {
            break;
        }
    }

    AudioServerBus *bus = memnew(AudioServerBus);
    bus->channels.resize(channel_count);
    for (int j = 0; j < channel_count; j++) {
        bus->channels[j].buffer.resize(buffer_size);
    }
    bus->name = attempt;
    bus->solo = false;
    bus->mute = false;
    bus->bypass = false;
    bus->volume_db = 0;

    bus_map[attempt] = bus;

    if (p_at_pos == -1)
        buses.push_back(bus);
    else
        buses.insert_at(p_at_pos, bus);

    emit_signal("bus_layout_changed");
}

void AudioServer::move_bus(int p_bus, int p_to_pos) {

    ERR_FAIL_COND(p_bus < 1 || p_bus >= buses.size());
    ERR_FAIL_COND(p_to_pos != -1 && (p_to_pos < 1 || p_to_pos > buses.size()));

    Object_set_edited(this,true);

    if (p_bus == p_to_pos)
        return;

    AudioServerBus *bus = buses[p_bus];
    buses.erase_at(p_bus);

    if (p_to_pos == -1) {
        buses.push_back(bus);
    } else if (p_to_pos < p_bus) {
        buses.insert_at(p_to_pos, bus);
    } else {
        buses.insert_at(p_to_pos - 1, bus);
    }

    emit_signal("bus_layout_changed");
}

int AudioServer::get_bus_count() const {

    return buses.size();
}

void AudioServer::set_bus_name(int p_bus, const StringName &p_name) {

    ERR_FAIL_INDEX(p_bus, buses.size());
    if (p_bus == 0 && p_name != StringView("Master")) {
        return; //bus 0 is always master
    }

    Object_set_edited(this,true);

    lock();

    if (buses[p_bus]->name == p_name) {
        unlock();
        return;
    }

    StringName attempt = p_name;
    int attempts = 1;

    while (true) {

        bool name_free = true;
        for (int i = 0; i < buses.size(); i++) {

            if (buses[i]->name == attempt) {
                name_free = false;
                break;
            }
        }

        if (name_free) {
            break;
        }

        attempts++;
        attempt = StringName(p_name + String(" ") + itos(attempts));
    }
    bus_map.erase(buses[p_bus]->name);
    buses[p_bus]->name = attempt;
    bus_map[attempt] = buses[p_bus];
    unlock();

    emit_signal("bus_layout_changed");
}
StringName AudioServer::get_bus_name(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), StringName());
    return buses[p_bus]->name;
}

int AudioServer::get_bus_index(const StringName &p_bus_name) const {
    for (int i = 0; i < buses.size(); ++i) {
        if (buses[i]->name == p_bus_name) {
            return i;
        }
    }
    return -1;
}

void AudioServer::set_bus_volume_db(int p_bus, float p_volume_db) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    buses[p_bus]->volume_db = p_volume_db;
}
float AudioServer::get_bus_volume_db(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
    return buses[p_bus]->volume_db;
}

int AudioServer::get_bus_channels(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
    return buses[p_bus]->channels.size();
}

void AudioServer::set_bus_send(int p_bus, const StringName &p_send) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    buses[p_bus]->send = p_send;
}

StringName AudioServer::get_bus_send(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), StringName());
    return buses[p_bus]->send;
}

void AudioServer::set_bus_solo(int p_bus, bool p_enable) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    buses[p_bus]->solo = p_enable;
}

bool AudioServer::is_bus_solo(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

    return buses[p_bus]->solo;
}

void AudioServer::set_bus_mute(int p_bus, bool p_enable) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    buses[p_bus]->mute = p_enable;
}
bool AudioServer::is_bus_mute(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

    return buses[p_bus]->mute;
}

void AudioServer::set_bus_bypass_effects(int p_bus, bool p_enable) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    buses[p_bus]->bypass = p_enable;
}
bool AudioServer::is_bus_bypassing_effects(int p_bus) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), false);

    return buses[p_bus]->bypass;
}

void AudioServer::_update_bus_effects(int p_bus) {

    for (int i = 0; i < buses[p_bus]->channels.size(); i++) {
        buses[p_bus]->channels[i].effect_instances.resize(buses[p_bus]->effects.size());
        for (int j = 0; j < buses[p_bus]->effects.size(); j++) {
            Ref<AudioEffectInstance> fx = buses[p_bus]->effects[j].effect->instance();
            auto ptr=object_cast<AudioEffectCompressorInstance>(fx.get());
            if (ptr) {
                ptr->set_current_channel(i);
            }
            buses[p_bus]->channels[i].effect_instances[j] = fx;
        }
    }
}

void AudioServer::add_bus_effect(int p_bus, const Ref<AudioEffect> &p_effect, int p_at_pos) {

    ERR_FAIL_COND(not p_effect);
    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    lock();

    AudioServerBus::Effect fx;
    fx.effect = p_effect;
    //fx.instance=p_effect->instance();
    fx.enabled = true;
#ifdef DEBUG_ENABLED
    fx.prof_time = 0;
#endif

    if (p_at_pos >= buses[p_bus]->effects.size() || p_at_pos < 0) {
        buses[p_bus]->effects.push_back(fx);
    } else {
        buses[p_bus]->effects.insert_at(p_at_pos, fx);
    }

    _update_bus_effects(p_bus);

    unlock();
}

void AudioServer::remove_bus_effect(int p_bus, int p_effect) {

    ERR_FAIL_INDEX(p_bus, buses.size());

    Object_set_edited(this,true);

    lock();

    buses[p_bus]->effects.erase_at(p_effect);
    _update_bus_effects(p_bus);

    unlock();
}

int AudioServer::get_bus_effect_count(int p_bus) {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);

    return buses[p_bus]->effects.size();
}

Ref<AudioEffectInstance> AudioServer::get_bus_effect_instance(int p_bus, int p_effect, int p_channel) {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), Ref<AudioEffectInstance>());
    ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), Ref<AudioEffectInstance>());
    ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), Ref<AudioEffectInstance>());

    return buses[p_bus]->channels[p_channel].effect_instances[p_effect];
}

Ref<AudioEffect> AudioServer::get_bus_effect(int p_bus, int p_effect) {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), Ref<AudioEffect>());
    ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), Ref<AudioEffect>());

    return buses[p_bus]->effects[p_effect].effect;
}

void AudioServer::swap_bus_effects(int p_bus, int p_effect, int p_by_effect) {

    ERR_FAIL_INDEX(p_bus, buses.size());
    ERR_FAIL_INDEX(p_effect, buses[p_bus]->effects.size());
    ERR_FAIL_INDEX(p_by_effect, buses[p_bus]->effects.size());

    Object_set_edited(this,true);

    lock();
    SWAP(buses[p_bus]->effects[p_effect], buses[p_bus]->effects[p_by_effect]);
    _update_bus_effects(p_bus);
    unlock();
}

void AudioServer::set_bus_effect_enabled(int p_bus, int p_effect, bool p_enabled) {

    ERR_FAIL_INDEX(p_bus, buses.size());
    ERR_FAIL_INDEX(p_effect, buses[p_bus]->effects.size());

    Object_set_edited(this,true);

    buses[p_bus]->effects[p_effect].enabled = p_enabled;
}
bool AudioServer::is_bus_effect_enabled(int p_bus, int p_effect) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), false);
    ERR_FAIL_INDEX_V(p_effect, buses[p_bus]->effects.size(), false);
    return buses[p_bus]->effects[p_effect].enabled;
}

float AudioServer::get_bus_peak_volume_left_db(int p_bus, int p_channel) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
    ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), 0);

    return buses[p_bus]->channels[p_channel].peak_volume.l;
}
float AudioServer::get_bus_peak_volume_right_db(int p_bus, int p_channel) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), 0);
    ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), 0);

    return buses[p_bus]->channels[p_channel].peak_volume.r;
}

bool AudioServer::is_bus_channel_active(int p_bus, int p_channel) const {

    ERR_FAIL_INDEX_V(p_bus, buses.size(), false);
    ERR_FAIL_INDEX_V(p_channel, buses[p_bus]->channels.size(), false);

    return buses[p_bus]->channels[p_channel].active;
}

void AudioServer::set_global_rate_scale(float p_scale) {
    ERR_FAIL_COND(p_scale <= 0);
    global_rate_scale = p_scale;
}
float AudioServer::get_global_rate_scale() const {

    return global_rate_scale;
}

void AudioServer::init_channels_and_buffers() {
    channel_count = get_channel_count();
    temp_buffer.resize(channel_count);

    for (int i = 0; i < temp_buffer.size(); i++) {
        temp_buffer[i].resize(buffer_size);
    }

    for (int i = 0; i < buses.size(); i++) {
        buses[i]->channels.resize(channel_count);
        for (int j = 0; j < channel_count; j++) {
            buses[i]->channels[j].buffer.resize(buffer_size);
        }
        _update_bus_effects(i);
    }
}

void AudioServer::init() {

    channel_disable_threshold_db = T_GLOBAL_DEF("audio/channel_disable_threshold_db", -60.0f,true);
    channel_disable_frames = T_GLOBAL_DEF("audio/channel_disable_time", 2.0f,true) * get_mix_rate();
    ProjectSettings::get_singleton()->set_custom_property_info("audio/channel_disable_time", PropertyInfo(VariantType::FLOAT, "audio/channel_disable_time", PropertyHint::Range, "0,5,0.01,or_greater"));
    buffer_size = 1024; //hardcoded for now

    init_channels_and_buffers();

    mix_count = 0;
    set_bus_count(1);
    set_bus_name(0, "Master");

    if (AudioDriver::get_singleton())
        AudioDriver::get_singleton()->start();

    Object_set_edited(this,false); //avoid editors from thinking this was edited

    GLOBAL_DEF_RST("audio/video_delay_compensation_ms", 0);
}

void AudioServer::update() {
    SCOPE_AUTONAMED;
#ifdef DEBUG_ENABLED
    if (ScriptDebugger::get_singleton() && ScriptDebugger::get_singleton()->is_profiling()) {

        // Driver time includes server time + effects times
        // Server time includes effects times
        uint64_t driver_time = AudioDriver::get_singleton()->get_profiling_time();
        uint64_t server_time = prof_time;

        // Subtract the server time from the driver time
        if (driver_time > server_time)
            driver_time -= server_time;

        Array values;

        for (int i = buses.size() - 1; i >= 0; i--) {
            AudioServerBus *bus = buses[i];
            if (bus->bypass)
                continue;

            for (int j = 0; j < bus->effects.size(); j++) {
                if (!bus->effects[j].enabled)
                    continue;

                values.push_back(String(bus->name) + bus->effects[j].effect->get_name());
                values.push_back(USEC_TO_SEC(bus->effects[j].prof_time));

                // Subtract the effect time from the driver and server times
                if (driver_time > bus->effects[j].prof_time)
                    driver_time -= bus->effects[j].prof_time;
                if (server_time > bus->effects[j].prof_time)
                    server_time -= bus->effects[j].prof_time;
            }
        }

        values.push_back("audio_server");
        values.push_back(USEC_TO_SEC(server_time));
        values.push_back("audio_driver");
        values.push_back(USEC_TO_SEC(driver_time));

        ScriptDebugger::get_singleton()->add_profiling_frame_data("audio_thread", values);
    }

    // Reset profiling times
    for (int i = buses.size() - 1; i >= 0; i--) {
        AudioServerBus *bus = buses[i];
        if (bus->bypass)
            continue;

        for (int j = 0; j < bus->effects.size(); j++) {
            if (!bus->effects[j].enabled)
                continue;

            bus->effects[j].prof_time = 0;
        }
    }

    AudioDriver::get_singleton()->reset_profiling_time();
    prof_time = 0;
#endif

    for (const CallbackItem &E : update_callbacks) {

        E.callback(E.userdata);
    }

}

void AudioServer::load_default_bus_layout() {

    String layout_path = ProjectSettings::get_singleton()->getT<String>("audio/default_bus_layout");

    if (gResourceManager().exists(layout_path)) {
        Ref<AudioBusLayout> default_layout = dynamic_ref_cast<AudioBusLayout>(gResourceManager().load(layout_path));
        if (default_layout) {
            set_bus_layout(default_layout);
        }
    }
}

void AudioServer::finish() {

    for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
        AudioDriverManager::get_driver(i)->finish();
    }

    for (auto & bus : buses) {
        memdelete(bus);
    }

    buses.clear();
}

/* MISC config */

void AudioServer::lock() {

    AudioDriver::get_singleton()->lock();
}
void AudioServer::unlock() {

    AudioDriver::get_singleton()->unlock();
}

AudioServer::SpeakerMode AudioServer::get_speaker_mode() const {

    return (AudioServer::SpeakerMode)AudioDriver::get_singleton()->get_speaker_mode();
}
float AudioServer::get_mix_rate() const {

    return AudioDriver::get_singleton()->get_mix_rate();
}

float AudioServer::read_output_peak_db() const {

    return 0;
}

AudioServer *AudioServer::get_singleton() {

    return singleton;
}

double AudioServer::get_output_latency() const {

    return AudioDriver::get_singleton()->get_latency();
}

double AudioServer::get_time_to_next_mix() const {

    return AudioDriver::get_singleton()->get_time_to_next_mix();
}

double AudioServer::get_time_since_last_mix() const {

    return AudioDriver::get_singleton()->get_time_since_last_mix();
}

AudioServer *AudioServer::singleton = nullptr;

void *AudioServer::audio_data_alloc(uint32_t p_data_len, const uint8_t *p_from_data) {

    void *ad = memalloc(p_data_len);
    ERR_FAIL_COND_V(!ad, nullptr);
    if (p_from_data) {
        memcpy(ad, p_from_data, p_data_len);
    }

    MutexLock scoped(audio_data_lock);
    audio_data[ad] = p_data_len;
    audio_data_total_mem += p_data_len;
    audio_data_max_mem = M_MAX(audio_data_total_mem, audio_data_max_mem);
    return ad;
}

void AudioServer::audio_data_free(void *p_data) {

    MutexLock scoped(audio_data_lock);
    if (!audio_data.contains(p_data)) {
        ERR_FAIL();
    }

    audio_data_total_mem -= audio_data[p_data];
    audio_data.erase(p_data);
    memfree(p_data);
}

size_t AudioServer::audio_data_get_total_memory_usage() const {

    return audio_data_total_mem;
}
size_t AudioServer::audio_data_get_max_memory_usage() const {

    return audio_data_max_mem;
}

void AudioServer::add_callback(AudioCallback p_callback, void *p_userdata) {
    CallbackItem ci;
    ci.callback = p_callback;
    ci.userdata = p_userdata;
    lock();
    if(!update_callbacks.contains(ci)) {
        update_callbacks.emplace_back(ci);
    }
    unlock();
}

void AudioServer::remove_callback(AudioCallback p_callback, void *p_userdata) {

    CallbackItem ci;
    ci.callback = p_callback;
    ci.userdata = p_userdata;
    lock();
    callbacks.erase_first_unsorted(ci);
    unlock();
}

void AudioServer::add_update_callback(AudioCallback p_callback, void *p_userdata) {
    CallbackItem ci;
    ci.callback = p_callback;
    ci.userdata = p_userdata;
    lock();
    if(!update_callbacks.contains(ci)) {
        update_callbacks.emplace_back(ci);
    }
    unlock();
}

void AudioServer::remove_update_callback(AudioCallback p_callback, void *p_userdata) {

    CallbackItem ci;
    ci.callback = p_callback;
    ci.userdata = p_userdata;
    lock();
    update_callbacks.erase_first_unsorted(ci);
    unlock();
}

void AudioServer::set_bus_layout(const Ref<AudioBusLayout> &p_bus_layout) {

    ERR_FAIL_COND(not p_bus_layout || p_bus_layout->bus_count()==0);

    lock();
    for (int i = 0; i < buses.size(); i++) {
        memdelete(buses[i]);
    }
    buses.resize(p_bus_layout->bus_count());
    bus_map.clear();
    for (int i = 0; i < p_bus_layout->bus_count(); i++) {
        AudioServerBus *bus = memnew(AudioServerBus);
        p_bus_layout->fill_bus_info(i,bus);

        bus_map[bus->name] = bus;
        buses[i] = bus;

        buses[i]->channels.resize(channel_count);
        for (int j = 0; j < channel_count; j++) {
            buses[i]->channels[j].buffer.resize(buffer_size);
        }
        _update_bus_effects(i);
    }
    Object_set_edited(this,false);
    unlock();
}

Ref<AudioBusLayout> AudioServer::generate_bus_layout() const {

    Ref<AudioBusLayout> state(make_ref_counted<AudioBusLayout>());
    state->generate_bus_layout(buses);

    return state;
}

Array AudioServer::get_device_list() {

    return AudioDriver::get_singleton()->get_device_list();
}

StringView AudioServer::get_device() {

    return AudioDriver::get_singleton()->get_device();
}

void AudioServer::set_device(StringView device) {

    AudioDriver::get_singleton()->set_device(device);
}

Array AudioServer::capture_get_device_list() {

    return AudioDriver::get_singleton()->capture_get_device_list();
}

String AudioServer::capture_get_device() {

    return AudioDriver::get_singleton()->capture_get_device();
}

void AudioServer::capture_set_device(StringView p_name) {

    AudioDriver::get_singleton()->capture_set_device(p_name);
}

void AudioServer::_bind_methods() {

    SE_BIND_METHOD(AudioServer,set_bus_count);
    SE_BIND_METHOD(AudioServer,get_bus_count);

    SE_BIND_METHOD(AudioServer,remove_bus);
    MethodBinder::bind_method(D_METHOD("add_bus", {"at_position"}), &AudioServer::add_bus, {DEFVAL(-1)});
    SE_BIND_METHOD(AudioServer,move_bus);

    SE_BIND_METHOD(AudioServer,set_bus_name);
    SE_BIND_METHOD(AudioServer,get_bus_name);
    SE_BIND_METHOD(AudioServer,get_bus_index);

    SE_BIND_METHOD(AudioServer,get_bus_channels);

    SE_BIND_METHOD(AudioServer,set_bus_volume_db);
    SE_BIND_METHOD(AudioServer,get_bus_volume_db);

    SE_BIND_METHOD(AudioServer,set_bus_send);
    SE_BIND_METHOD(AudioServer,get_bus_send);

    SE_BIND_METHOD(AudioServer,set_bus_solo);
    SE_BIND_METHOD(AudioServer,is_bus_solo);

    SE_BIND_METHOD(AudioServer,set_bus_mute);
    SE_BIND_METHOD(AudioServer,is_bus_mute);

    SE_BIND_METHOD(AudioServer,set_bus_bypass_effects);
    SE_BIND_METHOD(AudioServer,is_bus_bypassing_effects);

    MethodBinder::bind_method(D_METHOD("add_bus_effect", {"bus_idx", "effect", "at_position"}), &AudioServer::add_bus_effect, {DEFVAL(-1)});
    SE_BIND_METHOD(AudioServer,remove_bus_effect);

    SE_BIND_METHOD(AudioServer,get_bus_effect_count);
    SE_BIND_METHOD(AudioServer,get_bus_effect);
    MethodBinder::bind_method(D_METHOD("get_bus_effect_instance", {"bus_idx", "effect_idx", "channel"}), &AudioServer::get_bus_effect_instance, {DEFVAL(0)});
    SE_BIND_METHOD(AudioServer,swap_bus_effects);

    SE_BIND_METHOD(AudioServer,set_bus_effect_enabled);
    SE_BIND_METHOD(AudioServer,is_bus_effect_enabled);

    SE_BIND_METHOD(AudioServer,get_bus_peak_volume_left_db);
    SE_BIND_METHOD(AudioServer,get_bus_peak_volume_right_db);

    SE_BIND_METHOD(AudioServer,set_global_rate_scale);
    SE_BIND_METHOD(AudioServer,get_global_rate_scale);

    SE_BIND_METHOD(AudioServer,lock);
    SE_BIND_METHOD(AudioServer,unlock);

    SE_BIND_METHOD(AudioServer,get_speaker_mode);
    SE_BIND_METHOD(AudioServer,get_mix_rate);
    SE_BIND_METHOD(AudioServer,get_device_list);
    SE_BIND_METHOD(AudioServer,get_device);
    SE_BIND_METHOD(AudioServer,set_device);

    SE_BIND_METHOD(AudioServer,get_time_to_next_mix);
    SE_BIND_METHOD(AudioServer,get_time_since_last_mix);
    SE_BIND_METHOD(AudioServer,get_output_latency);

    SE_BIND_METHOD(AudioServer,capture_get_device_list);
    SE_BIND_METHOD(AudioServer,capture_get_device);
    SE_BIND_METHOD(AudioServer,capture_set_device);

    SE_BIND_METHOD(AudioServer,set_bus_layout);
    SE_BIND_METHOD(AudioServer,generate_bus_layout);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "bus_count"), "set_bus_count", "get_bus_count");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "device"), "set_device", "get_device");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "capture_device"), "capture_set_device", "capture_get_device");
    // The default value may be set to an empty string by the platform-specific audio driver.
    // Override for class reference generation purposes.
    ADD_PROPERTY_DEFAULT("capture_device", "Default");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "global_rate_scale"), "set_global_rate_scale", "get_global_rate_scale");

    ADD_SIGNAL(MethodInfo("bus_layout_changed"));

    BIND_ENUM_CONSTANT(SPEAKER_MODE_STEREO);
    BIND_ENUM_CONSTANT(SPEAKER_SURROUND_31);
    BIND_ENUM_CONSTANT(SPEAKER_SURROUND_51);
    BIND_ENUM_CONSTANT(SPEAKER_SURROUND_71);
}

AudioServer::AudioServer() {

    singleton = this;
    audio_data_total_mem = 0;
    audio_data_max_mem = 0;
    mix_frames = 0;
    channel_count = 0;
    to_mix = 0;
#ifdef DEBUG_ENABLED
    prof_time = 0;
#endif
    mix_time = 0;
    mix_size = 0;
    global_rate_scale = 1;
}

AudioServer::~AudioServer() {

    singleton = nullptr;
}

/////////////////////////////////
namespace {
    struct AudioBusLayout_priv {
        struct Bus {

            StringName name;
            bool solo;
            bool mute;
            bool bypass;

            struct Effect {
                Ref<AudioEffect> effect;
                bool enabled;
            };

            Vector<Effect> effects;

            float volume_db;
            StringName send;

            Bus() {
                solo = false;
                mute = false;
                bypass = false;
                volume_db = 0;
            }
        };

        Vector<Bus> buses;
        AudioBusLayout_priv() {
            buses.resize(1);
            buses[0].name = "Master";
        }
    };
} // end of anonymous namespace
#define D() ((AudioBusLayout_priv *)m_priv)

void AudioBusLayout::generate_bus_layout(const Vector<AudioServerBus *> &buses) {
    D()->buses.resize(buses.size());

    for (int i = 0; i < buses.size(); i++) {
        auto &tgt_bus(D()->buses[i]);
        tgt_bus.name = buses[i]->name;
        tgt_bus.send = buses[i]->send;
        tgt_bus.mute = buses[i]->mute;
        tgt_bus.solo = buses[i]->solo;
        tgt_bus.bypass = buses[i]->bypass;
        tgt_bus.volume_db = buses[i]->volume_db;
        for (int j = 0; j < buses[i]->effects.size(); j++) {
            AudioBusLayout_priv::Bus::Effect fx;
            fx.effect = buses[i]->effects[j].effect;
            fx.enabled = buses[i]->effects[j].enabled;
            tgt_bus.effects.push_back(fx);
        }
    }

}

size_t AudioBusLayout::bus_count() const {
    return D()->buses.size();
}

void AudioBusLayout::fill_bus_info(int i, AudioServerBus *bus) {

    if (i == 0) {
        bus->name = "Master";
    }
    else {
        bus->name = D()->buses[i].name;
        bus->send = D()->buses[i].send;
    }

    bus->solo = D()->buses[i].solo;
    bus->mute = D()->buses[i].mute;
    bus->bypass = D()->buses[i].bypass;
    bus->volume_db = D()->buses[i].volume_db;

    for (int j = 0; j < D()->buses[i].effects.size(); j++) {

        Ref<AudioEffect> fx = D()->buses[i].effects[j].effect;

        if (fx) {

            AudioServerBus::Effect bfx;
            bfx.effect = fx;
            bfx.enabled = D()->buses[i].effects[j].enabled;
            bus->effects.push_back(bfx);
        }
    }
}

bool AudioBusLayout::_set(const StringName &p_name, const Variant &p_value) {

    StringView s(p_name);
    if (!StringUtils::begins_with(s,"bus/"))
        return false;

    int index = StringUtils::to_int(StringUtils::get_slice(s,"/", 1));
    if (D()->buses.size() <= index) {
        D()->buses.resize(index + 1);
    }

    auto &bus = D()->buses[index];

    StringView what = StringUtils::get_slice(s,"/", 2);

    if (what == "name"_sv) {
        bus.name = p_value.as<StringName>();
    } else if (what == "solo"_sv) {
        bus.solo = p_value.as<bool>();
    } else if (what == "mute"_sv) {
        bus.mute = p_value.as<bool>();
    } else if (what == "bypass_fx"_sv) {
        bus.bypass = p_value.as<bool>();
    } else if (what == "volume_db"_sv) {
        bus.volume_db = p_value.as<float>();
    } else if (what == "send"_sv) {
        bus.send = p_value.as<StringName>();
    } else if (what == "effect"_sv) {
        int which = StringUtils::to_int(StringUtils::get_slice(s,"/", 3));
        if (bus.effects.size() <= which) {
            bus.effects.resize(which + 1);
        }

        auto &fx = bus.effects[which];

        StringView fxwhat = StringUtils::get_slice(s,"/", 4);
        if (fxwhat == "effect"_sv) {
            fx.effect = refFromVariant<AudioEffect>(p_value);
        } else if (fxwhat == "enabled"_sv) {
            fx.enabled = p_value.as<bool>();
        } else {
            return false;
        }

        return true;
    } else {
        return false;
    }

    return true;

}

bool AudioBusLayout::_get(const StringName &p_name, Variant &r_ret) const {

    StringView s = p_name;
    if (StringUtils::begins_with(s,"bus/")) {

        int index = StringUtils::to_int(StringUtils::get_slice(s,"/", 1));
        if (index < 0 || index >= D()->buses.size())
            return false;

        auto &bus = D()->buses[index];

        StringView what = StringUtils::get_slice(s,"/", 2);

        if (what == "name"_sv) {
            r_ret = bus.name;
        } else if (what == "solo"_sv) {
            r_ret = bus.solo;
        } else if (what == "mute"_sv) {
            r_ret = bus.mute;
        } else if (what == "bypass_fx"_sv) {
            r_ret = bus.bypass;
        } else if (what == "volume_db"_sv) {
            r_ret = bus.volume_db;
        } else if (what == "send"_sv) {
            r_ret = bus.send;
        } else if (what == "effect"_sv) {
            int which = StringUtils::to_int(StringUtils::get_slice(s,"/", 3));
            if (which < 0 || which >= bus.effects.size()) {
                return false;
            }

            const auto &fx = bus.effects[which];

            StringView fxwhat = StringUtils::get_slice(s,"/", 4);
            if (fxwhat == "effect"_sv) {
                r_ret = fx.effect;
            } else if (fxwhat == "enabled"_sv) {
                r_ret = fx.enabled;
            } else {
                return false;
            }

            return true;
        } else {
            return false;
        }

        return true;
    }

    return false;
}
void AudioBusLayout::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (int i = 0; i < D()->buses.size(); i++) {
        p_list->push_back(PropertyInfo(VariantType::STRING, StringName("bus/" + itos(i) + "/name"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName("bus/" + itos(i) + "/solo"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName("bus/" + itos(i) + "/mute"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::BOOL, StringName("bus/" + itos(i) + "/bypass_fx"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName("bus/" + itos(i) + "/volume_db"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        p_list->push_back(PropertyInfo(VariantType::FLOAT, StringName("bus/" + itos(i) + "/send"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));

        for (int j = 0; j < D()->buses[i].effects.size(); j++) {
            p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("bus/" + itos(i) + "/effect/" + itos(j) + "/effect"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
            p_list->push_back(PropertyInfo(VariantType::BOOL, StringName("bus/" + itos(i) + "/effect/" + itos(j) + "/enabled"), PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
        }
    }
}

AudioBusLayout::AudioBusLayout() {
    m_priv = memnew(AudioBusLayout_priv);
}

AudioBusLayout::~AudioBusLayout() {
    memdelete(D());
    m_priv = nullptr;
}

