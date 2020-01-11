/*************************************************************************/
/*  dynamic_font.cpp                                                     */
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

#ifdef FREETYPE_ENABLED
#include "dynamic_font.h"
#include "font_serializers.h"

#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/method_bind.h"
#include "core/string_utils.inl"
#include "servers/visual_server.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include FT_STROKER_H

#include <cstdint>

IMPL_GDCLASS(DynamicFontData)
IMPL_GDCLASS(DynamicFontAtSize)
IMPL_GDCLASS(DynamicFont)

VARIANT_ENUM_CAST(DynamicFontData::Hinting);
VARIANT_ENUM_CAST(DynamicFont::SpacingType);

static unsigned long _ft_stream_io(FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count);
static void _ft_stream_close(FT_Stream stream);

struct DynamicFontAtSize::ImplData
{
    FT_Library library; /* handle to library     */
    FT_Face face; /* handle to face object */
    FT_StreamRec stream;
    DynamicFontAtSize::Character _bitmap_to_character(DynamicFontAtSize *fa, FT_Bitmap bitmap, int yofs, int xofs, float advance) {
        int w = bitmap.width;
        int h = bitmap.rows;

        int mw = w + fa->rect_margin * 2;
        int mh = h + fa->rect_margin * 2;

        ERR_FAIL_COND_V(mw > 4096, DynamicFontAtSize::Character::not_found())
            ERR_FAIL_COND_V(mh > 4096, DynamicFontAtSize::Character::not_found())

            int color_size = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? 4 : 2;
        Image::Format require_format = color_size == 4 ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8;

        DynamicFontAtSize::TexturePosition tex_pos = fa->_find_texture_pos_for_glyph(color_size, require_format, mw, mh);
        ERR_FAIL_COND_V(tex_pos.index < 0, Character::not_found())

            //fit character in char texture

            CharTexture &tex = fa->textures[tex_pos.index];

        {
            PoolVector<uint8_t>::Write wr = tex.imgdata.write();

            for (int i = 0; i < h; i++) {
                for (int j = 0; j < w; j++) {

                    int ofs = ((i + tex_pos.y + fa->rect_margin) * tex.texture_size + j + tex_pos.x + fa->rect_margin) * color_size;
                    ERR_FAIL_COND_V(ofs >= tex.imgdata.size(), Character::not_found())
                        switch (bitmap.pixel_mode) {
                        case FT_PIXEL_MODE_MONO: {
                            int byte = i * bitmap.pitch + (j >> 3);
                            int bit = 1 << (7 - (j % 8));
                            wr[ofs + 0] = 255; //grayscale as 1
                            wr[ofs + 1] = (bitmap.buffer[byte] & bit) ? 255 : 0;
                        } break;
                        case FT_PIXEL_MODE_GRAY:
                            wr[ofs + 0] = 255; //grayscale as 1
                            wr[ofs + 1] = bitmap.buffer[i * bitmap.pitch + j];
                            break;
                        case FT_PIXEL_MODE_BGRA: {
                            int ofs_color = i * bitmap.pitch + (j << 2);
                            wr[ofs + 2] = bitmap.buffer[ofs_color + 0];
                            wr[ofs + 1] = bitmap.buffer[ofs_color + 1];
                            wr[ofs + 0] = bitmap.buffer[ofs_color + 2];
                            wr[ofs + 3] = bitmap.buffer[ofs_color + 3];
                        } break;
                            // TODO: FT_PIXEL_MODE_LCD
                        default:
                            ERR_FAIL_V_MSG(Character::not_found(), "Font uses unsupported pixel format: " + itos(bitmap.pixel_mode) + ".");
                            break;
                        }
                }
            }
        }

        //blit to image and texture
        {

            Ref<Image> img(make_ref_counted<Image>(tex.texture_size, tex.texture_size, 0, require_format, tex.imgdata));

            if (not tex.texture) {
                tex.texture = make_ref_counted<ImageTexture>();
                tex.texture->create_from_image(img, Texture::FLAG_VIDEO_SURFACE | fa->texture_flags);
            }
            else {
                tex.texture->set_data(img); //update
            }
        }

        // update height array

        for (int k = tex_pos.x; k < tex_pos.x + mw; k++) {
            tex.offsets[k] = tex_pos.y + mh;
        }

        Character chr;
        chr.h_align = xofs * fa->scale_color_font / fa->oversampling;
        chr.v_align = fa->ascent - (yofs * fa->scale_color_font / fa->oversampling); // + ascent - descent;
        chr.advance = advance * fa->scale_color_font / fa->oversampling;
        chr.texture_idx = tex_pos.index;
        chr.found = true;

        chr.rect_uv = Rect2(tex_pos.x + fa->rect_margin, tex_pos.y + fa->rect_margin, w, h);
        chr.rect = chr.rect_uv;
        chr.rect.position /= fa->oversampling;
        chr.rect.size = chr.rect.size * fa->scale_color_font / fa->oversampling;
        return chr;
    }
};

