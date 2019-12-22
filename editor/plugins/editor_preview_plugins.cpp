/*************************************************************************/
/*  editor_preview_plugins.cpp                                           */
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

#include "editor_preview_plugins.h"

#include "core/io/file_access_memory.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/method_bind.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

#include "servers/visual_server.h"
#include "servers/audio/audio_stream.h"

IMPL_GDCLASS(EditorTexturePreviewPlugin)
IMPL_GDCLASS(EditorImagePreviewPlugin)
IMPL_GDCLASS(EditorBitmapPreviewPlugin)
IMPL_GDCLASS(EditorMaterialPreviewPlugin)
IMPL_GDCLASS(EditorMeshPreviewPlugin)
IMPL_GDCLASS(EditorFontPreviewPlugin)

void post_process_preview(Ref<Image> p_image) {

    if (p_image->get_format() != Image::FORMAT_RGBA8)
        p_image->convert(Image::FORMAT_RGBA8);

    p_image->lock();

    const int w = p_image->get_width();
    const int h = p_image->get_height();

    const int r = MIN(w, h) / 32;
    const int r2 = r * r;
    Color transparent = Color(0, 0, 0, 0);

    for (int i = 0; i < r; i++) {
        for (int j = 0; j < r; j++) {
            int dx = i - r;
            int dy = j - r;
            if (dx * dx + dy * dy > r2) {
                p_image->set_pixel(i, j, transparent);
                p_image->set_pixel(w - 1 - i, j, transparent);
                p_image->set_pixel(w - 1 - i, h - 1 - j, transparent);
                p_image->set_pixel(i, h - 1 - j, transparent);
            } else {
                break;
            }
        }
    }

    p_image->unlock();
}

bool EditorTexturePreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "Texture");
}

bool EditorTexturePreviewPlugin::generate_small_preview_automatically() const {
    return true;
}

Ref<Texture> EditorTexturePreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<Image> img;
    Ref<AtlasTexture> atex = dynamic_ref_cast<AtlasTexture>(p_from);
    Ref<LargeTexture> ltex = dynamic_ref_cast<LargeTexture>(p_from);
    if (atex) {
        Ref<Texture> tex = atex->get_atlas();
        if (not tex) {
            return Ref<Texture>();
        }

        Ref<Image> atlas = tex->get_data();
        if (not atlas) {
            return Ref<Texture>();
        }

        img = atlas->get_rect(atex->get_region());
    } else if (ltex) {
        img = ltex->to_image();
    } else {
        Ref<Texture> tex = dynamic_ref_cast<Texture>(p_from);
        if(tex) {
            img = tex->get_data();
            if (img) {
                img = dynamic_ref_cast<Image>(img->duplicate());
            }
        }
    }

    if (not img || img->empty())
        return Ref<Texture>();

    img->clear_mipmaps();

    if (img->is_compressed()) {
        if (img->decompress() != OK)
            return Ref<Texture>();
    } else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
        img->convert(Image::FORMAT_RGBA8);
    }

    Vector2 new_size = img->get_size();
    if (new_size.x > p_size.x) {
        new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
    }
    if (new_size.y > p_size.y) {
        new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
    }
    img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());

    ptex->create_from_image(img, 0);
    return ptex;
}

EditorTexturePreviewPlugin::EditorTexturePreviewPlugin() {
}

////////////////////////////////////////////////////////////////////////////

bool EditorImagePreviewPlugin::handles(se_string_view p_type) const {

    return p_type == se_string_view("Image");
}

Ref<Texture> EditorImagePreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<Image> img = dynamic_ref_cast<Image>(p_from);

    if (not img || img->empty())
        return Ref<Texture>();

    img = dynamic_ref_cast<Image>(img->duplicate());
    img->clear_mipmaps();

    if (img->is_compressed()) {
        if (img->decompress() != OK)
            return Ref<Texture>();
    } else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
        img->convert(Image::FORMAT_RGBA8);
    }

    Vector2 new_size = img->get_size();
    if (new_size.x > p_size.x) {
        new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
    }
    if (new_size.y > p_size.y) {
        new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
    }
    img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());

    ptex->create_from_image(img, 0);
    return ptex;
}

EditorImagePreviewPlugin::EditorImagePreviewPlugin() {
}

