/*************************************************************************/
/*  ref_ptr.cpp                                                          */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

void RefPtr::operator=(const RefPtr &p_other) {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    Ref<Reference> *ref_other = reinterpret_cast<Ref<Reference> *>(&p_other.data);

    *ref = *ref_other;
}

bool RefPtr::operator==(const RefPtr &p_other) const {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    Ref<Reference> *ref_other = reinterpret_cast<Ref<Reference> *>(&p_other.data);

    return *ref == *ref_other;
}

RefPtr::RefPtr(const RefPtr &p_other) {

    memnew_placement(&data, Ref<Reference>);

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    Ref<Reference> *ref_other = reinterpret_cast<Ref<Reference> *>(&p_other.data);

    *ref = *ref_other;
}

bool RefPtr::is_null() const {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    return not (*ref);
}

RID RefPtr::get_rid() const {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    if ( not *ref)
        return RID();
    Resource *res = Object::cast_to<Resource>(ref->get());
    if (res)
        return res->get_rid();
    return RID();
}

void RefPtr::unref() {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    ref->unref();
}

RefPtr::RefPtr() {
    static_assert(sizeof(Ref<Reference>) <= sizeof(data));
    memnew_placement(&data, Ref<Reference>);
}

RefPtr::~RefPtr() {

    Ref<Reference> *ref = reinterpret_cast<Ref<Reference> *>(&data);
    ref->~Ref<Reference>();
}
