#pragma once

#include "core/string.h"
#include "core/vector.h"
#include "core/deque.h"
#include "core/hash_map.h"
#include "EASTL/vector_map.h"

#include "core/reflection_support/reflection_data.h"

struct NamespaceInterface;
struct ConstantInterface;
enum TargetCode : int8_t ;

namespace DocContents {
    struct ConstantDoc;
    struct ClassDoc;
    struct MethodDoc;
} // namespace DocContents

enum class CSAccessLevel {
    Public,
    Internal,
    Protected,
    Private
};
struct TS_TypeLike;
struct TS_Enum;
struct TS_Type;
struct TS_TypeMapper;
struct ResolvedTypeReference {
    const TS_TypeLike *type;
    TypePassBy pass_by = TypePassBy::Value;
    bool operator==(ResolvedTypeReference other) const {
        return type==other.type && pass_by==other.pass_by;
    }
    // Used by default eastl::hash<T>
    explicit operator size_t() const {
        return (uintptr_t(type)>>7) ^ size_t(pass_by);
    }
    String to_c_type(const TS_TypeLike *base_ns) const;
};

struct TS_Signal {
    static HashMap< const SignalInterface*, TS_Signal*> s_ptr_cache;
    const DocContents::MethodDoc *m_resolved_doc = nullptr;
    const SignalInterface* source_type;
    const TS_TypeLike *enclosing_type;

    String cs_name;
    Vector<ResolvedTypeReference> arg_types;
    Vector<String> arg_values; // name of variable or a value.
    Vector<bool> nullable_ref; // true if given parameter is nullable reference, and we need to always pass a valid pointer.
    eastl::vector_map<int,String> arg_defaults;
    bool m_imported=false; // if true, the methods is imported and should not be processed by generators etc.

    const StringView c_name() const {
        assert(source_type);
        return source_type->name;
    }
    static TS_Signal* from_rd(const TS_Type *inside, const SignalInterface* method_interface);
};

struct TS_Function {
    static HashMap< const MethodInterface*, TS_Function*> s_ptr_cache;
    const DocContents::MethodDoc *m_resolved_doc = nullptr;
    const MethodInterface* source_type;
    const TS_TypeLike *enclosing_type;

    String cs_name;
    ResolvedTypeReference return_type;
    Vector<ResolvedTypeReference> arg_types;
    Vector<String> arg_values; // name of variable or a value.
    Vector<bool> nullable_ref; // true if given parameter is nullable reference, and we need to always pass a valid pointer.
    eastl::vector_map<int,String> arg_defaults;
    bool m_imported=false; // if true, the methods is imported and should not be processed by generators etc.

    const StringView c_name() const {
        assert(source_type);
        return source_type->name;
    }
    static TS_Function* from_rd(const TS_TypeLike* inside, const MethodInterface* method_interface);
    static String mapMethodName(StringView method_name, StringView class_name = {}, StringView namespace_name = {});

};
struct TS_Property {
    static HashMap< const PropertyInterface*, TS_Property *> s_ptr_cache;
    String cs_name;
    const TS_Type *m_owner = nullptr;

    struct ResolvedPropertyEntry {
        String subfield_name;
        Vector<ResolvedTypeReference> entry_type; // can be more than one type for some resource types.
        const TS_Function *setter = nullptr;
        const TS_Function* getter = nullptr;
        const DocContents::PropertyDoc* m_docs=nullptr;
        int index;
    };
    Vector<ResolvedPropertyEntry> indexed_entries;
    bool m_imported = false;


    const PropertyInterface *source_type = nullptr;

    static TS_Property *from_rd(const TS_Type *owner, const PropertyInterface *type_interface);
};

struct TS_Constant {
    static HashMap<String,TS_Constant *> constants;
    const ConstantInterface *m_rd_data;
    const DocContents::ConstantDoc *m_resolved_doc;
    TypeReference const_type {"int32_t","",TypeRefKind::Simple};
    String xml_doc;
    String cs_name;
    String value;
    CSAccessLevel access_level = CSAccessLevel::Public;
    const TS_TypeLike *enclosing_type;
    bool m_imported=false; //!< if set to true, this constant is an imported one and should not be generated
    String c_name() const { return m_rd_data->name; }

    static String fix_cs_name(StringView cpp_ns_name);
    static String convert_name(StringView cpp_ns_name);
    static TS_Constant *get_instance_for(const TS_TypeLike *tl,const ConstantInterface *src);
    String relative_path(TargetCode tgt,const TS_TypeLike *rel_to=nullptr) const;
};


struct TS_TypeLike {
protected:
    String m_cs_name;
public:
    enum TypeKind {
        NAMESPACE,
        CLASS,
        ENUM
    };

    // Support for tree of nesting structures - namespace in another namespace, type in namespace,nested types etc.
    const TS_TypeLike *nested_in=nullptr;
    // support for inheritance : class/struct but also used for enum base types
    const TS_TypeLike *base_type=nullptr;

    // Nested types - (enum,type) in type, (namespace,enum,type) in namespace, () in enum
    Vector<TS_TypeLike *> m_children;
    Vector<TS_Constant *> m_constants;
    Vector<TS_Function *> m_functions;
    Vector<TS_Signal *> m_signals;
    const DocContents::ClassDoc *m_docs = nullptr;
    bool m_imported = false;
    bool m_skip_special_functions = false; // modules extending imported class should not generate special functions.

