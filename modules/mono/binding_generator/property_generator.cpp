#include "property_generator.h"

#include "cs_generator_visitor.h"

#include "generator_helpers.h"
#include "type_system.h"
#include "type_mapper.h"
#include "docs_helpers.h"

#include "core/string_builder.h"

#include "core/reflection_support/reflection_data.h"

#include "EASTL/fixed_hash_set.h"

static String func_return_type(const TS_Function &finfo) {
    String res=finfo.return_type.type->relative_path(TargetCode::CS_INTERFACE,finfo.enclosing_type);
    if(res.empty())
        return finfo.return_type.type->cs_name();
    return res;
}

static String gen_cs_icall(const String &icall_ns,const TS_Function &finfo,const eastl::vector_map<String, String> &mapped_args) {
    String ctx;
    ctx.append(icall_ns);
    ctx.append(".");
    ctx.append(c_func_name_to_icall(&finfo));
    String args=gen_func_args(finfo,mapped_args);
    ctx.append(args);
    return ctx;
}
static String gen_cs_getter(const TS_Property::ResolvedPropertyEntry &rprop,const String &nativecalls_ns,const eastl::vector_map<String, String> &mapped_args) {
    TS_TypeMapper &mapper(TS_TypeMapper::get());
    auto finfo = rprop.getter;
    if(!finfo)
        return "";
    auto out_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_ARGOUT, finfo->return_type);
    bool has_out_mapping = !out_mapping.empty();
    // if return mapping present the return pattern is:
    // pattern.replaced("val",%func_call%(%args)) + ";\n"
    auto ret_mapping = mapper.map_type(TS_TypeMapper::SCRIPT_TO_WRAP_OUT, finfo->return_type);
    bool has_ret_mapping = !ret_mapping.empty();
    String res = "get\n";
    assert(false == (has_ret_mapping&&has_out_mapping));
    if(has_ret_mapping||has_out_mapping) {
        res+="{\n    ";
    }
    else
        res = "get => ";
    String callstr=gen_cs_icall(nativecalls_ns,*rprop.getter,mapped_args);
    if(has_ret_mapping) {
        res += ret_mapping.replaced("%val%",callstr).replaced("%rettype%",func_return_type(*finfo))+";\n";
    }
    else if(has_out_mapping) {
        res += callstr + ";\n    return argRet;\n";
    }
    else
        res += callstr + ";\n";
    if(has_ret_mapping||has_out_mapping) {
        res+="}\n";
    }

    return res;
}
static const TS_TypeLike *common_base_type(const TS_TypeLike *a,const TS_TypeLike *b) {
    if(a==b)
        return a;
    eastl::fixed_hash_set<const TS_TypeLike*,32> b_path;
    const TS_TypeLike* rel_iter=b;
    while(rel_iter) {
        b_path.insert(rel_iter);
        rel_iter = rel_iter->base_type;
    }
    const TS_TypeLike* ns_iter = a;
    while (ns_iter)
    {
        if(b_path.contains(ns_iter))
            return ns_iter;
        ns_iter = ns_iter->base_type;
    }
    return nullptr;
}
static String gen_cs_setter(const TS_Property::ResolvedPropertyEntry &rprop,const String &nativecalls_ns,const eastl::vector_map<String, String> &mapped_args) {

    auto finfo = rprop.getter;
    if(!finfo)
        return "";
    String res = "set => ";
    String callstr=gen_cs_icall(nativecalls_ns,*rprop.setter,mapped_args);
    return res + callstr + ";\n";
}

