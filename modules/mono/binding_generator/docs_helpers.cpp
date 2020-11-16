#include "docs_helpers.h"

#include "generator_helpers.h"
#include "type_system.h"
#include "type_mapper.h"

#include "core/string_formatter.h"
#include "core/string_utils.h"
#include "core/string_utils.inl"
#include "modules/mono/godotsharp_defs.h"

#include <QXmlStreamWriter>

StringView StringUtils::substr(StringView s, int p_from, size_t p_chars) {
    StringView res(s);
    if (s.empty())
        return res;
    ssize_t count = static_cast<ssize_t>(p_chars);
    if ((p_from + count) > ssize_t(s.length())) {

        p_chars = s.length() - p_from;
    }

    return res.substr(p_from, p_chars);
}
StringView StringUtils::strip_edges(StringView str, bool left, bool right) {

    int len = str.length();
    int beg = 0, end = len;

    if (left) {
        for (int i = 0; i < len; i++) {

            if (str[i] <= 32)
                beg++;
            else
                break;
        }
    }

    if (right) {
        for (int i = (int)(len - 1); i >= 0; i--) {

            if (str[i] <= 32)
                end--;
            else
                break;
        }
    }

    if (beg == 0 && end == len)
        return str;

    return substr(str, beg, end - beg);
}
String StringUtils::dedent(StringView str) {

    String new_string;
    String indent;
    bool has_indent = false;
    bool has_text = false;
    size_t line_start = 0;
    int indent_stop = -1;

    for (size_t i = 0; i < str.length(); i++) {

        char c = str[i];
        if (c == '\n') {
            if (has_text)
                new_string += substr(str, indent_stop, i - indent_stop);
            new_string += '\n';
            has_text = false;
            line_start = i + 1;
            indent_stop = -1;
        }
        else if (!has_text) {
            if (c > 32) {
                has_text = true;
                if (!has_indent) {
                    has_indent = true;
                    indent = substr(str, line_start, i - line_start);
                    indent_stop = i;
                }
            }
            if (has_indent && indent_stop < 0) {
                int j = i - line_start;
                if (j >= indent.length() || c != indent[j])
                    indent_stop = i;
            }
        }
    }

    if (has_text)
        new_string += substr(str, indent_stop);

    return new_string;
}
static void _add_doc_lines(GeneratorContext& ctx, StringView xml_summary) {
    Vector<StringView> summary_lines;
    String::split_ref(summary_lines, xml_summary, '\n');
    if (summary_lines.empty())
        return;

    ctx.out.append_indented("/// <summary>\n");

    for (StringView summary_line : summary_lines) {
        ctx.out.append_indented("/// ");
        ctx.out.append(summary_line);
        ctx.out.append("\n");
    }

    ctx.out.append_indented("/// </summary>\n");
}

static String fix_doc_description(StringView p_bbcode) {

    // This seems to be the correct way to do this. It's the same EditorHelp does.

    return String(StringUtils::strip_edges(StringUtils::dedent(p_bbcode).replaced("\t", "")
        .replaced("\r", "")));
}
static const TS_TypeLike* referenced_type(const TS_TypeLike* from,StringView name) {
    const TS_TypeLike* iter=from->find_typelike_by_cpp_name(name);
    if(!iter)
        iter=from->find_typelike_by_cpp_name("_"+name);
    if(!iter) {
        if(name=="@GlobalScope") {
            iter=from;
            while(iter) {
                if(!iter->parent)
                    return iter;
                iter=iter->parent;
            }
        }
    }
    return iter;
}

