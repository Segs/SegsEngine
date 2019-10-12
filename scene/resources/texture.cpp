/*************************************************************************/
/*  texture.cpp                                                          */
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

#include "texture.h"
#include "texture_serializers.h"

#include "textures_enum_casters.h"
#include "curve_texture.h"
#include "mesh.h"

#include "core/core_string_names.h"
#include "core/image_enum_casters.h"
#include "core/io/image_loader.h"
#include "core/io/image_saver.h"
#include "core/io/resource_saver.h"
#include "core/method_bind.h"
#include "core/os/os.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/mesh.h"
#include "servers/camera/camera_feed.h"
#include "servers/visual_server.h"

IMPL_GDCLASS(Texture)
IMPL_GDCLASS(ImageTexture)
IMPL_GDCLASS(StreamTexture)
IMPL_GDCLASS(AtlasTexture)
IMPL_GDCLASS(MeshTexture)
IMPL_GDCLASS(LargeTexture)
IMPL_GDCLASS(CubeMap)
IMPL_GDCLASS(TextureLayered)
IMPL_GDCLASS(Texture3D)
IMPL_GDCLASS(TextureArray)
IMPL_GDCLASS(CurveTexture)
IMPL_GDCLASS(GradientTexture)
IMPL_GDCLASS(ProxyTexture)
IMPL_GDCLASS(AnimatedTexture)

RES_BASE_EXTENSION_IMPL(ImageTexture,"tex")
RES_BASE_EXTENSION_IMPL(AtlasTexture,"atlastex")
RES_BASE_EXTENSION_IMPL(MeshTexture,"meshtex")
RES_BASE_EXTENSION_IMPL(LargeTexture,"largetex")
RES_BASE_EXTENSION_IMPL(CubeMap,"cubemap")
RES_BASE_EXTENSION_IMPL(CurveTexture,"curvetex")
namespace  {
    class ResourceSaverImage final : public ResourceFormatSaver {
        ImageFormatSaver *m_saver;
    public:
        Error save(const String &p_path, const RES &p_resource, uint32_t p_flags = 0) final {

            Error err;
            Ref<ImageTexture> texture(dynamic_ref_cast<ImageTexture>(p_resource));

            ERR_FAIL_COND_V_CMSG(not texture, ERR_INVALID_PARAMETER, "Can't save invalid texture as PNG.")
            ERR_FAIL_COND_V_CMSG(!texture->get_width(), ERR_INVALID_PARAMETER, "Can't save empty texture as PNG.")

            Ref<Image> img(texture->get_data());
            FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
            ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save using saver wrapper at path: '%s'.", p_path))
            PODVector<uint8_t> buffer;
            err = m_saver->save_image(*img,buffer,{});

            file->store_buffer(buffer.data(), buffer.size());
            if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
                memdelete(file);
                return ERR_CANT_CREATE;
            }

            file->close();
            memdelete(file);


            return err;
        }
        bool recognize(const RES &p_resource) const final {

            return dynamic_ref_cast<ImageTexture>(p_resource)!=nullptr;
        }
        void get_recognized_extensions(const RES &p_resource, Vector<String> *p_extensions) const final {
            if (object_cast<ImageTexture>(p_resource.get()))
                return m_saver->get_saved_extensions(p_extensions);
        }

        ResourceSaverImage(ImageFormatSaver *saver) : m_saver(saver) {}
    };
    void register_image_resource_savers() {
        //TODO: SEGS: the code to register image resource savers is brittle, i.e. it assumes all plugins were available before Texture::_bind_methods()
        auto &all_savers(ImageSaver::get_image_format_savers());
        for (ImageFormatSaver *svr : all_savers)
        {
            if (svr->can_save("png"))
                ResourceSaver::add_resource_format_saver(Ref<ResourceSaverImage>(make_ref_counted<ResourceSaverImage>(svr)));

        }
    }
}


Size2 Texture::get_size() const {

    return Size2(get_width(), get_height());
}

bool Texture::is_pixel_opaque(int p_x, int p_y) const {
    return true;
}
void Texture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, get_size()), get_rid(), false, p_modulate, p_transpose, normal_rid);
}
void Texture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, get_rid(), p_tile, p_modulate, p_transpose, normal_rid);
}
void Texture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, get_rid(), p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
}

bool Texture::get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const {

    r_rect = p_rect;
    r_src_rect = p_src_rect;

    return true;
}

void Texture::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("get_width"), &Texture::get_width);
    MethodBinder::bind_method(D_METHOD("get_height"), &Texture::get_height);
    MethodBinder::bind_method(D_METHOD("get_size"), &Texture::get_size);
    MethodBinder::bind_method(D_METHOD("has_alpha"), &Texture::has_alpha);
    MethodBinder::bind_method(D_METHOD("set_flags", {"flags"}), &Texture::set_flags);
    MethodBinder::bind_method(D_METHOD("get_flags"), &Texture::get_flags);
    MethodBinder::bind_method(D_METHOD("draw", {"canvas_item", "position", "modulate", "transpose", "normal_map"}), &Texture::draw, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("draw_rect", {"canvas_item", "rect", "tile", "modulate", "transpose", "normal_map"}), &Texture::draw_rect, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("draw_rect_region", {"canvas_item", "rect", "src_rect", "modulate", "transpose", "normal_map", "clip_uv"}), &Texture::draw_rect_region, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant()), DEFVAL(true)});
    MethodBinder::bind_method(D_METHOD("get_data"), &Texture::get_data);

    ADD_GROUP("Flags", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flags", PROPERTY_HINT_FLAGS, "Mipmaps,Repeat,Filter,Anisotropic Linear,Convert to Linear,Mirrored Repeat,Video Surface"), "set_flags", "get_flags");
    ADD_GROUP("", "");

    BIND_ENUM_CONSTANT(FLAGS_DEFAULT)
    BIND_ENUM_CONSTANT(FLAG_MIPMAPS)
    BIND_ENUM_CONSTANT(FLAG_REPEAT)
    BIND_ENUM_CONSTANT(FLAG_FILTER)
    BIND_ENUM_CONSTANT(FLAG_ANISOTROPIC_FILTER)
    BIND_ENUM_CONSTANT(FLAG_CONVERT_TO_LINEAR)
    BIND_ENUM_CONSTANT(FLAG_MIRRORED_REPEAT)
    BIND_ENUM_CONSTANT(FLAG_VIDEO_SURFACE)
}

Texture::Texture() {
}

/////////////////////

void ImageTexture::reload_from_file() {

    String path = ResourceLoader::path_remap(get_path());
    if (!PathUtils::is_resource_file(path))
        return;

    uint32_t flags = get_flags();
    Ref<Image> img(make_ref_counted<Image>());

    if (ImageLoader::load_image(path, img) == OK) {
        create_from_image(img, flags);
    } else {
        Resource::reload_from_file();
        _change_notify();
        emit_changed();
    }
}

bool ImageTexture::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "image")
        create_from_image(refFromRefPtr<Image>(p_value), flags);
    else if (p_name == "flags")
        if (w * h == 0)
            flags = p_value;
        else
            set_flags(p_value);
    else if (p_name == "size") {
        Size2 s = p_value;
        w = s.width;
        h = s.height;
        VisualServer::get_singleton()->texture_set_size_override(texture, w, h, 0);
    } else if (p_name == "_data") {
        _set_data(p_value);
    } else
        return false;

    return true;
}

bool ImageTexture::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "image_data") {

    } else if (p_name == "image")
        r_ret = get_data();
    else if (p_name == "flags")
        r_ret = flags;
    else if (p_name == "size")
        r_ret = Size2(w, h);
    else
        return false;

    return true;
}

void ImageTexture::_get_property_list(ListPOD<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::INT, "flags", PROPERTY_HINT_FLAGS, "Mipmaps,Repeat,Filter,Anisotropic,sRGB,Mirrored Repeat"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "image", PROPERTY_HINT_RESOURCE_TYPE, "Image", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT));
    p_list->push_back(PropertyInfo(VariantType::VECTOR2, "size", PROPERTY_HINT_NONE, ""));
}

void ImageTexture::_reload_hook(const RID &p_hook) {

    String path = get_path();
    if (!PathUtils::is_resource_file(path))
        return;

    Ref<Image> img(make_ref_counted<Image>());
    Error err = ImageLoader::load_image(path, img);

    ERR_FAIL_COND(err != OK)

    VisualServer::get_singleton()->texture_set_data(texture, img);

    _change_notify();
    emit_changed();
}

void ImageTexture::create(int p_width, int p_height, Image::Format p_format, uint32_t p_flags) {

    flags = p_flags;
    VisualServer::get_singleton()->texture_allocate(texture, p_width, p_height, 0, p_format, VS::TEXTURE_TYPE_2D, p_flags);
    format = p_format;
    w = p_width;
    h = p_height;
    _change_notify();
    emit_changed();
}
void ImageTexture::create_from_image(const Ref<Image> &p_image, uint32_t p_flags) {

    ERR_FAIL_COND(not p_image)
    flags = p_flags;
    w = p_image->get_width();
    h = p_image->get_height();
    format = p_image->get_format();

    VisualServer::get_singleton()->texture_allocate(texture, p_image->get_width(), p_image->get_height(), 0, p_image->get_format(), VS::TEXTURE_TYPE_2D, p_flags);
    VisualServer::get_singleton()->texture_set_data(texture, p_image);
    _change_notify();
    emit_changed();

    image_stored = true;
}

void ImageTexture::set_flags(uint32_t p_flags) {

    if (flags == p_flags)
        return;

    flags = p_flags;
    if (w == 0 || h == 0) {
        return; //uninitialized, do not set to texture
    }
    VisualServer::get_singleton()->texture_set_flags(texture, p_flags);
    _change_notify("flags");
    emit_changed();
}

uint32_t ImageTexture::get_flags() const {

    return ImageTexture::flags;
}

