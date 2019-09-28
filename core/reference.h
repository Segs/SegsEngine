/*************************************************************************/
/*  reference.h                                                          */
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

#pragma once

#include "core/object.h"
#include "core/ref_ptr.h"
#include "core/safe_refcount.h"
#include "core/typesystem_decls.h"
#include <cassert>

template <class T, typename>
struct GetTypeInfo;

class GODOT_EXPORT Reference : public Object {

    GDCLASS(Reference,Object)

    friend class RefBase;
    SafeRefCount refcount;
    SafeRefCount refcount_init;

protected:
    static void _bind_methods();

public:
	_FORCE_INLINE_ bool is_referenced() const { return refcount_init.get() != 1; }
    bool init_ref();
    bool reference(); // returns false if refcount is at zero and didn't get increased
    bool unreference();
    int reference_get_count() const;

    Reference();
    ~Reference() override;
};
enum RefMode {
    AddRef,
    DoNotAddRef
};
template <class T>
class Ref {

    template <typename U>
    friend class Ref;

    T *reference;

public:
    constexpr Ref() : reference(nullptr) {}

    explicit Ref(T *p_reference,RefMode add_ref=AddRef) {
        reference = p_reference;
        if(p_reference && add_ref==AddRef)
            p_reference->reference();
    }
    Ref(const Ref &p_from) : reference(p_from.reference) {
        if (reference)
            reference->reference();
    }
    Ref(Ref &&p_from) noexcept
    {
        reference = p_from.reference;
        p_from.reference = nullptr;
    }

    template <class T_Other>
    Ref(const Ref<T_Other> &p_from) : reference(p_from.reference) {
        if(reference)
            reference->reference();
    }

    explicit Ref(const Variant &p_variant);


    ~Ref() {
        unref();
    }

    Ref &operator=(const Ref &p_from) {
        return operator=(p_from.reference);
    }

    Ref & operator=(Ref &&p_from) noexcept {
        // after swap p_from holds our data, and will deref it in it's destructor.
        eastl::swap(reference,p_from.reference);
        return *this;
    }

    template <class T_Other>
    Ref &operator=(const Ref<T_Other> &p_from) {
        return operator=(p_from.reference);
    }

    Ref& operator=(T* p_Object)
    {
        if(p_Object != reference)
        {
            T* const temp = reference;
            if(p_Object) {
                p_Object->reference();
            }
            reference = p_Object;
            if(temp && temp->unreference()) {
                memdelete(temp);
            }
        }
        return *this;
    }

    T& operator *() const
    {
        return *reference;
    }

    T* operator ->() const
    {
        return reference;
    }

    using bool_ = T *(Ref<T>::*)() const;
    operator bool_() const
    {
        if(reference)
            return &Ref::get;
        return nullptr;
    }

    void unref() {
        //TODO this should be moved to mutexes, since this engine does not really
        // do a lot of referencing on references and stuff
        // mutexes will avoid more crashes?

        if (reference && reference->unreference()) {

            memdelete(reference);
        }
        reference = nullptr;
    }

    operator Variant() const {

        return Variant(get_ref_ptr());
    }
    RefPtr get_ref_ptr() const {

        RefPtr refptr;
        Ref<Reference> *irr = reinterpret_cast<Ref<Reference> *>(refptr.get_data());
        *irr = *this;
        return refptr;
    }
    _FORCE_INLINE_ T *get() const {

        return reference;
    }

    void reset()
    {
        T* const temp = reference;
        reference = nullptr;
        if(temp && temp->unreference()) {
            memdelete(temp);
        }
    }
};
template <typename T, typename U>
bool operator==(Ref<T> const& lhs, Ref<U> const& rhs)
{
    return (lhs.get() == rhs.get());
}

template <typename T, typename U>
bool operator!=(Ref<T> const& lhs, Ref<U> const& rhs)
{
    return (lhs.get() != rhs.get());
}

template <typename T>
bool operator==(Ref<T> const& lhs, T* p)
{
    return (lhs.get() == p);
}

template <typename T>
bool operator!=(Ref<T> const& lhs, T* p)
{
    return (lhs.get() != p);
}

template <typename T>
bool operator==(T* p, Ref<T> const& rhs)
{
    return (p == rhs.get());
}

template <typename T>
bool operator!=(T* p, Ref<T> const& rhs)
{
    return (p != rhs.get());
}

