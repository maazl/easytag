/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
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

#include "config.h" // For definition of ENABLE_OGG

#ifdef ENABLE_OGG

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include "ogg_tag.h"
#include "vcedit.h"
#include "misc.h"
#include "picture.h"
#include "setting.h"
#include "charset.h"
#include "log.h"
#include "file_name.h"
#include "file_tag.h"
#include "file.h"

/* for mkstemp. */
#include "win32/win32dep.h"

#include <cmath>
#include <limits>
#include <memory>
using namespace std;

// registration
struct Ogg_Description : ET_File_Description
{	Ogg_Description(const char* extension, const char* description, File_Tag* (*read_file)(GFile* gfile, ET_File* FileTagNew, GError** error))
	{	Extension = extension;
		FileType = description;
		TagType = _("Ogg Vorbis Tag");
		this->read_file = read_file;
		write_file_tag = ogg_tag_write_file_tag;
		display_file_info_to_ui = et_ogg_header_display_file_info_to_ui;
	}
};

const Ogg_Description
OGG_Description(".ogg", _("Ogg Vorbis File"), ogg_read_file),
OGA_Description(".oga", _("Ogg Vorbis File"), ogg_read_file);
#ifdef ENABLE_SPEEX
const Ogg_Description
SPX_Description(".spx", _("Speex File"), speex_read_file);
#endif


/***************
 * Declaration *
 ***************/

#define VERSION_EXTRACTOR " +\\[([^\\[\\]]+)\\]$"

/*
 * EtOggHeaderState:
 * @file: the Ogg file which is currently being parsed
 * @istream: an input stream for the current Ogg file
 * @error: either the most recent error, or %NULL
 *
 * The current state of the Ogg parser, for passing between the callbacks used
 * in ov_open_callbacks().
 */
typedef struct
{
    GFile *file;
    GInputStream *istream;
    GError *error;
} EtOggHeaderState;

/*
 * et_ogg_read_func:
 * @ptr: the buffer to fill with data
 * @size: the size of individual reads
 * @nmemb: the number of members to read
 * @datasource: the Ogg parser state
 *
 * Read a number of bytes from the Ogg file.
 *
 * Returns: the number of bytes read from the stream. Returns 0 on end-of-file.
 * Sets errno and returns 0 on error
 */
static size_t
et_ogg_read_func (void *ptr, size_t size, size_t nmemb, void *datasource)
{
    EtOggHeaderState *state = (EtOggHeaderState *)datasource;
    gssize bytes_read;

    bytes_read = g_input_stream_read (state->istream, ptr, size * nmemb, NULL,
                                      &state->error);

    if (bytes_read == -1)
    {
        /* FIXME: Convert state->error to errno. */
        errno = EIO;
        return 0;
    }
    else
    {
        return bytes_read;
    }
}

/*
 * et_ogg_seek_func:
 * @datasource: the Ogg parser state
 * @offset: the number of bytes to seek
 * @whence: either %SEEK_SET, %SEEK_CUR or %SEEK_END
 *
 * Seek in the currently-open Ogg file.
 *
 * Returns: 0 on success, -1 and sets errno on error
 */
static int
et_ogg_seek_func (void *datasource, ogg_int64_t offset, int whence)
{
    EtOggHeaderState *state = (EtOggHeaderState *)datasource;
    GSeekType seektype;

    if (!g_seekable_can_seek (G_SEEKABLE (state->istream)))
    {
        return -1;
    }
    else
    {
        switch (whence)
        {
            case SEEK_SET:
                seektype = G_SEEK_SET;
                break;
            case SEEK_CUR:
                seektype = G_SEEK_CUR;
                break;
            case SEEK_END:
                seektype = G_SEEK_END;
                break;
            default:
                errno = EINVAL;
                return -1;
        }

        if (g_seekable_seek (G_SEEKABLE (state->istream), offset, seektype,
                             NULL, &state->error))
        {
            return 0;
        }
        else
        {
            errno = EBADF;
            return -1;
        }
    }
}

/*
 * et_ogg_close_func:
 * @datasource: the Ogg parser state
 *
 * Close the Ogg stream and invalidate the parser state given by @datasource.
 * Be sure to check the error field before invaidating the state.
 *
 * Returns: 0
 */