bool EditorImagePreviewPlugin::generate_small_preview_automatically() const {
    return true;
}
////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////
bool EditorBitmapPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "BitMap");
}

Ref<Texture> EditorBitmapPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<BitMap> bm = dynamic_ref_cast<BitMap>(p_from);

    if (bm->get_size() == Size2()) {
        return Ref<Texture>();
    }

    PoolVector<uint8_t> data;

    data.resize(bm->get_size().width * bm->get_size().height);

    {
        PoolVector<uint8_t>::Write w = data.write();

        for (int i = 0; i < bm->get_size().width; i++) {
            for (int j = 0; j < bm->get_size().height; j++) {
                if (bm->get_bit(Point2i(i, j))) {
                    w[j * bm->get_size().width + i] = 255;
                } else {
                    w[j * bm->get_size().width + i] = 0;
                }
            }
        }
    }

    Ref<Image> img(make_ref_counted<Image>());
    img->create(bm->get_size().width, bm->get_size().height, false, Image::FORMAT_L8, data);

    if (img->is_compressed()) {
        if (img->decompress() != OK)
            return Ref<Texture>();
    } else if (img->get_format() != Image::FORMAT_RGB8 && img->get_format() != Image::FORMAT_RGBA8) {
        img->convert(Image::FORMAT_RGBA8);
    }

    Vector2 new_size = img->get_size();
    if (new_size.x > p_size.x) {
        new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
    }
    if (new_size.y > p_size.y) {
        new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
    }
    img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());

    ptex->create_from_image(img, 0);
    return ptex;
}

bool EditorBitmapPreviewPlugin::generate_small_preview_automatically() const {
    return true;
}

EditorBitmapPreviewPlugin::EditorBitmapPreviewPlugin() {
}

///////////////////////////////////////////////////////////////////////////

bool EditorPackedScenePreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "PackedScene");
}
Ref<Texture> EditorPackedScenePreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    return generate_from_path(p_from->get_path(), p_size);
}

Ref<Texture> EditorPackedScenePreviewPlugin::generate_from_path(se_string_view p_path, const Size2 &p_size) const {

    se_string temp_path = EditorSettings::get_singleton()->get_cache_dir();
    se_string cache_base = StringUtils::md5_text(ProjectSettings::get_singleton()->globalize_path(p_path));
    cache_base = PathUtils::plus_file(temp_path,"resthumb-" + cache_base);

    //does not have it, try to load a cached thumbnail

    se_string path = cache_base + ".png";

    if (!FileAccess::exists(path))
        return Ref<Texture>();

    Ref<Image> img(make_ref_counted<Image>());
    Error err = img->load(path);
    if (err == OK) {

        Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());

        post_process_preview(img);
        ptex->create_from_image(img, 0);
        return ptex;

    } else {
        return Ref<Texture>();
    }
}

EditorPackedScenePreviewPlugin::EditorPackedScenePreviewPlugin() {
}

//////////////////////////////////////////////////////////////////

void EditorMaterialPreviewPlugin::_preview_done(const Variant &p_udata) {

    preview_done = true;
}

void EditorMaterialPreviewPlugin::_bind_methods() {

    MethodBinder::bind_method("_preview_done", &EditorMaterialPreviewPlugin::_preview_done);
}

bool EditorMaterialPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "Material"); //any material
}

bool EditorMaterialPreviewPlugin::generate_small_preview_automatically() const {
    return true;
}

Ref<Texture> EditorMaterialPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<Material> material = dynamic_ref_cast<Material>(p_from);
    ERR_FAIL_COND_V(not material, Ref<Texture>())

    if (material->get_shader_mode() == ShaderMode::SPATIAL) {

        VisualServer::get_singleton()->mesh_surface_set_material(sphere, 0, material->get_rid());

        VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_ONCE); //once used for capture

        preview_done = false;
        VisualServer::get_singleton()->request_frame_drawn_callback(const_cast<EditorMaterialPreviewPlugin *>(this), "_preview_done", Variant());

        while (!preview_done) {
            OS::get_singleton()->delay_usec(10);
        }

        Ref<Image> img = VisualServer::get_singleton()->texture_get_data(viewport_texture);
        VisualServer::get_singleton()->mesh_surface_set_material(sphere, 0, RID());

        ERR_FAIL_COND_V(not img, Ref<ImageTexture>())

        img->convert(Image::FORMAT_RGBA8);
        int thumbnail_size = MAX(p_size.x, p_size.y);
        img->resize(thumbnail_size, thumbnail_size, Image::INTERPOLATE_CUBIC);
        post_process_preview(img);
        Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());
        ptex->create_from_image(img, 0);
        return ptex;
    }

    return Ref<Texture>();
}

