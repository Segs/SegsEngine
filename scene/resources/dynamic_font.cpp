/*************************************************************************/
/*  dynamic_font.cpp                                                     */
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

#ifdef FREETYPE_ENABLED
#include "dynamic_font.h"
#include "font_serializers.h"

#include "core/object_tooling.h"
#include "core/os/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "scene/resources/texture.h"
#include "core/method_bind.h"
#include "core/ustring.h"
#include "core/pair.h"
#include "core/string_utils.inl"
#include "servers/rendering_server.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include FT_STROKER_H

#include <cstdint>

IMPL_GDCLASS(DynamicFontData)
IMPL_GDCLASS(DynamicFontAtSize)
IMPL_GDCLASS(DynamicFont)

VARIANT_ENUM_CAST(DynamicFontData::Hinting);
VARIANT_ENUM_CAST(DynamicFont::SpacingType);

static Vector<DynamicFont *> dynamic_fonts;

struct DynamicFontAtSize::ImplData
{
    struct CharTexture {

        PoolVector<uint8_t> imgdata;
        Vector<int> offsets;
        Ref<ImageTexture> texture;
        int texture_size;
    };

    struct Character {

        Rect2 rect={};
        Rect2 rect_uv={};
        int texture_idx=0;
        float v_align=0;
        float h_align=0;
        float advance=0;
        bool found=false;

        constexpr Character() {}

        constexpr static Character not_found() {
            Character ch;
            ch.texture_idx = -1;
            return ch;
        }
    };

    struct TexturePosition {
        int index;
        int x;
        int y;
    };
    _THREAD_SAFE_CLASS_
    Vector<uint8_t> s_df_fontdata;
    HashMap<int32_t, Character> char_map;
    Vector<CharTexture> textures;
    FT_Library library; /* handle to library     */
    FT_Face face; /* handle to face object */
    FT_StreamRec stream;
    Ref<DynamicFontData> font;
    DynamicFontData::CacheID id;
    float ascent;
    float descent;
    float rect_margin;
    float linegap;
    float oversampling;
    float scale_color_font;

    uint32_t texture_flags;
    bool valid;