static String bbcode_to_xml(StringView p_bbcode, const TS_TypeLike* p_itype, bool verbose) {

    //const TS_Namespace *our_ns = nullptr;

    // Get namespace from path
    // Based on the version in EditorHelp
    using namespace eastl;
    TS_TypeResolver &resolver(TS_TypeResolver::get());
    QByteArray target;
    QXmlStreamWriter xml_output(&target);
    xml_output.setAutoFormatting(true);

    if (p_bbcode.empty())
        return String();

    String bbcode(p_bbcode);
    xml_output.writeStartElement("para");

    Vector<String> tag_stack;
    bool code_tag = false;

    size_t pos = 0;
    while (pos < bbcode.length()) {
        auto brk_pos = bbcode.find('[', pos);

        if (brk_pos == String::npos)
            brk_pos = bbcode.length();

        if (brk_pos > pos) {
            StringView text = StringView(bbcode).substr(pos, brk_pos - pos);
            if (code_tag || !tag_stack.empty()) {
                xml_output.writeCharacters(QString::fromUtf8(text.data(), text.size()).replace("&quot;","\""));
            }
            else {
                Vector<StringView> lines;
                String::split_ref(lines, text, '\n',true);
                for (size_t i = 0; i < lines.size(); i++) {
                    if (i != 0)
                        xml_output.writeStartElement("para");
                    xml_output.writeCharacters(QString::fromUtf8(lines[i].data(), lines[i].size()).replace("&quot;","\""));

                    if (i != lines.size() - 1)
                        xml_output.writeEndElement();
                }
            }
        }

        if (brk_pos == bbcode.length())
            break; // nothing else to add

        size_t brk_end = bbcode.find("]", brk_pos + 1);

        if (brk_end == String::npos) {
            StringView text = StringView(bbcode).substr(brk_pos, bbcode.length() - brk_pos);
            if (code_tag || !tag_stack.empty()) {
                xml_output.writeCharacters(QString::fromUtf8(text.data(), text.size()).replace("&quot;","\""));
            }
            else {

                Vector<StringView> lines;
                String::split_ref(lines, text, '\n');
                for (size_t i = 0; i < lines.size(); i++) {
                    if (i != 0)
                        xml_output.writeStartElement("para");

                    xml_output.writeCharacters(QString::fromUtf8(lines[i].data(), lines[i].size()));

                    if (i != lines.size() - 1) {
                        xml_output.writeEndElement();
                        target.append("\n");
                    }
                }
            }

            break;
        }

        StringView tag = StringView(bbcode).substr(brk_pos + 1, brk_end - brk_pos - 1);

        if (tag.starts_with('/')) {
            bool tag_ok = tag_stack.size() && tag_stack.front() == tag.substr(1, tag.length());

            if (!tag_ok) {
                xml_output.writeCharacters("[");
                pos = brk_pos + 1;
                continue;
            }

            tag_stack.pop_front();
            pos = brk_end + 1;
            code_tag = false;

            if (tag == "/url"_sv) {
                xml_output.writeEndElement(); //</a>
            }
            else if (tag == "/code"_sv) {
                xml_output.writeEndElement(); //</c>
            }
            else if (tag == "/codeblock"_sv) {
                xml_output.writeEndElement(); //</code>
            }
        }
        else if (code_tag) {
            xml_output.writeCharacters("[");
            pos = brk_pos + 1;
        }
        else if (tag.starts_with("method ") || tag.starts_with("member ") || tag.starts_with("signal ") || tag.starts_with("enum ") || tag.starts_with("constant ")) {
            StringView link_target = tag.substr(tag.find(" ") + 1, tag.length());
            StringView link_tag = tag.substr(0, tag.find(" "));

            Vector<StringView> link_target_parts;
            String::split_ref(link_target_parts, link_target, '.');

            if (link_target_parts.empty() || link_target_parts.size() > 2) {
                ERR_PRINT("Invalid reference format: '" + tag + "'.");

                xml_output.writeTextElement("c", QString::fromUtf8(tag.data(), tag.size()));

                pos = brk_end + 1;
                continue;
            }

            const TS_TypeLike* target_itype;
            StringName target_cname;

            if (link_target_parts.size() == 2) {
                if(link_target_parts[0]=="@GlobalScope")
                    link_target_parts[0] = "Godot";
                target_itype = p_itype->find_typelike_by_cpp_name(String(link_target_parts.front()));
                if (!target_itype) {
                    target_itype = p_itype->find_typelike_by_cpp_name("_" + String(link_target_parts.front()));
                }
                target_cname = StringName(link_target_parts[1]);
            }
            else {
                target_itype = p_itype;
                target_cname = StringName(link_target_parts[0]);
            }
            if (link_tag == "method"_sv) {
                if(!target_itype)
                    target_itype = p_itype;
                const TS_Function* target_imethod = target_itype->find_method_by_name(CS_INTERFACE, TS_Function::mapMethodName(target_cname, target_itype->cs_name()), true);
                if (target_imethod) {
                    xml_output.writeEmptyElement("see");
                    String full_path;
                    if(target_imethod->source_type->implements_property) {
                        const TS_Type *enc=(const TS_Type *)target_imethod->enclosing_type;
                        full_path = enc->get_property_path_by_func(target_imethod);
                    }
                    else {
                        full_path = target_imethod->cs_name;
                    }
                    xml_output.writeAttribute("cref", QString::fromUtf8((target_imethod->enclosing_type->relative_path(CS_INTERFACE) + "." +full_path).c_str()));
                }
            }
            else if (link_tag == "member"_sv) {
                if (!target_itype) { // || !target_itype->source_type->is_object_type
                    if (verbose) {
                        qDebug("Cannot resolve type from member reference in documentation: %.*s", (int)link_target.size(), link_target.data());
                    }

                    // TODO Map what we can
                    xml_output.writeTextElement("c", QString::fromUtf8(link_target.data(), link_target.size()));
                }
                else {
                    // member reference could have been made in a constant belonging to enum belonging to class, so we need to find first enclosing class.
                    const TS_TypeLike *actual_type = target_itype;
                    while(actual_type->kind()!= TS_TypeLike::CLASS)
                        actual_type = target_itype->parent;
                    assert(actual_type);
                    const TS_Property* target_iprop = ((const TS_Type*)actual_type)->find_property_by_name(target_cname);
                    if(!target_iprop) {
                        ERR_PRINTF("Missing CSProperty for:%s",target_cname.asCString());
                    }
                    if (target_iprop) {
                        xml_output.writeEmptyElement("see");
                        String full_path = actual_type->relative_path(CS_INTERFACE) + "." + target_iprop->cs_name;
                        xml_output.writeAttribute("cref", full_path.c_str());
                    }
                }
            }
            else if (link_tag == "signal"_sv) {
                // We do not declare signals in any way in C#, so there is nothing to reference
                xml_output.writeTextElement("c", QString::fromUtf8(link_target.data(), link_target.size()));
            }
            else if (link_tag == "enum"_sv) {
                String search_cname = target_cname.asCString();
                if(target_itype==nullptr)
                    target_itype = p_itype;
                const TS_Enum *enum_match;
                if(target_itype->kind()==TS_TypeLike::ENUM)
                    enum_match = (const TS_Enum *)target_itype;
                else
                    enum_match = target_itype->find_enum_by_cpp_name(search_cname);

                if (!enum_match) // try the fixed name -> "Enum"
                    enum_match = target_itype->find_enum_by_cpp_name(search_cname + "Enum");
                if(!enum_match)
                    if (search_cname == "Operator") //HACK: to handle Variant fiasco
                        enum_match = target_itype->find_enum_by_cpp_name("Variant::Operator");

                if (enum_match) {
                    xml_output.writeEmptyElement("see");
                    String full_path = enum_match->relative_path(CS_INTERFACE, nullptr);
                    xml_output.writeAttribute("cref", full_path.c_str()); // Includes nesting class if any
                }
                else {
                    ERR_PRINT("Cannot resolve enum reference in documentation: '" + link_target + "'.");

                    xml_output.writeTextElement("c", QByteArray::fromRawData(link_target.data(), link_target.size()));
                }
            }
            else if (link_tag == "constant"_sv) {
                const TS_TypeLike *search_in = !target_itype ? p_itype : target_itype;
                if (search_in) {
                    // Try to find as a global constant
                    const TS_Constant * c = p_itype->find_constant_by_cpp_name(target_cname);
                    if(c) {
                        xml_output.writeEmptyElement("see");
                        String full_path = c->relative_path(CS_INTERFACE, nullptr);
                        xml_output.writeAttribute("cref", full_path.c_str()); // Includes nesting class if any
                    }
                    else {
                        qDebug("Cannot resolve type from constant reference in documentation: %.*s", (int)link_target.size(), link_target.data());
                    }
                }
            }
            pos = brk_end + 1;
        }
        else if (tag == "b"_sv) {
            // bold is not supported in xml comments
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "i"_sv) {
            // italics is not supported in xml comments
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "code"_sv) {
            xml_output.writeStartElement("c");

            code_tag = true;
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "codeblock"_sv) {
            xml_output.writeStartElement("code");

            code_tag = true;
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "center"_sv) {
            // center is alignment not supported in xml comments
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "br"_sv) {
            assert(false);
            //xml_output.append("\n"); // FIXME: Should use <para> instead. Luckily this tag isn't used for now.
            pos = brk_end + 1;
        }
        else if (tag == "u"_sv) {
            // underline is not supported in xml comments
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "s"_sv) {
            // strikethrough is not supported in xml comments
            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag == "url"_sv) {
            size_t end = bbcode.find("[", brk_end);
            if (end == String::npos)
                end = bbcode.length();
            StringView url = StringView(bbcode).substr(brk_end + 1, end - brk_end - 1);
            QString qurl = QString::fromUtf8(url.data(), url.size());
            xml_output.writeEmptyElement("a");
            xml_output.writeAttribute("href", qurl);
            xml_output.writeCharacters(qurl);

            pos = brk_end + 1;
            tag_stack.push_front(String(tag));
        }
        else if (tag.starts_with("url=")) {
            StringView url = tag.substr(4, tag.length());
            QString qurl = QString::fromUtf8(url.data(), url.size());
            xml_output.writeStartElement("a");
            xml_output.writeAttribute("href", qurl);

            pos = brk_end + 1;
            tag_stack.push_front("url");
        }
        else if (tag == "img"_sv) {
            auto end = bbcode.find("[", brk_end);
            if (end == String::npos)
                end = bbcode.length();
            StringView image(StringView(bbcode).substr(brk_end + 1, end - brk_end - 1));

            // Not supported. Just append the bbcode.
            xml_output.writeCharacters("[img]" + QString::fromUtf8(image.data(), image.size()) + "[/img]");

            pos = end;
            tag_stack.push_front(String(tag));
        }
        else if (tag.starts_with("color=")) {
            // Not supported.
            pos = brk_end + 1;
            tag_stack.push_front("color");
        }
        else if (tag.starts_with("font=")) {
            // Not supported.
            pos = brk_end + 1;
            tag_stack.push_front("font");
        }
        else if (auto reslv = referenced_type(p_itype,tag)) {
            QString qtag = QString::fromUtf8(tag.data(), tag.size());
            if (tag == "Array"_sv || tag == "Dictionary"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", QString(BINDINGS_NAMESPACE_COLLECTIONS) + "." + qtag);
            }
            else if (tag == "bool"_sv || tag == "int"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", qtag);
            }
            else if (tag == "float"_sv) {
                const char* tname = "float";
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", tname);
            }
            else if (tag == "Variant"_sv) {
                // We use System.Object for Variant, so there is no Variant type in C#
                xml_output.writeTextElement("c", "Variant");
            }
            else if (tag == "String"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "string");
            }
            else if (tag == "Nil"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("langword", "null");
            }
            else if (tag.starts_with('@')) {
                // @GlobalScope, @GDScript, etc
                xml_output.writeTextElement("c", qtag);
            }
            else if (tag == "PoolByteArray"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "byte");
            }
            else if (tag == "PoolIntArray"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "int");
            }
            else if (tag == "PoolRealArray"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "float");
            }
            else if (tag == "PoolStringArray"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "string");
            }
            else if (tag == "PoolVector2Array"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "" BINDINGS_NAMESPACE ".Vector2");
            }
            else if (tag == "PoolVector3Array"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "" BINDINGS_NAMESPACE ".Vector3");
            }
            else if (tag == "PoolColorArray"_sv) {
                xml_output.writeEmptyElement("see");
                xml_output.writeAttribute("cref", "" BINDINGS_NAMESPACE ".Color");
            }
            else {
                const TS_TypeLike* target_itype = reslv;
                if (!target_itype) {
                    String cs_classname = TS_Type::convert_name(tag, p_itype->relative_path(CS_INTERFACE));
                    auto resv=resolver.resolveType({cs_classname});
                    if (resv.type)
                        target_itype = resv.type;
                }
                if (target_itype) {
                    xml_output.writeEmptyElement("see");
                    xml_output.writeAttribute("cref", target_itype->relative_path(CS_INTERFACE).c_str());
                }
                else {
                    ERR_PRINT("Cannot resolve type reference in documentation: '" + tag + "'.");
                    xml_output.writeTextElement("c", qtag);
                }
            }

            pos = brk_end + 1;

        }
        else {
            xml_output.writeCharacters("["); // ignore
            pos = brk_pos + 1;
        }
    }
    xml_output.writeEndElement();

    return target.trimmed().data();
}



