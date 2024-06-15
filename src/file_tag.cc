/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024  Marcel Müller
 * Copyright (C) 2014,2015  David King <amigadave@amigadave.com>
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

#include "file_tag.h"

#include "misc.h"
#include "log.h"

#include <limits>
#include <cmath>
#include <string>

using namespace std;

File_Tag::File_Tag()
:	key(et_undo_key_new ())
,	saved(false)
,	picture(nullptr)
,	other(nullptr)
,	track_gain(numeric_limits<float>::quiet_NaN())
,	track_peak(numeric_limits<float>::quiet_NaN())
,	album_gain(numeric_limits<float>::quiet_NaN())
,	album_peak(numeric_limits<float>::quiet_NaN())
{}

File_Tag::File_Tag(const File_Tag& r)
:	key(et_undo_key_new ())
,	saved(false)
,	title(r.title)
,	subtitle(r.subtitle)
,	version(r.version)
,	artist(r.artist)
,	album_artist(r.album_artist)
,	album(r.album)
,	disc_subtitle(r.disc_subtitle)
,	disc_number(r.disc_number)
,	disc_total(r.disc_total)
,	year(r.year)
,	release_year(r.release_year)
,	track(r.track)
,	track_total(r.track_total)
,	genre(r.genre)
,	comment(r.comment)
,	composer(r.composer)
,	orig_artist(r.orig_artist)
,	orig_year(r.orig_year)
,	copyright(r.copyright)
,	url(r.url)
,	encoded_by(r.encoded_by)
,	description(r.description)
,	picture(nullptr)
,	other(nullptr)
,	track_gain(r.track_gain)
,	track_peak(r.track_peak)
,	album_gain(r.album_gain)
,	album_peak(r.album_peak)
{
	et_file_tag_set_picture(this, r.picture);

	for (GList* l = r.other; l != NULL; l = g_list_next(l))
		other = g_list_prepend(other, g_strdup((gchar*)l->data));
	other = g_list_reverse(other);
}

/*
 * Frees the list of 'other' field in a File_Tag item (contains attached gchar data).
 */
static void
et_file_tag_free_other_field (File_Tag *file_tag)
{
    g_list_free_full (file_tag->other, g_free);
    file_tag->other = NULL;
}


File_Tag::~File_Tag()
{
	et_file_tag_set_picture(this, NULL);
	et_file_tag_free_other_field(this);
}

string File_Tag::format_float(const char* fmt, float value)
{	string ret(12, '\0');
	if (isfinite(value))
	{	unsigned len = snprintf(&ret[0], ret.size(), fmt, value);
		if (len < ret.size()) ret.resize(len);
		auto dot = ret.find(','); // work around for locale problems
		if (dot != string::npos) ret[dot] = '.';
	}
	ret.erase(strlen(ret.c_str()));
	return ret;
}

float File_Tag::parse_float(const char* value)
{	if (et_str_empty(value))
		return numeric_limits<float>::quiet_NaN();
	// ugly work around for locale problems
	while (isspace(*value)) ++value;
	int sign = 1;
	if (*value == '-')
	{	sign = -1;
		++value;
	}
	int i, n = -1;
	if (!*value || sscanf(value, "%d%*[.,]%n", &i, &n) < 1)
		return numeric_limits<float>::quiet_NaN();
	double ret = i;
	if (n > 0)
	{	value += n;
		double exp = .1;
		while (isdigit(*value))
		{	ret += exp * (*value++ - '0');
			exp /= 10;
		}
	}
	return ret * sign;
}

bool File_Tag::empty() const
{	return !title
		&& !version
		&& !subtitle
		&& !artist
		&& !album_artist
		&& !album
		&& !disc_subtitle
		&& !disc_number
		&& !disc_total
		&& !year
		&& !release_year
		&& !track
		&& !track_total
		&& !genre
		&& !comment
		&& !composer
		&& !orig_artist
		&& !orig_year
		&& !copyright
		&& !url
		&& !encoded_by
		&& !picture
		&& !isfinite(track_gain)
		&& !isfinite(track_peak)
		&& !isfinite(album_gain)
		&& !isfinite(album_peak);
}

/*
 * et_file_tag_set_picture:
 * @file_tag: the #File_Tag on which to set the image
 * @pic: the image to set
 *
 * Set the images inside @file_tag to be @pic, freeing existing images as
 * necessary. Copies @pic with et_picture_copy_all().
 */
void
et_file_tag_set_picture (File_Tag *file_tag,
                         const EtPicture *pic)
{
    g_return_if_fail (file_tag != NULL);

    if (file_tag->picture != NULL)
    {
        et_picture_free (file_tag->picture);
        file_tag->picture = NULL;
    }

    if (pic)
    {
        file_tag->picture = et_picture_copy_all (pic);
    }
}

static bool floatequals(float f1, float f2, float epsilon)
{	return isnan(f1) == isnan(f2)
		&& !(fabs(f1 - f2) >= epsilon);
}

/*
 * Compares two File_Tag items and returns TRUE if there aren't the same.
 * Notes:
 *  - if field is '' or NULL => will be removed
 */
