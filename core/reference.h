/*************************************************************************/
/*  reference.h                                                          */
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

#include "core/object.h"
#include "core/ref_ptr.h"
#include "core/safe_refcount.h"
#include "core/typesystem_decls.h"

class GODOT_EXPORT RefCounted : public Object {

    GDCLASS(RefCounted,Object)

    friend class RefBase;
    SafeRefCount refcount;
    SafeRefCount refcount_init;

protected:
    static void _bind_methods();

public:
    bool is_referenced() const { return refcount_init.get() != 1; }
    bool init_ref();
    bool reference(); // returns false if refcount is at zero and didn't get increased
    bool unreference();
    int reference_get_count() const;

    RefCounted();
    ~RefCounted() override;
};

class GODOT_EXPORT EncodedObjectAsID : public RefCounted {
    GDCLASS(EncodedObjectAsID, RefCounted)

    GameEntity id {entt::null};

protected:
    static void _bind_methods();

public:
    void set_object_id(GameEntity p_id);
    GameEntity get_object_id() const;

    EncodedObjectAsID() = default;
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
    constexpr Ref() noexcept : reference(nullptr) {}

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

    Ref(const Variant &p_variant);


    ~Ref() {
        unref();
    }

    Ref &operator=(const Ref &p_from) {
        operator=(p_from.reference);
        return *this;
    }

    Ref & operator=(Ref &&p_from) noexcept {
        // after swap p_from holds our data, and will deref it in it's destructor.
        eastl::swap(reference,p_from.reference);
        return *this;
    }

    template <class T_Other>
    Ref &operator=(const Ref<T_Other> &p_from) {
        operator=(p_from.reference);
        return *this;
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

    [[nodiscard]] operator Variant() const {
        return Variant(get_ref_ptr());
    }

    [[nodiscard]] RefPtr get_ref_ptr() const {

        RefPtr refptr;
        Ref<RefCounted> *irr = reinterpret_cast<Ref<RefCounted> *>(refptr.get());
        *irr = *this;
        return refptr;
    }

    [[nodiscard]] T *get() const { return reference; }

    void reset()
    {
        T* const temp = reference;
        reference = nullptr;
        if(temp && temp->unreference()) {
            memdelete(temp);
        }
    }
};
//Q_DECLARE_SMART_POINTER_METATYPE(Ref)
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
inline Ref<T> refFromRefPtr(const RefPtr &p_refptr) {

    Ref<RefCounted> *irr = reinterpret_cast<Ref<RefCounted> *>(p_refptr.get());
    RefCounted *refb = irr->get();
    if (!refb) {
        return Ref<T>();
    }
    return Ref<T>(object_cast<T>(refb));
}
template<class T>
inline Ref<T> refFromVariant(const Variant &p_variant) {
    //TODO: SEGS: notify about failed type conversions?
    RefPtr refptr(p_variant.as<RefPtr>());
    return refFromRefPtr<T>(refptr);
}
template<class T>
Ref<T>::Ref(const Variant &p_variant) {
   RefPtr refptr = p_variant.as<RefPtr>();
   reference = nullptr;
   *this = refFromRefPtr<T>(refptr);
}

template <class T, class U>
Ref<T> dynamic_ref_cast(const Ref<U>& intrusivePtr)
{
    return Ref<T>(object_cast<T>(intrusivePtr.get()));
}

template <class T, class U>
Ref<T> static_ref_cast(const Ref<U>& intrusivePtr)
{
    return Ref<T>(static_cast<T *>(intrusivePtr.get()));
}

using REF = Ref<RefCounted>;

class GODOT_EXPORT WeakRef : public RefCounted {

    GDCLASS(WeakRef,RefCounted)

    GameEntity ref {entt::null};

protected:
    static void _bind_methods();

public:
    Variant get_ref() const;
    void set_obj(Object *p_object);
    void set_ref(const REF &p_ref);

    WeakRef() = default;
};

template <class T, typename>
struct GetTypeInfo;

template <class T>
struct GetTypeInfo<Ref<T>,void> {
    static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::RefValue;

    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo{ nullptr,T::get_class_static(),T::get_class_static(),int8_t(VariantType::OBJECT), PropertyHint::ResourceType };
    }
};

template <class T>
struct GetTypeInfo<const Ref<T> &,void> {
    static const VariantType VARIANT_TYPE = VariantType::OBJECT;
    static const GodotTypeInfo::Metadata METADATA = GodotTypeInfo::METADATA_NONE;
    constexpr static const TypePassBy PASS_BY = TypePassBy::ConstRefReference;

    constexpr static inline RawPropertyInfo get_class_info() {
        return RawPropertyInfo { nullptr,T::get_class_static(),T::get_class_static(),int8_t(VariantType::OBJECT), PropertyHint::ResourceType };
    }
};


namespace eastl {
template<typename T>
struct hash<Ref<T>> {
    size_t operator()(const Ref<T> &np) const {
        return eastl::hash<uint64_t>()( uint64_t(np.get())/next_power_of_2(sizeof(T)) );
    }
};
}
