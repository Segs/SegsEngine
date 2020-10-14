/*************************************************************************/
/*  class_db.h                                                           */
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

#include "core/hash_map.h"
#include "core/string_name.h"
#include "core/hashfuncs.h"
#include "core/variant.h"
#include "core/hash_set.h"
#include "core/list.h"
#include "core/method_info.h"

#include "EASTL/vector.h"

#include <initializer_list>

class MethodBind;
class RWLock;
template<class T>
using Vector = eastl::vector<T,wrap_allocator>;

#define DEFVAL(m_defval) Variant(m_defval)

//#define SIMPLE_METHODDEF

#ifdef DEBUG_METHODS_ENABLED

struct MethodDefinition {

    StringName name;
    int arg_count=0;
    //eastl::vector<StringName,wrap_allocator> args;
    MethodDefinition() = default;
    MethodDefinition(const char *p_name) :
            name(p_name) {}
    MethodDefinition(StringName p_name,int count=0) :
            name(p_name),arg_count(count) {}
    MethodDefinition(MethodDefinition &&d) noexcept = default;
    int parameterCount() const { return arg_count; } //args.size();
};

inline MethodDefinition D_METHOD(StringName p_name) {
    return MethodDefinition {p_name,0};
}
template<int N>
inline MethodDefinition D_METHOD(StringName p_name, const char * const(&)[N]) {
    return MethodDefinition { p_name, N};
}

#else

//#define NO_VARIADIC_MACROS

#ifdef NO_VARIADIC_MACROS

static _FORCE_INLINE_ const char *D_METHOD(const char *m_name, ...) {
    return m_name;
}

#else

// When DEBUG_METHODS_ENABLED is set this will let the engine know
// the argument names for easier debugging.
#define D_METHOD(m_c, ...) m_c

#endif

#endif
// Function that has to be specialized and visible at the point register_class or register_custom_instance_class is called
template<class T>
void register_custom_data_to_otdb() {
    //NOTE: remember to override this when needed
}

class GODOT_EXPORT ClassDB {
public:
    enum APIType {
        API_INVALID=-1,
        API_CORE,
        API_EDITOR,
        API_CLIENT,
        API_SERVER, // server only APIs ?
        API_NONE
    };

public:
    struct PropertySetGet {
        StringName setter;
        StringName getter;
        MethodBind *_setptr;
        MethodBind *_getptr;
        int index;
        VariantType type;
    };
    struct EnumDescriptor {
        StringName underlying_type;
        Vector<StringName> enumerators;
    };

    struct ClassInfo {
        APIType api = API_NONE;
        ClassInfo *inherits_ptr=nullptr;
        const void *class_ptr=nullptr;
        HashMap<StringName, MethodBind *> method_map;
        HashMap<StringName, int> constant_map;
        HashMap<StringName, EnumDescriptor > enum_map;
        HashMap<StringName, MethodInfo> signal_map;
        Vector<PropertyInfo> property_list;
#ifdef DEBUG_METHODS_ENABLED
        Vector<StringName> constant_order;
        Vector<StringName> method_order;
        HashSet<StringName> methods_in_properties;
        Vector<MethodInfo> virtual_methods;
        StringName category;
#endif
        HashMap<StringName, PropertySetGet> property_setget;
        String usage_header;

        StringName inherits;
        StringName name;
        bool disabled=false;
        bool exposed=false;
        bool is_namespace=false;
        HashMap<StringName, MethodInfo> &class_signal_map() {return signal_map;}
        Object *(*creation_func)() = nullptr;

        ClassInfo();
        ~ClassInfo();
    };

    struct NamespaceInfo {
        static HashMap<StringName, ClassInfo> classes;
        Vector<NamespaceInfo *> nested_namespaces;
    };

    template <class T>
    static Object *creator() {
        return memnew(T);
    }

    static RWLock *lock;
    static HashMap<StringName, NamespaceInfo> namespaces;
    static HashMap<StringName, ClassInfo> classes;
    static HashMap<StringName, StringName> resource_base_extensions;
    static HashMap<StringName, StringName> compat_classes;

#ifdef DEBUG_METHODS_ENABLED
    static MethodBind *bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const MethodDefinition &method_name, std::initializer_list<Variant> def_vals);
    static void _set_class_header(const StringName &p_class, StringView header_file);
#else
    static MethodBind *bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const char *method_name, std::initializer_list<Variant> p_defs);
    static void _set_class_header(const StringName &, const char *) {}
#endif

    static APIType current_api;

    static void _add_class2(const StringName &p_class, const StringName &p_inherits);

    static HashMap<StringName, HashMap<StringName, Variant> > default_values;
    static HashSet<StringName> default_values_cached;