bool operator!=(const File_Tag& l, const File_Tag& r)
{
    const EtPicture *pic1;
    const EtPicture *pic2;

    if (l.title != r.title
        || l.version != r.version
        || l.subtitle != r.subtitle
        || l.artist != r.artist
        || l.album_artist != r.album_artist
        || l.album != r.album
        || l.disc_subtitle != r.disc_subtitle
        || l.disc_number != r.disc_number
        || l.disc_total != r.disc_total
        || l.year != r.year
        || l.release_year != r.release_year
        || l.track != r.track
        || l.track_total != r.track_total
        || l.genre != r.genre
        || l.comment != r.comment
        || l.composer != r.composer
        || l.orig_year != r.orig_year
        || l.orig_artist != r.orig_artist
        || l.copyright != r.copyright
        || l.url != r.url
        || l.encoded_by != r.encoded_by
        || l.description != r.description
        || !floatequals(l.track_gain, r.track_gain, File_Tag::gain_epsilon)
        || !floatequals(l.track_peak, r.track_peak, File_Tag::peak_epsilon)
        || !floatequals(l.album_gain, r.album_gain, File_Tag::gain_epsilon)
        || !floatequals(l.album_peak, r.album_peak, File_Tag::peak_epsilon))
        return TRUE;

    /* Picture */
    for (pic1 = l.picture, pic2 = r.picture;
         pic1 || pic2;
         pic1 = pic1->next, pic2 = pic2->next)
        if (et_picture_detect_difference (pic1, pic2))
            return TRUE;

    return FALSE; /* No changes */
}

File_Tag::time File_Tag::parse_datetime(const char*& value)
{	time ret;
	int n;
	if (et_str_empty(value))
		return ret;

	if (sscanf(value, "%4u%n", &ret.tm_year, &n) != 1)
		goto fail;
	ret.tm_year -= 1900;
	ret.field_count = 1;
	value += n;
	if (!*value)
		return ret;

	if (sscanf(value, "%*1[-:/]%2u%n", &ret.tm_mon, &n) != 1)
		goto fail;
	ret.tm_mon -= 1;
	ret.field_count = 2;
	value += n;
	if (!*value)
		return ret;

	if (sscanf(value, "%*1[-:/]%2u%n", &ret.tm_mday, &n) != 1)
		goto fail;
	ret.field_count = 3;
	value += n;
	if (!*value)
		return ret;

	if (sscanf(value, "%*1[ T]%2u%n", &ret.tm_hour, &n) != 1)
		goto fail;
	ret.field_count = 4;
	value += n;
	if (!*value)
		return ret;

	if (sscanf(value, "%*1[:-]%2u%n", &ret.tm_min, &n) != 1)
		goto fail;
	ret.field_count = 5;
	value += n;
	if (!*value)
		return ret;

	if (sscanf(value, "%*1[:-]%2u%n", &ret.tm_sec, &n) != 1)
		goto fail;
	ret.field_count = 6;
	value += n;
	if (!*value)
		return ret;

fail:
	ret.invalid = true;
	return ret;
}

/** Check whether time stamp valid for the current format.
 * @param value Tag value
 * @param max_fields Maximum number of fields, i.e. 1111-22-33T44:55:66.
 * @param additional_content Allow arbitrary additional content after the last field.
 * @return true: valid
 */
bool File_Tag::check_date(const char* value, int max_fields, bool additional_content)
{
	File_Tag::time t = parse_datetime(value);
	if (!t.invalid)
		return t.field_count <= max_fields;
	if (!additional_content)
		return false;
	return t.field_count <= max_fields;
}

std::string File_Tag::track_and_total() const
{	if (et_str_empty(track))
		return std::string();
	if (et_str_empty(track_total))
		return track.get();
	return std::string(track) + '/' + track_total.get();
}

std::string File_Tag::disc_and_total() const
{	if (et_str_empty(disc_number))
		return std::string();
	if (et_str_empty(disc_total))
		return disc_number.get();
	return std::string(disc_number) + '/' + disc_total.get();
}

void File_Tag::track_and_total(const char* value)
{	if (et_str_empty(value))
	{	track.reset();
		track_total.reset();
		return;
	}
	/* cut off the total tracks if present */
	const char *field2 = strchr(value, '/');
	if (field2)
	{	track_total.assignNFC(et_disc_number_to_string(field2 + 1));
		string field1(value, field2 - value);
		track.assignNFC(et_disc_number_to_string(field1.c_str()));
	} else
	{	track.assignNFC(et_disc_number_to_string(value).c_str());
		track_total.reset();
	}
}

void File_Tag::disc_and_total(const char* value)
{	if (et_str_empty(value))
	{	disc_number.reset();
		disc_total.reset();
		return;
	}
	/* cut off the total discs if present */
	const char *field2 = strchr(value, '/');
	if (field2)
	{	disc_total.assignNFC(et_disc_number_to_string(field2 + 1));
		string field1(value, field2 - value);
		disc_number.assignNFC(et_disc_number_to_string(field1.c_str()));
	} else
	{	disc_number.assignNFC(et_disc_number_to_string(value).c_str());
		disc_total.reset();
	}
}

static bool cmpassign(xString& l, string r)
{	if (l.equals(r))
		return false;
	l = r;
	return true;
}

bool File_Tag::autofix()
{	return title.trim()
		| version.trim()
		| subtitle.trim()
		| artist.trim()
		| album_artist.trim()
		| album.trim()
		| disc_subtitle.trim()
		| cmpassign(disc_number, et_disc_number_to_string(disc_number))
		| cmpassign(disc_total, et_disc_number_to_string(disc_total))
		| year.trim()
		| release_year.trim()
		| cmpassign(track, et_track_number_to_string(track))
		| cmpassign(track_total, et_track_number_to_string(track_total))
		| genre.trim()
		| comment.trim()
		| description.trim()
		| composer.trim()
		| orig_artist.trim()
		| orig_year.trim()
		| copyright.trim()
		| url.trim()
		| encoded_by.trim();
}