bool DynamicFontData::CacheID::operator<(CacheID right) const {
    return key < right.key;
}

Ref<DynamicFontAtSize> DynamicFontData::_get_dynamic_font_at_size(CacheID p_cache_id) {

    if (size_cache.contains(p_cache_id)) {
        return Ref<DynamicFontAtSize>(size_cache[p_cache_id]);
    }

    Ref<DynamicFontAtSize> dfas(make_ref_counted<DynamicFontAtSize>());

    dfas->font = Ref<DynamicFontData>(this);

    size_cache[p_cache_id] = dfas.get();
    dfas->id = p_cache_id;
    dfas->_load();

    return dfas;
}

void DynamicFontData::set_font_ptr(const uint8_t *p_font_mem, int p_font_mem_size) {

    font_mem = p_font_mem;
    font_mem_size = p_font_mem_size;
}

void DynamicFontData::set_font_path(se_string_view p_path) {

    font_path = p_path;
}

const se_string &DynamicFontData::get_font_path() const {
    return font_path;
}

void DynamicFontData::set_force_autohinter(bool p_force) {

    force_autohinter = p_force;
}

void DynamicFontData::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("set_antialiased", {"antialiased"}), &DynamicFontData::set_antialiased);
    MethodBinder::bind_method(D_METHOD("is_antialiased"), &DynamicFontData::is_antialiased);
    MethodBinder::bind_method(D_METHOD("set_font_path", {"path"}), &DynamicFontData::set_font_path);
    MethodBinder::bind_method(D_METHOD("get_font_path"), &DynamicFontData::get_font_path);
    MethodBinder::bind_method(D_METHOD("set_hinting", {"mode"}), &DynamicFontData::set_hinting);
    MethodBinder::bind_method(D_METHOD("get_hinting"), &DynamicFontData::get_hinting);

    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "antialiased"), "set_antialiased", "is_antialiased");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "hinting", PROPERTY_HINT_ENUM, "None,Light,Normal"), "set_hinting", "get_hinting");

    BIND_ENUM_CONSTANT(HINTING_NONE)
    BIND_ENUM_CONSTANT(HINTING_LIGHT)
    BIND_ENUM_CONSTANT(HINTING_NORMAL)

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "font_path", PROPERTY_HINT_FILE, "*.ttf,*.otf"), "set_font_path", "get_font_path");
}

DynamicFontData::DynamicFontData() {

    antialiased = true;
    force_autohinter = false;
    hinting = DynamicFontData::HINTING_NORMAL;
    font_mem = nullptr;
    font_mem_size = 0;
}

DynamicFontData::~DynamicFontData() {
}

////////////////////
HashMap<se_string, PODVector<uint8_t> > DynamicFontAtSize::_fontdata;