EditorMaterialPreviewPlugin::EditorMaterialPreviewPlugin() {

    scenario = VisualServer::get_singleton()->scenario_create();

    viewport = VisualServer::get_singleton()->viewport_create();
    VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_DISABLED);
    VisualServer::get_singleton()->viewport_set_scenario(viewport, scenario);
    VisualServer::get_singleton()->viewport_set_size(viewport, 128, 128);
    VisualServer::get_singleton()->viewport_set_transparent_background(viewport, true);
    VisualServer::get_singleton()->viewport_set_active(viewport, true);
    VisualServer::get_singleton()->viewport_set_vflip(viewport, true);
    viewport_texture = VisualServer::get_singleton()->viewport_get_texture(viewport);

    camera = VisualServer::get_singleton()->camera_create();
    VisualServer::get_singleton()->viewport_attach_camera(viewport, camera);
    VisualServer::get_singleton()->camera_set_transform(camera, Transform(Basis(), Vector3(0, 0, 3)));
    VisualServer::get_singleton()->camera_set_perspective(camera, 45, 0.1f, 10);

    light = VisualServer::get_singleton()->directional_light_create();
    light_instance = VisualServer::get_singleton()->instance_create2(light, scenario);
    VisualServer::get_singleton()->instance_set_transform(light_instance, Transform().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));

    light2 = VisualServer::get_singleton()->directional_light_create();
    VisualServer::get_singleton()->light_set_color(light2, Color(0.7f, 0.7f, 0.7f));
    //VisualServer::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));

    light_instance2 = VisualServer::get_singleton()->instance_create2(light2, scenario);

    VisualServer::get_singleton()->instance_set_transform(light_instance2, Transform().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));

    sphere = VisualServer::get_singleton()->mesh_create();
    sphere_instance = VisualServer::get_singleton()->instance_create2(sphere, scenario);

    int lats = 32;
    int lons = 32;
    float radius = 1.0;

    PoolVector<Vector3> vertices;
    PoolVector<Vector3> normals;
    PoolVector<Vector2> uvs;
    PoolVector<float> tangents;
    Basis tt = Basis(Vector3(0, 1, 0), Math_PI * 0.5);

    for (int i = 1; i <= lats; i++) {
        double lat0 = Math_PI * (-0.5 + (double)(i - 1) / lats);
        double z0 = Math::sin(lat0);
        double zr0 = Math::cos(lat0);

        double lat1 = Math_PI * (-0.5 + (double)i / lats);
        double z1 = Math::sin(lat1);
        double zr1 = Math::cos(lat1);

        for (int j = lons; j >= 1; j--) {

            double lng0 = 2 * Math_PI * (double)(j - 1) / lons;
            double x0 = Math::cos(lng0);
            double y0 = Math::sin(lng0);

            double lng1 = 2 * Math_PI * (double)j / lons;
            double x1 = Math::cos(lng1);
            double y1 = Math::sin(lng1);

            Vector3 v[4] = {
                Vector3(x1 * zr0, z0, y1 * zr0),
                Vector3(x1 * zr1, z1, y1 * zr1),
                Vector3(x0 * zr1, z1, y0 * zr1),
                Vector3(x0 * zr0, z0, y0 * zr0)
            };

#define ADD_POINT(m_idx)                                                                       \
    normals.push_back(v[m_idx]);                                                               \
    vertices.push_back(v[m_idx] * radius);                                                     \
    {                                                                                          \
        Vector2 uv(Math::atan2(v[m_idx].x, v[m_idx].z), Math::atan2(-v[m_idx].y, v[m_idx].z)); \
        uv /= Math_PI;                                                                         \
        uv *= 4.0;                                                                             \
        uv = uv * 0.5 + Vector2(0.5, 0.5);                                                     \
        uvs.push_back(uv);                                                                     \
    }                                                                                          \
    {                                                                                          \
        Vector3 t = tt.xform(v[m_idx]);                                                        \
        tangents.push_back(t.x);                                                               \
        tangents.push_back(t.y);                                                               \
        tangents.push_back(t.z);                                                               \
        tangents.push_back(1.0);                                                               \
    }

            ADD_POINT(0);
            ADD_POINT(1);
            ADD_POINT(2);

            ADD_POINT(2);
            ADD_POINT(3);
            ADD_POINT(0);
        }
    }

    Array arr;
    arr.resize(VS::ARRAY_MAX);
    arr[VS::ARRAY_VERTEX] = vertices;
    arr[VS::ARRAY_NORMAL] = normals;
    arr[VS::ARRAY_TANGENT] = tangents;
    arr[VS::ARRAY_TEX_UV] = Variant(uvs);
    VisualServer::get_singleton()->mesh_add_surface_from_arrays(sphere, VS::PRIMITIVE_TRIANGLES, arr);
}

