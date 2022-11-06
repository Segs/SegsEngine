/*************************************************************************/
/*  font.cpp                                                             */
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

#include "font.h"
#include "core/pair.h"
#include "font_serializers.h"

#include "core/io/resource_loader.h"
#include "core/os/file_access.h"
#include "core/hashfuncs.h"
#include "core/method_bind.h"
#include "core/resource/resource_manager.h"
#include "core/string.h"
#include "core/ustring.h"
#include "scene/resources/texture.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(Font)
IMPL_GDCLASS(BitmapFont)
RES_BASE_EXTENSION_IMPL(BitmapFont,"font")

void Font::draw_halign(RenderingEntity p_canvas_item, const Point2 &p_pos, HAlign p_align, float p_width, const UIString &p_text, const Color &p_modulate, const Color &p_outline_modulate) const {
    float length = get_ui_string_size(p_text).width;
    if (length >= p_width) {
        draw_ui_string(p_canvas_item, p_pos, p_text, p_modulate, p_width, p_outline_modulate);
        return;
    }

    float ofs = 0.f;
    switch (p_align) {
        case HALIGN_LEFT: {
            ofs = 0;
        } break;
        case HALIGN_CENTER: {
            ofs = Math::floor((p_width - length) / 2.0f);
        } break;
        case HALIGN_RIGHT: {
            ofs = p_width - length;
        } break;
        default: {
            ERR_PRINT("Unknown halignment type");
        } break;
    }
    draw_ui_string(p_canvas_item, p_pos + Point2(ofs, 0), p_text, p_modulate, p_width, p_outline_modulate);
}
void Font::draw_halign_utf8(RenderingEntity p_canvas_item, const Point2 &p_pos, HAlign p_align, float p_width, StringView p_text, const Color &p_modulate, const Color &p_outline_modulate) const {
    draw_halign(p_canvas_item, p_pos, p_align, p_width, StringUtils::from_utf8(p_text), p_modulate, p_outline_modulate);
}
void Font::draw_ui_string(RenderingEntity p_canvas_item, const Point2 &p_pos, const UIString &p_text, const Color &p_modulate, int p_clip_w, const Color &p_outline_modulate) const {
    Vector2 ofs;
    int chars_drawn = 0;
    bool with_outline = has_outline();
    for (int i = 0; i < p_text.length(); i++) {

        int width = get_char_size(p_text[i]).width;
        CharType next = (i+1)<p_text.size() ? p_text[i + 1] : '0';
        if (p_clip_w >= 0 && (ofs.x + width) > p_clip_w)
            break; //clip

        ofs.x += draw_char(p_canvas_item, p_pos + ofs, p_text[i], next, with_outline ? p_outline_modulate : p_modulate, with_outline);
        ++chars_drawn;
    }

    if (has_outline()) {
        ofs = Vector2(0, 0);
        for (int i = 0; i < chars_drawn; i++) {
            CharType next = (i+1)<p_text.size() ? p_text[i + 1] : '0';
            ofs.x += draw_char(p_canvas_item, p_pos + ofs, p_text[i], next, p_modulate, false);
        }
    }
}
void Font::draw(RenderingEntity p_canvas_item, const Point2 &p_pos, StringView p_text, const Color &p_modulate, int p_clip_w, const Color &p_outline_modulate) const {
    draw_ui_string(p_canvas_item, p_pos, StringUtils::from_utf8(p_text), p_modulate, p_clip_w, p_outline_modulate);
}
void Font::update_changes() {

    emit_changed();
}

void Font::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("draw", {"canvas_item", "position", "string", "modulate", "clip_w", "outline_modulate"}), &Font::draw, {DEFVAL(Color(1, 1, 1)), DEFVAL(-1), DEFVAL(Color(1, 1, 1))});
    SE_BIND_METHOD(Font,get_ascent);
    SE_BIND_METHOD(Font,get_descent);
    SE_BIND_METHOD(Font,get_height);
    SE_BIND_METHOD(Font,is_distance_field_hint);
    SE_BIND_METHOD(Font,get_string_size);
    SE_BIND_METHOD(Font,get_wordwrap_string_size);
    SE_BIND_METHOD(Font,has_outline);
    MethodBinder::bind_method(D_METHOD("draw_char", {"canvas_item", "position", "char", "next", "modulate", "outline"}), &Font::draw_char, {DEFVAL(0), DEFVAL(Color(1, 1, 1)), DEFVAL(false)});
    SE_BIND_METHOD(Font,update_changes);
}