Error DynamicFontAtSize::_load() {

    int error = FT_Init_FreeType(&m_impl->library);

    ERR_FAIL_COND_V_MSG(error != 0, ERR_CANT_CREATE, "Error initializing FreeType.")

    // FT_OPEN_STREAM is extremely slow only on Android.
    if (OS::get_singleton()->get_name() == "Android" && font->font_mem == nullptr && !font->font_path.empty()) {
        // cache font only once for each font->font_path
        if (_fontdata.contains(font->font_path)) {

            font->set_font_ptr(_fontdata[font->font_path].data(), _fontdata[font->font_path].size());

        } else {

            FileAccess *f = FileAccess::open(font->font_path, FileAccess::READ);
            ERR_FAIL_COND_V(!f, ERR_CANT_OPEN)

            size_t len = f->get_len();
            PODVector<uint8_t> &fontdata = _fontdata[font->font_path];
            fontdata.clear();
            fontdata.resize(len);
            fontdata.shrink_to_fit();
            f->get_buffer(fontdata.data(), len);
            font->set_font_ptr(fontdata.data(), len);
            f->close();
        }
    }

    if (font->font_mem == nullptr && !font->font_path.empty()) {

        FileAccess *f = FileAccess::open(font->font_path, FileAccess::READ);
        ERR_FAIL_COND_V(!f, ERR_CANT_OPEN)

        memset(&m_impl->stream, 0, sizeof(FT_StreamRec));
        m_impl->stream.base = nullptr;
        m_impl->stream.size = f->get_len();
        m_impl->stream.pos = 0;
        m_impl->stream.descriptor.pointer = f;
        m_impl->stream.read = _ft_stream_io;
        m_impl->stream.close = _ft_stream_close;

        FT_Open_Args fargs;
        memset(&fargs, 0, sizeof(FT_Open_Args));
        fargs.flags = FT_OPEN_STREAM;
        fargs.stream = &m_impl->stream;
        error = FT_Open_Face(m_impl->library, &fargs, 0, &m_impl->face);
    } else if (font->font_mem) {

        memset(&m_impl->stream, 0, sizeof(FT_StreamRec));
        m_impl->stream.base = (unsigned char *)font->font_mem;
        m_impl->stream.size = font->font_mem_size;
        m_impl->stream.pos = 0;

        FT_Open_Args fargs;
        memset(&fargs, 0, sizeof(FT_Open_Args));
        fargs.memory_base = (unsigned char *)font->font_mem;
        fargs.memory_size = font->font_mem_size;
        fargs.flags = FT_OPEN_MEMORY;
        fargs.stream = &m_impl->stream;
        error = FT_Open_Face(m_impl->library, &fargs, 0, &m_impl->face);

    } else {
        ERR_FAIL_V_MSG(ERR_UNCONFIGURED, "DynamicFont uninitialized.");
    }

    //error = FT_New_Face( library, src_path.utf8().get_data(),0,&face );

    if (error == FT_Err_Unknown_File_Format) {
        ERR_PRINT("Unknown font format.")
        FT_Done_FreeType(m_impl->library);
    } else if (error) {

        ERR_PRINT("Error loading font.")
        FT_Done_FreeType(m_impl->library);
    }

    ERR_FAIL_COND_V(error, ERR_FILE_CANT_OPEN)

    if (FT_HAS_COLOR(m_impl->face) && m_impl->face->num_fixed_sizes > 0) {
        int best_match = 0;
        int diff = ABS(id.size - ((int64_t)m_impl->face->available_sizes[0].width));
        scale_color_font = float(id.size) / m_impl->face->available_sizes[0].width;
        for (int i = 1; i < m_impl->face->num_fixed_sizes; i++) {
            int ndiff = ABS(id.size - ((int64_t)m_impl->face->available_sizes[i].width));
            if (ndiff < diff) {
                best_match = i;
                diff = ndiff;
                scale_color_font = float(id.size) / m_impl->face->available_sizes[i].width;
            }
        }
        FT_Select_Size(m_impl->face, best_match);
    } else {
        FT_Set_Pixel_Sizes(m_impl->face, 0, id.size * oversampling);
    }

    ascent = (m_impl->face->size->metrics.ascender / 64.0f) / oversampling * scale_color_font;
    descent = (-m_impl->face->size->metrics.descender / 64.0f) / oversampling * scale_color_font;
    linegap = 0;
    texture_flags = 0;
    if (id.mipmaps)
        texture_flags |= Texture::FLAG_MIPMAPS;
    if (id.filter)
        texture_flags |= Texture::FLAG_FILTER;

    valid = true;
    return OK;
}

float DynamicFontAtSize::font_oversampling = 1.0;

float DynamicFontAtSize::get_height() const {

    return ascent + descent;
}

float DynamicFontAtSize::get_ascent() const {

    return ascent;
}
float DynamicFontAtSize::get_descent() const {

    return descent;
}

const Pair<const DynamicFontAtSize::Character *, DynamicFontAtSize *> DynamicFontAtSize::_find_char_with_font(CharType p_char, const PODVector<Ref<DynamicFontAtSize> > &p_fallbacks) const {
    const Character *chr = char_map.getptr(p_char);
    ERR_FAIL_COND_V(!chr, (Pair<const Character *, DynamicFontAtSize *>(NULL, NULL)))

    if (!chr->found) {

        //not found, try in fallbacks
        for (int i = 0; i < p_fallbacks.size(); i++) {

            DynamicFontAtSize *fb = const_cast<DynamicFontAtSize *>(p_fallbacks[i].get());
            if (!fb->valid)
                continue;

            fb->_update_char(p_char);
            const Character *fallback_chr = fb->char_map.getptr(p_char);
            ERR_CONTINUE(!fallback_chr)

            if (!fallback_chr->found)
                continue;

            return Pair<const Character *, DynamicFontAtSize *>(fallback_chr, fb);
        }

        //not found, try 0xFFFD to display 'not found'.
        const_cast<DynamicFontAtSize *>(this)->_update_char(0xFFFD);
        chr = char_map.getptr(0xFFFD);
        ERR_FAIL_COND_V(!chr, (Pair<const Character *, DynamicFontAtSize *>(NULL, NULL)))
    }

    return Pair<const Character *, DynamicFontAtSize *>(chr, const_cast<DynamicFontAtSize *>(this));
}

Size2 DynamicFontAtSize::get_char_size(CharType p_char, CharType p_next, const PODVector<Ref<DynamicFontAtSize> > &p_fallbacks) const {

    if (!valid)
        return Size2(1, 1);
    const_cast<DynamicFontAtSize *>(this)->_update_char(p_char);

    Pair<const Character *, DynamicFontAtSize *> char_pair_with_font = _find_char_with_font(p_char, p_fallbacks);
    const Character *ch = char_pair_with_font.first;
    ERR_FAIL_COND_V(!ch, Size2())

    Size2 ret(0, get_height());

    if (ch->found) {
        ret.x = ch->advance;
    }

    return ret;
}

