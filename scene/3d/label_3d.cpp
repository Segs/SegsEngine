/*************************************************************************/
/*  label_3d.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "label_3d.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/method_bind.h"
#include "core/method_enum_caster.h"
#include "core/math/triangle_mesh.h"
#include "scene/resources/theme.h"
#include "scene/scene_string_names.h"

VARIANT_ENUM_CAST(Label3D::DrawFlags);
VARIANT_ENUM_CAST(Label3D::AlphaCutMode);
VARIANT_ENUM_CAST(Label3D::Align);
VARIANT_ENUM_CAST(Label3D::VAlign);
VARIANT_ENUM_CAST(SpatialMaterial::BillboardMode);

IMPL_GDCLASS(Label3D)

void Label3D::_bind_methods() {
    BIND_METHOD(Label3D,set_horizontal_alignment);
    BIND_METHOD(Label3D,get_horizontal_alignment);

    BIND_METHOD(Label3D,set_vertical_alignment);
    BIND_METHOD(Label3D,get_vertical_alignment);

    BIND_METHOD(Label3D,set_modulate);
    BIND_METHOD(Label3D,get_modulate);

    BIND_METHOD(Label3D,set_outline_modulate);
    BIND_METHOD(Label3D,get_outline_modulate);

    BIND_METHOD(Label3D,set_text);
    BIND_METHOD(Label3D,get_text);

    BIND_METHOD(Label3D,set_uppercase);
    BIND_METHOD(Label3D,is_uppercase);

    BIND_METHOD(Label3D,set_render_priority);
    BIND_METHOD(Label3D,get_render_priority);

    BIND_METHOD(Label3D,set_outline_render_priority);
    BIND_METHOD(Label3D,get_outline_render_priority);

    BIND_METHOD(Label3D,set_font);
    BIND_METHOD(Label3D,get_font);

    BIND_METHOD(Label3D,set_line_spacing);
    BIND_METHOD(Label3D,get_line_spacing);

    BIND_METHOD(Label3D,set_autowrap);
    BIND_METHOD(Label3D,get_autowrap);

    BIND_METHOD(Label3D,set_width);
    BIND_METHOD(Label3D,get_width);

    BIND_METHOD(Label3D,set_pixel_size);
    BIND_METHOD(Label3D,get_pixel_size);

    BIND_METHOD(Label3D,set_offset);
    BIND_METHOD(Label3D,get_offset);

    BIND_METHOD(Label3D,set_draw_flag);
    BIND_METHOD(Label3D,get_draw_flag);

    BIND_METHOD(Label3D,set_billboard_mode);
    BIND_METHOD(Label3D,get_billboard_mode);

    BIND_METHOD(Label3D,set_alpha_cut_mode);
    BIND_METHOD(Label3D,get_alpha_cut_mode);

    BIND_METHOD(Label3D,set_alpha_scissor_threshold);
    BIND_METHOD(Label3D,get_alpha_scissor_threshold);

    BIND_METHOD(Label3D,generate_triangle_mesh);

    BIND_METHOD(Label3D,_queue_update);
    BIND_METHOD(Label3D,_font_changed);
    BIND_METHOD(Label3D,_im_update);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pixel_size", PropertyHint::Range, "0.0001,128,0.0001"), "set_pixel_size", "get_pixel_size");
    ADD_PROPERTY(PropertyInfo(VariantType::VECTOR2, "offset"), "set_offset", "get_offset");

    ADD_GROUP("Flags", "");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "billboard", PropertyHint::Enum, "Disabled,Enabled,Y-Billboard"), "set_billboard_mode", "get_billboard_mode");
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "shaded"), "set_draw_flag", "get_draw_flag", FLAG_SHADED);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "double_sided"), "set_draw_flag", "get_draw_flag", FLAG_DOUBLE_SIDED);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "no_depth_test"), "set_draw_flag", "get_draw_flag", FLAG_DISABLE_DEPTH_TEST);
    ADD_PROPERTYI(PropertyInfo(VariantType::BOOL, "fixed_size"), "set_draw_flag", "get_draw_flag", FLAG_FIXED_SIZE);
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "alpha_cut", PropertyHint::Enum, "Disabled,Discard,Opaque Pre-Pass"), "set_alpha_cut_mode", "get_alpha_cut_mode");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "alpha_scissor_threshold", PropertyHint::Range, "0,1,0.01"), "set_alpha_scissor_threshold", "get_alpha_scissor_threshold");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "render_priority", PropertyHint::Range, itos(RS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(RS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_render_priority", "get_render_priority");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "outline_render_priority", PropertyHint::Range, itos(RS::MATERIAL_RENDER_PRIORITY_MIN) + "," + itos(RS::MATERIAL_RENDER_PRIORITY_MAX) + ",1"), "set_outline_render_priority", "get_outline_render_priority");

    ADD_GROUP("Text", "");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "modulate"), "set_modulate", "get_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::COLOR, "outline_modulate"), "set_outline_modulate", "get_outline_modulate");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "text", PropertyHint::MultilineText, ""), "set_text", "get_text");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "font", PropertyHint::ResourceType, "Font"), "set_font", "get_font");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "horizontal_alignment", PropertyHint::Enum, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "vertical_alignment", PropertyHint::Enum, "Top,Center,Bottom"), "set_vertical_alignment", "get_vertical_alignment");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "uppercase"), "set_uppercase", "is_uppercase");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "line_spacing"), "set_line_spacing", "get_line_spacing");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "autowrap"), "set_autowrap", "get_autowrap");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "width"), "set_width", "get_width");

    BIND_ENUM_CONSTANT(FLAG_SHADED);
    BIND_ENUM_CONSTANT(FLAG_DOUBLE_SIDED);
    BIND_ENUM_CONSTANT(FLAG_DISABLE_DEPTH_TEST);
    BIND_ENUM_CONSTANT(FLAG_FIXED_SIZE);
    BIND_ENUM_CONSTANT(FLAG_MAX);

    BIND_ENUM_CONSTANT(ALPHA_CUT_DISABLED);
    BIND_ENUM_CONSTANT(ALPHA_CUT_DISCARD);
    BIND_ENUM_CONSTANT(ALPHA_CUT_OPAQUE_PREPASS);

    BIND_ENUM_CONSTANT(ALIGN_LEFT);
    BIND_ENUM_CONSTANT(ALIGN_CENTER);
    BIND_ENUM_CONSTANT(ALIGN_RIGHT);
    BIND_ENUM_CONSTANT(ALIGN_FILL);

    BIND_ENUM_CONSTANT(VALIGN_TOP);
    BIND_ENUM_CONSTANT(VALIGN_CENTER);
    BIND_ENUM_CONSTANT(VALIGN_BOTTOM);
    BIND_ENUM_CONSTANT(VALIGN_FILL);
}

void Label3D::_validate_property(PropertyInfo &property) const {
    if (property.name == "material_override" || property.name == "material_overlay") {
        property.usage = PROPERTY_USAGE_NOEDITOR;
    }
}

int Label3D::get_longest_line_width() const {
    Ref<Font> font = _get_font_or_default();
    real_t max_line_width = 0;
    real_t line_width = 0;

    for (int i = 0; i < xl_text.size(); i++) {
        CharType current = xl_text[i];
        if (uppercase) {
            current = StringUtils::char_uppercase(current);
        }

        if (current < 32) {
            if (current == '\n') {
                if (line_width > max_line_width) {
                    max_line_width = line_width;
                }
                line_width = 0;
            }
        } else {
            real_t char_width = font->get_char_size(current, xl_text[i + 1]).width;
            line_width += char_width;
        }
    }

    if (line_width > max_line_width) {
        max_line_width = line_width;
    }

    // ceiling to ensure autowrapping does not cut text
    return Math::ceil(max_line_width);
}

void Label3D::regenerate_word_cache() {
    while (word_cache) {
        WordCache *current = word_cache;
        word_cache = current->next;
        memdelete(current);
    }

    int max_line_width;
    if (autowrap) {
        max_line_width = width;
    } else {
        max_line_width = get_longest_line_width();
    }

    Ref<Font> font = _get_font_or_default();

    real_t current_word_size = 0;
    int word_pos = 0;
    real_t line_width = 0;
    int space_count = 0;
    real_t space_width = font->get_char_size(' ').width;
    line_count = 1;
    bool was_separatable = false;

    WordCache *last = nullptr;

    for (int i = 0; i <= xl_text.length(); i++) {
        CharType current = i < xl_text.length() ? xl_text[i] : L' '; //always a space at the end, so the algo works

        if (uppercase) {
            current = StringUtils::char_uppercase(current);
        }

        // ranges taken from https://en.wikipedia.org/wiki/Plane_(Unicode)
        // if your language is not well supported, consider helping improve
        // the unicode support in Godot.
        bool separatable = (current >= 0x2E08 && current <= 0x9FFF) || // CJK scripts and symbols.
                (current >= 0xAC00 && current <= 0xD7FF) || // Hangul Syllables and Hangul Jamo Extended-B.
                (current >= 0xF900 && current <= 0xFAFF) || // CJK Compatibility Ideographs.
                (current >= 0xFE30 && current <= 0xFE4F) || // CJK Compatibility Forms.
                (current >= 0xFF65 && current <= 0xFF9F) || // Halfwidth forms of katakana
                (current >= 0xFFA0 && current <= 0xFFDC) || // Halfwidth forms of compatibility jamo characters for Hangul
                (current >= 0x20000 && current <= 0x2FA1F) || // CJK Unified Ideographs Extension B ~ F and CJK Compatibility Ideographs Supplement.
                (current >= 0x30000 && current <= 0x3134F); // CJK Unified Ideographs Extension G.
        bool insert_newline = false;
        real_t char_width = 0;

        bool separation_changed = i > 0 && was_separatable != separatable;
        was_separatable = separatable;

        if (current < 33) { // Control characters and space.
            if (current_word_size > 0) { // These characters always create a word-break.
                WordCache *wc = memnew(WordCache);
                if (word_cache) {
                    last->next = wc;
                } else {
                    word_cache = wc;
                }
                last = wc;

                wc->pixel_width = current_word_size;
                wc->char_pos = word_pos;
                wc->word_len = i - word_pos;
                wc->space_count = space_count;
                current_word_size = 0;
                space_count = 0;
            } else if ((i == xl_text.length() || current == '\n') && last != nullptr && space_count != 0) {
                // In case there are trailing white spaces we add a placeholder word cache with just the spaces.
                WordCache *wc = memnew(WordCache);
                if (word_cache) {
                    last->next = wc;
                } else {
                    word_cache = wc;
                }
                last = wc;

                wc->pixel_width = 0;
                wc->char_pos = 0;
                wc->word_len = 0;
                wc->space_count = space_count;
                current_word_size = 0;
                space_count = 0;
            }

            if (current == '\n') {
                insert_newline = true;
            }

            if (i < xl_text.length() && xl_text[i] == ' ') {
                if (line_width == 0) {
                    if (current_word_size == 0) {
                        word_pos = i;
                    }
                    current_word_size += space_width;
                    line_width += space_width;
                } else if (line_width > 0 || last == nullptr || last->char_pos != WordCache::CHAR_WRAPLINE) {
                    space_count++;
                    line_width += space_width;
                } else {
                    space_count = 0;
                }
            }

        } else { // Characters with graphical representation.
            // Word-break on CJK & non-CJK edge.
            if (separation_changed && current_word_size > 0) {
                WordCache *wc = memnew(WordCache);
                if (word_cache) {
                    last->next = wc;
                } else {
                    word_cache = wc;
                }
                last = wc;

                wc->pixel_width = current_word_size;
                wc->char_pos = word_pos;
                wc->word_len = i - word_pos;
                wc->space_count = space_count;
                current_word_size = 0;
                space_count = 0;
            }
            if (current_word_size == 0) {
                word_pos = i;
            }
            char_width = font->get_char_size(current, xl_text[i + 1]).width;
            current_word_size += char_width;
            line_width += char_width;

            // allow autowrap to cut words when they exceed line width
            if (autowrap && (current_word_size > max_line_width)) {
                separatable = true;
            }
        }

        if ((autowrap && (line_width >= max_line_width) && ((last && last->char_pos >= 0) || separatable)) || insert_newline) {
            if (separatable) {
                if (current_word_size > 0) {
                    WordCache *wc = memnew(WordCache);
                    if (word_cache) {
                        last->next = wc;
                    } else {
                        word_cache = wc;
                    }
                    last = wc;

                    wc->pixel_width = current_word_size - char_width;
                    wc->char_pos = word_pos;
                    wc->word_len = i - word_pos;
                    wc->space_count = space_count;
                    current_word_size = char_width;
                    word_pos = i;
                }
            }

            WordCache *wc = memnew(WordCache);
            if (word_cache) {
                last->next = wc;
            } else {
                word_cache = wc;
            }
            last = wc;

            wc->pixel_width = 0;
            wc->char_pos = insert_newline ? WordCache::CHAR_NEWLINE : WordCache::CHAR_WRAPLINE;

            line_width = current_word_size;
            line_count++;
            space_count = 0;
        }
    }

    word_cache_dirty = false;
}

void Label3D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            if (!pending_update) {
                _im_update();
            }
        } break;
        case NOTIFICATION_TRANSLATION_CHANGED: {
            StringName new_text = tr(text);
            if (new_text == xl_text) {
                return; // Nothing new.
            }
            xl_text = new_text;

            regenerate_word_cache();
            _queue_update();
        } break;
    }
}

void Label3D::_im_update() {
    _shape();

    triangle_mesh.unref();
    update_gizmo();

    pending_update = false;
}

void Label3D::_queue_update() {
    if (pending_update) {
        return;
    }

    pending_update = true;
    call_deferred(SceneStringNames::_im_update);
}

AABB Label3D::get_aabb() const {
    return aabb;
}

Ref<TriangleMesh> Label3D::generate_triangle_mesh() const {
    if (triangle_mesh) {
        return triangle_mesh;
    }

    Ref<Font> font = _get_font_or_default();
    if (!font) {
        return Ref<TriangleMesh>();
    }

    FixedVector<Vector3,6> faces;

    if (word_cache_dirty) {
        const_cast<Label3D *>(this)->regenerate_word_cache();
    }

    float font_h = font->get_height() + line_spacing;
    real_t space_w = font->get_char_size(' ').width;
    float total_h = line_count * font_h;

    float vbegin = 0;
    switch (vertical_alignment) {
        case VALIGN_FILL:
        case VALIGN_TOP: {
            // Nothing.
        } break;
        case VALIGN_CENTER: {
            vbegin = (total_h - line_spacing) / 2.0;
        } break;
        case VALIGN_BOTTOM: {
            vbegin = (total_h - line_spacing);
        } break;
    }

    WordCache *wc = word_cache;
    if (!wc) {
        return Ref<TriangleMesh>();
    }

    float max_line_w = 0.0;
    int line = 0;
    while (wc) {
        if (line >= line_count) {
            break;
        }

        if (wc->char_pos < 0) {
            wc = wc->next;
            line++;
            continue;
        }

        WordCache *to = wc;

        float taken = 0;
        int spaces = 0;
        while (to && to->char_pos >= 0) {
            taken += to->pixel_width;
            if (to->space_count) {
                spaces += to->space_count;
            }
            to = to->next;
        }

        max_line_w = M_MAX(max_line_w, (taken + spaces * space_w));

        wc = to ? to->next : nullptr;
        line++;
    }

    Vector2 offset = Vector2(0, vbegin);
    switch (horizontal_alignment) {
        case ALIGN_FILL:
        case ALIGN_LEFT: {
            // Noting
        } break;
        case ALIGN_CENTER: {
            offset.x = -max_line_w / 2.0;
        } break;
        case ALIGN_RIGHT: {
            offset.x = -max_line_w;
        } break;
    }

    Rect2 final_rect = Rect2(offset + lbl_offset, Size2(max_line_w, total_h));

    if (final_rect.size.x == 0 || final_rect.size.y == 0) {
        return Ref<TriangleMesh>();
    }

    real_t pixel_size = get_pixel_size();

    Vector2 vertices[4] = {

        (final_rect.position + Vector2(0, -final_rect.size.y)) * pixel_size,
        (final_rect.position + Vector2(final_rect.size.x, -final_rect.size.y)) * pixel_size,
        (final_rect.position + Vector2(final_rect.size.x, 0)) * pixel_size,
        final_rect.position * pixel_size,

    };

    static const int indices[6] = {
        0, 1, 2,
        0, 2, 3
    };

    for (int j = 0; j < 6; j++) {
        int i = indices[j];
        Vector3 vtx(vertices[i].x,vertices[i].y,0);
        faces.emplace_back(vtx);
    }

    triangle_mesh = Ref<TriangleMesh>(memnew(TriangleMesh));
    triangle_mesh->create(faces);

    return triangle_mesh;
}

Vector<Face3> Label3D::get_faces(uint32_t p_usage_flags) const {
    return {};
}

float Label3D::_generate_glyph_surfaces(const Ref<Font> &p_font, CharType p_char, CharType p_next, Vector2 p_offset, const Color &p_modulate, int p_priority, bool p_outline) {
    Vector2 gl_of;
    Vector2 gl_sz;
    Rect2 gl_uv;
    Size2 texs;
    RenderingEntity tex;
    RenderingServer *rs = RenderingServer::get_singleton();
    tex = p_font->get_char_texture(p_char, p_next, p_outline);
    gl_of = p_font->get_char_tx_offset(p_char, p_next, p_outline);
    gl_sz = p_font->get_char_tx_size(p_char, p_next, p_outline);
    gl_uv = p_font->get_char_tx_uv_rect(p_char, p_next, p_outline);
    texs = p_font->get_char_texture_size(p_char, p_next, p_outline);

    SurfaceKey key = SurfaceKey(entt::to_integral(tex), p_priority);
    if (!surfaces.contains(key)) {
        SurfaceData surf;
        surf.material = RID_PRIME(RenderingServer::get_singleton()->material_create());
        // Set defaults for material, names need to match up those in SpatialMaterial
        rs->material_set_param(surf.material, "albedo", Color(1, 1, 1, 1));
        rs->material_set_param(surf.material, "specular", 0.5);
        rs->material_set_param(surf.material, "metallic", 0.0);
        rs->material_set_param(surf.material, "roughness", 1.0);
        rs->material_set_param(surf.material, "uv1_offset", Vector3(0, 0, 0));
        rs->material_set_param(surf.material, "uv1_scale", Vector3(1, 1, 1));
        rs->material_set_param(surf.material, "uv2_offset", Vector3(0, 0, 0));
        rs->material_set_param(surf.material, "uv2_scale", Vector3(1, 1, 1));
        rs->material_set_param(surf.material, "alpha_scissor_threshold", alpha_scissor_threshold);

        RenderingEntity shader_rid = SpatialMaterial::get_material_rid_for_2d(get_draw_flag(FLAG_SHADED), true,
                get_draw_flag(FLAG_DOUBLE_SIDED), get_alpha_cut_mode() == ALPHA_CUT_DISCARD,
                get_alpha_cut_mode() == ALPHA_CUT_OPAQUE_PREPASS,
                get_billboard_mode() == SpatialMaterial::BILLBOARD_ENABLED,
                get_billboard_mode() == SpatialMaterial::BILLBOARD_FIXED_Y, get_draw_flag(FLAG_DISABLE_DEPTH_TEST),
                get_draw_flag(FLAG_FIXED_SIZE), p_font->is_distance_field_hint());

        rs->material_set_shader(surf.material, rs->material_get_shader(shader_rid));
        rs->material_set_param(surf.material, "texture_albedo", Variant::from(tex));
        if (get_alpha_cut_mode() == ALPHA_CUT_DISABLED) {
            rs->material_set_render_priority(surf.material, p_priority);
        } else {
            surf.z_shift = p_priority;
        }

        surfaces[key] = surf;
    }
    SurfaceData &s = surfaces[key];

    s.mesh_vertices.resize((s.offset + 1) * 4);
    s.mesh_normals.resize((s.offset + 1) * 4);
    s.mesh_tangents.resize((s.offset + 1) * 16);
    s.mesh_colors.resize((s.offset + 1) * 4);
    s.mesh_uvs.resize((s.offset + 1) * 4);

    s.mesh_vertices[(s.offset * 4) + 3] = Vector3(p_offset.x + gl_of.x, p_offset.y - gl_of.y - gl_sz.y, s.z_shift) * pixel_size;
    s.mesh_vertices[(s.offset * 4) + 2] = Vector3(p_offset.x + gl_of.x + gl_sz.x, p_offset.y - gl_of.y - gl_sz.y, s.z_shift) * pixel_size;
    s.mesh_vertices[(s.offset * 4) + 1] = Vector3(p_offset.x + gl_of.x + gl_sz.x, p_offset.y - gl_of.y, s.z_shift) * pixel_size;
    s.mesh_vertices[(s.offset * 4) + 0] = Vector3(p_offset.x + gl_of.x, p_offset.y - gl_of.y, s.z_shift) * pixel_size;

    for (int i = 0; i < 4; i++) {
        s.mesh_normals[(s.offset * 4) + i] = Vector3(0.0, 0.0, 1.0);
        s.mesh_tangents[(s.offset * 16) + (i * 4) + 0] = 1.0;
        s.mesh_tangents[(s.offset * 16) + (i * 4) + 1] = 0.0;
        s.mesh_tangents[(s.offset * 16) + (i * 4) + 2] = 0.0;
        s.mesh_tangents[(s.offset * 16) + (i * 4) + 3] = 1.0;
        s.mesh_colors[(s.offset * 4) + i] = p_modulate;
        s.mesh_uvs[(s.offset * 4) + i] = Vector2();

        if (aabb == AABB()) {
            aabb.position = s.mesh_vertices[(s.offset * 4) + i];
        } else {
            aabb.expand_to(s.mesh_vertices[(s.offset * 4) + i]);
        }
    }

    if (tex != entt::null) {
        s.mesh_uvs[(s.offset * 4) + 3] = Vector2(gl_uv.position.x / texs.x, (gl_uv.position.y + gl_uv.size.y) / texs.y);
        s.mesh_uvs[(s.offset * 4) + 2] = Vector2((gl_uv.position.x + gl_uv.size.x) / texs.x, (gl_uv.position.y + gl_uv.size.y) / texs.y);
        s.mesh_uvs[(s.offset * 4) + 1] = Vector2((gl_uv.position.x + gl_uv.size.x) / texs.x, gl_uv.position.y / texs.y);
        s.mesh_uvs[(s.offset * 4) + 0] = Vector2(gl_uv.position.x / texs.x, gl_uv.position.y / texs.y);
    }

    s.indices.resize((s.offset + 1) * 6);
    s.indices[(s.offset * 6) + 0] = (s.offset * 4) + 0;
    s.indices[(s.offset * 6) + 1] = (s.offset * 4) + 1;
    s.indices[(s.offset * 6) + 2] = (s.offset * 4) + 2;
    s.indices[(s.offset * 6) + 3] = (s.offset * 4) + 0;
    s.indices[(s.offset * 6) + 4] = (s.offset * 4) + 2;
    s.indices[(s.offset * 6) + 5] = (s.offset * 4) + 3;

    s.offset++;
    return p_font->get_char_size(p_char, p_next).x;
}

void Label3D::_shape() {
    // Clear mesh.
    RenderingServer *rs = RenderingServer::get_singleton();
    rs->mesh_clear(mesh);
    aabb = AABB();

    // Clear materials.
    {
        for(const auto &mat : surfaces) {
            rs->free_rid(mat.second.material);
        }
        surfaces.clear();
    }

    Ref<Font> font = _get_font_or_default();
    ERR_FAIL_COND(!font);

    if (word_cache_dirty) {
        regenerate_word_cache();
    }

    // Generate surfaces and materials.

    float font_h = font->get_height() + line_spacing;
    real_t space_w = font->get_char_size(' ').width;
    float total_h = line_count * font_h;

    float vbegin = 0.0;
    switch (vertical_alignment) {
        case VALIGN_FILL:
        case VALIGN_TOP: {
            // Nothing.
        } break;
        case VALIGN_CENTER: {
            vbegin = (total_h - line_spacing) / 2.0;
        } break;
        case VALIGN_BOTTOM: {
            vbegin = (total_h - line_spacing);
        } break;
    }

    WordCache *wc = word_cache;
    if (!wc) {
        return;
    }

    int line = 0;
    while (wc) {
        if (line >= line_count) {
            break;
        }

        if (wc->char_pos < 0) {
            wc = wc->next;
            line++;
            continue;
        }

        WordCache *from = wc;
        WordCache *to = wc;

        float taken = 0;
        int spaces = 0;
        while (to && to->char_pos >= 0) {
            taken += to->pixel_width;
            if (to->space_count) {
                spaces += to->space_count;
            }
            to = to->next;
        }

        bool can_fill = to && (to->char_pos == WordCache::CHAR_WRAPLINE || to->char_pos == WordCache::CHAR_NEWLINE);

        float x_ofs = 0;
        switch (horizontal_alignment) {
            case ALIGN_FILL: {
                x_ofs = -width / 2.0;
            } break;
            case ALIGN_LEFT: {
                // Noting
            } break;
            case ALIGN_CENTER: {
                x_ofs = -(taken + spaces * space_w) / 2.0;
            } break;
            case ALIGN_RIGHT: {
                x_ofs = -(taken + spaces * space_w);
            } break;
        }

        float y_ofs = 0;
        y_ofs -= line * font_h + font->get_ascent();
        y_ofs += vbegin;

        while (from != to) {
            // draw a word
            int pos = from->char_pos;
            if (from->char_pos < 0) {
                ERR_PRINT("BUG");
                return;
            }
            if (from->space_count) {
                /* spacing */
                x_ofs += space_w * from->space_count;
                if (can_fill && horizontal_alignment == ALIGN_FILL && spaces) {
                    x_ofs += ((width - (taken + space_w * spaces)) / spaces);
                }
            }

            if (font->has_outline()) {
                float x_ofs_ol = x_ofs;
                for (int i = 0; i < from->word_len; i++) {
                    CharType c = xl_text[i + pos];
                    CharType n = xl_text[i + pos + 1];
                    if (uppercase) {
                        c = StringUtils::char_uppercase(c);
                        n = StringUtils::char_uppercase(n);
                    }

                    x_ofs_ol += _generate_glyph_surfaces(font, c, n, lbl_offset + Point2(x_ofs_ol, y_ofs), outline_modulate, outline_render_priority, true);
                }
            }
            for (int i = 0; i < from->word_len; i++) {
                CharType c = xl_text[i + pos];
                CharType n = xl_text[i + pos + 1];
                if (uppercase) {
                    c = StringUtils::char_uppercase(c);
                    n = StringUtils::char_uppercase(n);
                }
                x_ofs += _generate_glyph_surfaces(font, c, n, lbl_offset + Point2(x_ofs, y_ofs), modulate, render_priority, false);
            }
            from = from->next;
        }

        wc = to ? to->next : nullptr;
        line++;
    }

    int idx = 0;
    for(auto & sdata : surfaces) {
        const SurfaceData &surf = sdata.second;
        SurfaceArrays mesh_array(Vector<Vector3>(surf.mesh_vertices));
        mesh_array.m_normals = surf.mesh_normals;
        mesh_array.m_tangents = surf.mesh_tangents;
        mesh_array.m_colors = surf.mesh_colors;
        mesh_array.m_uv_1 = surf.mesh_uvs;
        mesh_array.m_indices = surf.indices;

        rs->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, mesh_array);
        rs->instance_set_surface_material(get_instance(), idx++, surf.material);
    }
}

