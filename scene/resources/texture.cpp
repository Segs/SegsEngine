/*************************************************************************/
/*  texture.cpp                                                          */
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

#include "texture.h"
#include "texture_serializers.h"

#include "textures_enum_casters.h"
#include "curve_texture.h"
#include "mesh.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/image_enum_casters.h"
#include "core/io/image_loader.h"
#include "core/io/image_saver.h"
#include "core/io/resource_saver.h"
#include "core/io/resource_loader.h"
#include "core/math/geometry.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/os/os.h"
#include "core/string_formatter.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/resource/resource_manager.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/mesh.h"
#include "servers/rendering_server.h"

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
IMPL_GDCLASS(GradientTexture)
IMPL_GDCLASS(GradientTexture2D)
IMPL_GDCLASS(ProxyTexture)
IMPL_GDCLASS(AnimatedTexture)
IMPL_GDCLASS(ExternalTexture)

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
        Error save(StringView p_path, const RES &p_resource, uint32_t p_flags = 0) final {

            Error err;
            Ref<ImageTexture> texture(dynamic_ref_cast<ImageTexture>(p_resource));

            ERR_FAIL_COND_V_MSG(not texture, ERR_INVALID_PARAMETER, "Can't save invalid texture as PNG.");
            ERR_FAIL_COND_V_MSG(!texture->get_width(), ERR_INVALID_PARAMETER, "Can't save empty texture as PNG.");

            Ref<Image> img(texture->get_data());
            FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
            ERR_FAIL_COND_V_MSG(err, err, FormatVE("Can't save using saver wrapper at path: '%.*s'.", (int)p_path.size(),p_path.data()));
            Vector<uint8_t> buffer;
            err = m_saver->save_image(img->img_data(),buffer,{});

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
        void get_recognized_extensions(const RES &p_resource, Vector<String> &p_extensions) const final {
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
                gResourceManager().add_resource_format_saver(Ref<ResourceSaverImage>(make_ref_counted<ResourceSaverImage>(svr)));

        }
    }
}


Size2 Texture::get_size() const {

    return Size2(get_width(), get_height());
}

bool Texture::is_pixel_opaque(int p_x, int p_y) const {
    return true;
}
void Texture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, get_size()), get_rid(), false, p_modulate, p_transpose, normal_rid);
}
void Texture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, get_rid(), p_tile, p_modulate, p_transpose, normal_rid);
}
void Texture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, get_rid(), p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
}

bool Texture::get_rect_region(const Rect2 &p_rect, const Rect2 &p_src_rect, Rect2 &r_rect, Rect2 &r_src_rect) const {

    r_rect = p_rect;
    r_src_rect = p_src_rect;

    return true;
}

void Texture::_bind_methods() {
    SE_BIND_METHOD(Texture,get_width);
    SE_BIND_METHOD(Texture,get_height);
    SE_BIND_METHOD(Texture,get_size);
    SE_BIND_METHOD(Texture,has_alpha);
    SE_BIND_METHOD(Texture,set_flags);
    SE_BIND_METHOD(Texture,get_flags);
    MethodBinder::bind_method(D_METHOD("draw", {"canvas_item", "position", "modulate", "transpose", "normal_map"}), &Texture::draw, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("draw_rect", {"canvas_item", "rect", "tile", "modulate", "transpose", "normal_map"}), &Texture::draw_rect, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant())});
    MethodBinder::bind_method(D_METHOD("draw_rect_region", {"canvas_item", "rect", "src_rect", "modulate", "transpose", "normal_map", "clip_uv"}), &Texture::draw_rect_region, {DEFVAL(Color(1, 1, 1)), DEFVAL(false), DEFVAL(Variant()), DEFVAL(true)});
    SE_BIND_METHOD(Texture,get_data);

    ADD_GROUP("Flags", "flg_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flg_flags", PropertyHint::Flags, "Mipmaps,Repeat,Filter,Anisotropic Filter,Convert to Linear,Mirrored Repeat,Video Surface"), "set_flags", "get_flags");
    ADD_GROUP("", "");

    BIND_ENUM_CONSTANT(FLAGS_DEFAULT);
    BIND_ENUM_CONSTANT(FLAG_MIPMAPS);
    BIND_ENUM_CONSTANT(FLAG_REPEAT);
    BIND_ENUM_CONSTANT(FLAG_FILTER);
    BIND_ENUM_CONSTANT(FLAG_ANISOTROPIC_FILTER);
    BIND_ENUM_CONSTANT(FLAG_CONVERT_TO_LINEAR);
    BIND_ENUM_CONSTANT(FLAG_MIRRORED_REPEAT);
    BIND_ENUM_CONSTANT(FLAG_VIDEO_SURFACE);
}

Texture::Texture() {
}

/////////////////////

void ImageTexture::reload_from_file() {

    String path = gResourceRemapper().path_remap(get_path());
    if (!PathUtils::is_resource_file(path))
        return;

    uint32_t flags = get_flags();
    Ref<Image> img(make_ref_counted<Image>());

    if (ImageLoader::load_image(path, img) == OK) {
        create_from_image(img, flags);
    } else {
        Resource::reload_from_file();
        Object_change_notify(this);
        emit_changed();
    }
}

bool ImageTexture::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "image")
        create_from_image(refFromVariant<Image>(p_value), flags);
    else if (p_name == "flags")
        if (w * h == 0)
            flags = p_value.as<uint32_t>();
        else
            set_flags(p_value.as<uint32_t>());
    else if (p_name == "size") {
        Size2 s = p_value.as<Vector2>();
        w = s.width;
        h = s.height;
        RenderingServer::get_singleton()->texture_set_size_override(texture, w, h, 0);
    } else if (p_name == "_data") {
        _set_data(p_value.as<Dictionary>());
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

void ImageTexture::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->push_back(PropertyInfo(VariantType::INT, "flags", PropertyHint::Flags, "Mipmaps,Repeat,Filter,Anisotropic,sRGB,Mirrored Repeat"));
    p_list->push_back(PropertyInfo(VariantType::OBJECT, "image", PropertyHint::ResourceType, "Image", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT));
    p_list->push_back(PropertyInfo(VariantType::VECTOR2, "size", PropertyHint::None, ""));
}

void ImageTexture::_reload_hook(RenderingEntity p_hook) {

    String path = get_path();
    if (!PathUtils::is_resource_file(path))
        return;

    Ref<Image> img(make_ref_counted<Image>());
    Error err = ImageLoader::load_image(path, img);

    ERR_FAIL_COND(err != OK);

    RenderingServer::get_singleton()->texture_set_data(texture, img);

    Object_change_notify(this);
    emit_changed();
}

void ImageTexture::create(int p_width, int p_height, Image::Format p_format, uint32_t p_flags) {

    flags = p_flags;
    RenderingServer::get_singleton()->texture_allocate(texture, p_width, p_height, 0, p_format, RS::TEXTURE_TYPE_2D, p_flags);
    format = p_format;
    w = p_width;
    h = p_height;
    Object_change_notify(this);
    emit_changed();
}
void ImageTexture::create_from_image(const Ref<Image> &p_image, uint32_t p_flags) {

    ERR_FAIL_COND_MSG(not p_image || p_image->is_empty(), "Invalid image");
    ERR_FAIL_COND(not p_image);
    flags = p_flags;
    w = p_image->get_width();
    h = p_image->get_height();
    format = p_image->get_format();

    RenderingServer::get_singleton()->texture_allocate(texture, p_image->get_width(), p_image->get_height(), 0, p_image->get_format(), RS::TEXTURE_TYPE_2D, p_flags);
    RenderingServer::get_singleton()->texture_set_data(texture, p_image);
    Object_change_notify(this);
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
    RenderingServer::get_singleton()->texture_set_flags(texture, p_flags);
    Object_change_notify(this,"flags");
    emit_changed();
}

uint32_t ImageTexture::get_flags() const {

    return ImageTexture::flags;
}

Image::Format ImageTexture::get_format() const {

    return format;
}