EditorMaterialPreviewPlugin::~EditorMaterialPreviewPlugin() {

    VisualServer::get_singleton()->free(sphere);
    VisualServer::get_singleton()->free(sphere_instance);
    VisualServer::get_singleton()->free(viewport);
    VisualServer::get_singleton()->free(light);
    VisualServer::get_singleton()->free(light_instance);
    VisualServer::get_singleton()->free(light2);
    VisualServer::get_singleton()->free(light_instance2);
    VisualServer::get_singleton()->free(camera);
    VisualServer::get_singleton()->free(scenario);
}

///////////////////////////////////////////////////////////////////////////

static bool _epp_is_text_char(CharType c) {

    return c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '_';
}

bool EditorScriptPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "Script");
}

Ref<Texture> EditorScriptPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<Script> scr = dynamic_ref_cast<Script>(p_from);
    if (not scr)
        return Ref<Texture>();

    se_string_view code = StringUtils::strip_edges(scr->get_source_code());
    if (code.empty())
        return Ref<Texture>();

    List<se_string> kwors;
    scr->get_language()->get_reserved_words(&kwors);

    Set<se_string> keywords;

    for (List<se_string>::Element *E = kwors.front(); E; E = E->next()) {

        keywords.insert(E->deref());
    }

    int line = 0;
    int col = 0;
    Ref<Image> img(make_ref_counted<Image>());
    int thumbnail_size = MAX(p_size.x, p_size.y);
    img->create(thumbnail_size, thumbnail_size, false, Image::FORMAT_RGBA8);

    Color bg_color = EditorSettings::get_singleton()->get("text_editor/highlighting/background_color");
    Color keyword_color = EditorSettings::get_singleton()->get("text_editor/highlighting/keyword_color");
    Color text_color = EditorSettings::get_singleton()->get("text_editor/highlighting/text_color");
    Color symbol_color = EditorSettings::get_singleton()->get("text_editor/highlighting/symbol_color");

    img->lock();

    if (bg_color.a == 0)
        bg_color = Color(0, 0, 0, 0);
    bg_color.a = MAX(bg_color.a, 0.2f); // some background

    for (int i = 0; i < thumbnail_size; i++) {
        for (int j = 0; j < thumbnail_size; j++) {
            img->set_pixel(i, j, bg_color);
        }
    }

    const int x0 = thumbnail_size / 8;
    const int y0 = thumbnail_size / 8;
    const int available_height = thumbnail_size - 2 * y0;
    col = x0;

    bool prev_is_text = false;
    bool in_keyword = false;
    for (size_t i = 0; i < code.length(); i++) {

        CharType c = code[i];
        if (c > 32) {
            if (col < thumbnail_size) {
                Color color = text_color;

                if (c != '_' && (c >= '!' && c <= '/' || c >= ':' && c <= '@' || c >= '[' && c <= '`' || c >= '{' && c <= '~' || c == '\t')) {
                    //make symbol a little visible
                    color = symbol_color;
                    in_keyword = false;
                } else if (!prev_is_text && _epp_is_text_char(c)) {
                    int pos = i;

                    while (_epp_is_text_char(code[pos])) {
                        pos++;
                    }
                    se_string_view word = StringUtils::substr(code,i, pos - i);
                    if (keywords.contains_as(word))
                        in_keyword = true;

                } else if (!_epp_is_text_char(c)) {
                    in_keyword = false;
                }

                if (in_keyword)
                    color = keyword_color;

                Color ul = color;
                ul.a *= 0.5f;
                img->set_pixel(col, y0 + line * 2, bg_color.blend(ul));
                img->set_pixel(col, y0 + line * 2 + 1, color);

                prev_is_text = _epp_is_text_char(c);
            }
        } else {

            prev_is_text = false;
            in_keyword = false;

            if (c == '\n') {
                col = x0;
                line++;
                if (line >= available_height / 2)
                    break;
            } else if (c == '\t') {
                col += 3;
            }
        }
        col++;
    }

    img->unlock();

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());

    ptex->create_from_image(img, 0);
    return ptex;
}

