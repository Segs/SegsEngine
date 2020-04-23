#include "manifest.h"

#include "core/class_db.h"

#include "core/os/file_access.h"

IMPL_GDCLASS(ResourceManifest)

RES_BASE_EXTENSION_IMPL(ResourceManifest,"manifest")

void ResourceManifest::reload_from_file()
{
    load_manifest(m_path);
}


Error ResourceManifest::load_manifest(StringView p_path) {

    Vector<uint8_t> sourcef;
    Error err;
    FileAccessRef f(FileAccess::open(p_path, FileAccess::READ, &err));
    if (err) {
        ERR_FAIL_COND_V(err, err);
    }

    int len = f->get_len();
    sourcef.resize(len + 1,0);
    int r = f->get_buffer(sourcef.data(), len);
    f->close();
    ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);


    m_path = p_path;
    return OK;
}
