/*************************************************************************/
/*  undo_redo.h                                                          */
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

#include "core/reference.h"

struct GODOT_EXPORT UndoableAction {
    virtual ~UndoableAction() {}
    virtual StringName name() const = 0;
    virtual void redo() = 0;
    virtual void undo() = 0;
    // checks if operation is still applicable.
    virtual bool can_apply() = 0;
};

class GODOT_EXPORT UndoRedo : public Object {

    GDCLASS(UndoRedo,Object)

    OBJ_SAVE_TYPE(UndoRedo)

public:
    enum MergeMode : int8_t {
        MERGE_DISABLE,
        MERGE_ENDS,
        MERGE_ALL
    };

    using CommitNotifyCallback = void (*)(void *, StringView);
    Variant _add_do_method(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
    Variant _add_undo_method(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

    using MethodNotifyCallback = void (*)(void *, Object *, const StringName &, const Variant &, const Variant &, const Variant &, const Variant &, const Variant &);
    using PropertyNotifyCallback = void (*)(void *, Object *, const StringName &, const Variant &);

private:
    struct PrivateData;
    PrivateData *pimpl=nullptr;

protected:
    static void _bind_methods();

public:
    void create_action(StringView p_name, MergeMode p_mode = MERGE_DISABLE);
    void create_action_pair(StringView p_name, GameEntity owner,eastl::function<void()> do_actions, eastl::function<void()> undo_actions, MergeMode p_mode = MERGE_DISABLE);
    void add_action(UndoableAction *p_action);

    void add_do_method(Object *p_object, const StringName &p_method, VARIANT_ARG_LIST);
    void add_do_method(eastl::function<void()> func,GameEntity owner = entt::null);

    void add_undo_method(Object *p_object, const StringName &p_method, VARIANT_ARG_LIST);
    void add_undo_method(eastl::function<void()> func, GameEntity owner = entt::null);

    void add_do_property(Object *p_object, StringView p_property, const Variant &p_value);
    void add_undo_property(Object *p_object, StringView p_property, const Variant &p_value);
    void add_do_reference(Object *p_object);
    void add_undo_reference(Object *p_object);

    bool is_committing_action() const;
    void commit_action();

    bool redo();
    bool undo();
    StringView get_current_action_name() const;
    void clear_history(bool p_increase_version = true);

    bool has_undo() const;
    bool has_redo() const;

    uint64_t get_version() const;

    void set_commit_notify_callback(CommitNotifyCallback p_callback, void *p_ud);

    void set_method_notify_callback(MethodNotifyCallback p_method_callback, void *p_ud);
    void set_property_notify_callback(PropertyNotifyCallback p_property_callback, void *p_ud);

    ~UndoRedo() override;
private:
    //NOTE: only constructed from editor code, consider moving it there
    friend class EditorData;
    friend class EditorSettingsDialog;
    UndoRedo();
};