EditorScriptPreviewPlugin::EditorScriptPreviewPlugin() {
}
///////////////////////////////////////////////////////////////////

bool EditorAudioStreamPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "AudioStream");
}

Ref<Texture> EditorAudioStreamPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<AudioStream> stream = dynamic_ref_cast<AudioStream>(p_from);
    ERR_FAIL_COND_V(not stream, Ref<Texture>())

    PoolVector<uint8_t> img;

    int w = p_size.x;
    int h = p_size.y;
    img.resize(w * h * 3);

    PoolVector<uint8_t>::Write imgdata = img.write();
    uint8_t *imgw = imgdata.ptr();

    Ref<AudioStreamPlayback> playback = stream->instance_playback();
    ERR_FAIL_COND_V(not playback, Ref<Texture>())

    float len_s = stream->get_length();
    if (len_s == 0) {
        len_s = 60; //one minute audio if no length specified
    }
    int frame_length = AudioServer::get_singleton()->get_mix_rate() * len_s;

    Vector<AudioFrame> frames;
    frames.resize(frame_length);

    playback->start();
    playback->mix(frames.ptrw(), 1, frames.size());
    playback->stop();

    for (int i = 0; i < w; i++) {

        float max = -1000;
        float min = 1000;
        int from = uint64_t(i) * frame_length / w;
        int to = (uint64_t(i) + 1) * frame_length / w;
        to = MIN(to, frame_length);
        from = MIN(from, frame_length - 1);
        if (to == from) {
            to = from + 1;
        }

        for (int j = from; j < to; j++) {

            max = MAX(max, frames[j].l);
            max = MAX(max, frames[j].r);

            min = MIN(min, frames[j].l);
            min = MIN(min, frames[j].r);
        }

        int pfrom = CLAMP((min * 0.5 + 0.5) * h / 2, 0, h / 2) + h / 4;
        int pto = CLAMP((max * 0.5 + 0.5) * h / 2, 0, h / 2) + h / 4;

        for (int j = 0; j < h; j++) {
            uint8_t *p = &imgw[(j * w + i) * 3];
            if (j < pfrom || j > pto) {
                p[0] = 100;
                p[1] = 100;
                p[2] = 100;
            } else {
                p[0] = 180;
                p[1] = 180;
                p[2] = 180;
            }
        }
    }

    imgdata.release();
    //post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());
    Ref<Image> image(make_ref_counted<Image>());
    image->create(w, h, false, Image::FORMAT_RGB8, img);
    ptex->create_from_image(image, 0);
    return ptex;
}

EditorAudioStreamPreviewPlugin::EditorAudioStreamPreviewPlugin() {
}

///////////////////////////////////////////////////////////////////////////

void EditorMeshPreviewPlugin::_preview_done(const Variant &p_udata) {

    preview_done = true;
}

void EditorMeshPreviewPlugin::_bind_methods() {

    MethodBinder::bind_method("_preview_done", &EditorMeshPreviewPlugin::_preview_done);
}
bool EditorMeshPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "Mesh"); //any Mesh
}

