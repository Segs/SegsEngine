#include "cs_generator_visitor.h"

#include "generator_helpers.h"
#include "type_system.h"
#include "type_mapper.h"
#include "type_generator.h"
#include "property_generator.h"
#include "docs_helpers.h"

extern void generate_cs_type_constants(const TS_TypeLike* itype, ProjectContext& prj);
extern void generate_cs_type_enums(const TS_TypeLike* itype, ProjectContext& prj);

namespace {
void buildCallArgumentList(const TS_Function *finfo, const eastl::vector_map<String, String> &mapped_args,StringBuilder &out) {
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    int argc = finfo->arg_types.size();
    FixedVector<String,32,false> arg_parts;
    const TS_TypeLike *parent_ns = finfo->enclosing_type;
    while(parent_ns && parent_ns->kind() != TS_TypeLike::NAMESPACE) {
        parent_ns = parent_ns->parent;
    }
    out.append("(");
    if(finfo->enclosing_type) {
        auto iter = mapped_args.find("%self%");
        if(iter!=mapped_args.end()) {
            arg_parts.emplace_back(iter->second);
        }
        else
            arg_parts.emplace_back("Object.GetPtr(this)");
    }

    for(int i=0; i<argc; ++i) {
        auto mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_IN_ARG,finfo->arg_types[i]);
        auto input_arg(finfo->arg_values[i]);
        auto input_type(finfo->arg_types[i]);

        auto iter = mapped_args.find(input_arg);
        if(iter!=mapped_args.end()) {
            if(iter->second.empty()) // skip parameter that was packed into the varargs array.
                continue;
            input_arg = iter->second;
        }
        if(!mapping.empty()) {
            input_arg = eastl::move(mapping.replaced("%input%",input_arg));
        }
        else if(input_type.type->kind()==TS_TypeLike::ENUM) {
            const TS_Enum *en = (const TS_Enum *)(input_type.type);
            String enum_type=en->underlying_val_type.type->cs_name();
            input_arg = eastl::move("("+enum_type+")"+input_arg);
        }
        arg_parts.emplace_back(eastl::move(input_arg));
    }
    auto out_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_ARGOUT, finfo->return_type);
    if (!out_mapping.empty()) {
        auto mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_TYPE, finfo->return_type);
        arg_parts.emplace_back(out_mapping.replaced("%input%", "argRet"));
    }


    out.append(String::joined(arg_parts,", "));
    out.append(")");
}
void mapFunctionArguments(const TS_Function *finfo,GeneratorContext &ctx) {
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    ctx.out.append("(");
    int argc = finfo->arg_types.size();

    for(int i=0; i<argc; ++i) {
        auto arg_type=finfo->arg_types[i].type;
        auto mapping = arg_type->relative_path(TargetCode::CS_INTERFACE,finfo->enclosing_type);
        if(mapping.empty())
            mapping = arg_type->cs_name();
        if(!mapping.empty()) {
            bool nullable_val=false;
            if(finfo->nullable_ref[i]) {
                if(arg_type->kind()==TS_TypeLike::CLASS) {
                    nullable_val = ((const TS_Type *)arg_type)->m_value_type;
                }
            }

            if(nullable_val)
                ctx.out.append("Nullable<");
            ctx.out.append(mapping);
            if(nullable_val)
                ctx.out.append(">");
        }
        else {
            ctx.out.append("MissingWrap<");
            ctx.out.append(arg_type->c_name());
            ctx.out.append(">");
        }
        ctx.out.append(" ");
        ctx.out.append(finfo->arg_values[i]);
        if(!finfo->arg_defaults.empty()) {
            auto defval=finfo->arg_defaults.find(i);
            if(defval!=finfo->arg_defaults.end())
            {
                auto default_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_CS_DEFAULT_WRAPPER, finfo->arg_types[i]);
                ctx.out.append(" = ");
                if(!finfo->nullable_ref[i]) {
                    if(default_mapping.empty())
                        ctx.out.append(String(String::CtorSprintf(),defval->second.c_str(),mapping.c_str()));
                    else {
                        ctx.out.append(default_mapping.replaced("%type%",mapping).replaced("%value%",defval->second));
                    }
                }
                else
                    ctx.out.append("null");
            }
        }
        if(i<argc-1)
            ctx.out.append(", ");
    }
    ctx.out.append(") ");
}