    Character _bitmap_to_character(DynamicFontAtSize::ImplData *fa, FT_Bitmap bitmap, int yofs, int xofs, float advance) {
        int w = bitmap.width;
        int h = bitmap.rows;

        int mw = w + fa->rect_margin * 2;
        int mh = h + fa->rect_margin * 2;

        ERR_FAIL_COND_V(mw > 4096, Character::not_found());
            ERR_FAIL_COND_V(mh > 4096, Character::not_found());

            int color_size = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA ? 4 : 2;
        Image::Format require_format = color_size == 4 ? ImageData::FORMAT_RGBA8 : ImageData::FORMAT_LA8;

        TexturePosition tex_pos = fa->_find_texture_pos_for_glyph(color_size, require_format, mw, mh);
        ERR_FAIL_COND_V(tex_pos.index < 0, Character::not_found());

            //fit character in char texture

            CharTexture &tex = fa->textures[tex_pos.index];

        {
            PoolVector<uint8_t>::Write wr = tex.imgdata.write();

            for (int i = 0; i < h; i++) {
                for (int j = 0; j < w; j++) {

                    int ofs = ((i + tex_pos.y + fa->rect_margin) * tex.texture_size + j + tex_pos.x + fa->rect_margin) * color_size;
                    ERR_FAIL_COND_V(ofs >= tex.imgdata.size(), Character::not_found());
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
    const Pair<const Character *, ImplData *> _find_char_with_font(int32_t p_char, const Vector<Ref<DynamicFontAtSize> > &p_fallbacks) const {
        auto chr = char_map.find(p_char);
        ERR_FAIL_COND_V(chr== char_map.end(), (Pair<const Character *, ImplData *>(nullptr, nullptr)));

        //TODO: the code here assumes pointer/iterator stability of char_map by returning pointers to value
        if (!chr->second.found) {

            //not found, try in fallbacks
            for (const Ref<DynamicFontAtSize>& fallback : p_fallbacks) {

                DynamicFontAtSize *fb = const_cast<DynamicFontAtSize *>(fallback.get());
                if (!fb->m_impl->valid)
                    continue;

                fb->m_impl->_update_char(p_char);
                const auto fallback_chr = fb->m_impl->char_map.find(p_char);
                ERR_CONTINUE(fallback_chr==fb->m_impl->char_map.end());

                if (!fallback_chr->second.found)
                    continue;
                return Pair<const Character *, ImplData *>(&fallback_chr->second, fb->m_impl);
            }

            //not found, try 0xFFFD to display 'not found'.
            const_cast<DynamicFontAtSize::ImplData *>(this)->_update_char(0xFFFD);
            chr = char_map.find(0xFFFD);
            ERR_FAIL_COND_V(chr == char_map.end(), (Pair<const Character *, ImplData *>(nullptr, nullptr)));
        }

        return Pair<const Character *, ImplData *>(&chr->second, const_cast<ImplData *>(this));
    }
    void _update_char(int32_t p_char) {

        if (char_map.contains(p_char))
            return;

        _THREAD_SAFE_METHOD_;

        Character character = Character::not_found();

        FT_GlyphSlot slot = face->glyph;

        if (FT_Get_Char_Index(face, p_char) == 0) {
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

        int error = FT_Load_Char(face, p_char, FT_HAS_COLOR(face) ? FT_LOAD_COLOR : FT_LOAD_DEFAULT | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0) | ft_hinting);
        if (error) {
            char_map[p_char] = character;
            return;
        }

        if (id.outline_size > 0) {
            character = _make_outline_char(p_char);
        } else {
            error = FT_Render_Glyph(face->glyph, font->antialiased ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO);
            if (!error)
                character = _bitmap_to_character(this,slot->bitmap, slot->bitmap_top, slot->bitmap_left, slot->advance.x / 64.0f);
        }

        char_map[p_char] = character;
    }
    float _get_kerning_advance(int32_t p_char, int32_t p_next) const
    {
        float advance = 0.0;

        if (p_next!=0) {
            FT_Vector delta;
            FT_Get_Kerning(face, FT_Get_Char_Index(face, p_char), FT_Get_Char_Index(face, p_next), FT_KERNING_DEFAULT, &delta);
            advance = (delta.x / 64.0) / oversampling;
        }

        return advance;
    }
    Character _make_outline_char(int32_t p_char) {
        Character ret = Character::not_found();

        if (FT_Load_Char(face, p_char, FT_LOAD_NO_BITMAP | (font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0)) != 0)
            return ret;

        FT_Stroker stroker;
        if (FT_Stroker_New(library, &stroker) != 0)
            return ret;

        FT_Stroker_Set(stroker, (int)(id.outline_size * oversampling * 64.0f), FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph glyph;
        FT_BitmapGlyph glyph_bitmap;

        if (FT_Get_Glyph(face->glyph, &glyph) != 0)
            goto cleanup_stroker;
        if (FT_Glyph_Stroke(&glyph, stroker, 1) != 0)
            goto cleanup_glyph;
        if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, nullptr, 1) != 0)
            goto cleanup_glyph;

        glyph_bitmap = (FT_BitmapGlyph)glyph;
        ret = _bitmap_to_character(this,glyph_bitmap->bitmap, glyph_bitmap->top, glyph_bitmap->left, glyph->advance.x / 65536.0f);

    cleanup_glyph:
        FT_Done_Glyph(glyph);
    cleanup_stroker:
        FT_Stroker_Done(stroker);
        return ret;
    }

    TexturePosition _find_texture_pos_for_glyph(int p_color_size, Image::Format p_image_format, int p_width, int p_height) {
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

            int texsize = M_MAX(id.size * oversampling * 8, 256);
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
                // zero texture
                PoolVector<uint8_t>::Write w = tex.imgdata.write();
                ERR_FAIL_COND_V(texsize * texsize * p_color_size > tex.imgdata.size(), ret);
                // Initialize the texture to all-white pixels to prevent artifacts when the
                // font is displayed at a non-default scale with filtering enabled.
                if (p_color_size == 2) {
                    for (int i = 0; i < texsize * texsize * p_color_size; i += 2) {
                        w[i + 0] = 255;
                        w[i + 1] = 0;
                    }
                } else {
                    for (int i = 0; i < texsize * texsize * p_color_size; i += 4) {
                        w[i + 0] = 255;
                        w[i + 1] = 255;
                        w[i + 2] = 255;
                        w[i + 3] = 0;
                    }
                }
            }
            tex.offsets.resize(texsize,0); //zero offsets

            textures.push_back(tex);
            ret.index = textures.size() - 1;
        }

        return ret;
    }

    Error load() {
        int error = FT_Init_FreeType(&library);

        ERR_FAIL_COND_V_MSG(error != 0, ERR_CANT_CREATE, "Error initializing FreeType.");

        if (font->font_mem == nullptr && !font->font_path.empty()) {
            FileAccessRef f(FileAccess::open(font->font_path, FileAccess::READ));
            if (!f) {
                FT_Done_FreeType(library);
                ERR_FAIL_V_MSG(ERR_CANT_OPEN, "Cannot open font file '" + font->font_path + "'.");
            }

            uint64_t len = f->get_len();
            font->_fontdata = Vector<uint8_t>();
            font->_fontdata.resize(len);
            f->get_buffer(font->_fontdata.data(), len);
            font->set_font_ptr(font->_fontdata.data(), len);
            f->close();
        }

        if (font->font_mem) {
            memset(&stream, 0, sizeof(FT_StreamRec));
            stream.base = (unsigned char *)font->font_mem;
            stream.size = font->font_mem_size;
            stream.pos = 0;

            FT_Open_Args fargs;
            memset(&fargs, 0, sizeof(FT_Open_Args));
            fargs.memory_base = (unsigned char *)font->font_mem;
            fargs.memory_size = font->font_mem_size;
            fargs.flags = FT_OPEN_MEMORY;
            fargs.stream = &stream;
            error = FT_Open_Face(library, &fargs, 0, &face);

        } else {
            FT_Done_FreeType(library);
            ERR_FAIL_V_MSG(ERR_UNCONFIGURED, "DynamicFont uninitialized.");
        }

        //error = FT_New_Face( library, src_path.utf8().get_data(),0,&face );

        if (error == FT_Err_Unknown_File_Format) {
            ERR_PRINT("Unknown font format.");
            FT_Done_FreeType(library);
        } else if (error) {

            ERR_PRINT("Error loading font.");
            FT_Done_FreeType(library);
        }

        ERR_FAIL_COND_V(error, ERR_FILE_CANT_OPEN);

        if (FT_HAS_COLOR(face) && face->num_fixed_sizes > 0) {
            int best_match = 0;
            int diff = ABS(id.size - ((int64_t)face->available_sizes[0].width));
            scale_color_font = float(id.size * oversampling) / face->available_sizes[0].width;
            for (int i = 1; i < face->num_fixed_sizes; i++) {
                int ndiff = ABS(id.size - ((int64_t)face->available_sizes[i].width));
                if (ndiff < diff) {
                    best_match = i;
                    diff = ndiff;
                    scale_color_font = float(id.size * oversampling) / face->available_sizes[i].width;
                }
            }
            FT_Select_Size(face, best_match);
        } else {
            FT_Set_Pixel_Sizes(face, 0, id.size * oversampling);
        }

        ascent = (face->size->metrics.ascender / 64.0f) / oversampling * scale_color_font;
        descent = (-face->size->metrics.descender / 64.0f) / oversampling * scale_color_font;
        linegap = 0;
        texture_flags = 0;
        if (id.mipmaps)
            texture_flags |= Texture::FLAG_MIPMAPS;
        if (id.filter)
            texture_flags |= Texture::FLAG_FILTER;

        valid = true;
        return OK;
    }
    void update_oversampling() {
        if (!valid) {
            return;
        }
        float new_oversampling = (font && font->override_oversampling > 0) ? font->override_oversampling : font_oversampling;
        if (oversampling == new_oversampling) {
            return;
        }

        FT_Done_FreeType(library);
        textures.clear();
        char_map.clear();
        oversampling = new_oversampling;
        valid = false;
        load();
    }

    ImplData() {
        valid = false;
        rect_margin = 1;
        ascent = 1;
        descent = 1;
        linegap = 1;
        texture_flags = 0;
        oversampling = font_oversampling;
        scale_color_font = 1;
    }
    ~ImplData() {
        if (valid) {
            FT_Done_FreeType(library);
        }
        font->size_cache.erase(id);
        font.unref();
    }
};

Ref<DynamicFontAtSize> DynamicFontData::_get_dynamic_font_at_size(CacheID p_cache_id) {

    if (size_cache.contains(p_cache_id)) {
        return Ref<DynamicFontAtSize>(size_cache[p_cache_id]);
    }

    Ref<DynamicFontAtSize> dfas(make_ref_counted<DynamicFontAtSize>());

    dfas->m_impl->font = Ref<DynamicFontData>(this);
    dfas->m_impl->oversampling = (override_oversampling > 0) ? override_oversampling : DynamicFontAtSize::font_oversampling;

    size_cache[p_cache_id] = dfas.get();
    dfas->m_impl->id = p_cache_id;
    dfas->_load();

    return dfas;
}

void DynamicFontData::set_font_ptr(const uint8_t *p_font_mem, int p_font_mem_size) {

    font_mem = p_font_mem;
    font_mem_size = p_font_mem_size;
}

void DynamicFontData::set_font_path(StringView p_path) {

    font_path = p_path;
}

const String &DynamicFontData::get_font_path() const {
    return font_path;
}

void DynamicFontData::set_force_autohinter(bool p_force) {

    force_autohinter = p_force;
}
float DynamicFontData::get_override_oversampling() const {
    return override_oversampling;
}

void DynamicFontData::set_override_oversampling(float p_oversampling) {
    if (override_oversampling == p_oversampling) {
        return;
    }

    override_oversampling = p_oversampling;
    DynamicFont::update_oversampling();
}

void DynamicFontData::_bind_methods() {
    SE_BIND_METHOD(DynamicFontData,set_antialiased);
    SE_BIND_METHOD(DynamicFontData,is_antialiased);
    SE_BIND_METHOD(DynamicFontData,set_font_path);
    SE_BIND_METHOD(DynamicFontData,get_font_path);
    SE_BIND_METHOD(DynamicFontData,set_hinting);
    SE_BIND_METHOD(DynamicFontData,get_hinting);

    SE_BIND_METHOD(DynamicFontData,get_override_oversampling);
    SE_BIND_METHOD(DynamicFontData,set_override_oversampling);
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "antialiased"), "set_antialiased", "is_antialiased");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "hinting", PropertyHint::Enum, "None,Light,Normal"), "set_hinting", "get_hinting");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "override_oversampling"), "set_override_oversampling", "get_override_oversampling");

    BIND_ENUM_CONSTANT(HINTING_NONE);
    BIND_ENUM_CONSTANT(HINTING_LIGHT);
    BIND_ENUM_CONSTANT(HINTING_NORMAL);

    // Only WOFF1 is supported as WOFF2 requires a Brotli decompression library to be linked.
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "font_path", PropertyHint::File, "*.ttf,*.otf,*.woff,*.woff2"), "set_font_path", "get_font_path");
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