static int
et_ogg_close_func (void *datasource)
{
    EtOggHeaderState *state = (EtOggHeaderState *)datasource;

    g_clear_object (&state->istream);
    g_clear_error (&state->error);

    return 0;
}

/*
 * et_ogg_tell_func:
 * @datasource: the Ogg parser state
 *
 * Tell the current position of the stream from the beginning of the Ogg file.
 *
 * Returns: the current position in the Ogg file
 */
static long
et_ogg_tell_func (void *datasource)
{
    EtOggHeaderState *state = (EtOggHeaderState *)datasource;

    return g_seekable_tell (G_SEEKABLE (state->istream));
}

/*
 * Read data into an Ogg Vorbis file.
 * Note:
 *  - if field is found but contains no info (strlen(str)==0), we don't read it
 */
File_Tag* ogg_read_file(GFile *file, ET_File *ETFile, GError **error)
{
    OggVorbis_File vf;
    gint res;
    ov_callbacks callbacks = { et_ogg_read_func, et_ogg_seek_func,
                               et_ogg_close_func, et_ogg_tell_func };

    g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

    EtOggHeaderState state;
    state.file = file;
    state.error = NULL;
    state.istream = G_INPUT_STREAM (g_file_read (state.file, NULL,
                                                 &state.error));

    if (!state.istream)
        return g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
            _("Error while opening file: %s"), state.error->message), nullptr;

    /* Check for an unsupported ID3v2 tag. */
    guchar tmp_id3[10];

    if (g_input_stream_read (state.istream, tmp_id3, sizeof(tmp_id3), NULL, error) == sizeof(tmp_id3))
    {
        goffset start = 0;

        /* Calculate ID3v2 length. */
        if (tmp_id3[0] == 'I' && tmp_id3[1] == 'D' && tmp_id3[2] == '3'
            && tmp_id3[3] < 0xFF)
        {
            /* ID3v2 tag skipper $49 44 33 yy yy xx zz zz zz zz [zz size]. */
            /* Size is 6-9 position */
            gchar *path = g_file_get_path (file);
            Log_Print (LOG_WARNING, _("Ogg file '%s' contains an unsupported ID3v2 tag."), path);
            g_free (path);

            start = (tmp_id3[9] | (tmp_id3[8] | (tmp_id3[7] | (tmp_id3[6] << 7) << 7) << 7)) + sizeof(tmp_id3);

            /* Mark the file as modified, so that the ID3 tag is removed
             * upon saving. */
            ETFile->force_tag_save();
        }

        if (!g_seekable_seek (G_SEEKABLE(state.istream), start, G_SEEK_SET, NULL, error))
        {
            g_object_unref (state.istream);
            return nullptr;
        }
    }

    if ((res = ov_open_callbacks (&state, &vf, NULL, 0, callbacks)) == 0)
    {
        vorbis_info* vi = ov_info(&vf, 0);
        if (vi)
        {
            ETFileInfo->version    = vi->version;         // Vorbis encoder version used to create this bitstream.
            ETFileInfo->mode       = vi->channels;        // Number of channels in bitstream.
            ETFileInfo->samplerate = vi->rate;            // (Hz) Sampling rate of the bitstream.
            ETFileInfo->bitrate    = vi->bitrate_nominal; // (b/s) Specifies the average bitrate for a VBR bitstream.
            ETFileInfo->variable_bitrate = vi->bitrate_nominal != vi->bitrate_lower || vi->bitrate_nominal != vi->bitrate_upper;
        }
        else
        {
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s",
                         _("The specified bitstream does not exist or the "
                         "file has been initialized improperly"));
            et_ogg_close_func (&state);
            return nullptr;
        }

        ETFileInfo->duration = ov_time_total(&vf, -1); // (s) Total time.

        File_Tag* FileTag = get_file_tags_from_vorbis_comments(ov_comment(&vf, 0), ETFile);

        ov_clear(&vf); // This close also the file

        return FileTag;
    }else
    {
        /* On error. */
        if (state.error)
        {
            gchar *message;

            switch (res)
            {
                case OV_EREAD:
                    message = _("Read from media returned an error");
                    break;
                case OV_ENOTVORBIS:
                    message = _("Bitstream is not Vorbis data");
                    break;
                case OV_EVERSION:
                    message = _("Vorbis version mismatch");
                    break;
                case OV_EBADHEADER:
                    message = _("Invalid Vorbis bitstream header");
                    break;
                case OV_EFAULT:
                    message = _("Internal logic fault, indicates a bug or heap/stack corruption");
                    break;
                default:
                    message = _("Error reading tags from file");
                    break;
            }

            g_set_error (error, state.error->domain, state.error->code,
                         "%s", message);
        }

        et_ogg_close_func (&state);
        return nullptr;
    }
}