Image::Format ImageTexture::get_format() const {

    return format;
}
#ifndef DISABLE_DEPRECATED
Error ImageTexture::load(const String &p_path) {

    WARN_DEPRECATED
    Ref<Image> img(make_ref_counted<Image>());
    Error err = img->load(p_path);
    if (err == OK) {
        create_from_image(img);
    }
    return err;
}
#endif
void ImageTexture::set_data(const Ref<Image> &p_image) {

    ERR_FAIL_COND(not p_image)

    VisualServer::get_singleton()->texture_set_data(texture, p_image);

    _change_notify();
    emit_changed();

    alpha_cache.reset(); //TODO: memory de-allocation
    image_stored = true;
}

void ImageTexture::_resource_path_changed() {

    String path = get_path();
}

Ref<Image> ImageTexture::get_data() const {

    if (image_stored) {
        return VisualServer::get_singleton()->texture_get_data(texture);
    } else {
        return Ref<Image>();
    }
}

int ImageTexture::get_width() const {

    return w;
}

int ImageTexture::get_height() const {

    return h;
}

RID ImageTexture::get_rid() const {

    return texture;
}

bool ImageTexture::has_alpha() const {

    return (format == Image::FORMAT_LA8 || format == Image::FORMAT_RGBA8);
}

void ImageTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((w | h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, Size2(w, h)), texture, false, p_modulate, p_transpose, normal_rid);
}
void ImageTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((w | h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, texture, p_tile, p_modulate, p_transpose, normal_rid);
}
void ImageTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    if ((w | h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, texture, p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
}

bool ImageTexture::is_pixel_opaque(int p_x, int p_y) const {

    if (nullptr==alpha_cache) {
        Ref<Image> img = get_data();
        if (img) {
            if (img->is_compressed()) { //must decompress, if compressed
                Ref<Image> decom = dynamic_ref_cast<Image>(img->duplicate());
                decom->decompress();
                img = decom;
            }
            alpha_cache = eastl::make_unique<BitMap>();
            alpha_cache->create_from_image_alpha(img);
        }
    }

    if (nullptr!=alpha_cache) {

        int aw = int(alpha_cache->get_size().width);
        int ah = int(alpha_cache->get_size().height);
        if (aw == 0 || ah == 0) {
            return true;
        }

        int x = p_x * aw / w;
        int y = p_y * ah / h;

        x = CLAMP(x, 0, aw);
        y = CLAMP(y, 0, ah);

        return alpha_cache->get_bit(Point2(x, y));
    }

    return true;
}

void ImageTexture::set_size_override(const Size2 &p_size) {

    Size2 s = p_size;
    if (s.x != 0.0f)
        w = s.x;
    if (s.y != 0.0f)
        h = s.y;
    VisualServer::get_singleton()->texture_set_size_override(texture, w, h, 0);
}

void ImageTexture::set_path(const String &p_path, bool p_take_over) {

    if (texture.is_valid()) {
        VisualServer::get_singleton()->texture_set_path(texture, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

void ImageTexture::set_storage(Storage p_storage) {

    storage = p_storage;
}

ImageTexture::Storage ImageTexture::get_storage() const {

    return storage;
}

void ImageTexture::set_lossy_storage_quality(float p_lossy_storage_quality) {

    lossy_storage_quality = p_lossy_storage_quality;
}

float ImageTexture::get_lossy_storage_quality() const {

    return lossy_storage_quality;
}

void ImageTexture::_set_data(Dictionary p_data) {

    Ref<Image> img(p_data["image"]);
    ERR_FAIL_COND(not img)
    uint32_t flags = p_data["flags"];

    create_from_image(img, flags);

    set_storage(Storage(p_data["storage"].operator int()));
    set_lossy_storage_quality(p_data["lossy_quality"]);

    set_size_override(p_data["size"]);
};

void ImageTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("create", {"width", "height", "format", "flags"}), &ImageTexture::create, {DEFVAL(FLAGS_DEFAULT)});
    MethodBinder::bind_method(D_METHOD("create_from_image", {"image", "flags"}), &ImageTexture::create_from_image, {DEFVAL(FLAGS_DEFAULT)});
    MethodBinder::bind_method(D_METHOD("get_format"), &ImageTexture::get_format);
#ifndef DISABLE_DEPRECATED
    MethodBinder::bind_method(D_METHOD("load", {"path"}), &ImageTexture::load);
#endif
    MethodBinder::bind_method(D_METHOD("set_data", {"image"}), &ImageTexture::set_data);
    MethodBinder::bind_method(D_METHOD("set_storage", {"mode"}), &ImageTexture::set_storage);
    MethodBinder::bind_method(D_METHOD("get_storage"), &ImageTexture::get_storage);
    MethodBinder::bind_method(D_METHOD("set_lossy_storage_quality", {"quality"}), &ImageTexture::set_lossy_storage_quality);
    MethodBinder::bind_method(D_METHOD("get_lossy_storage_quality"), &ImageTexture::get_lossy_storage_quality);

    MethodBinder::bind_method(D_METHOD("set_size_override", {"size"}), &ImageTexture::set_size_override);
    MethodBinder::bind_method(D_METHOD("_reload_hook", {"rid"}), &ImageTexture::_reload_hook);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "storage", PROPERTY_HINT_ENUM, "Uncompressed,Compress Lossy,Compress Lossless"), "set_storage", "get_storage");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "lossy_quality", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"), "set_lossy_storage_quality", "get_lossy_storage_quality");

    BIND_ENUM_CONSTANT(STORAGE_RAW)
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSY)
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSLESS)
}

ImageTexture::ImageTexture() {

    w = h = 0;
    flags = FLAGS_DEFAULT;
    texture = VisualServer::get_singleton()->texture_create();
    storage = STORAGE_RAW;
    lossy_storage_quality = 0.7f;
    image_stored = false;
    format = Image::FORMAT_L8;
}

ImageTexture::~ImageTexture() {

    VisualServer::get_singleton()->free(texture);
}

//////////////////////////////////////////
struct StreamTexture::StreamTextureData {
    String path_to_file;
    RID texture;
    Image::Format format;
    uint32_t flags;
    int w, h;
    mutable eastl::unique_ptr<BitMap> alpha_cache;
};

void StreamTexture::set_path(const String &p_path, bool p_take_over) {

    if (m_impl_data->texture.is_valid()) {
        VisualServer::get_singleton()->texture_set_path(m_impl_data->texture, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

void StreamTexture::_requested_3d(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_3d_callback)
    request_3d_callback(stex->get_path());
}

void StreamTexture::_requested_srgb(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_srgb_callback)
    request_srgb_callback(stex->get_path());
}

void StreamTexture::_requested_normal(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_normal_callback)
    request_normal_callback(stex->get_path());
}

StreamTexture::TextureFormatRequestCallback StreamTexture::request_3d_callback = nullptr;
StreamTexture::TextureFormatRequestCallback StreamTexture::request_srgb_callback = nullptr;
StreamTexture::TextureFormatRequestCallback StreamTexture::request_normal_callback = nullptr;

uint32_t StreamTexture::get_flags() const {

    return m_impl_data->flags;
}
Image::Format StreamTexture::get_format() const {

    return m_impl_data->format;
}

