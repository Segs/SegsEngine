#include "translation_helpers.h"

#include "core/string.h"
#include "core/string_utils.h"
#include "core/translation.h"

#ifdef TOOLS_ENABLED
StringName TTR(StringView p_text,StringView p_context) {

    if (TranslationServer::get_singleton()) {
        return TranslationServer::get_singleton()->tool_translate(StringName(p_text),StringName(p_context));
    }

    return StringName(p_text);
}

String TTRS(StringView p_text,StringView p_context) {

    if (TranslationServer::get_singleton()) {
        return String(TranslationServer::get_singleton()->tool_translate(StringName(p_text),StringName(p_context)));
    }

    return String(p_text);
}

/* DTR is used for the documentation, handling descriptions extracted from the XML.
 * It also replaces `$DOCS_URL` with the actual URL to the documentation's branch,
 * to allow dehardcoding it in the XML and doing proper substitutions everywhere.
 */
StringName DTR(StringView p_text) {
    using namespace StringUtils;
    if (TranslationServer::get_singleton()) {
        // Comes straight from the XML, so remove indentation and any trailing whitespace.
        String ded=dedent(p_text);
        StringView text = strip_edges(ded);
        return TranslationServer::get_singleton()->doc_translate(StringName(text));
    }

    return StringName(p_text);
}

#endif

StringName RTR(const char *p_text) {

    if (TranslationServer::get_singleton()) {
        StringName rtr(TranslationServer::get_singleton()->tool_translate(StringName(p_text),StringName()));
        if (rtr.empty() || rtr == p_text) {
            return TranslationServer::get_singleton()->translate(StringName(p_text));
        }
        return rtr;
    }

    return StringName(p_text);
}
String RTR_utf8(StringView sv) {
    return String(RTR(String(sv).c_str())).data();
}