#ifdef ENABLE_SPEEX

File_Tag* speex_read_file(GFile *file, ET_File *ETFile, GError **error)
{
    EtOggState *state;
    const SpeexHeader *si;
    GError *tmp_error = NULL;

    g_return_val_if_fail (file != NULL && ETFile != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    state = vcedit_new_state();    // Allocate memory for 'state'

    if (!vcedit_open (state, file, &tmp_error))
    {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                     _("Failed to open file as Vorbis: %s"),
                     tmp_error->message);
        g_error_free (tmp_error);
        vcedit_clear (state);
        return FALSE;
    }

    ET_File_Info *ETFileInfo = &ETFile->ETFileInfo;

    /* Get Speex information. */
    if ((si = vcedit_speex_header (state)) != NULL)
    {
        ETFileInfo->mpc_version = g_strdup(si->speex_version);
        ETFileInfo->mode        = si->nb_channels; // Number of channels in bitstream.
        ETFileInfo->samplerate  = si->rate;        // (Hz) Sampling rate of the bitstream.
        ETFileInfo->bitrate     = si->bitrate;     // (b/s) Specifies the bitrate

        //if (bitrate > 0)
        //    ETFileInfo->duration = filesize*8/bitrate/1000; // FIXME : Approximation!! Needs to remove tag size!
        //else
        ETFileInfo->duration    = 0.;//ov_time_total(&vf,-1); // (s) Total time.

        //g_print("play time: %ld s\n",(long)ov_time_total(&vf,-1));
        //g_print("serialnumber: %ld\n",(long)ov_serialnumber(&vf,-1));
        //g_print("compressed length: %ld bytes\n",(long)(ov_raw_total(&vf,-1)));
    }

    File_Tag* FileTag = get_file_tags_from_vorbis_comments(vcedit_comments(state), ETFile);

    vcedit_clear(state);
    return FileTag;
}
#endif

void
et_ogg_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    /* Encoder version */
    fields->version_label = _("Encoder:");

    if (!info->mpc_version)
        fields->version = strprintf("%d", info->version);
    else
        fields->version = info->mpc_version;

    /* Mode */
    fields->mode_label = _("Channels:");
    fields->mode = strprintf("%d", info->mode);
}

/*
 * convert_to_byte_array:
 * @n: Integer to convert
 * @array: Destination byte array
 *
 * Converts an integer to byte array.
 */
static void
convert_to_byte_array (gsize n, guchar *array)
{
    array [0] = (n >> 24) & 0xFF;
    array [1] = (n >> 16) & 0xFF;
    array [2] = (n >> 8) & 0xFF;
    array [3] = n & 0xFF;
}

/*
 * add_to_guchar_str:
 * @ustr: Destination string
 * @ustr_len: Pointer to length of destination string
 * @str: String to append
 * @str_len: Length of str
 *
 * Append a guchar string to given guchar string.
 */

static void
add_to_guchar_str (guchar *ustr,
                   gsize *ustr_len,
                   const guchar *str,
                   gsize str_len)
{
    gsize i;

    for (i = *ustr_len; i < *ustr_len + str_len; i++)
    {
        ustr[i] = str[i - *ustr_len];
    }

    *ustr_len += str_len;
}

/*
 * read_guint_from_byte:
 * @str: the byte string
 * @start: position to start with
 *
 * Reads and returns an integer from given byte string starting from start.
 * Returns: Integer which is read
 */
static guint32
read_guint32_from_byte (guchar *str, gsize start)
{
    gsize i;
    guint32 read = 0;

    for (i = start; i < start + 4; i++)
    {
        read = (read << 8) + str[i];
    }

    return read;
}

