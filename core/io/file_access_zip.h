/*************************************************************************/
/*  file_access_zip.h                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifdef MINIZIP_ENABLED

#ifndef FILE_ACCESS_ZIP_H
#define FILE_ACCESS_ZIP_H

#include "core/io/file_access_pack.h"
#include "core/map.h"

#include "thirdparty/minizip/unzip.h"

#include <stdlib.h>

class ZipArchive : public PackSource {

public:
	struct File {

		int package;
		unz_file_pos file_pos;
		File() {

			package = -1;
		};
	};

private:
	struct Package {
		String filename;
		unzFile zfile;
	};
	Vector<Package> packages;

	Map<String, File> files;

	static ZipArchive *instance;

	FileAccess::CreateFunc fa_create_func;

public:
	void close_handle(unzFile p_file) const;
	unzFile get_file_handle(String p_file) const;

	Error add_package(String p_name);

	bool file_exists(String p_name) const;

	bool try_open_pack(const String &p_path) override;
	FileAccess *get_file(const String &p_path, PackedData::PackedFile *p_file) override;

	static ZipArchive *get_singleton();

	ZipArchive();
	~ZipArchive() override;
};

class FileAccessZip : public FileAccess {

	unzFile zfile;
	unz_file_info64 file_info;

	mutable bool at_eof;

public:
	Error _open(const String &p_path, int p_mode_flags) override; ///< open a file
	void close() override; ///< close a file
	bool is_open() const override; ///< true when file is open

	void seek(size_t p_position) override; ///< seek to a given position
	void seek_end(int64_t p_position = 0) override; ///< seek from the end of file
	size_t get_position() const override; ///< get position in the file
	size_t get_len() const override; ///< get size of the file

	bool eof_reached() const override; ///< reading passed EOF

	uint8_t get_8() const override; ///< get a byte
	int get_buffer(uint8_t *p_dst, int p_length) const override;

	Error get_error() const override; ///< get last error

	void flush() override;
	void store_8(uint8_t p_dest) override; ///< store a byte
	bool file_exists(const String &p_name) override; ///< return true if a file exists

	uint64_t _get_modified_time(const String &p_file) override { return 0; } // todo
	uint32_t _get_unix_permissions(const String &p_file) override { return 0; }
	Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override { return FAILED; }

	FileAccessZip(const String &p_path, const PackedData::PackedFile &p_file);
	~FileAccessZip() override;
};

#endif // FILE_ACCESS_ZIP_H

#endif // MINIZIP_ENABLED