    virtual const String &cs_name() const { return m_cs_name; }
    virtual TypeKind kind() const = 0;
    virtual StringView c_name() const = 0;
    // find a common base type for this and with
    virtual const TS_TypeLike *common_base(const TS_TypeLike *with) const;

    // overriden by Type to also visit base classes.
    virtual TS_TypeLike *find_by(eastl::function<bool(const TS_TypeLike *)> func) const;
    virtual TS_Function * find_method_by_name(TargetCode tgt,StringView name, bool try_parent) const;
    virtual bool enum_name_would_clash_with_property(StringView cs_enum_name) const {
        return false;
    }
    virtual bool needs_instance() const { return false; }

    void visit_kind(TypeKind to_visit,eastl::function<void(const TS_TypeLike *)> visitor) const;
    TS_TypeLike* find_typelike_by_cpp_name(StringView name) const;
    TS_Enum *find_enum_by_cpp_name(StringView name) const;
    TS_Constant *find_constant_by_cpp_name(StringView name) const;

    TS_Type *find_by_cs_name(const String &name) const;
    TS_Type *find_type_by_cpp_name(StringView name) const;
    TS_Constant *add_constant(const ConstantInterface *ci);
    String relative_path(TargetCode tgt,const TS_TypeLike *rel_to=nullptr) const;

    void set_cs_name(StringView n) { m_cs_name=n; }
    void add_enum(TS_Enum* enm) {
        //TODO: add sanity checks here
        m_children.emplace_back(enm);
    }
};

struct TS_Namespace;

struct TS_Module {
    static HashMap<String, TS_Module * > s_modules;

    static TS_Module *find_module(StringView name);
    static TS_Module *create_instance(const ReflectionData &src,bool is_imported);
    static String convert_ns_name(StringView cpp_ns_name);

    TS_Namespace *create_ns(const String & access_path, const NamespaceInterface &src);
    TS_Namespace *find_ns(StringView full_ns_name);
    String m_name;
    bool m_imported;
    const ReflectionData *m_source=nullptr;
    HashMap<String,TS_Namespace *> m_namespaces;
    Vector<TS_Module *> m_imports;

};

struct TS_Namespace : public TS_TypeLike {
    friend struct TS_Module;
private:
    const NamespaceInterface *m_source;
public:
    //static TS_Namespace *get_instance_for(const String &access_path, const NamespaceInterface &src);

    static TS_Namespace* from_path(StringView path);
    static TS_Namespace *from_path(Span<const StringView> path);


    TypeKind kind() const override { return NAMESPACE; }
    StringView c_name() const override { return m_source->name; }

    TS_Type* find_or_create_by_cpp_name(const String& name);
    Vector<StringView> cs_path_components() const;

    TS_Module *m_module=nullptr;

};

struct TS_Type : public TS_TypeLike {
    static HashMap< const TypeInterface*, TS_Type*> s_ptr_cache;
    const TypeInterface* source_type;
    Vector<TS_Property *> m_properties;
    mutable int pass=0;
    bool m_value_type = false; // right now used to mark struct types

    static TS_Type* create_type(const TS_TypeLike *owning_type, const TypeInterface * type_interface);
    static String convert_name(StringView name) {
        return String(name.starts_with('_') ? name.substr(1) : name);
    }
    static TS_Type* by_rd(const TypeInterface* type_interface) {
        return s_ptr_cache[type_interface];
    }

    TypeKind kind() const override { return CLASS; }
    StringView c_name() const override { return source_type->name; }
    // If this object is not a singleton, it needs the instance pointer.
    bool needs_instance() const override { return !source_type->is_singleton; }
    TS_Property * find_property_by_name(StringView name) const;
    TS_Property * find_property_by_exact_name(StringView name) const;

    String get_property_path_by_func(const TS_Function *f) const;
    // search in base class first, then enclosing space
    TS_TypeLike *find_by(eastl::function<bool(const TS_TypeLike *)> func) const override;
    TS_Function * find_method_by_name(TargetCode tgt,StringView name, bool try_parent) const override;
    bool enum_name_would_clash_with_property(StringView cs_enum_name) const override;
};

struct TS_Enum : public TS_TypeLike {
    static HashMap<String,TS_Enum *> enums;
    String static_wrapper_class;
    const EnumInterface *m_rd_data;
    ResolvedTypeReference underlying_val_type;

    TypeKind kind() const override { return ENUM; }
    StringView c_name() const override {
        if(!static_wrapper_class.empty()) { // for synthetic enums - those that don't actually have mapped struct but their name refer to it by `StructName::` syntax
            if(m_rd_data->cname.starts_with(static_wrapper_class))
                return StringView(m_rd_data->cname).substr(static_wrapper_class.size()+2); // static classname + "::"
        }
        return m_rd_data->cname;
    }

    static String convert_name(const String &access_path, StringView cpp_ns_name);
    static TS_Enum *get_instance_for(const TS_TypeLike *enclosing,const String &access_path,const EnumInterface *src);
};