void DynamicFontAtSize::set_texture_flags(uint32_t p_flags) {

    texture_flags = p_flags;
    for (int i = 0; i < textures.size(); i++) {
        Ref<ImageTexture> &tex = textures[i].texture;
        if (tex)
            tex->set_flags(p_flags);
    }
}

float DynamicFontAtSize::draw_char(RID p_canvas_item, const Point2 &p_pos, CharType p_char, const Color &p_modulate, const PODVector<Ref<DynamicFontAtSize> > &p_fallbacks, bool p_advance_only) const {

    if (!valid)
        return 0;

    const_cast<DynamicFontAtSize *>(this)->_update_char(p_char);

    Pair<const Character *, DynamicFontAtSize *> char_pair_with_font = _find_char_with_font(p_char, p_fallbacks);
    const Character *ch = char_pair_with_font.first;
    DynamicFontAtSize *font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, 0.0)

    float advance = 0.0;

    if (ch->found) {
        ERR_FAIL_COND_V(ch->texture_idx < -1 || ch->texture_idx >= font->textures.size(), 0)

        if (!p_advance_only && ch->texture_idx != -1) {
            Point2 cpos = p_pos;
            cpos.x += ch->h_align;
            cpos.y -= font->get_ascent();
            cpos.y += ch->v_align;
            Color modulate = p_modulate;
            if (FT_HAS_COLOR(m_impl->face)) {
                modulate.r = modulate.g = modulate.b = 1.0;
            }
            RID texture = font->textures[ch->texture_idx].texture->get_rid();
            VisualServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, Rect2(cpos, ch->rect.size), texture, ch->rect_uv, modulate, false, RID(), false);
        }

        advance = ch->advance;
    }

    return advance;
}

static unsigned long _ft_stream_io(FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count) {

    FileAccess *f = (FileAccess *)stream->descriptor.pointer;

    if (f->get_position() != offset) {
        f->seek(offset);
    }

    if (count == 0)
        return 0;

    return f->get_buffer(buffer, count);
}
static void _ft_stream_close(FT_Stream stream) {

    FileAccess *f = (FileAccess *)stream->descriptor.pointer;
    f->close();
    memdelete(f);
}

DynamicFontAtSize::Character DynamicFontAtSize::Character::not_found() {
    Character ch;
    ch.texture_idx = -1;
    ch.advance = 0;
    ch.h_align = 0;
    ch.v_align = 0;
    ch.found = false;
    return ch;
}

DynamicFontAtSize::TexturePosition DynamicFontAtSize::_find_texture_pos_for_glyph(int p_color_size, Image::Format p_image_format, int p_width, int p_height) {
    TexturePosition ret;
    ret.index = -1;
    ret.x = 0;
    ret.y = 0;

    int mw = p_width;
    int mh = p_height;

    for (int i = 0; i < textures.size(); i++) {

        const CharTexture &ct = textures[i];

        if (ct.texture->get_format() != p_image_format)
            continue;

        if (mw > ct.texture_size || mh > ct.texture_size) //too big for this texture
            continue;

        ret.y = 0x7FFFFFFF;
        ret.x = 0;

        for (int j = 0; j < ct.texture_size - mw; j++) {

            int max_y = 0;

            for (int k = j; k < j + mw; k++) {

                int y = ct.offsets[k];
                if (y > max_y)
                    max_y = y;
            }

            if (max_y < ret.y) {
                ret.y = max_y;
                ret.x = j;
            }
        }

        if (ret.y == 0x7FFFFFFF || ret.y + mh > ct.texture_size)
            continue; //fail, could not fit it here

        ret.index = i;
        break;
    }

    if (ret.index == -1) {
        //could not find texture to fit, create one
        ret.x = 0;
        ret.y = 0;

        int texsize = MAX(id.size * oversampling * 8, 256);
        if (mw > texsize)
            texsize = mw; //special case, adapt to it?
        if (mh > texsize)
            texsize = mh; //special case, adapt to it?

        texsize = next_power_of_2(texsize);

        texsize = MIN(texsize, 4096);

        CharTexture tex;
        tex.texture_size = texsize;
        tex.imgdata.resize(texsize * texsize * p_color_size); //grayscale alpha

        {
            //zero texture
            PoolVector<uint8_t>::Write w = tex.imgdata.write();
            ERR_FAIL_COND_V(texsize * texsize * p_color_size > tex.imgdata.size(), ret)
            for (int i = 0; i < texsize * texsize * p_color_size; i++) {
                w[i] = 0;
            }
        }
        tex.offsets.resize(texsize,0); //zero offsets

        textures.push_back(tex);
        ret.index = textures.size() - 1;
    }

    return ret;
}