void ImageTexture::set_data(const Ref<Image> &p_image) {

    ERR_FAIL_COND_MSG(not p_image,"Invalid image");

    RenderingServer::get_singleton()->texture_set_data(texture, p_image);

    Object_change_notify(this);
    emit_changed();

    alpha_cache.reset(); //TODO: memory de-allocation
    image_stored = true;
}

void ImageTexture::_resource_path_changed() {
    //TODO: SEGS: ImageTexture::_resource_path_changed - this looks like a dead code?
    String path = get_path();
}

Ref<Image> ImageTexture::get_data() const {

    if (image_stored) {
        return RenderingServer::get_singleton()->texture_get_data(texture);
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

RenderingEntity ImageTexture::get_rid() const {

    return texture;
}

bool ImageTexture::has_alpha() const {

    return (format == ImageData::FORMAT_LA8 || format == ImageData::FORMAT_RGBA8);
}

void ImageTexture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((w | h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, Size2(w, h)), texture, false, p_modulate, p_transpose, normal_rid);
}
void ImageTexture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((w | h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, texture, p_tile, p_modulate, p_transpose, normal_rid);
}
void ImageTexture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    if ((w | h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, texture, p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
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
    RenderingServer::get_singleton()->texture_set_size_override(texture, w, h, 0);
}

void ImageTexture::set_path(StringView p_path, bool p_take_over) {

    if (texture!=entt::null) {
        RenderingServer::get_singleton()->texture_set_path(texture, p_path);
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
    ERR_FAIL_COND(not img);
    uint32_t flags = p_data["flags"].as<uint32_t>();

    create_from_image(img, flags);

    set_storage(Storage(p_data["storage"].as<int>()));
    set_lossy_storage_quality(p_data["lossy_quality"].as<float>());

    set_size_override(p_data["size"].as<Vector2>());
}

void ImageTexture::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("create", {"width", "height", "format", "flags"}), &ImageTexture::create, {DEFVAL(FLAGS_DEFAULT)});
    MethodBinder::bind_method(D_METHOD("create_from_image", {"image", "flags"}), &ImageTexture::create_from_image, {DEFVAL(FLAGS_DEFAULT)});
    SE_BIND_METHOD(ImageTexture,get_format);
    SE_BIND_METHOD(ImageTexture,set_data);
    SE_BIND_METHOD(ImageTexture,set_storage);
    SE_BIND_METHOD(ImageTexture,get_storage);
    SE_BIND_METHOD(ImageTexture,set_lossy_storage_quality);
    SE_BIND_METHOD(ImageTexture,get_lossy_storage_quality);

    SE_BIND_METHOD(ImageTexture,set_size_override);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "storage", PropertyHint::Enum, "Uncompressed,Compress Lossy,Compress Lossless"), "set_storage", "get_storage");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lossy_quality", PropertyHint::Range, "0.0,1.0,0.01"), "set_lossy_storage_quality", "get_lossy_storage_quality");

    BIND_ENUM_CONSTANT(STORAGE_RAW);
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSY);
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSLESS);
}

ImageTexture::ImageTexture() {

    w = h = 0;
    flags = FLAGS_DEFAULT;
    texture = RenderingServer::get_singleton()->texture_create();
    storage = STORAGE_RAW;
    lossy_storage_quality = 0.7f;
    image_stored = false;
    format = ImageData::FORMAT_L8;
}

ImageTexture::~ImageTexture() {

    RenderingServer::get_singleton()->free_rid(texture);
}

//////////////////////////////////////////
struct StreamTexture::StreamTextureData {
    String path_to_file;
    RenderingEntity texture=entt::null;
    uint32_t flags;
    int w, h;
    Image::Format format;
    mutable eastl::unique_ptr<BitMap> alpha_cache;
};

void StreamTexture::set_path(StringView p_path, bool p_take_over) {

    if (m_impl_data->texture!=entt::null) {
        RenderingServer::get_singleton()->texture_set_path(m_impl_data->texture, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

void StreamTexture::_requested_3d(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_3d_callback);
    request_3d_callback(StringName(stex->get_path()));
}

void StreamTexture::_requested_srgb(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_srgb_callback);
    request_srgb_callback(StringName(stex->get_path()));
}

void StreamTexture::_requested_normal(void *p_ud) {

    StreamTexture *st = (StreamTexture *)p_ud;
    Ref<StreamTexture> stex(st);
    ERR_FAIL_COND(!request_normal_callback);
    request_normal_callback(StringName(stex->get_path()));
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

Error StreamTexture::_load_data(StringView p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image, int p_size_limit) {

    m_impl_data->alpha_cache.reset(nullptr); // TODO: memory de-allocation, check if actually needed ?

    ERR_FAIL_COND_V(not image, ERR_INVALID_PARAMETER);

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, FormatVE("Unable to open file: %.*s.", p_path.size(),p_path.data()));

    uint8_t header[4];
    f->get_buffer(header, 4);
    if (header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T') {
        memdelete(f);
        ERR_FAIL_COND_V(header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T', ERR_FILE_CORRUPT);
    }

    tw = f->get_16();
    tw_custom = f->get_16();
    th = f->get_16();
    th_custom = f->get_16();

    flags = f->get_32(); //texture flags!
    uint32_t df = f->get_32(); //data format

#ifdef TOOLS_ENABLED
    RenderingEntity texture = m_impl_data->texture;
    if (request_3d_callback && df & FORMAT_BIT_DETECT_3D) {
        RenderingServer::get_singleton()->texture_set_detect_3d_callback(texture, _requested_3d, this);
    } else {
        RenderingServer::get_singleton()->texture_set_detect_3d_callback(texture, nullptr, nullptr);
    }

    if (request_srgb_callback && df & FORMAT_BIT_DETECT_SRGB) {
        RenderingServer::get_singleton()->texture_set_detect_srgb_callback(texture, _requested_srgb, this);
    } else {
        RenderingServer::get_singleton()->texture_set_detect_srgb_callback(texture, nullptr, nullptr);
    }

    if (request_srgb_callback && df & FORMAT_BIT_DETECT_NORMAL) {
        RenderingServer::get_singleton()->texture_set_detect_normal_callback(texture, _requested_normal, this);
    } else {
        RenderingServer::get_singleton()->texture_set_detect_normal_callback(texture, nullptr, nullptr);
    }
#endif
    if (!(df & FORMAT_BIT_STREAM)) {
        p_size_limit = 0;
    }

    if (df & FORMAT_BIT_PNG || df & FORMAT_BIT_WEBP) {
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

            sw = M_MAX(sw >> 1, 1);
            sh = M_MAX(sh >> 1, 1);
            mipmaps--;
        }

        //mipmaps need to be read independently, they will be later combined
        Vector<Ref<Image> > mipmap_images;
        uint64_t total_size = 0;
        Vector<uint8_t> pv;

        for (uint32_t i = 0; i < mipmaps; i++) {

            if (i) {
                size = f->get_32();
            }

            pv.resize(size);
            f->get_buffer(pv.data(), size);
            Ref<Image> img;
            if (df & FORMAT_BIT_PNG) {
                img = Image::png_unpacker(pv);
            } else {
                img = Image::webp_unpacker(pv);
            }

            if (not img || img->is_empty()) {
                memdelete(f);
                ERR_FAIL_COND_V(not img || img->is_empty(), ERR_FILE_CORRUPT);
            }

            if (i != 0) {
                img->convert(mipmap_images[0]->get_format()); // ensure the same format for all mipmaps
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
                for (size_t i = 0; i < mipmap_images.size(); i++) {

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
            uint64_t size = Image::get_image_data_size(tw, th, format, false);

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
            uint64_t total_size = Image::get_image_data_size(tw, th, format, true);
            int idx = 0;

            while (mipmaps2 > 1 && p_size_limit > 0 && (sw > p_size_limit || sh > p_size_limit)) {

                sw = M_MAX(sw >> 1, 1);
                sh = M_MAX(sh >> 1, 1);
                mipmaps2--;
                idx++;
            }

            int ofs = Image::get_image_mipmap_offset(tw, th, format, idx);

            if (total_size - ofs <= 0) {
                memdelete(f);
                ERR_FAIL_V(ERR_FILE_CORRUPT);
            }

            f->seek(f->get_position() + ofs);

            PoolVector<uint8_t> img_data;
            img_data.resize(total_size - ofs);

            {
                PoolVector<uint8_t>::Write w = img_data.write();
                uint64_t bytes = f->get_buffer(w.ptr(), total_size - ofs);
                //print_line("requested read: " + itos(total_size - ofs) + " but got: " + itos(bytes));

                memdelete(f);

                uint64_t expected = total_size - ofs;
                if (bytes < expected) {
                    //this is a compatibility workaround for older format, which saved less mipmaps2. It is still recommended the image is reimported.
                    memset(w.ptr() + bytes, 0, (expected - bytes));
                } else if (bytes != expected) {
                    ERR_FAIL_V(ERR_FILE_CORRUPT);
                }
            }

            image->create(sw, sh, true, format, img_data);

            return OK;
        }
    }

    return ERR_BUG; //unreachable
}

Error StreamTexture::load(StringView p_path) {

    int lw, lh, lwc, lhc, lflags;
    Ref<Image> image(make_ref_counted<Image>());
    Error err = _load_data(p_path, lw, lh, lwc, lhc, lflags, image);
    if (err)
        return err;
    RenderingEntity texture = m_impl_data->texture;

    if (get_path().empty()) {
        //temporarily set path if no path set for resource, helps find errors
        RenderingServer::get_singleton()->texture_set_path(texture, p_path);
    }
    RenderingServer::get_singleton()->texture_allocate(texture, image->get_width(), image->get_height(), 0, image->get_format(), RS::TEXTURE_TYPE_2D, lflags);
    RenderingServer::get_singleton()->texture_set_data(texture, image);
    if (lwc || lhc) {
        RenderingServer::get_singleton()->texture_set_size_override(texture, lwc, lhc, 0);
    } else {
    }

    m_impl_data->w = lwc ? lwc : lw;
    m_impl_data->h = lhc ? lhc : lh;
    m_impl_data->flags = lflags;
    m_impl_data->path_to_file = p_path;
    m_impl_data->format = image->get_format();

    Object_change_notify(this);
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
RenderingEntity StreamTexture::get_rid() const {

    return m_impl_data->texture;
}

void StreamTexture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, Rect2(p_pos, Size2(m_impl_data->w, m_impl_data->h)), m_impl_data->texture, false, p_modulate, p_transpose, normal_rid);
}
void StreamTexture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect(p_canvas_item, p_rect, m_impl_data->texture, p_tile, p_modulate, p_transpose, normal_rid);
}
void StreamTexture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    if ((m_impl_data->w | m_impl_data->h) == 0)
        return;
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, p_rect, m_impl_data->texture, p_src_rect, p_modulate, p_transpose, normal_rid, p_clip_uv);
}