Error DynamicFontAtSize::_load() {
    return m_impl->load();
}

float DynamicFontAtSize::font_oversampling = 1.0;

float DynamicFontAtSize::get_height() const {

    return m_impl->ascent + m_impl->descent;
}

float DynamicFontAtSize::get_ascent() const {

    return m_impl->ascent;
}
float DynamicFontAtSize::get_descent() const {

    return m_impl->descent;
}

Size2 DynamicFontAtSize::get_char_size(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize> > &p_fallbacks) const {

    if (!m_impl->valid) {
        return Size2(1, 1);
    }
    bool skip_kerning = false;

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = CharType::surrogateToUcs4(p_char,p_next);
        skip_kerning = true;
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return Size2();
    }

    const_cast<DynamicFontAtSize *>(this)->m_impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    ERR_FAIL_COND_V(!ch, Size2());

    Size2 ret(0, get_height());

    if (ch->found) {
        ret.x = ch->advance;
    }
    if (!skip_kerning) {
        ret.x += m_impl->_get_kerning_advance(p_char.unicode(), p_next.unicode());
    }

    return ret;
}

UIString DynamicFontAtSize::get_available_chars() const {
    if (!m_impl->valid) {
        return "";
    }
    UIString chars;

    FT_UInt gindex;
    FT_ULong charcode = FT_Get_First_Char(m_impl->face, &gindex);
    while (gindex != 0) {
        if (charcode != 0) {
            chars += QChar((uint)charcode);
        }
        charcode = FT_Get_Next_Char(m_impl->face, charcode, &gindex);
    }

    return chars;
}

