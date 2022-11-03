/*************************************************************************/
/*  editor_fonts.cpp                                                     */
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

#include "editor_fonts.h"

//#include "builtin_fonts.gen.h"
#include "core/os/dir_access.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "scene/resources/default_theme/default_theme.h"
#include "scene/resources/dynamic_font.h"

#include <QDir>
#include <QFile>
#include <QDebug>
#include <QResource>

namespace  {
struct FontHolder {
    static constexpr const char * m_font_data_locations[] = {
        ":/binary/fonts/NotoSansUI_Regular.woff2",
        ":/binary/fonts/NotoSansUI_Bold.woff2",
        ":/binary/fonts/DroidSansFallback.woff2",
        ":/binary/fonts/DroidSansJapanese.woff2",
        ":/binary/fonts/NotoNaskhArabicUI_Regular.woff2",
        ":/binary/fonts/NotoSansHebrew_Regular.woff2",
        ":/binary/fonts/NotoSansThaiUI_Regular.woff2",
        ":/binary/fonts/NotoSansDevanagariUI_Regular.woff2",
        ":/binary/fonts/Hack_Regular.woff2"
    };
    enum FontIndices {
        DefaultFont,
        DefaultFontBold,
        FontFallback,
        FontJapanese,
        FontArabic,
        FontHebrew,
        FontThai,
        FontHindi,
        FontSourceCode,
        FONT_COUNT
    };
    Ref<DynamicFontData> m_all_fonts[FONT_COUNT];

    constexpr const Ref<DynamicFontData> &get(FontIndices fi) {
        return m_all_fonts[fi];
    }
    void init(bool font_antialiased,DynamicFontData::Hinting font_hinting) {
        for(int i=0; i<FONT_COUNT; ++i)
        {
            Ref<DynamicFontData> &data(m_all_fonts[i]);
            QResource res(m_font_data_locations[i]);
            data = make_ref_counted<DynamicFontData>();
            data->set_antialiased(font_antialiased);
            data->set_hinting(font_hinting);
            data->set_font_ptr(res.data(),res.size());
            if(i!=FontSourceCode)
                data->set_force_autohinter(true); //just looks better..i think?
        }
    }
    void add_fallbacks(Ref<DynamicFont> &to_font) {
        to_font->add_fallback(m_all_fonts[FontArabic]);
        to_font->add_fallback(m_all_fonts[FontHebrew]);
        to_font->add_fallback(m_all_fonts[FontThai]);
        to_font->add_fallback(m_all_fonts[FontHindi]);
        to_font->add_fallback(m_all_fonts[FontJapanese]);
        to_font->add_fallback(m_all_fonts[FontFallback]);
    }
};

static Ref<DynamicFont>  make_def_font(FontHolder &holder,float size,FontHolder::FontIndices baseline,Ref<DynamicFontData> &CustomFont) {
    Ref<DynamicFont> m_name(make_ref_counted<DynamicFont>());
    m_name->set_size(size);
    m_name->set_use_filter(true);
    m_name->set_use_mipmaps(true);

    if (CustomFont) {
        m_name->set_font_data(CustomFont);
        m_name->add_fallback(holder.get(baseline));
    } else {
        m_name->set_font_data(holder.get(baseline));
    }
    m_name->set_spacing(DynamicFont::SPACING_TOP, -EDSCALE);
    m_name->set_spacing(DynamicFont::SPACING_BOTTOM, -EDSCALE);
    holder.add_fallbacks(m_name);
    return m_name;
}
} // end of anonymous namespace

// Enable filtering and mipmaps on the editor fonts to improve text appearance
// in editors that are zoomed in/out without having dedicated fonts to generate.
// This is the case in GraphEdit-based editors such as the visual script and
// visual shader editors.

// the custom spacings might only work with Noto Sans
#define MAKE_DEFAULT_FONT(m_name, m_size)                       \
    auto m_name = make_def_font(holder,m_size,FontHolder::DefaultFont,CustomFont);

#define MAKE_BOLD_FONT(m_name, m_size)                          \
    auto m_name = make_def_font(holder,m_size,FontHolder::DefaultFontBold,CustomFontBold);

#define MAKE_SOURCE_FONT(m_name, m_size)                        \
    auto m_name = make_def_font(holder,m_size,FontHolder::FontSourceCode,CustomFontSource);