void prepareArgumentLocals(const TS_Function *finfo, eastl::vector_map<String, String> &mapped_args,GeneratorContext &ctx) {
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    if(finfo->enclosing_type && finfo->enclosing_type->kind()==TS_TypeLike::CLASS) {
        auto classtype = (const TS_Type *)finfo->enclosing_type;
        if (classtype->source_type->is_singleton) {
            mapped_args["%self%"] = "ptr";
        }
    }
    int argc = finfo->arg_types.size();
    int additional_argc=0; // used by vararg functions

    for (int i = 0; i < argc; ++i) {
        auto mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_IN, finfo->arg_types[i]);
        bool multiline_mapping = mapping.contains('\n');
        auto input_wrap = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_IN_ARG, finfo->arg_types[i]);

        auto input_arg = finfo->arg_values[i];
        if(finfo->nullable_ref[i]) {
            auto pass_by = finfo->arg_types[i];
            bool nullable_val=false;
            if(pass_by.type->kind()==TS_TypeLike::CLASS) {
                nullable_val = ((const TS_Type *)pass_by.type)->m_value_type;
            }
            String decl;
            String locarg("in_" + input_arg);
            if(nullable_val) {
                decl = "%type% %val% = %input%.HasValue ? %input%.Value : ";
            }
            else
                decl = "%type% %val% = %input% != null ? %input% : ";
            //%type% arg1_in = arg1 != null ? arg1 : new Godot.Collections.Array {};
            decl += finfo->arg_defaults.find(i)->second.replaced("%s", "%type%");
            decl += ";\n";

            String realized_decl(decl.replaced("%val%", locarg)
                                 .replaced("%input%", input_arg)
                                 .replaced("%type%", finfo->arg_types[i].type->cs_name()));
            ctx.out.append_indented(realized_decl);
            ctx.out.append(";\n");
            mapped_args[finfo->arg_values[i]] = locarg;
            continue;

        }
        // WRAP_TO_CPP_IN_ARG
        if (mapping.empty()) {
            continue;
        }
        String locarg("temp_" + input_arg);
        // FIXME: brittle way of detecting non-vararg parts
        // Multiline mappings are not recording their temp names
        if(!multiline_mapping) {
            mapped_args[finfo->arg_values[i]] = locarg;
        }

        // multiline mappings will not use `input_wrap`
        if (!multiline_mapping && !input_wrap.empty()) {
            input_arg = input_wrap.replaced("%input%", input_arg);
        }
        String realized_mapping(mapping.replaced("%val%", locarg)
                                .replaced("%input%", input_arg)
                                .replaced("%type%", finfo->arg_types[i].type->cs_name()));
        if(!multiline_mapping) {
            ctx.out.append_indented(realized_mapping);
            ctx.out.append(";\n");
        }
        else {
            // this might be vararg thing
            realized_mapping.replace("%additional_argc%",String().sprintf("%d",additional_argc));
            realized_mapping.replace("%process_varargs%","/*No additional args*/");

            ctx.out.append_indented_multiline(realized_mapping);
        }
    }
}
void mapSignalArguments(const TS_Signal *finfo,GeneratorContext &ctx) {
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    ctx.out.append("(");
    int argc = finfo->arg_types.size();

    for(int i=0; i<argc; ++i) {
        auto arg_type=finfo->arg_types[i].type;
        auto mapping = arg_type->relative_path(TargetCode::CS_INTERFACE,finfo->enclosing_type);
        if(mapping.empty())
            mapping = arg_type->cs_name();
        if(!mapping.empty()) {
            bool nullable_val=false;
            if(finfo->nullable_ref[i]) {
                if(arg_type->kind()==TS_TypeLike::CLASS) {
                    nullable_val = ((const TS_Type *)arg_type)->m_value_type;
                }
            }

            if(nullable_val)
                ctx.out.append("Nullable<");
            ctx.out.append(mapping);
            if(nullable_val)
                ctx.out.append(">");
        }
        else {
            ctx.out.append("MissingWrap<");
            ctx.out.append(arg_type->c_name());
            ctx.out.append(">");
        }
        ctx.out.append(" ");
        ctx.out.append(finfo->arg_values[i]);
        if(!finfo->arg_defaults.empty()) {
            auto defval=finfo->arg_defaults.find(i);
            if(defval!=finfo->arg_defaults.end())
            {
                auto default_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_CS_DEFAULT_WRAPPER, finfo->arg_types[i]);
                ctx.out.append(" = ");
                if(!finfo->nullable_ref[i]) {
                    if(default_mapping.empty())
                        ctx.out.append(String(String::CtorSprintf(),defval->second.c_str(),mapping.c_str()));
                    else {
                        ctx.out.append(default_mapping.replaced("%type%",mapping).replaced("%value%",defval->second));
                    }
                }
                else
                    ctx.out.append("null");
            }
        }
        if(i<argc-1)
            ctx.out.append(", ");
    }
    ctx.out.append(") ");
}
void visitSignal(const TS_Signal &finfo,GeneratorContext &ctx,const String &nativecalls_ns) {
    _generate_docs_for(&finfo,ctx);
    String delegate = finfo.cs_name+"Handler";
    ctx.out.append_indented("public delegate void ");
    ctx.out.append(delegate);
    mapSignalArguments(&finfo,ctx);
    ctx.out.append(";\n");

    // Cached signal name (StringName)
    ctx.out.append_indented("[DebuggerBrowsable(DebuggerBrowsableState.Never)]\n");
    ctx.out.append_indented("private static StringName __signal_name_");
    ctx.out.append(finfo.c_name());
    ctx.out.append(" = \"");
    ctx.out.append(finfo.c_name());
    ctx.out.append("\";\n");
    // Generate event
    ctx.append_line("[Signal]");
    ctx.out.append_indented("public ");

    if (!finfo.enclosing_type->needs_instance()) {
        ctx.out.append("static ");
    }
    ctx.out.append("event ");
    ctx.out.append(delegate);
    ctx.out.append(" ");
    ctx.out.append(finfo.cs_name);
    ctx.out.append("\n");
    ctx.start_block();
    if (!finfo.enclosing_type->needs_instance()) {
        ctx.out.append_indented("add => Singleton.Connect(__signal_name_");
    } else {
        ctx.out.append_indented("add => Connect(__signal_name_");
    }
    ctx.out.append(finfo.c_name());
    ctx.out.append(", new Callable(value));\n");
    if (!finfo.enclosing_type->needs_instance()) {
        ctx.out.append_indented("remove => Singleton.Disconnect(__signal_name_");
    } else {
        ctx.out.append_indented("remove => Disconnect(__signal_name_");
    }
    ctx.out.append(finfo.c_name());
    ctx.out.append(", new Callable(value));\n");
    ctx.end_block();
}