Font::Font() {
}

/////////////////////////////////////////////////////////////////

void BitmapFont::_set_chars(const PoolVector<int> &p_chars) {

    int len = p_chars.size();
    //char 1 charsize 1 texture, 4 rect, 2 align, advance 1
    ERR_FAIL_COND(len % 9);
    if (!len) {
        return; //none to do
    }
    int chars = len / 9;

    PoolVector<int>::Read r = p_chars.read();
    for (int i = 0; i < chars; i++) {

        const int *data = &r[i * 9];
        add_char(data[0], data[1], Rect2(data[2], data[3], data[4], data[5]), Size2(data[6], data[7]), data[8]);
    }
}

PoolVector<int> BitmapFont::_get_chars() const {

    PoolVector<int> chars;


    for(const auto &v  : char_map) {

        const Character &c = v.second;
        chars.push_back(v.first);
        chars.push_back(c.texture_idx);
        chars.push_back(c.rect.position.x);
        chars.push_back(c.rect.position.y);

        chars.push_back(c.rect.size.x);
        chars.push_back(c.rect.size.y);
        chars.push_back(c.h_align);
        chars.push_back(c.v_align);
        chars.push_back(c.advance);
    }

    return chars;
}

void BitmapFont::_set_kernings(const PoolVector<int> &p_kernings) {

    int len = p_kernings.size();
    ERR_FAIL_COND(len % 3);
    if (!len) {
        return;
    }
    PoolVector<int>::Read r = p_kernings.read();

    for (int i = 0; i < len / 3; i++) {

        const int *data = &r[i * 3];
        add_kerning_pair(data[0], data[1], data[2]);
    }
}

PoolVector<int> BitmapFont::_get_kernings() const {

    PoolVector<int> kernings;

    for (const eastl::pair<const KerningPairKey,int> &E : kerning_map) {

        kernings.push_back(E.first.A);
        kernings.push_back(E.first.B);
        kernings.push_back(E.second);
    }

    return kernings;
}

void BitmapFont::_set_textures(const Vector<Variant> &p_textures) {

    textures.clear();
    textures.reserve(p_textures.size());
    for (int i = 0; i < p_textures.size(); i++) {
        Ref<Texture> tex = refFromVariant<Texture>(p_textures[i]);
        ERR_CONTINUE(not tex);
        add_texture(tex);
    }
}

Vector<Variant> BitmapFont::_get_textures() const {

    Vector<Variant> rtextures;
    rtextures.reserve(textures.size());
    for (int i = 0; i < textures.size(); i++)
        rtextures.emplace_back(textures[i].get_ref_ptr());
    return rtextures;
}