void DynamicFontAtSize::set_texture_flags(uint32_t p_flags) {

    m_impl->texture_flags = p_flags;
    for (size_t i = 0; i < m_impl->textures.size(); i++) {
        Ref<ImageTexture> &tex = m_impl->textures[i].texture;
        if (tex)
            tex->set_flags(p_flags);
    }
}

float DynamicFontAtSize::draw_char(RenderingEntity p_canvas_item, const Point2 &p_pos, CharType p_char, CharType p_next,
        const Color &p_modulate, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks, bool p_advance_only,
        bool p_outline) const {
    if (!m_impl->valid) {
        return 0;
    }
    int32_t c = p_char.unicode();
    bool skip_kerning = false;

    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = CharType::surrogateToUcs4(p_char,p_next);
        skip_kerning = true;
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return 0;
    }
    const_cast<DynamicFontAtSize *>(this)->m_impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    ImplData *font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, 0.0);

    float advance = 0.0;
    // use normal character size if there's no outline charater
    if (p_outline && !ch->found) {
        FT_GlyphSlot slot = m_impl->face->glyph;
        int error = FT_Load_Char(
                m_impl->face, c, FT_HAS_COLOR(m_impl->face) ? FT_LOAD_COLOR : FT_LOAD_DEFAULT);
        if (!error) {
            error = FT_Render_Glyph(m_impl->face->glyph, FT_RENDER_MODE_NORMAL);
            if (!error) {
                ImplData::Character character = m_impl->_bitmap_to_character(const_cast<ImplData *>(m_impl),
                        slot->bitmap, slot->bitmap_top, slot->bitmap_left, slot->advance.x / 64.0f);
                advance = character.advance;
            }
        }
    }
    if (ch->found) {
        ERR_FAIL_COND_V(ch->texture_idx < -1 || ch->texture_idx >= font->textures.size(), 0);

        if (!p_advance_only && ch->texture_idx != -1) {
            Point2 cpos = p_pos;
            cpos.x += ch->h_align;
            cpos.y -= m_impl->ascent;
            cpos.y += ch->v_align;
            Color modulate = p_modulate;
            if (FT_HAS_COLOR(m_impl->face)) {
                modulate.r = modulate.g = modulate.b = 1.0f;
            }
            RenderingEntity texture = font->textures[ch->texture_idx].texture->get_rid();
            RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item,
                    Rect2(cpos, ch->rect.size), texture, ch->rect_uv, modulate, false, entt::null, false);
        }

        advance = ch->advance;
    }
    if (!skip_kerning) {
        advance += m_impl->_get_kerning_advance(p_char.unicode(), p_next.unicode());
    }

    return advance;
}

