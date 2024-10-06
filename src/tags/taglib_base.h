/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2024  Marcel Müller
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

#ifndef ET_TAGLIB_TAG_H_
#define ET_TAGLIB_TAG_H_

#include "config.h"
#if defined(ENABLE_ASF) || defined(ENABLE_MP4)

#include "../misc.h"

#include <glib.h>

#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>

#include <string>

struct ET_File;
struct File_Tag;

/// Fetch a property value from TagLib PropertyMap
/// @param fields Property store
/// @param delimiter Field delimiter, used as temporary storage, should be the same for subsequent calls.
/// If null it is initialized by setting "ogg-split-fields" when required.
/// Pass \c nullptr to discard anything but the first value.
/// @param property TagLib name of the property to get
/// @return Property value, empty if not found
std::string taglib_fetch_property(const TagLib::PropertyMap& fields, gString* delimiter, const char* property);

File_Tag* taglib_read_tag(const TagLib::File& tfile, ET_File *ETFile, GError **error);

/// Add a property to a TagLib property store
/// @param fields Target
/// @param delimiter Field delimiter, used as temporary storage, should be the same for subsequent calls.
/// If null it is initialized by setting "ogg-split-fields" when required.
/// Pass \c nullptr to write only one value.
/// @param property TagLib name of the property to add
/// @param value Property value. Nothing is added if empty.
void taglib_set_property(TagLib::PropertyMap& fields, gString* delimiter, const char* property, const char* value);

/// Add all properties to an (empty) TagLib proeprty store
/// @param fields Target
/// @param ETFile source file structure
/// @param split_fields Bit vector of EtColumn with fields that should have multiple values assigned.
/// @remarks The function automatically takes care of
void taglib_write_file_tag(TagLib::PropertyMap& fields, const ET_File *ETFile, unsigned split_fields);

#endif
#endif