Error StreamTexture::_load_data(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image, int p_size_limit) {

    m_impl_data->alpha_cache.reset(nullptr); // TODO: memory de-allocation, check if actually needed ?

    ERR_FAIL_COND_V(not image, ERR_INVALID_PARAMETER)

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V(!f, ERR_CANT_OPEN)

    uint8_t header[4];
    f->get_buffer(header, 4);
    if (header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T') {
        memdelete(f);
        ERR_FAIL_COND_V(header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T', ERR_FILE_CORRUPT)
    }

    tw = f->get_16();
    tw_custom = f->get_16();
    th = f->get_16();
    th_custom = f->get_16();

    flags = f->get_32(); //texture flags!
    uint32_t df = f->get_32(); //data format

    /*
    print_line("width: " + itos(tw));
    print_line("height: " + itos(th));
    print_line("flags: " + itos(flags));
    print_line("df: " + itos(df));
    */
#ifdef TOOLS_ENABLED
    RID texture = m_impl_data->texture;
    if (request_3d_callback && df & FORMAT_BIT_DETECT_3D) {
        //print_line("request detect 3D at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_3d_callback(texture, _requested_3d, this);
    } else {
        //print_line("not requesting detect 3D at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_3d_callback(texture, nullptr, nullptr);
    }

    if (request_srgb_callback && df & FORMAT_BIT_DETECT_SRGB) {
        //print_line("request detect srgb at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_srgb_callback(texture, _requested_srgb, this);
    } else {
        //print_line("not requesting detect srgb at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_srgb_callback(texture, nullptr, nullptr);
    }

    if (request_srgb_callback && df & FORMAT_BIT_DETECT_NORMAL) {
        //print_line("request detect srgb at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_normal_callback(texture, _requested_normal, this);
    } else {
        //print_line("not requesting detect normal at " + p_path);
        VisualServer::get_singleton()->texture_set_detect_normal_callback(texture, nullptr, nullptr);
    }
#endif
    if (!(df & FORMAT_BIT_STREAM)) {
        p_size_limit = 0;
    }

    if (df & FORMAT_BIT_LOSSLESS || df & FORMAT_BIT_LOSSY) {
        //look for a PNG or WEBP file inside

        int sw = tw;
        int sh = th;

        uint32_t mipmaps = f->get_32();
        uint32_t size = f->get_32();

        //print_line("mipmaps: " + itos(mipmaps));

        while (mipmaps > 1 && p_size_limit > 0 && (sw > p_size_limit || sh > p_size_limit)) {

            f->seek(f->get_position() + size);
            mipmaps = f->get_32();
            size = f->get_32();

            sw = MAX(sw >> 1, 1);
            sh = MAX(sh >> 1, 1);
            mipmaps--;
        }

        //mipmaps need to be read independently, they will be later combined
        Vector<Ref<Image> > mipmap_images;
        int total_size = 0;
        PODVector<uint8_t> pv;

        for (uint32_t i = 0; i < mipmaps; i++) {

            if (i) {
                size = f->get_32();
            }

            pv.resize(size);
            f->get_buffer(pv.data(), size);
            Ref<Image> img;
            if (df & FORMAT_BIT_LOSSLESS) {
                img = Image::lossless_unpacker(pv);
            } else {
                img = Image::lossy_unpacker(pv);
            }

            if (not img || img->empty()) {
                memdelete(f);
                ERR_FAIL_COND_V(not img || img->empty(), ERR_FILE_CORRUPT)
            }

            total_size += img->get_data().size();

            mipmap_images.push_back(img);
        }

        //print_line("mipmap read total: " + itos(mipmap_images.size()));

        memdelete(f); //no longer needed

        if (mipmap_images.size() == 1) {

            image = mipmap_images[0];
            return OK;

        } else {
            PoolVector<uint8_t> img_data;
            img_data.resize(total_size);

            {
                PoolVector<uint8_t>::Write w = img_data.write();

                int ofs = 0;
                for (int i = 0; i < mipmap_images.size(); i++) {

                    PoolVector<uint8_t> id = mipmap_images[i]->get_data();
                    int len = id.size();
                    PoolVector<uint8_t>::Read r = id.read();
                    memcpy(&w[ofs], r.ptr(), len);
                    ofs += len;
                }
            }

            image->create(sw, sh, true, mipmap_images[0]->get_format(), img_data);
            return OK;
        }

    } else {

        //look for regular format
        Image::Format format = (Image::Format)(df & FORMAT_MASK_IMAGE_FORMAT);
        bool mipmaps = df & FORMAT_BIT_HAS_MIPMAPS;

        if (!mipmaps) {
            int size = Image::get_image_data_size(tw, th, format, false);

            PoolVector<uint8_t> img_data;
            img_data.resize(size);

            {
                PoolVector<uint8_t>::Write w = img_data.write();
                f->get_buffer(w.ptr(), size);
            }

            memdelete(f);

            image->create(tw, th, false, format, img_data);
            return OK;
        } else {

            int sw = tw;
            int sh = th;

            int mipmaps2 = Image::get_image_required_mipmaps(tw, th, format);
            int total_size = Image::get_image_data_size(tw, th, format, true);
            int idx = 0;

            while (mipmaps2 > 1 && p_size_limit > 0 && (sw > p_size_limit || sh > p_size_limit)) {

                sw = MAX(sw >> 1, 1);
                sh = MAX(sh >> 1, 1);
                mipmaps2--;
                idx++;
            }

            int ofs = Image::get_image_mipmap_offset(tw, th, format, idx);

            if (total_size - ofs <= 0) {
                memdelete(f);
                ERR_FAIL_V(ERR_FILE_CORRUPT)
            }

            f->seek(f->get_position() + ofs);

            PoolVector<uint8_t> img_data;
            img_data.resize(total_size - ofs);

            {
                PoolVector<uint8_t>::Write w = img_data.write();
                int bytes = f->get_buffer(w.ptr(), total_size - ofs);
                //print_line("requested read: " + itos(total_size - ofs) + " but got: " + itos(bytes));

                memdelete(f);

                int expected = total_size - ofs;
                if (bytes < expected) {
                    //this is a compatibility workaround for older format, which saved less mipmaps2. It is still recommended the image is reimported.
                    memset(w.ptr() + bytes, 0, (expected - bytes));
                } else if (bytes != expected) {
                    ERR_FAIL_V(ERR_FILE_CORRUPT)
                }
            }

            image->create(sw, sh, true, format, img_data);

            return OK;
        }
    }

    return ERR_BUG; //unreachable
}

Error StreamTexture::load(const String &p_path) {

    int lw, lh, lwc, lhc, lflags;
    Ref<Image> image(make_ref_counted<Image>());
    Error err = _load_data(p_path, lw, lh, lwc, lhc, lflags, image);
    if (err)
        return err;
    RID texture = m_impl_data->texture;

    if (get_path().empty()) {
        //temporarily set path if no path set for resource, helps find errors
        VisualServer::get_singleton()->texture_set_path(texture, p_path);
    }
    VisualServer::get_singleton()->texture_allocate(texture, image->get_width(), image->get_height(), 0, image->get_format(), VS::TEXTURE_TYPE_2D, lflags);
    VisualServer::get_singleton()->texture_set_data(texture, image);
    if (lwc || lhc) {
        VisualServer::get_singleton()->texture_set_size_override(texture, lwc, lhc, 0);
    } else {
    }

    m_impl_data->w = lwc ? lwc : lw;
    m_impl_data->h = lhc ? lhc : lh;
    m_impl_data->flags = lflags;
    m_impl_data->path_to_file = p_path;
    m_impl_data->format = image->get_format();

    _change_notify();
    emit_changed();
    return OK;
}
String StreamTexture::get_load_path() const {

    return m_impl_data->path_to_file;
}

int StreamTexture::get_width() const {

    return m_impl_data->w;
}
int StreamTexture::get_height() const {

    return m_impl_data->h;
}
RID StreamTexture::get_rid() const {

    return m_impl_data->texture;
}

void StreamTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, Size2(m_impl_data->w, m_impl_data->h)), m_impl_data->texture, false, p_modulate, p_transpose, normal_rid);
}
void StreamTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, m_impl_data->texture, p_tile, p_modulate, p_transpose, normal_rid);
}
void StreamTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, m_impl_data->texture, p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
}

bool StreamTexture::has_alpha() const {

    return false;
}

Ref<Image> StreamTexture::get_data() const {

    return VisualServer::get_singleton()->texture_get_data(m_impl_data->texture);
}

bool StreamTexture::is_pixel_opaque(int p_x, int p_y) const {

    if (nullptr== m_impl_data->alpha_cache) {
        Ref<Image> img = get_data();
        if (img) {
            if (img->is_compressed()) { //must decompress, if compressed
                Ref<Image> decom = dynamic_ref_cast<Image>(img->duplicate());
                decom->decompress();
                img = decom;
            }

            m_impl_data->alpha_cache = eastl::make_unique<BitMap>();
            m_impl_data->alpha_cache->create_from_image_alpha(img);
        }
    }

    if (nullptr!= m_impl_data->alpha_cache) {

        int aw = int(m_impl_data->alpha_cache->get_size().width);
        int ah = int(m_impl_data->alpha_cache->get_size().height);
        if (aw == 0 || ah == 0) {
            return true;
        }

        int x = p_x * aw / m_impl_data->w;
        int y = p_y * ah / m_impl_data->h;

        x = CLAMP(x, 0, aw);
        y = CLAMP(y, 0, ah);

        return m_impl_data->alpha_cache->get_bit(Point2(x, y));
    }

    return true;
}
void StreamTexture::set_flags(uint32_t p_flags) {
    m_impl_data->flags = p_flags;
    VisualServer::get_singleton()->texture_set_flags(m_impl_data->texture, m_impl_data->flags);
    _change_notify("flags");
    emit_changed();
}

void StreamTexture::reload_from_file() {

    String path = get_path();
    if (!PathUtils::is_resource_file(path))
        return;

    path = ResourceLoader::path_remap(path); //remap for translation
    path = ResourceLoader::import_remap(path); //remap for import
    if (!PathUtils::is_resource_file(path))
        return;

    load(path);
}

void StreamTexture::_validate_property(PropertyInfo &property) const {
    if (property.name == "flags") {
        property.usage = PROPERTY_USAGE_NOEDITOR;
    }
}

void StreamTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("load", {"path"}), &StreamTexture::load);
    MethodBinder::bind_method(D_METHOD("get_load_path"), &StreamTexture::get_load_path);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "load_path", PROPERTY_HINT_FILE, "*.stex"), "load", "get_load_path");
}

StreamTexture::StreamTexture() {
    m_impl_data = new StreamTextureData;
    m_impl_data->format = Image::FORMAT_MAX;
    m_impl_data->flags = 0;
    m_impl_data->w = 0;
    m_impl_data->h = 0;

    m_impl_data->texture = VisualServer::get_singleton()->texture_create();
}

StreamTexture::~StreamTexture() {
    VisualServer::get_singleton()->free(m_impl_data->texture);
    delete m_impl_data;
}

RES ResourceFormatLoaderStreamTexture::load(const String &p_path, const String &p_original_path, Error *r_error) {

    Ref<StreamTexture> st(make_ref_counted<StreamTexture>());
    Error err = st->load(p_path);
    if (r_error)
        *r_error = err;
    if (err != OK)
        return RES();

    return st;
}

void ResourceFormatLoaderStreamTexture::get_recognized_extensions(ListPOD<String> *p_extensions) const {

    p_extensions->push_back("stex");
}
bool ResourceFormatLoaderStreamTexture::handles_type(const String &p_type) const {
    return p_type == "StreamTexture";
}
String ResourceFormatLoaderStreamTexture::get_resource_type(const String &p_path) const {

    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "stex")
        return "StreamTexture";
    return "";
}

//////////////////////////////////////////

int AtlasTexture::get_width() const {

    if (region.size.width == 0) {
        if (atlas)
            return atlas->get_width();
        return 1;
    } else {
        return region.size.width + margin.size.width;
    }
}
int AtlasTexture::get_height() const {

    if (region.size.height == 0) {
        if (atlas)
            return atlas->get_height();
        return 1;
    } else {
        return region.size.height + margin.size.height;
    }
}
RID AtlasTexture::get_rid() const {

    if (atlas)
        return atlas->get_rid();

    return RID();
}

bool AtlasTexture::has_alpha() const {

    if (atlas)
        return atlas->has_alpha();

    return false;
}

void AtlasTexture::set_flags(uint32_t p_flags) {

    if (atlas)
        atlas->set_flags(p_flags);
}

