#include "type_mapper.h"

#include "generator_helpers.h"
#include "type_system.h"
#include "core/hash_set.h"


#include <QDebug>

String TS_TypeMapper::mapIntTypeName(IntTypes it) {
    switch (it) {
    case INT_8: return "sbyte";
    case UINT_8: return "byte";
    case INT_16: return "short";
    case UINT_16:return "ushort";
    case INT_32: return "int";
    case UINT_32:return "uint";
    case INT_64: return "long";
    case UINT_64:return "ulong";
    }
    assert(false);
    return "";
}

String TS_TypeMapper::mapFloatTypeName(FloatTypes ft) {
    switch(ft) {

    case FLOAT_32:  return "float";
    case DOUBLE_64: return "double";
    }
    assert(false);
    return "";
}


ResolvedTypeReference TS_TypeResolver::resolveType(const TypeReference &ref,const TS_TypeLike *inside) {
    //1. Resolve the provided type
    //
    String actual_name = ref.cname;
    TypePassBy pass_by = ref.pass_by;
    if(pass_by==TypePassBy::ConstReference) {
        if(ref.cname=="Ref" && !ref.template_argument.empty()) {
            actual_name = ref.template_argument;
            pass_by = TypePassBy::ConstRefReference;
        }
    }
    if(ref.is_enum==TypeRefKind::Enum) {
        auto parts = ref.cname.split('.');
        actual_name = String::joined(parts,"::");
    }
    auto iter = from_c_name_to_mapping.find(actual_name);
    if(iter == from_c_name_to_mapping.end()) // try default NS ?
        iter = from_c_name_to_mapping.find("Godot::"+ actual_name);
    if(iter == from_c_name_to_mapping.end() && inside) {
        auto nested_type = inside->find_typelike_by_cpp_name(actual_name);
        return {nested_type,ref.pass_by};
    }
    assert(iter!=from_c_name_to_mapping.end());
    // Resolve some hardcoded templates.
    return {iter->second,ref.pass_by};
}

ResolvedTypeReference TS_TypeResolver::resolveType(StringView name, StringView path)
{
    String actual_name(name);
    auto parts = actual_name.split('.');
    if(!path.empty()) {

        Vector<StringView> parts_path;
        String::split_ref(parts_path,path,"::");
        parts.insert(parts.begin(),parts_path.begin(),parts_path.end());

    }
    actual_name = String::joined(parts,"::");
    auto iter = from_c_name_to_mapping.find(actual_name);
    if(iter == from_c_name_to_mapping.end()) // try default NS ?
        iter = from_c_name_to_mapping.find("Godot::"+ actual_name);
    assert(iter!=from_c_name_to_mapping.end());
    return {iter->second};

}

ResolvedTypeReference TS_TypeResolver::registerType(const TS_TypeLike *tl,std::initializer_list<StringView> alternate_c_names) {
    String full_c_name = tl->relative_path(CPP_IMPL);
    String full_cs_name = tl->relative_path(CS_INTERFACE);

    auto iter = from_c_name_to_mapping.find(full_c_name);
    if(iter!=from_c_name_to_mapping.end())
        return {iter->second};


    from_c_name_to_mapping[full_c_name] = tl;
    if (tl->c_name().starts_with('_')) { // allow using non-underscore names for type lookup
        String helper=full_c_name;
        helper.replace(tl->c_name(),tl->c_name().substr(1));
        from_c_name_to_mapping[helper] = tl;
    }

    for(StringView alt_name : alternate_c_names) {
        from_c_name_to_mapping[String(alt_name)] = tl;
    }

    from_cs_name_to_mapping[full_cs_name] = tl;

    return {tl};
}

TS_TypeResolver &TS_TypeResolver::get()
{
    static TS_TypeResolver *instance=nullptr;
    if(!instance) {
        instance = new TS_TypeResolver;
    }
    return *instance;
}

