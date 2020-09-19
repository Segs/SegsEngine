#pragma once
#include "core/forward_decls.h"
struct TS_Property;
struct GeneratorContext;
struct ProjectContext;
struct PropertyInterface;

void process_array_property(const PropertyInterface &inp, GeneratorContext &cs_ctx);
void process_group_property(const PropertyInterface &aprop, GeneratorContext &cs_ctx);
void process_property(const String &icall_ns,const TS_Property &pinfo, GeneratorContext &ctx);