bool StreamTexture::has_alpha() const {

    return false;
}

Ref<Image> StreamTexture::get_data() const {

    return RenderingServer::get_singleton()->texture_get_data(m_impl_data->texture);
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
    RenderingServer::get_singleton()->texture_set_flags(m_impl_data->texture, m_impl_data->flags);
    Object_change_notify(this,"flags");
    emit_changed();
}

void StreamTexture::reload_from_file() {

    String path = get_path();
    if (!PathUtils::is_resource_file(path))
        return;

    path = gResourceRemapper().path_remap(path); //remap for translation
    path = gResourceRemapper().import_remap(path); //remap for import
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

    SE_BIND_METHOD(StreamTexture,load);
    SE_BIND_METHOD(StreamTexture,get_load_path);

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "load_path", PropertyHint::File, "*.stex"), "load", "get_load_path");
}

StreamTexture::StreamTexture() {
    m_impl_data = new StreamTextureData;
    m_impl_data->format = ImageData::FORMAT_MAX;
    m_impl_data->flags = 0;
    m_impl_data->w = 0;
    m_impl_data->h = 0;

    m_impl_data->texture = RenderingServer::get_singleton()->texture_create();
}

StreamTexture::~StreamTexture() {
    RenderingServer::get_singleton()->free_rid(m_impl_data->texture);
    delete m_impl_data;
}

RES ResourceFormatLoaderStreamTexture::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    Ref<StreamTexture> st(make_ref_counted<StreamTexture>());
    Error err = st->load(p_path);
    if (r_error)
        *r_error = err;
    if (err != OK)
        return RES();

    return st;
}

void ResourceFormatLoaderStreamTexture::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back(("stex"));
}
bool ResourceFormatLoaderStreamTexture::handles_type(StringView p_type) const {
    return p_type == StringView("StreamTexture");
}
String ResourceFormatLoaderStreamTexture::get_resource_type(StringView p_path) const {

    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "stex")
        return ("StreamTexture");
    return {};
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
RenderingEntity AtlasTexture::get_rid() const {

    if (atlas)
        return atlas->get_rid();

    return entt::null;
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

    ERR_FAIL_COND(this == p_atlas.get());
    if (atlas == p_atlas)
        return;
    atlas = p_atlas;
    emit_changed();
    Object_change_notify(this,"atlas");
}
Ref<Texture> AtlasTexture::get_atlas() const {

    return atlas;
}

void AtlasTexture::set_region(const Rect2 &p_region) {

    if (region == p_region)
        return;
    region = p_region;
    emit_changed();
    Object_change_notify(this,"region");
}

Rect2 AtlasTexture::get_region() const {

    return region;
}

void AtlasTexture::set_margin(const Rect2 &p_margin) {

    if (margin == p_margin)
        return;
    margin = p_margin;
    emit_changed();
    Object_change_notify(this,"margin");
}

Rect2 AtlasTexture::get_margin() const {

    return margin;
}

void AtlasTexture::set_filter_clip(const bool p_enable) {

    filter_clip = p_enable;
    emit_changed();
    Object_change_notify(this,"filter_clip");
}

Ref<Image> AtlasTexture::get_data() const {
    if (!atlas || !atlas->get_data()) {
        return Ref<Image>();
    }

    return atlas->get_data()->get_rect(region);
}

bool AtlasTexture::has_filter_clip() const {

    return filter_clip;
}

void AtlasTexture::_bind_methods() {

    SE_BIND_METHOD(AtlasTexture,set_atlas);
    SE_BIND_METHOD(AtlasTexture,get_atlas);

    SE_BIND_METHOD(AtlasTexture,set_region);
    SE_BIND_METHOD(AtlasTexture,get_region);

    SE_BIND_METHOD(AtlasTexture,set_margin);
    SE_BIND_METHOD(AtlasTexture,get_margin);

    SE_BIND_METHOD(AtlasTexture,set_filter_clip);
    SE_BIND_METHOD(AtlasTexture,has_filter_clip);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "atlas", PropertyHint::ResourceType, "Texture"), "set_atlas", "get_atlas");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "region"), "set_region", "get_region");
    ADD_PROPERTY(PropertyInfo(VariantType::RECT2, "margin"), "set_margin", "get_margin");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "filter_clip"), "set_filter_clip", "has_filter_clip");
}

void AtlasTexture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if (not atlas)
        return;

    Rect2 rc = region;

    if (rc.size.width == 0) {
        rc.size.width = atlas->get_width();
    }

    if (rc.size.height == 0) {
        rc.size.height = atlas->get_height();
    }

    atlas->draw_rect_region(p_canvas_item, Rect2(p_pos + margin.position, rc.size), rc, p_modulate, p_transpose, p_normal_map);
}

