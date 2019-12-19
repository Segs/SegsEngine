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
#pragma once

#include "core/io/file_access_pack.h"
#include "core/map.h"

#include "thirdparty/minizip/unzip.h"

#include <cstdlib>

#include "core/plugin_interfaces/PluginDeclarations.h"

class ZipArchive : public QObject, public PackSourceInterface {
    Q_PLUGIN_METADATA(IID "org.godot.ZipArchive")
    Q_INTERFACES(PackSourceInterface)
    Q_OBJECT
public:
	struct File {

		int package;
		unz_file_pos file_pos;
		File() {

			package = -1;
		}
	};

private:
	struct Package {
        se_string filename;
		unzFile zfile;
	};
	Vector<Package> packages;

    Map<se_string, File> files;

	static ZipArchive *instance;

	FileAccess::CreateFunc fa_create_func;

public:
	void close_handle(unzFile p_file) const;
    unzFile get_file_handle(se_string_view p_file) const;

	Error add_package(String p_name);

    bool file_exists(se_string_view p_name) const;

	bool try_open_pack(se_string_view p_path, bool p_replace_files) override;
    FileAccess *get_file(se_string_view p_path, PackedDataFile *p_file) override;

	static ZipArchive *get_singleton();

    ZipArchive(QObject *ob=nullptr);
	~ZipArchive() override;
};
