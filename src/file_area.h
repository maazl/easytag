/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022-2026  Marcel Müller <github@maazl.de>
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ET_FILE_AREA_H_
#define ET_FILE_AREA_H_

#include "setting.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <cstddef>

struct EtFileHeaderFields;
struct ET_File;

GType et_file_area_get_type();
#define ET_TYPE_FILE_AREA (et_file_area_get_type ())
#define ET_FILE_AREA(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ET_TYPE_FILE_AREA, EtFileArea))

typedef struct _EtFileArea EtFileArea;
typedef struct _EtFileAreaClass EtFileAreaClass;

struct _EtFileArea
{private:
	/*< private >*/
	GtkBin parent_instance;

	/// Get default header fields w/o specific tag handler.
	static void default_header_fields(EtFileHeaderFields& fields, const ET_File& ETFile);
	void set_header_fields(const EtFileHeaderFields& fields);
public:
	void clear();
	/// Update file area controls from file instance
	void display_et_file(const ET_File *ETFile, EtColumn columns);
	const gchar* get_file_name();
	const gchar* get_file_path();
};

struct _EtFileAreaClass
{
    /*< private >*/
    GtkBinClass parent_class;
};

#endif /* ET_FILE_AREA_H_ */