void AtlasTexture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

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

    atlas->draw_rect_region(p_canvas_item, dr, rc, p_modulate, p_transpose, p_normal_map);
}
void AtlasTexture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

    //this might not necessarily work well if using a rect, needs to be fixed properly
    if (not atlas)
        return;

    Rect2 dr;
    Rect2 src_c;
    get_rect_region(p_rect, p_src_rect, dr, src_c);

    atlas->draw_rect_region(p_canvas_item, dr, src_c, p_modulate, p_transpose, p_normal_map);
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
    if (x < 0 || x >= atlas->get_width()) {
        return false;
    }
    if (y < 0 || y >= atlas->get_height()) {
        return false;
    }

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
RenderingEntity MeshTexture::get_rid() const {
    return entt::null;
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

void MeshTexture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    if (not mesh || not base_texture) {
        return;
    }
    Transform2D xform;
    xform.set_origin(p_pos);
    if (p_transpose) {
        SWAP(xform.elements[0][1], xform.elements[1][0]);
        SWAP(xform.elements[0][0], xform.elements[1][1]);
    }
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
}
void MeshTexture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {
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
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
}
void MeshTexture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

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
    RenderingEntity normal_rid = p_normal_map ? p_normal_map->get_rid() : entt::null;
    RenderingServer::get_singleton()->canvas_item_add_mesh(p_canvas_item, mesh->get_rid(), xform, p_modulate, base_texture->get_rid(), normal_rid);
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
    SE_BIND_METHOD(MeshTexture,set_mesh);
    SE_BIND_METHOD(MeshTexture,get_mesh);
    SE_BIND_METHOD(MeshTexture,set_image_size);
    SE_BIND_METHOD(MeshTexture,get_image_size);
    SE_BIND_METHOD(MeshTexture,set_base_texture);
    SE_BIND_METHOD(MeshTexture,get_base_texture);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "mesh", PropertyHint::ResourceType, "Mesh"), "set_mesh", "get_mesh");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "base_texture", PropertyHint::ResourceType, "Texture"), "set_base_texture", "get_base_texture");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "image_size", PropertyHint::Range, "0,16384,1"), "set_image_size", "get_image_size");
}

MeshTexture::MeshTexture() {
}

MeshTexture::~MeshTexture() {
}
//////////////////////////////////////////

int LargeTexture::get_width() const {

    return size.width;
}
int LargeTexture::get_height() const {

    return size.height;
}
RenderingEntity LargeTexture::get_rid() const {

    return entt::null;
}

bool LargeTexture::has_alpha() const {

    for (size_t i = 0; i < pieces.size(); i++) {
        if (pieces[i].texture->has_alpha())
            return true;
    }

    return false;
}

void LargeTexture::set_flags(uint32_t p_flags) {

    for (size_t i = 0; i < pieces.size(); i++) {
        pieces[i].texture->set_flags(p_flags);
    }
}

uint32_t LargeTexture::get_flags() const {

    if (!pieces.empty())
        return pieces[0].texture->get_flags();

    return 0;
}

int LargeTexture::add_piece(const Point2 &p_offset, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND_V(not p_texture, -1);
    Piece p;
    p.offset = p_offset;
    p.texture = p_texture;
    pieces.push_back(p);

    return pieces.size() - 1;
}

void LargeTexture::set_piece_offset(int p_idx, const Point2 &p_offset) {

    ERR_FAIL_INDEX(p_idx, pieces.size());
    pieces[p_idx].offset = p_offset;
}

void LargeTexture::set_piece_texture(int p_idx, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this);
    ERR_FAIL_COND(not p_texture);
    ERR_FAIL_INDEX(p_idx, pieces.size());
    pieces[p_idx].texture = p_texture;
}

void LargeTexture::set_size(const Size2 &p_size) {

    size = p_size;
}
void LargeTexture::clear() {

    pieces.clear();
    size = Size2i();
}

Array LargeTexture::_get_data() const {

    Array arr;
    for (size_t i = 0; i < pieces.size(); i++) {
        arr.push_back(pieces[i].offset);
        arr.push_back(pieces[i].texture);
    }
    arr.push_back(Size2(size));
    return arr;
}
void LargeTexture::_set_data(const Array &p_array) {

    ERR_FAIL_COND(p_array.empty());
    ERR_FAIL_COND(!(p_array.size() & 1));
    clear();
    for (int i = 0; i < p_array.size() - 1; i += 2) {
        add_piece(p_array[i].as<Vector2>(), refFromVariant<Texture>(p_array[i + 1]));
    }
    size = p_array[p_array.size() - 1].as<Vector2>();
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

    Ref<Image> img(make_ref_counted<Image>(this->get_width(), this->get_height(), false, ImageData::FORMAT_RGBA8));
    for (size_t i = 0; i < pieces.size(); i++) {

        Ref<Image> src_img = pieces[i].texture->get_data();
        img->blit_rect(src_img, Rect2(0, 0, src_img->get_width(), src_img->get_height()), pieces[i].offset);
    }

    return img;
}

void LargeTexture::_bind_methods() {

    SE_BIND_METHOD(LargeTexture,add_piece);
    SE_BIND_METHOD(LargeTexture,set_piece_offset);
    SE_BIND_METHOD(LargeTexture,set_piece_texture);
    SE_BIND_METHOD(LargeTexture,set_size);
    SE_BIND_METHOD(LargeTexture,clear);

    SE_BIND_METHOD(LargeTexture,get_piece_count);
    SE_BIND_METHOD(LargeTexture,get_piece_offset);
    SE_BIND_METHOD(LargeTexture,get_piece_texture);

    SE_BIND_METHOD(LargeTexture,_set_data);
    SE_BIND_METHOD(LargeTexture,_get_data);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "_data", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
}

void LargeTexture::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        pieces[i].texture->draw(p_canvas_item, pieces[i].offset + p_pos, p_modulate, p_transpose, p_normal_map);
    }
}

void LargeTexture::draw_rect(RenderingEntity p_canvas_item, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map) const {

    //tiling not supported for this
    if (size.x == 0 || size.y == 0)
        return;

    Size2 scale = p_rect.size / size;

    for (int i = 0; i < pieces.size(); i++) {

        // TODO
        pieces[i].texture->draw_rect(p_canvas_item, Rect2(pieces[i].offset * scale + p_rect.position, pieces[i].texture->get_size() * scale), false, p_modulate, p_transpose, p_normal_map);
    }
}
void LargeTexture::draw_rect_region(RenderingEntity p_canvas_item, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, const Ref<Texture> &p_normal_map, bool p_clip_uv) const {

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
        RenderingServer::get_singleton()->texture_set_flags(cubemap, flags);
}

uint32_t CubeMap::get_flags() const {

    return flags;
}

void CubeMap::set_side(Side p_side, const Ref<Image> &p_image) {

    ERR_FAIL_COND(not p_image);
    ERR_FAIL_COND(p_image->is_empty());
    ERR_FAIL_INDEX(p_side, 6);

    if (!_is_valid()) {
        format = p_image->get_format();
        w = p_image->get_width();
        h = p_image->get_height();
        RenderingServer::get_singleton()->texture_allocate(cubemap, w, h, 0, p_image->get_format(), RS::TEXTURE_TYPE_CUBEMAP, flags);
    }

    RenderingServer::get_singleton()->texture_set_data(cubemap, p_image, RS::CubeMapSide(p_side));
    valid[p_side] = true;
}