uint32_t AtlasTexture::get_flags() const {

    if (atlas)
        return atlas->get_flags();

    return 0;
}

void AtlasTexture::set_atlas(const Ref<Texture> &p_atlas) {

    ERR_FAIL_COND(this == p_atlas.get())
    if (atlas == p_atlas)
        return;
    atlas = p_atlas;
    emit_changed();
    _change_notify("atlas");
}
Ref<Texture> AtlasTexture::get_atlas() const {

    return atlas;
}

void AtlasTexture::set_region(const Rect2 &p_region) {

    if (region == p_region)
        return;
    region = p_region;
    emit_changed();
    _change_notify("region");
}

Rect2 AtlasTexture::get_region() const {

    return region;
}

void AtlasTexture::set_margin(const Rect2 &p_margin) {

    if (margin == p_margin)
        return;
    margin = p_margin;
    emit_changed();
    _change_notify("margin");
}

Rect2 AtlasTexture::get_margin() const {

    return margin;
}

void AtlasTexture::set_filter_clip(const bool p_enable) {

    filter_clip = p_enable;
    emit_changed();
    _change_notify("filter_clip");
}

bool AtlasTexture::has_filter_clip() const {

    return filter_clip;
}

void AtlasTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_atlas", {"atlas"}), &AtlasTexture::set_atlas);
    MethodBinder::bind_method(D_METHOD("get_atlas"), &AtlasTexture::get_atlas);

    MethodBinder::bind_method(D_METHOD("set_region", {"region"}), &AtlasTexture::set_region);
    MethodBinder::bind_method(D_METHOD("get_region"), &AtlasTexture::get_region);

    MethodBinder::bind_method(D_METHOD("set_margin", {"margin"}), &AtlasTexture::set_margin);
    MethodBinder::bind_method(D_METHOD("get_margin"), &AtlasTexture::get_margin);

    MethodBinder::bind_method(D_METHOD("set_filter_clip", {"enable"}), &AtlasTexture::set_filter_clip);
    MethodBinder::bind_method(D_METHOD("has_filter_clip"), &AtlasTexture::has_filter_clip);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "atlas", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_atlas", "get_atlas");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region"), "set_region", "get_region");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "margin"), "set_margin", "get_margin");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter_clip"), "set_filter_clip", "has_filter_clip");
}

void AtlasTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if (not atlas)
        return;

    Rect2 rc = region;

    if (rc.size.width == 0) {
        rc.size.width = atlas->get_width();
    }

    if (rc.size.height == 0) {
        rc.size.height = atlas->get_height();
    }

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, Rect2(p_pos + margin.position, rc.size), atlas->get_rid(), rc, p_modulate, p_transpose, normal_rid, filter_clip);
}

void AtlasTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if (not atlas)
        return;

    Rect2 rc = region;

    if (rc.size.width == 0) {
        rc.size.width = atlas->get_width();
    }

    if (rc.size.height == 0) {
        rc.size.height = atlas->get_height();
    }

    Vector2 scale = p_rect.size / (region.size + margin.size);
    Rect2 dr(p_rect.position + margin.position * scale, rc.size * scale);

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, dr, atlas->get_rid(), rc, p_modulate, p_transpose, normal_rid, filter_clip);
}
void AtlasTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    //this might not necessarily work well if using a rect, needs to be fixed properly
    if (not atlas)
        return;

    Rect2 dr;
    Rect2 src_c;
    get_rect_region(p_rect, p_src_rect, dr, src_c);

    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, dr, atlas->get_rid(), src_c, p_modulate, p_transpose, normal_rid, filter_clip);
}

bool AtlasTexture::get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const {

    if (not atlas)
        return false;

    Rect2 rc = region;

    Rect2 src = p_src_rect;
    if (src.size == Size2()) {
        src.size = rc.size;
    }
    Vector2 scale = p_rect.size / src.size;

    src.position += (rc.position - margin.position);
    Rect2 src_c = rc.clip(src);
    if (src_c.size == Size2())
        return false;
    Vector2 ofs = (src_c.position - src.position);

    if (scale.x < 0) {
        float mx = (margin.size.width - margin.position.x);
        mx -= margin.position.x;
        ofs.x = -(ofs.x + mx);
    }
    if (scale.y < 0) {
        float my = margin.size.height - margin.position.y;
        my -= margin.position.y;
        ofs.y = -(ofs.y + my);
    }
    Rect2 dr(p_rect.position + ofs * scale, src_c.size * scale);

    r_rect = dr;
    r_src_rect = src_c;
    return true;
}

bool AtlasTexture::is_pixel_opaque(int p_x, int p_y) const {

    if (not atlas)
        return true;

    int x = p_x + region.position.x - margin.position.x;
    int y = p_y + region.position.y - margin.position.y;

    // margin edge may outside of atlas
    if (x < 0 || x >= atlas->get_width()) return false;
    if (y < 0 || y >= atlas->get_height()) return false;

    return atlas->is_pixel_opaque(x, y);
}

AtlasTexture::AtlasTexture() {
    filter_clip = false;
}

/////////////////////////////////////////

int MeshTexture::get_width() const {
    return size.width;
}
int MeshTexture::get_height() const {
    return size.height;
}
RID MeshTexture::get_rid() const {
    return RID();
}

bool MeshTexture::has_alpha() const {
    return false;
}

void MeshTexture::set_flags(uint32_t p_flags) {
}

uint32_t MeshTexture::get_flags() const {
    return 0;
}

void MeshTexture::set_mesh(const Ref<Mesh> &p_mesh) {
    mesh = p_mesh;
}
const Ref<Mesh> &MeshTexture::get_mesh() const {
    return mesh;
}

void MeshTexture::set_image_size(const Size2 &p_size) {
    size = p_size;
}

Size2 MeshTexture::get_image_size() const {

    return size;
}

void MeshTexture::set_base_texture(const Ref<Texture> &p_texture) {
    base_texture = p_texture;
}

const Ref<Texture> &MeshTexture::get_base_texture() const {
    return base_texture;
}

void MeshTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if (not mesh || not base_texture) {
        return;
    }
    Transform2D xform;
    xform.set_origin(p_pos);
    if (p_transpose) {
        SWAP(xform.elements[0][1], xform.elements[1][0]);
        SWAP(xform.elements[0][0], xform.elements[1][1]);
    }
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
}
void MeshTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {
    if (not mesh || not base_texture) {
        return;
    }
    Transform2D xform;
    Vector2 origin = p_rect.position;
    if (p_rect.size.x < 0) {
        origin.x += size.x;
    }
    if (p_rect.size.y < 0) {
        origin.y += size.y;
    }
    xform.set_origin(origin);
    xform.set_scale(p_rect.size / size);

    if (p_transpose) {
        SWAP(xform.elements[0][1], xform.elements[1][0]);
        SWAP(xform.elements[0][0], xform.elements[1][1]);
    }
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
}
void MeshTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    if (not mesh || not base_texture) {
        return;
    }
    Transform2D xform;
    Vector2 origin = p_rect.position;
    if (p_rect.size.x < 0) {
        origin.x += size.x;
    }
    if (p_rect.size.y < 0) {
        origin.y += size.y;
    }
    xform.set_origin(origin);
    xform.set_scale(p_rect.size / size);

    if (p_transpose) {
        SWAP(xform.elements[0][1], xform.elements[1][0]);
        SWAP(xform.elements[0][0], xform.elements[1][1]);
    }
    RID normal_rid = p_normal_map ? p_normal_map->get_rid() : RID();
    VisualServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
}
bool MeshTexture::get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const {
    r_rect = p_rect;
    r_src_rect = p_src_rect;
    return true;
}

bool MeshTexture::is_pixel_opaque(int p_x, int p_y) const {
    return true;
}

void MeshTexture::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_mesh", {"mesh"}), &MeshTexture::set_mesh);
    MethodBinder::bind_method(D_METHOD("get_mesh"), &MeshTexture::get_mesh);
    MethodBinder::bind_method(D_METHOD("set_image_size", {"size"}), &MeshTexture::set_image_size);
    MethodBinder::bind_method(D_METHOD("get_image_size"), &MeshTexture::get_image_size);
    MethodBinder::bind_method(D_METHOD("set_base_texture", {"texture"}), &MeshTexture::set_base_texture);
    MethodBinder::bind_method(D_METHOD("get_base_texture"), &MeshTexture::get_base_texture);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "base_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_base_texture", "get_base_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "image_size", PROPERTY_HINT_RANGE, "0,16384,1"), "set_image_size", "get_image_size");
}

MeshTexture::MeshTexture() {
}

//////////////////////////////////////////

int LargeTexture::get_width() const {

    return size.width;
}
int LargeTexture::get_height() const {

    return size.height;
}
RID LargeTexture::get_rid() const {

    return RID();
}

bool LargeTexture::has_alpha() const {

    for (int i = 0; i < pieces.size(); i++) {
        if (pieces[i].texture->has_alpha())
            return true;
    }

    return false;
}

void LargeTexture::set_flags(uint32_t p_flags) {

    for (int i = 0; i < pieces.size(); i++) {
        pieces.write[i].texture->set_flags(p_flags);
    }
}

uint32_t LargeTexture::get_flags() const {

    if (!pieces.empty())
        return pieces[0].texture->get_flags();

    return 0;
}

int LargeTexture::add_piece(const Point2 &p_offset, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND_V(not p_texture, -1)
    Piece p;
    p.offset = p_offset;
    p.texture = p_texture;
    pieces.push_back(p);

    return pieces.size() - 1;
}

void LargeTexture::set_piece_offset(int p_idx, const Point2 &p_offset) {

    ERR_FAIL_INDEX(p_idx, pieces.size());
    pieces.write[p_idx].offset = p_offset;
};

void LargeTexture::set_piece_texture(int p_idx, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this)
    ERR_FAIL_INDEX(p_idx, pieces.size());
    pieces.write[p_idx].texture = p_texture;
};