void editor_register_fonts(const Ref<Theme>& p_theme) {
    DirAccess *dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    /* Custom font */

    bool font_antialiased = EditorSettings::get_singleton()->getT<bool>("interface/editor/font_antialiased");
    int font_hinting_setting = EditorSettings::get_singleton()->getT<int>("interface/editor/font_hinting");

    DynamicFontData::Hinting font_hinting;
    switch (font_hinting_setting) {
        case 0:
            // The "Auto" setting uses the setting that best matches the OS' font rendering:
            // - macOS doesn't use font hinting.
            // - Windows uses ClearType, which is in between "Light" and "Normal" hinting.
            // - Linux has configurable font hinting, but most distributions including Ubuntu default to "Light".
#ifdef OSX_ENABLED
            font_hinting = DynamicFontData::HINTING_NONE;
#else
            font_hinting = DynamicFontData::HINTING_LIGHT;
#endif
            break;
        case 1:
            font_hinting = DynamicFontData::HINTING_NONE;
            break;
        case 2:
            font_hinting = DynamicFontData::HINTING_LIGHT;
            break;
        default:
            font_hinting = DynamicFontData::HINTING_NORMAL;
            break;
    }

    String custom_font_path = EditorSettings::get_singleton()->getT<String>("interface/editor/main_font");
    Ref<DynamicFontData> CustomFont;
    if (custom_font_path.length() > 0 && dir->file_exists(custom_font_path)) {
        CustomFont = make_ref_counted<DynamicFontData>();
        CustomFont->set_antialiased(font_antialiased);
        CustomFont->set_hinting(font_hinting);
        CustomFont->set_font_path(custom_font_path);
        CustomFont->set_force_autohinter(true); //just looks better..i think?
    } else {
        EditorSettings::get_singleton()->set_manually("interface/editor/main_font", "");
    }

    /* Custom Bold font */

    String custom_font_path_bold = EditorSettings::get_singleton()->getT<String>("interface/editor/main_font_bold");
    Ref<DynamicFontData> CustomFontBold;
    if (custom_font_path_bold.length() > 0 && dir->file_exists(custom_font_path_bold)) {
        CustomFontBold = make_ref_counted<DynamicFontData>();
        CustomFontBold->set_antialiased(font_antialiased);
        CustomFontBold->set_hinting(font_hinting);
        CustomFontBold->set_font_path(custom_font_path_bold);
        CustomFontBold->set_force_autohinter(true); //just looks better..i think?
    } else {
        EditorSettings::get_singleton()->set_manually("interface/editor/main_font_bold", "");
    }

    /* Custom source code font */

    String custom_font_path_source = EditorSettings::get_singleton()->getT<String>("interface/editor/code_font");
    Ref<DynamicFontData> CustomFontSource;
    if (custom_font_path_source.length() > 0 && dir->file_exists(custom_font_path_source)) {
        CustomFontSource = make_ref_counted<DynamicFontData>();
        CustomFontSource->set_antialiased(font_antialiased);
        CustomFontSource->set_hinting(font_hinting);
        CustomFontSource->set_font_path(custom_font_path_source);
    } else {
        EditorSettings::get_singleton()->set_manually("interface/editor/code_font", "");
    }

    memdelete(dir);
    /* Droid Sans */

    FontHolder holder;
    holder.init(font_antialiased,font_hinting);


    int default_font_size = EDITOR_GET_T<int>("interface/editor/main_font_size") * EDSCALE;

    // Default font
    auto df = make_def_font(holder,default_font_size,FontHolder::DefaultFont,CustomFont);
    p_theme->set_default_theme_font(df);
    p_theme->set_font("main", "EditorFonts", df);

    // Bold font
    MAKE_BOLD_FONT(df_bold, default_font_size)
    p_theme->set_font("bold", "EditorFonts", df_bold);

    // Title font
    MAKE_BOLD_FONT(df_title, default_font_size + 2 * EDSCALE)
    p_theme->set_font("title", "EditorFonts", df_title);

    // Documentation fonts
    MAKE_DEFAULT_FONT(df_doc, EDITOR_GET_T<int>("text_editor/help/help_font_size") * EDSCALE)
    MAKE_BOLD_FONT(df_doc_bold, EDITOR_GET_T<int>("text_editor/help/help_font_size") * EDSCALE)
    MAKE_BOLD_FONT(df_doc_title, EDITOR_GET_T<int>("text_editor/help/help_title_font_size") * EDSCALE)
    MAKE_SOURCE_FONT(df_doc_code, EDITOR_GET_T<int>("text_editor/help/help_source_font_size") * EDSCALE)

    p_theme->set_font("doc", "EditorFonts", df_doc);
    p_theme->set_font("doc_bold", "EditorFonts", df_doc_bold);
    p_theme->set_font("doc_title", "EditorFonts", df_doc_title);

    p_theme->set_font("doc_source", "EditorFonts", df_doc_code);

    // Ruler font
    MAKE_DEFAULT_FONT(df_rulers, 8 * EDSCALE)
    p_theme->set_font("rulers", "EditorFonts", df_rulers);

    // Rotation widget font
    MAKE_DEFAULT_FONT(df_rotation_control, 14 * EDSCALE);
    p_theme->set_font("rotation_control", "EditorFonts", df_rotation_control);

    // Code font
    MAKE_SOURCE_FONT(df_code, EDITOR_GET_T<int>("interface/editor/code_font_size") * EDSCALE)
    p_theme->set_font("source", "EditorFonts", df_code);

    MAKE_SOURCE_FONT(df_expression, (EDITOR_GET_T<int>("interface/editor/code_font_size") - 1) * EDSCALE)
    p_theme->set_font("expression", "EditorFonts", df_expression);

    MAKE_SOURCE_FONT(df_output_code, EDITOR_GET_T<int>("run/output/font_size") * EDSCALE)
    p_theme->set_font("output_source", "EditorFonts", df_output_code);

    MAKE_SOURCE_FONT(df_text_editor_status_code, default_font_size)
    p_theme->set_font("status_source", "EditorFonts", df_text_editor_status_code);
}