Ref<Image> CubeMap::get_side(Side p_side) const {

    ERR_FAIL_INDEX_V(p_side, 6, Ref<Image>());
    if (!valid[p_side])
        return Ref<Image>();
    return RenderingServer::get_singleton()->texture_get_data(cubemap, RS::CubeMapSide(p_side));
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

RenderingEntity CubeMap::get_rid() const {

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

void CubeMap::set_path(StringView p_path, bool p_take_over) {

    if (cubemap!=entt::null) {
        RenderingServer::get_singleton()->texture_set_path(cubemap, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

bool CubeMap::_set(const StringName &p_name, const Variant &p_value) {

    if (p_name == "side/left") {
        set_side(SIDE_LEFT, refFromVariant<Image>(p_value));
    } else if (p_name == "side/right") {
        set_side(SIDE_RIGHT, refFromVariant<Image>(p_value));
    } else if (p_name == "side/bottom") {
        set_side(SIDE_BOTTOM, refFromVariant<Image>(p_value));
    } else if (p_name == "side/top") {
        set_side(SIDE_TOP, refFromVariant<Image>(p_value));
    } else if (p_name == "side/front") {
        set_side(SIDE_FRONT, refFromVariant<Image>(p_value));
    } else if (p_name == "side/back") {
        set_side(SIDE_BACK, refFromVariant<Image>(p_value));
    } else if (p_name == "storage") {
        storage = Storage(p_value.as<int>());
    } else if (p_name == "lossy_quality") {
        lossy_storage_quality = p_value.as<float>();
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

void CubeMap::_get_property_list(Vector<PropertyInfo> *p_list) const {

    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/left", PropertyHint::ResourceType, "Image"));
    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/right", PropertyHint::ResourceType, "Image"));
    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/bottom", PropertyHint::ResourceType, "Image"));
    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/top", PropertyHint::ResourceType, "Image"));
    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/front", PropertyHint::ResourceType, "Image"));
    p_list->emplace_back(PropertyInfo(VariantType::OBJECT, "side/back", PropertyHint::ResourceType, "Image"));
}

void CubeMap::_bind_methods() {

    SE_BIND_METHOD(CubeMap,get_width);
    SE_BIND_METHOD(CubeMap,get_height);
    SE_BIND_METHOD(CubeMap,set_flags);
    SE_BIND_METHOD(CubeMap,get_flags);
    SE_BIND_METHOD(CubeMap,set_side);
    SE_BIND_METHOD(CubeMap,get_side);
    SE_BIND_METHOD(CubeMap,set_storage);
    SE_BIND_METHOD(CubeMap,get_storage);
    SE_BIND_METHOD(CubeMap,set_lossy_storage_quality);
    SE_BIND_METHOD(CubeMap,get_lossy_storage_quality);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flags", PropertyHint::Flags, "Mipmaps,Repeat,Filter"), "set_flags", "get_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "storage_mode", PropertyHint::Enum, "Raw,Lossy Compressed,Lossless Compressed"), "set_storage", "get_storage");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "lossy_storage_quality"), "set_lossy_storage_quality", "get_lossy_storage_quality");

    BIND_ENUM_CONSTANT(STORAGE_RAW);
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSY);
    BIND_ENUM_CONSTANT(STORAGE_COMPRESS_LOSSLESS);

    BIND_ENUM_CONSTANT(SIDE_LEFT);
    BIND_ENUM_CONSTANT(SIDE_RIGHT);
    BIND_ENUM_CONSTANT(SIDE_BOTTOM);
    BIND_ENUM_CONSTANT(SIDE_TOP);
    BIND_ENUM_CONSTANT(SIDE_FRONT);
    BIND_ENUM_CONSTANT(SIDE_BACK);

    BIND_ENUM_CONSTANT(FLAG_MIPMAPS);
    BIND_ENUM_CONSTANT(FLAG_REPEAT);
    BIND_ENUM_CONSTANT(FLAG_FILTER);
    BIND_ENUM_CONSTANT(FLAGS_DEFAULT);
}

CubeMap::CubeMap() {

    w = h = 0;
    flags = FLAGS_DEFAULT;
    for (int i = 0; i < 6; i++)
        valid[i] = false;
    cubemap = RenderingServer::get_singleton()->texture_create();
    storage = STORAGE_RAW;
    lossy_storage_quality = 0.7f;
    format = ImageData::FORMAT_BPTC_RGBA;
}

CubeMap::~CubeMap() {

    RenderingServer::get_singleton()->free_rid(cubemap);
}

/*  BIND_ENUM(CubeMapSize);
    BIND_ENUM_CONSTANT( FLAG_CUBEMAP )
    BIND_ENUM_CONSTANT( CUBEMAP_LEFT )
    BIND_ENUM_CONSTANT( CUBEMAP_RIGHT )
    BIND_ENUM_CONSTANT( CUBEMAP_BOTTOM )
    BIND_ENUM_CONSTANT( CUBEMAP_TOP )
    BIND_ENUM_CONSTANT( CUBEMAP_FRONT )
    BIND_ENUM_CONSTANT( CUBEMAP_BACK )
*/

//setter and getter names for property serialization
#define COLOR_RAMP_GET_OFFSETS "get_offsets"
#define COLOR_RAMP_GET_COLORS "get_colors"
#define COLOR_RAMP_SET_OFFSETS "set_offsets"
#define COLOR_RAMP_SET_COLORS "set_colors"

GradientTexture::GradientTexture() {
    update_pending = false;
    width = 2048;

    texture = RenderingServer::get_singleton()->texture_create();
    _queue_update();
}

GradientTexture::~GradientTexture() {
    RenderingServer::get_singleton()->free_rid(texture);
}

void GradientTexture::_bind_methods() {

    SE_BIND_METHOD(GradientTexture,set_gradient);
    SE_BIND_METHOD(GradientTexture,get_gradient);

    SE_BIND_METHOD(GradientTexture,set_width);
    // The `get_width()` method is already exposed by the parent class Texture.

    SE_BIND_METHOD(GradientTexture,set_use_hdr);
    SE_BIND_METHOD(GradientTexture,is_using_hdr);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "gradient", PropertyHint::ResourceType, "Gradient"), "set_gradient", "get_gradient");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PropertyHint::Range, "1,4096,1,or_greater"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");
}

void GradientTexture::set_gradient(const Ref<Gradient>& p_gradient) {
    if (p_gradient == gradient)
        return;
    if (gradient) {
        gradient->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &GradientTexture::_update));
    }
    gradient = p_gradient;
    if (gradient) {
        gradient->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &GradientTexture::_update));
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
    call_deferred([this](){ this->_update();});
}

void GradientTexture::_update() {

    update_pending = false;

    if (not gradient)
        return;
    if (use_hdr) {
        // High dynamic range.
        Ref<Image> image(make_ref_counted<Image>(width, 1, false, ImageData::FORMAT_RGBAF));
        Gradient &g(*gradient);
        // `create()` isn't available for non-uint8_t data, so fill in the data manually.
        image->lock();
        for (int i = 0; i < width; i++) {
            float ofs = float(i) / (width - 1);
            image->set_pixel(i, 0, g.get_color_at_offset(ofs));
        }
        image->unlock();

        RenderingServer::get_singleton()->texture_allocate(
                texture, width, 1, 0, ImageData::FORMAT_RGBAF, RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER);
        RenderingServer::get_singleton()->texture_set_data(texture, image);
    } else {
        // Low dynamic range. "Overbright" colors will be clamped.
    PoolVector<uint8_t> data;
    data.resize(width * 4);
    {
        PoolVector<uint8_t>::Write wd8 = data.write();
        Gradient &g = *gradient;

        for (int i = 0; i < width; i++) {

            float ofs = float(i) / (width - 1);
            Color color = g.get_color_at_offset(ofs);

            wd8[i * 4 + 0] = uint8_t(CLAMP<float>(color.r * 255.0f, 0, 255));
            wd8[i * 4 + 1] = uint8_t(CLAMP<float>(color.g * 255.0f, 0, 255));
            wd8[i * 4 + 2] = uint8_t(CLAMP<float>(color.b * 255.0f, 0, 255));
            wd8[i * 4 + 3] = uint8_t(CLAMP<float>(color.a * 255.0f, 0, 255));
        }
    }

    Ref<Image> image(make_ref_counted<Image>(width, 1, false, ImageData::FORMAT_RGBA8, data));

        RenderingServer::get_singleton()->texture_allocate(
                texture, width, 1, 0, ImageData::FORMAT_RGBA8, RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER);
    RenderingServer::get_singleton()->texture_set_data(texture, image);
    }

    emit_changed();
}

void GradientTexture::set_width(int p_width) {

    width = p_width;
    _queue_update();
}
int GradientTexture::get_width() const {

    return width;
}

void GradientTexture::set_use_hdr(bool p_enabled) {
    if (p_enabled == use_hdr) {
        return;
    }

    use_hdr = p_enabled;
    _queue_update();
}

bool GradientTexture::is_using_hdr() const {
    return use_hdr;
}
Ref<Image> GradientTexture::get_data() const {
    return RenderingServer::get_singleton()->texture_get_data(texture);
}

//////////////////////////////////////

GradientTexture2D::GradientTexture2D() {
    texture = RID_PRIME(RenderingServer::get_singleton()->texture_create());
    _queue_update();
}

GradientTexture2D::~GradientTexture2D() {
    RenderingServer::get_singleton()->free_rid(texture);
}