RenderingEntity DynamicFontAtSize::get_char_texture(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return entt::null;
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return entt::null;
    }
    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;
    impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    auto *font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, entt::null);
    if (ch->found) {
        ERR_FAIL_COND_V(ch->texture_idx < -1 || ch->texture_idx >= font->textures.size(), entt::null);

        if (ch->texture_idx != -1) {
            return font->textures[ch->texture_idx].texture->get_rid();
        }
    }
    return entt::null;
}

Size2 DynamicFontAtSize::get_char_texture_size(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return Size2();
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return Size2();
    }
    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;

    impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    auto *font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, Size2());
    if (ch->found) {
        ERR_FAIL_COND_V(ch->texture_idx < -1 || ch->texture_idx >= font->textures.size(), Size2());

        if (ch->texture_idx != -1) {
            return font->textures[ch->texture_idx].texture->get_size();
        }
    }
    return Size2();
}

Vector2 DynamicFontAtSize::get_char_tx_offset(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return Vector2();
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate())  { // skip trail surrogate.
        return Vector2();
    }
    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;

    impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    auto *font = char_pair_with_font.second;

    ERR_FAIL_COND_V(!ch, Vector2());
    if (ch->found) {
        Point2 cpos;
        cpos.x += ch->h_align;
        cpos.y -= font->ascent;
        cpos.y += ch->v_align;

        return cpos;
    }
    return Vector2();
}

Size2 DynamicFontAtSize::get_char_tx_size(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return Size2();
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate())  { // skip trail surrogate.
        return Size2();
    }

    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;

    impl->_update_char(c);
    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;


    ERR_FAIL_COND_V(!ch, Size2());
    if (ch->found) {
        return ch->rect.size;
    }
    return Size2();
}

Rect2 DynamicFontAtSize::get_char_tx_uv_rect(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return Rect2();
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate())  { // skip trail surrogate.
        return Rect2();
    }
    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;
    impl->_update_char(c);
    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;

    ERR_FAIL_COND_V(!ch, Rect2());
    if (ch->found) {
        return ch->rect_uv;
    }
    return Rect2();
}

void DynamicFontAtSize::update_oversampling() {
    m_impl->update_oversampling();
}

DynamicFontAtSize::DynamicFontAtSize() :m_impl(memnew(ImplData)) {
}

DynamicFontAtSize::~DynamicFontAtSize() {
    memdelete(m_impl);
}

/////////////////////////

void DynamicFont::_reload_cache(const char *p_triggering_property) {

    ERR_FAIL_COND(cache_id.size < 1);
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
    Object_change_notify(this,StringName(p_triggering_property));
}

void DynamicFont::set_font_data(const Ref<DynamicFontData> &p_data) {

    data = p_data;
    _reload_cache(); // not passing the prop name as clearing the font data also clears fallbacks
}

Ref<DynamicFontData> DynamicFont::get_font_data() const {

    return data;
}

void DynamicFont::set_size(int p_size) {

    if (cache_id.size == p_size)
        return;
    cache_id.size = p_size;
    outline_cache_id.size = p_size;
    _reload_cache("size");
}

int DynamicFont::get_size() const {

    return cache_id.size;
}

void DynamicFont::set_outline_size(int p_size) {
    if (outline_cache_id.outline_size == p_size)
        return;
    ERR_FAIL_COND(p_size < 0 || p_size > UINT8_MAX);
    outline_cache_id.outline_size = p_size;
    _reload_cache("outline_size");
}

int DynamicFont::get_outline_size() const {
    return outline_cache_id.outline_size;
}