void process_array_property(const String &icall_ns,const TS_Property &aprop, GeneratorContext &cs_ctx) {

    String base_property_name = aprop.cs_name; //c_property_name_to_cs(aprop.cname)
    String holder_name = base_property_name+"Holder";
    cs_ctx.start_class_block("public",holder_name);
        cs_ctx.start_struct_block("public","Propertifier");
            cs_ctx.append_line("IntPtr owner_ptr; int tgt_idx;");
            cs_ctx.append_line("public Propertifier(IntPtr owner, int idx)");
            cs_ctx.start_block();
                cs_ctx.append_line("owner_ptr = owner;");
                cs_ctx.append_line("tgt_idx = idx;");
            cs_ctx.end_block();
            int idx = 0;
            for(const auto &prop : aprop.indexed_entries) {
                _generate_docs_for(&aprop,idx,cs_ctx);

                cs_ctx.out.append_indented("public ");
                // If prop.entry_type is multi-type, use their common base type, and add a check in cs code ?
                const TS_TypeLike *tp=prop.entry_type.front().type;
                if(prop.entry_type.size()>1) {
                    for(size_t i=1; i<prop.entry_type.size(); ++i) {
                        tp = common_base_type(tp,prop.entry_type[i].type);
                    }
                    assert(tp); // if this fails we have no common base type.
                }
                String full_return_type=tp->relative_path(TargetCode::CS_INTERFACE,aprop.m_owner);
                if(full_return_type.empty())
                    full_return_type = tp->cs_name();

                cs_ctx.out.append(full_return_type);
                cs_ctx.out.append(" ");
                cs_ctx.out.append(c_property_name_to_cs(prop.subfield_name));
                cs_ctx.out.append("\n");
                cs_ctx.start_block();
                    eastl::vector_map<String, String> mapped_args;
                    mapped_args["%self%"] = "owner_ptr";
                    mapped_args[prop.getter->arg_values.front()] = "tgt_idx";
                    cs_ctx.append_multiline(gen_cs_getter(prop,icall_ns,mapped_args));
                    mapped_args[prop.setter->arg_values.front()] = "tgt_idx";
                    mapped_args[prop.setter->arg_values[1]] = "value";
                    cs_ctx.append_multiline(gen_cs_setter(prop,icall_ns,mapped_args));
                cs_ctx.end_block();
            }
        cs_ctx.end_block(); // end of propetrifier code
        // Holder constructor
        cs_ctx.out.append_indented("public ");
        cs_ctx.out.append(holder_name);
        cs_ctx.out.append("(IntPtr owner_ptr) { our_owner=owner_ptr; }\n");
        // Holder [] operator
        cs_ctx.append_line("public Propertifier this[int i] => new Propertifier(our_owner, i);");
        cs_ctx.append_line("private IntPtr our_owner;");
    cs_ctx.end_block();

    String base_cs_property = "public %name%Holder %name% => new %name%Holder(Object.GetPtr(this));";
    base_cs_property.replace("%name%",base_property_name);
    cs_ctx.append_line(base_cs_property);
}
/*
    public struct %prop_name%Structifier
    {
        C our_owner;
        public %prop_name%Structifier(C owner)
        {
            our_owner = owner;
        }
        %foreach field in property%
        public %field.type %field.name
        {
            get => our_owner.%field.getter(our_owner,%field.idx);
            set => our_owner.%field.setter(our_owner,%field.idx,value)
        }
    }
    public %prop_name%Structifier %prop_name% => new %prop_name%Structifier(this);
*/
void process_group_property(const String &icall_ns,const TS_Property &cprop, GeneratorContext &cs_ctx) {

    String base_property_name = cprop.cs_name;
    String holder_name = base_property_name+"Structifier";

//    auto intptr=resolver.resolveType({"IntPtr"});
//    auto inttype=resolver.resolveType({"int"});
    cs_ctx.start_class_block("public",holder_name);
        cs_ctx.append_line("IntPtr owner_ptr;");
        cs_ctx.out.append_indented("public ");
        cs_ctx.out.append(holder_name);
        cs_ctx.out.append("(IntPtr owner)\n");

        cs_ctx.start_block();
            cs_ctx.append_line("owner_ptr = owner;");
        cs_ctx.end_block();
        int idx=0;
        for(const auto &prop : cprop.indexed_entries) {
            const TS_Function *setter_finfo=prop.setter;
            const TS_Function *getter_finfo=prop.getter;
            eastl::vector_map<String, String> replacements;
            replacements["%self%"] = "owner_ptr";
            if(prop.setter) {
                if(prop.index!=-1) {
                    // value passed after index.
                    replacements[setter_finfo->arg_values[0]] = String(String::CtorSprintf(),"%d",prop.index); // index
                    replacements[setter_finfo->arg_values[1]] = "value";
                }
                else {
                    replacements[setter_finfo->arg_values[0]] = "value"; // index
                }
            }
            if(prop.getter) {
                if(prop.index!=-1) {
                    // index is the first arg
                    replacements[getter_finfo->arg_values[0]] = String(String::CtorSprintf(),"%d",prop.index); // index
                }
            }
            const TS_TypeLike *tp=prop.entry_type.front().type;
            if(prop.entry_type.size()>1) {
                for(size_t i=1; i<prop.entry_type.size(); ++i) {
                    tp = common_base_type(tp,prop.entry_type[i].type);
                }
                assert(tp); // if this fails we have no common base type.
            }
            String full_return_type=tp->relative_path(TargetCode::CS_INTERFACE,cprop.m_owner);
            if(full_return_type.empty())
                full_return_type = tp->cs_name();

            _generate_docs_for(&cprop,idx,cs_ctx);

            cs_ctx.out.append_indented("public ");
            cs_ctx.out.append(full_return_type);
            cs_ctx.out.append(" ");
            cs_ctx.out.append(c_property_name_to_cs(prop.subfield_name));
            cs_ctx.out.append("\n");
            cs_ctx.start_block();
            //%field.getter(our_owner,%field.idx);
            String get = gen_cs_getter(prop,icall_ns,replacements);
            String set = gen_cs_setter(prop,icall_ns,replacements);
            cs_ctx.append_multiline(get);
            cs_ctx.append_multiline(set);

            cs_ctx.end_block();
            ++idx;
        }
    cs_ctx.end_block();

    const TS_Type *enc=cprop.m_owner;
    bool is_in_singleton = enc->source_type->is_singleton;

    String base_cs_property = "public "+String(is_in_singleton ? "static " : "") +
                              "%name%Structifier %name% => new %name%Structifier("
                              + String(is_in_singleton ? "ptr":"Object.GetPtr(this)") +
                              ");";
    base_cs_property.replace("%name%",base_property_name);
    cs_ctx.append_line(base_cs_property);
}