void GradientTexture2D::set_gradient(const Ref<Gradient>& p_gradient) {
    if (gradient == p_gradient) {
        return;
    }
    if (gradient) {
        gradient->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &GradientTexture2D::_update));
    }
    gradient = p_gradient;
    if (gradient) {
        gradient->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &GradientTexture2D::_update));
    }
    _queue_update();
}

Ref<Gradient> GradientTexture2D::get_gradient() const {
    return gradient;
}

void GradientTexture2D::_queue_update() {
    if (update_pending) {
        return;
    }
    update_pending = true;
    call_deferred("_update");
}

void GradientTexture2D::_update() {
    update_pending = false;

    if (gradient) {
        return;
    }
    Ref<Image> image(make_ref_counted<Image>());

    const Vector<Gradient::Point> &points(gradient->get_points());

    if (points.size() <= 1) { // No need to interpolate.
        image->create(width, height, false, (use_hdr) ? ImageData::FORMAT_RGBAF : ImageData::FORMAT_RGBA8);
        image->fill((points.size() == 1) ? gradient->get_color(0) : Color(0, 0, 0, 1));
    } else {
        if (use_hdr) {
            image->create(width, height, false, ImageData::FORMAT_RGBAF);
            Gradient &g = *gradient;
            // `create()` isn't available for non-uint8_t data, so fill in the data manually.
            image->lock();
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    float ofs = _get_gradient_offset_at(x, y);
                    image->set_pixel(x, y, g.get_color_at_offset(ofs));
                }
            }
            image->unlock();
        } else {
            PoolVector<uint8_t> data;
            data.resize(width * height * 4);
            {
                uint8_t *wd8 = data.write().ptr();
                Gradient &g = *gradient;
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        float ofs = _get_gradient_offset_at(x, y);
                        const Color &c = g.get_color_at_offset(ofs);

                        wd8[(x + (y * width)) * 4 + 0] = uint8_t(CLAMP<float>(c.r * 255.0, 0, 255));
                        wd8[(x + (y * width)) * 4 + 1] = uint8_t(CLAMP<float>(c.g * 255.0, 0, 255));
                        wd8[(x + (y * width)) * 4 + 2] = uint8_t(CLAMP<float>(c.b * 255.0, 0, 255));
                        wd8[(x + (y * width)) * 4 + 3] = uint8_t(CLAMP<float>(c.a * 255.0, 0, 255));
                    }
                }
            }
            image->create(width, height, false, ImageData::FORMAT_RGBA8, data);
        }
    }
    RenderingServer::get_singleton()->texture_allocate(texture, width, height, 0, image->get_format(), RS::TEXTURE_TYPE_2D, RS::TEXTURE_FLAG_FILTER);
    RenderingServer::get_singleton()->texture_set_data(texture, image);

    emit_changed();
}

float GradientTexture2D::_get_gradient_offset_at(int x, int y) const {
    if (fill_to == fill_from) {
        return 0;
    }
    float ofs = 0;
    Vector2 pos;
    if (width > 1) {
        pos.x = static_cast<float>(x) / (width - 1);
    }
    if (height > 1) {
        pos.y = static_cast<float>(y) / (height - 1);
    }
    if (fill == Fill::FILL_LINEAR) {
        Vector2 segment[2];
        segment[0] = fill_from;
        segment[1] = fill_to;
        Vector2 closest = Geometry::get_closest_point_to_segment_uncapped_2d(pos, &segment[0]);
        ofs = (closest - fill_from).length() / (fill_to - fill_from).length();
        if ((closest - fill_from).dot(fill_to - fill_from) < 0) {
            ofs *= -1;
        }
    } else if (fill == Fill::FILL_RADIAL) {
        ofs = (pos - fill_from).length() / (fill_to - fill_from).length();
    }
    if (repeat == Repeat::REPEAT_NONE) {
        ofs = CLAMP<float>(ofs, 0.0, 1.0);
    } else if (repeat == Repeat::REPEAT) {
        ofs = Math::fmod(ofs, 1.0f);
        if (ofs < 0) {
            ofs = 1 + ofs;
        }
    } else if (repeat == Repeat::REPEAT_MIRROR) {
        ofs = Math::abs(ofs);
        ofs = Math::fmod(ofs, 2.0f);
        if (ofs > 1.0) {
            ofs = 2.0 - ofs;
        }
    }
    return ofs;
}

void GradientTexture2D::set_width(int p_width) {
    width = p_width;
    _queue_update();
}

int GradientTexture2D::get_width() const {
    return width;
}

void GradientTexture2D::set_height(int p_height) {
    height = p_height;
    _queue_update();
}

int GradientTexture2D::get_height() const {
    return height;
}

void GradientTexture2D::set_flags(uint32_t p_flags) {
    if (p_flags == flags) {
        return;
    }

    flags = p_flags;
    RenderingServer::get_singleton()->texture_set_flags(texture, flags);
    Object_change_notify(this,"flags");
    emit_changed();
}

uint32_t GradientTexture2D::get_flags() const {
    return flags;
}

void GradientTexture2D::set_use_hdr(bool p_enabled) {
    if (p_enabled == use_hdr) {
        return;
    }

    use_hdr = p_enabled;
    _queue_update();
}

bool GradientTexture2D::is_using_hdr() const {
    return use_hdr;
}

void GradientTexture2D::set_fill_from(Vector2 p_fill_from) {
    fill_from = p_fill_from;
    _queue_update();
}

Vector2 GradientTexture2D::get_fill_from() const {
    return fill_from;
}

void GradientTexture2D::set_fill_to(Vector2 p_fill_to) {
    fill_to = p_fill_to;
    _queue_update();
}

Vector2 GradientTexture2D::get_fill_to() const {
    return fill_to;
}

void GradientTexture2D::set_fill(Fill p_fill) {
    fill = p_fill;
    _queue_update();
}

GradientTexture2D::Fill GradientTexture2D::get_fill() const {
    return fill;
}

void GradientTexture2D::set_repeat(Repeat p_repeat) {
    repeat = p_repeat;
    _queue_update();
}

GradientTexture2D::Repeat GradientTexture2D::get_repeat() const {
    return repeat;
}

RenderingEntity GradientTexture2D::get_rid() const {
    return texture;
}

Ref<Image> GradientTexture2D::get_image() const {
    if (texture==entt::null) {
        return Ref<Image>();
    }
    return RenderingServer::get_singleton()->texture_get_data(texture);
}

void GradientTexture2D::_bind_methods() {
    SE_BIND_METHOD(GradientTexture2D,set_gradient);
    SE_BIND_METHOD(GradientTexture2D,get_gradient);

    SE_BIND_METHOD(GradientTexture2D,set_width);
    SE_BIND_METHOD(GradientTexture2D,set_height);

    SE_BIND_METHOD(GradientTexture2D,set_use_hdr);
    SE_BIND_METHOD(GradientTexture2D,is_using_hdr);

    SE_BIND_METHOD(GradientTexture2D,set_fill);
    SE_BIND_METHOD(GradientTexture2D,get_fill);
    SE_BIND_METHOD(GradientTexture2D,set_fill_from);
    SE_BIND_METHOD(GradientTexture2D,get_fill_from);
    SE_BIND_METHOD(GradientTexture2D,set_fill_to);
    SE_BIND_METHOD(GradientTexture2D,get_fill_to);

    SE_BIND_METHOD(GradientTexture2D,set_repeat);
    SE_BIND_METHOD(GradientTexture2D,get_repeat);

    SE_BIND_METHOD(GradientTexture2D,_update);
    SE_BIND_METHOD(GradientTexture2D,_queue_update);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "gradient", PropertyHint::ResourceType, "Gradient"), "set_gradient", "get_gradient");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "width", PropertyHint::Range, "1,2048,1,or_greater"), "set_width", "get_width");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "height", PropertyHint::Range, "1,2048,1,or_greater"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_hdr"), "set_use_hdr", "is_using_hdr");

    ADD_GROUP("Fill", "fill_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "fill_type", PropertyHint::Enum, "Linear,Radial"), "set_fill", "get_fill");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "fill_from"), "set_fill_from", "get_fill_from");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "fill_to"), "set_fill_to", "get_fill_to");

    ADD_GROUP("Repeat", "repeat_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "repeat_mode", PropertyHint::Enum, "No Repeat,Repeat,Mirror Repeat"), "set_repeat", "get_repeat");

    BIND_ENUM_CONSTANT(FILL_LINEAR);
    BIND_ENUM_CONSTANT(FILL_RADIAL);

    BIND_ENUM_CONSTANT(REPEAT_NONE);
    BIND_ENUM_CONSTANT(REPEAT);
    BIND_ENUM_CONSTANT(REPEAT_MIRROR);
}

//////////////////////////////////
void ProxyTexture::_bind_methods() {

    SE_BIND_METHOD(ProxyTexture,set_base);
    SE_BIND_METHOD(ProxyTexture,get_base);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "base", PropertyHint::ResourceType, "Texture"), "set_base", "get_base");
}