void LargeTexture::set_size(const Size2 &p_size) {

    size = p_size;
}
void LargeTexture::clear() {

    pieces.clear();
    size = Size2i();
}

Array LargeTexture::_get_data() const {

    Array arr;
    for (int i = 0; i < pieces.size(); i++) {
        arr.push_back(pieces[i].offset);
        arr.push_back(pieces[i].texture);
    }
    arr.push_back(Size2(size));
    return arr;
}
void LargeTexture::_set_data(const Array &p_array) {

    ERR_FAIL_COND(p_array.empty())
    ERR_FAIL_COND(!(p_array.size() & 1))
    clear();
    for (int i = 0; i < p_array.size() - 1; i += 2) {
        add_piece(p_array[i], refFromRefPtr<Texture>(p_array[i + 1]));
    }
    size = Size2(p_array[p_array.size() - 1]);
}

int LargeTexture::get_piece_count() const {

    return pieces.size();
}
Vector2 LargeTexture::get_piece_offset(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, pieces.size(), Vector2());
    return pieces[p_idx].offset;
}
Ref<Texture> LargeTexture::get_piece_texture(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, pieces.size(), Ref<Texture>());
    return pieces[p_idx].texture;
}
Ref<Image> LargeTexture::to_image() const {

    Ref<Image> img(make_ref_counted<Image>(this->get_width(), this->get_height(), false, Image::FORMAT_RGBA8));
    for (int i = 0; i < pieces.size(); i++) {

        Ref<Image> src_img = pieces[i].texture->get_data();
        img->blit_rect(src_img, Rect2(0, 0, src_img->get_width(), src_img->get_height()), pieces[i].offset);
    }

    return img;
}

void LargeTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("add_piece", {"ofs", "texture"}), &LargeTexture::add_piece);
    MethodBinder::bind_method(D_METHOD("set_piece_offset", {"idx", "ofs"}), &LargeTexture::set_piece_offset);
    MethodBinder::bind_method(D_METHOD("set_piece_texture", {"idx", "texture"}), &LargeTexture::set_piece_texture);
    MethodBinder::bind_method(D_METHOD("set_size", {"size"}), &LargeTexture::set_size);
    MethodBinder::bind_method(D_METHOD("clear"), &LargeTexture::clear);

    MethodBinder::bind_method(D_METHOD("get_piece_count"), &LargeTexture::get_piece_count);
    MethodBinder::bind_method(D_METHOD("get_piece_offset", {"idx"}), &LargeTexture::get_piece_offset);
    MethodBinder::bind_method(D_METHOD("get_piece_texture", {"idx"}), &LargeTexture::get_piece_texture);

    MethodBinder::bind_method(D_METHOD("_set_data", {"data"}), &LargeTexture::_set_data);
    MethodBinder::bind_method(D_METHOD("_get_data"), &LargeTexture::_get_data);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
}

void LargeTexture::draw(RID p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        pieces[i].texture->draw(p_canvas_item, pieces[i].offset + p_pos, p_modulate, p_transpose, p_normal_map);
    }
}

void LargeTexture::draw_rect(RID p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    //tiling not supported for this
    if (size.x == 0 || size.y == 0)
        return;

    Size2 scale = p_rect.size / size;

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        pieces[i].texture->draw_rect(p_canvas_item, Rect2(pieces[i].offset * scale + p_rect.position, pieces[i].texture->get_size() * scale), false, p_modulate, p_transpose, p_normal_map);
    }
}
void LargeTexture::draw_rect_region(RID p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    //tiling not supported for this
    if (p_src_rect.size.x == 0 || p_src_rect.size.y == 0)
        return;

    Size2 scale = p_rect.size / p_src_rect.size;

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        Rect2 rect(pieces[i].offset, pieces[i].texture->get_size());
        if (!p_src_rect.intersects(rect))
            continue;
        Rect2 local = p_src_rect.clip(rect);
        Rect2 target = local;
        target.size *= scale;
        target.position = p_rect.position + (p_src_rect.position + rect.position) * scale;
        local.position -= rect.position;
        pieces[i].texture->draw_rect_region(p_canvas_item, target, local, p_modulate, p_transpose, p_normal_map, false);
    }
}

bool LargeTexture::is_pixel_opaque(int p_x, int p_y) const {

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        if (not pieces[i].texture)
            continue;

        Rect2 rect(pieces[i].offset, pieces[i].texture->get_size());
        if (rect.has_point(Point2(p_x, p_y))) {
            return pieces[i].texture->is_pixel_opaque(p_x - rect.position.x, p_y - rect.position.y);
        }
    }

    return true;
}

LargeTexture::LargeTexture() {
}

///////////////////////////////////////////////

void CubeMap::set_flags(uint32_t p_flags) {

    flags = p_flags;
    if (_is_valid())
        VisualServer::get_singleton()->texture_set_flags(cubemap, flags);
}

uint32_t CubeMap::get_flags() const {

    return flags;
}

void CubeMap::set_side(Side p_side, const Ref<Image> &p_image) {

    ERR_FAIL_COND(not p_image)
    ERR_FAIL_COND(p_image->empty())
    ERR_FAIL_INDEX(p_side, 6);

    if (!_is_valid()) {
        format = p_image->get_format();
        w = p_image->get_width();
        h = p_image->get_height();
        VisualServer::get_singleton()->texture_allocate(cubemap, w, h, 0, p_image->get_format(), VS::TEXTURE_TYPE_CUBEMAP, flags);
    }

    VisualServer::get_singleton()->texture_set_data(cubemap, p_image, VS::CubeMapSide(p_side));
    valid[p_side] = true;
}

Ref<Image> CubeMap::get_side(Side p_side) const {

    ERR_FAIL_INDEX_V(p_side, 6, Ref<Image>());
    if (!valid[p_side])
        return Ref<Image>();
    return VisualServer::get_singleton()->texture_get_data(cubemap, VS::CubeMapSide(p_side));
}

Image::Format CubeMap::get_format() const {

    return format;
}
int CubeMap::get_width() const {

    return w;
}
int CubeMap::get_height() const {

    return h;
}

RID CubeMap::get_rid() const {

    return cubemap;
}

void CubeMap::set_storage(Storage p_storage) {

    storage = p_storage;
}

CubeMap::Storage CubeMap::get_storage() const {

    return storage;
}

void CubeMap::set_lossy_storage_quality(float p_lossy_storage_quality) {

    lossy_storage_quality = p_lossy_storage_quality;
}

float CubeMap::get_lossy_storage_quality() const {

    return lossy_storage_quality;
}

void CubeMap::set_path(const String &p_path, bool p_take_over) {

    if (cubemap.is_valid()) {
        VisualServer::get_singleton()->texture_set_path(cubemap, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

bool CubeMap::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "side/left") {
        set_side(SIDE_LEFT, refFromRefPtr<Image>(p_value));
    } else if (p_name == "side/right") {
        set_side(SIDE_RIGHT, refFromRefPtr<Image>(p_value));
    } else if (p_name == "side/bottom") {
        set_side(SIDE_BOTTOM, refFromRefPtr<Image>(p_value));
    } else if (p_name == "side/top") {
        set_side(SIDE_TOP, refFromRefPtr<Image>(p_value));
    } else if (p_name == "side/front") {
        set_side(SIDE_FRONT, refFromRefPtr<Image>(p_value));
    } else if (p_name == "side/back") {
        set_side(SIDE_BACK, refFromRefPtr<Image>(p_value));
    } else if (p_name == "storage") {
        storage = Storage(p_value.operator int());
    } else if (p_name == "lossy_quality") {
        lossy_storage_quality = p_value;
    } else
        return false;

    return true;
}

bool CubeMap::_get(const StringName &p_name, Variant &r_ret) const {

    if (p_name == "side/left") {
        r_ret = get_side(SIDE_LEFT);
    } else if (p_name == "side/right") {
        r_ret = get_side(SIDE_RIGHT);
    } else if (p_name == "side/bottom") {
        r_ret = get_side(SIDE_BOTTOM);
    } else if (p_name == "side/top") {
        r_ret = get_side(SIDE_TOP);
    } else if (p_name == "side/front") {
        r_ret = get_side(SIDE_FRONT);
    } else if (p_name == "side/back") {
        r_ret = get_side(SIDE_BACK);
    } else if (p_name == "storage") {
        r_ret = storage;
    } else if (p_name == "lossy_quality") {
        r_ret = lossy_storage_quality;
    } else
        return false;

    return true;
}

void CubeMap::_get_property_list(ListPOD<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/left", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/right", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/bottom", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/top", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/front", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "side/back", PROPERTY_HINT_RESOURCE_TYPE, "Image"));
}

void CubeMap::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("get_width"), &CubeMap::get_width);
    MethodBinder::bind_method(D_METHOD("get_height"), &CubeMap::get_height);
    MethodBinder::bind_method(D_METHOD("set_flags", {"flags"}), &CubeMap::set_flags);
    MethodBinder::bind_method(D_METHOD("get_flags"), &CubeMap::get_flags);
    MethodBinder::bind_method(D_METHOD("set_side", {"side", "image"}), &CubeMap::set_side);
    MethodBinder::bind_method(D_METHOD("get_side", {"side"}), &CubeMap::get_side);
    MethodBinder::bind_method(D_METHOD("set_storage", {"mode"}), &CubeMap::set_storage);
    MethodBinder::bind_method(D_METHOD("get_storage"), &CubeMap::get_storage);
    MethodBinder::bind_method(D_METHOD("set_lossy_storage_quality", {"quality"}), &CubeMap::set_lossy_storage_quality);
    MethodBinder::bind_method(D_METHOD("get_lossy_storage_quality"), &CubeMap::get_lossy_storage_quality);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flags", PROPERTY_HINT_FLAGS, "Mipmaps,Repeat,Filter"), "set_flags", "get_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "storage_mode", PROPERTY_HINT_ENUM, "Raw,Lossy Compressed,Lossless Compressed"), "set_storage", "get_storage");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "lossy_storage_quality"), "set_lossy_storage_quality", "get_lossy_storage_quality");

    BIND_ENUM_CONSTANT(STORAGE_RAW)
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSY)
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSLESS)

    BIND_ENUM_CONSTANT(SIDE_LEFT)
    BIND_ENUM_CONSTANT(SIDE_RIGHT)
    BIND_ENUM_CONSTANT(SIDE_BOTTOM)
    BIND_ENUM_CONSTANT(SIDE_TOP)
    BIND_ENUM_CONSTANT(SIDE_FRONT)
    BIND_ENUM_CONSTANT(SIDE_BACK)

    BIND_ENUM_CONSTANT(FLAG_MIPMAPS)
    BIND_ENUM_CONSTANT(FLAG_REPEAT)
    BIND_ENUM_CONSTANT(FLAG_FILTER)
    BIND_ENUM_CONSTANT(FLAGS_DEFAULT)
}