void gen_property_cs_impl(const String &icall_ns,const TS_Property &pinfo,GeneratorContext &cs_ctx) {
    _generate_docs_for(&pinfo,cs_ctx);

    String decl;
    if(pinfo.m_owner->source_type->is_singleton) {
        decl="static ";
    }
    decl += "public %type %name ";
    auto first=pinfo.indexed_entries.front();
    String ret_type(func_return_type(*first.getter));
    decl.replace("%type",ret_type);
    decl.replace("%name",c_property_name_to_cs(pinfo.cs_name));
    cs_ctx.append_line(decl);

    cs_ctx.start_block();
        if(first.getter) {
            eastl::vector_map<String, String> replacements;
            cs_ctx.append_multiline(gen_cs_getter(first,icall_ns,replacements));
        }
        if(first.setter) {
            eastl::vector_map<String, String> replacements;
            replacements[first.setter->arg_values.front()] = "value";
            cs_ctx.append_multiline(gen_cs_setter(first,icall_ns,replacements));
        }

    cs_ctx.end_block();
}

void process_property(const String &icall_ns, const TS_Property &pinfo, GeneratorContext &ctx) {

    if(pinfo.source_type->max_property_index==-1) {
        gen_property_cs_impl(icall_ns,pinfo,ctx);
    }
    else if (pinfo.source_type->max_property_index==-2) {
        process_group_property(icall_ns,pinfo,ctx);
    }
    else if(pinfo.source_type->max_property_index>0){
        process_array_property(icall_ns,pinfo,ctx);
    }
    else {
        assert(false);
    }

}

String get_property_typename(const TS_Property &pinfo)
{
    if(pinfo.source_type->max_property_index==-1) {
        return pinfo.cs_name;
    }
    else if (pinfo.source_type->max_property_index==-2) {
        return pinfo.cs_name+"Structifier";
    }
    else if(pinfo.source_type->max_property_index>0){
        return pinfo.cs_name+"Holder.Propertifier";
    }
    return pinfo.cs_name;
}