void ProxyTexture::set_base(const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this);
    base = p_texture;
    if (base) {
        RenderingServer::get_singleton()->texture_set_proxy(proxy, base->get_rid());
    } else {
        RenderingServer::get_singleton()->texture_set_proxy(proxy, entt::null);
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
RenderingEntity ProxyTexture::get_rid() const {

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

    proxy = RenderingServer::get_singleton()->texture_create();
}

ProxyTexture::~ProxyTexture() {

    RenderingServer::get_singleton()->free_rid(proxy);
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
    while (iter_max && !pause) {
        float frame_limit = limit + frames[current_frame].delay_sec;

        if (time > frame_limit) {
            current_frame++;
            if (current_frame >= frame_count) {
                if (oneshot) {
                    current_frame = frame_count - 1;
                } else {
                    current_frame = 0;
                }
            }
            time -= frame_limit;
            Object_change_notify(this,"current_frame");
        } else {
            break;
        }
        iter_max--;
    }

    if (frames[current_frame].texture) {
        RenderingServer::get_singleton()->texture_set_proxy(proxy, frames[current_frame].texture->get_rid());
    }
}

void AnimatedTexture::set_frames(int p_frames) {
    ERR_FAIL_COND(p_frames < 1 || p_frames > MAX_FRAMES);

    RWLockWrite r(rw_lock);

    frame_count = p_frames;
}
int AnimatedTexture::get_frames() const {
    return frame_count;
}

void AnimatedTexture::set_current_frame(int p_frame) {
    ERR_FAIL_COND(p_frame < 0 || p_frame >= frame_count);

    RWLockWrite r(rw_lock);

    current_frame = p_frame;
}
int AnimatedTexture::get_current_frame() const {
    return current_frame;
}

void AnimatedTexture::set_pause(bool p_pause) {
    RWLockWrite r(rw_lock);
    pause = p_pause;
}
bool AnimatedTexture::get_pause() const {
    return pause;
}

void AnimatedTexture::set_oneshot(bool p_oneshot) {
    RWLockWrite r(rw_lock);
    oneshot = p_oneshot;
}
bool AnimatedTexture::get_oneshot() const {
    return oneshot;
}

void AnimatedTexture::set_frame_texture(int p_frame, const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(p_texture.get() == this);
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
    ERR_FAIL_COND(p_fps < 0 || p_fps >= 1000);

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
RenderingEntity AnimatedTexture::get_rid() const {
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

    StringName prop = property.name;
    if (StringUtils::begins_with(prop,"frame/")) {
        FixedVector<StringView,3> parts;
        String::split_ref(parts,prop,'/');
        int frame = StringUtils::to_int(parts[1]);
        if (frame >= frame_count) {
            property.usage = 0;
        }
    }
}

void AnimatedTexture::_bind_methods() {
    SE_BIND_METHOD(AnimatedTexture,set_frames);
    SE_BIND_METHOD(AnimatedTexture,get_frames);

    SE_BIND_METHOD(AnimatedTexture,set_current_frame);
    SE_BIND_METHOD(AnimatedTexture,get_current_frame);

    SE_BIND_METHOD(AnimatedTexture,set_pause);
    SE_BIND_METHOD(AnimatedTexture,get_pause);

    SE_BIND_METHOD(AnimatedTexture,set_oneshot);
    SE_BIND_METHOD(AnimatedTexture,get_oneshot);

    SE_BIND_METHOD(AnimatedTexture,set_fps);
    SE_BIND_METHOD(AnimatedTexture,get_fps);

    SE_BIND_METHOD(AnimatedTexture,set_frame_texture);
    SE_BIND_METHOD(AnimatedTexture,get_frame_texture);

    SE_BIND_METHOD(AnimatedTexture,set_frame_delay);
    SE_BIND_METHOD(AnimatedTexture,get_frame_delay);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "frames", PropertyHint::Range, "1," + itos(MAX_FRAMES), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_frames", "get_frames");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "current_frame", PropertyHint::None, "", 0), "set_current_frame", "get_current_frame");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "pause"), "set_pause", "get_pause");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "oneshot"), "set_oneshot", "get_oneshot");

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "fps", PropertyHint::Range, "0,1024,0.1"), "set_fps", "get_fps");

    ADD_PROPERTY_ARRAY("Frames",MAX_FRAMES,"frame");
    for (int i = 0; i < MAX_FRAMES; i++) {
        ADD_PROPERTYI(PropertyInfo(VariantType::OBJECT, StringName("frame/" + itos(i) + "/texture"), PropertyHint::ResourceType, "Texture", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_texture", "get_frame_texture", i);
        ADD_PROPERTYI(PropertyInfo(VariantType::FLOAT, StringName("frame/" + itos(i) + "/delay_sec"), PropertyHint::Range, "0.0,16.0,0.01", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_delay", "get_frame_delay", i);
    }

    BIND_CONSTANT(MAX_FRAMES);
}

AnimatedTexture::AnimatedTexture() {
    proxy = RenderingServer::get_singleton()->texture_create();
    RenderingServer::get_singleton()->texture_set_force_redraw_if_visible(proxy, true);
    time = 0;
    frame_count = 1;
    fps = 4;
    prev_ticks = 0;
    current_frame = 0;
    pause = false;
    oneshot = false;
    RenderingServer::get_singleton()->connect("frame_pre_draw",callable_mp(this, &ClassName::_update_proxy));
}

AnimatedTexture::~AnimatedTexture() {
    RenderingServer::get_singleton()->free_rid(proxy);
}
///////////////////////////////

void TextureLayered::set_flags(uint32_t p_flags) {
    flags = p_flags;

    if (texture!=entt::null) {
        RenderingServer::get_singleton()->texture_set_flags(texture, flags);
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
    ERR_FAIL_COND(!p_data.has("width"));
    ERR_FAIL_COND(!p_data.has("height"));
    ERR_FAIL_COND(!p_data.has("depth"));
    ERR_FAIL_COND(!p_data.has("format"));
    ERR_FAIL_COND(!p_data.has("flags"));
    ERR_FAIL_COND(!p_data.has("layers"));
    int w = p_data["width"].as<int>();
    int h = p_data["height"].as<int>();
    int d = p_data["depth"].as<int>();
    Image::Format format = p_data["format"].as<Image::Format>();
    int flags = p_data["flags"].as<int>();
    Array layers = p_data["layers"].as<Array>();
    ERR_FAIL_COND(layers.size() != d);

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
    RenderingServer::get_singleton()->texture_allocate(texture, p_width, p_height, p_depth, p_format, is_3d ? RS::TEXTURE_TYPE_3D : RS::TEXTURE_TYPE_2D_ARRAY, p_flags);

    width = p_width;
    height = p_height;
    depth = p_depth;
    format = p_format;
    flags = p_flags;
}

void TextureLayered::set_layer_data(const Ref<Image> &p_image, int p_layer) {
    ERR_FAIL_COND(texture==entt::null);
    ERR_FAIL_COND(!p_image);
    ERR_FAIL_COND_MSG(
            p_image->get_width() > width || p_image->get_height() > height,
            FormatVE("Image size(%dx%d) is bigger than texture size (%dx%d).", p_image->get_width(), p_image->get_height(), width, height));
    RenderingServer::get_singleton()->texture_set_data(texture, p_image, p_layer);
}

Ref<Image> TextureLayered::get_layer_data(int p_layer) const {

    ERR_FAIL_COND_V(texture==entt::null, Ref<Image>());
    return RenderingServer::get_singleton()->texture_get_data(texture, p_layer);
}

void TextureLayered::set_data_partial(const Ref<Image> &p_image, int p_x_ofs, int p_y_ofs, int p_z, int p_mipmap) {
    ERR_FAIL_COND(texture==entt::null);
    ERR_FAIL_COND(!p_image);
    RenderingServer::get_singleton()->texture_set_data_partial(texture, p_image, 0, 0, p_image->get_width(), p_image->get_height(), p_x_ofs, p_y_ofs, p_mipmap, p_z);
}

RenderingEntity TextureLayered::get_rid() const {
    return texture;
}

void TextureLayered::set_path(StringView p_path, bool p_take_over) {
    if (texture!=entt::null) {
        RenderingServer::get_singleton()->texture_set_path(texture, p_path);
    }

    Resource::set_path(p_path, p_take_over);
}

void TextureLayered::_bind_methods() {
    SE_BIND_METHOD(TextureLayered,set_flags);
    SE_BIND_METHOD(TextureLayered,get_flags);

    SE_BIND_METHOD(TextureLayered,get_format);

    SE_BIND_METHOD(TextureLayered,get_width);
    SE_BIND_METHOD(TextureLayered,get_height);
    SE_BIND_METHOD(TextureLayered,get_depth);

    SE_BIND_METHOD(TextureLayered,set_layer_data);
    SE_BIND_METHOD(TextureLayered,get_layer_data);
    MethodBinder::bind_method(D_METHOD("set_data_partial", {"image", "x_offset", "y_offset", "layer", "mipmap"}), &TextureLayered::set_data_partial, {DEFVAL(0)});

    SE_BIND_METHOD(TextureLayered,_set_data);
    SE_BIND_METHOD(TextureLayered,_get_data);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "flags", PropertyHint::Flags, "Mipmaps,Repeat,Filter,Anisotropic Filter"), "set_flags", "get_flags");
    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "data", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR), "_set_data", "_get_data");

    BIND_ENUM_CONSTANT(FLAGS_DEFAULT_TEXTURE_3D);
    BIND_ENUM_CONSTANT(FLAGS_DEFAULT_TEXTURE_ARRAY);
    BIND_ENUM_CONSTANT(FLAG_MIPMAPS);
    BIND_ENUM_CONSTANT(FLAG_REPEAT);
    BIND_ENUM_CONSTANT(FLAG_FILTER);
    BIND_ENUM_CONSTANT(FLAG_ANISOTROPIC_FILTER);
}

TextureLayered::TextureLayered(bool p_3d) {
    is_3d = p_3d;
    flags = p_3d ? FLAGS_DEFAULT_TEXTURE_3D : FLAGS_DEFAULT_TEXTURE_ARRAY;
    format = ImageData::FORMAT_MAX;

    width = 0;
    height = 0;
    depth = 0;

    texture = RenderingServer::get_singleton()->texture_create();
}

TextureLayered::~TextureLayered() {
    if (texture!=entt::null) {
        RenderingServer::get_singleton()->free_rid(texture);
    }
}

void Texture3D::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("create", {"width", "height", "depth", "format", "flags"}), &Texture3D::create,{DEFVAL(FLAGS_DEFAULT_TEXTURE_3D)});
}

