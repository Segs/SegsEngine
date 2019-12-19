#pragma once
#include "core/forward_decls.h"

class FileAccess;
struct PackedDataFile;

class PackSourceInterface {

public:
    virtual bool try_open_pack(se_string_view p_path, bool p_replace_files) = 0;
    virtual FileAccess *get_file(se_string_view p_path, PackedDataFile *p_file) = 0;
    virtual ~PackSourceInterface() = default;
};
