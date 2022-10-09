#include "translation.h"

#include "core/project_settings.h"
#include "core/string_utils.h"
#include "core/os/os.h"


void TranslationServer::setup() {

    String test = GLOBAL_DEF("locale/test", "").as<String>();
    test = StringUtils::strip_edges( test);
    if (!test.empty())
        set_locale(test);
    else
        set_locale(OS::get_singleton()->get_locale());
    fallback = GLOBAL_DEF("locale/fallback", "en").as<String>();
}