void TextureArray::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("create", {"width", "height", "depth", "format", "flags"}), &TextureArray::create,{DEFVAL(FLAGS_DEFAULT_TEXTURE_ARRAY)});
}

RES ResourceFormatLoaderTextureLayered::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

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
        ERR_FAIL_V_MSG(RES(), "Unrecognized layered texture extension.");
    }

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    ERR_FAIL_COND_V(!f, RES());

    uint8_t header[5] = { 0, 0, 0, 0, 0 };
    f->get_buffer(header, 4);

    if (header[0] == 'G' && header[1] == 'D' && header[2] == '3' && header[3] == 'T') {
        if (not tex3d) {
            f->close();
            memdelete(f);
            ERR_FAIL_COND_V(not tex3d, RES());
        }
    } else if (header[0] == 'G' && header[1] == 'D' && header[2] == 'A' && header[3] == 'T') {
        if (not texarr) {
            f->close();
            memdelete(f);
            ERR_FAIL_COND_V(not texarr, RES());
        }
    } else {
        f->close();
        memdelete(f);
        ERR_FAIL_V_MSG(RES(), "Unrecognized layered texture file format: " + String((const char *)header) + ".");
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

        if (compression == TextureLayered::COMPRESSION_LOSSLESS) {
            //look for a PNG file inside

            int mipmaps = f->get_32();
            Vector<Ref<Image> > mipmap_images;

            for (int i = 0; i < mipmaps; i++) {
                uint32_t size = f->get_32();

                Vector<uint8_t> pv;
                pv.resize(size);
                f->get_buffer(pv.data(), size);

                Ref<Image> img = Image::png_unpacker(pv);

                if (not img || img->is_empty() || format != img->get_format()) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                    }
                    f->close();
                    memdelete(f);
                    ERR_FAIL_V(RES());
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
                if (image->is_empty()) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                    }
                    f->close();
                    memdelete(f);
                    ERR_FAIL_V(RES());
                }
            }

        } else {

            //look for regular format
            bool mipmaps = (flags & Texture::FLAG_MIPMAPS);
            uint64_t total_size = Image::get_image_data_size(tw, th, format, mipmaps);

            PoolVector<uint8_t> img_data;
            img_data.resize(total_size);

            {
                PoolVector<uint8_t>::Write w = img_data.write();
                uint64_t bytes = f->get_buffer(w.ptr(), total_size);
                if (bytes != total_size) {
                    if (r_error) {
                        *r_error = ERR_FILE_CORRUPT;
                    }
                    f->close();
                    memdelete(f);
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

void ResourceFormatLoaderTextureLayered::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back(("tex3d"));
    p_extensions.push_back(("texarr"));
}
bool ResourceFormatLoaderTextureLayered::handles_type(StringView p_type) const {
    return p_type == StringView("Texture3D") || p_type == StringView("TextureArray");
}
String ResourceFormatLoaderTextureLayered::get_resource_type(StringView p_path) const {

    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "tex3d")
        return ("Texture3D");
    if (StringUtils::to_lower(PathUtils::get_extension(p_path)) == "texarr")
        return ("TextureArray");
    return {};
}



void ExternalTexture::_bind_methods() {
    SE_BIND_METHOD(ExternalTexture,set_size);
    SE_BIND_METHOD(ExternalTexture,get_external_texture_id);

    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "size"), "set_size", "get_size");
}

uint32_t ExternalTexture::get_external_texture_id() {
    return RenderingServer::get_singleton()->texture_get_texid(texture);
}

void ExternalTexture::set_size(const Size2 &p_size) {

    if (p_size.width > 0 && p_size.height > 0) {
        size = p_size;
        RenderingServer::get_singleton()->texture_set_size_override(texture, size.width, size.height, 0);
    }
}

int ExternalTexture::get_width() const {
    return size.width;
}

int ExternalTexture::get_height() const {
    return size.height;
}

Size2 ExternalTexture::get_size() const {
    return size;
}

RenderingEntity ExternalTexture::get_rid() const {
    return texture;
}

bool ExternalTexture::has_alpha() const {
    return true;
}

void ExternalTexture::set_flags(uint32_t p_flags) {
    // not supported
}

uint32_t ExternalTexture::get_flags() const {
    return Texture::FLAG_VIDEO_SURFACE;
}

ExternalTexture::ExternalTexture() {
    size = Size2(1.0, 1.0);
    texture = RenderingServer::get_singleton()->texture_create();

    RenderingServer::get_singleton()->texture_allocate(texture, size.width, size.height, 0, ImageData::FORMAT_RGBA8, RS::TEXTURE_TYPE_EXTERNAL, Texture::FLAG_VIDEO_SURFACE);
    Object_change_notify(this);
    emit_changed();
}

ExternalTexture::~ExternalTexture() {
    RenderingServer::get_singleton()->free_rid(texture);
}
