#pragma once
#include "core/forward_decls.h"

class FileAccess;
struct PackedDataFile;

class PackSourceInterface {

public:
    virtual bool try_open_pack(StringView p_path, bool p_replace_files, StringView p_destination="") = 0;
    virtual FileAccess *get_file(StringView p_path, PackedDataFile *p_file) = 0;
    virtual ~PackSourceInterface() = default;
};
