#pragma once
struct TS_Function;
struct ProjectContext;
struct GeneratorContext;
void process_call(const TS_Function &finfo,ProjectContext &ctx);
void gen_icall_impl(const TS_Function &finfo,GeneratorContext &ctx);
void gen_icall(const TS_Function &finfo,GeneratorContext &ctx);
void gen_cs_icall(GeneratorContext &ctx,const TS_Function &finfo);
// used by property setters/getters
void gen_cs_body_impl(const TS_Function &finfo,GeneratorContext &ctx);
