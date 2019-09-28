#pragma once

class String;
class FileAccess;
struct PackedDataFile;

class PackSourceInterface {

public:
    virtual bool try_open_pack(const String &p_path, bool p_replace_files) = 0;
    virtual FileAccess *get_file(const String &p_path, PackedDataFile *p_file) = 0;
    virtual ~PackSourceInterface() {}
};
