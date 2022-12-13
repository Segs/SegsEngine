/*************************************************************************/
/*  range.h                                                              */
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

#include "core/hash_set.h"
#include "scene/gui/control.h"

class GODOT_EXPORT Range : public Control {

    GDCLASS(Range,Control)

    struct Shared {
        real_t val, min, max;
        real_t step, page;
        bool exp_ratio;
        bool allow_greater;
        bool allow_lesser;
        HashSet<Range *> owners;
        void emit_value_changed();
        void emit_changed(StringName p_what = StringName());
    };

    Shared *shared;

    void _ref_shared(Shared *p_shared);
    void _unref_shared();


    void _value_changed_notify();
    void _changed_notify(StringName p_what = "");
public:
    void _share(Node *p_range);
protected:
    virtual void _value_changed(double) {}

    static void _bind_methods();

    bool _rounded_values;

public:
    void set_value(real_t p_val);
    void set_min(real_t p_min);
    void set_max(real_t p_max);
    void set_step(real_t p_step);
    void set_page(real_t p_page);
    void set_as_ratio(real_t p_value);

    real_t get_value() const;
    real_t get_min() const;
    real_t get_max() const;
    real_t get_step() const;
    real_t get_page() const;
    real_t get_as_ratio() const;

    void set_use_rounded_values(bool p_enable);
    bool is_using_rounded_values() const;

    void set_exp_ratio(bool p_enable);
    bool is_ratio_exp() const;

    void set_allow_greater(bool p_allow);
    bool is_greater_allowed() const;

    void set_allow_lesser(bool p_allow);
    bool is_lesser_allowed() const;

    void share(Range *p_range);
    void unshare();

    String get_configuration_warning() const override;

    Range();
    ~Range() override;
};