void DynamicFont::set_outline_color(Color p_color) {
    if (p_color != outline_color) {
        outline_color = p_color;
        emit_changed();
        Object_change_notify(this,"outline_color");
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
        Object_change_notify(this, "extra_spacing_top");
    } else if (p_type == SPACING_BOTTOM) {
        spacing_bottom = p_value;
        Object_change_notify(this, "extra_spacing_bottom");
    } else if (p_type == SPACING_CHAR) {
        spacing_char = p_value;
        Object_change_notify(this, "extra_spacing_char");
    } else if (p_type == SPACING_SPACE) {
        spacing_space = p_value;
        Object_change_notify(this, "extra_spacing_space");
    }

    emit_changed();
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

String DynamicFont::get_available_chars() const {

    if (!data_at_size)
        return "";

    UIString chars = data_at_size->get_available_chars();

    for (int i = 0; i < fallback_data_at_size.size(); i++) {
        UIString fallback_chars = fallback_data_at_size[i]->get_available_chars();
        for (int j = 0; j < fallback_chars.length(); j++) {
            if (chars.contains(fallback_chars[j])) {
                chars += fallback_chars[j];
            }
        }
    }

    return StringUtils::to_utf8(chars);
}

bool DynamicFont::is_distance_field_hint() const {

    return false;
}

bool DynamicFont::has_outline() const {
    return outline_cache_id.outline_size > 0;
}

RenderingEntity DynamicFont::get_char_texture(CharType p_char, CharType p_next, bool p_outline) const {
    if (!data_at_size) {
        return entt::null;
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            return outline_data_at_size->get_char_texture(p_char, p_next, fallback_outline_data_at_size);
        }
        return entt::null;
    } else {
        return data_at_size->get_char_texture(p_char, p_next, fallback_data_at_size);
    }
}

Size2 DynamicFont::get_char_texture_size(CharType p_char, CharType p_next, bool p_outline) const {
    if (!data_at_size) {
        return Size2();
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            return outline_data_at_size->get_char_texture_size(p_char, p_next, fallback_outline_data_at_size);
        }
        return Size2();
    } else {
        return data_at_size->get_char_texture_size(p_char, p_next, fallback_data_at_size);
    }
}

Vector2 DynamicFont::get_char_tx_offset(CharType p_char, CharType p_next, bool p_outline) const {
    if (!data_at_size) {
        return Vector2();
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            return outline_data_at_size->get_char_tx_offset(p_char, p_next, fallback_outline_data_at_size);
        }
        return Vector2();
    } else {
        return data_at_size->get_char_tx_offset(p_char, p_next, fallback_data_at_size);
    }
}

Size2 DynamicFont::get_char_tx_size(CharType p_char, CharType p_next, bool p_outline) const {
    if (!data_at_size) {
        return Size2();
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            return outline_data_at_size->get_char_tx_size(p_char, p_next, fallback_outline_data_at_size);
        }
        return Size2();
    } else {
        return data_at_size->get_char_tx_size(p_char, p_next, fallback_data_at_size);
    }
}

Rect2 DynamicFont::get_char_tx_uv_rect(CharType p_char, CharType p_next, bool p_outline) const {
    if (!data_at_size) {
        return Rect2();
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            return outline_data_at_size->get_char_tx_uv_rect(p_char, p_next, fallback_outline_data_at_size);
        }
        return Rect2();
    } else {
        return data_at_size->get_char_tx_uv_rect(p_char, p_next, fallback_data_at_size);
    }
}

float DynamicFont::draw_char(RenderingEntity p_canvas_item, const Point2 &p_pos, CharType p_char, CharType p_next, const Color &p_modulate, bool p_outline) const {
    if (!data_at_size)
        return 0;
    int spacing = spacing_char;
    if (p_char == ' ') {
        spacing += spacing_space;
    }

    if (p_outline) {
        if (outline_data_at_size && outline_cache_id.outline_size > 0) {
            outline_data_at_size->draw_char(p_canvas_item, p_pos, p_char, p_next, p_modulate * outline_color, fallback_outline_data_at_size, false, true); // Draw glyph outline.
        }
        return data_at_size->draw_char(p_canvas_item, p_pos, p_char, p_next, p_modulate, fallback_data_at_size, true, false) + spacing; // Return advance of the base glyph.
    } else {
        return data_at_size->draw_char(p_canvas_item, p_pos, p_char, p_next, p_modulate, fallback_data_at_size, false, false) + spacing; // Draw base glyph and return advance.
    }
}