DynamicFontAtSize::Character DynamicFontAtSize::_make_outline_char(CharType p_char) {
    Character ret = Character::not_found();

    if (FT_Load_Char(m_impl->face, p_char.unicode(), FT_LOAD_NO_BITMAP | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0)) != 0)
        return ret;

    FT_Stroker stroker;
    if (FT_Stroker_New(m_impl->library, &stroker) != 0)
        return ret;

    FT_Stroker_Set(stroker, (int)(id.outline_size * oversampling * 64.0f), FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
    FT_Glyph glyph;
    FT_BitmapGlyph glyph_bitmap;

    if (FT_Get_Glyph(m_impl->face->glyph, &glyph) != 0)
        goto cleanup_stroker;
    if (FT_Glyph_Stroke(&glyph, stroker, 1) != 0)
        goto cleanup_glyph;
    if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, 1) != 0)
        goto cleanup_glyph;

    glyph_bitmap = (FT_BitmapGlyph)glyph;
    ret = m_impl->_bitmap_to_character(this,glyph_bitmap->bitmap, glyph_bitmap->top, glyph_bitmap->left, glyph->advance.x / 65536.0f);

cleanup_glyph:
    FT_Done_Glyph(glyph);
cleanup_stroker:
    FT_Stroker_Done(stroker);
    return ret;
}

void DynamicFontAtSize::_update_char(CharType p_char) {

    if (char_map.contains(p_char))
        return;

    _THREAD_SAFE_METHOD_

    Character character = Character::not_found();

    FT_GlyphSlot slot = m_impl->face->glyph;

    if (FT_Get_Char_Index(m_impl->face, p_char.unicode()) == 0) {
        char_map[p_char] = character;
        return;
    }

    int ft_hinting;

    switch (font->hinting) {
        case DynamicFontData::HINTING_NONE:
            ft_hinting = FT_LOAD_NO_HINTING;
            break;
        case DynamicFontData::HINTING_LIGHT:
            ft_hinting = FT_LOAD_TARGET_LIGHT;
            break;
        default:
            ft_hinting = FT_LOAD_TARGET_NORMAL;
            break;
    }

    int error = FT_Load_Char(m_impl->face, p_char.unicode(), FT_HAS_COLOR(m_impl->face) ? FT_LOAD_COLOR : FT_LOAD_DEFAULT | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0) | ft_hinting);
    if (error) {
        char_map[p_char] = character;
        return;
    }

    if (id.outline_size > 0) {
        character = _make_outline_char(p_char);
    } else {
        error = FT_Render_Glyph(m_impl->face->glyph, font->antialiased ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
        if (!error)
            character = m_impl->_bitmap_to_character(this,slot->bitmap, slot->bitmap_top, slot->bitmap_left, slot->advance.x / 64.0f);
    }

    char_map[p_char] = character;
}

void DynamicFontAtSize::update_oversampling() {
    if (oversampling == font_oversampling || !valid)
        return;

    FT_Done_FreeType(m_impl->library);
    textures.clear();
    char_map.clear();
    oversampling = font_oversampling;
    valid = false;
    _load();
}

DynamicFontAtSize::DynamicFontAtSize() {
    __thread__safe__.reset(new Mutex);

    valid = false;
    rect_margin = 1;
    ascent = 1;
    descent = 1;
    linegap = 1;
    texture_flags = 0;
    oversampling = font_oversampling;
    scale_color_font = 1;
    m_impl = new ImplData;
}

DynamicFontAtSize::~DynamicFontAtSize() {

    if (valid) {
        FT_Done_FreeType(m_impl->library);
    }
    font->size_cache.erase(id);
    font.unref();
    delete m_impl;
}

/////////////////////////

void DynamicFont::_reload_cache() {

    ERR_FAIL_COND(cache_id.size < 1)
    if (not data) {
        data_at_size.unref();
        outline_data_at_size.unref();
        fallbacks.clear();
        fallback_data_at_size.resize(0);
        fallback_outline_data_at_size.resize(0);
        return;
    }

    data_at_size = data->_get_dynamic_font_at_size(cache_id);
    if (outline_cache_id.outline_size > 0) {
        outline_data_at_size = data->_get_dynamic_font_at_size(outline_cache_id);
        fallback_outline_data_at_size.resize(fallback_data_at_size.size());
    } else {
        outline_data_at_size.unref();
        fallback_outline_data_at_size.resize(0);
    }

    for (int i = 0; i < fallbacks.size(); i++) {
        fallback_data_at_size[i] = fallbacks[i]->_get_dynamic_font_at_size(cache_id);
        if (outline_cache_id.outline_size > 0)
            fallback_outline_data_at_size[i] = fallbacks[i]->_get_dynamic_font_at_size(outline_cache_id);
    }

    emit_changed();
    Object_change_notify(this);
}

void DynamicFont::set_font_data(const Ref<DynamicFontData> &p_data) {

    data = p_data;
    _reload_cache();

    emit_changed();
    Object_change_notify(this);
}

Ref<DynamicFontData> DynamicFont::get_font_data() const {

    return data;
}

