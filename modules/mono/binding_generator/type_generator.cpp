#include "type_generator.h"

#include "generator_helpers.h"
#include "docs_helpers.h"
#include "type_system.h"

static int number_complexity(const char *f) {
    uint8_t counts[16] = {0,0,0,0,0,0,0, 0, 0,0,0,0, 0,0,0,0};
    uint8_t other=0;
    uint8_t count=0;
    while(*f) {
        if(isdigit(*f)) {
            counts[*f -'0']++;
        }
        else if(eastl::CharToLower(*f)>='a' && eastl::CharToLower(*f) <= 'f') {
            counts[10+ eastl::CharToLower(*f) - 'a']++;
        }
        else
            other++;
        ++f;
        count++;
    }
    int digit_count = 0;
    int highest_count=0;
    for(uint8_t v : counts) {
        digit_count+=v;
        if(v> highest_count)
            highest_count = v;
    }
    // reduce complexity by removing the highest repeating digit
        digit_count -= highest_count;
    if(other!=0)
        digit_count++;
    return digit_count + count;
}
static void _write_constant(StringBuilder& p_output, const TS_Constant &constant) {
    p_output.append(constant.cs_name);
    p_output.append(" = ");
    bool was_parsed=true;
    int64_t sig_val = QString(constant.value.c_str()).toLongLong(&was_parsed);
    if(!was_parsed) { // non number constants
        p_output.append(constant.value);
        return;
    }
    uint32_t val = sig_val;
    if(val<32) {
        p_output.append(constant.value);
        return;
    }
    char select[3][32];
    snprintf(select[0],32,"%d",val);
    snprintf(select[1], 32, "0x%x", val);
    snprintf(select[2], 32, "~0x%x", ~val);
    int complexity[3] = {
        number_complexity(select[0])+1, // so 0x is disregarded during complexity compare
        number_complexity(select[1]),
        number_complexity(select[2]),
    };
    int best=0;
    for(int i=1; i<3; ++i) {
        if(complexity[i]<complexity[best]) {
            best = i;
        }
    }
    p_output.append(String(select[best]));
}

void generate_cs_type_constants(const TS_TypeLike* itype, ProjectContext& prj) {
    GeneratorContext &ctx(prj.generator());
    bool all_imported=true;
    for (const TS_Constant* iconstant : itype->m_constants) {
        if (iconstant->m_imported)
            continue;
        all_imported = false;
        break;
    }
    if(all_imported)
        return;

    ctx.out.append_indented("// ");
    ctx.out.append(itype->cs_name());
    ctx.out.append(" constants\n");
    for (const TS_Constant* iconstant : itype->m_constants) {
        if(iconstant->m_imported)
            continue;
        _generate_docs_for(iconstant, ctx);
        // TODO: use iconstant->const_type below.
        if(iconstant->const_type.cname=="String") {
            ctx.out.append_indented("public const string ");
            ctx.out.append(iconstant->cs_name);
            ctx.out.append(" = \"");
            ctx.out.append(iconstant->value);
            ctx.out.append("\"");
        }
        else {
            ctx.out.append_indented("public const int ");
            _write_constant(ctx.out, *iconstant);
        }
        ctx.out.append(";\n");
    }
    if (!itype->m_constants.empty())
        ctx.out.append("\n");
}
static void generate_enum_entry(const TS_Constant* iconstant, ProjectContext& prj) {
    GeneratorContext &ctx(prj.generator());
    _generate_docs_for(iconstant, ctx);
    ctx.out.append_indented("");
    _write_constant(ctx.out, *iconstant);
}
void generate_cs_type_enums(const TS_TypeLike* itype, ProjectContext& prj) {
    GeneratorContext &ctx(prj.generator());

    itype->visit_kind(TS_TypeLike::ENUM, [&](const TS_TypeLike* entry) {
        if(entry->m_imported)
            return;

        const TS_Enum* ienum = (const TS_Enum*)entry;
        if (ienum->m_constants.empty()) {
            qCritical("Encountered enum '%s' without constants!", ienum->cs_name().c_str());
            return;
        }
        if (!ienum->static_wrapper_class.empty()) {
            ctx.out.append_indented("public static partial class ");
            ctx.out.append(ienum->static_wrapper_class);
            ctx.out.append("\n");
            ctx.start_block();
        }
        ctx.out.append_indented("public enum ");
        ctx.out.append(ienum->cs_name());
        ctx.out.append("\n");
        ctx.start_block();
        for (const TS_Constant* iconstant : ienum->m_constants) {
            generate_enum_entry(iconstant, prj);
            if(ienum->m_constants.back()!=iconstant)
                ctx.out.append(",\n");
            else
                ctx.out.append("\n");
        }
        ctx.end_block();
        if (!ienum->static_wrapper_class.empty()) {
            ctx.end_block();
        }

    });
}

