#pragma once
struct GeneratorContext;
struct TS_TypeLike;
struct TS_Constant;
struct TS_Property;
struct TS_Function;
struct TS_Signal;

void _generate_docs_for(const TS_TypeLike* itype, GeneratorContext &ctx);
void _generate_docs_for(const TS_Property* property, GeneratorContext &ctx);
void _generate_docs_for(const TS_Property* property, int subfield,GeneratorContext &ctx);
void _generate_docs_for(const TS_Constant* iconstant, GeneratorContext& ctx);
void _generate_docs_for(const TS_Function* func, GeneratorContext& ctx);
void _generate_docs_for(const TS_Signal* func, GeneratorContext& ctx);