void DynamicFont::set_size(int p_size) {

    if (cache_id.size == p_size)
        return;
    cache_id.size = p_size;
    outline_cache_id.size = p_size;
    _reload_cache();
}

int DynamicFont::get_size() const {

    return cache_id.size;
}

void DynamicFont::set_outline_size(int p_size) {
    if (outline_cache_id.outline_size == p_size)
        return;
    ERR_FAIL_COND(p_size < 0 || p_size > UINT8_MAX)
    outline_cache_id.outline_size = p_size;
    _reload_cache();
}

int DynamicFont::get_outline_size() const {
    return outline_cache_id.outline_size;
}

void DynamicFont::set_outline_color(Color p_color) {
    if (p_color != outline_color) {
        outline_color = p_color;
        emit_changed();
        Object_change_notify(this);
    }
}

Color DynamicFont::get_outline_color() const {
    return outline_color;
}

bool DynamicFont::get_use_mipmaps() const {

    return cache_id.mipmaps;
}

void DynamicFont::set_use_mipmaps(bool p_enable) {

    if (cache_id.mipmaps == p_enable)
        return;
    cache_id.mipmaps = p_enable;
    outline_cache_id.mipmaps = p_enable;
    _reload_cache();
}

bool DynamicFont::get_use_filter() const {

    return cache_id.filter;
}

void DynamicFont::set_use_filter(bool p_enable) {

    if (cache_id.filter == p_enable)
        return;
    cache_id.filter = p_enable;
    outline_cache_id.filter = p_enable;
    _reload_cache();
}

bool DynamicFontData::is_antialiased() const {

    return antialiased;
}

void DynamicFontData::set_antialiased(bool p_antialiased) {

    if (antialiased == p_antialiased)
        return;
    antialiased = p_antialiased;
}

DynamicFontData::Hinting DynamicFontData::get_hinting() const {

    return hinting;
}

void DynamicFontData::set_hinting(Hinting p_hinting) {

    if (hinting == p_hinting)
        return;
    hinting = p_hinting;
}

int DynamicFont::get_spacing(int p_type) const {

    if (p_type == SPACING_TOP) {
        return spacing_top;
    } else if (p_type == SPACING_BOTTOM) {
        return spacing_bottom;
    } else if (p_type == SPACING_CHAR) {
        return spacing_char;
    } else if (p_type == SPACING_SPACE) {
        return spacing_space;
    }

    return 0;
}

void DynamicFont::set_spacing(int p_type, int p_value) {

    if (p_type == SPACING_TOP) {
        spacing_top = p_value;
    } else if (p_type == SPACING_BOTTOM) {
        spacing_bottom = p_value;
    } else if (p_type == SPACING_CHAR) {
        spacing_char = p_value;
    } else if (p_type == SPACING_SPACE) {
        spacing_space = p_value;
    }

    emit_changed();
    Object_change_notify(this);
}

float DynamicFont::get_height() const {

    if (not data_at_size)
        return 1;

    return data_at_size->get_height() + spacing_top + spacing_bottom;
}

float DynamicFont::get_ascent() const {

    if (not data_at_size)
        return 1;

    return data_at_size->get_ascent() + spacing_top;
}

float DynamicFont::get_descent() const {

    if (not data_at_size)
        return 1;

    return data_at_size->get_descent() + spacing_bottom;
}

Size2 DynamicFont::get_char_size(CharType p_char, CharType p_next) const {

    if (not data_at_size)
        return Size2(1, 1);

    Size2 ret = data_at_size->get_char_size(p_char, p_next, fallback_data_at_size);
    if (p_char == ' ')
        ret.width += spacing_space + spacing_char;
    else if (!p_next.isNull())
        ret.width += spacing_char;

    return ret;
}

bool DynamicFont::is_distance_field_hint() const {

    return false;
}

bool DynamicFont::has_outline() const {
    return outline_cache_id.outline_size > 0;
}

float DynamicFont::draw_char(RID p_canvas_item, const Point2 &p_pos, CharType p_char, CharType p_next, const Color &p_modulate, bool p_outline) const {
    const Ref<DynamicFontAtSize> &font_at_size = p_outline && outline_cache_id.outline_size > 0 ? outline_data_at_size : data_at_size;

    if (not font_at_size)
        return 0;

    const PODVector<Ref<DynamicFontAtSize> > &fallbacks = p_outline && outline_cache_id.outline_size > 0 ? fallback_outline_data_at_size : fallback_data_at_size;
    Color color = p_outline && outline_cache_id.outline_size > 0 ? p_modulate * outline_color : p_modulate;

    // If requested outline draw, but no outline is present, simply return advance without drawing anything
    bool advance_only = p_outline && outline_cache_id.outline_size == 0;
    return font_at_size->draw_char(p_canvas_item, p_pos, p_char, color, fallbacks, advance_only) + spacing_char;
}