void _generate_docs_for(const TS_TypeLike* itype, GeneratorContext& ctx)
{
    if (!itype->m_docs || itype->m_docs->description.empty())
        return;

    String xml_summary = bbcode_to_xml(fix_doc_description(itype->m_docs->description), itype, true);

    _add_doc_lines(ctx, xml_summary);
}
void _generate_docs_for(const TS_Property* property, GeneratorContext& ctx)
{
    _generate_docs_for(property,0,ctx);
}
void _generate_docs_for(const TS_Constant* iconstant, GeneratorContext& ctx) {
    if (!iconstant->m_resolved_doc || iconstant->m_resolved_doc->description.empty())
        return;

    String xml_summary = bbcode_to_xml(fix_doc_description(iconstant->m_resolved_doc->description), iconstant->enclosing_type, true);

    _add_doc_lines(ctx, xml_summary);
}
void _generate_docs_for(const TS_Function* func, GeneratorContext& ctx) {
    if (!func->m_resolved_doc || func->m_resolved_doc->description.empty())
        return;

    String xml_summary = bbcode_to_xml(fix_doc_description(func->m_resolved_doc->description), func->enclosing_type, true);
    _add_doc_lines(ctx, xml_summary);
}
void _generate_docs_for(const TS_Signal* func, GeneratorContext& ctx) {
    if (!func->m_resolved_doc || func->m_resolved_doc->description.empty())
        return;

    String xml_summary = bbcode_to_xml(fix_doc_description(func->m_resolved_doc->description), func->enclosing_type, true);
    _add_doc_lines(ctx, xml_summary);
}


void _generate_docs_for(const TS_Property *property, int subfield, GeneratorContext &ctx)
{
    if(size_t(subfield)>=property->indexed_entries.size())
        return;
    const auto &entry(property->indexed_entries[subfield]);
    if (!entry.m_docs || entry.m_docs->description.empty())
        return;

    String xml_summary = bbcode_to_xml(fix_doc_description(entry.m_docs->description), property->m_owner, true);

    _add_doc_lines(ctx, xml_summary);
}
#include <list>