Error BitmapFont::create_from_fnt(StringView p_file) {
    using namespace eastl;
    //fnt format used by angelcode bmfont
    //http://www.angelcode.com/products/bmfont/

    FileAccess *f = FileAccess::open(p_file, FileAccess::READ);

    ERR_FAIL_COND_V_MSG(!f, ERR_FILE_NOT_FOUND, "Can't open font: " + p_file + ".");

    clear();

    while (true) {

        String line = (f->get_line());

        int delimiter = StringUtils::find(line," ");
        StringView type = StringUtils::substr(line,0, delimiter);
        size_t pos = delimiter + 1;
        Map<StringView, StringView> keys;

        while (pos < line.size() && line[pos] == ' ')
            pos++;

        while (pos < line.size()) {

            size_t eq = StringUtils::find(line,"=", pos);
            if (eq == String::npos)
                break;
            StringView key = StringUtils::substr(line,pos, eq - pos);
            int end = -1;
            StringView value;
            if (line[eq + 1] == '"') {
                end = StringUtils::find(line,"\"", eq + 2);
                if (end == -1)
                    break;
                value = StringUtils::substr(line,eq + 2, end - 1 - eq - 1);
                pos = end + 1;
            } else {
                end = StringUtils::find(line," ", eq + 1);
                if (end == -1)
                    end = line.size();

                value = StringUtils::substr(line,eq + 1, end - eq);

                pos = end;
            }

            while (pos < line.size() && line[pos] == ' ')
                pos++;

            keys[key] = value;
        }

        if (type == "info"_sv) {

            if (keys.contains("face"_sv))
                set_name((keys["face"]));
            /*
            if (keys.has("size"))
                font->set_height(StringUtils::to_int(keys["size"]));
            */

        } else if (type == "common"_sv) {

            if (keys.contains("lineHeight"))
                set_height(StringUtils::to_int(keys["lineHeight"]));
            if (keys.contains("base"))
                set_ascent(StringUtils::to_int(keys["base"]));

        } else if (type == "page"_sv) {

            if (keys.contains("file")) {

                StringView base_dir = PathUtils::get_base_dir(p_file);
                String file = PathUtils::plus_file(base_dir,(keys["file"]));
                Ref<Texture> tex = dynamic_ref_cast<Texture>(gResourceManager().load(file));
                if (not tex) {
                    ERR_PRINT("Can't load font texture!");
                } else {
                    add_texture(tex);
                }
            }
        } else if (type == "char"_sv) {

            int32_t idx = 0;
            if (keys.contains("id"))
                idx = StringUtils::to_int(keys["id"]);

            Rect2 rect;

            if (keys.contains("x"))
                rect.position.x = StringUtils::to_int(keys["x"]);
            if (keys.contains("y"))
                rect.position.y = StringUtils::to_int(keys["y"]);
            if (keys.contains("width"))
                rect.size.width = StringUtils::to_int(keys["width"]);
            if (keys.contains("height"))
                rect.size.height = StringUtils::to_int(keys["height"]);

            Point2 ofs;

            if (keys.contains("xoffset"))
                ofs.x = StringUtils::to_int(keys["xoffset"]);
            if (keys.contains("yoffset"))
                ofs.y = StringUtils::to_int(keys["yoffset"]);

            int texture = 0;
            if (keys.contains("page"))
                texture = StringUtils::to_int(keys["page"]);
            int advance = -1;
            if (keys.contains("xadvance"))
                advance = StringUtils::to_int(keys["xadvance"]);

            add_char(idx, texture, rect, ofs, advance);

        } else if (type == "kerning"_sv) {

            int32_t first = 0, second = 0;
            int k = 0;

            if (keys.contains("first"))
                first = StringUtils::to_int(keys["first"]);
            if (keys.contains("second"))
                second = StringUtils::to_int(keys["second"]);
            if (keys.contains("amount"))
                k = StringUtils::to_int(keys["amount"]);

            add_kerning_pair(first, second, -k);
        }

        if (f->eof_reached())
            break;
    }

    memdelete(f);

    return OK;
}

void BitmapFont::set_height(float p_height) {

    height = p_height;
}
float BitmapFont::get_height() const {

    return height;
}

void BitmapFont::set_ascent(float p_ascent) {

    ascent = p_ascent;
}
float BitmapFont::get_ascent() const {

    return ascent;
}
float BitmapFont::get_descent() const {

    return height - ascent;
}

void BitmapFont::add_texture(const Ref<Texture> &p_texture) {

    ERR_FAIL_COND(not p_texture);
    textures.push_back(p_texture);
}

int BitmapFont::get_texture_count() const {

    return textures.size();
}

Ref<Texture> BitmapFont::get_texture(int p_idx) const {

    ERR_FAIL_INDEX_V(p_idx, textures.size(), Ref<Texture>());
    return textures[p_idx];
}

int BitmapFont::get_character_count() const {

    return char_map.size();
}

Vector<int32_t> BitmapFont::get_char_keys() const {

    Vector<int32_t> chars;
    chars.reserve(char_map.size());

    for(const auto &v  : char_map) {

        chars.push_back(v.first);
    }

    return chars;
}

BitmapFont::Character BitmapFont::get_character(int32_t p_char) const {
    auto iter = char_map.find(p_char);
    if (iter==char_map.end()) {
        ERR_FAIL_V(Character());
    }

    return iter->second;
}

void BitmapFont::add_char(
        int32_t p_char, int p_texture_idx, const Rect2 &p_rect, const Size2 &p_align, float p_advance) {

    if (p_advance < 0)
        p_advance = p_rect.size.width;

    Character c;
    c.rect = p_rect;
    c.texture_idx = p_texture_idx;
    c.v_align = p_align.y;
    c.advance = p_advance;
    c.h_align = p_align.x;

    char_map[p_char] = c;
}

