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

#ifndef ET_FILE_TAG_H_
#define ET_FILE_TAG_H_

#include <glib.h>

#include "misc.h"
#include "picture.h"
#include "xstring.h"
#include "undo_list.h"

#include <string>
#include <ctime>
#include <vector>

class File_Name;

/** Metadata of a file.
 * @remarks All text fields contain <b>UTF-8 encoded NFC normalized data.</b>
 * You must ensure this when assigning new data. Using \c xString::resetNFC is recommended.
 */
struct File_Tag : public UndoList<File_Tag>::Intrusive
{
	xString0 title;         ///< Track name
	xString0 subtitle;      ///< Track subtitle
	xString0 version;       ///< Track version
	xString0 artist;        ///< Track artist
	xString0 album_artist;  ///< Album artist
	xString0 album;         ///< Album name
	xString0 disc_subtitle; ///< Medium title
	xString0 disc_number;   ///< Medium number within a set (as a string)
	xString0 disc_total;    ///< Total number of media in the set (as a string)
	xString0 year;          ///< Year, optionally with date (as a string)
	xString0 release_year;  ///< Release year, optionally with date (as a string)
	xString0 track;         ///< Track number within the medium (as a string)
	xString0 track_total;   ///< Total number of tracks of the medium (as a string)
	xString0 genre;         ///< Text genre
	xString0 comment;       ///< Comment, multi-line in general
	xString0 composer;      ///< Composer
	xString0 orig_artist;   ///< Original artist of the track
	xString0 orig_year;     ///< Original year of the track
	xString0 copyright;     ///< Copyright note
	xString0 url;           ///< URL
	xString0 encoded_by;    ///< Encoded by (strictly, a person, but often the encoding application)
	xString0 description;   ///< Detailed description of the track, multi-line
	std::vector<EtPicture> pictures; ///< List of pictures
	GList *other;           ///< List of other tags, used for Vorbis comments
	float track_gain;       ///< Replay gain of the track in dB that should be applied during playback
	float track_peak;       ///< Peak level of the track relative to 0dB FSR
	float album_gain;       ///< Replay gain of the album or entire set in dB that should be applied during playback
	float album_peak;       ///< Peak level of the album or entire set relative to 0dB FSR

	/// Consider ReplayGain changes of less than that as insignificant.
	static constexpr float gain_epsilon = .05f;
	/// Consider ReplayGain peak changes of less than that as insignificant.
	static constexpr float peak_epsilon = .005f;

	/// Create empty tag
	File_Tag();
	/// Clone tag
	File_Tag(const File_Tag& r);
	~File_Tag();

	bool empty() const;    ///< check whether this instance contains no data

	/// Compares two File_Tag items and returns \c true if they aren't the same.
	friend bool operator!=(const File_Tag& l, const File_Tag& r);
	/// Compares two File_Tag items and returns \c true if they are the same.
	friend bool operator==(const File_Tag& l, const File_Tag& r) { return !(l != r); }

	/// Parse ISO date time
	struct time : tm
	{ int field_count; ///< Number of fields parsed, <= 0 in case of an error
		bool invalid;
		time() : tm{ 0, 0, 0, 1, 0, 0 }, field_count(0), invalid(false) {}
	};
	static time parse_datetime(const char*& value);
	/// Check whether time stamp valid for the current format.
	/// @param value Tag value
	/// @param max_fields Maximum number of fields, i.e. 1111-22-33T44:55:66.
	/// @param additional_content Allow arbitrary additional content after the last field.
	static bool check_date(const char* value, int max_fields, bool additional_content);
	/// Check all time stamp for the current format and create a warning if not.
	/// @param max_fields Maximum number of fields, i.e. 1111-22-33T44:55:66.
	/// @param additional_content Allow arbitrary additional content after the last field.
	void check_dates(int max_fields, bool additional_content, const File_Name& filename) const;

	std::string track_and_total() const;
	std::string disc_and_total() const;
	void track_and_total(const char* value);
	void disc_and_total(const char* value);

	// locale invariant format for ReplayGain values
	static std::string format_float(const char* fmt, float value);
	static float parse_float(const char* value);

	std::string track_gain_str() const { return format_float("%.1f dB", track_gain); }
	std::string track_peak_str() const { return format_float("%.2f", track_peak); }
	std::string album_gain_str() const { return format_float("%.1f dB", album_gain); }
	std::string album_peak_str() const { return format_float("%.2f", album_peak); }
	void track_gain_str(const char* value) { track_gain = parse_float(value); }
	void track_peak_str(const char* value) { track_peak = parse_float(value); }
	void album_gain_str(const char* value) { album_gain = parse_float(value); }
	void album_peak_str(const char* value) { album_peak = parse_float(value); }

	/// Apply automatic corrections
	/// @return true if the operation made at least one change.
	bool autofix();
};

#endif /* !ET_FILE_TAG_H_ */