public:
    // DO NOT USE THIS!!!!!! NEEDS TO BE PUBLIC BUT DO NOT USE NO MATTER WHAT!!!
    template <class T,class PARENT>
    static void _add_class() {
        if constexpr(std::is_same_v<PARENT,void>)
        {
            _add_class2(T::get_class_static_name(), StringName());
        }
        else
            _add_class2(T::get_class_static_name(), PARENT::get_class_static_name());
    }
    static void add_namespace(StringName ns,StringView header_file) {
        GLOBAL_LOCK_FUNCTION
        ERR_FAIL_COND(classes.find(ns)!=classes.end());
        classes[ns] = ClassInfo();
        ClassInfo &ti = classes[ns];
        ti.name = ns;
        ti.inherits = StringName();
        ti.api = current_api;
        ti.inherits_ptr=nullptr;
        ti.exposed = true;
        ti.is_namespace=true;
#ifdef DEBUG_METHODS_ENABLED
        ti.usage_header=header_file;
#endif
    }
    template <class T>
    static void register_class() {

        GLOBAL_LOCK_FUNCTION
        T::initialize_class();
        auto iter = classes.find(T::get_class_static_name());
        ERR_FAIL_COND(iter==classes.end());
        ClassInfo &ci(iter->second);
        ci.creation_func = &creator<T>;
        ci.exposed = true;
        ci.class_ptr = T::get_class_ptr_static();
        T::register_custom_data_to_otdb();
    }

    template <class T>
    static void register_virtual_class() {

        GLOBAL_LOCK_FUNCTION
        T::initialize_class();
        auto iter = classes.find(T::get_class_static_name());
        ERR_FAIL_COND(iter==classes.end());
        ClassInfo &ci(iter->second);
        ci.exposed = true;
        ci.class_ptr = T::get_class_ptr_static();
        //nothing
    }

    template <class T>
    static Object *_create_ptr_func() {

        return T::create();
    }

    template <class T>
    static void register_custom_instance_class() {

        GLOBAL_LOCK_FUNCTION
        T::initialize_class();
        auto iter = classes.find(T::get_class_static_name());
        ERR_FAIL_COND(iter==classes.end());
        ClassInfo &ci(iter->second);
        ci.exposed = true;
        ci.class_ptr = T::get_class_ptr_static();
        ci.creation_func = &_create_ptr_func<T>;
        T::register_custom_data_to_otdb();
    }
    static bool bind_helper(MethodBind *bind,const char * instance_type,const StringName &p_name);
    static bool can_bind(const StringName &classname, const StringName &p_name);

    static void get_class_list(Vector<StringName> *p_classes);
    static void get_inheriters_from_class(const StringName &p_class, Vector<StringName> *p_classes);
    static void get_direct_inheriters_from_class(const StringName &p_class, Vector<StringName> *p_classes);
    static StringName get_parent_class_nocheck(const StringName &p_class);
    static StringName get_parent_class(const StringName &p_class);
    static StringName get_compatibility_remapped_class(const StringName &p_class);
    static bool class_exists(const StringName &p_class);
    static bool is_parent_class(const StringName &p_class, const StringName &p_inherits);
    static bool can_instance(const StringName &p_class);
    static Object *instance(const StringName &p_class);
    static APIType get_api_type(const StringName &p_class);

    static uint64_t get_api_hash(APIType p_api);

    static void add_signal(StringName p_class, MethodInfo && p_signal);
    static bool has_signal(StringName p_class, StringName p_signal);
    static bool get_signal(StringName p_class, StringName p_signal, MethodInfo *r_signal);
    static void get_signal_list(StringName p_class, Vector<MethodInfo> *p_signals, bool p_no_inheritance = false);

    static void add_property_group(StringName p_class, const char *p_name, const char *p_prefix = nullptr);
    static void add_property_array(StringName p_class, const char *p_name, int elem_count, const char *p_prefix = nullptr);
    static void add_property(StringName p_class, const PropertyInfo &p_pinfo, const StringName &p_setter, const StringName &p_getter, int p_index = -1);
    static void set_property_default_value(StringName p_class, const StringName &p_name, const Variant &p_default);
    static void get_property_list(StringName p_class, Vector<PropertyInfo> *p_list, bool p_no_inheritance = false, const Object *p_validator = nullptr);
    static bool set_property(Object *p_object, const StringName &p_property, const Variant &p_value, bool *r_valid = nullptr);
    static bool get_property(Object *p_object, const StringName &p_property, Variant &r_value);
    static bool has_property(const StringName &p_class, const StringName &p_property, bool p_no_inheritance = false);
    static int get_property_index(const StringName &p_class, const StringName &p_property, bool *r_is_valid = nullptr);
    static VariantType get_property_type(const StringName &p_class, const StringName &p_property, bool *r_is_valid = nullptr);
    static StringName get_property_setter(StringName p_class, const StringName &p_property);
    static StringName get_property_getter(StringName p_class, const StringName &p_property);

    static bool has_method(StringName p_class, StringName p_method, bool p_no_inheritance = false);
    static void set_method_flags(StringName p_class, StringName p_method, int p_flags);

    static void get_method_list(const StringName& p_class, Vector<MethodInfo> *p_methods, bool p_no_inheritance = false, bool p_exclude_from_properties = false);
    static MethodBind *get_method(StringName p_class, StringName p_name);
    static HashMap<StringName, MethodInfo> *get_signal_list(const StringName& p_class);

    static void add_virtual_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual = true);
    static void get_virtual_methods(const StringName &p_class, Vector<MethodInfo> *p_methods, bool p_no_inheritance = false);

    static void register_enum_type(const StringName &p_class,const StringName &p_enum,const StringName &p_underlying_type);
    static void bind_integer_constant(const StringName &p_class, const StringName &p_enum, const StringName &p_name, int p_constant);
    static void get_integer_constant_list(const StringName &p_class, List<String> *p_constants, bool p_no_inheritance = false);
    static int get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success = nullptr);

    static StringName get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance = false);
    static void get_enum_list(const StringName &p_class, List<StringName> *p_enums, bool p_no_inheritance = false);
    static void get_enum_constants(const StringName &p_class, const StringName &p_enum, List<StringName> *p_constants, bool p_no_inheritance = false);

    static Variant class_get_default_property_value(const StringName &p_class, const StringName &p_property, bool *r_valid = nullptr);

    static StringName get_category(const StringName &p_node);

    static void set_class_enabled(StringName p_class, bool p_enable);
    static bool is_class_enabled(StringName p_class);

    static bool is_class_exposed(StringName p_class);

    static void add_resource_base_extension(const StringName &p_extension, const StringName &p_class);
    static void get_resource_base_extensions(Vector<String> &p_extensions);
    static void get_extensions_for_type(const StringName &p_class, Vector<String> *p_extensions);

    static void add_compatibility_class(const StringName &p_class, const StringName &p_fallback);
    static void init();

    static void set_current_api(APIType p_api);
    static APIType get_current_api();
    static void cleanup_defaults();
    static void cleanup();
};