void BitmapFont::add_kerning_pair(int32_t p_A, int32_t p_B, int p_kerning) {

    KerningPairKey kpk;
    kpk.A = p_A;
    kpk.B = p_B;

    if (p_kerning == 0 && kerning_map.contains(kpk)) {

        kerning_map.erase(kpk);
    } else {

        kerning_map[kpk] = p_kerning;
    }
}

Vector<BitmapFont::KerningPairKey> BitmapFont::get_kerning_pair_keys() const {

    Vector<BitmapFont::KerningPairKey> ret;
    ret.reserve(kerning_map.size());

    for (const eastl::pair<const KerningPairKey,int> &E : kerning_map) {
        ret.emplace_back(E.first);
    }

    return ret;
}

int BitmapFont::get_kerning_pair(int32_t p_A, int32_t p_B) const {

    KerningPairKey kpk;
    kpk.A = p_A;
    kpk.B = p_B;

    return kerning_map.at(kpk,0);
}

void BitmapFont::set_distance_field_hint(bool p_distance_field) {

    distance_field_hint = p_distance_field;
    emit_changed();
}

bool BitmapFont::is_distance_field_hint() const {

    return distance_field_hint;
}

void BitmapFont::clear() {

    height = 1;
    ascent = 0;
    char_map.clear();
    textures.clear();
    kerning_map.clear();
    distance_field_hint = false;
}

Size2 Font::get_ui_string_size(const UIString &p_string) const {

    float w = 0;

    int l = p_string.length();
    if (l == 0)
        return Size2(0, get_height());
    const CharType *sptr = p_string.constData();

    for (int i = 0; i < l; i++) {

        w += get_char_size(sptr[i], sptr[i + 1]).width;
    }

    return Size2(w, get_height());
}

Size2 Font::get_string_size(StringView p_string) const {

    Size2 res(0, get_height());
    QString a(QString::fromUtf8(p_string.data(),p_string.size()));
    a.push_back(QChar(0)); // sentinel 0
    int l = a.length();
    for (int i = 0; i < l; i++) {
        CharType next_char = (i+1)<l ? a[i+1] : CharType(0);
        res.x += get_char_size(a[i], next_char).width;
    }

    return res;
}
Size2 Font::get_wordwrap_ui_string_size(const UIString &p_string, float p_width) const {

    ERR_FAIL_COND_V(p_width <= 0, Vector2(0, get_height()));

    int l = p_string.length();
    if (l == 0)
        return Size2(p_width, get_height());

    float line_w = 0;
    float h = 0;
    float space_w = get_char_size(' ').width;
    Vector<UIString> lines = StringUtils::split(p_string,'\n');
    for (const UIString &t : lines) {
        h += get_height();
        line_w = 0;
        Vector<UIString> words = StringUtils::split(t,' ');
        for (const UIString &word : words) {
            line_w += get_ui_string_size(word).x;
            if (line_w > p_width) {
                h += get_height();
                line_w = get_ui_string_size(word).x;
            } else {
                line_w += space_w;
            }
        }
    }

    return Size2(p_width, h);
}
Size2 Font::get_wordwrap_string_size(StringView p_string, float p_width) const {

    return get_wordwrap_ui_string_size(StringUtils::from_utf8(p_string),p_width);
}
void BitmapFont::set_fallback(const Ref<BitmapFont> &p_fallback) {

    for (Ref<BitmapFont> fallback_child = p_fallback; fallback_child != nullptr; fallback_child = fallback_child->get_fallback()) {
        ERR_FAIL_COND_MSG(fallback_child == this, "Can't set as fallback one of its parents to prevent crashes due to recursive loop.");
    }

    fallback = p_fallback;
}
static Pair<int32_t,bool> toUcs4(CharType p_char, CharType p_next) {
    int32_t ch = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        ch = CharType::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return {0,false};
    }
    return {ch,true};
}
Ref<BitmapFont> BitmapFont::get_fallback() const {

    return fallback;
}
RenderingEntity BitmapFont::get_char_texture(CharType p_char, CharType p_next, bool p_outline) const {
    auto dec = toUcs4(p_char,p_next);
    if(!dec.second) {
        return entt::null;
    }
    int32_t ch = dec.first;
    auto iter = char_map.find(ch);

    if (iter==char_map.end()) {
        if (fallback) {
            return fallback->get_char_texture(p_char, p_next, p_outline);
        }
        return entt::null;
    }
    auto &c(iter->second);
    ERR_FAIL_COND_V(c.texture_idx < -1 || c.texture_idx >= textures.size(), entt::null);
    if (!p_outline && c.texture_idx != -1) {
        return textures[c.texture_idx]->get_rid();
    } else {
        return entt::null;
    }
}