unsigned vorbis_tag::key_hash::operator()(const xString::cstring& tag) const
{	const char* s = tag.Str;
	const char* se = s + tag.Len;
	unsigned hash = 2166136261U;
	while (s != se && *s != '=')
	{	hash ^= tolower(*s++);
		hash *= 16777619U;
	}
	return hash;
}

bool vorbis_tag::key_equal::operator()(const xString::cstring& tag1, const xString::cstring& tag2) const
{	const char* s1 = tag1.Str;
	const char* s1e = s1 + tag1.Len;
	const char* s2 = tag2.Str;
	const char* s2e = s2 + tag2.Len;
	do
	{	if (s1 == s1e || *s1 == '=')
			return s2 == s2e || *s2 == '=';
		if (s2 == s2e || *s2 == '=')
			return false;
	} while (tolower(*s1++) == tolower(*s2++));
	return true;
}

xString::cstring vorbis_tag::key() const
{	const char* sep = (const char*)memchr(Str, '=', Len);
	if (!sep)
		return *this;
	return xString::cstring(Str, sep - Str);
}

xString::cstring vorbis_tag::value() const
{	const char* sep = (const char*)memchr(Str, '=', Len);
	if (!sep)
		return xString::cstring(nullptr, 0);
	return xString::cstring(sep + 1, Len - (sep + 1 - Str));
}

void vorbis_tags::fetch_field(const vorbis_tag& fieldname, xStringD0& target, bool useNewline)
{	auto range = equal_range(fieldname);
	if (range.first == range.second)
	{	target.reset();
		return;
	}
	auto value = range.first->value();
	if (++range.first == range.second)
	{	// simple: only one instance
		target.assignNFC(value.Str, value.Len);
	} else
	{	// multiple items => concatenate
		// calculate length
		size_t len = value.Len;
		size_t delim_len = 1;
		if (!useNewline)
		{	if (!delimiter)
				delimiter.reset(g_settings_get_string(MainSettings, "split-delimiter"));
			delim_len = strlen(delimiter);
		}
		for (iterator it = range.first; it != range.second; ++it)
			len += delim_len + it->value().Len;
		// assign value
		xString res;
		char* dp = res.alloc(len);
		memcpy(dp, value.Str, value.Len);
		for (iterator it = range.first; it != range.second; ++it)
		{	dp += value.Len;
			if (useNewline)
				*dp = '\n';
			else
				memcpy(dp, delimiter, delim_len);
			dp += delim_len;
			value = it->value();
			memcpy(dp, value.Str, value.Len);
		}
		target.assignNFC(res);
	}
	erase(range.first, range.second);
}

float vorbis_tags::fetch_float(const vorbis_tag& fieldname)
{	auto it = find(fieldname);
	if (it == end())
		return numeric_limits<float>::quiet_NaN();
	auto value = it->value();
	float f = File_Tag::parse_float(string(value.Str, value.Len).c_str());
	erase(it);
	return f;
}