void DynamicFont::set_fallback(int p_idx, const Ref<DynamicFontData> &p_data) {

    ERR_FAIL_COND(not p_data)
    ERR_FAIL_INDEX(p_idx, fallbacks.size())
    fallbacks[p_idx] = p_data;
    fallback_data_at_size[p_idx] = fallbacks[p_idx]->_get_dynamic_font_at_size(cache_id);
}

void DynamicFont::add_fallback(const Ref<DynamicFontData> &p_data) {

    ERR_FAIL_COND(not p_data)
    fallbacks.push_back(p_data);
    fallback_data_at_size.push_back(fallbacks[fallbacks.size() - 1]->_get_dynamic_font_at_size(cache_id)); //const..
    if (outline_cache_id.outline_size > 0)
        fallback_outline_data_at_size.push_back(fallbacks[fallbacks.size() - 1]->_get_dynamic_font_at_size(outline_cache_id));

    Object_change_notify(this);
    emit_changed();
    Object_change_notify(this);
}

int DynamicFont::get_fallback_count() const {
    return fallbacks.size();
}
Ref<DynamicFontData> DynamicFont::get_fallback(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, fallbacks.size(), Ref<DynamicFontData>())

    return fallbacks[p_idx];
}
void DynamicFont::remove_fallback(int p_idx) {

    ERR_FAIL_INDEX(p_idx, fallbacks.size())
    fallbacks.erase_at(p_idx);
    fallback_data_at_size.erase_at(p_idx);
    emit_changed();
    Object_change_notify(this);
}

bool DynamicFont::_set(const StringName &p_name, const Variant &p_value) {

    if (StringUtils::begins_with(p_name,"fallback/")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        Ref<DynamicFontData> fd = refFromRefPtr<DynamicFontData>(p_value);

        if (fd) {
            if (idx == fallbacks.size()) {
                add_fallback(fd);
                return true;
            } else if (idx >= 0 && idx < fallbacks.size()) {
                set_fallback(idx, fd);
                return true;
            } else {
                return false;
            }
        } else if (idx >= 0 && idx < fallbacks.size()) {
            remove_fallback(idx);
            return true;
        }
    }

    return false;
}

bool DynamicFont::_get(const StringName &p_name, Variant &r_ret) const {

    StringName str(p_name);
    if (StringUtils::begins_with(str,"fallback/")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(str,'/', 1));

        if (idx == fallbacks.size()) {
            r_ret = Ref<DynamicFontData>();
            return true;
        } else if (idx >= 0 && idx < fallbacks.size()) {
            r_ret = get_fallback(idx);
            return true;
        }
    }

    return false;
}
void DynamicFont::_get_property_list(ListPOD<PropertyInfo> *p_list) const {

    for (int i = 0; i < fallbacks.size(); i++) {
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("fallback/" + itos(i)), PROPERTY_HINT_RESOURCE_TYPE, "DynamicFontData"));
    }

    p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("fallback/" + itos(fallbacks.size())), PROPERTY_HINT_RESOURCE_TYPE, "DynamicFontData"));
}

void DynamicFont::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("set_font_data", {"data"}), &DynamicFont::set_font_data);
    MethodBinder::bind_method(D_METHOD("get_font_data"), &DynamicFont::get_font_data);

    MethodBinder::bind_method(D_METHOD("set_size", {"data"}), &DynamicFont::set_size);
    MethodBinder::bind_method(D_METHOD("get_size"), &DynamicFont::get_size);

    MethodBinder::bind_method(D_METHOD("set_outline_size", {"size"}), &DynamicFont::set_outline_size);
    MethodBinder::bind_method(D_METHOD("get_outline_size"), &DynamicFont::get_outline_size);

    MethodBinder::bind_method(D_METHOD("set_outline_color", {"color"}), &DynamicFont::set_outline_color);
    MethodBinder::bind_method(D_METHOD("get_outline_color"), &DynamicFont::get_outline_color);

    MethodBinder::bind_method(D_METHOD("set_use_mipmaps", {"enable"}), &DynamicFont::set_use_mipmaps);
    MethodBinder::bind_method(D_METHOD("get_use_mipmaps"), &DynamicFont::get_use_mipmaps);
    MethodBinder::bind_method(D_METHOD("set_use_filter", {"enable"}), &DynamicFont::set_use_filter);
    MethodBinder::bind_method(D_METHOD("get_use_filter"), &DynamicFont::get_use_filter);
    MethodBinder::bind_method(D_METHOD("set_spacing", {"type", "value"}), &DynamicFont::set_spacing);
    MethodBinder::bind_method(D_METHOD("get_spacing", {"type"}), &DynamicFont::get_spacing);

    MethodBinder::bind_method(D_METHOD("add_fallback", {"data"}), &DynamicFont::add_fallback);
    MethodBinder::bind_method(D_METHOD("set_fallback", {"idx", "data"}), &DynamicFont::set_fallback);
    MethodBinder::bind_method(D_METHOD("get_fallback", {"idx"}), &DynamicFont::get_fallback);
    MethodBinder::bind_method(D_METHOD("remove_fallback", {"idx"}), &DynamicFont::remove_fallback);
    MethodBinder::bind_method(D_METHOD("get_fallback_count"), &DynamicFont::get_fallback_count);

    ADD_GROUP("Settings", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "size", PROPERTY_HINT_RANGE, "1,255,1"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "outline_size", PROPERTY_HINT_RANGE, "0,255,1"), "set_outline_size", "get_outline_size");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "outline_color"), "set_outline_color", "get_outline_color");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_mipmaps"), "set_use_mipmaps", "get_use_mipmaps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "use_filter"), "set_use_filter", "get_use_filter");
    ADD_GROUP("Extra Spacing", "extra_spacing");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_top"), "set_spacing", "get_spacing", SPACING_TOP);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_bottom"), "set_spacing", "get_spacing", SPACING_BOTTOM);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_char"), "set_spacing", "get_spacing", SPACING_CHAR);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_space"), "set_spacing", "get_spacing", SPACING_SPACE);
    ADD_GROUP("Font", "");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "font_data", PROPERTY_HINT_RESOURCE_TYPE, "DynamicFontData"), "set_font_data", "get_font_data");

    BIND_ENUM_CONSTANT(SPACING_TOP)
    BIND_ENUM_CONSTANT(SPACING_BOTTOM)
    BIND_ENUM_CONSTANT(SPACING_CHAR)
    BIND_ENUM_CONSTANT(SPACING_SPACE)
}