Size2 BitmapFont::get_char_texture_size(CharType p_char, CharType p_next, bool p_outline) const {
    auto dec = toUcs4(p_char,p_next);
    if(!dec.second) {
        return Size2();
    }
    int32_t ch = dec.first;

    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback) {
            return fallback->get_char_texture_size(p_char, p_next, p_outline);
        }
        return Size2();
    }
    auto &cr(c->second);

    ERR_FAIL_COND_V(cr.texture_idx < -1 || cr.texture_idx >= textures.size(), Size2());
    if (!p_outline && cr.texture_idx != -1) {
        return textures[cr.texture_idx]->get_size();
    } else {
        return Size2();
    }
}

Vector2 BitmapFont::get_char_tx_offset(CharType p_char, CharType p_next, bool p_outline) const {
    auto dec = toUcs4(p_char,p_next);
    if(!dec.second) {
        return Vector2();
    }
    int32_t ch = dec.first;

    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback) {
            return fallback->get_char_tx_offset(p_char, p_next, p_outline);
        }
        return Vector2();
    }
    auto &cr(c->second);

    ERR_FAIL_COND_V(cr.texture_idx < -1 || cr.texture_idx >= textures.size(), Vector2());
    if (!p_outline && cr.texture_idx != -1) {
        Point2 cpos;
        cpos.x += cr.h_align;
        cpos.y -= ascent;
        cpos.y += cr.v_align;
        return cpos;
    } else {
        return Vector2();
    }
}

Size2 BitmapFont::get_char_tx_size(CharType p_char, CharType p_next, bool p_outline) const {
    auto dec = toUcs4(p_char,p_next);
    if(!dec.second) {
        return Size2();
    }
    int32_t ch = dec.first;

    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback) {
            return fallback->get_char_tx_size(p_char, p_next, p_outline);
        }
        return Size2();
    }
    auto &cr(c->second);
    ERR_FAIL_COND_V(cr.texture_idx < -1 || cr.texture_idx >= textures.size(), Size2());
    if (!p_outline && cr.texture_idx != -1) {
        return cr.rect.size;
    } else {
        return Size2();
    }
}

Rect2 BitmapFont::get_char_tx_uv_rect(CharType p_char, CharType p_next, bool p_outline) const {
    auto dec = toUcs4(p_char,p_next);
    if(!dec.second) {
        return Rect2();
    }
    int32_t ch = dec.first;

    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback) {
            return fallback->get_char_tx_uv_rect(p_char, p_next, p_outline);
        }
        return Rect2();
    }
    auto &cr(c->second);
    ERR_FAIL_COND_V(cr.texture_idx < -1 || cr.texture_idx >= textures.size(), Rect2());
    if (!p_outline && cr.texture_idx != -1) {
        return cr.rect;
    } else {
        return Rect2();
    }
}
float BitmapFont::draw_char(RenderingEntity p_canvas_item, const Point2 &p_pos, CharType p_char, CharType p_next, const Color &p_modulate, bool p_outline) const {

    int32_t ch = p_char.unicode();
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        ch = CharType::surrogateToUcs4(p_char,p_next);
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return 0;
    }
    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback) {
            return fallback->draw_char(p_canvas_item, p_pos, p_char, p_next, p_modulate, p_outline);
        }
        return 0;
    }

    ERR_FAIL_COND_V(c->second.texture_idx < -1 || c->second.texture_idx >= textures.size(), 0);
    if (!p_outline && c->second.texture_idx != -1) {
        Point2 cpos = p_pos;
        cpos.x += c->second.h_align;
        cpos.y -= ascent;
        cpos.y += c->second.v_align;
        RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas_item, Rect2(cpos, c->second.rect.size), textures[c->second.texture_idx]->get_rid(), c->second.rect, p_modulate, false, entt::null, false);
    }

    return get_char_size(p_char, p_next).width;
}

