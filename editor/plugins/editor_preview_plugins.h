/*************************************************************************/
/*  editor_preview_plugins.h                                             */
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

#pragma once

#include "editor/editor_resource_preview.h"
#include "core/safe_refcount.h"

void post_process_preview(const Ref<Image> &p_image);

class EditorTexturePreviewPlugin : public EditorResourcePreviewGenerator {
    GDCLASS(EditorTexturePreviewPlugin,EditorResourcePreviewGenerator)

public:
    bool handles(StringView p_type) const override;
    bool generate_small_preview_automatically() const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorTexturePreviewPlugin();
};

class EditorImagePreviewPlugin : public EditorResourcePreviewGenerator {
    GDCLASS(EditorImagePreviewPlugin,EditorResourcePreviewGenerator)

public:
    bool handles(StringView p_type) const override;
    bool generate_small_preview_automatically() const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorImagePreviewPlugin();
};

class EditorBitmapPreviewPlugin : public EditorResourcePreviewGenerator {
    GDCLASS(EditorBitmapPreviewPlugin,EditorResourcePreviewGenerator)

public:
    bool handles(StringView p_type) const override;
    bool generate_small_preview_automatically() const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorBitmapPreviewPlugin();
};

class EditorPackedScenePreviewPlugin : public EditorResourcePreviewGenerator {

public:
    bool handles(StringView p_type) const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;
    Ref<Texture> generate_from_path(StringView p_path, const Size2 &p_size) const override;

    EditorPackedScenePreviewPlugin();
};

class EditorMaterialPreviewPlugin : public EditorResourcePreviewGenerator {

    GDCLASS(EditorMaterialPreviewPlugin,EditorResourcePreviewGenerator)

    RenderingEntity scenario;
    RenderingEntity sphere;
    RenderingEntity sphere_instance;
    RenderingEntity viewport;
    RenderingEntity viewport_texture;
    RenderingEntity light;
    RenderingEntity light_instance;
    RenderingEntity light2;
    RenderingEntity light_instance2;
    RenderingEntity camera;
    mutable SafeFlag preview_done;

    void _preview_done(const Variant &p_udata);

protected:
    static void _bind_methods() {}

public:
    bool handles(StringView p_type) const override;
    bool generate_small_preview_automatically() const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorMaterialPreviewPlugin();
    ~EditorMaterialPreviewPlugin() override;
};

class EditorScriptPreviewPlugin : public EditorResourcePreviewGenerator {
public:
    bool handles(StringView p_type) const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorScriptPreviewPlugin();
};

class EditorAudioStreamPreviewPlugin : public EditorResourcePreviewGenerator {
public:
    bool handles(StringView p_type) const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorAudioStreamPreviewPlugin();
};

class EditorMeshPreviewPlugin : public EditorResourcePreviewGenerator {

    GDCLASS(EditorMeshPreviewPlugin,EditorResourcePreviewGenerator)

    RenderingEntity scenario;
    RenderingEntity mesh_instance;
    RenderingEntity viewport;
    RenderingEntity viewport_texture;
    RenderingEntity light;
    RenderingEntity light_instance;
    RenderingEntity light2;
    RenderingEntity light_instance2;
    RenderingEntity camera;
    mutable SafeFlag preview_done;

    void _preview_done(const Variant &p_udata);

protected:
    static void _bind_methods() { }

public:
    bool handles(StringView p_type) const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;

    EditorMeshPreviewPlugin();
    ~EditorMeshPreviewPlugin() override;
};

class EditorFontPreviewPlugin : public EditorResourcePreviewGenerator {

    GDCLASS(EditorFontPreviewPlugin,EditorResourcePreviewGenerator)

    RenderingEntity viewport;
    RenderingEntity viewport_texture;
    RenderingEntity canvas;
    RenderingEntity canvas_item;
    mutable SafeFlag preview_done;

    void _preview_done(const Variant &p_udata);

protected:
    static void _bind_methods() { }

public:
    bool handles(StringView p_type) const override;
    Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const override;
    Ref<Texture> generate_from_path(StringView p_path, const Size2 &p_size) const override;

    EditorFontPreviewPlugin();
    ~EditorFontPreviewPlugin() override;
};
