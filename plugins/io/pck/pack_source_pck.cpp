#include "pack_source_pck.h"

#include "core/os/file_access.h"

#include "core/io/file_access_pack.h"
#include "core/version.h"

#define PACK_VERSION 1

class FileAccessPack : public FileAccess {

    PackedDataFile pf;

    mutable size_t pos;
    mutable bool eof;

    FileAccess *f;
    Error _open(const String &p_path, int p_mode_flags) override;
    uint64_t _get_modified_time(const String &p_file) override { return 0; }
    uint32_t _get_unix_permissions(const String &p_file) override { return 0; }
    Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override { return FAILED; }

public:
    void close() override;
    bool is_open() const override;

    void seek(size_t p_position) override;
    void seek_end(int64_t p_position = 0) override;
    size_t get_position() const override;
    size_t get_len() const override;

    bool eof_reached() const override;

    uint8_t get_8() const override;

    int get_buffer(uint8_t *p_dst, int p_length) const override;

    void set_endian_swap(bool p_swap) override;

    Error get_error() const override;

    void flush() override;
    void store_8(uint8_t p_dest) override;

    void store_buffer(const uint8_t *p_src, int p_length) override;

    bool file_exists(const String &p_name) override;

    FileAccessPack(const String &p_path, const PackedDataFile &p_file);
    ~FileAccessPack() override;
};
//////////////////////////////////////////////////////////////////

Error FileAccessPack::_open(const String &p_path, int p_mode_flags) {

    ERR_FAIL_V(ERR_UNAVAILABLE)
    return ERR_UNAVAILABLE;
}

void FileAccessPack::close() {

    f->close();
}

bool FileAccessPack::is_open() const {

    return f->is_open();
}

void FileAccessPack::seek(size_t p_position) {

    if (p_position > pf.size) {
        eof = true;
    } else {
        eof = false;
    }

    f->seek(pf.offset + p_position);
    pos = p_position;
}
void FileAccessPack::seek_end(int64_t p_position) {

    seek(pf.size + p_position);
}
size_t FileAccessPack::get_position() const {

    return pos;
}
size_t FileAccessPack::get_len() const {

    return pf.size;
}
bool FileAccessPack::eof_reached() const {

    return eof;
}

uint8_t FileAccessPack::get_8() const {

    if (pos >= pf.size) {
        eof = true;
        return 0;
    }

    pos++;
    return f->get_8();
}

int FileAccessPack::get_buffer(uint8_t *p_dst, int p_length) const {

    if (eof)
        return 0;

    uint64_t to_read = p_length;
    if (to_read + pos > pf.size) {
        eof = true;
        to_read = int64_t(pf.size) - int64_t(pos);
    }

    pos += p_length;

    if (to_read <= 0)
        return 0;
    f->get_buffer(p_dst, to_read);

    return to_read;
}

void FileAccessPack::set_endian_swap(bool p_swap) {
    FileAccess::set_endian_swap(p_swap);
    f->set_endian_swap(p_swap);
}

Error FileAccessPack::get_error() const {

    if (eof)
        return ERR_FILE_EOF;
    return OK;
}

void FileAccessPack::flush() {

    ERR_FAIL()
}

void FileAccessPack::store_8(uint8_t p_dest) {

    ERR_FAIL()
}

void FileAccessPack::store_buffer(const uint8_t *p_src, int p_length) {

    ERR_FAIL()
}

bool FileAccessPack::file_exists(const String &p_name) {

    return false;
}

FileAccessPack::FileAccessPack(const String &p_path, const PackedDataFile &p_file) :
        pf(p_file),
        f(FileAccess::open(pf.pack, FileAccess::READ)) {

    ERR_FAIL_COND_MSG(!f, "Can't open pack-referenced file: " + String(pf.pack) + ".")

    f->seek(pf.offset);
    pos = 0;
    eof = false;
}

FileAccessPack::~FileAccessPack() {
    if (f)
        memdelete(f);
}




//////////////////////////////////////////////////////////////////

bool PackedSourcePCK::try_open_pack(const String &p_path) {

    FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
    if (!f)
        return false;

    //printf("try open %ls!\n", p_path.c_str());

    uint32_t magic = f->get_32();

    if (magic != 0x43504447) {
        //maybe at the end.... self contained exe
        f->seek_end();
        f->seek(f->get_position() - 4);
        magic = f->get_32();
        if (magic != 0x43504447) {

            memdelete(f);
            return false;
        }
        f->seek(f->get_position() - 12);

        uint64_t ds = f->get_64();
        f->seek(f->get_position() - ds - 8);

        magic = f->get_32();
        if (magic != 0x43504447) {

            memdelete(f);
            return false;
        }
    }

    uint32_t version = f->get_32();
    uint32_t ver_major = f->get_32();
    uint32_t ver_minor = f->get_32();
    f->get_32(); // ver_rev
    uint32_t major,minor,patch;
    getCoreInterface()->fillVersion(major,minor,patch);

    ERR_FAIL_COND_V_MSG(version != PACK_VERSION, false, "Pack version unsupported: " + itos(version) + ".")
    ERR_FAIL_COND_V_MSG(ver_major > major || (ver_major == major && ver_minor > minor), false,
            "Pack created with a newer version of the engine: " + itos(ver_major) + "." + itos(ver_minor) + ".")

    for (int i = 0; i < 16; i++) {
        //reserved
        f->get_32();
    }

    int file_count = f->get_32();

    for (int i = 0; i < file_count; i++) {

        uint32_t sl = f->get_32();
        CharString cs;
        cs.resize(sl + 1);
        f->get_buffer((uint8_t *)cs.data(), sl);
        cs[sl] = 0;

        String path = StringUtils::from_utf8(cs.data());

        uint64_t ofs = f->get_64();
        uint64_t size = f->get_64();
        uint8_t md5[16];
        f->get_buffer(md5, 16);
        PackedData::get_singleton()->add_path(p_path, path, ofs, size, md5, this);
    }

    return true;
};


FileAccess *PackedSourcePCK::get_file(const String &p_path, PackedDataFile *p_file) {

    return memnew_basic(FileAccessPack(p_path, *p_file));
};