void Label3D::set_text(const String &p_string) {
    text = p_string;
    xl_text = tr(p_string);
    word_cache_dirty = true;
    _queue_update();
}

String Label3D::get_text() const {
    return text;
}

void Label3D::set_horizontal_alignment(Label3D::Align p_alignment) {
    ERR_FAIL_INDEX((int)p_alignment, 4);
    if (horizontal_alignment != p_alignment) {
        horizontal_alignment = p_alignment;
        _queue_update();
    }
}

Label3D::Align Label3D::get_horizontal_alignment() const {
    return horizontal_alignment;
}

void Label3D::set_vertical_alignment(Label3D::VAlign p_alignment) {
    ERR_FAIL_INDEX((int)p_alignment, 4);
    if (vertical_alignment != p_alignment) {
        vertical_alignment = p_alignment;
        _queue_update();
    }
}

Label3D::VAlign Label3D::get_vertical_alignment() const {
    return vertical_alignment;
}

void Label3D::set_uppercase(bool p_uppercase) {
    if (uppercase != p_uppercase) {
        uppercase = p_uppercase;
        word_cache_dirty = true;
        _queue_update();
    }
}

bool Label3D::is_uppercase() const {
    return uppercase;
}

void Label3D::set_render_priority(int p_priority) {
    ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);
    if (render_priority != p_priority) {
        render_priority = p_priority;
        _queue_update();
    }
}