Ref<Texture> EditorMeshPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    Ref<Mesh> mesh = dynamic_ref_cast<Mesh>(p_from);
    ERR_FAIL_COND_V(not mesh, Ref<Texture>())

    VisualServer::get_singleton()->instance_set_base(mesh_instance, mesh->get_rid());

    AABB aabb = mesh->get_aabb();
    Vector3 ofs = aabb.position + aabb.size * 0.5;
    aabb.position -= ofs;
    Transform xform;
    xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math_PI * 0.125);
    xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI * 0.125) * xform.basis;
    AABB rot_aabb = xform.xform(aabb);
    float m = MAX(rot_aabb.size.x, rot_aabb.size.y) * 0.5;
    if (m == 0)
        return Ref<Texture>();
    m = 1.0 / m;
    m *= 0.5;
    xform.basis.scale(Vector3(m, m, m));
    xform.origin = -xform.basis.xform(ofs); //-ofs*m;
    xform.origin.z -= rot_aabb.size.z * 2;
    VisualServer::get_singleton()->instance_set_transform(mesh_instance, xform);

    VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_ONCE); //once used for capture

    preview_done = false;
    VisualServer::get_singleton()->request_frame_drawn_callback(const_cast<EditorMeshPreviewPlugin *>(this), "_preview_done", Variant());

    while (!preview_done) {
        OS::get_singleton()->delay_usec(10);
    }

    Ref<Image> img = VisualServer::get_singleton()->texture_get_data(viewport_texture);
    ERR_FAIL_COND_V(not img, Ref<ImageTexture>())

    VisualServer::get_singleton()->instance_set_base(mesh_instance, RID());

    img->convert(Image::FORMAT_RGBA8);

    Vector2 new_size = img->get_size();
    if (new_size.x > p_size.x) {
        new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
    }
    if (new_size.y > p_size.y) {
        new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
    }
    img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());
    ptex->create_from_image(img, 0);
    return ptex;
}

EditorMeshPreviewPlugin::EditorMeshPreviewPlugin() {

    scenario = VisualServer::get_singleton()->scenario_create();

    viewport = VisualServer::get_singleton()->viewport_create();
    VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_DISABLED);
    VisualServer::get_singleton()->viewport_set_vflip(viewport, true);
    VisualServer::get_singleton()->viewport_set_scenario(viewport, scenario);
    VisualServer::get_singleton()->viewport_set_size(viewport, 128, 128);
    VisualServer::get_singleton()->viewport_set_transparent_background(viewport, true);
    VisualServer::get_singleton()->viewport_set_active(viewport, true);
    viewport_texture = VisualServer::get_singleton()->viewport_get_texture(viewport);

    camera = VisualServer::get_singleton()->camera_create();
    VisualServer::get_singleton()->viewport_attach_camera(viewport, camera);
    VisualServer::get_singleton()->camera_set_transform(camera, Transform(Basis(), Vector3(0, 0, 3)));
    //VisualServer::get_singleton()->camera_set_perspective(camera,45,0.1,10);
    VisualServer::get_singleton()->camera_set_orthogonal(camera, 1.0, 0.01, 1000.0);

    light = VisualServer::get_singleton()->directional_light_create();
    light_instance = VisualServer::get_singleton()->instance_create2(light, scenario);
    VisualServer::get_singleton()->instance_set_transform(light_instance, Transform().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));

    light2 = VisualServer::get_singleton()->directional_light_create();
    VisualServer::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));
    //VisualServer::get_singleton()->light_set_color(light2, VS::LIGHT_COLOR_SPECULAR, Color(0.0, 0.0, 0.0));
    light_instance2 = VisualServer::get_singleton()->instance_create2(light2, scenario);

    VisualServer::get_singleton()->instance_set_transform(light_instance2, Transform().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));

    //sphere = VisualServer::get_singleton()->mesh_create();
    mesh_instance = VisualServer::get_singleton()->instance_create();
    VisualServer::get_singleton()->instance_set_scenario(mesh_instance, scenario);
}

EditorMeshPreviewPlugin::~EditorMeshPreviewPlugin() {

    //VisualServer::get_singleton()->free(sphere);
    VisualServer::get_singleton()->free(mesh_instance);
    VisualServer::get_singleton()->free(viewport);
    VisualServer::get_singleton()->free(light);
    VisualServer::get_singleton()->free(light_instance);
    VisualServer::get_singleton()->free(light2);
    VisualServer::get_singleton()->free(light_instance2);
    VisualServer::get_singleton()->free(camera);
    VisualServer::get_singleton()->free(scenario);
}

///////////////////////////////////////////////////////////////////////////

