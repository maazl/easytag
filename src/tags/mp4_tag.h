/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2012-1014  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2005  Jerome Couderc <easytag@gmail.com>
 * Copyright (C) 2005  Michael Ihde <mike.ihde@randomwalking.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef ET_MP4_TAG_H_
#define ET_MP4_TAG_H_

#include "config.h"
#ifdef ENABLE_MP4

#include <gio/gio.h>

G_BEGIN_DECLS

struct ET_File;
struct File_Tag;
struct EtFileHeaderFields;

File_Tag* mp4_read_file(GFile *file, ET_File *ETFile, GError **error);
gboolean mp4tag_write_file_tag (const ET_File *ETFile, GError **error);
void et_mp4_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile);
unsigned mp4tag_unsupported_fields(const ET_File* file);

G_END_DECLS

#endif
#endif /* ET_MP4_TAG_H_ */