CubeMap::CubeMap() {

    w = h = 0;
    flags = FLAGS_DEFAULT;
    for (int i = 0; i < 6; i++)
        valid[i] = false;
    cubemap = VisualServer::get_singleton()->texture_create();
    storage = STORAGE_RAW;
    lossy_storage_quality = 0.7f;
    format = Image::FORMAT_BPTC_RGBA;
}

CubeMap::~CubeMap() {

    VisualServer::get_singleton()->free(cubemap);
}

/*	BIND_ENUM(CubeMapSize);
    BIND_ENUM_CONSTANT( FLAG_CUBEMAP )
    BIND_ENUM_CONSTANT( CUBEMAP_LEFT )
    BIND_ENUM_CONSTANT( CUBEMAP_RIGHT )
    BIND_ENUM_CONSTANT( CUBEMAP_BOTTOM )
    BIND_ENUM_CONSTANT( CUBEMAP_TOP )
    BIND_ENUM_CONSTANT( CUBEMAP_FRONT )
    BIND_ENUM_CONSTANT( CUBEMAP_BACK )
*/
///////////////////////////

void CurveTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_width", {"width"}), &CurveTexture::set_width);

    MethodBinder::bind_method(D_METHOD("set_curve", {"curve"}), &CurveTexture::set_curve);
    MethodBinder::bind_method(D_METHOD("get_curve"), &CurveTexture::get_curve);

    MethodBinder::bind_method(D_METHOD("_update"), &CurveTexture::_update);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PROPERTY_HINT_RANGE, "32,4096"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
}

void CurveTexture::set_width(int p_width) {

    ERR_FAIL_COND(p_width < 32 || p_width > 4096)
    _width = p_width;
    _update();
}

int CurveTexture::get_width() const {

    return _width;
}

void CurveTexture::ensure_default_setup(float p_min, float p_max) {
    if (not _curve) {
        Ref<Curve> curve(make_ref_counted<Curve>());
        curve->add_point(Vector2(0, 1));
        curve->add_point(Vector2(1, 1));
        curve->set_min_value(p_min);
        curve->set_max_value(p_max);
        set_curve(curve);
        // Min and max is 0..1 by default
    }
}

void CurveTexture::set_curve(const Ref<Curve>& p_curve) {
    if (_curve != p_curve) {
        if (_curve) {
            _curve->disconnect(CoreStringNames::get_singleton()->changed, this, "_update");
        }
        _curve = p_curve;
        if (_curve) {
            _curve->connect(CoreStringNames::get_singleton()->changed, this, "_update");
        }
        _update();
    }
}

void CurveTexture::_update() {

    PoolVector<uint8_t> data;
    data.resize(_width * sizeof(float));

    // The array is locked in that scope
    {
        PoolVector<uint8_t>::Write wd8 = data.write();
        float *wd = (float *)wd8.ptr();

        if (_curve) {
            Curve &curve = *_curve;
            for (int i = 0; i < _width; ++i) {
                float t = i / static_cast<float>(_width);
                wd[i] = curve.interpolate_baked(t);
            }

        } else {
            for (int i = 0; i < _width; ++i) {
                wd[i] = 0;
            }
        }
    }

    Ref<Image> image(make_ref_counted<Image>(_width, 1, false, Image::FORMAT_RF, data));

    VisualServer::get_singleton()->texture_allocate(_texture, _width, 1, 0, Image::FORMAT_RF, VS::TEXTURE_TYPE_2D, VS::TEXTURE_FLAG_FILTER);
    VisualServer::get_singleton()->texture_set_data(_texture, image);

    emit_changed();
}

Ref<Curve> CurveTexture::get_curve() const {

    return _curve;
}

RID CurveTexture::get_rid() const {

    return _texture;
}

CurveTexture::CurveTexture() {
    _width = 2048;
    _texture = VisualServer::get_singleton()->texture_create();
}
CurveTexture::~CurveTexture() {
    VisualServer::get_singleton()->free(_texture);
}
//////////////////

//setter and getter names for property serialization
#define COLOR_RAMP_GET_OFFSETS "get_offsets"
#define COLOR_RAMP_GET_COLORS "get_colors"
#define COLOR_RAMP_SET_OFFSETS "set_offsets"
#define COLOR_RAMP_SET_COLORS "set_colors"

GradientTexture::GradientTexture() {
    update_pending = false;
    width = 2048;

    texture = VisualServer::get_singleton()->texture_create();
    _queue_update();
}

GradientTexture::~GradientTexture() {
    VisualServer::get_singleton()->free(texture);
}

void GradientTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_gradient", {"gradient"}), &GradientTexture::set_gradient);
    MethodBinder::bind_method(D_METHOD("get_gradient"), &GradientTexture::get_gradient);

    MethodBinder::bind_method(D_METHOD("set_width", {"width"}), &GradientTexture::set_width);

    MethodBinder::bind_method(D_METHOD("_update"), &GradientTexture::_update);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "gradient", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_gradient", "get_gradient");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PROPERTY_HINT_RANGE, "1,2048,1,or_greater"), "set_width", "get_width");
}

void GradientTexture::set_gradient(const Ref<Gradient>& p_gradient) {
    if (p_gradient == gradient)
        return;
    if (gradient) {
        gradient->disconnect(CoreStringNames::get_singleton()->changed, this, "_update");
    }
    gradient = p_gradient;
    if (gradient) {
        gradient->connect(CoreStringNames::get_singleton()->changed, this, "_update");
    }
    _update();
    emit_changed();
}

Ref<Gradient> GradientTexture::get_gradient() const {
    return gradient;
}

void GradientTexture::_queue_update() {

    if (update_pending)
        return;

    update_pending = true;
    call_deferred("_update");
}

void GradientTexture::_update() {

    update_pending = false;

    if (not gradient)
        return;

    PoolVector<uint8_t> data;
    data.resize(width * 4);
    {
        PoolVector<uint8_t>::Write wd8 = data.write();
        Gradient &g = *gradient;

        for (int i = 0; i < width; i++) {

            float ofs = float(i) / (width - 1);
            Color color = g.get_color_at_offset(ofs);

            wd8[i * 4 + 0] = uint8_t(CLAMP(color.r * 255.0, 0, 255));
            wd8[i * 4 + 1] = uint8_t(CLAMP(color.g * 255.0, 0, 255));
            wd8[i * 4 + 2] = uint8_t(CLAMP(color.b * 255.0, 0, 255));
            wd8[i * 4 + 3] = uint8_t(CLAMP(color.a * 255.0, 0, 255));
        }
    }

    Ref<Image> image(make_ref_counted<Image>(width, 1, false, Image::FORMAT_RGBA8, data));

    VisualServer::get_singleton()->texture_allocate(texture, width, 1, 0, Image::FORMAT_RGBA8, VS::TEXTURE_TYPE_2D, VS::TEXTURE_FLAG_FILTER);
    VisualServer::get_singleton()->texture_set_data(texture, image);

    emit_changed();
}

void GradientTexture::set_width(int p_width) {

    width = p_width;
    _queue_update();
}
int GradientTexture::get_width() const {

    return width;
}

Ref<Image> GradientTexture::get_data() const {
    return VisualServer::get_singleton()->texture_get_data(texture);
}

//////////////////////////////////////

void ProxyTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_base", {"base"}), &ProxyTexture::set_base);
    MethodBinder::bind_method(D_METHOD("get_base"), &ProxyTexture::get_base);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "base", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_base", "get_base");
}

void ProxyTexture::set_base(const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this)
    base = p_texture;
    if (base) {
        VisualServer::get_singleton()->texture_set_proxy(proxy, base->get_rid());
    } else {
        VisualServer::get_singleton()->texture_set_proxy(proxy, RID());
    }
}

Ref<Texture> ProxyTexture::get_base() const {

    return base;
}

int ProxyTexture::get_width() const {

    if (base)
        return base->get_width();
    return 1;
}
int ProxyTexture::get_height() const {

    if (base)
        return base->get_height();
    return 1;
}
RID ProxyTexture::get_rid() const {

    return proxy;
}

bool ProxyTexture::has_alpha() const {

    if (base)
        return base->has_alpha();
    return false;
}

void ProxyTexture::set_flags(uint32_t p_flags) {
}

uint32_t ProxyTexture::get_flags() const {

    if (base)
        return base->get_flags();
    return 0;
}

ProxyTexture::ProxyTexture() {

    proxy = VisualServer::get_singleton()->texture_create();
}

ProxyTexture::~ProxyTexture() {

    VisualServer::get_singleton()->free(proxy);
}
//////////////////////////////////////////////

