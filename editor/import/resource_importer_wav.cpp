/*************************************************************************/
/*  resource_importer_wav.cpp                                            */
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

#include "resource_importer_wav.h"

#include "core/class_db.h"
#include "core/io/marshalls.h"
#include "core/os/file_access.h"
#include "core/resource/resource_manager.h"
#include "scene/resources/audio_stream_sample.h"

IMPL_GDCLASS(ResourceImporterWAV)

const float TRIM_DB_LIMIT = -50;
const int TRIM_FADE_OUT_FRAMES = 500;

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
        int16_t xm_sample;

        if (i >= datamax)
            xm_sample = 0;
        else
        {
            xm_sample = CLAMP<float>(in[i] * 32767.0f, -32768, 32767);
            /*
            if (xm_sample==32767 || xm_sample==-32768)
                printf("clippy!\n",xm_sample);
            */
        }

        //xm_sample=xm_sample+xm_prev;
        //xm_prev=xm_sample;

        int diff = (int)xm_sample - prev;

        uint8_t nibble = 0;
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

const char *ResourceImporterWAV::get_importer_name() const {

    return "wav";
}

const char *ResourceImporterWAV::get_visible_name() const {

    return "Microsoft WAV";
}
void ResourceImporterWAV::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.emplace_back("wav");
}
StringName ResourceImporterWAV::get_save_extension() const {
    return "sample";
}

StringName ResourceImporterWAV::get_resource_type() const {

    return "AudioStreamSample";
}

bool ResourceImporterWAV::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {

    if (p_option == "force/max_rate_hz" && !p_options.at("force/max_rate").as<bool>()) {
        return false;
    }

    // Don't show begin/end loop points if loop mode is auto-detected or disabled.
    if ((p_option == "edit/loop_begin" || p_option == "edit/loop_end") && p_options.at("edit/loop_mode").as<int>() < 2) {
        return false;
    }
    return true;
}


int ResourceImporterWAV::get_preset_count() const {
    return 0;
}
StringName ResourceImporterWAV::get_preset_name(int p_idx) const {

    return StringName();
}

void ResourceImporterWAV::get_import_options(Vector<ResourceImporterInterface::ImportOption> *r_options, int p_preset) const {

    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "force/8_bit"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "force/mono"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "force/max_rate", PropertyHint::None, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::FLOAT, "force/max_rate_hz", PropertyHint::ExpRange, "11025,192000,1"), 44100));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "edit/trim"), false));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::BOOL, "edit/normalize"), false));
    // Keep the `edit/loop_mode` enum in sync with AudioStreamSample::LoopMode (note: +1 offset due to "Detect From WAV").
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "edit/loop_mode", PropertyHint::Enum, "Detect From WAV,Disabled,Forward,Ping-Pong,Backward",
                                              PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED),
            0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "edit/loop_begin"), 0));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "edit/loop_end"), -1));
    r_options->push_back(ImportOption(PropertyInfo(VariantType::INT, "compress/mode", PropertyHint::Enum, "Disabled,RAM (Ima-ADPCM)"), 0));
}