static ResolvedTypeReference baseType(const ResolvedTypeReference& ref) {
    if(ref.type && ref.type->kind()==TS_TypeLike::ENUM) {
        ResolvedTypeReference underlying=((const TS_Enum *)ref.type)->underlying_val_type;
        underlying.pass_by=ref.pass_by;
        return underlying;
    }
    if(ref.type && ref.type->base_type)
        return { ref.type->base_type ,ref.pass_by};
    return {};
}

String TS_TypeMapper::map_type(TypemapKind kind, const ResolvedTypeReference &ref) {
//  static HashSet<String> already_reported;
    auto iter_mapping = m_type_to_mapping.find(ref);

    // Try `Value` mapping as a fallback.
    if(iter_mapping==m_type_to_mapping.end() && ref.pass_by!=TypePassBy::Value) {
        ResolvedTypeReference val_ref = {ref.type,TypePassBy::Value};
        iter_mapping = m_type_to_mapping.find(val_ref);
    }

    // Try parent type.
    if(iter_mapping==m_type_to_mapping.end()) {

        auto base_type=baseType(ref);
        if(base_type==ref)
            return "";
        // Try base type.
        return map_type(kind, base_type);
    }

    auto &mapping(*iter_mapping->second);
    auto iter_pattern = mapping.mappings.find(kind);
    if(iter_pattern==mapping.mappings.end()) {
        // No such mapping in this specific type, trying part.
        auto base_type=baseType(ref);
        if(base_type==ref)
            return "";
//        if(!ref.type || ref.type->kind()!=TS_TypeLike::ENUM) {
//            String warning_msg(String::CtorSprintf(),"No mapping(%s:%d) available trying in baseType map", ref.type? String(ref.type->c_name()).c_str():"nullptr",kind);
//            if(!already_reported.contains(warning_msg)) {
//                qDebug() << warning_msg.c_str();
//                already_reported.emplace(eastl::move(warning_msg));
//            }
//        }
        return map_type(kind, base_type);
    }
    return iter_pattern->second;
}
void register_enum(const TS_Namespace *ns, const TS_TypeLike *parent, TS_Enum *cs_enum) {
    TS_TypeResolver &res(TS_TypeResolver::get());
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    ResolvedTypeReference val_type = res.registerType(cs_enum);
    cs_enum->base_type = cs_enum->underlying_val_type.type;
    // other type mapping things handled by underlying type map?

    mapper.registerTypeMap(val_type, TS_TypeMapper::SCRIPT_TO_WRAP_OUT, "return (%rettype%)%val%");

}
void TS_TypeMapper::registerTypeMap(ResolvedTypeReference ti, TypemapKind kind, StringView pattern) {

    assert(ti.type != nullptr);

    auto iter= m_type_to_mapping.find(ti);
    Mapping *m;
    if(iter!=m_type_to_mapping.end()) {
        m = iter->second;
    }
    else {
        stored_mappings.emplace_back();
        m_type_to_mapping[ti] = &stored_mappings.back();
        m = &stored_mappings.back();
        m->underlying_type = ti;
    }
    m->mappings[kind] = pattern;
}

void TS_TypeMapper::registerTypeMaps(ResolvedTypeReference ti, std::initializer_list<eastl::pair<TS_TypeMapper::TypemapKind, StringView> > patterns)
{
    auto iter= m_type_to_mapping.find(ti);
    Mapping *m;
    if(iter!=m_type_to_mapping.end()) {
        m = iter->second;
    }
    else {
        stored_mappings.emplace_back();
        m_type_to_mapping[ti] = &stored_mappings.back();
        m = &stored_mappings.back();
        m->underlying_type = ti;
    }
    for(const auto &p : patterns) {
        m->mappings[p.first] = p.second;
    }
}

