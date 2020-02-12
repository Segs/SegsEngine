/*************************************************************************/
/*  editor_themes.cpp                                                    */
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

#include "editor_themes.h"

#include "editor_fonts.h"
#include "editor_scale.h"
#include "editor_settings.h"

#include "core/io/resource_loader.h"
#include "core/io/image_loader.h"
#include "core/print_string.h"
#include "core/string_utils.inl"
#include "plugins/formats/image/svg/image_loader_svg.h"
#include "scene/resources/font.h"

#include <QDir>
#include <QDirIterator>
#include <QResource>
#include <ctime>

static Ref<StyleBoxTexture> make_stylebox(const Ref<Texture>& p_texture, float p_left, float p_top, float p_right, float p_botton, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_botton = -1, bool p_draw_center = true) {
    Ref<StyleBoxTexture> style(make_ref_counted<StyleBoxTexture>());
    style->set_texture(p_texture);
    style->set_margin_size(Margin::Left, p_left * EDSCALE);
    style->set_margin_size(Margin::Right, p_right * EDSCALE);
    style->set_margin_size(Margin::Bottom, p_botton * EDSCALE);
    style->set_margin_size(Margin::Top, p_top * EDSCALE);
    style->set_default_margin(Margin::Left, p_margin_left * EDSCALE);
    style->set_default_margin(Margin::Right, p_margin_right * EDSCALE);
    style->set_default_margin(Margin::Bottom, p_margin_botton * EDSCALE);
    style->set_default_margin(Margin::Top, p_margin_top * EDSCALE);
    style->set_draw_center(p_draw_center);
    return style;
}

static Ref<StyleBoxEmpty> make_empty_stylebox(float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1) {
    Ref<StyleBoxEmpty> style(make_ref_counted<StyleBoxEmpty>());
    style->set_default_margin(Margin::Left, p_margin_left * EDSCALE);
    style->set_default_margin(Margin::Right, p_margin_right * EDSCALE);
    style->set_default_margin(Margin::Bottom, p_margin_bottom * EDSCALE);
    style->set_default_margin(Margin::Top, p_margin_top * EDSCALE);
    return style;
}

static Ref<StyleBoxFlat> make_flat_stylebox(Color p_color, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1) {
    Ref<StyleBoxFlat> style(make_ref_counted<StyleBoxFlat>());
    style->set_bg_color(p_color);
    style->set_default_margin(Margin::Left, p_margin_left * EDSCALE);
    style->set_default_margin(Margin::Right, p_margin_right * EDSCALE);
    style->set_default_margin(Margin::Bottom, p_margin_bottom * EDSCALE);
    style->set_default_margin(Margin::Top, p_margin_top * EDSCALE);
    return style;
}

static Ref<StyleBoxLine> make_line_stylebox(Color p_color, int p_thickness = 1, float p_grow_begin = 1, float p_grow_end = 1, bool p_vertical = false) {
    Ref<StyleBoxLine> style(make_ref_counted<StyleBoxLine>());
    style->set_color(p_color);
    style->set_grow_begin(p_grow_begin);
    style->set_grow_end(p_grow_end);
    style->set_thickness(p_thickness);
    style->set_vertical(p_vertical);
    return style;
}

static Ref<ImageTexture> editor_generate_icon(const QString &resource_path,bool p_convert_color, float p_scale = EDSCALE, bool p_force_filter = false) {

    Ref<ImageTexture> icon(make_ref_counted<ImageTexture>());
    Ref<Image> img(make_ref_counted<Image>());
    QFile resource_file(resource_path);
    bool openened = resource_file.open(QFile::ReadOnly);
    if(!openened)
        return Ref<ImageTexture>();
    // dumb gizmo check
    bool is_gizmo = QFileInfo(resource_path).baseName().startsWith("Gizmo");
    QByteArray resource_data = resource_file.readAll();
    // Upsample icon generation only if the editor scale isn't an integer multiplier.
    // Generating upsampled icons is slower, and the benefit is hardly visible
    // with integer editor scales.
    const bool upsample = !Math::is_equal_approx(Math::round(p_scale), p_scale);

    LoadParams svg_load = {p_scale, false, upsample, p_convert_color };

    img->create(ImageLoader::load_image("svg",(const uint8_t *)resource_data.data(),resource_data.size(),svg_load));

    if (p_scale - (float)(int)p_scale > 0.0f || is_gizmo || p_force_filter)
        icon->create_from_image(img); // in this case filter really helps
    else
        icon->create_from_image(img, 0);

    return icon;
}

#ifndef ADD_CONVERT_COLOR
#define ADD_CONVERT_COLOR(dictionary, old_color, new_color) dictionary.emplace_back(Color::html(old_color),Color::html(new_color))
#endif

static void editor_register_and_generate_icons(

    const Ref<Theme> &p_theme, bool p_dark_theme = true, int p_thumb_size = 32, bool p_only_thumbs = false) {
    ImageFormatLoader * loader= ImageLoader::recognize("svg");
    if (loader) {
        // The default icon theme is designed to be used for a dark theme.
        // This dictionary stores color codes to convert to other colors
        // for better readability on a light theme.
        //Dictionary dark_icon_color_dictionary;
        Vector<eastl::pair<Color,Color>> dark_icon_color_dictionary;
        // The names of the icons to never convert, even if one of their colors
        // are contained in the dictionary above.
        Set<se_string_view> exceptions;

        if (!p_dark_theme) {
            dark_icon_color_dictionary.reserve(100);
            // convert color:                              FROM       TO
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e0e0e0", "#5a5a5a"); // common icon color
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffffff", "#414141"); // white
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#b4b4b4", "#363636"); // script darker color
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f9f9f9", "#606060"); // scrollbar grabber highlight color

            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#cea4f1", "#a85de9"); // animation
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#fc9c9c", "#cd3838"); // spatial
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#a5b7f3", "#3d64dd"); // 2d
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#708cea", "#1a3eac"); // 2d dark
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#a5efac", "#2fa139"); // control

            // rainbow
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff7070", "#ff2929"); // red
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffeb70", "#ffe337"); // yellow
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#9dff70", "#74ff34"); // green
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#70ffb9", "#2cff98"); // aqua
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#70deff", "#22ccff"); // blue
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#9f70ff", "#702aff"); // purple
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff70ac", "#ff2781"); // pink

            // audio gradient
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff8484", "#ff4040"); // red
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e1dc7a", "#d6cf4b"); // yellow
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84ffb1", "#00f010"); // green

            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ffd684", "#fea900"); // mesh (orange)
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#40a2ff", "#68b6ff"); // shape (blue)

            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ff8484", "#ff3333"); // remove (red)
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84ffb1", "#00db50"); // add (green)
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#84c2ff", "#5caeff"); // selection (blue)

            // Animation editor tracks
            // The property track icon color is set by the common icon color
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ea9568", "#bd5e2c"); // 3D Transform track
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#66f376", "#16a827"); // Call Method track
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#5792f6", "#236be6"); // Bezier Curve track
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#eae668", "#9f9722"); // Audio Playback track
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#b76ef0", "#9853ce"); // Animation Playback track

            // TileSet editor icons
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#fce844", "#aa8d24"); // New Single Tile
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#4490fc", "#0350bd"); // New Autotile
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#c9cfd4", "#828f9b"); // New Atlas
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#69ecbd", "#25e3a0"); // VS variant
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#8da6f0", "#6d8eeb"); // VS bool
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#7dc6ef", "#4fb2e9"); // VS int
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#61daf4", "#27ccf0"); // VS float
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#6ba7ec", "#4690e7"); // VS string
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#bd91f1", "#ad76ee"); // VS vector2
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f191a5", "#ee758e"); // VS rect
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e286f0", "#dc6aed"); // VS vector3
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#c4ec69", "#96ce1a"); // VS transform2D
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f77070", "#f77070"); // VS plane
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ec69a3", "#ec69a3"); // VS quat
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#ee7991", "#ee7991"); // VS aabb
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#e3ec69", "#b2bb19"); // VS basis
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#f6a86e", "#f49047"); // VS transform
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#6993ec", "#6993ec"); // VS path
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#69ec9a", "#2ce573"); // VS rid
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#79f3e8", "#12d5c3"); // VS object
            ADD_CONVERT_COLOR(dark_icon_color_dictionary, "#77edb1", "#57e99f"); // VS dict

            exceptions.insert("EditorPivot");
            exceptions.insert("EditorHandle");
            exceptions.insert("Editor3DHandle");
            exceptions.insert("Godot");
            exceptions.insert("PanoramaSky");
            exceptions.insert("ProceduralSky");
            exceptions.insert("EditorControlAnchor");
            exceptions.insert("DefaultProjectIcon");
            exceptions.insert("GuiCloseCustomizable");
            exceptions.insert("GuiGraphNodePort");
            exceptions.insert("GuiResizer");
            exceptions.insert("ZoomMore");
            exceptions.insert("ZoomLess");
            exceptions.insert("ZoomReset");
            exceptions.insert("LockViewport");
            exceptions.insert("GroupViewport");
            exceptions.insert("StatusError");
            exceptions.insert("StatusSuccess");
            exceptions.insert("StatusWarning");
            exceptions.insert("NodeWarning");
            exceptions.insert("OverbrightIndicator");
        }

        // These ones should be converted even if we are using a dark theme.
        const Color error_color = p_theme->get_color("error_color", "Editor");
        const Color success_color = p_theme->get_color("success_color", "Editor");
        const Color warning_color = p_theme->get_color("warning_color", "Editor");
        dark_icon_color_dictionary.emplace_back(Color::html("#ff0000"),error_color);
        dark_icon_color_dictionary.emplace_back(Color::html("#45ff8b"),success_color);
        dark_icon_color_dictionary.emplace_back(Color::html("#dbab09"),warning_color);
        // Setup svg color conversion
        loader->set_loader_option(0,&dark_icon_color_dictionary);

        // generate icons
        if (!p_only_thumbs) {
            QDirIterator embedded_icons(":/icons", { "*.svg" });
            while (embedded_icons.hasNext()) {
                const QString resourcepath = embedded_icons.next();
                const QString base_name = embedded_icons.fileInfo().baseName();
                auto is_exception = exceptions.contains(qPrintable(base_name));
                Ref<ImageTexture> icon = editor_generate_icon(resourcepath, !is_exception);
                p_theme->set_icon(StringName(StringUtils::to_utf8(base_name)), "EditorIcons", icon);
            }
        }

        // Generate thumbnail icons with the given thumbnail size.
        // We don't need filtering when generating at one of the default resolutions.
        const bool force_filter = p_thumb_size != 64 && p_thumb_size != 32;
        if (p_thumb_size >= 64) {
            const float scale = (float)p_thumb_size / 64.0f * EDSCALE;
            QDirIterator embedded_icons(":/icons/big_thumbs", { "*.svg" });
            while (embedded_icons.hasNext()) {
                const QString resourcepath = embedded_icons.next();
                const QString base_name = embedded_icons.fileInfo().baseName();
                auto is_exception = exceptions.contains(qPrintable(base_name));
                const Ref<ImageTexture> icon =
                        editor_generate_icon(resourcepath, !p_dark_theme && !is_exception, scale, force_filter);
                p_theme->set_icon(StringName(StringUtils::to_utf8(base_name)), "EditorIcons", icon);
            }
        } else {
            float scale = (float)p_thumb_size / 32.0f * EDSCALE;
            QDirIterator embedded_icons(":/icons/medium_thumbs", { "*.svg" });
            while (embedded_icons.hasNext()) {
                const QString resourcepath = embedded_icons.next();
                const QString base_name = embedded_icons.fileInfo().baseName();
                auto is_exception = exceptions.contains(qPrintable(base_name));
                Ref<ImageTexture> icon =
                        editor_generate_icon(resourcepath, !p_dark_theme && !is_exception, scale, force_filter);
                p_theme->set_icon(StringName(StringUtils::to_utf8(base_name)), "EditorIcons", icon);
            }
        }
        // Reset svg color conversion
        loader->set_loader_option(0,nullptr);
    } else {
        print_line("SVG plugin disabled, editor icons won't be rendered.");
    }
}