void vorbis_tags::to_file_tags(File_Tag *FileTag)
{
	fetch_field(ET_VORBIS_COMMENT_FIELD_TITLE, FileTag->title);
	fetch_field(ET_VORBIS_COMMENT_FIELD_VERSION, FileTag->version);
	fetch_field(ET_VORBIS_COMMENT_FIELD_SUBTITLE, FileTag->subtitle);

	fetch_field(ET_VORBIS_COMMENT_FIELD_ARTIST, FileTag->artist);
	fetch_field(ET_VORBIS_COMMENT_FIELD_ALBUM_ARTIST, FileTag->album_artist);

	fetch_field(ET_VORBIS_COMMENT_FIELD_ALBUM, FileTag->album);
	fetch_field(ET_VORBIS_COMMENT_FIELD_DISC_SUBTITLE, FileTag->disc_subtitle);

	/* Disc number and total discs. */
	fetch_field(ET_VORBIS_COMMENT_FIELD_DISC_TOTAL, FileTag->disc_total);
	FileTag->disc_total = et_disc_number_to_string(FileTag->disc_total);

	fetch_field(ET_VORBIS_COMMENT_FIELD_DISC_NUMBER, FileTag->disc_number);
	if (!et_str_empty(FileTag->disc_number))
		FileTag->disc_and_total(FileTag->disc_number);

	fetch_field(ET_VORBIS_COMMENT_FIELD_DATE, FileTag->year);
	fetch_field(ET_VORBIS_COMMENT_FIELD_RELEASE_DATE, FileTag->release_year);

	/* Track number and total tracks. */
	fetch_field(ET_VORBIS_COMMENT_FIELD_TRACK_TOTAL, FileTag->track_total);
	FileTag->track_total = et_disc_number_to_string(FileTag->track_total);

	fetch_field(ET_VORBIS_COMMENT_FIELD_TRACK_NUMBER, FileTag->track);
	if (!et_str_empty(FileTag->track))
		FileTag->track_and_total(FileTag->track);

	fetch_field(ET_VORBIS_COMMENT_FIELD_GENRE, FileTag->genre);
	fetch_field(ET_VORBIS_COMMENT_FIELD_COMMENT, FileTag->comment, g_settings_get_boolean(MainSettings, "tag-multiline-comment"));
	fetch_field(ET_VORBIS_COMMENT_FIELD_DESCRIPTION, FileTag->description, TRUE);

	fetch_field(ET_VORBIS_COMMENT_FIELD_COMPOSER, FileTag->composer);
	fetch_field(ET_VORBIS_COMMENT_FIELD_PERFORMER, FileTag->orig_artist);
	fetch_field(ET_VORBIS_COMMENT_FIELD_ORIG_DATE, FileTag->orig_year);

	fetch_field(ET_VORBIS_COMMENT_FIELD_COPYRIGHT, FileTag->copyright);
	fetch_field(ET_VORBIS_COMMENT_FIELD_CONTACT, FileTag->url);
	fetch_field(ET_VORBIS_COMMENT_FIELD_ENCODED_BY, FileTag->encoded_by);

	FileTag->track_gain = fetch_float(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_GAIN);
	FileTag->track_peak = fetch_float(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_PEAK);
	FileTag->album_gain = fetch_float(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_GAIN);
	FileTag->album_peak = fetch_float(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_PEAK);
}

void vorbis_tags::to_other_tags(ET_File *ETFile)
{	gString* arr = new gString[size() + 1];
	ETFile->other.reset(arr);
	for (const auto& v : *this)
		*arr++ = g_strndup(v.Str, v.Len);
}

// Variant of g_base64_decode_step that can deal with length limited input
static guchar* base64_decode(const char* str, size_t len, gsize& data_size)
{	gint state = 0;
	guint save = 0;
	guchar* out = (guchar*)g_malloc((len / 4 + 1) * 3);
	data_size = g_base64_decode_step(str, len, out, &state, &save);
	return out;
}

/*
 * et_add_file_tags_from_vorbis_comments:
 * @vc: Vorbis comment from which to fill @FileTag
 * @FileTag: tag to populate from @vc
 *
 * Reads Vorbis comments and copies them to file tag.
 */
