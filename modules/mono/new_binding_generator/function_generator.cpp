#include "function_generator.h"

#include "generator_helpers.h"
#include "type_system.h"

#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "core/string_builder.h"
#include "core/deque.h"

#include "core/hash_map.h"
#include "core/hash_set.h"
#include "EASTL/unordered_set.h"
#include "EASTL/vector_set.h"
#include "EASTL/deque.h"
#include "EASTL/algorithm.h"
#include "EASTL/sort.h"

void gen_icall(const TS_Function &finfo,GeneratorContext &ctx) {
    ctx.append_line("[MethodImpl(MethodImplOptions.InternalCall)]");
    ctx.out.append_indented("internal extern static ");
    ctx.out.append(finfo.return_type.type->cs_name());
    ctx.out.append(" ");
    ctx.out.append(c_func_name_to_icall(&finfo));
    ctx.out.append("()");
    ctx.out.append(";\n");
    ctx.out.append("\n");
}
void gen_cs_icall(GeneratorContext &ctx,const TS_Function &finfo) {
    ctx.out.append(c_func_name_to_icall(&finfo));
    if(finfo.arg_types.empty())
        ctx.out.append("();\n");
    else {
        ctx.out.append("(");
        ctx.out.append(String::joined(finfo.arg_values,","));
        ctx.out.append(");\n");
    }
}
void gen_cs_body_impl(const TS_Function &finfo,GeneratorContext &ctx) {
    ctx.start_block();
    ctx.out.append_indented("");
    if(finfo.return_type.type->cs_name() != "void")
        ctx.out.append("return ");
    gen_cs_icall(ctx,finfo);
    ctx.end_block();

}
void gen_cs_impl(const TS_Function &finfo,GeneratorContext &ctx) {
    ctx.out.append(finfo.return_type.type->cs_name());
    ctx.out.append(" ");
    ctx.out.append(c_func_name_to_cs(finfo.cs_name));
    ctx.out.append("()\n");
    gen_cs_body_impl(finfo,ctx);


}
void process_call(const TS_Function &finfo,ProjectContext &ctx) {

    gen_cs_impl(finfo,ctx.impl_cs_ctx());
    gen_icall(finfo,ctx.icall_cs_impl_ctx());
}