Mutex *DynamicFont::dynamic_font_mutex = nullptr;

SelfList<DynamicFont>::List *DynamicFont::dynamic_fonts = nullptr;

DynamicFont::DynamicFont() :
        font_list(this) {

    cache_id.size = 16;
    outline_cache_id.size = 16;
    spacing_top = 0;
    spacing_bottom = 0;
    spacing_char = 0;
    spacing_space = 0;
    outline_color = Color(1, 1, 1);
    if (dynamic_font_mutex) {
        dynamic_font_mutex->lock();
        dynamic_fonts->add(&font_list);
        dynamic_font_mutex->unlock();
    }
}

DynamicFont::~DynamicFont() {
    if (dynamic_font_mutex) {
        dynamic_font_mutex->lock();
        dynamic_fonts->remove(&font_list);
        dynamic_font_mutex->unlock();
    }
}

void DynamicFont::initialize_dynamic_fonts() {
    dynamic_fonts = memnew(SelfList<DynamicFont>::List());
    dynamic_font_mutex = memnew(Mutex);
}

void DynamicFont::finish_dynamic_fonts() {
    memdelete(dynamic_font_mutex);
    dynamic_font_mutex = nullptr;
    memdelete(dynamic_fonts);
    dynamic_fonts = nullptr;
}

void DynamicFont::update_oversampling() {

    Vector<Ref<DynamicFont> > changed;

    if (dynamic_font_mutex)
        dynamic_font_mutex->lock();

    SelfList<DynamicFont> *E = dynamic_fonts->first();
    while (E) {

        if (E->self()->data_at_size) {
            E->self()->data_at_size->update_oversampling();

            if (E->self()->outline_data_at_size) {
                E->self()->outline_data_at_size->update_oversampling();
            }

            for (int i = 0; i < E->self()->fallback_data_at_size.size(); i++) {
                if (E->self()->fallback_data_at_size[i]) {
                    E->self()->fallback_data_at_size[i]->update_oversampling();

                    if (E->self()->has_outline() && E->self()->fallback_outline_data_at_size[i]) {
                        E->self()->fallback_outline_data_at_size[i]->update_oversampling();
                    }
                }
            }

            changed.push_back(Ref<DynamicFont>(E->self()));
        }

        E = E->next();
    }

    if (dynamic_font_mutex)
        dynamic_font_mutex->unlock();

    for (int i = 0; i < changed.size(); i++) {
        changed.write[i]->emit_changed();
    }
}

/////////////////////////

RES ResourceFormatLoaderDynamicFont::load(se_string_view p_path, se_string_view p_original_path, Error *r_error) {

    if (r_error)
        *r_error = ERR_FILE_CANT_OPEN;

    Ref<DynamicFontData> dfont(make_ref_counted<DynamicFontData>());
    dfont->set_font_path(p_path);

    if (r_error)
        *r_error = OK;

    return dfont;
}

void ResourceFormatLoaderDynamicFont::get_recognized_extensions(PODVector<se_string> &p_extensions) const {

    p_extensions.push_back(("ttf"));
    p_extensions.push_back(("otf"));
}

bool ResourceFormatLoaderDynamicFont::handles_type(se_string_view p_type) const {

    return (p_type == se_string_view("DynamicFontData"));
}

se_string ResourceFormatLoaderDynamicFont::get_resource_type(se_string_view p_path) const {

    se_string el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "ttf" || el == "otf")
        return ("DynamicFontData");
    return {};
}

#endif