ResolvedTypeReference TS_TypeMapper::registerBuiltinType(StringView name, StringView cs_name, std::initializer_list<StringView> alternate_c_names) {
    builtins.emplace_back(String(name));
    TS_Type *type_obj=TS_Type::create_type(nullptr, &builtins.back());
    if(cs_name.empty())
        type_obj->set_cs_name(name);
    else
        type_obj->set_cs_name(cs_name);
    type_obj->m_imported = true;
    return TS_TypeResolver::get().registerType(type_obj,alternate_c_names);
}

ResolvedTypeReference TS_TypeMapper::getGodotOpaqueType(
        StringView name, StringView cs_name, bool value, std::initializer_list<StringView> alternate_c_names) {
    auto core_module = TS_Module::find_module("GodotCore");
    auto godot_ns = core_module->find_ns("Godot");
    auto type_obj = godot_ns->find_type_by_cpp_name(name);
    assert(type_obj && type_obj->source_type->is_opaque_type);
    type_obj->m_imported = true;
    type_obj->m_value_type = value;
    if (cs_name.empty())
        type_obj->set_cs_name(name);
    else
        type_obj->set_cs_name(cs_name);

    return TS_TypeResolver::get().registerType(type_obj, alternate_c_names);
}

void TS_TypeMapper::register_default_types() {

    // default typemap
    registerTypeMaps({},{
//                       {WRAP_TO_CPP_IN,"auto %val%=static_cast<%type%>(%input%)"},
                         {WRAP_TO_CPP_OUT,"return static_cast<%type%>(%val%)"}
                     });

    registerTypeMaps(registerBuiltinType("void"), {
                         {CPP_TO_WRAP_TYPE,"void"},
                         {SCRIPT_TO_WRAP_TYPE,"void"},

                     } );
    //registerTypeMap(void_resolved, WRAP_ARG_FROM_MONO, "", "");

    // bool
    registerTypeMaps(registerBuiltinType("bool"), {
                         {CPP_TO_WRAP_TYPE,"MonoBoolean"},
                         { SCRIPT_TO_WRAP_TYPE,"bool"},
                         {WRAP_TO_CPP_IN,"auto %val%=%input%"},

                     }

    );
    // Integer types
    {
        ResolvedTypeReference resolved;
#define INSERT_INT_TYPE(m_kind, m_c_name, ...) \
        resolved = registerBuiltinType(#m_c_name,m_kind, {__VA_ARGS__});\
        registerTypeMaps(resolved, { \
                               { CPP_TO_WRAP_TYPE,#m_c_name },\
                               { WRAP_TO_CPP_OUT,"return %val%"},\
                               { SCRIPT_TO_WRAP_TYPE,m_kind}, \
                         }\
        );

        INSERT_INT_TYPE("sbyte", int8_t,"char");
        INSERT_INT_TYPE("short", int16_t);
        INSERT_INT_TYPE("int", int32_t,"int");

        INSERT_INT_TYPE("byte", uint8_t);
        INSERT_INT_TYPE("ushort", uint16_t);
        INSERT_INT_TYPE("uint", uint32_t);
        INSERT_INT_TYPE("ulong", uint64_t);
        INSERT_INT_TYPE("long", int64_t);
#undef INSERT_INT_TYPE
    }
    // Floating point types
    {
        // float
        registerTypeMaps(registerBuiltinType("float"), {
                             {CPP_TO_WRAP_TYPE,"float"},
                             {WRAP_TO_CPP_OUT,"return %val%"},
                             {SCRIPT_TO_WRAP_TYPE,"float"},
                         }
        );
        registerTypeMaps(registerBuiltinType("double"), {
                             {CPP_TO_WRAP_TYPE,"double *"}, // doubles are passed as pointers, always.
                             {WRAP_TO_CPP_IN,"auto %val%=*%input%"},
                             {WRAP_TO_CPP_OUT,"*%result% = %val%"},
                             {WRAP_TO_CPP_ARGOUT,"%input%"},
                             {SCRIPT_TO_WRAP_TYPE,"double"},
                             {SCRIPT_TO_WRAP_ARGOUT,"out double %input%"}
                         }
        );

    }
}
void TS_TypeMapper::register_godot_base_types() {
    // Variant
    //TODO: original cs_name was using 'object' as cs name but that broke many codegen things around here.
    registerTypeMaps(getGodotOpaqueType("Variant","object"), {
                         {CPP_TO_WRAP_TYPE,"MonoObject *"},
                         {WRAP_TO_CPP_IN,"auto %val%(::mono_object_to_variant(%input%))"},
                         {WRAP_TO_CPP_OUT,"return ::variant_to_mono_object(%val%)"},
                         {SCRIPT_TO_WRAP_TYPE,"object"},
                     });
    // String
    registerTypeMaps(getGodotOpaqueType("String","string"), {
                         {CPP_TO_WRAP_TYPE,"MonoString *"},
                         {WRAP_TO_CPP_IN,"auto %val%(::mono_string_to_godot(%input%))"},
                         {WRAP_TO_CPP_OUT,"return ::mono_string_from_godot(%val%)"},
                         {SCRIPT_TO_WRAP_TYPE,"string"},
    });

    registerTypeMaps(getGodotOpaqueType("NodePath"), {
                         {CPP_TO_WRAP_TYPE,"NodePath *"},
                         {WRAP_TO_CPP_IN_ARG,"*%input%"},
                         {WRAP_TO_CPP_OUT,"return memnew(NodePath(%val%))"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN_ARG,"NodePath.GetPtr(%input%)"},
                         {SCRIPT_TO_WRAP_OUT,"return new NodePath(%val%)"},
    });
    // RID
    registerTypeMaps(getGodotOpaqueType("RID"), {
                         {CPP_TO_WRAP_TYPE,"RID *"},
                         {WRAP_TO_CPP_IN_ARG,"*%input%"},
                         {WRAP_TO_CPP_OUT,"return memnew(RID(%val%))"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN_ARG,"RID.GetPtr(%input%)"},
                         {SCRIPT_TO_WRAP_OUT,"return new RID(%val%)"},
    });
    registerTypeMaps(getGodotOpaqueType("GameEntity","ulong"), {
                         {CPP_TO_WRAP_TYPE,"uint64_t"},
                         {WRAP_TO_CPP_IN_ARG,"GameEntity(%input%)"},
                         {SCRIPT_TO_WRAP_TYPE,"ulong"},
                         {WRAP_TO_CPP_OUT,"return entt::to_integral(%val%)"},
    });
    // type used to pass variable number of arguments
    registerTypeMaps(getGodotOpaqueType("VarArg","params object[]"), {
                         {CPP_TO_WRAP_TYPE,"MonoArray *"},
                         {WRAP_TO_CPP_IN,R"(
int vararg_length = mono_array_length(%input%);
int total_length = %additional_argc% + vararg_length;
ArgumentsVector<Variant> %val%_vals(vararg_length);
ArgumentsVector<const Variant *> %val%(total_length);
%process_varargs%
for (int i = %additional_argc%; i < vararg_length; i++) {
  MonoObject* elem = mono_array_get(%input%, MonoObject*, i);
  %val%_vals[i]= GDMonoMarshal::mono_object_to_variant(elem);
  %val%[0 + i] = &%val%_vals[i];
}
Callable::CallError vcall_error;
)"},
                         {WRAP_TO_CPP_IN_ARG,"temp_%input%.data(),total_length, vcall_error"}, // unpack to 3 argumens
                         {SCRIPT_TO_WRAP_TYPE,"object[]"},

                     }
    );

#define INSERT_STRUCT_TYPE(m_type)                                     \
    if constexpr (true) {                                                                  \
        auto resolved = getGodotOpaqueType(#m_type,{},true); \
        registerTypeMaps(resolved, {\
            {CPP_TO_WRAP_TYPE,"GDMonoMarshal::M_" #m_type "*"},\
            {WRAP_TO_CPP_IN,"auto %val%(MARSHALLED_IN(" #m_type ",%input%))"},    \
            {WRAP_TO_CPP_OUT,"*%result% = (MARSHALLED_OUT(" #m_type ",%val%))"},    \
            {WRAP_TO_CPP_ARGOUT,"%input%"},\
            {SCRIPT_TO_WRAP_TYPE,"ref " #m_type},\
            {SCRIPT_TO_WRAP_IN_ARG,"ref %input%"},\
            {SCRIPT_TO_WRAP_ARGOUT,"out " #m_type " %input%"},\
        });\
    } else\
        void()

    INSERT_STRUCT_TYPE(Vector2);
    INSERT_STRUCT_TYPE(Rect2);
    INSERT_STRUCT_TYPE(Transform2D);
    INSERT_STRUCT_TYPE(Vector3);
    INSERT_STRUCT_TYPE(Basis);
    INSERT_STRUCT_TYPE(Quat);
    INSERT_STRUCT_TYPE(Transform);
    INSERT_STRUCT_TYPE(AABB);
    INSERT_STRUCT_TYPE(Color);
    INSERT_STRUCT_TYPE(Plane);

#define INSERT_ARRAY_FULL(m_name, m_type, m_proxy_t)                          \
    {                                                                         \
        auto resolved = getGodotOpaqueType(#m_type,#m_proxy_t "[]"); \
        registerTypeMaps(resolved, {\
            {CPP_TO_WRAP_TYPE,"MonoArray *"},\
            {WRAP_TO_CPP_IN,""},\
            {WRAP_TO_CPP_IN_ARG,"VectorAutoConverter(%input%)"},    \
            {WRAP_TO_CPP_OUT,"return ::container_to_mono_array(%val%)"},    \
            {SCRIPT_TO_WRAP_TYPE,#m_proxy_t "[]"},\
        });\
    }

#define INSERT_ARRAY_NC_FULL(m_name, m_type, m_proxy_t)                          \
    {                                                                         \
        auto resolved = getGodotOpaqueType(#m_type,#m_proxy_t "[]"); \
        registerTypeMaps(resolved, {\
            {CPP_TO_WRAP_TYPE,"MonoArray *"},\
            {WRAP_TO_CPP_IN,""}, \
            {WRAP_TO_CPP_IN_ARG,"VectorAutoConverter(%input%)"},    \
            {WRAP_TO_CPP_OUT,"*%result% = ::container_to_mono_array(%val%)"},    \
            {SCRIPT_TO_WRAP_TYPE,#m_proxy_t "[]"},\
        });\
    }
#define INSERT_ARRAY(m_type, m_proxy_t) INSERT_ARRAY_FULL(m_type, m_type, m_proxy_t)

    INSERT_ARRAY(PoolIntArray, int);
    INSERT_ARRAY_NC_FULL(VecInt, VecInt, int);
    INSERT_ARRAY_NC_FULL(VecByte, VecByte, byte);
    INSERT_ARRAY_NC_FULL(VecFloat, VecFloat, float);
    INSERT_ARRAY_NC_FULL(VecString, VecString, string);
    INSERT_ARRAY_NC_FULL(VecVector2, VecVector2, Vector2);
    INSERT_ARRAY_NC_FULL(VecVector3, VecVector3, Vector3);
    INSERT_ARRAY_NC_FULL(VecColor, VecColor, Color);
    INSERT_ARRAY_FULL(PoolByteArray, PoolByteArray, byte)


    INSERT_ARRAY(PoolRealArray, float);
    INSERT_ARRAY(PoolStringArray, string);

    INSERT_ARRAY(PoolColorArray, Color);
    INSERT_ARRAY(PoolVector2Array, Vector2);
    INSERT_ARRAY(PoolVector3Array, Vector3);

#undef INSERT_ARRAY
    // Dictionary

    registerTypeMaps(getGodotOpaqueType("Dictionary","Collections.Dictionary"), {
                         {CPP_TO_WRAP_TYPE,"Dictionary *"},
                         {WRAP_TO_CPP_IN,""}, // empty mapping to prevent temporaries
                         {WRAP_TO_CPP_IN_ARG,"*%input%"},
                         {WRAP_TO_CPP_OUT,"return memnew(Dictionary(%val%))"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN_ARG,"%input%.GetPtr()"},
                         {SCRIPT_TO_WRAP_OUT,"return new Collections.Dictionary(%val%)"},
    });

    // Array
    registerTypeMaps(getGodotOpaqueType("Array","Collections.Array"), {
                         {CPP_TO_WRAP_TYPE,"Array *"},
                         {WRAP_TO_CPP_IN,""}, // empty mapping to prevent temporaries
                         {WRAP_TO_CPP_IN_ARG,"ArrConverter(%input%)"},
                         {WRAP_TO_CPP_OUT,"return ToArray(%val%)"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN_ARG,"%input%.GetPtr()"},
                         {SCRIPT_TO_WRAP_OUT,"return new Collections.Array(%val%)"},
    });

    registerTypeMaps(getGodotOpaqueType("Callable","Callable"), {
                         {CPP_TO_WRAP_TYPE,"GDMonoMarshal::M_Callable*"},
                         {WRAP_TO_CPP_IN_ARG,""},
                         {WRAP_TO_CPP_IN_ARG,"::managed_to_callable(*%input%)"},
                         {WRAP_TO_CPP_OUT,"return ::callable_to_managed(%val%)"},
                         {SCRIPT_TO_WRAP_TYPE,"ref Callable"},
                         {SCRIPT_TO_WRAP_IN_ARG,"ref %input%"},
                         {SCRIPT_TO_WRAP_ARGOUT,"out Callable %input%"},

    });
    // Callable
//    itype.c_in = "\t%0 %1_in = " C_METHOD_MANAGED_TO_CALLABLE "(*%1);\n";
//    itype.c_out = "\t*%3 = " C_METHOD_MANAGED_FROM_CALLABLE "(%1);\n";
//    itype.c_arg_in = "&%s_in";
//    itype.cs_in = "ref %s";
//    /* in cs_out, im_type_out (%3) includes the 'out ' part */
//    itype.cs_out = "%0(%1, %3 argRet); return argRet;";
//    itype.im_type_out = "out " + itype.cs_type;
//    itype.ret_as_byref_arg = true;
//    builtin_types.insert(itype.cname, itype);

//    // Signal
//    itype = TypeInterface();
//    itype.name = "Signal";
//    itype.cname = itype.name;
//    itype.proxy_name = "SignalInfo";
//    itype.c_in = "\t%0 %1_in = " C_METHOD_MANAGED_TO_SIGNAL "(*%1);\n";
//    itype.c_out = "\t*%3 = " C_METHOD_MANAGED_FROM_SIGNAL "(%1);\n";
//    itype.c_arg_in = "&%s_in";
//    itype.c_type = itype.name;
//    itype.c_type_in = "GDMonoMarshal::M_SignalInfo*";
//    itype.c_type_out = "GDMonoMarshal::M_SignalInfo";
//    itype.cs_in = "ref %s";
//    /* in cs_out, im_type_out (%3) includes the 'out ' part */
//    itype.cs_out = "%0(%1, %3 argRet); return argRet;";
//    itype.cs_type = itype.proxy_name;
//    itype.im_type_in = "ref " + itype.cs_type;
//    itype.im_type_out = "out " + itype.cs_type;
//    itype.ret_as_byref_arg = true;

    // StringView
    registerTypeMaps(getGodotOpaqueType("StringView","string"), {
                         {CPP_TO_WRAP_TYPE,"MonoString *"},
                         {WRAP_TO_CPP_IN,"TmpString<512> %val%(::mono_string_to_godot(%input%))"},
                         {WRAP_TO_CPP_OUT,"return ::mono_string_from_godot(%val%)"},
                         {SCRIPT_TO_WRAP_TYPE,"string"},
    });
    // StringName
    registerTypeMaps(getGodotOpaqueType("StringName","StringName"), {
                         {CPP_TO_WRAP_TYPE,"StringName *"},
                         {WRAP_TO_CPP_IN,"StringName %val%(%input% ? *%input%:StringName())"},
                         {WRAP_TO_CPP_OUT,"return memnew(StringName(%val%))"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN,"%type% %val% = %input% != null ? %input% : (%type%)\"\";"},
                         {SCRIPT_TO_WRAP_IN_ARG,"StringName.GetPtr(%input%)"},
                         {SCRIPT_TO_WRAP_OUT,"return new Godot.StringName(%val%)"},
                         {SCRIPT_CS_DEFAULT_WRAPPER,"null"}

    });
/*
    // StringName
    itype = TypeInterface();
    itype.name = "StringName";
    itype.cname = itype.name;
    itype.proxy_name = "StringName";
    itype.c_in = "\t%0 %1_in = %1 ? *%1 : StringName();\n";
    itype.c_out = "\treturn memnew(StringName(%1));\n";
    itype.c_arg_in = "&%s_in";
    itype.c_type = itype.name;
    itype.c_type_in = itype.c_type + "*";
    itype.c_type_out = itype.c_type + "*";
    itype.cs_type = itype.proxy_name;
    itype.cs_in = "StringName." CS_SMETHOD_GETINSTANCE "(%0)";
    itype.cs_out = "return new %2(%0(%1));";
    itype.im_type_in = "IntPtr";
    itype.im_type_out = "IntPtr";
    builtin_types.insert(itype.cname, itype);
*/
}


String TS_TypeMapper::mapPropertyName(StringView src_name, StringView class_name, StringView namespace_name) {
    String conv_name = escape_csharp_keyword(snake_to_pascal_case(src_name));
    String mapped_class_name(class_name);
    // Prevent the property and its enclosing type from sharing the same name
    if (conv_name == mapped_class_name) {
        qWarning("Name of property '%s' is ambiguous with the name of its enclosing class '%s'. Renaming property to '%s_'\n",
                 conv_name.c_str(), mapped_class_name.c_str(), conv_name.c_str());

        conv_name += "_";
    }
    return conv_name;
}
void TS_TypeMapper::register_complex_type(TS_Type *cs) {
    auto &resolver(TS_TypeResolver::get());
    assert(resolver.isRegisteredType(cs->c_name())==false);
    if(cs->source_type->is_opaque_type) {
        resolver.registerType(cs);
        return; // opaque typemaps are done `by-hand`
    }
    registerTypeMaps(resolver.registerType(cs), {
                         {CPP_TO_WRAP_TYPE,"Object *"},
                         {CPP_TO_WRAP_TYPE_OUT,"MonoObject *"},
                         {WRAP_TO_CPP_IN_ARG,"AutoRef(%input%)"},
                         {WRAP_TO_CPP_OUT,"return GDMonoUtils::unmanaged_get_managed(AutoUnwrap(%val%))"},
                         {SCRIPT_TO_WRAP_TYPE,"IntPtr"},
                         {SCRIPT_TO_WRAP_IN_ARG,"Object.GetPtr(%input%)"},
                         {WRAP_TO_SCRIPT_TYPE_OUT,"%type%"},

    });
}

TS_TypeMapper &TS_TypeMapper::get()
{
    static TS_TypeMapper *instance=nullptr;
    if(!instance) {
        instance = new TS_TypeMapper;
    }
    return *instance;
}