void visitFunction(const TS_Function &finfo,GeneratorContext &ctx,const String &nativecalls_ns) {
    const bool non_void_return=finfo.return_type.type->c_name()!=StringView("void");
//    auto &m_cpp_icalls(m_namespace_files.back());

//    //TODO: handle virtual methods better than using call-by-name from c#?
//    if (finfo->source_type->is_virtual)
//        return;



    if (finfo.m_imported) // skip functions marked as imported
        return;
    if(finfo.c_name()=="to_string" && finfo.enclosing_type && finfo.enclosing_type->c_name()=="Object") {
        return;
    }
    if(finfo.c_name()=="_to_string" && finfo.enclosing_type && finfo.enclosing_type->c_name()=="Object") {
        return;
    }

    if(finfo.source_type->implements_property) // property icalls are made inside property implementations.
        return;

    _generate_docs_for(&finfo,ctx);

    ctx.out.append_indented("[GodotMethod(\"");
    ctx.out.append(finfo.c_name());
    ctx.out.append("\")]\n");
    ctx.out.append_indented(finfo.source_type->is_internal ? "internal " : "public ");
    if (finfo.enclosing_type->kind() == TS_TypeLike::CLASS) {
        const TS_Type* as_class = (const TS_Type*)finfo.enclosing_type;
        if (as_class->source_type->is_singleton) {
            ctx.out.append("static ");
        }
        else if (finfo.source_type->is_virtual) {
            ctx.out.append("virtual ");
        }
    }
    String full_return_type=finfo.return_type.type->relative_path(TargetCode::CS_INTERFACE,finfo.enclosing_type);
    if(full_return_type.empty())
        full_return_type = finfo.return_type.type->cs_name();

    ctx.out.append(full_return_type);
    ctx.out.append(" ");
    ctx.out.append(c_func_name_to_cs(finfo.cs_name));
    // Perform argument type mappings.

    mapFunctionArguments(&finfo,ctx);
    ctx.out.append("\n");

    ctx.start_block();
    //TODO: fixme 'free' calls on Object class...
    if(finfo.c_name()=="free" && finfo.enclosing_type && finfo.enclosing_type->c_name()=="Object") {
        ctx.out.append_indented("Call(\"free\");\n");
        ctx.end_block();
        return;
    }
    if(finfo.source_type->is_virtual) {

        if(non_void_return) {
            ctx.out.append_indented("return default(");
            ctx.out.append(full_return_type);
            ctx.out.append(");\n");
        }
        else {
            ctx.out.append_indented("return;\n");
        }
        ctx.end_block();
        return;
    }
    //    verifyMethodSelfPtr(finfo,non_void_return);

    String instance=finfo.enclosing_type ? "Object.GetPtr(this)" : "";

    // Perform mono to icall type conversions.
    eastl::vector_map<String,String> arg_locals;

    prepareArgumentLocals(&finfo,arg_locals,ctx);

    TS_TypeMapper &mapper(TS_TypeMapper::get());
    // if output mapping present the return pattern is:
    // %func_call%(%args, out %type argRet);\n
    // return argRet;
    auto out_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_ARGOUT, finfo.return_type);
    bool has_out_mapping = !out_mapping.empty();
    // if return mapping present the return pattern is:
    // pattern.replaced("val",%func_call%(%args)) + ";\n"
    auto ret_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_OUT, finfo.return_type);
    bool has_ret_mapping = !ret_mapping.empty();

    //    if(!instance.empty()) {
    //        m_cpp_icalls.out.append(instance);
    //        m_cpp_icalls.out.append("->");
    //    }
    //    m_cpp_icalls.out.append(replaceFunctionName(finfo,m_namespace_stack.back()->c_name()=="Godot"));
    ctx.out.append_indented("");
    String call_str = nativecalls_ns + "." + c_func_name_to_icall(&finfo);
    if(non_void_return && !(has_out_mapping || has_ret_mapping) ) {
        ctx.out.append("return ");
    }
    StringBuilder call_args;
    buildCallArgumentList(&finfo,arg_locals, call_args);
    call_str.append(call_args.as_string());

    if(has_out_mapping) {
        ctx.out.append(call_str);
        ctx.out.append(";\n");
        ctx.out.append_indented("return (");
        ctx.out.append(full_return_type);
        ctx.out.append(")argRet; \n");
    }
    else if(has_ret_mapping)
    {
        ctx.out.append(ret_mapping.replaced("%val%",call_str).replaced("%rettype%",full_return_type));
        ctx.out.append(";\n");
    }
    else
    {
        ctx.out.append(call_str);
        ctx.out.append(";\n");
    }
    ctx.end_block();
}