template <typename T, typename U>
bool operator<(Ref<T> const& lhs, Ref<U> const& rhs)
{
    return ((uintptr_t)lhs.get() < (uintptr_t)rhs.get());
}

template <typename T, typename... Args>
inline Ref<T> make_ref_counted(Args&&... args)
    { return Ref<T>(_post_initialize(new (typeid(T).name()) T(eastl::forward<Args>(args)...)),DoNotAddRef); }

template<class T>
inline Ref<T> refFromRefPtr(RefPtr p_refptr) {

    Ref<Reference> *irr = reinterpret_cast<Ref<Reference> *>(p_refptr.get_data());
    Reference *refb = irr->get();
    if (!refb) {
        return Ref<T>();
    }
    return Ref<T>(Object::cast_to<T>(refb));
}
template<class T>
inline Ref<T> refFromVariant(const Variant &p_variant) {
    //TODO: SEGS: notify about failed type conversions?
    RefPtr refptr = p_variant.as<RefPtr>();
    return refFromRefPtr<T>(refptr);
}
template<class T>
Ref<T>::Ref(const Variant &p_variant) {
   RefPtr refptr = p_variant;
   reference = nullptr;
   *this = refFromRefPtr<T>(refptr);
}
//template <class T, class U>
//Ref<T> static_ref_cast(const Ref<U>& intrusivePtr)
//{
//    return static_cast<T*>(intrusivePtr.ptr());
//}
template <class T, class U>
Ref<T> dynamic_ref_cast(const Ref<U>& intrusivePtr)
{
    return Ref<T>(Object::cast_to<T>(intrusivePtr.get()));
}
template <class T, class U>
Ref<T> dynamic_ref_cast(Ref<U>& intrusivePtr)
{
    return Ref<T>(Object::cast_to<T>(intrusivePtr.get()));
}

using REF = Ref<Reference>;

class WeakRef : public Reference {

    GDCLASS(WeakRef,Reference)

    ObjectID ref {0};

protected:
    static void _bind_methods();

public:
    Variant get_ref() const;
    void set_obj(Object *p_object);
    void set_ref(const REF &p_ref);

    WeakRef() {}
};

#ifdef PTRCALL_ENABLED
template <class T>
struct PtrToArg;

template <class T>
struct PtrToArg<Ref<T> > {

    _FORCE_INLINE_ static Ref<T> convert(const void *p_ptr) {

        return Ref<T>(const_cast<T *>(reinterpret_cast<const T *>(p_ptr)));
    }

    _FORCE_INLINE_ static void encode(const Ref<T> &p_val, void *p_ptr) {

        *(Ref<Reference> *)p_ptr = p_val;
    }
};

template <class T>
struct PtrToArg<const Ref<T> &> {

    _FORCE_INLINE_ static Ref<T> convert(const void *p_ptr) {

        return Ref<T>((T *)p_ptr);
    }
};

//this is for RefPtr

template <>
struct PtrToArg<RefPtr> {

    _FORCE_INLINE_ static RefPtr convert(const void *p_ptr) {

        return Ref<Reference>(const_cast<Reference *>(reinterpret_cast<const Reference *>(p_ptr))).get_ref_ptr();
    }

    _FORCE_INLINE_ static void encode(const RefPtr &p_val, void *p_ptr) {

        *(Ref<Reference> *)p_ptr = refFromRefPtr<Reference>(p_val);
    }
};

template <>
struct PtrToArg<const RefPtr &> {

    _FORCE_INLINE_ static RefPtr convert(const void *p_ptr) {

        return Ref<Reference>(const_cast<Reference *>(reinterpret_cast<const Reference *>(p_ptr))).get_ref_ptr();
    }
};

#endif // PTRCALL_ENABLED

#ifdef DEBUG_METHODS_ENABLED

template <class T>
struct GetTypeInfo<Ref<T>,void> {
    static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;

    static RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr,T::get_class_static(),nullptr,int8_t(VariantType::OBJECT), PROPERTY_HINT_RESOURCE_TYPE };
    }
};

template <class T>
struct GetTypeInfo<const Ref<T> &,void> {
    static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;

    static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,T::get_class_static(),nullptr,int8_t(VariantType::OBJECT), PROPERTY_HINT_RESOURCE_TYPE };
    }
};

#endif // DEBUG_METHODS_ENABLED
