#include "core/translation.h"

#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/os/os.h"

void TranslationServer::setup() {
    String test = T_GLOBAL_DEF<String>("locale/test", "");
    test = StringUtils::strip_edges(test);
    if (!test.empty()) {
        set_locale(test);
    }
    else {
        set_locale(OS::get_singleton()->get_locale());
    }

    fallback = T_GLOBAL_DEF<String>("locale/fallback", "en");
#ifdef TOOLS_ENABLED
    ProjectSettings::get_singleton()->set_custom_property_info(
            "locale/fallback", PropertyInfo(VariantType::STRING, "locale/fallback", PropertyHint::LocaleID, ""));
#endif
}