CharContour DynamicFontAtSize::get_char_contours(CharType p_char, CharType p_next, const Vector<Ref<DynamicFontAtSize>> &p_fallbacks) const {
    if (!m_impl->valid) {
        return CharContour();
    }

    int32_t c = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        c = QChar::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return CharContour();
    }
    auto *impl = const_cast<DynamicFontAtSize *>(this)->m_impl;

    impl->_update_char(c);

    auto char_pair_with_font = m_impl->_find_char_with_font(c, p_fallbacks);
    const ImplData::Character *ch = char_pair_with_font.first;
    DynamicFontAtSize::ImplData *font = char_pair_with_font.second;

    if (ch->found) {
        Vector<Vector3> points;
        Vector<int> contours;

        int error = FT_Load_Char(font->face, c, FT_LOAD_NO_BITMAP | (font->font->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0));
        ERR_FAIL_COND_V(error, CharContour());

        double scale = (1.0 / 64.0) / impl->oversampling * m_impl->scale_color_font;
        for (short i = 0; i < font->face->glyph->outline.n_points; i++) {
            points.emplace_back(font->face->glyph->outline.points[i].x * scale, -font->face->glyph->outline.points[i].y * scale, FT_CURVE_TAG(font->face->glyph->outline.tags[i]));
        }
        for (short i = 0; i < font->face->glyph->outline.n_contours; i++) {
            contours.emplace_back(font->face->glyph->outline.contours[i]);
        }
        bool orientation = (FT_Outline_Get_Orientation(&font->face->glyph->outline) == FT_ORIENTATION_FILL_RIGHT);

        CharContour out {eastl::move(points),eastl::move(contours), orientation,true};
        return out;
    } else {
        return CharContour();
    }
}

CharContour DynamicFont::get_char_contours(CharType p_char, CharType p_next) const {
    if (!data_at_size) {
        return CharContour();
    }

    return data_at_size->get_char_contours(p_char, p_next, fallback_data_at_size);
}

void DynamicFont::set_fallback(int p_idx, const Ref<DynamicFontData> &p_data) {

    ERR_FAIL_COND(not p_data);
    ERR_FAIL_INDEX(p_idx, fallbacks.size());
    fallbacks[p_idx] = p_data;
    fallback_data_at_size[p_idx] = fallbacks[p_idx]->_get_dynamic_font_at_size(cache_id);
}

void DynamicFont::add_fallback(const Ref<DynamicFontData> &p_data) {

    ERR_FAIL_COND(not p_data);
    fallbacks.push_back(p_data);
    fallback_data_at_size.push_back(fallbacks[fallbacks.size() - 1]->_get_dynamic_font_at_size(cache_id)); //const..
    if (outline_cache_id.outline_size > 0)
        fallback_outline_data_at_size.push_back(fallbacks[fallbacks.size() - 1]->_get_dynamic_font_at_size(outline_cache_id));

    emit_changed();
    Object_change_notify(this);
}

int DynamicFont::get_fallback_count() const {
    return fallbacks.size();
}
Ref<DynamicFontData> DynamicFont::get_fallback(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, fallbacks.size(), Ref<DynamicFontData>());

    return fallbacks[p_idx];
}
void DynamicFont::remove_fallback(int p_idx) {

    ERR_FAIL_INDEX(p_idx, fallbacks.size());
    fallbacks.erase_at(p_idx);
    fallback_data_at_size.erase_at(p_idx);
    emit_changed();
    Object_change_notify(this);
}

bool DynamicFont::_set(const StringName &p_name, const Variant &p_value) {

    if (StringUtils::begins_with(p_name,"fallback/")) {
        int idx = StringUtils::to_int(StringUtils::get_slice(p_name,'/', 1));
        Ref<DynamicFontData> fd = refFromVariant<DynamicFontData>(p_value);

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
void DynamicFont::_get_property_list(Vector<PropertyInfo> *p_list) const {

    for (int i = 0; i < fallbacks.size(); i++) {
        p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("fallback/" + itos(i)), PropertyHint::ResourceType, "DynamicFontData"));
    }

    p_list->push_back(PropertyInfo(VariantType::OBJECT, StringName("fallback/" + itos(fallbacks.size())), PropertyHint::ResourceType, "DynamicFontData"));
}