#ifdef DEBUG_METHODS_ENABLED

#define BIND_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant(get_class_static_name(), StringName(), #m_constant, m_constant);
#define BIND_NS_CONSTANT(ns,m_constant) \
    ClassDB::bind_integer_constant(#ns, StringName(), #m_constant, int(ns::m_constant));

#define REGISTER_ENUM(name,type) \
    ClassDB::register_enum_type(get_class_static_name(),get_class_static_name()+"::"+#name, #type);\
    static_assert(eastl::is_same_v<eastl::underlying_type_t<name>,type>);

#define BIND_ENUM_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant(get_class_static_name(), __constant_get_enum_name(m_constant, #m_constant), #m_constant, m_constant)
#define BIND_NS_ENUM_CONSTANT(m_namespace,m_constant) \
    ClassDB::bind_integer_constant(#m_namespace, __constant_get_enum_name(m_namespace::m_constant, #m_constant), #m_constant, int(m_namespace::m_constant))
#define BIND_NS_ENUM_CLASS_CONSTANT(m_namespace,m_eclass,m_constant) \
    ClassDB::bind_integer_constant(#m_namespace, __constant_get_enum_name(m_namespace::m_eclass::m_constant, #m_eclass "::" #m_constant), #m_constant, int(m_namespace::m_eclass::m_constant))

#define BIND_GLOBAL_ENUM_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant("@", __constant_get_enum_name(m_constant, #m_constant), #m_constant, int(m_constant))

#else

#define BIND_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant(get_class_static_name(), StringName(), #m_constant, m_constant);

#define BIND_NS_CONSTANT(ns,m_constant) \
    ClassDB::bind_integer_constant(#ns, StringName(), #m_constant, int(ns::m_constant));

#define BIND_ENUM_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant(get_class_static_name(), StringName(), #m_constant, m_constant);

#define BIND_NS_ENUM_CONSTANT(m_namespace,m_constant) \
    ClassDB::bind_integer_constant(get_class_static_name(), StringName(), #m_constant, int(m_namespace::m_constant));

#define BIND_NS_ENUM_CLASS_CONSTANT(m_namespace,m_eclass,m_constant) \
    ClassDB::bind_integer_constant(#m_namespace, StringName(), #m_constant, int(m_namespace::m_eclass::m_constant))

#define REGISTER_ENUM(name,type) \
    ClassDB::register_enum_type(get_class_static_name(),#name, #type);

#define BIND_GLOBAL_ENUM_CONSTANT(m_constant) \
    ClassDB::bind_integer_constant("@", StringName(), #m_constant, int(m_constant))


#endif

#ifdef TOOLS_ENABLED
#define BIND_VMETHOD(m_method) \
    ClassDB::add_virtual_method(get_class_static_name(), m_method)

#else

#define BIND_VMETHOD(m_method)

#endif