void AnimatedTexture::_update_proxy() {

    RWLockRead r(rw_lock);

    float delta;
    if (prev_ticks == 0) {
        delta = 0;
        prev_ticks = OS::get_singleton()->get_ticks_usec();
    } else {
        uint64_t ticks = OS::get_singleton()->get_ticks_usec();
        delta = float(double(ticks - prev_ticks) / 1000000.0);
        prev_ticks = ticks;
    }

    time += delta;

    float limit;

    if (fps == 0) {
        limit = 0;
    } else {
        limit = 1.0 / fps;
    }

    int iter_max = frame_count;
    while (iter_max) {
        float frame_limit = limit + frames[current_frame].delay_sec;

        if (time > frame_limit) {
            current_frame++;
            if (current_frame >= frame_count) {
                current_frame = 0;
            }
            time -= frame_limit;
        } else {
            break;
        }
        iter_max--;
    }

    if (frames[current_frame].texture) {
        VisualServer::get_singleton()->texture_set_proxy(proxy, frames[current_frame].texture->get_rid());
    }
}

void AnimatedTexture::set_frames(int p_frames) {
    ERR_FAIL_COND(p_frames < 1 || p_frames > MAX_FRAMES)

    RWLockWrite r(rw_lock);

    frame_count = p_frames;
}
int AnimatedTexture::get_frames() const {
    return frame_count;
}

void AnimatedTexture::set_frame_texture(int p_frame, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this)
    ERR_FAIL_INDEX(p_frame, MAX_FRAMES);

    RWLockWrite w(rw_lock);

    frames[p_frame].texture = p_texture;
}
Ref<Texture> AnimatedTexture::get_frame_texture(int p_frame) const {
    ERR_FAIL_INDEX_V(p_frame, MAX_FRAMES, Ref<Texture>());

    RWLockRead r(rw_lock);

    return frames[p_frame].texture;
}

void AnimatedTexture::set_frame_delay(int p_frame, float p_delay_sec) {
    ERR_FAIL_INDEX(p_frame, MAX_FRAMES);

    RWLockRead r(rw_lock);

    frames[p_frame].delay_sec = p_delay_sec;
}
float AnimatedTexture::get_frame_delay(int p_frame) const {
    ERR_FAIL_INDEX_V(p_frame, MAX_FRAMES, 0);

    RWLockRead r(rw_lock);

    return frames[p_frame].delay_sec;
}

void AnimatedTexture::set_fps(float p_fps) {
    ERR_FAIL_COND(p_fps < 0 || p_fps >= 1000)

    fps = p_fps;
}
float AnimatedTexture::get_fps() const {
    return fps;
}

int AnimatedTexture::get_width() const {
    RWLockRead r(rw_lock);

    if (not frames[current_frame].texture) {
        return 1;
    }

    return frames[current_frame].texture->get_width();
}
int AnimatedTexture::get_height() const {
    RWLockRead r(rw_lock);

    if (not frames[current_frame].texture) {
        return 1;
    }

    return frames[current_frame].texture->get_height();
}
RID AnimatedTexture::get_rid() const {
    return proxy;
}

bool AnimatedTexture::has_alpha() const {

    RWLockRead r(rw_lock);

    if (not frames[current_frame].texture) {
        return false;
    }

    return frames[current_frame].texture->has_alpha();
}

Ref<Image> AnimatedTexture::get_data() const {

    RWLockRead r(rw_lock);

    if (not frames[current_frame].texture) {
        return Ref<Image>();
    }

    return frames[current_frame].texture->get_data();
}

bool AnimatedTexture::is_pixel_opaque(int p_x, int p_y) const {

    RWLockRead r(rw_lock);

    if (frames[current_frame].texture) {
        return frames[current_frame].texture->is_pixel_opaque(p_x, p_y);
    }
    return true;
}

void AnimatedTexture::set_flags(uint32_t p_flags) {
}
uint32_t AnimatedTexture::get_flags() const {

    RWLockRead r(rw_lock);

    if (not frames[current_frame].texture) {
        return 0;
    }

    return frames[current_frame].texture->get_flags();
}

void AnimatedTexture::_validate_property(PropertyInfo &property) const {

    String prop = property.name;
    if (StringUtils::begins_with(prop,"frame_")) {
        int frame = StringUtils::to_int(StringUtils::get_slice(StringUtils::get_slice(prop,'/', 0),'_', 1));
        if (frame >= frame_count) {
            property.usage = 0;
        }
    }
}

void AnimatedTexture::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_frames", {"frames"}), &AnimatedTexture::set_frames);
    MethodBinder::bind_method(D_METHOD("get_frames"), &AnimatedTexture::get_frames);

    MethodBinder::bind_method(D_METHOD("set_fps", {"fps"}), &AnimatedTexture::set_fps);
    MethodBinder::bind_method(D_METHOD("get_fps"), &AnimatedTexture::get_fps);

    MethodBinder::bind_method(D_METHOD("set_frame_texture", {"frame", "texture"}), &AnimatedTexture::set_frame_texture);
    MethodBinder::bind_method(D_METHOD("get_frame_texture", {"frame"}), &AnimatedTexture::get_frame_texture);

    MethodBinder::bind_method(D_METHOD("set_frame_delay", {"frame", "delay"}), &AnimatedTexture::set_frame_delay);
    MethodBinder::bind_method(D_METHOD("get_frame_delay", {"frame"}), &AnimatedTexture::get_frame_delay);

    MethodBinder::bind_method(D_METHOD("_update_proxy"), &AnimatedTexture::_update_proxy);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frames", PROPERTY_HINT_RANGE, "1," + itos(MAX_FRAMES), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_frames", "get_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::REAL, "fps", PROPERTY_HINT_RANGE, "0,1024,0.1"), "set_fps", "get_fps");

    for (int i = 0; i < MAX_FRAMES; i++) {
        ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, "frame_" + itos(i) + "/texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_texture", "get_frame_texture", i);
        ADD_PROPERTYI(PropertyInfo(VariantType::REAL, "frame_" + itos(i) + "/delay_sec", PROPERTY_HINT_RANGE, "0.0,16.0,0.01", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_delay", "get_frame_delay", i);
    }

    BIND_CONSTANT(MAX_FRAMES);
}

AnimatedTexture::AnimatedTexture() {
    proxy = VisualServer::get_singleton()->texture_create();
    VisualServer::get_singleton()->texture_set_force_redraw_if_visible(proxy, true);
    time = 0;
    frame_count = 1;
    fps = 4;
    prev_ticks = 0;
    current_frame = 0;
    VisualServer::get_singleton()->connect("frame_pre_draw", this, "_update_proxy");

#ifndef NO_THREADS
    rw_lock = RWLock::create();
#else
    rw_lock = nullptr;
#endif
}

AnimatedTexture::~AnimatedTexture() {
    VisualServer::get_singleton()->free(proxy);
    if (rw_lock) {
        memdelete(rw_lock);
    }
}
///////////////////////////////

void TextureLayered::set_flags(uint32_t p_flags) {
    flags = p_flags;

    if (texture.is_valid()) {
        VisualServer::get_singleton()->texture_set_flags(texture, flags);
    }
}

uint32_t TextureLayered::get_flags() const {
    return flags;
}

Image::Format TextureLayered::get_format() const {
    return format;
}

uint32_t TextureLayered::get_width() const {
    return width;
}

uint32_t TextureLayered::get_height() const {
    return height;
}

uint32_t TextureLayered::get_depth() const {
    return depth;
}

void TextureLayered::_set_data(const Dictionary &p_data) {
    ERR_FAIL_COND(!p_data.has("width"))
    ERR_FAIL_COND(!p_data.has("height"))
    ERR_FAIL_COND(!p_data.has("depth"))
    ERR_FAIL_COND(!p_data.has("format"))
    ERR_FAIL_COND(!p_data.has("flags"))
    ERR_FAIL_COND(!p_data.has("layers"))
    int w = p_data["width"];
    int h = p_data["height"];
    int d = p_data["depth"];
    Image::Format format = Image::Format(int(p_data["format"]));
    int flags = p_data["flags"];
    Array layers = p_data["layers"];
    ERR_FAIL_COND(layers.size() != d)

    create(w, h, d, format, flags);

    for (int i = 0; i < layers.size(); i++) {
        Ref<Image> img(layers[i]);
        ERR_CONTINUE(not img);
        ERR_CONTINUE(img->get_format() != format);
        ERR_CONTINUE(img->get_width() != w);
        ERR_CONTINUE(img->get_height() != h);
        set_layer_data(img, i);
    }
}

Dictionary TextureLayered::_get_data() const {
    Dictionary d;
    d["width"] = width;
    d["height"] = height;
    d["depth"] = depth;
    d["flags"] = flags;
    d["format"] = format;

    Array layers;
    for (int i = 0; i < depth; i++) {
        layers.push_back(get_layer_data(i));
    }
    d["layers"] = layers;
    return d;
}

void TextureLayered::create(uint32_t p_width, uint32_t p_height, uint32_t p_depth, Image::Format p_format, uint32_t p_flags) {
    VisualServer::get_singleton()->texture_allocate(texture, p_width, p_height, p_depth, p_format, is_3d ? VS::TEXTURE_TYPE_3D : VS::TEXTURE_TYPE_2D_ARRAY, p_flags);

    width = p_width;
    height = p_height;
    depth = p_depth;
    format = p_format;
    flags = p_flags;
}

void TextureLayered::set_layer_data(const Ref<Image> &p_image, int p_layer) {
    ERR_FAIL_COND(!texture.is_valid())
    VisualServer::get_singleton()->texture_set_data(texture, p_image, p_layer);
}

Ref<Image> TextureLayered::get_layer_data(int p_layer) const {

    ERR_FAIL_COND_V(!texture.is_valid(), Ref<Image>())
    return VisualServer::get_singleton()->texture_get_data(texture, p_layer);
}

void TextureLayered::set_data_partial(const Ref<Image> &p_image, int p_x_ofs, int p_y_ofs, int p_z, int p_mipmap) {
    ERR_FAIL_COND(!texture.is_valid())
    VisualServer::get_singleton()->texture_set_data_partial(texture, p_image, 0, 0, p_image->get_width(), p_image->get_height(), p_x_ofs, p_y_ofs, p_mipmap, p_z);
}