int Label3D::get_render_priority() const {
    return render_priority;
}

void Label3D::set_outline_render_priority(int p_priority) {
    ERR_FAIL_COND(p_priority < RS::MATERIAL_RENDER_PRIORITY_MIN || p_priority > RS::MATERIAL_RENDER_PRIORITY_MAX);
    if (outline_render_priority != p_priority) {
        outline_render_priority = p_priority;
        _queue_update();
    }
}

int Label3D::get_outline_render_priority() const {
    return outline_render_priority;
}

void Label3D::_font_changed() {
    word_cache_dirty = true;
    _queue_update();
}

void Label3D::set_font(const Ref<Font> &p_font) {
    if (font_override != p_font) {
        if (font_override) {
            font_override->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Label3D::_font_changed));
        }
        font_override = p_font;
        if (font_override) {
            font_override->connect(CoreStringNames::get_singleton()->changed, callable_mp(this, &Label3D::_font_changed));
        }
        _queue_update();
    }
}

Ref<Font> Label3D::get_font() const {
    return font_override;
}

Ref<Font> Label3D::_get_font_or_default() const {
    if (theme_font) {
        theme_font->disconnect(CoreStringNames::get_singleton()->changed, callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
        theme_font.unref();
    }

    if (font_override) {
        return font_override;
    }

    // Check the project-defined Theme resource.
    if (Theme::get_project_default()) {
        Vector<StringName> theme_types;
        Theme::get_project_default()->get_type_dependencies(get_class_name(), StringName(), &theme_types);

        for (const StringName &s : theme_types) {
            if (Theme::get_project_default()->has_theme_item(Theme::DATA_TYPE_FONT, "font", s)) {
                Ref<Font> f = Theme::get_project_default()->get_theme_item(Theme::DATA_TYPE_FONT, "font", s);
                if (f) {
                    theme_font = f;
                    theme_font->connect(CoreStringNames::get_singleton()->changed, callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
                }
                return f;
            }
        }
    }

    // Lastly, fall back on the items defined in the default Theme, if they exist.
    {
        Vector<StringName> theme_types;
        Theme::get_default()->get_type_dependencies(get_class_name(), StringName(), &theme_types);

        for (const StringName &s : theme_types) {
            if (Theme::get_default()->has_theme_item(Theme::DATA_TYPE_FONT, "font", s)) {
                Ref<Font> f = Theme::get_default()->get_theme_item(Theme::DATA_TYPE_FONT, "font", s);
                if (f) {
                    theme_font = f;
                    theme_font->connect(CoreStringNames::get_singleton()->changed, callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
                }
                return f;
            }
        }
    }

    // If they don't exist, use any type to return the default/empty value.
    Ref<Font> f = Theme::get_default()->get_theme_item(Theme::DATA_TYPE_FONT, "font", StringName());
    if (f) {
        theme_font = f;
        theme_font->connect(CoreStringNames::get_singleton()->changed, callable_mp(const_cast<Label3D *>(this), &Label3D::_font_changed));
    }
    return f;
}

void Label3D::set_modulate(const Color &p_color) {
    if (modulate != p_color) {
        modulate = p_color;
        _queue_update();
    }
}

Color Label3D::get_modulate() const {
    return modulate;
}

void Label3D::set_outline_modulate(const Color &p_color) {
    if (outline_modulate != p_color) {
        outline_modulate = p_color;
        _queue_update();
    }
}

Color Label3D::get_outline_modulate() const {
    return outline_modulate;
}

void Label3D::set_autowrap(bool p_autowrap) {
    if (autowrap != p_autowrap) {
        autowrap = p_autowrap;
        word_cache_dirty = true;
        _queue_update();
    }
}

bool Label3D::get_autowrap() const {
    return autowrap;
}

void Label3D::set_width(float p_width) {
    if (width != p_width) {
        width = p_width;
        word_cache_dirty = true;
        _queue_update();
    }
}

float Label3D::get_width() const {
    return width;
}

void Label3D::set_pixel_size(real_t p_amount) {
    if (pixel_size != p_amount) {
        pixel_size = p_amount;
        _queue_update();
    }
}

real_t Label3D::get_pixel_size() const {
    return pixel_size;
}

void Label3D::set_offset(const Point2 &p_offset) {
    if (lbl_offset != p_offset) {
        lbl_offset = p_offset;
        _queue_update();
    }
}

Point2 Label3D::get_offset() const {
    return lbl_offset;
}

void Label3D::set_line_spacing(float p_line_spacing) {
    if (line_spacing != p_line_spacing) {
        line_spacing = p_line_spacing;
        _queue_update();
    }
}

float Label3D::get_line_spacing() const {
    return line_spacing;
}

void Label3D::set_draw_flag(DrawFlags p_flag, bool p_enable) {
    ERR_FAIL_INDEX(p_flag, FLAG_MAX);
    if (flags[p_flag] != p_enable) {
        flags[p_flag] = p_enable;
        _queue_update();
    }
}

bool Label3D::get_draw_flag(DrawFlags p_flag) const {
    ERR_FAIL_INDEX_V(p_flag, FLAG_MAX, false);
    return flags[p_flag];
}

void Label3D::set_billboard_mode(SpatialMaterial::BillboardMode p_mode) {
    ERR_FAIL_INDEX(p_mode, 3);
    if (billboard_mode != p_mode) {
        billboard_mode = p_mode;
        _queue_update();
    }
}

SpatialMaterial::BillboardMode Label3D::get_billboard_mode() const {
    return billboard_mode;
}

void Label3D::set_alpha_cut_mode(AlphaCutMode p_mode) {
    ERR_FAIL_INDEX(p_mode, 3);
    if (alpha_cut != p_mode) {
        alpha_cut = p_mode;
        _queue_update();
    }
}

Label3D::AlphaCutMode Label3D::get_alpha_cut_mode() const {
    return alpha_cut;
}

void Label3D::set_alpha_scissor_threshold(float p_threshold) {
    if (alpha_scissor_threshold != p_threshold) {
        alpha_scissor_threshold = p_threshold;
        _queue_update();
    }
}

float Label3D::get_alpha_scissor_threshold() const {
    return alpha_scissor_threshold;
}

Label3D::Label3D() {
    for (int i = 0; i < FLAG_MAX; i++) {
        flags[i] = (i == FLAG_DOUBLE_SIDED);
    }

    mesh = RID_PRIME(RenderingServer::get_singleton()->mesh_create());

    set_base(mesh);
}

Label3D::~Label3D() {
    RenderingServer *rs = RenderingServer::get_singleton();
    while (word_cache) {
        WordCache *current = word_cache;
        word_cache = current->next;
        memdelete(current);
    }

    rs->free_rid(mesh);
    for(auto &surf : surfaces) {
        rs->free_rid(surf.second.material);
    }
    surfaces.clear();
}