void EditorFontPreviewPlugin::_preview_done(const Variant &p_udata) {

    preview_done = true;
}

void EditorFontPreviewPlugin::_bind_methods() {

    MethodBinder::bind_method("_preview_done", &EditorFontPreviewPlugin::_preview_done);
}

bool EditorFontPreviewPlugin::handles(se_string_view p_type) const {

    return ClassDB::is_parent_class(StringName(p_type), "DynamicFontData") || ClassDB::is_parent_class(StringName(p_type), "DynamicFont");

}

Ref<Texture> EditorFontPreviewPlugin::generate_from_path(se_string_view p_path, const Size2 &p_size) const {

    RES res = ResourceLoader::load(p_path);
    Ref<DynamicFont> sampled_font;
    if (res->is_class("DynamicFont")) {
        sampled_font = dynamic_ref_cast<DynamicFont>(res->duplicate());
        if (sampled_font->get_outline_color() == Color(1, 1, 1, 1)) {
            sampled_font->set_outline_color(Color(0, 0, 0, 1));
        }
    } else if (res->is_class("DynamicFontData")) {
        sampled_font = make_ref_counted<DynamicFont>();
        sampled_font->set_font_data(dynamic_ref_cast<DynamicFontData>(res));
    }
    sampled_font->set_size(50);

    String sampled_text = "Abg";

    Vector2 size = sampled_font->get_string_size(sampled_text);

    Vector2 pos;

    pos.x = 64 - size.x / 2;
    pos.y = 80;

    Ref<Font> font = sampled_font;

    font->draw(canvas_item, pos, sampled_text);

    preview_done = false;
    VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_ONCE); //once used for capture
    VisualServer::get_singleton()->request_frame_drawn_callback(const_cast<EditorFontPreviewPlugin *>(this), "_preview_done", Variant());

    while (!preview_done) {
        OS::get_singleton()->delay_usec(10);
    }

    VisualServer::get_singleton()->canvas_item_clear(canvas_item);

    Ref<Image> img = VisualServer::get_singleton()->texture_get_data(viewport_texture);
    ERR_FAIL_COND_V(not img, Ref<ImageTexture>())

    img->convert(Image::FORMAT_RGBA8);

    Vector2 new_size = img->get_size();
    if (new_size.x > p_size.x) {
        new_size = Vector2(p_size.x, new_size.y * p_size.x / new_size.x);
    }
    if (new_size.y > p_size.y) {
        new_size = Vector2(new_size.x * p_size.y / new_size.y, p_size.y);
    }
    img->resize(new_size.x, new_size.y, Image::INTERPOLATE_CUBIC);

    post_process_preview(img);

    Ref<ImageTexture> ptex(make_ref_counted<ImageTexture>());
    ptex->create_from_image(img, 0);

    return ptex;
}

Ref<Texture> EditorFontPreviewPlugin::generate(const RES &p_from, const Size2 &p_size) const {

    se_string path = p_from->get_path();
    if (!FileAccess::exists(path)) {
        return Ref<Texture>();
    }
    return generate_from_path(path, p_size);
}

EditorFontPreviewPlugin::EditorFontPreviewPlugin() {

    viewport = VisualServer::get_singleton()->viewport_create();
    VisualServer::get_singleton()->viewport_set_update_mode(viewport, VS::VIEWPORT_UPDATE_DISABLED);
    VisualServer::get_singleton()->viewport_set_vflip(viewport, true);
    VisualServer::get_singleton()->viewport_set_size(viewport, 128, 128);
    VisualServer::get_singleton()->viewport_set_active(viewport, true);
    viewport_texture = VisualServer::get_singleton()->viewport_get_texture(viewport);

    canvas = VisualServer::get_singleton()->canvas_create();
    canvas_item = VisualServer::get_singleton()->canvas_item_create();

    VisualServer::get_singleton()->viewport_attach_canvas(viewport, canvas);
    VisualServer::get_singleton()->canvas_item_set_parent(canvas_item, canvas);
}

EditorFontPreviewPlugin::~EditorFontPreviewPlugin() {

    VisualServer::get_singleton()->free(canvas_item);
    VisualServer::get_singleton()->free(canvas);
    VisualServer::get_singleton()->free(viewport);
}