File_Tag*
get_file_tags_from_vorbis_comments (const vorbis_comment *vc, ET_File *ETFile)
{
	if (!vc)
		return nullptr;

	File_Tag *FileTag = new File_Tag();
	vorbis_tags tags(vc->comments);

	for (int i = 0; i < vc->comments; i++)
		tags.emplace(vc->user_comments[i], vc->comment_lengths[i]);

	/* add standard tags */
	tags.to_file_tags(FileTag);

	/* Cover art. */
	auto range = tags.equal_range(ET_VORBIS_COMMENT_FIELD_COVER_ART);
	if (range.first != range.second)
	{
		auto types = tags.equal_range(ET_VORBIS_COMMENT_FIELD_COVER_ART_TYPE);
		auto descs = tags.equal_range(ET_VORBIS_COMMENT_FIELD_COVER_ART_DESCRIPTION);
		vorbis_tags::iterator l = range.first;
		vorbis_tags::iterator m = types.first;
		vorbis_tags::iterator n = descs.first;

		/* Force marking the file as modified, so that the deprecated cover art
		 * field is converted to a METADATA_PICTURE_BLOCK field. */
		ETFile->force_tag_save();

		for (; l != range.second; ++l)
		{	auto value = l->value();
			if (!value.Len)
				break;

			/* Decode picture data. */
			gsize data_size;
			gAlloc<guchar> data(base64_decode(value.Str, value.Len, data_size));

			/* It is only necessary for there to be image data, but the type
			 * and description are optional. */
			EtPictureType type = ET_PICTURE_TYPE_FRONT_COVER;
			if (m != types.second)
			{	value = m++->value();
				if (!value.Len)
					type = (EtPictureType)atoi(string(value.Str, value.Len).c_str());
			}

			xStringD0 description;
			if (n != descs.second)
			{	value = n++->value();
				if (!value.Len)
					description.assignNFC(value);
			}

			FileTag->pictures.emplace_back(type, description, 0, 0, data.get(), data_size);
		}

		tags.erase(range.first, range.second); // invalidates iterators
		tags.erase(types.first, types.second);
		tags.erase(descs.first, descs.second);
	}

	/* METADATA_BLOCK_PICTURE tag used for picture information. */
	range = tags.equal_range(ET_VORBIS_COMMENT_FIELD_METADATA_BLOCK_PICTURE);
	if (range.first != range.second)
	{
		for (vorbis_tags::iterator l = range.first; l != range.second; l = ++l)
		{
			gsize bytes_pos, mimelen, desclen;
			EtPictureType type;
			gsize data_size;

			/* Decode picture data. */
			auto value = l->value();
			gsize decoded_size;
			gAlloc<guchar> decoded_ustr(base64_decode(value.Str, value.Len, decoded_size));

			/* Check that the comment decoded to a long enough string to hold the
			 * whole structure (8 fields of 4 bytes each). */
			if (decoded_size < 8 * 4)
			{invalid_picture:
				/* Mark the file as modified, so that the invalid field is removed upon
				 * saving. */
				ETFile->force_tag_save();
				continue;
			}

			/* Reading picture type. */
			type = (EtPictureType)read_guint32_from_byte(decoded_ustr.get(), 0);
			bytes_pos = 4;

			/* TODO: Check that there is a maximum of 1 of each of
			 * ET_PICTURE_TYPE_FILE_ICON and ET_PICTURE_TYPE_OTHER_FILE_ICON types
			 * in the file. */
			if (type >= ET_PICTURE_TYPE_UNDEFINED)
				goto invalid_picture;

			/* Reading MIME data. */
			mimelen = read_guint32_from_byte(decoded_ustr.get(), bytes_pos);
			bytes_pos += 4;

			if (mimelen > decoded_size - bytes_pos - (6 * 4))
				goto invalid_picture;

			/* Check for a valid MIME type. */
			if (mimelen > 0)
			{	const gchar *mime = (const gchar*)&decoded_ustr.get()[bytes_pos];
				/* TODO: Check for "-->" when adding linked image support. */
				if (strncmp (mime, "image/", mimelen) != 0
					&& strncmp (mime, "image/png", mimelen) != 0
					&& strncmp (mime, "image/jpeg", mimelen) != 0)
				{	g_debug("Invalid Vorbis comment image MIME type: %*s", (int)mimelen, mime);
					goto invalid_picture;
				}
			}

			/* Skip over the MIME type, as gdk-pixbuf does not use it. */
			bytes_pos += mimelen;

			/* Reading description */
			desclen = read_guint32_from_byte (decoded_ustr.get(), bytes_pos);
			bytes_pos += 4;

			if (desclen > decoded_size - bytes_pos - (5 * 4))
				goto invalid_picture;

			xStringD0 description;
			description.assignNFC((const char*)&decoded_ustr.get()[bytes_pos], desclen);

			/* Skip the width, height, color depth and number-of-colors fields. */
			bytes_pos += desclen + 16;

			/* Reading picture size */
			data_size = read_guint32_from_byte (decoded_ustr.get(), bytes_pos);
			bytes_pos += 4;

			if (data_size > decoded_size - bytes_pos)
				goto invalid_picture;

			FileTag->pictures.emplace_back(type, description, 0, 0,
				decoded_ustr.get() + bytes_pos, data_size);
		}

		tags.erase(range.first, range.second);
	}

	/* Save unsupported fields. */
	tags.to_other_tags(ETFile);

	// validate date fields
	FileTag->check_dates(3, true, *ETFile->FileNameCur()); // From field 3 arbitrary strings are allowed

	return FileTag;
}