void DynamicFont::_bind_methods() {

    SE_BIND_METHOD(DynamicFont,set_font_data);
    SE_BIND_METHOD(DynamicFont,get_font_data);

    SE_BIND_METHOD(DynamicFont,get_available_chars);

    SE_BIND_METHOD(DynamicFont,set_size);
    SE_BIND_METHOD(DynamicFont,get_size);

    SE_BIND_METHOD(DynamicFont,set_outline_size);
    SE_BIND_METHOD(DynamicFont,get_outline_size);

    SE_BIND_METHOD(DynamicFont,set_outline_color);
    SE_BIND_METHOD(DynamicFont,get_outline_color);

    SE_BIND_METHOD(DynamicFont,set_use_mipmaps);
    SE_BIND_METHOD(DynamicFont,get_use_mipmaps);
    SE_BIND_METHOD(DynamicFont,set_use_filter);
    SE_BIND_METHOD(DynamicFont,get_use_filter);
    SE_BIND_METHOD(DynamicFont,set_spacing);
    SE_BIND_METHOD(DynamicFont,get_spacing);

    SE_BIND_METHOD(DynamicFont,add_fallback);
    SE_BIND_METHOD(DynamicFont,set_fallback);
    SE_BIND_METHOD(DynamicFont,get_fallback);
    SE_BIND_METHOD(DynamicFont,remove_fallback);
    SE_BIND_METHOD(DynamicFont,get_fallback_count);

    ADD_GROUP("Settings", "stng_");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "stng_size", PropertyHint::Range, "1,1024,1"), "set_size", "get_size");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "stng_outline_size", PropertyHint::Range, "0,255,1"), "set_outline_size", "get_outline_size");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "stng_outline_color"), "set_outline_color", "get_outline_color");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "stng_use_mipmaps"), "set_use_mipmaps", "get_use_mipmaps");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "stng_use_filter"), "set_use_filter", "get_use_filter");
    ADD_GROUP("Extra Spacing", "extra_spacing_");
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_top"), "set_spacing", "get_spacing", SPACING_TOP);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_bottom"), "set_spacing", "get_spacing", SPACING_BOTTOM);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_char"), "set_spacing", "get_spacing", SPACING_CHAR);
    ADD_PROPERTYI(PropertyInfo(VariantType::INT, "extra_spacing_space"), "set_spacing", "get_spacing", SPACING_SPACE);
    ADD_GROUP("Font", "font_");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "font_data", PropertyHint::ResourceType, "DynamicFontData"), "set_font_data", "get_font_data");

    BIND_ENUM_CONSTANT(SPACING_TOP);
    BIND_ENUM_CONSTANT(SPACING_BOTTOM);
    BIND_ENUM_CONSTANT(SPACING_CHAR);
    BIND_ENUM_CONSTANT(SPACING_SPACE);
}

Mutex DynamicFont::dynamic_font_mutex;

DynamicFont::DynamicFont() {

    cache_id.size = 16;
    outline_cache_id.size = 16;

    spacing_top = 0;
    spacing_bottom = 0;
    spacing_char = 0;
    spacing_space = 0;
    outline_color = Color(1, 1, 1);
    MutexGuard guard(dynamic_font_mutex);
    dynamic_fonts.push_back(this);
}

DynamicFont::~  DynamicFont() {
    MutexGuard guard(dynamic_font_mutex);
    dynamic_fonts.erase_first(this);
}

void DynamicFont::initialize_dynamic_fonts() {
    ERR_FAIL_COND(!dynamic_fonts.empty());
}

void DynamicFont::finish_dynamic_fonts() {
    ERR_FAIL_COND_MSG(!dynamic_fonts.empty(),"Not all dynamic fonts were destroyed before the global list reset");
    dynamic_fonts.clear();
}

void DynamicFont::update_oversampling() {

    Vector<Ref<DynamicFont> > changed;

    {
        MutexGuard guard(dynamic_font_mutex);

    for(DynamicFont * fnt : dynamic_fonts) {

        if (fnt->data_at_size) {
            fnt->data_at_size->update_oversampling();

            if (fnt->outline_data_at_size) {
                fnt->outline_data_at_size->update_oversampling();
            }

            for (size_t i = 0; i < fnt->fallback_data_at_size.size(); i++) {
                if (fnt->fallback_data_at_size[i]) {
                    fnt->fallback_data_at_size[i]->update_oversampling();

                    if (fnt->has_outline() && fnt->fallback_outline_data_at_size[i]) {
                        fnt->fallback_outline_data_at_size[i]->update_oversampling();
                    }
                }
            }

            changed.emplace_back(Ref<DynamicFont>(fnt));
        }

        }
    }

    for (const Ref<DynamicFont> & c : changed) {
        c->emit_changed();
    }
}

/////////////////////////

RES ResourceFormatLoaderDynamicFont::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    if (r_error)
        *r_error = ERR_FILE_CANT_OPEN;

    Ref<DynamicFontData> dfont(make_ref_counted<DynamicFontData>());
    dfont->set_font_path(p_path);

    if (r_error)
        *r_error = OK;

    return dfont;
}

void ResourceFormatLoaderDynamicFont::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.emplace_back("ttf");
    p_extensions.emplace_back("otf");
    p_extensions.emplace_back("woff");
    p_extensions.emplace_back("woff2");
}

bool ResourceFormatLoaderDynamicFont::handles_type(StringView p_type) const {

    return p_type == StringView("DynamicFontData");
}

String ResourceFormatLoaderDynamicFont::get_resource_type(StringView p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "ttf" || el == "otf" || el == "woff" || el == "woff2") {
        return "DynamicFontData";
    }
    return {};
}

#endif