Error ResourceImporterWAV::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
    Vector<String> *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    /* STEP 1, READ WAVE FILE */

    Error err;
    FileAccessRef<true> file(FileAccess::open(p_source_file, FileAccess::READ, &err));

    ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, String("Cannot open file '") + p_source_file + "'.");

    /* CHECK RIFF */
    char riff[5];
    riff[4] = 0;
    file->get_buffer((uint8_t *)&riff, 4); //RIFF

    if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
        ERR_FAIL_V(ERR_FILE_UNRECOGNIZED);
    }

    /* GET FILESIZE */
    file->get_32(); // filesize

    /* CHECK WAVE */

    char wave[4];

    file->get_buffer((uint8_t *)&wave, 4); //RIFF

    if (wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E') {
        ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Not a WAV file (no WAVE RIFF header).");
    }
    // Let users override potential loop points from the WAV.
    // We parse the WAV loop points only with "Detect From WAV" (0).
    int import_loop_mode = p_options.at("edit/loop_mode").as<int>();

    uint16_t format_bits = 0;
    uint16_t format_channels = 0;

    AudioStreamSample::LoopMode loop_mode = AudioStreamSample::LOOP_DISABLED;
    uint16_t compression_code = 1;
    bool format_found = false;
    bool data_found = false;
    uint32_t format_freq = 0;
    int32_t loop_begin = 0;
    int32_t loop_end = 0;
    size_t frames = 0;

    Vector<float> data;

    while (!file->eof_reached()) {

        /* chunk */
        char chunkID[4];
        file->get_buffer((uint8_t *)&chunkID, 4); //RIFF

        /* chunk size */
        uint32_t chunksize = file->get_32();
        size_t file_pos = file->get_position(); //save file pos, so we can skip to next chunk safely

        if (file->eof_reached()) {

            //ERR_PRINT("EOF REACH");
            break;
        }

        if (chunkID[0] == 'f' && chunkID[1] == 'm' && chunkID[2] == 't' && chunkID[3] == ' ' && !format_found) {
            /* IS FORMAT CHUNK */

            //Issue: #7755 : Not a bug - usage of other formats (format codes) are unsupported in current importer version.
            //Consider revision for engine version 3.0
            compression_code = file->get_16();
            if (compression_code != 1 && compression_code != 3) {
                ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Format not supported for WAVE file (not PCM). Save WAVE files as uncompressed PCM or IEEE float instead.");
            }

            format_channels = file->get_16();
            if (format_channels != 1 && format_channels != 2) {
                ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Format not supported for WAVE file (not stereo or mono).");
            }

            format_freq = file->get_32(); //sampling rate

            file->get_32(); // average bits/second (unused)
            file->get_16(); // block align (unused)
            format_bits = file->get_16(); // bits per sample

            if (format_bits % 8 || format_bits == 0) {
                ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid amount of bits in the sample (should be one of 8, 16, 24 or 32).");
            }

            if (compression_code == 3 && format_bits % 32) {
                file->close();
                ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid amount of bits in the IEEE float sample (should be 32 or 64).");
            }
            /* Don't need anything else, continue */
            format_found = true;
        }

        if (chunkID[0] == 'd' && chunkID[1] == 'a' && chunkID[2] == 't' && chunkID[3] == 'a' && !data_found) {
            /* IS DATA CHUNK */
            data_found = true;

            if (!format_found) {
                ERR_PRINT("'data' chunk before 'format' chunk found.");
                break;
            }

            frames = chunksize;

            ERR_FAIL_COND_V(format_channels == 0, ERR_INVALID_DATA);

            frames /= format_channels;
            frames /= format_bits >> 3;

            /*print_line("chunksize: "+itos(chunksize));
            print_line("channels: "+itos(format_channels));
            print_line("bits: "+itos(format_bits));
            */

            data.resize(frames * format_channels);

            if (compression_code == 1) {
            if (format_bits == 8) {
                for (size_t i = 0; i < frames * format_channels; i++) {
                    // 8 bit samples are UNSIGNED

                    data[i] = int8_t(file->get_8() - 128) / 128.f;
                }
                }
                else if (format_bits == 16) {
                for (size_t i = 0; i < frames * format_channels; i++) {
                    //16 bit SIGNED

                    data[i] = int16_t(file->get_16()) / 32768.f;
                }
                }
                else {
                for (size_t i = 0; i < frames * format_channels; i++) {
                    //16+ bits samples are SIGNED
                    // if sample is > 16 bits, just read extra bytes

                    uint32_t s = 0;
                    for (int b = 0; b < format_bits >> 3; b++) {

                        s |= (uint32_t)file->get_8() << b * 8;
                    }
                    s <<= 32 - format_bits;

                    data[i] = (int32_t(s) >> 16) / 32768.f;
                }
            }
            }
            else if (compression_code == 3) {
                if (format_bits == 32) {
                    for (int i = 0; i < frames * format_channels; i++) {
                        //32 bit IEEE Float

                        data[i] = file->get_float();
                    }
                }
                else if (format_bits == 64) {
                    for (int i = 0; i < frames * format_channels; i++) {
                        //64 bit IEEE Float

                        data[i] = file->get_double();
                    }
                }
            }
            if (file->eof_reached()) {
                ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Premature end of file.");
            }
        }

        if (import_loop_mode == 0 && chunkID[0] == 's' && chunkID[1] == 'm' && chunkID[2] == 'p' && chunkID[3] == 'l') {
            // Loop point info!

            /**
            *    Consider exploring next document:
            *        http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/RIFFNEW.pdf
            *    Especially on page:
            *        16 - 17
            *    Timestamp:
            *        22:38 06.07.2017 GMT
            **/

            for (int i = 0; i < 10; i++)
                file->get_32(); // i wish to know why should i do this... no doc!

            // only read 0x00 (loop forward), 0x01 (loop ping-pong) and 0x02 (loop backward)
            // Skip anything else because it's not supported, reserved for future uses or sampler specific
            // from https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl (loop type values table)
            uint32_t loop_type = file->get_32();
            if (loop_type == 0x00 || loop_type == 0x01 || loop_type == 0x02) {
                if (loop_type == 0x00) {
                    loop_mode = AudioStreamSample::LOOP_FORWARD;
                } else if (loop_type == 0x01) {
                    loop_mode = AudioStreamSample::LOOP_PING_PONG;
                } else if (loop_type == 0x02) {
                    loop_mode = AudioStreamSample::LOOP_BACKWARD;
                }
                loop_begin = file->get_32();
                loop_end = file->get_32();
            }
        }
        file->seek(file_pos + chunksize);
    }

    // STEP 2, APPLY CONVERSIONS

    bool is16 = format_bits != 8;
    int rate = format_freq;

    /*
    print_line("Input Sample: ");
    print_line("\tframes: " + itos(frames));
    print_line("\tformat_channels: " + itos(format_channels));
    print_line("\t16bits: " + itos(is16));
    print_line("\trate: " + itos(rate));
    print_line("\tloop: " + itos(loop));
    print_line("\tloop begin: " + itos(loop_begin));
    print_line("\tloop end: " + itos(loop_end));
    */

    //apply frequency limit

    bool limit_rate = p_options.at("force/max_rate").as<bool>();
    int limit_rate_hz = p_options.at("force/max_rate_hz").as<int>();
    if (limit_rate && rate > limit_rate_hz && rate > 0 && frames > 0) {
        // resample!
        int new_data_frames = (int)(frames * (float)limit_rate_hz / (float)rate);

        Vector<float> new_data;
        new_data.resize(new_data_frames * format_channels);
        for (int c = 0; c < format_channels; c++) {

            float frac = .0f;
            int ipos = 0;

            for (int i = 0; i < new_data_frames; i++) {

                //simple cubic interpolation should be enough.

                float mu = frac;

                float y0 = data[M_MAX(0, ipos - 1) * format_channels + c];
                float y1 = data[ipos * format_channels + c];
                float y2 = data[MIN(frames - 1, ipos + 1) * format_channels + c];
                float y3 = data[MIN(frames - 1, ipos + 2) * format_channels + c];

                float mu2 = mu * mu;
                float a0 = y3 - y2 - y0 + y1;
                float a1 = y0 - y1 - a0;
                float a2 = y2 - y0;
                float a3 = y1;

                float res = a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;

                new_data[i * format_channels + c] = res;

                // update position and always keep fractional part within ]0...1]
                // in order to avoid 32bit floating point precision errors

                frac += (float)rate / (float)limit_rate_hz;
                int tpos = (int)Math::floor(frac);
                ipos += tpos;
                frac -= tpos;
            }
        }

        if (loop_mode) {
            loop_begin = (int)(loop_begin * (float)new_data_frames / (float)frames);
            loop_end = (int)(loop_end * (float)new_data_frames / (float)frames);
        }

        data = new_data;
        rate = limit_rate_hz;
        frames = new_data_frames;
    }

    bool normalize = p_options.at("edit/normalize").as<bool>();

    if (normalize) {

        float max = 0;
        for (float v : data) {
            float amp = Math::abs(v);
            if (amp > max)
                max = amp;
        }

        if (max > 0) {

            float mult = 1.0f / max;
            for (float & v : data) {
                v *= mult;
            }
        }
    }

    bool trim = p_options.at("edit/trim").as<bool>();

    if (trim && (loop_mode != AudioStreamSample::LOOP_DISABLED) && format_channels > 0) {

        int first = 0;
        int last = frames / format_channels - 1;
        bool found = false;
        float limit = Math::db2linear(TRIM_DB_LIMIT);

        for (int i = 0; i < data.size() / format_channels; i++) {
            float ampChannelSum = 0;
            for (int j = 0; j < format_channels; j++) {
                ampChannelSum += Math::abs(data[i * format_channels + j]);
            }

            float amp = Math::abs(ampChannelSum / (float)format_channels);

            if (!found && amp > limit) {
                first = i;
                found = true;
            }

            if (found && amp > limit) {
                last = i;
            }
        }

        if (first < last) {
            Vector<float> new_data;
            new_data.resize((last - first) * format_channels);
            for (int i = first; i < last; i++) {

                float fadeOutMult = 1;

                if (last - i < TRIM_FADE_OUT_FRAMES) {
                    fadeOutMult = (float)(last - i - 1) / (float)TRIM_FADE_OUT_FRAMES;
                }

                for (int j = 0; j < format_channels; j++) {
                    new_data[(i - first) * format_channels + j] = data[i * format_channels + j] * fadeOutMult;
                }
            }

            data = new_data;
            frames = data.size() / format_channels;
        }
    }


    if (import_loop_mode >= 2) {
        loop_mode = (AudioStreamSample::LoopMode)(import_loop_mode - 1);
        loop_begin = p_options.at("edit/loop_begin").as<int>();
        loop_end = p_options.at("edit/loop_end").as<int>();
        // Wrap around to max frames, so `-1` can be used to select the end, etc.
        if (loop_begin < 0) {
            loop_begin = CLAMP<int>(loop_begin + frames + 1, 0, frames);
        }
        if (loop_end < 0) {
            loop_end = CLAMP<int>(loop_end + frames + 1, 0, frames);
        }
    }

    int compression = p_options.at("compress/mode").as<int>();
    bool force_mono = p_options.at("force/mono").as<bool>();

    if (force_mono && format_channels == 2) {

        Vector<float> new_data;
        new_data.resize(data.size() / 2);
        for (size_t i = 0; i < frames; i++) {
            new_data[i] = (data[i * 2 + 0] + data[i * 2 + 1]) / 2.0f;
        }

        data = new_data;
        format_channels = 1;
    }

    bool force_8_bit = p_options.at("force/8_bit").as<bool>();
    if (force_8_bit) {

        is16 = false;
    }

    Vector<uint8_t> dst_data;
    AudioStreamSample::Format dst_format;

    if (compression == 1) {

        dst_format = AudioStreamSample::FORMAT_IMA_ADPCM;
        if (format_channels == 1) {
            WAV_compress_ima_adpcm(data, dst_data);
        } else {

            //byte interleave
            Vector<float> left;
            Vector<float> right;

            int tframes = data.size() / 2;
            left.resize(tframes);
            right.resize(tframes);

            for (int i = 0; i < tframes; i++) {
                left[i] = data[i * 2 + 0];
                right[i] = data[i * 2 + 1];
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
        }

    } else {

        dst_format = is16 ? AudioStreamSample::FORMAT_16_BITS : AudioStreamSample::FORMAT_8_BITS;
        dst_data.resize(data.size() * (is16 ? 2 : 1));
        {
            size_t ds = data.size();
            for (size_t i = 0; i < ds; i++) {

                if (is16) {
                    int16_t v = CLAMP<float>(data[i] * 32768, -32768, 32767);
                    encode_uint16(v, &dst_data[i * 2]);
                } else {
                    int8_t v = CLAMP<float>(data[i] * 128, -128, 127);
                    dst_data[i] = v;
                }
            }
        }
    }

    Ref<AudioStreamSample> sample(make_ref_counted<AudioStreamSample>());
    sample->set_data(dst_data);
    sample->set_format(dst_format);
    sample->set_mix_rate(rate);
    sample->set_loop_mode(loop_mode);
    sample->set_loop_begin(loop_begin);
    sample->set_loop_end(loop_end);
    sample->set_stereo(format_channels == 2);

    gResourceManager().save(String(p_save_path) + ".sample", sample);

    return OK;
}

ResourceImporterWAV::ResourceImporterWAV() = default;
