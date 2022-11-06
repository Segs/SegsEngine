/*************************************************************************/
/*  ref_ptr.cpp                                                          */
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

#include "ref_ptr.h"

#include "core/reference.h"
#include "core/resource.h"
#include "core/rid.h"

RefPtr &RefPtr::operator=(const RefPtr &p_other) {
    if (data == p_other.data) {
        return *this;
    }

    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    Ref<RefCounted> *ref_other = reinterpret_cast<Ref<RefCounted> *>(&p_other.data);

    *ref = *ref_other;
    return *this;
}

bool RefPtr::operator==(const RefPtr &p_other) const noexcept {
    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    Ref<RefCounted> *ref_other = reinterpret_cast<Ref<RefCounted> *>(&p_other.data);

    return *ref == *ref_other;
}

RefPtr::RefPtr(const RefPtr &p_other) {
    memnew_placement(&data, Ref<RefCounted>);

    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    Ref<RefCounted> *ref_other = reinterpret_cast<Ref<RefCounted> *>(&p_other.data);

    *ref = *ref_other;
}

bool RefPtr::is_null() const {
    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    return data == 0 || not(*ref);
}

RenderingEntity RefPtr::get_rid() const {
    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    if (not *ref) {
        return entt::null;
    }
    Resource *res = object_cast<Resource>(ref->get());
    if (res) {
        return res->get_rid();
    }
    return entt::null;
}

RID RefPtr::get_phys_rid() const
{
    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    if (not *ref) {
        return RID();
    }
    Resource *res = object_cast<Resource>(ref->get());
    if (res) {
        return res->get_phys_rid();
    }
    return RID();
}

void RefPtr::unref() {
    if (0 == data) {
        return;
    }

    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    ref->unref();
}

RefPtr::RefPtr() noexcept {
    static_assert(sizeof(Ref<RefCounted>) <= sizeof(data));
    memnew_placement(&data, Ref<RefCounted>);
}

RefPtr::~RefPtr() {
    if (!data) {
        return;
    }
    Ref<RefCounted> *ref = reinterpret_cast<Ref<RefCounted> *>(&data);
    ref->~Ref<RefCounted>();
}