static constexpr const char *singleton_accessor=
R"raw(private static Godot.Object singleton;

public static Godot.Object Singleton
{
    get
    {
        if (singleton == null)
            singleton = Engine.GetNamedSingleton(typeof(%s).Name);
        return singleton;
    }
}

)raw";


} // end of anonymous namespace

String gen_func_args(const TS_Function &finfo,const eastl::vector_map<String, String> &mapped_args) {
    StringBuilder call_args;
    buildCallArgumentList(&finfo,mapped_args, call_args);
    return call_args.as_string();
}

void CsGeneratorVisitor::generateSpecialFunctions(TS_TypeLike *itype, GeneratorContext &ctx) {
//    String nativecalls_ns = m_namespace_stack.back()->relative_path(TargetCode::CS_INTERFACE);
//    nativecalls_ns = nativecalls_ns + "."+m_current_module->m_name + "NativeCalls";
    //NOTE: this assumes that correct ICALLs class is accessible in current namespace.
    String nativecalls_ns=m_current_module->m_name + "NativeCalls";
    if(itype->kind() != TS_TypeLike::CLASS)
        return;

    const TS_Type *classtype((const TS_Type *)itype);


    if (classtype->source_type->is_singleton) {
        // Add the type name and the singleton pointer as static fields

        ctx.append_multiline(String().sprintf(singleton_accessor, classtype->cs_name().c_str()));

        ctx.append_line(String().sprintf("private readonly static StringName nativeName = \"%s\";\n", classtype->source_type->name.c_str()));
        ctx.append_line(String().sprintf("internal static IntPtr ptr = %s.%s();\n",
                                         nativecalls_ns.c_str(),c_special_func_name_to_icall(classtype,SpecialFuncType::Singleton).c_str()));
    }
    else if (!classtype->source_type->base_name.empty()) {
        String ctor_method("icall_");
        ctor_method += String(itype->c_name()) + "_Ctor"; // Used only for derived types
        // Add member fields
        ctx.out.append_indented(String().sprintf("private readonly static StringName nativeName = \"%s\";\n\n", classtype->source_type->name.c_str()));
        // Add default constructor
        if (classtype->source_type->is_instantiable) {
            ctx.out.append_indented(String().sprintf("public %s() : this(%s)\n",itype->cs_name().c_str(), classtype->source_type->memory_own ? "true" : "false"));
            // The default constructor may also be called by the engine when instancing existing native objects
            // The engine will initialize the pointer field of the managed side before calling the constructor
            // This is why we only allocate a new native object from the constructor if the pointer field is not set
            ctx.out.append_indented_multiline(String().sprintf(R"raw({
if (ptr == IntPtr.Zero)
    ptr = %s.%s(this);
}
)raw", nativecalls_ns.c_str(), ctor_method.c_str()));

        }
        else {
            // Hide the constructor
            ctx.append_line(String().sprintf("internal %s(){}\n",itype->cs_name().c_str()));
        }
        // Add.. em.. trick constructor. Sort of.
        ctx.append_line(String().sprintf("public %s(bool memoryOwn) : base(memoryOwn){}\n", itype->cs_name().c_str()));
    }
}
void CsGeneratorVisitor::visitNSInternal(TS_Namespace *tp) {
    m_path_components.emplace_back(tp->cs_name());
    m_gen_files.emplace_back(m_ctx.add_source_file(String::joined(m_path_components,"/")+"/"+tp->cs_name()+"_Globals.cs"));
    m_gen_stack.push_back(m_gen_files.back());

    GeneratorContext& ctx(m_ctx.generator());

    // Constants (in partial GD class)
    ctx.out.append("\n#pragma warning disable CS1591 // Disable warning: "
                   "'Missing XML comment for publicly visible type or member'\n");
    ctx.out.append_indented("namespace ");

    ctx.out.append(tp->relative_path(TargetCode::CS_INTERFACE)); // namespace Godot.Foo.Bar
    ctx.out.append("\n");
    ctx.start_block();
    // constants

    ctx.out.append_indented("public static partial class Constants\n");
    ctx.start_block();

    //ns.m_globals.m_class_constants
    generate_cs_type_constants(tp,m_ctx);

    // in namespace we have a synthetic Constants class, so we close it here
    ctx.end_block("end of Constants class");

    // Enums
    generate_cs_type_enums(tp,m_ctx);
    ctx.end_block("end of namespace");
    ctx.out.append("\n#pragma warning restore CS1591\n");

    for(const auto &chld : tp->m_children) {
        visitType(chld);
    }
    m_gen_stack.pop_back();
    m_ctx.set_generator(!m_gen_stack.empty() ? m_gen_stack.back() : nullptr);
    m_path_components.pop_back();
}
void CsGeneratorVisitor::visitClassInternal(TS_Type *tp) {

    if(tp->source_type->is_opaque_type)
        return;
    if (tp->m_imported)
        return;

    bool top_level_class=tp->parent && tp->parent->kind()==TS_TypeLike::NAMESPACE;
    String nativecalls_ns=m_current_module->m_name + "NativeCalls";

    if(top_level_class) {

        m_gen_files.emplace_back(m_ctx.add_source_file(String::joined(m_path_components,"/")+"/"+tp->cs_name()+".cs"));
        m_gen_stack.push_back(m_gen_files.back());

    }

    GeneratorContext& ctx(m_ctx.generator());
    // Constants (in partial GD class)
    if(top_level_class) {
        ctx.append_line("using System;");
        ctx.append_line("using System.Diagnostics;\n");
        ctx.append_line("#pragma warning disable CS1591 // Disable warning: 'Missing XML comment for publicly visible type or member'");
        ctx.append_line("#pragma warning disable CS1573 // Disable warning: 'Parameter has no matching param tag in the XML comment'\n");
    }
    String namespace_path = tp->parent->relative_path(TargetCode::CS_INTERFACE,nullptr);
    String icall_ns = m_current_module->m_name+"NativeCalls";
    ctx.start_cs_namespace(namespace_path);

    _generate_docs_for(tp,ctx);
    ctx.out.append_indented("public ");
    if(tp->source_type->is_instantiable)
        ctx.out.append("partial class ");
    else
        ctx.out.append("abstract partial class ");

    ctx.out.append(tp->cs_name());
    if(tp->base_type) {
        ctx.out.append(" : ");
        ctx.out.append(tp->base_type->cs_name());
    }
    ctx.out.append("\n");

    ctx.start_block();

    generate_cs_type_enums(tp,m_ctx);

    // sub types.
    for(const auto & chld : tp->m_children) {
        visitType(chld);
    }
    // properties
    for(const TS_Property *method : tp->m_properties) {
        process_property(icall_ns,*method,ctx);
    }
    // constructors
    if(!tp->m_skip_special_functions) {
        generateSpecialFunctions(tp,ctx);
    }
    // constants
    generate_cs_type_constants(tp,m_ctx);

    // methods
    for(const TS_Function *method : tp->m_functions) {
        visitFunction(*method,ctx,nativecalls_ns);
    }

    // signals
    for(const TS_Signal *method : tp->m_signals) {
        visitSignal(*method,ctx,nativecalls_ns);
    }

    ctx.end_block("end of type");
    ctx.end_block("end of namespace");
    if(top_level_class) {
        ctx.append_line("#pragma warning restore CS1591");
        ctx.append_line("#pragma warning restore CS1573");
    }
    if(top_level_class) {
        m_gen_stack.pop_back();
        m_ctx.set_generator(!m_gen_stack.empty() ? m_gen_stack.back() : nullptr);
    }
}