Size2 BitmapFont::get_char_size(CharType p_char, CharType p_next) const {
    int32_t ch = p_char.unicode();
    bool skip_kerning = false;
    if (p_char.isHighSurrogate() && p_next.isLowSurrogate()) { // decode surrogate pair.
        ch = CharType::surrogateToUcs4(p_char,p_next);
        skip_kerning = true;
    }
    if (p_char.isLowSurrogate()) { // skip trail surrogate.
        return Size2();
    }

    auto c = char_map.find(ch);

    if (c==char_map.end()) {
        if (fallback)
            return fallback->get_char_size(p_char, p_next);
        return Size2();
    }

    Size2 ret(c->second.advance, c->second.rect.size.y);

    if (!skip_kerning) {
    if (!p_next.isNull()) {

        KerningPairKey kpk;
        kpk.A = p_char.unicode();
        kpk.B = p_next.unicode();

        const Map<KerningPairKey, int>::const_iterator E = kerning_map.find(kpk);
        if (E!=kerning_map.end()) {

            ret.width -= E->second;
        }
    }

    }
    return ret;
}

void BitmapFont::_bind_methods() {

    SE_BIND_METHOD(BitmapFont,create_from_fnt);
    SE_BIND_METHOD(BitmapFont,set_height);

    SE_BIND_METHOD(BitmapFont,set_ascent);

    SE_BIND_METHOD(BitmapFont,add_kerning_pair);
    SE_BIND_METHOD(BitmapFont,get_kerning_pair);

    SE_BIND_METHOD(BitmapFont,add_texture);
    MethodBinder::bind_method(D_METHOD("add_char", {"character", "texture", "rect", "align", "advance"}), &BitmapFont::add_char, {DEFVAL(Point2()), DEFVAL(-1)});

    SE_BIND_METHOD(BitmapFont,get_texture_count);
    SE_BIND_METHOD(BitmapFont,get_texture);

    MethodBinder::bind_method(D_METHOD("get_char_size", {"char", "next"}), &BitmapFont::get_char_size, {DEFVAL(0)});

    SE_BIND_METHOD(BitmapFont,set_distance_field_hint);

    SE_BIND_METHOD(BitmapFont,clear);

    SE_BIND_METHOD(BitmapFont,_set_chars);
    SE_BIND_METHOD(BitmapFont,_get_chars);

    SE_BIND_METHOD(BitmapFont,_set_kernings);
    SE_BIND_METHOD(BitmapFont,_get_kernings);

    SE_BIND_METHOD(BitmapFont,_set_textures);
    SE_BIND_METHOD(BitmapFont,_get_textures);

    SE_BIND_METHOD(BitmapFont,set_fallback);
    SE_BIND_METHOD(BitmapFont,get_fallback);

    ADD_PROPERTY(PropertyInfo(VariantType::ARRAY, "textures", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_textures", "_get_textures");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_INT_ARRAY, "chars", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_chars", "_get_chars");
    ADD_PROPERTY(PropertyInfo(VariantType::POOL_INT_ARRAY, "kernings", PropertyHint::None, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "_set_kernings", "_get_kernings");

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "height", PropertyHint::Range, "1,1024,1"), "set_height", "get_height");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "ascent", PropertyHint::Range, "0,1024,1"), "set_ascent", "get_ascent");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "distance_field"), "set_distance_field_hint", "is_distance_field_hint");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "fallback", PropertyHint::ResourceType, "BitmapFont"), "set_fallback", "get_fallback");
}

BitmapFont::BitmapFont() {

    clear();
}

BitmapFont::~BitmapFont() {

    clear();
}

////////////

RES ResourceFormatLoaderBMFont::load(StringView p_path, StringView p_original_path, Error *r_error, bool p_no_subresource_cache) {

    if (r_error)
        *r_error = ERR_FILE_CANT_OPEN;

    Ref<BitmapFont> font(make_ref_counted<BitmapFont>());

    Error err = font->create_from_fnt(p_path);

    if (err) {
        if (r_error)
            *r_error = err;
        return RES();
    }

    return font;
}

void ResourceFormatLoaderBMFont::get_recognized_extensions(Vector<String> &p_extensions) const {

    p_extensions.push_back(("fnt"));
}

bool ResourceFormatLoaderBMFont::handles_type(StringView p_type) const {

    return (p_type == StringView("BitmapFont"));
}

String ResourceFormatLoaderBMFont::get_resource_type(StringView p_path) const {

    String el = StringUtils::to_lower(PathUtils::get_extension(p_path));
    if (el == "fnt")
        return ("BitmapFont");
    return String();
}
