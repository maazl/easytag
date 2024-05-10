/* EasyTAG - tag editor for audio files
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

#include "config.h"
#include "file_description.h"
#include "misc.h"

#include <glib/gi18n.h>

#include <vector>
#include <algorithm>
using namespace std;


static vector<const ET_File_Description*> ETFileDescription;

const struct NotSupportedDescription : ET_File_Description
{	NotSupportedDescription()
	{	Extension = "";
		FileType = _("File");
		TagType = "";
		unsupported_fields = [](const ET_File*) { return ~0U; };
	}
} NotSupportedDescription;

ET_File_Description::ET_File_Description()
{	if (this != &NotSupportedDescription)
		ETFileDescription.push_back(this);
	support_multiple_pictures = [](const ET_File* file) { return true; };
}

ET_File_Description::~ET_File_Description()
{	if (this != &NotSupportedDescription)
		ETFileDescription.erase(find(ETFileDescription.begin(), ETFileDescription.end(), this));
}

const ET_File_Description *
ET_Get_File_Description (const gchar *filename)
{	filename = ET_Get_File_Extension(filename);
	if (!et_str_empty(filename))
		for (auto desc : ETFileDescription)
			if (g_ascii_strcasecmp(desc->Extension, filename) == 0)
				return desc;
	// If not found in the list
	return &NotSupportedDescription;
}