gboolean
ogg_tag_write_file_tag (const ET_File *ETFile,
                        GError **error)
{
    GFile           *file;
    EtOggState *state;
    vorbis_comment *vc;

    g_return_val_if_fail (ETFile != NULL && ETFile->FileTagNew() != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    const File_Tag* FileTag = ETFile->FileTagNew();

    file = g_file_new_for_path(ETFile->FilePath);

    state = vcedit_new_state();    // Allocate memory for 'state'

    if (!vcedit_open (state, file, error))
    {
        g_assert (error == NULL || *error != NULL);
        g_object_unref (file);
        vcedit_clear(state);
        return FALSE;
    }

    g_assert (error == NULL || *error == NULL);

    /* Get data from tag */
    vc = vcedit_comments(state);
    vorbis_comment_clear(vc);
    vorbis_comment_init(vc);

    gString delimiter;
    auto et_ogg_set_tag = [vc, &delimiter](const gchar *tag_name, const gchar *value, gint split = 0)
    {
        if (et_str_empty(value))
            return;

        if (split && (g_settings_get_flags(MainSettings, "ogg-split-fields") & abs(split)))
        {   const char* delim = "\n";
            if (split > 0)
            {   if (!delimiter)
                    delimiter.reset(g_settings_get_string(MainSettings, "split-delimiter"));
                delim = delimiter;
            }

            gchar **strings = g_strsplit(value, delim, 255);
            for (int i = 0; strings[i] != NULL; i++)
                vorbis_comment_add_tag(vc, tag_name, strings[i]);
            g_strfreev (strings);
        }
        else
            vorbis_comment_add_tag(vc, tag_name, value);
    };

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_TITLE, FileTag->title, ET_PROCESS_FIELD_TITLE);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_VERSION, FileTag->version, ET_PROCESS_FIELD_VERSION);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_SUBTITLE, FileTag->subtitle, ET_PROCESS_FIELD_SUBTITLE);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_ARTIST, FileTag->artist, ET_PROCESS_FIELD_ARTIST);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_ALBUM_ARTIST, FileTag->album_artist, ET_PROCESS_FIELD_ALBUM_ARTIST);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_ALBUM, FileTag->album, ET_PROCESS_FIELD_ALBUM);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_DISC_SUBTITLE, FileTag->disc_subtitle, ET_PROCESS_FIELD_DISC_SUBTITLE);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_DISC_NUMBER, FileTag->disc_number);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_DISC_TOTAL, FileTag->disc_total);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_DATE, FileTag->year);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_RELEASE_DATE, FileTag->release_year);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_TRACK_NUMBER, FileTag->track);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_TRACK_TOTAL, FileTag->track_total);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_GENRE, FileTag->genre, ET_PROCESS_FIELD_GENRE);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_COMMENT, FileTag->comment, ET_PROCESS_FIELD_COMMENT * (g_settings_get_boolean(MainSettings, "tag-multiline-comment") ? -1 : 1));
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_DESCRIPTION, FileTag->description, -ET_PROCESS_FIELD_DESCRIPTION);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_COMPOSER, FileTag->composer, ET_PROCESS_FIELD_COMPOSER);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_PERFORMER, FileTag->orig_artist, ET_PROCESS_FIELD_ORIGINAL_ARTIST);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_ORIG_DATE, FileTag->orig_year);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_COPYRIGHT, FileTag->copyright);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_CONTACT, FileTag->url, ET_PROCESS_FIELD_URL);
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_ENCODED_BY, FileTag->encoded_by, ET_PROCESS_FIELD_ENCODED_BY);

    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_GAIN, FileTag->track_gain_str().c_str());
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_TRACK_PEAK, FileTag->track_peak_str().c_str());
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_GAIN, FileTag->album_gain_str().c_str());
    et_ogg_set_tag(ET_VORBIS_COMMENT_FIELD_REPLAYGAIN_ALBUM_PEAK, FileTag->album_peak_str().c_str());

    /***********
     * Picture *
     ***********/
    for (const EtPicture& pic : FileTag->pictures)
    {
        const gchar *mime;
        guchar array[4];
        guchar *ustring = NULL;
        gsize ustring_len = 0;
        gchar *base64_string;
        gsize desclen;
        Picture_Format format = pic.Format();
        gBytes bytes;

        /* According to the specification, only PNG and JPEG images should
         * be added to Vorbis comments. */
        if (format != PICTURE_FORMAT_PNG && format != PICTURE_FORMAT_JPEG)
        {
            GdkPixbufLoader *loader;
            GError *loader_error = NULL;

            loader = gdk_pixbuf_loader_new ();

            if (!gdk_pixbuf_loader_write_bytes(loader, pic.bytes().get(), &loader_error))
            {
                g_debug ("Error parsing image data: %s",
                         loader_error->message);
                g_error_free (loader_error);
                g_object_unref (loader);
                continue;
            }
            else
            {
                GdkPixbuf *pixbuf;
                gchar *buffer;
                gsize buffer_size;

                if (!gdk_pixbuf_loader_close (loader, &loader_error))
                {
                    g_debug ("Error parsing image data: %s",
                             loader_error->message);
                    g_error_free (loader_error);
                    g_object_unref (loader);
                    continue;
                }

                pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

                if (!pixbuf)
                {
                    g_object_unref (loader);
                    continue;
                }

                g_object_ref (pixbuf);
                g_object_unref (loader);

                /* Always convert to PNG. */
                if (!gdk_pixbuf_save_to_buffer (pixbuf, &buffer,
                                                &buffer_size, "png",
                                                &loader_error, NULL))
                {
                    g_debug ("Error while converting image to PNG: %s",
                             loader_error->message);
                    g_error_free (loader_error);
                    g_object_unref (pixbuf);
                    continue;
                }

                g_object_unref (pixbuf);

                bytes = gBytes(g_bytes_new_take(buffer, buffer_size));

                /* Set the picture format to reflect the new data. */
                format = PICTURE_FORMAT_PNG;
            }
        } else
            bytes = pic.bytes();

        mime = EtPicture::Mime_Type_String(format);

        /* Calculating full length of byte string and allocating. */
        desclen = strlen(pic.description);
        ustring = (guchar*)g_malloc (4 * 8 + strlen (mime) + desclen + bytes.size());

        /* Adding picture type. */
        convert_to_byte_array (pic.type, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        /* Adding MIME string and its length. */
        convert_to_byte_array (strlen (mime), array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);
        add_to_guchar_str (ustring, &ustring_len, (const guchar *)mime,
                           strlen (mime));

        /* Adding picture description. */
        convert_to_byte_array (desclen, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);
        add_to_guchar_str (ustring, &ustring_len,
                           (guchar *)pic.description.get(),
                           desclen);

        /* Adding width, height, color depth, indexed colors. */
        convert_to_byte_array (pic.storage->Width, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        convert_to_byte_array (pic.storage->Height, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        /* TODO: Determine the depth per pixel by querying the pixbuf to see
         * whether an alpha channel is present. */
        convert_to_byte_array (0, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        /* Non-indexed images should set this to zero. */
        convert_to_byte_array (0, array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        /* Adding picture data and its size. */
        convert_to_byte_array (bytes.size(), array);
        add_to_guchar_str (ustring, &ustring_len, array, 4);

        add_to_guchar_str (ustring, &ustring_len, (const guchar*)bytes.data(), bytes.size());

        base64_string = g_base64_encode (ustring, ustring_len);
        vorbis_comment_add_tag (vc,
                                ET_VORBIS_COMMENT_FIELD_METADATA_BLOCK_PICTURE,
                                base64_string);

        g_free (base64_string);
        g_free (ustring);
    }

    /**************************
     * Set unsupported fields *
     **************************/
    if (ETFile->other)
        for (gString* l = ETFile->other.get(); *l; ++l)
            vorbis_comment_add(vc, *l);

    /* Write tag to 'file' in all cases */
    if (!vcedit_write (state, file, error))
    {
        g_assert (error == NULL || *error != NULL);
        g_object_unref (file);
        vcedit_clear(state);
        return FALSE;
    }
    else
    {
        vcedit_clear (state);
    }

    g_object_unref (file);

    g_assert (error == NULL || *error == NULL);
    return TRUE;
}
#endif /* ENABLE_OGG */
