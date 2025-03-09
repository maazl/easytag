/* EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
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

#ifndef ET_OGG_TAG_H_
#define ET_OGG_TAG_H_

#ifdef ENABLE_OGG

#include "config.h"

#include <glib.h>
#include <gio/gio.h>
#include <vorbis/vorbisfile.h>

#include "../misc.h"
#include "../xstring.h"

struct ET_File;
struct File_Tag;
struct EtFileHeaderFields;

#include <unordered_set>
#include <string>

struct vorbis_tag : public xString::cstring
{
	struct key_hash
	{	unsigned operator()(const xString::cstring& tag) const;
	};

	struct key_equal
	{	bool operator()(const xString::cstring& tag1, const xString::cstring& tag2) const;
	};

	constexpr vorbis_tag(const char* str, std::size_t len) noexcept : xString::cstring(str, len) {}
	template <std::size_t N>
	constexpr vorbis_tag(const char (&str)[N]) noexcept : xString::cstring(str, N-1) {}

	xString::cstring key() const;
	xString::cstring value() const;
};

class vorbis_tags : public std::unordered_multiset<vorbis_tag, vorbis_tag::key_hash, vorbis_tag::key_equal>
{	gString delimiter;
	void fetch_field(const vorbis_tag& fieldname, xStringD0& target, bool useNewline = false);
	float fetch_float(const vorbis_tag& fieldname);
public:
	vorbis_tags(std::size_t capacity) : std::unordered_multiset<vorbis_tag, vorbis_tag::key_hash, vorbis_tag::key_equal>(capacity) {}
	void to_file_tags(File_Tag *FileTag);
	void to_other_tags(ET_File *ETFile);
};

/*
 * EtOGGError:
 * @ET_OGG_ERROR_BOS: beginning of stream not found
 * @ET_OGG_ERROR_EOS: reached end of logical bitstream
 * @ET_OGG_ERROR_EOF: reached end of file
 * @ET_OGG_ERROR_SN: page and state's serial number are unequal
 * @ET_OGG_ERROR_TRUNC: input truncated or empty
 * @ET_OGG_ERROR_NOTOGG: input is not ogg bitstream
 * @ET_OGG_ERROR_PAGE: cannot read first page of ogg bitstream
 * @ET_OGG_ERROR_HEADER: error reading initial header packet
 * @ET_OGG_ERROR_INVALID: ogg bitstream doesnot contains Speex or Vorbis data
 * @ET_OGG_ERROR_CORRUPT: corrupt secondary header
 * @ET_OGG_ERROR_EXTRA: need to save extra headers
 * @ET_OGG_ERROR_VORBIS: eof before end of Vorbis headers
 * @ET_OGG_ERROR_FAILED: corrupt or missing data
 * @ET_OGG_ERROR_OUTPUT: error writing stream to output
 *
 * Errors that can occur when reading OGG files.
 */
typedef enum
{
    ET_OGG_ERROR_BOS,
    ET_OGG_ERROR_EOS,
    ET_OGG_ERROR_EOF,
    ET_OGG_ERROR_SN,
    ET_OGG_ERROR_TRUNC,
    ET_OGG_ERROR_NOTOGG,
    ET_OGG_ERROR_PAGE,
    ET_OGG_ERROR_HEADER,
    ET_OGG_ERROR_INVALID,
    ET_OGG_ERROR_CORRUPT,
    ET_OGG_ERROR_EXTRA,
    ET_OGG_ERROR_VORBIS,
    ET_OGG_ERROR_FAILED,
    ET_OGG_ERROR_OUTPUT
} EtOGGError;

GQuark et_ogg_error_quark (void);

#define ET_OGG_ERROR et_ogg_error_quark ()

File_Tag* ogg_read_file (GFile *file, ET_File *ETFile, GError **error);
gboolean ogg_tag_write_file_tag (const ET_File *ETFile, GError **error);

File_Tag* get_file_tags_from_vorbis_comments (const vorbis_comment *vc, ET_File *ETFile);
void et_add_vorbis_comments_from_file_tags (vorbis_comment *vc, File_Tag *FileTag);

void et_ogg_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile);

File_Tag* speex_read_file(GFile *file, ET_File *ETFile, GError **error);

#endif /* ENABLE_OGG */

#endif /* ET_OGG_TAG_H_ */
