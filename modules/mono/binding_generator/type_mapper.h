#pragma once
#include "core/map.h"
#include "type_system.h"

#include "core/string.h"
#include "core/vector.h"
#include "core/deque.h"
#include "core/hash_map.h"
#include "core/reflection_support/reflection_data.h"

#include "EASTL/vector_map.h"

struct TS_TypeLike;
struct TS_Enum;
struct TS_Namespace;
struct TS_Type;
struct ProjectContext;

enum TargetCode : int8_t {
    CS_INTERFACE,
    CS_GLUE,
    CPP_IMPL
};
struct TS_TypeWrapper {
    const String *map_prepare; // a code block to prepare the value for transformation ( checks etc. )
    const String *execute_pattern; // a code block
    const String *icall_perform; // a code block
};
struct TS_TypeResolver {
private:
    Map<String, const TS_TypeLike *> from_c_name_to_mapping;
    Map<String, const TS_TypeLike *> from_cs_name_to_mapping;

    TS_TypeResolver() {}
public:
    static TS_TypeResolver &get();

    TS_TypeResolver(const TS_TypeResolver &) = delete;
    TS_TypeResolver &operator=(const TS_TypeResolver &) = delete;

    bool isRegisteredType(StringView type_name) const {
        return from_c_name_to_mapping.contains_as(type_name);
    }
    ResolvedTypeReference resolveType(const TypeReference& ref);
    ResolvedTypeReference resolveType(StringView name,StringView path);

    ResolvedTypeReference registerType(const TS_TypeLike *tl, std::initializer_list<StringView> alternate_c_names={});


};
ResolvedTypeReference resolveType(ProjectContext &ctx,const TypeReference& ref);

struct TS_TypeMapper {
    enum IntTypes {
        INT_8,
        UINT_8,
        INT_16,
        UINT_16,
        INT_32,
        UINT_32,
        INT_64,
        UINT_64,
        INT_TYPE_LAST
    };
    enum FloatTypes {
        FLOAT_32,
        DOUBLE_64,
    };

    // SCRIPT TO WRAP -> arguments
    // WRAP TO SCRIPT -> return values, out arguments.
    // WRAP TO CPP -> argument types, transform to cpp arg
    // CPP TO WRAP -> return values, out arguments.
    enum TypemapKind {
        CPP_TO_WRAP_TYPE, // map a cpp type to wrapper type.
        CPP_TO_WRAP_TYPE_OUT, // map a cpp type to returned wrapper type, if not set CPP_TO_WRAP_TYPE is used.
        WRAP_TO_CPP_TYPECHECK, // check provided wrapper argument, boolean expression `( is_a<T>(%input%)==Foo::Type)`
        WRAP_TO_CPP_IN, // function argument from wrapper type to cpp, %input% is the name of the argument and %type% is the target type
        WRAP_TO_CPP_IN_ARG, // patter used for passing argument to cpp function
        WRAP_TO_CPP_VALUECHECK, // after converting through WRAP_TO_CPP_IN, perform this check on converted value.
        WRAP_TO_CPP_OUT, // convert return value from cpp into a wrapper type. %result% contains the name of result variable, %val% a name of value to return
        WRAP_TO_CPP_ARGOUT, // used to return a value using arg-out handles %result%, %input%

        SCRIPT_TO_WRAP_TYPE,
        WRAP_TO_SCRIPT_TYPE_OUT,
        SCRIPT_TO_WRAP_IN_ARG,
        SCRIPT_TO_WRAP_TYPECHECK,
        SCRIPT_TO_WRAP_IN,
        SCRIPT_TO_WRAP_VALUECHECK,
        SCRIPT_TO_WRAP_OUT,
        SCRIPT_TO_WRAP_ARGOUT,
        SCRIPT_CS_DEFAULT_WRAPPER,

//        WRAP_ARG_FROM_MONO,
//        WRAP_MONO_ARG_TYPE,
//        C_INOUT,
//        C_OUTPUT,
//        SC_INPUT, // Map argument of a named Type to script-language specific version
//        SC_INPUT_ARG_TYPE, // Map argument of a named Type to script-language specific version
//        SC_INPUT_ARG_VALUE,
//        SC_OUTPUT, // Map out-argument of a named Type to script-language specific version
//        SC_INOUT, // Map onput/out-argument of a named Type to script-language specific version
//        SC_RETURN, // Map return value type to script-language specific version
//        SC_RETURN_PATTERN, // Map return value type to script-language specific version

//        WRAP_ARG_TYPE, //
//        WRAP_ARG_TYPE_USE, // map incoming scripting type to wrapper argument
//        WRAP_RETURN, // map outgoing type from c to script wrap type
//        WRAP_RETURN_TYPE, // if not set - void and in cases where function returns something we need to pass it through an additional wrapper argument
//        WRAP_RETURN_PATTERN, // expression to execute to perform return.
        TK_MAX
    };

    struct MappingEntry {
        String type;
        String execute_pattern;
        String icall_type; //
    };
    TS_TypeMapper(const TS_TypeMapper &) = delete;
    TS_TypeMapper &operator=(const TS_TypeMapper &) = delete;
private:
    TS_TypeMapper() = default;
/* Type mapping expression elements
 * %type
 * %tmpname
 * %argtype
 * %arg
 * %outval
 * %outtype
 * %tgtarg name of target argument to be written with data marshalled out from %outval
 */
    struct Mapping {
        ResolvedTypeReference underlying_type;
        eastl::vector_map<TypemapKind,String> mappings;
    };
    Dequeue<TypeInterface> builtins;
    Dequeue<TypeInterface> enum_wrappers;

    Dequeue<Mapping> stored_mappings;
    // null typemap is the default used in all cases where a type does not contain specific typemap.
    HashMap<ResolvedTypeReference,Mapping *> m_type_to_mapping;

    ResolvedTypeReference registerBuiltinType(StringView name, StringView cs_name={}, std::initializer_list<StringView> alternate_c_names={});
    ResolvedTypeReference getGodotOpaqueType(StringView name, StringView cs_name={}, bool value=false, std::initializer_list<StringView> alternate_c_names={});
public:

    String mapIntTypeName(IntTypes it) ;
    String mapFloatTypeName(FloatTypes ft) ;
    String mapPropertyName(StringView src_name, StringView class_name = {}, StringView namespace_name = {}) ;
    String mapArgumentName(StringView src_name) ;
    bool shouldSkipMethod(StringView method_name, StringView class_name = {}, StringView namespace_name = {}) ;

    void registerTypeMap(ResolvedTypeReference ti, TypemapKind, StringView pattern);
    void registerTypeMaps(ResolvedTypeReference ti, std::initializer_list<eastl::pair<TypemapKind,StringView>> patterns);
    //TS_TypeWrapper map_type(TypemapKind kind, const TypeReference &ref, StringView instance_name);
    String map_type(TypemapKind kind, const ResolvedTypeReference &ref);


    void register_default_types();
    void register_godot_base_types();

    void register_complex_type(TS_Type * cs);
    //Render selected value, taking current namespace/type into account
    StringView render(TS_TypeWrapper tw, TargetCode tc, const TS_TypeLike *current_type);
    static TS_TypeMapper &get();
};

void register_enum(const TS_Namespace *ns, const TS_TypeLike *parent, TS_Enum *cs_enum);