void CsGeneratorVisitor::visitModule(TS_Module *mod)
{
    if(mod->m_imported)
        return;
    m_current_module = mod;
    m_path_components.emplace_back(mod->m_name);
    m_path_components.emplace_back("cs");
    for(const auto &v : mod->m_namespaces) {
        visitNamespace(v.second);
    }
    m_path_components.pop_back();
    m_path_components.pop_back();

    m_current_module = nullptr;

}
void CsGeneratorVisitor::visitType(TS_TypeLike *tp) {
    switch(tp->kind()) {
        case TS_TypeLike::NAMESPACE:
            visitNSInternal(static_cast<TS_Namespace *>(tp));
            break;
        case TS_TypeLike::CLASS:
            visitClassInternal(static_cast<TS_Type *>(tp));
            break;
        case TS_TypeLike::ENUM:
            // handled by generate_cs_type_enums
            ;
    }
}

void CsGeneratorVisitor::visitNamespace(TS_Namespace *iface) {
    if(iface->m_imported)
        return; // We don't want to generate anything from imported data.

    m_namespace_stack.push_back(iface);
    visitType(iface);
    leaveNamespace();
}
void CsGeneratorVisitor::finalize() {

    for (GeneratorContext *gen : m_gen_files) {
        auto aa = gen->tgt_file_path;
        QString bb = m_ctx.m_target_dir.relativeFilePath(aa.c_str());
        m_ctx.write_string_builder(gen->tgt_file_path, gen->out);
    }
}
