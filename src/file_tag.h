/* EasyTAG - tag editor for audio files
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

#ifndef ET_FILE_TAG_H_
#define ET_FILE_TAG_H_

#include <glib.h>

#include "misc.h"
#include "picture.h"

#ifdef __cplusplus
#include <string>
#include <ctime>
#endif

/*
 * File_Tag:
 * @key: incremented value
 * @saved: %TRUE if the tag has been saved, %FALSE otherwise
 * @title: track name
 * @artist: track artist
 * @album_artist: artist for the album of which this track is part
 * @album: album name
 * @disc_number: disc number within a set (as a string)
 * @disc_total: total number of discs in the set (as a string)
 * @year: year (as a string)
 * @track: track number (as a string)
 * @track_total: total number of tracks (as a string)
 * @genre: text genre
 * @comment: comment
 * @composer: composer
 * @orig_artist: original artist
 * @copyright: copyright
 * @url: URL
 * @encoded_by: encoded by (strictly, a person, but often the encoding
 *              application)
 * @picture: #EtPicture, which may have several other linked instances
 * @other: a list of other tags, used for Vorbis comments
 * Description of each item of the TagList list
 */
typedef struct File_Tag
{
    guint key;
    gboolean saved;

    gchar *title;
    gchar *subtitle;
    gchar *version;
    gchar *artist;
    gchar *album_artist;
    gchar *album;
    gchar *disc_subtitle;
    gchar *disc_number;
    gchar *disc_total;
    gchar *year;
    gchar *release_year;
    gchar *track;
    gchar *track_total;
    gchar *genre;
    gchar *comment;
    gchar *composer;
    gchar *orig_artist;
    gchar *orig_year;
    gchar *copyright;
    gchar *url;
    gchar *encoded_by;
    gchar *description;
    EtPicture *picture;
    GList *other;
    float track_gain;
    float track_peak;
    float album_gain;
    float album_peak;

#ifdef __cplusplus
    /// Consider ReplayGain changes of less than that as insignificant.
    static constexpr float gain_epsilon = .05f;
    /// Consider ReplayGain peak changes of less than that as insignificant.
    static constexpr float peak_epsilon = .005f;

    bool empty() const;

    File_Tag* clone() const;
    /// Set field value
    gchar* set_field(gchar* File_Tag::*field, const gchar* value)
    {	g_free(this->*field);
    	return this->*field = et_str_empty(value) ? nullptr : g_strdup(value);
    }
    /// Set field value and take ownership of value
    gchar* emplace_field(gchar* File_Tag::*field, gchar* value)
    {	g_free(this->*field);
    	if (!et_str_empty(value))
    		return this->*field = value;
    	g_free(value);
    	return this->*field = nullptr;
    }

    /// Parse ISO date time
    struct time : tm
    { int field_count; ///< Number of fields parsed, <= 0 in case of an error
      bool invalid;
      time() : tm{ 0, 0, 0, 1, 0, 0 }, field_count(0), invalid(false) {}
    };
    static time parse_datetime(const char* value);

    std::string track_and_total() const;
    std::string disc_and_total() const;
    void track_and_total(const char* value);
    void disc_and_total(const char* value);

    // locale invariant format for ReplayGain values
    static small_str<8> format_float(const char* fmt, float value);
    static float parse_float(const char* fmt);

    small_str<8> track_gain_str() const { return format_float("%.1f dB", track_gain); }
    small_str<8> track_peak_str() const { return format_float("%.2f", track_peak); }
    small_str<8> album_gain_str() const { return format_float("%.1f dB", album_gain); }
    small_str<8> album_peak_str() const { return format_float("%.2f", album_peak); }
    void track_gain_str(const char* value) { track_gain = parse_float(value); }
    void track_peak_str(const char* value) { track_peak = parse_float(value); }
    void album_gain_str(const char* value) { album_gain = parse_float(value); }
    void album_peak_str(const char* value) { album_peak = parse_float(value); }

#endif
} File_Tag;

G_BEGIN_DECLS

File_Tag * et_file_tag_new (void);
void et_file_tag_free (File_Tag *file_tag);

void et_file_tag_set_title (File_Tag *file_tag, const gchar *title);
void et_file_tag_set_version (File_Tag *file_tag, const gchar *version);
void et_file_tag_set_subtitle (File_Tag *file_tag, const gchar *subtitle);
void et_file_tag_set_artist (File_Tag *file_tag, const gchar *artist);
void et_file_tag_set_album_artist (File_Tag *file_tag, const gchar *album_artist);
void et_file_tag_set_album (File_Tag *file_tag, const gchar *album);
void et_file_tag_set_disc_subtitle (File_Tag *file_tag, const gchar *disc_subtitle);
void et_file_tag_set_disc_number (File_Tag *file_tag, const gchar *disc_number);
void et_file_tag_set_disc_total (File_Tag *file_tag, const gchar *disc_total);
void et_file_tag_set_year (File_Tag *file_tag, const gchar *year);
void et_file_tag_set_release_year (File_Tag *file_tag, const gchar *year);
void et_file_tag_set_track_number (File_Tag *file_tag, const gchar *track_number);
void et_file_tag_set_track_total (File_Tag *file_tag, const gchar *track_total);
void et_file_tag_set_genre (File_Tag *file_tag, const gchar *genre);
void et_file_tag_set_comment (File_Tag *file_tag, const gchar *comment);
void et_file_tag_set_composer (File_Tag *file_tag, const gchar *composer);
void et_file_tag_set_orig_artist (File_Tag *file_tag, const gchar *orig_artist);
void et_file_tag_set_orig_year (File_Tag *file_tag, const gchar *year);
void et_file_tag_set_copyright (File_Tag *file_tag, const gchar *copyright);
void et_file_tag_set_url (File_Tag *file_tag, const gchar *url);
void et_file_tag_set_encoded_by (File_Tag *file_tag, const gchar *encoded_by);
void et_file_tag_set_description (File_Tag *file_tag, const gchar *description);
void et_file_tag_set_picture (File_Tag *file_tag, const EtPicture *pic);
void et_file_tag_set_track_gain (File_Tag *file_tag, float gain, float peek);
void et_file_tag_set_album_gain (File_Tag *file_tag, float gain, float peek);

gboolean et_file_tag_detect_difference (const File_Tag *FileTag1, const File_Tag  *FileTag2);

G_END_DECLS

#endif /* !ET_FILE_TAG_H_ */