RID TextureLayered::get_rid() const {
    return texture;
}

void TextureLayered::set_path(const String &p_path, bool p_take_over) {
    if (texture.is_valid()) {
        VisualServer::get_singleton()->texture_set_path(texture, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

void TextureLayered::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_flags", {"flags"}), &TextureLayered::set_flags);
    MethodBinder::bind_method(D_METHOD("get_flags"), &TextureLayered::get_flags);

    MethodBinder::bind_method(D_METHOD("get_format"), &TextureLayered::get_format);

    MethodBinder::bind_method(D_METHOD("get_width"), &TextureLayered::get_width);
    MethodBinder::bind_method(D_METHOD("get_height"), &TextureLayered::get_height);
    MethodBinder::bind_method(D_METHOD("get_depth"), &TextureLayered::get_depth);

    MethodBinder::bind_method(D_METHOD("create", {"width", "height", "depth", "format", "flags"}), &TextureLayered::create, {DEFVAL(FLAGS_DEFAULT)});
    MethodBinder::bind_method(D_METHOD("set_layer_data", {"image", "layer"}), &TextureLayered::set_layer_data);
    MethodBinder::bind_method(D_METHOD("get_layer_data", {"layer"}), &TextureLayered::get_layer_data);
    MethodBinder::bind_method(D_METHOD("set_data_partial", {"image", "x_offset", "y_offset", "layer", "mipmap"}), &TextureLayered::set_data_partial, {DEFVAL(0)});

    MethodBinder::bind_method(D_METHOD("_set_data", {"data"}), &TextureLayered::_set_data);
    MethodBinder::bind_method(D_METHOD("_get_data"), &TextureLayered::_get_data);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flags", PROPERTY_HINT_FLAGS, "Mipmaps,Repeat,Filter"), "set_flags", "get_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR), "_set_data", "_get_data");

    BIND_ENUM_CONSTANT(FLAG_MIPMAPS)
    BIND_ENUM_CONSTANT(FLAG_REPEAT)
    BIND_ENUM_CONSTANT(FLAG_FILTER)
    BIND_ENUM_CONSTANT(FLAGS_DEFAULT)
}

TextureLayered::TextureLayered(bool p_3d) {
    is_3d = p_3d;
    format = Image::FORMAT_MAX;
    flags = FLAGS_DEFAULT;

    width = 0;
    height = 0;
    depth = 0;

    texture = VisualServer::get_singleton()->texture_create();
}

TextureLayered::~TextureLayered() {
    if (texture.is_valid()) {
        VisualServer::get_singleton()->free(texture);
    }
}

RES ResourceFormatLoaderTextureLayered::load(const String &p_path, const String &p_original_path, Error *r_error) {

    if (r_error) {
        *r_error = ERR_CANT_OPEN;
    }

    Ref<TextureLayered> lt;
    Ref<Texture3D> tex3d;
    Ref<TextureArray> texarr;

    if (StringUtils::ends_with(p_path,"tex3d")) {
        tex3d = make_ref_counted<Texture3D>();
        lt = tex3d;
    } else if (StringUtils::ends_with(p_path,"texarr")) {
        texarr = make_ref_counted<TextureArray>();
        lt = texarr;
    } else {
        ERR_FAIL_V_CMSG(RES(), "Unrecognized layered texture extension.")
    }

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V(!f, RES())

    uint8_t header[5] = { 0, 0, 0, 0, 0 };
    f->get_buffer(header, 4);

    if (header[0] == 'G' && header[1] == 'D' && header[2] == '3' && header[3] == 'T') {
        if (not tex3d) {
            memdelete(f);
            ERR_FAIL_COND_V(not tex3d, RES())
        }
    } else if (header[0] == 'G' && header[1] == 'D' && header[2] == 'A' && header[3] == 'T') {
        if (not texarr) {
            memdelete(f);
            ERR_FAIL_COND_V(not texarr, RES())
        }
    } else {

        ERR_FAIL_V_MSG(RES(), "Unrecognized layered texture file format: " + String((const char *)header) + ".")
    }

    int tw = f->get_32();
    int th = f->get_32();
    int td = f->get_32();
    int flags = f->get_32(); //texture flags!
    Image::Format format = Image::Format(f->get_32());
    uint32_t compression = f->get_32(); // 0 - lossless (PNG), 1 - vram, 2 - uncompressed

    lt->create(tw, th, td, format, flags);

    for (int layer = 0; layer < td; layer++) {

        Ref<Image> image(make_ref_counted<Image>());

        if (compression == COMPRESSION_LOSSLESS) {
            //look for a PNG file inside

            int mipmaps = f->get_32();
            Vector<Ref<Image> > mipmap_images;

            for (int i = 0; i < mipmaps; i++) {
                uint32_t size = f->get_32();

                PODVector<uint8_t> pv;
                pv.resize(size);
                f->get_buffer(pv.data(), size);

                Ref<Image> img = Image::lossless_unpacker(pv);

                if (not img || img->empty() || format != img->get_format()) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                    }
                    memdelete(f);
                    ERR_FAIL_V(RES())
                }

                mipmap_images.push_back(img);
            }

            if (mipmap_images.size() == 1) {

                image = mipmap_images[0];

            } else {
                int total_size = Image::get_image_data_size(tw, th, format, true);
                PoolVector<uint8_t> img_data;
                img_data.resize(total_size);

                {
                    PoolVector<uint8_t>::Write w = img_data.write();

                    int ofs = 0;
                    for (int i = 0; i < mipmap_images.size(); i++) {

                        PoolVector<uint8_t> id = mipmap_images[i]->get_data();
                        int len = id.size();
                        PoolVector<uint8_t>::Read r = id.read();
                        memcpy(&w[ofs], r.ptr(), len);
                        ofs += len;
                    }
                }

                image->create(tw, th, true, format, img_data);
                if (image->empty()) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                    }
                    memdelete(f);
                    ERR_FAIL_V(RES());
                }
            }

        } else {

            //look for regular format
            bool mipmaps = (flags & Texture::FLAG_MIPMAPS);
            int total_size = Image::get_image_data_size(tw, th, format, mipmaps);

            PoolVector<uint8_t> img_data;
            img_data.resize(total_size);

            {
                PoolVector<uint8_t>::Write w = img_data.write();
                int bytes = f->get_buffer(w.ptr(), total_size);
                if (bytes != total_size) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                        memdelete(f);
                    }
                    ERR_FAIL_V(RES());
                }
            }

            image->create(tw, th, mipmaps, format, img_data);
        }

        lt->set_layer_data(image, layer);
    }

    if (r_error)
        *r_error = OK;

    return lt;
}

void ResourceFormatLoaderTextureLayered::get_recognized_extensions(ListPOD<String> *p_extensions) const {

    p_extensions->push_back("tex3d");
    p_extensions->push_back("texarr");
}
bool ResourceFormatLoaderTextureLayered::handles_type(const String &p_type) const {
    return p_type == "Texture3D" || p_type == "TextureArray";
}
String ResourceFormatLoaderTextureLayered::get_resource_type(const String &p_path) const {

    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "tex3d")
        return "Texture3D";
    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "texarr")
        return "TextureArray";
    return "";
}

#include "camera_texture.h"
#include "servers/camera_server_enum_casters.h"


IMPL_GDCLASS(CameraTexture)

void CameraTexture::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_camera_feed_id", {"feed_id"}), &CameraTexture::set_camera_feed_id);
    MethodBinder::bind_method(D_METHOD("get_camera_feed_id"), &CameraTexture::get_camera_feed_id);

    MethodBinder::bind_method(D_METHOD("set_which_feed", {"which_feed"}), &CameraTexture::set_which_feed);
    MethodBinder::bind_method(D_METHOD("get_which_feed"), &CameraTexture::get_which_feed);

    MethodBinder::bind_method(D_METHOD("set_camera_active", {"active"}), &CameraTexture::set_camera_active);
    MethodBinder::bind_method(D_METHOD("get_camera_active"), &CameraTexture::get_camera_active);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "camera_feed_id"), "set_camera_feed_id", "get_camera_feed_id");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "which_feed"), "set_which_feed", "get_which_feed");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "camera_is_active"), "set_camera_active", "get_camera_active");
}

int CameraTexture::get_width() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_base_width();
    } else {
        return 0;
    }
}

int CameraTexture::get_height() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_base_height();
    } else {
        return 0;
    }
}

bool CameraTexture::has_alpha() const {
    return false;
}

RID CameraTexture::get_rid() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->get_texture(which_feed);
    } else {
        return RID();
    }
}

void CameraTexture::set_flags(uint32_t p_flags) {
    // not supported
}

uint32_t CameraTexture::get_flags() const {
    // not supported
    return 0;
}

Ref<Image> CameraTexture::get_data() const {
    // not (yet) supported
    return Ref<Image>();
}

void CameraTexture::set_camera_feed_id(int p_new_id) {
    camera_feed_id = p_new_id;
    _change_notify();
}

int CameraTexture::get_camera_feed_id() const {
    return camera_feed_id;
}

void CameraTexture::set_which_feed(CameraServer::FeedImage p_which) {
    which_feed = p_which;
    _change_notify();
}

CameraServer::FeedImage CameraTexture::get_which_feed() const {
    return which_feed;
}

void CameraTexture::set_camera_active(bool p_active) {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        feed->set_active(p_active);
        _change_notify();
    }
}

bool CameraTexture::get_camera_active() const {
    Ref<CameraFeed> feed = CameraServer::get_singleton()->get_feed_by_id(camera_feed_id);
    if (feed) {
        return feed->is_active();
    } else {
        return false;
    }
}

CameraTexture::CameraTexture() {
    camera_feed_id = 0;
    which_feed = CameraServer::FEED_RGBA_IMAGE;
}

CameraTexture::~CameraTexture() {
    // nothing to do here yet
}