Ref<Theme> create_editor_theme(const Ref<Theme>& p_theme) {

    Ref<Theme> theme(make_ref_counted<Theme>());

    const float default_contrast = 0.25f;

    //Theme settings
    Color accent_color = EDITOR_GET("interface/theme/accent_color");
    Color base_color = EDITOR_GET("interface/theme/base_color");
    float contrast = EDITOR_GET("interface/theme/contrast");
    float relationship_line_opacity = EDITOR_GET("interface/theme/relationship_line_opacity");

    String preset = EDITOR_GET("interface/theme/preset");

    bool highlight_tabs = EDITOR_GET("interface/theme/highlight_tabs");
    int border_size = EDITOR_GET("interface/theme/border_size");

    bool use_gn_headers = EDITOR_GET("interface/theme/use_graph_node_headers");

    Color preset_accent_color;
    Color preset_base_color;
    float preset_contrast = 0;

    // Please, use alphabet order if you've added new theme here(After "Default" and "Custom")

    if (preset == "Default") {
        preset_accent_color = Color(0.41f, 0.61f, 0.91f);
        preset_base_color = Color(0.2f, 0.23f, 0.31f);
            preset_contrast = default_contrast;
    } else if (preset == "Custom") {
        accent_color = EDITOR_GET("interface/theme/accent_color");
        base_color = EDITOR_GET("interface/theme/base_color");
        contrast = EDITOR_GET("interface/theme/contrast");
    } else if (preset == "Alien") {
        preset_accent_color = Color(0.11f, 1.0f, 0.6f);
        preset_base_color = Color(0.18f, 0.22f, 0.25f);
            preset_contrast = 0.25f;
    } else if (preset == "Arc") {
        preset_accent_color = Color(0.32f, 0.58f, 0.89f);
        preset_base_color = Color(0.22f, 0.24f, 0.29f);
            preset_contrast = 0.25f;
    } else if (preset == "Godot 2") {
        preset_accent_color = Color(0.53f, 0.67f, 0.89f);
        preset_base_color = Color(0.24f, 0.23f, 0.27f);
        preset_contrast = 0.25f;
    } else if (preset == "Grey") {
        preset_accent_color = Color(0.72f, 0.89f, 1.0f);
        preset_base_color = Color(0.24f, 0.24f, 0.24f);
        preset_contrast = 0.2f;
    } else if (preset == "Light") {
        preset_accent_color = Color(0.13f, 0.44f, 1.0f);
        preset_base_color = Color(1, 1, 1);
            preset_contrast = 0.08f;
    } else if (preset == "Solarized (Dark)") {
        preset_accent_color = Color(0.15f, 0.55f, 0.82f);
        preset_base_color = Color(0.03f, 0.21f, 0.26f);
        preset_contrast = 0.23f;
    } else if (preset == "Solarized (Light)") {
        preset_accent_color = Color(0.15f, 0.55f, 0.82f);
        preset_base_color = Color(0.99f, 0.96f, 0.89f);
            preset_contrast = 0.06f;
    } else { // Default
        preset_accent_color = Color(0.41f, 0.61f, 0.91f);
        preset_base_color = Color(0.2f, 0.23f, 0.31f);
        preset_contrast = default_contrast;
    }

    if (preset != "Custom") {
        accent_color = preset_accent_color;
        base_color = preset_base_color;
        contrast = preset_contrast;
        EditorSettings::get_singleton()->set_initial_value("interface/theme/accent_color", accent_color);
        EditorSettings::get_singleton()->set_initial_value("interface/theme/base_color", base_color);
        EditorSettings::get_singleton()->set_initial_value("interface/theme/contrast", contrast);
    }
    EditorSettings::get_singleton()->set_manually("interface/theme/preset", preset);
    EditorSettings::get_singleton()->set_manually("interface/theme/accent_color", accent_color);
    EditorSettings::get_singleton()->set_manually("interface/theme/base_color", base_color);
    EditorSettings::get_singleton()->set_manually("interface/theme/contrast", contrast);

    //Colors
    bool dark_theme = EditorSettings::get_singleton()->is_dark_theme();

    const Color dark_color_1 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast);
    const Color dark_color_2 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast * 1.5f);
    const Color dark_color_3 = base_color.linear_interpolate(Color(0, 0, 0, 1), contrast * 2);

    const Color background_color = dark_color_2;

    // white (dark theme) or black (light theme), will be used to generate the rest of the colors
    const Color mono_color = dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);

    const Color contrast_color_1 = base_color.linear_interpolate(mono_color, MAX(contrast, default_contrast));
    const Color contrast_color_2 = base_color.linear_interpolate(mono_color, MAX(contrast * 1.5f, default_contrast * 1.5f));

    const Color font_color = mono_color.linear_interpolate(base_color, 0.25f);
    const Color font_color_hl = mono_color.linear_interpolate(base_color, 0.15f);
    const Color font_color_disabled = Color(mono_color.r, mono_color.g, mono_color.b, 0.3f);
    const Color font_color_selection = accent_color * Color(1, 1, 1, 0.4f);
    const Color color_disabled = mono_color.inverted().linear_interpolate(base_color, 0.7f);
    const Color color_disabled_bg = mono_color.inverted().linear_interpolate(base_color, 0.9f);

    Color icon_color_hover = Color(1, 1, 1) * (dark_theme ? 1.15f : 1.45f);
    icon_color_hover.a = 1.0f;
    // Make the pressed icon color overbright because icons are not completely white on a dark theme.
    // On a light theme, icons are dark, so we need to modulate them with an even brighter color.
    Color icon_color_pressed = accent_color * (dark_theme ? 1.15f : 3.5f);
    icon_color_pressed.a = 1.0f;

    const Color separator_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.1f);

    const Color highlight_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.2f);
    const Theme::ThemeColor generic_colors[] = {
        {"accent_color", "Editor", accent_color },
        {"highlight_color", "Editor", highlight_color },
        {"base_color", "Editor", base_color },
        {"dark_color_1", "Editor", dark_color_1 },
        {"dark_color_2", "Editor", dark_color_2 },
        {"dark_color_3", "Editor", dark_color_3 },
        {"contrast_color_1", "Editor", contrast_color_1 },
        {"contrast_color_2", "Editor", contrast_color_2 },
        {"box_selection_fill_color", "Editor", accent_color * Color(1, 1, 1, 0.3f) },
        {"box_selection_stroke_color", "Editor", accent_color * Color(1, 1, 1, 0.8f) },

        {"axis_x_color", "Editor", Color(0.96f, 0.20f, 0.32f) },
        {"axis_y_color", "Editor", Color(0.53f, 0.84f, 0.01f) },
        {"axis_z_color", "Editor", Color(0.16f, 0.55f, 0.96f) },

        {"font_color", "Editor", font_color },
        {"highlighted_font_color", "Editor", font_color_hl },
        {"disabled_font_color", "Editor", font_color_disabled },

        {"mono_color", "Editor", mono_color },

    };

    theme->set_colors(generic_colors);

    Color success_color = Color(0.45f, 0.95f, 0.5f);
    Color warning_color = Color(1, 0.87f, 0.4f);
    Color error_color = Color(1, 0.47f, 0.42f);
    Color property_color = font_color.linear_interpolate(Color(0.5f, 0.5f, 0.5f), 0.5f);
    if (!dark_theme) {
        // Darken some colors to be readable on a light background
        success_color = success_color.linear_interpolate(mono_color, 0.35f);
        warning_color = warning_color.linear_interpolate(mono_color, 0.35f);
        error_color = error_color.linear_interpolate(mono_color, 0.25f);
    }
    theme->set_color("success_color", "Editor", success_color);
    theme->set_color("warning_color", "Editor", warning_color);
    theme->set_color("error_color", "Editor", error_color);
    theme->set_color("property_color", "Editor", property_color);

    const int thumb_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
    theme->set_constant("scale", "Editor", EDSCALE);
    theme->set_constant("thumb_size", "Editor", thumb_size);
    theme->set_constant("dark_theme", "Editor", dark_theme);

    //Register icons + font

    // the resolution and the icon color (dark_theme bool) has not changed, so we do not regenerate the icons
    if (p_theme != nullptr && std::abs(p_theme->get_constant("scale", "Editor") - EDSCALE) < 0.00001f &&
            p_theme->get_constant("dark_theme", "Editor") == dark_theme) {
        // register already generated icons
        QDirIterator embedded_icons(":/icons",{"*.svg"},QDir::NoFilter,QDirIterator::Subdirectories);
        while(embedded_icons.hasNext()) {
            embedded_icons.next();
            const String basename = StringUtils::to_utf8(embedded_icons.fileInfo().baseName());
            theme->set_icon(StringName(basename), "EditorIcons", p_theme->get_icon(StringName(basename), "EditorIcons"));
        }
    } else {
        editor_register_and_generate_icons(theme, dark_theme, thumb_size);
    }
    // thumbnail size has changed, so we regenerate the medium sizes
    if (p_theme != nullptr && fabs((double)p_theme->get_constant("thumb_size", "Editor") - thumb_size) > 0.00001f) {
        editor_register_and_generate_icons(p_theme, dark_theme, thumb_size, true);
    }

    editor_register_fonts(theme);

    // Highlighted tabs and border width
    Color tab_color = highlight_tabs ? base_color.linear_interpolate(font_color, contrast) : base_color;
    const int border_width = CLAMP(border_size, 0, 3) * EDSCALE;

    const int default_margin_size = 4;
    const int margin_size_extra = default_margin_size + CLAMP(border_size, 0, 3);

    // styleboxes
    // this is the most commonly used stylebox, variations should be made as duplicate of this
    Ref<StyleBoxFlat> style_default = make_flat_stylebox(base_color, default_margin_size, default_margin_size, default_margin_size, default_margin_size);
    style_default->set_border_width_all(border_width);
    style_default->set_border_color(base_color);
    style_default->set_draw_center(true);

    // Button and widgets
    const float extra_spacing = EDITOR_GET("interface/theme/additional_spacing");

    Ref<StyleBoxFlat> style_widget = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_widget->set_default_margin(Margin::Left, (extra_spacing + 6) * EDSCALE);
    style_widget->set_default_margin(Margin::Top, (extra_spacing + default_margin_size) * EDSCALE);
    style_widget->set_default_margin(Margin::Right, (extra_spacing + 6) * EDSCALE);
    style_widget->set_default_margin(Margin::Bottom, (extra_spacing + default_margin_size) * EDSCALE);
    style_widget->set_bg_color(dark_color_1);
    style_widget->set_border_color(dark_color_2);

    Ref<StyleBoxFlat> style_widget_disabled = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_widget_disabled->set_border_color(color_disabled);
    style_widget_disabled->set_bg_color(color_disabled_bg);

    Ref<StyleBoxFlat> style_widget_focus = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_widget_focus->set_border_color(accent_color);

    Ref<StyleBoxFlat> style_widget_pressed = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_widget_pressed->set_border_color(accent_color);

    Ref<StyleBoxFlat> style_widget_hover = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_widget_hover->set_border_color(contrast_color_1);

    // style for windows, popups, etc..
    Ref<StyleBoxFlat> style_popup = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    const int popup_margin_size = default_margin_size * EDSCALE * 2;
    style_popup->set_default_margin(Margin::Left, popup_margin_size);
    style_popup->set_default_margin(Margin::Top, popup_margin_size);
    style_popup->set_default_margin(Margin::Right, popup_margin_size);
    style_popup->set_default_margin(Margin::Bottom, popup_margin_size);
    style_popup->set_border_color(contrast_color_1);
    style_popup->set_border_width_all(MAX(EDSCALE, border_width));
    const Color shadow_color = Color(0, 0, 0, dark_theme ? 0.3f : 0.1f);
    style_popup->set_shadow_color(shadow_color);
    style_popup->set_shadow_size(4 * EDSCALE);

    Ref<StyleBoxLine> style_popup_separator(make_ref_counted<StyleBoxLine>());
    style_popup_separator->set_color(separator_color);
    style_popup_separator->set_grow_begin(popup_margin_size - MAX(EDSCALE, border_width));
    style_popup_separator->set_grow_end(popup_margin_size - MAX(EDSCALE, border_width));
    style_popup_separator->set_thickness(MAX(EDSCALE, border_width));

    Ref<StyleBoxLine> style_popup_labeled_separator_left(make_ref_counted<StyleBoxLine>());
    style_popup_labeled_separator_left->set_grow_begin(popup_margin_size - MAX(EDSCALE, border_width));
    style_popup_labeled_separator_left->set_color(separator_color);
    style_popup_labeled_separator_left->set_thickness(MAX(EDSCALE, border_width));

    Ref<StyleBoxLine> style_popup_labeled_separator_right(make_ref_counted<StyleBoxLine>());
    style_popup_labeled_separator_right->set_grow_end(popup_margin_size - MAX(EDSCALE, border_width));
    style_popup_labeled_separator_right->set_color(separator_color);
    style_popup_labeled_separator_right->set_thickness(MAX(EDSCALE, border_width));
    Ref<StyleBoxEmpty> style_empty = make_empty_stylebox(default_margin_size, default_margin_size, default_margin_size, default_margin_size);

    // Tabs

    const int tab_default_margin_side = 10 * EDSCALE + extra_spacing * EDSCALE;
    const int tab_default_margin_vertical = 5 * EDSCALE + extra_spacing * EDSCALE;

    Ref<StyleBoxFlat> style_tab_selected = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());

    style_tab_selected->set_border_width_all(border_width);
    style_tab_selected->set_border_width(Margin::Bottom, 0);
    style_tab_selected->set_border_color(dark_color_3);
    style_tab_selected->set_expand_margin_size(Margin::Bottom, border_width);
    style_tab_selected->set_default_margin(Margin::Left, tab_default_margin_side);
    style_tab_selected->set_default_margin(Margin::Right, tab_default_margin_side);
    style_tab_selected->set_default_margin(Margin::Bottom, tab_default_margin_vertical);
    style_tab_selected->set_default_margin(Margin::Top, tab_default_margin_vertical);
    style_tab_selected->set_bg_color(tab_color);

    Ref<StyleBoxFlat> style_tab_unselected = dynamic_ref_cast<StyleBoxFlat>(style_tab_selected->duplicate());
    style_tab_unselected->set_bg_color(dark_color_1);
    style_tab_unselected->set_border_color(dark_color_2);

    Ref<StyleBoxFlat> style_tab_disabled = dynamic_ref_cast<StyleBoxFlat>(style_tab_selected->duplicate());
    style_tab_disabled->set_bg_color(color_disabled_bg);
    style_tab_disabled->set_border_color(color_disabled);

    // Editor background
    theme->set_stylebox("Background", "EditorStyles", make_flat_stylebox(background_color, default_margin_size, default_margin_size, default_margin_size, default_margin_size));

    // Focus
    Ref<StyleBoxFlat> style_focus = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_focus->set_draw_center(false);
    style_focus->set_border_color(contrast_color_2);
    theme->set_stylebox("Focus", "EditorStyles", style_focus);

    // Menu
    Ref<StyleBoxFlat> style_menu = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_menu->set_draw_center(false);
    style_menu->set_border_width_all(0);
    theme->set_stylebox("panel", "PanelContainer", style_menu);
    theme->set_stylebox("MenuPanel", "EditorStyles", style_menu);

    // Script Editor
    theme->set_stylebox("ScriptEditorPanel", "EditorStyles", make_empty_stylebox(default_margin_size, 0, default_margin_size, default_margin_size));
    theme->set_stylebox("ScriptEditor", "EditorStyles", make_empty_stylebox(0, 0, 0, 0));

    // Play button group
    theme->set_stylebox("PlayButtonPanel", "EditorStyles", style_empty);

    //MenuButton
    Ref<StyleBoxFlat> style_menu_hover_border = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_menu_hover_border->set_draw_center(false);
    style_menu_hover_border->set_border_width_all(0);
    style_menu_hover_border->set_border_width(Margin::Bottom, border_width);
    style_menu_hover_border->set_border_color(accent_color);

    Ref<StyleBoxFlat> style_menu_hover_bg = dynamic_ref_cast<StyleBoxFlat>(style_widget->duplicate());
    style_menu_hover_bg->set_border_width_all(0);
    style_menu_hover_bg->set_bg_color(dark_color_1);

    theme->set_stylebox("normal", "MenuButton", style_menu);
    theme->set_stylebox("hover", "MenuButton", style_menu);
    theme->set_stylebox("pressed", "MenuButton", style_menu);
    theme->set_stylebox("focus", "MenuButton", style_menu);
    theme->set_stylebox("disabled", "MenuButton", style_menu);

    theme->set_stylebox("normal", "PopupMenu", style_menu);
    theme->set_stylebox("hover", "PopupMenu", style_menu_hover_bg);
    theme->set_stylebox("pressed", "PopupMenu", style_menu);
    theme->set_stylebox("focus", "PopupMenu", style_menu);
    theme->set_stylebox("disabled", "PopupMenu", style_menu);

    theme->set_stylebox("normal", "ToolButton", style_menu);
    theme->set_stylebox("hover", "ToolButton", style_menu);
    theme->set_stylebox("pressed", "ToolButton", style_menu);
    theme->set_stylebox("focus", "ToolButton", style_menu);
    theme->set_stylebox("disabled", "ToolButton", style_menu);

    theme->set_color("font_color", "MenuButton", font_color);
    theme->set_color("font_color_hover", "MenuButton", font_color_hl);
    theme->set_color("font_color", "ToolButton", font_color);
    theme->set_color("font_color_hover", "ToolButton", font_color_hl);
    theme->set_color("font_color_pressed", "ToolButton", accent_color);

    theme->set_stylebox("MenuHover", "EditorStyles", style_menu_hover_border);

    // Buttons
    theme->set_stylebox("normal", "Button", style_widget);
    theme->set_stylebox("hover", "Button", style_widget_hover);
    theme->set_stylebox("pressed", "Button", style_widget_pressed);
    theme->set_stylebox("focus", "Button", style_widget_focus);
    theme->set_stylebox("disabled", "Button", style_widget_disabled);

    theme->set_color("font_color", "Button", font_color);
    theme->set_color("font_color_hover", "Button", font_color_hl);
    theme->set_color("font_color_pressed", "Button", accent_color);
    theme->set_color("font_color_disabled", "Button", font_color_disabled);
    theme->set_color("icon_color_hover", "Button", icon_color_hover);
    theme->set_color("icon_color_pressed", "Button", icon_color_pressed);

    // OptionButton
    theme->set_stylebox("normal", "OptionButton", style_widget);
    theme->set_stylebox("hover", "OptionButton", style_widget_hover);
    theme->set_stylebox("pressed", "OptionButton", style_widget_pressed);
    theme->set_stylebox("focus", "OptionButton", style_widget_focus);
    theme->set_stylebox("disabled", "OptionButton", style_widget_disabled);

    theme->set_color("font_color", "OptionButton", font_color);
    theme->set_color("font_color_hover", "OptionButton", font_color_hl);
    theme->set_color("font_color_pressed", "OptionButton", accent_color);
    theme->set_color("font_color_disabled", "OptionButton", font_color_disabled);
    theme->set_color("icon_color_hover", "OptionButton", icon_color_hover);
    theme->set_icon("arrow", "OptionButton", theme->get_icon("GuiOptionArrow", "EditorIcons"));
    theme->set_constant("arrow_margin", "OptionButton", default_margin_size * EDSCALE);
    theme->set_constant("modulate_arrow", "OptionButton", true);
    theme->set_constant("hseparation", "OptionButton", 4 * EDSCALE);

    // CheckButton
    theme->set_stylebox("normal", "CheckButton", style_menu);
    theme->set_stylebox("pressed", "CheckButton", style_menu);
    theme->set_stylebox("disabled", "CheckButton", style_menu);
    theme->set_stylebox("hover", "CheckButton", style_menu);

    theme->set_icon("on", "CheckButton", theme->get_icon("GuiToggleOn", "EditorIcons"));
    theme->set_icon("on_disabled", "CheckButton", theme->get_icon("GuiToggleOnDisabled", "EditorIcons"));
    theme->set_icon("off", "CheckButton", theme->get_icon("GuiToggleOff", "EditorIcons"));
    theme->set_icon("off_disabled", "CheckButton", theme->get_icon("GuiToggleOffDisabled", "EditorIcons"));

    theme->set_color("font_color", "CheckButton", font_color);
    theme->set_color("font_color_hover", "CheckButton", font_color_hl);
    theme->set_color("font_color_pressed", "CheckButton", accent_color);
    theme->set_color("font_color_disabled", "CheckButton", font_color_disabled);
    theme->set_color("icon_color_hover", "CheckButton", icon_color_hover);

    theme->set_constant("hseparation", "CheckButton", 4 * EDSCALE);
    theme->set_constant("check_vadjust", "CheckButton", 0 * EDSCALE);

    // Checkbox
    Ref<StyleBoxFlat> sb_checkbox = dynamic_ref_cast<StyleBoxFlat>(style_menu->duplicate());
    sb_checkbox->set_default_margin(Margin::Left, default_margin_size * EDSCALE);
    sb_checkbox->set_default_margin(Margin::Right, default_margin_size * EDSCALE);
    sb_checkbox->set_default_margin(Margin::Top, default_margin_size * EDSCALE);
    sb_checkbox->set_default_margin(Margin::Bottom, default_margin_size * EDSCALE);

    theme->set_stylebox("normal", "CheckBox", sb_checkbox);
    theme->set_stylebox("pressed", "CheckBox", sb_checkbox);
    theme->set_stylebox("disabled", "CheckBox", sb_checkbox);
    theme->set_stylebox("hover", "CheckBox", sb_checkbox);
    theme->set_icon("checked", "CheckBox", theme->get_icon("GuiChecked", "EditorIcons"));
    theme->set_icon("unchecked", "CheckBox", theme->get_icon("GuiUnchecked", "EditorIcons"));
    theme->set_icon("radio_checked", "CheckBox", theme->get_icon("GuiRadioChecked", "EditorIcons"));
    theme->set_icon("radio_unchecked", "CheckBox", theme->get_icon("GuiRadioUnchecked", "EditorIcons"));

    theme->set_color("font_color", "CheckBox", font_color);
    theme->set_color("font_color_hover", "CheckBox", font_color_hl);
    theme->set_color("font_color_pressed", "CheckBox", accent_color);
    theme->set_color("font_color_disabled", "CheckBox", font_color_disabled);
    theme->set_color("icon_color_hover", "CheckBox", icon_color_hover);

    theme->set_constant("hseparation", "CheckBox", 4 * EDSCALE);
    theme->set_constant("check_vadjust", "CheckBox", 0 * EDSCALE);

    // PopupDialog
    theme->set_stylebox("panel", "PopupDialog", style_popup);

    // PopupMenu
    theme->set_stylebox("panel", "PopupMenu", style_popup);
    theme->set_stylebox("separator", "PopupMenu", style_popup_separator);
    theme->set_stylebox("labeled_separator_left", "PopupMenu", style_popup_labeled_separator_left);
    theme->set_stylebox("labeled_separator_right", "PopupMenu", style_popup_labeled_separator_right);
    theme->set_color("font_color", "PopupMenu", font_color);
    theme->set_color("font_color_hover", "PopupMenu", font_color_hl);
    theme->set_color("font_color_accel", "PopupMenu", font_color_disabled);
    theme->set_color("font_color_disabled", "PopupMenu", font_color_disabled);

    const Theme::ThemeIcon popup_icons[] = {
        {"checked","GuiChecked","EditorIcons"},
        {"unchecked","GuiUnchecked","EditorIcons"},
        {"radio_checked","GuiRadioChecked","EditorIcons"},
        {"radio_unchecked","GuiRadioUnchecked","EditorIcons"},
        {"submenu","ArrowRight","EditorIcons"},
        {"visibility_hidden","GuiVisibilityHidden","EditorIcons"},
        {"visibility_visible","GuiVisibilityVisible","EditorIcons"},
        {"visibility_xray","GuiVisibilityXray","EditorIcons"}
    };

    theme->set_icons(popup_icons,"PopupMenu");
    theme->set_constant("vseparation", "PopupMenu", (extra_spacing + default_margin_size + 1) * EDSCALE);

    Ref<StyleBoxFlat> sub_inspector_bg = make_flat_stylebox(dark_color_1.linear_interpolate(accent_color, 0.08f), 2, 0, 2, 2);
    sub_inspector_bg->set_border_width(Margin::Left, 2);
    sub_inspector_bg->set_border_width(Margin::Right, 2);
    sub_inspector_bg->set_border_width(Margin::Bottom, 2);
    sub_inspector_bg->set_border_color(accent_color * Color(1, 1, 1, 0.3f));
    sub_inspector_bg->set_draw_center(true);

    theme->set_stylebox("sub_inspector_bg", "Editor", sub_inspector_bg);
    theme->set_constant("inspector_margin", "Editor", 8 * EDSCALE);

    // Tree & ItemList background
    Ref<StyleBoxFlat> style_tree_bg = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_bg->set_bg_color(dark_color_1);
    style_tree_bg->set_border_color(dark_color_3);
    theme->set_stylebox("bg", "Tree", style_tree_bg);

    const Color guide_color = Color(mono_color.r, mono_color.g, mono_color.b, 0.05f);
    Color relationship_line_color = Color(mono_color.r, mono_color.g, mono_color.b, relationship_line_opacity);
    // Tree
    const Theme::ThemeIcon tree_icons[] = {
        {"checked", "GuiChecked", "EditorIcons" },
        {"unchecked", "GuiUnchecked", "EditorIcons" },
        {"arrow", "GuiTreeArrowDown", "EditorIcons" },
        {"arrow_collapsed", "GuiTreeArrowRight", "EditorIcons" },
        {"updown", "GuiTreeUpdown", "EditorIcons" },
        {"select_arrow", "GuiDropdown", "EditorIcons" },

    };

    theme->set_stylebox("bg_focus", "Tree", style_focus);
    theme->set_stylebox("custom_button", "Tree", make_empty_stylebox());
    theme->set_stylebox("custom_button_pressed", "Tree", make_empty_stylebox());
    theme->set_stylebox("custom_button_hover", "Tree", style_widget);

    const Theme::ThemeColor tree_colors[] = {
        {"custom_button_font_highlight", "Tree", font_color_hl },
        {"font_color", "Tree", font_color },
        {"font_color_selected", "Tree", mono_color },
        {"title_button_color", "Tree", font_color },
        {"guide_color", "Tree", guide_color },
        {"relationship_line_color", "Tree", relationship_line_color },
        {"drop_position_color", "Tree", accent_color },
    };

    const Theme::ThemeConstant tree_constants[] = {
        {"vseparation", "Tree", int((extra_spacing + default_margin_size) * EDSCALE)},
        {"hseparation", "Tree", int((extra_spacing + default_margin_size) * EDSCALE)},
        {"item_margin", "Tree", int(3 * default_margin_size * EDSCALE)},
        {"button_margin", "Tree", int(default_margin_size * EDSCALE)},
        {"draw_relationship_lines", "Tree", relationship_line_opacity >= 0.01f},
        {"draw_guides", "Tree", relationship_line_opacity < 0.01f},
        {"scroll_border", "Tree", int(40 * EDSCALE)},
        {"scroll_speed", "Tree", 12},
    };

    theme->set_icons(tree_icons, "Tree");
    theme->set_colors(tree_colors);
    theme->set_constants(tree_constants);

    Ref<StyleBoxFlat> style_tree_btn = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_btn->set_bg_color(contrast_color_1);
    style_tree_btn->set_border_width_all(0);
    theme->set_stylebox("button_pressed", "Tree", style_tree_btn);

    Ref<StyleBoxFlat> style_tree_hover = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_hover->set_bg_color(highlight_color * Color(1, 1, 1, 0.4f));
    style_tree_hover->set_border_width_all(0);
    theme->set_stylebox("hover", "Tree", style_tree_hover);

    Ref<StyleBoxFlat> style_tree_focus = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_focus->set_bg_color(highlight_color);
    style_tree_focus->set_border_width_all(0);
    theme->set_stylebox("selected_focus", "Tree", style_tree_focus);

    Ref<StyleBoxFlat> style_tree_selected = dynamic_ref_cast<StyleBoxFlat>(style_tree_focus->duplicate());
    theme->set_stylebox("selected", "Tree", style_tree_selected);

    Ref<StyleBoxFlat> style_tree_cursor = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_cursor->set_draw_center(false);
    style_tree_cursor->set_border_width_all(border_width);
    style_tree_cursor->set_border_color(contrast_color_1);

    Ref<StyleBoxFlat> style_tree_title = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_tree_title->set_bg_color(dark_color_3);
    style_tree_title->set_border_width_all(0);
    theme->set_stylebox("cursor", "Tree", style_tree_cursor);
    theme->set_stylebox("cursor_unfocused", "Tree", style_tree_cursor);
    theme->set_stylebox("title_button_normal", "Tree", style_tree_title);
    theme->set_stylebox("title_button_hover", "Tree", style_tree_title);
    theme->set_stylebox("title_button_pressed", "Tree", style_tree_title);

    Color prop_category_color = dark_color_1.linear_interpolate(mono_color, 0.12f);
    Color prop_section_color = dark_color_1.linear_interpolate(mono_color, 0.09f);
    Color prop_subsection_color = dark_color_1.linear_interpolate(mono_color, 0.06f);
    theme->set_color("prop_category", "Editor", prop_category_color);
    theme->set_color("prop_section", "Editor", prop_section_color);
    theme->set_color("prop_subsection", "Editor", prop_subsection_color);
    theme->set_color("drop_position_color", "Tree", accent_color);

    // ItemList
    Ref<StyleBoxFlat> style_itemlist_bg = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_itemlist_bg->set_bg_color(dark_color_1);
    style_itemlist_bg->set_border_width_all(border_width);
    style_itemlist_bg->set_border_color(dark_color_3);

    Ref<StyleBoxFlat> style_itemlist_cursor = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_itemlist_cursor->set_draw_center(false);
    style_itemlist_cursor->set_border_width_all(border_width);
    style_itemlist_cursor->set_border_color(highlight_color);
    theme->set_stylebox("cursor", "ItemList", style_itemlist_cursor);
    theme->set_stylebox("cursor_unfocused", "ItemList", style_itemlist_cursor);
    theme->set_stylebox("selected_focus", "ItemList", style_tree_focus);
    theme->set_stylebox("selected", "ItemList", style_tree_selected);
    theme->set_stylebox("bg_focus", "ItemList", style_focus);
    theme->set_stylebox("bg", "ItemList", style_itemlist_bg);
    theme->set_color("font_color", "ItemList", font_color);
    theme->set_color("font_color_selected", "ItemList", mono_color);
    theme->set_color("guide_color", "ItemList", guide_color);
    theme->set_constant("vseparation", "ItemList", 3 * EDSCALE);
    theme->set_constant("hseparation", "ItemList", 3 * EDSCALE);
    theme->set_constant("icon_margin", "ItemList", default_margin_size * EDSCALE);
    theme->set_constant("line_separation", "ItemList", 3 * EDSCALE);

    // Tabs & TabContainer
    theme->set_stylebox("tab_fg", "TabContainer", style_tab_selected);
    theme->set_stylebox("tab_bg", "TabContainer", style_tab_unselected);
    theme->set_stylebox("tab_disabled", "TabContainer", style_tab_disabled);
    theme->set_stylebox("tab_fg", "Tabs", style_tab_selected);
    theme->set_stylebox("tab_bg", "Tabs", style_tab_unselected);
    theme->set_stylebox("tab_disabled", "Tabs", style_tab_disabled);
    theme->set_color("font_color_fg", "TabContainer", font_color);
    theme->set_color("font_color_bg", "TabContainer", font_color_disabled);
    theme->set_color("font_color_fg", "Tabs", font_color);
    theme->set_color("font_color_bg", "Tabs", font_color_disabled);
    theme->set_stylebox("SceneTabFG", "EditorStyles", style_tab_selected);
    theme->set_stylebox("SceneTabBG", "EditorStyles", style_tab_unselected);
    theme->set_stylebox("button_pressed", "Tabs", style_menu);
    theme->set_stylebox("button", "Tabs", style_menu);
    const Theme::ThemeIcon tab_icons[] = {
        {"close", "GuiClose", "EditorIcons" },
        {"increment", "GuiScrollArrowRight", "EditorIcons" },
        {"decrement", "GuiScrollArrowLeft", "EditorIcons" },
        {"increment_highlight", "GuiScrollArrowRightHl", "EditorIcons" },
        {"decrement_highlight", "GuiScrollArrowLeftHl", "EditorIcons" },
    };
    const Theme::ThemeIcon tab_container_icons[] = {
        {"menu", "GuiTabMenu", "EditorIcons" },
        {"menu_highlight", "GuiTabMenuHl", "EditorIcons" },
        {"increment", "GuiScrollArrowRight", "EditorIcons" },
        {"decrement", "GuiScrollArrowLeft", "EditorIcons" },
        {"increment_highlight", "GuiScrollArrowRightHl", "EditorIcons" },
        {"decrement_highlight", "GuiScrollArrowLeftHl", "EditorIcons" },
    };
    theme->set_icons(tab_icons,"Tabs");
    theme->set_icons(tab_container_icons,"TabContainer");
    theme->set_constant("hseparation", "Tabs", 4 * EDSCALE);

    // Content of each tab
    Ref<StyleBoxFlat> style_content_panel = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_content_panel->set_border_color(dark_color_3);
    style_content_panel->set_border_width_all(border_width);
    // compensate the border
    style_content_panel->set_default_margin(Margin::Top, margin_size_extra * EDSCALE);
    style_content_panel->set_default_margin(Margin::Right, margin_size_extra * EDSCALE);
    style_content_panel->set_default_margin(Margin::Bottom, margin_size_extra * EDSCALE);
    style_content_panel->set_default_margin(Margin::Left, margin_size_extra * EDSCALE);

    // this is the stylebox used in 3d and 2d viewports (no borders)
    Ref<StyleBoxFlat> style_content_panel_vp = dynamic_ref_cast<StyleBoxFlat>(style_content_panel->duplicate());
    style_content_panel_vp->set_default_margin(Margin::Left, border_width * 2);
    style_content_panel_vp->set_default_margin(Margin::Top, default_margin_size * EDSCALE);
    style_content_panel_vp->set_default_margin(Margin::Right, border_width * 2);
    style_content_panel_vp->set_default_margin(Margin::Bottom, border_width * 2);
    theme->set_stylebox("panel", "TabContainer", style_content_panel);
    theme->set_stylebox("Content", "EditorStyles", style_content_panel_vp);

    // Separators
    theme->set_stylebox("separator", "HSeparator", make_line_stylebox(separator_color, border_width));
    theme->set_stylebox("separator", "VSeparator", make_line_stylebox(separator_color, border_width, 0, 0, true));

    // Debugger

    Ref<StyleBoxFlat> style_panel_debugger = dynamic_ref_cast<StyleBoxFlat>(style_content_panel->duplicate());
    style_panel_debugger->set_border_width(Margin::Bottom, 0);
    theme->set_stylebox("DebuggerPanel", "EditorStyles", style_panel_debugger);
    theme->set_stylebox("DebuggerTabFG", "EditorStyles", style_tab_selected);
    theme->set_stylebox("DebuggerTabBG", "EditorStyles", style_tab_unselected);

    Ref<StyleBoxFlat> style_panel_invisible_top = dynamic_ref_cast<StyleBoxFlat>(style_content_panel->duplicate());
    int stylebox_offset = theme->get_font("tab_fg", "TabContainer")->get_height() + theme->get_stylebox("tab_fg", "TabContainer")->get_minimum_size().height + theme->get_stylebox("panel", "TabContainer")->get_default_margin(Margin::Top);
    style_panel_invisible_top->set_expand_margin_size(Margin::Top, -stylebox_offset);
    theme->set_stylebox("BottomPanelDebuggerOverride", "EditorStyles", style_panel_invisible_top);

    // LineEdit
    theme->set_stylebox("normal", "LineEdit", style_widget);
    theme->set_stylebox("focus", "LineEdit", style_widget_focus);
    theme->set_stylebox("read_only", "LineEdit", style_widget_disabled);
    theme->set_icon("clear", "LineEdit", theme->get_icon("GuiClose", "EditorIcons"));
    const Theme::ThemeColor line_edit_colors[] = {
        {"read_only", "LineEdit", font_color_disabled },
        {"font_color", "LineEdit", font_color },
        {"font_color_selected", "LineEdit", mono_color },
        {"cursor_color", "LineEdit", font_color },
        {"selection_color", "LineEdit", font_color_selection },
        {"clear_button_color", "LineEdit", font_color },
        {"clear_button_color_pressed", "LineEdit", accent_color },

    };
    theme->set_colors(line_edit_colors);

    // TextEdit
    theme->set_stylebox("normal", "TextEdit", style_widget);
    theme->set_stylebox("focus", "TextEdit", style_widget_hover);
    theme->set_stylebox("read_only", "TextEdit", style_widget_disabled);
    theme->set_constant("side_margin", "TabContainer", 0);
    theme->set_icon("tab", "TextEdit", theme->get_icon("GuiTab", "EditorIcons"));
    theme->set_icon("space", "TextEdit", theme->get_icon("GuiSpace", "EditorIcons"));
    theme->set_icon("folded", "TextEdit", theme->get_icon("GuiTreeArrowRight", "EditorIcons"));
    theme->set_icon("fold", "TextEdit", theme->get_icon("GuiTreeArrowDown", "EditorIcons"));
    theme->set_color("font_color", "TextEdit", font_color);
    theme->set_color("caret_color", "TextEdit", font_color);
    theme->set_color("selection_color", "TextEdit", font_color_selection);

    // H/VSplitContainer
    theme->set_stylebox("bg", "VSplitContainer", make_stylebox(theme->get_icon("GuiVsplitBg", "EditorIcons"), 1, 1, 1, 1));
    theme->set_stylebox("bg", "HSplitContainer", make_stylebox(theme->get_icon("GuiHsplitBg", "EditorIcons"), 1, 1, 1, 1));

    theme->set_icon("grabber", "VSplitContainer", theme->get_icon("GuiVsplitter", "EditorIcons"));
    theme->set_icon("grabber", "HSplitContainer", theme->get_icon("GuiHsplitter", "EditorIcons"));

    theme->set_constant("separation", "HSplitContainer", default_margin_size * 2 * EDSCALE);
    theme->set_constant("separation", "VSplitContainer", default_margin_size * 2 * EDSCALE);

    // Containers
    const Theme::ThemeConstant container_constants[] = {
        {"separation", "BoxContainer", int(default_margin_size * EDSCALE)},
        {"separation", "HBoxContainer", int(default_margin_size * EDSCALE)},
        {"separation", "VBoxContainer", int(default_margin_size * EDSCALE)},
        {"margin_left", "MarginContainer", 0},
        {"margin_top", "MarginContainer", 0},
        {"margin_right", "MarginContainer", 0},
        {"margin_bottom", "MarginContainer", 0},
        {"hseparation", "GridContainer", int(default_margin_size * EDSCALE)},
        {"vseparation", "GridContainer", int(default_margin_size * EDSCALE)}
    };
    theme->set_constants(container_constants);
    // WindowDialog
    Ref<StyleBoxFlat> style_window = dynamic_ref_cast<StyleBoxFlat>(style_popup->duplicate());
    style_window->set_border_color(tab_color);
    style_window->set_border_width(Margin::Top, 24 * EDSCALE);
    style_window->set_expand_margin_size(Margin::Top, 24 * EDSCALE);
    theme->set_stylebox("panel", "WindowDialog", style_window);
    theme->set_color("title_color", "WindowDialog", font_color);
    theme->set_icon("close", "WindowDialog", theme->get_icon("GuiClose", "EditorIcons"));
    theme->set_icon("close_highlight", "WindowDialog", theme->get_icon("GuiClose", "EditorIcons"));
    theme->set_constant("close_h_ofs", "WindowDialog", 22 * EDSCALE);
    theme->set_constant("close_v_ofs", "WindowDialog", 20 * EDSCALE);
    theme->set_constant("title_height", "WindowDialog", 24 * EDSCALE);
    theme->set_font("title_font", "WindowDialog", theme->get_font("title", "EditorFonts"));

    // complex window, for now only Editor settings and Project settings
    Ref<StyleBoxFlat> style_complex_window = dynamic_ref_cast<StyleBoxFlat>(style_window->duplicate());
    style_complex_window->set_bg_color(dark_color_2);
    style_complex_window->set_border_color(highlight_tabs ? tab_color : dark_color_2);
    theme->set_stylebox("panel", "EditorSettingsDialog", style_complex_window);
    theme->set_stylebox("panel", "ProjectSettingsEditor", style_complex_window);
    theme->set_stylebox("panel", "EditorAbout", style_complex_window);

    // HScrollBar
    Ref<Texture> empty_icon(make_ref_counted<ImageTexture>());

    theme->set_stylebox("scroll", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
    theme->set_stylebox("scroll_focus", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
    theme->set_stylebox("grabber", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabber", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));
    theme->set_stylebox("grabber_highlight", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberHl", "EditorIcons"), 5, 5, 5, 5, 2, 2, 2, 2));
    theme->set_stylebox("grabber_pressed", "HScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberPressed", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));

    theme->set_icon("increment", "HScrollBar", empty_icon);
    theme->set_icon("increment_highlight", "HScrollBar", empty_icon);
    theme->set_icon("decrement", "HScrollBar", empty_icon);
    theme->set_icon("decrement_highlight", "HScrollBar", empty_icon);

    // VScrollBar
    theme->set_stylebox("scroll", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
    theme->set_stylebox("scroll_focus", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollBg", "EditorIcons"), 5, 5, 5, 5, 0, 0, 0, 0));
    theme->set_stylebox("grabber", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabber", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));
    theme->set_stylebox("grabber_highlight", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberHl", "EditorIcons"), 5, 5, 5, 5, 2, 2, 2, 2));
    theme->set_stylebox("grabber_pressed", "VScrollBar", make_stylebox(theme->get_icon("GuiScrollGrabberPressed", "EditorIcons"), 6, 6, 6, 6, 2, 2, 2, 2));

    theme->set_icon("increment", "VScrollBar", empty_icon);
    theme->set_icon("increment_highlight", "VScrollBar", empty_icon);
    theme->set_icon("decrement", "VScrollBar", empty_icon);
    theme->set_icon("decrement_highlight", "VScrollBar", empty_icon);

    // HSlider
    theme->set_icon("grabber_highlight", "HSlider", theme->get_icon("GuiSliderGrabberHl", "EditorIcons"));
    theme->set_icon("grabber", "HSlider", theme->get_icon("GuiSliderGrabber", "EditorIcons"));
    theme->set_stylebox("slider", "HSlider", make_flat_stylebox(dark_color_3, 0, default_margin_size / 2, 0, default_margin_size / 2));
    theme->set_stylebox("grabber_area", "HSlider", make_flat_stylebox(contrast_color_1, 0, default_margin_size / 2, 0, default_margin_size / 2));

    // VSlider
    theme->set_icon("grabber", "VSlider", theme->get_icon("GuiSliderGrabber", "EditorIcons"));
    theme->set_icon("grabber_highlight", "VSlider", theme->get_icon("GuiSliderGrabberHl", "EditorIcons"));
    theme->set_stylebox("slider", "VSlider", make_flat_stylebox(dark_color_3, default_margin_size / 2, 0, default_margin_size / 2, 0));
    theme->set_stylebox("grabber_area", "VSlider", make_flat_stylebox(contrast_color_1, default_margin_size / 2, 0, default_margin_size / 2, 0));

    //RichTextLabel
    theme->set_color("default_color", "RichTextLabel", font_color);
    theme->set_color("font_color_shadow", "RichTextLabel", Color(0, 0, 0, 0));
    theme->set_constant("shadow_offset_x", "RichTextLabel", 1 * EDSCALE);
    theme->set_constant("shadow_offset_y", "RichTextLabel", 1 * EDSCALE);
    theme->set_constant("shadow_as_outline", "RichTextLabel", 0 * EDSCALE);
    theme->set_stylebox("focus", "RichTextLabel", make_empty_stylebox());
    theme->set_stylebox("normal", "RichTextLabel", style_tree_bg);

    theme->set_color("headline_color", "EditorHelp", mono_color);

    // Panel
    theme->set_stylebox("panel", "Panel", make_flat_stylebox(dark_color_1, 6, 4, 6, 4));

    // Label
    theme->set_stylebox("normal", "Label", style_empty);
    theme->set_color("font_color", "Label", font_color);
    theme->set_color("font_color_shadow", "Label", Color(0, 0, 0, 0));
    theme->set_constant("shadow_offset_x", "Label", 1 * EDSCALE);
    theme->set_constant("shadow_offset_y", "Label", 1 * EDSCALE);
    theme->set_constant("shadow_as_outline", "Label", 0 * EDSCALE);
    theme->set_constant("line_spacing", "Label", 3 * EDSCALE);

    // LinkButton
    theme->set_stylebox("focus", "LinkButton", style_empty);
    theme->set_color("font_color", "LinkButton", font_color);

    // TooltipPanel
    Ref<StyleBoxFlat> style_tooltip = dynamic_ref_cast<StyleBoxFlat>(style_popup->duplicate());
    float v = MAX(border_size * EDSCALE, 1.0f);
    style_tooltip->set_default_margin(Margin::Left, v);
    style_tooltip->set_default_margin(Margin::Top, v);
    style_tooltip->set_default_margin(Margin::Right, v);
    style_tooltip->set_default_margin(Margin::Bottom, v);
    style_tooltip->set_bg_color(Color(mono_color.r, mono_color.g, mono_color.b, 0.9f));
    style_tooltip->set_border_width_all(border_width);
    style_tooltip->set_border_color(mono_color);
    theme->set_color("font_color", "TooltipLabel", font_color.inverted());
    theme->set_color("font_color_shadow", "TooltipLabel", mono_color.inverted() * Color(1, 1, 1, 0.1f));
    theme->set_stylebox("panel", "TooltipPanel", style_tooltip);

    // PopupPanel
    theme->set_stylebox("panel", "PopupPanel", style_popup);

    // SpinBox
    theme->set_icon("updown", "SpinBox", theme->get_icon("GuiSpinboxUpdown", "EditorIcons"));

    // ProgressBar
    theme->set_stylebox("bg", "ProgressBar", make_stylebox(theme->get_icon("GuiProgressBar", "EditorIcons"), 4, 4, 4, 4, 0, 0, 0, 0));
    theme->set_stylebox("fg", "ProgressBar", make_stylebox(theme->get_icon("GuiProgressFill", "EditorIcons"), 6, 6, 6, 6, 2, 1, 2, 1));
    theme->set_color("font_color", "ProgressBar", font_color);

    // GraphEdit
    theme->set_stylebox("bg", "GraphEdit", style_tree_bg);
    if (dark_theme) {
        theme->set_color("grid_major", "GraphEdit", Color(1.0f, 1.0f, 1.0f, 0.15f));
        theme->set_color("grid_minor", "GraphEdit", Color(1.0f, 1.0f, 1.0f, 0.07f));
    } else {
        theme->set_color("grid_major", "GraphEdit", Color(0.0f, 0.0f, 0.0f, 0.15f));
        theme->set_color("grid_minor", "GraphEdit", Color(0.0f, 0.0f, 0.0f, 0.07f));
    }
    theme->set_color("selection_fill", "GraphEdit", theme->get_color("box_selection_fill_color", "Editor"));
    theme->set_color("selection_stroke", "GraphEdit", theme->get_color("box_selection_stroke_color", "Editor"));

    theme->set_color("activity", "GraphEdit", accent_color);
    theme->set_icon("minus", "GraphEdit", theme->get_icon("ZoomLess", "EditorIcons"));
    theme->set_icon("more", "GraphEdit", theme->get_icon("ZoomMore", "EditorIcons"));
    theme->set_icon("reset", "GraphEdit", theme->get_icon("ZoomReset", "EditorIcons"));
    theme->set_icon("snap", "GraphEdit", theme->get_icon("SnapGrid", "EditorIcons"));
    theme->set_constant("bezier_len_pos", "GraphEdit", 80 * EDSCALE);
    theme->set_constant("bezier_len_neg", "GraphEdit", 160 * EDSCALE);

    // GraphNode

    const float mv = dark_theme ? 0.0f : 1.0f;
    const float mv2 = 1.0f - mv;
    const int gn_margin_side = 28;
    Ref<StyleBoxFlat> graphsb = make_flat_stylebox(Color(mv, mv, mv, 0.7f), gn_margin_side, 24, gn_margin_side, 5);
    graphsb->set_border_width_all(border_width);
    graphsb->set_border_color(Color(mv2, mv2, mv2, 0.9f));
    Ref<StyleBoxFlat> graphsbselected = make_flat_stylebox(Color(mv, mv, mv, 0.9f), gn_margin_side, 24, gn_margin_side, 5);
    graphsbselected->set_border_width_all(border_width);
    graphsbselected->set_border_color(Color(accent_color.r, accent_color.g, accent_color.b, 0.9f));
    graphsbselected->set_shadow_size(8 * EDSCALE);
    graphsbselected->set_shadow_color(shadow_color);
    Ref<StyleBoxFlat> graphsbcomment = make_flat_stylebox(Color(mv, mv, mv, 0.3f), gn_margin_side, 24, gn_margin_side, 5);
    graphsbcomment->set_border_width_all(border_width);
    graphsbcomment->set_border_color(Color(mv2, mv2, mv2, 0.9f));
    Ref<StyleBoxFlat> graphsbcommentselected = make_flat_stylebox(Color(mv, mv, mv, 0.4f), gn_margin_side, 24, gn_margin_side, 5);
    graphsbcommentselected->set_border_width_all(border_width);
    graphsbcommentselected->set_border_color(Color(mv2, mv2, mv2, 0.9f));
    Ref<StyleBoxFlat> graphsbbreakpoint = dynamic_ref_cast<StyleBoxFlat>(graphsbselected->duplicate());
    graphsbbreakpoint->set_draw_center(false);
    graphsbbreakpoint->set_border_color(warning_color);
    graphsbbreakpoint->set_shadow_color(warning_color * Color(1.0f, 1.0f, 1.0f, 0.1f));
    Ref<StyleBoxFlat> graphsbposition = dynamic_ref_cast<StyleBoxFlat>(graphsbselected->duplicate());
    graphsbposition->set_draw_center(false);
    graphsbposition->set_border_color(error_color);
    graphsbposition->set_shadow_color(error_color * Color(1.0f, 1.0f, 1.0f, 0.2f));
    Ref<StyleBoxFlat> smgraphsb = make_flat_stylebox(Color(mv, mv, mv, 0.7f), gn_margin_side, 24, gn_margin_side, 5);
    smgraphsb->set_border_width_all(border_width);
    smgraphsb->set_border_color(Color(mv2, mv2, mv2, 0.9f));
    Ref<StyleBoxFlat> smgraphsbselected = make_flat_stylebox(Color(mv, mv, mv, 0.9f), gn_margin_side, 24, gn_margin_side, 5);
    smgraphsbselected->set_border_width_all(border_width);
    smgraphsbselected->set_border_color(Color(accent_color.r, accent_color.g, accent_color.b, 0.9f));
    smgraphsbselected->set_shadow_size(8 * EDSCALE);
    smgraphsbselected->set_shadow_color(shadow_color);

    if (use_gn_headers) {
        graphsb->set_border_width(Margin::Top, 24 * EDSCALE);
        graphsbselected->set_border_width(Margin::Top, 24 * EDSCALE);
        graphsbcomment->set_border_width(Margin::Top, 24 * EDSCALE);
        graphsbcommentselected->set_border_width(Margin::Top, 24 * EDSCALE);
    }

    theme->set_stylebox("frame", "GraphNode", graphsb);
    theme->set_stylebox("selectedframe", "GraphNode", graphsbselected);
    theme->set_stylebox("comment", "GraphNode", graphsbcomment);
    theme->set_stylebox("commentfocus", "GraphNode", graphsbcommentselected);
    theme->set_stylebox("breakpoint", "GraphNode", graphsbbreakpoint);
    theme->set_stylebox("position", "GraphNode", graphsbposition);
    theme->set_stylebox("state_machine_frame", "GraphNode", smgraphsb);
    theme->set_stylebox("state_machine_selectedframe", "GraphNode", smgraphsbselected);

    Color default_node_color = Color(mv2, mv2, mv2);
    theme->set_color("title_color", "GraphNode", default_node_color);
    default_node_color.a = 0.7f;
    theme->set_color("close_color", "GraphNode", default_node_color);
    theme->set_color("resizer_color", "GraphNode", default_node_color);
    const Theme::ThemeConstant graph_node_constants[] = {
        {"port_offset", "GraphNode", int(14 * EDSCALE)},
        {"title_h_offset", "GraphNode", int(-16 * EDSCALE)},
        {"title_offset", "GraphNode", int(20 * EDSCALE)},
        {"close_h_offset", "GraphNode", int(20 * EDSCALE)},
        {"close_offset", "GraphNode", int(20 * EDSCALE)},
        {"separation", "GraphNode", int(1 * EDSCALE)},
    };
    theme->set_constants(graph_node_constants);

    theme->set_icon("close", "GraphNode", theme->get_icon("GuiCloseCustomizable", "EditorIcons"));
    theme->set_icon("resizer", "GraphNode", theme->get_icon("GuiResizer", "EditorIcons"));
    theme->set_icon("port", "GraphNode", theme->get_icon("GuiGraphNodePort", "EditorIcons"));

    // GridContainer
    theme->set_constant("vseparation", "GridContainer", (extra_spacing + default_margin_size) * EDSCALE);

    // FileDialog
    theme->set_icon("folder", "FileDialog", theme->get_icon("Folder", "EditorIcons"));
    theme->set_icon("parent_folder", "FileDialog", theme->get_icon("ArrowUp", "EditorIcons"));
    theme->set_icon("reload", "FileDialog", theme->get_icon("Reload", "EditorIcons"));
    theme->set_icon("toggle_hidden", "FileDialog", theme->get_icon("GuiVisibilityVisible", "EditorIcons"));
    // Use a different color for folder icons to make them easier to distinguish from files.
    // On a light theme, the icon will be dark, so we need to lighten it before blending it with the accent color.
    theme->set_color("folder_icon_modulate", "FileDialog", (dark_theme ? Color(1, 1, 1) : Color(4.25f, 4.25f, 4.25f)).linear_interpolate(accent_color, 0.7f));
    theme->set_color("files_disabled", "FileDialog", font_color_disabled);

    // color picker
    theme->set_constant("margin", "ColorPicker", popup_margin_size);
    theme->set_constant("sv_width", "ColorPicker", 256 * EDSCALE);
    theme->set_constant("sv_height", "ColorPicker", 256 * EDSCALE);
    theme->set_constant("h_width", "ColorPicker", 30 * EDSCALE);
    theme->set_constant("label_width", "ColorPicker", 10 * EDSCALE);
    theme->set_icon("screen_picker", "ColorPicker", theme->get_icon("ColorPick", "EditorIcons"));
    theme->set_icon("add_preset", "ColorPicker", theme->get_icon("Add", "EditorIcons"));
    theme->set_icon("preset_bg", "ColorPicker", theme->get_icon("GuiMiniCheckerboard", "EditorIcons"));
    theme->set_icon("overbright_indicator", "ColorPicker", theme->get_icon("OverbrightIndicator", "EditorIcons"));

    theme->set_icon("bg", "ColorPickerButton", theme->get_icon("GuiMiniCheckerboard", "EditorIcons"));

    // Information on 3D viewport
    Ref<StyleBoxFlat> style_info_3d_viewport = dynamic_ref_cast<StyleBoxFlat>(style_default->duplicate());
    style_info_3d_viewport->set_bg_color(style_info_3d_viewport->get_bg_color() * Color(1, 1, 1, 0.5f));
    style_info_3d_viewport->set_border_width_all(0);
    theme->set_stylebox("Information3dViewport", "EditorStyles", style_info_3d_viewport);

    // adaptive script theme constants
    // for comments and elements with lower relevance
    const Color dim_color = Color(font_color.r, font_color.g, font_color.b, 0.5f);

    const float mono_value = mono_color.r;
    const Color alpha1 = Color(mono_value, mono_value, mono_value, 0.07f);
    const Color alpha2 = Color(mono_value, mono_value, mono_value, 0.14f);
    const Color alpha3 = Color(mono_value, mono_value, mono_value, 0.7f);

    // editor main color
    const Color main_color = dark_theme ? Color(0.34f, 0.7f, 1.0f) : Color(0.02f, 0.5f, 1.0f);

    const Color symbol_color = Color(0.34f, 0.57f, 1.0f).linear_interpolate(mono_color, dark_theme ? 0.5f : 0.3f);
    const Color keyword_color = Color(1.0f, 0.44f, 0.52f);
    const Color basetype_color = dark_theme ? Color(0.26f, 1.0f, 0.76f) : Color(0.0f, 0.76f, 0.38f);
    const Color type_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.4f : 0.3f);
    const Color usertype_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.7f : 0.5f);
    const Color comment_color = dim_color;
    const Color string_color = (dark_theme ? Color(1.0f, 0.85f, 0.26f) : Color(1.0f, 0.82f, 0.09f)).linear_interpolate(mono_color, dark_theme ? 0.5f : 0.3f);

    const Color te_background_color = dark_theme ? background_color : base_color;
    const Color completion_background_color = dark_theme ? base_color : background_color;
    const Color completion_selected_color = alpha1;
    const Color completion_existing_color = alpha2;
    const Color completion_scroll_color = alpha1;
    const Color completion_font_color = font_color;
    const Color text_color = font_color;
    const Color line_number_color = dim_color;
    const Color safe_line_number_color = dim_color * Color(1, 1.2f, 1, 1.5f);
    const Color caret_color = mono_color;
    const Color caret_background_color = mono_color.inverted();
    const Color text_selected_color = dark_color_3;
    const Color selection_color = accent_color * Color(1, 1, 1, 0.35f);
    const Color brace_mismatch_color = error_color;
    const Color current_line_color = alpha1;
    const Color line_length_guideline_color = dark_theme ? base_color : background_color;
    const Color word_highlighted_color = alpha1;
    const Color number_color = basetype_color.linear_interpolate(mono_color, dark_theme ? 0.5f : 0.3f);
    const Color function_color = main_color;
    const Color member_variable_color = main_color.linear_interpolate(mono_color, 0.6f);
    const Color mark_color = Color(error_color.r, error_color.g, error_color.b, 0.3f);
    const Color bookmark_color = Color(0.08f, 0.49f, 0.98f);
    const Color breakpoint_color = error_color;
    const Color executing_line_color = Color(0.2f, 0.8f, 0.2f, 0.4f);
    const Color code_folding_color = alpha3;
    const Color search_result_color = alpha1;
    const Color search_result_border_color = Color(0.41f, 0.61f, 0.91f, 0.38f);

    EditorSettings *setting = EditorSettings::get_singleton();
    UIString text_editor_color_theme = setting->get("text_editor/theme/color_theme");
    if (text_editor_color_theme == "Adaptive") {
        setting->set_initial_value("text_editor/highlighting/symbol_color", symbol_color, true);
        setting->set_initial_value("text_editor/highlighting/keyword_color", keyword_color, true);
        setting->set_initial_value("text_editor/highlighting/base_type_color", basetype_color, true);
        setting->set_initial_value("text_editor/highlighting/engine_type_color", type_color, true);
        setting->set_initial_value("text_editor/highlighting/user_type_color", usertype_color, true);
        setting->set_initial_value("text_editor/highlighting/comment_color", comment_color, true);
        setting->set_initial_value("text_editor/highlighting/string_color", string_color, true);
        setting->set_initial_value("text_editor/highlighting/background_color", te_background_color, true);
        setting->set_initial_value("text_editor/highlighting/completion_background_color", completion_background_color, true);
        setting->set_initial_value("text_editor/highlighting/completion_selected_color", completion_selected_color, true);
        setting->set_initial_value("text_editor/highlighting/completion_existing_color", completion_existing_color, true);
        setting->set_initial_value("text_editor/highlighting/completion_scroll_color", completion_scroll_color, true);
        setting->set_initial_value("text_editor/highlighting/completion_font_color", completion_font_color, true);
        setting->set_initial_value("text_editor/highlighting/text_color", text_color, true);
        setting->set_initial_value("text_editor/highlighting/line_number_color", line_number_color, true);
        setting->set_initial_value("text_editor/highlighting/safe_line_number_color", safe_line_number_color, true);
        setting->set_initial_value("text_editor/highlighting/caret_color", caret_color, true);
        setting->set_initial_value("text_editor/highlighting/caret_background_color", caret_background_color, true);
        setting->set_initial_value("text_editor/highlighting/text_selected_color", text_selected_color, true);
        setting->set_initial_value("text_editor/highlighting/selection_color", selection_color, true);
        setting->set_initial_value("text_editor/highlighting/brace_mismatch_color", brace_mismatch_color, true);
        setting->set_initial_value("text_editor/highlighting/current_line_color", current_line_color, true);
        setting->set_initial_value("text_editor/highlighting/line_length_guideline_color", line_length_guideline_color, true);
        setting->set_initial_value("text_editor/highlighting/word_highlighted_color", word_highlighted_color, true);
        setting->set_initial_value("text_editor/highlighting/number_color", number_color, true);
        setting->set_initial_value("text_editor/highlighting/function_color", function_color, true);
        setting->set_initial_value("text_editor/highlighting/member_variable_color", member_variable_color, true);
        setting->set_initial_value("text_editor/highlighting/mark_color", mark_color, true);
        setting->set_initial_value("text_editor/highlighting/bookmark_color", bookmark_color, true);
        setting->set_initial_value("text_editor/highlighting/breakpoint_color", breakpoint_color, true);
        setting->set_initial_value("text_editor/highlighting/executing_line_color", executing_line_color, true);
        setting->set_initial_value("text_editor/highlighting/code_folding_color", code_folding_color, true);
        setting->set_initial_value("text_editor/highlighting/search_result_color", search_result_color, true);
        setting->set_initial_value("text_editor/highlighting/search_result_border_color", search_result_border_color, true);
    } else if (text_editor_color_theme == "Default") {
        setting->load_text_editor_theme();
    }

    return theme;
}

Ref<Theme> create_custom_theme(const Ref<Theme>& p_theme) {
    Ref<Theme> theme;

    String custom_theme = EditorSettings::get_singleton()->get("interface/theme/custom_theme");
    if (!custom_theme.empty()) {
        theme = dynamic_ref_cast<Theme>(ResourceLoader::load(custom_theme));
    }

    if (not theme) {
        theme = create_editor_theme(p_theme);
    }

    return theme;
}
