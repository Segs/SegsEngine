/*************************************************************************/
/*  editor_resource_preview.h                                            */
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

#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/string.h"
#include "core/list.h"
#include "core/map.h"
#include "scene/main/node.h"
#include "scene/resources/texture.h"

class GODOT_EXPORT EditorResourcePreviewGenerator : public RefCounted {

    GDCLASS(EditorResourcePreviewGenerator,RefCounted)

protected:
    static void _bind_methods();

public:
    virtual bool handles(StringView p_type) const;
    virtual Ref<Texture> generate(const RES &p_from, const Size2 &p_size) const;
    virtual Ref<Texture> generate_from_path(StringView p_path, const Size2 &p_size) const;

    virtual bool generate_small_preview_automatically() const;
    virtual bool can_generate_small_preview() const;

    EditorResourcePreviewGenerator();
};

class GODOT_EXPORT EditorResourcePreview : public Node {

    GDCLASS(EditorResourcePreview,Node)

    static EditorResourcePreview *singleton;

    struct QueueItem {
        Ref<Resource> resource;
        String path;
        Callable callable;
    };
    struct Item {
        Ref<Texture> preview;
        Ref<Texture> small_preview;
        int order;
        uint32_t last_hash;
        uint64_t modified_time;
    };

    List<QueueItem> queue;

    Mutex preview_mutex;
    Semaphore preview_sem;
    Thread thread;
    SafeFlag exit;
    SafeFlag exited;
    int order;
    Map<String, Item> cache;
    Vector<Ref<EditorResourcePreviewGenerator> > preview_generators;

    void _preview_ready(StringView p_str, const Ref<Texture> &p_texture, const Ref<Texture> &p_small_texture, const Callable &callit);
    void _generate_preview(Ref<ImageTexture> &r_texture, Ref<ImageTexture> &r_small_texture, const QueueItem &p_item, StringView cache_base);

    static void _thread_func(void *ud);
    void _thread();


protected:
    static void _bind_methods();

public:
    static EditorResourcePreview *get_singleton();

    // p_receiver_func callback has signature (String p_path, Ref<Texture> p_preview, Ref<Texture> p_preview_small, Variant p_userdata)
    // p_preview will be null if there was an error
    void queue_resource_preview(StringView p_path, const Callable &callback);
    void queue_edited_resource_preview(const Ref<Resource> &p_res, const Callable &callback);
    void queue_edited_resource_preview_lambda(const Ref<Resource> &p_res, Object *owner, eastl::function<void (const String &, const Ref<Texture> &, const Ref<Texture> &)> &&cb);

    void add_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator);
    void remove_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator);
    void check_for_invalidation(StringView p_path);

    void start();
    void stop();

    EditorResourcePreview();
    ~EditorResourcePreview() override;
};
