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

#include "file_tag.h"

#include "misc.h"

using namespace std;

/*
 * Create a new File_Tag structure.
 */
File_Tag *
et_file_tag_new (void)
{
    File_Tag *file_tag;

    file_tag = g_slice_new0 (File_Tag);
    file_tag->key = et_undo_key_new ();

    return file_tag;
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


/*
 * Frees a File_Tag item.
 */
void
et_file_tag_free (File_Tag *FileTag)
{
    g_return_if_fail (FileTag != NULL);

    g_free(FileTag->title);
    g_free(FileTag->version);
    g_free(FileTag->subtitle);
    g_free(FileTag->artist);
    g_free(FileTag->album_artist);
    g_free(FileTag->album);
    g_free(FileTag->disc_subtitle);
    g_free(FileTag->disc_number);
    g_free(FileTag->disc_total);
    g_free(FileTag->year);
    g_free(FileTag->release_year);
    g_free(FileTag->track);
    g_free(FileTag->track_total);
    g_free(FileTag->genre);
    g_free(FileTag->comment);
    g_free(FileTag->composer);
    g_free(FileTag->orig_artist);
    g_free(FileTag->orig_year);
    g_free(FileTag->copyright);
    g_free(FileTag->url);
    g_free(FileTag->encoded_by);
    g_free(FileTag->description);
    et_file_tag_set_picture (FileTag, NULL);
    et_file_tag_free_other_field (FileTag);

    g_slice_free (File_Tag, FileTag);
}

/*
 * Copy data of the File_Tag structure (of ETFile) to the FileTag item.
 * Reallocate data if not null.
 */
File_Tag* File_Tag::clone() const
{
	File_Tag* destination = et_file_tag_new();

	/* Key for the item, may be overwritten. */
	destination->key = et_undo_key_new();

	et_file_tag_set_title(destination, title);
	et_file_tag_set_version(destination, version);
	et_file_tag_set_subtitle(destination, subtitle);
	et_file_tag_set_artist(destination, artist);
	et_file_tag_set_album_artist(destination, album_artist);
	et_file_tag_set_album(destination, album);
	et_file_tag_set_disc_subtitle(destination, disc_subtitle);
	et_file_tag_set_disc_number(destination, disc_number);
	et_file_tag_set_disc_total(destination, disc_total);
	et_file_tag_set_year(destination, year);
	et_file_tag_set_release_year(destination, release_year);
	et_file_tag_set_track_number(destination, track);
	et_file_tag_set_track_total(destination, track_total);
	et_file_tag_set_genre(destination, genre);
	et_file_tag_set_comment(destination, comment);
	et_file_tag_set_composer(destination, composer);
	et_file_tag_set_orig_artist(destination, orig_artist);
	et_file_tag_set_orig_year(destination, orig_year);
	et_file_tag_set_copyright(destination, copyright);
	et_file_tag_set_url(destination, url);
	et_file_tag_set_encoded_by(destination, encoded_by);
	et_file_tag_set_description(destination, description);
	et_file_tag_set_picture(destination, picture);

	GList *new_other = NULL;
	for (GList* l = other; l != NULL; l = g_list_next(l))
		new_other = g_list_prepend(new_other, g_strdup((gchar*)l->data));
	destination->other = g_list_reverse(new_other);

	return destination;
}

void et_file_tag_set_title(File_Tag *file_tag, const gchar *title)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::title, title);
}

void et_file_tag_set_version(File_Tag *file_tag, const gchar *version)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::version, version);
}

void et_file_tag_set_subtitle(File_Tag *file_tag, const gchar *subtitle)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::subtitle, subtitle);
}

void et_file_tag_set_artist(File_Tag *file_tag, const gchar *artist)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::artist, artist);
}

void et_file_tag_set_album_artist(File_Tag *file_tag, const gchar *album_artist)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::album_artist, album_artist);
}

void et_file_tag_set_album(File_Tag *file_tag, const gchar *album)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::album, album);
}

void et_file_tag_set_disc_subtitle(File_Tag *file_tag, const gchar *disc_subtitle)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::disc_subtitle, disc_subtitle);
}

void et_file_tag_set_disc_number(File_Tag *file_tag, const gchar *disc_number)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::disc_number, disc_number);
}

void et_file_tag_set_disc_total(File_Tag *file_tag, const gchar *disc_total)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::disc_total, disc_total);
}

void et_file_tag_set_year(File_Tag *file_tag, const gchar *year)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::year, year);
}

void et_file_tag_set_release_year(File_Tag *file_tag, const gchar *release_year)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::release_year, release_year);
}

void et_file_tag_set_track_number(File_Tag *file_tag, const gchar *track_number)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::track, track_number);
}

void et_file_tag_set_track_total(File_Tag *file_tag, const gchar *track_total)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::track_total, track_total);
}

void et_file_tag_set_genre(File_Tag *file_tag, const gchar *genre)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::genre, genre);
}

void et_file_tag_set_comment(File_Tag *file_tag, const gchar *comment)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::comment, comment);
}

void et_file_tag_set_composer(File_Tag *file_tag, const gchar *composer)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::composer, composer);
}

void et_file_tag_set_orig_artist(File_Tag *file_tag, const gchar *orig_artist)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::orig_artist, orig_artist);
}

void et_file_tag_set_orig_year(File_Tag *file_tag, const gchar *orig_year)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::orig_year, orig_year);
}

void et_file_tag_set_copyright(File_Tag *file_tag, const gchar *copyright)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::copyright, copyright);
}

void et_file_tag_set_url(File_Tag *file_tag, const gchar *url)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::url, url);
}

void et_file_tag_set_encoded_by(File_Tag *file_tag, const gchar *encoded_by)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::encoded_by, encoded_by);
}

void et_file_tag_set_description(File_Tag *file_tag, const gchar *description)
{	g_return_if_fail (file_tag != NULL);
	file_tag->set_field(&File_Tag::description, description);
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

/*
 * Compares two File_Tag items and returns TRUE if there aren't the same.
 * Notes:
 *  - if field is '' or NULL => will be removed
 */
gboolean
et_file_tag_detect_difference (const File_Tag *FileTag1,
                               const File_Tag *FileTag2)
{
    const EtPicture *pic1;
    const EtPicture *pic2;

    g_return_val_if_fail (FileTag1 != NULL && FileTag2 != NULL, FALSE);

    if ( ( FileTag1 && !FileTag2)
      || (!FileTag1 &&  FileTag2) )
        return TRUE;

    if (et_normalized_strcmp0 (FileTag1->title, FileTag2->title) != 0
        || et_normalized_strcmp0 (FileTag1->version, FileTag2->version) != 0
        || et_normalized_strcmp0 (FileTag1->subtitle, FileTag2->subtitle) != 0
        || et_normalized_strcmp0 (FileTag1->artist, FileTag2->artist) != 0
        || et_normalized_strcmp0 (FileTag1->album_artist, FileTag2->album_artist) != 0
        || et_normalized_strcmp0 (FileTag1->album, FileTag2->album) != 0
        || et_normalized_strcmp0 (FileTag1->disc_subtitle, FileTag2->disc_subtitle) != 0
        || et_normalized_strcmp0 (FileTag1->disc_number, FileTag2->disc_number) != 0
        || et_normalized_strcmp0 (FileTag1->disc_total, FileTag2->disc_total) != 0
        || et_normalized_strcmp0 (FileTag1->year, FileTag2->year) != 0
        || et_normalized_strcmp0 (FileTag1->release_year, FileTag2->release_year) != 0
        || et_normalized_strcmp0 (FileTag1->track, FileTag2->track) != 0
        || et_normalized_strcmp0 (FileTag1->track_total, FileTag2->track_total) != 0
        || et_normalized_strcmp0 (FileTag1->genre, FileTag2->genre) != 0
        || et_normalized_strcmp0 (FileTag1->comment, FileTag2->comment) != 0
        || et_normalized_strcmp0 (FileTag1->composer, FileTag2->composer) != 0
        || et_normalized_strcmp0 (FileTag1->orig_year, FileTag2->orig_year) != 0
        || et_normalized_strcmp0 (FileTag1->orig_artist, FileTag2->orig_artist) != 0
        || et_normalized_strcmp0 (FileTag1->copyright, FileTag2->copyright) != 0
        || et_normalized_strcmp0 (FileTag1->url, FileTag2->url) != 0
        || et_normalized_strcmp0 (FileTag1->encoded_by, FileTag2->encoded_by) != 0
        || et_normalized_strcmp0 (FileTag1->description, FileTag2->description) != 0)
        return TRUE;

    /* Picture */
    for (pic1 = FileTag1->picture, pic2 = FileTag2->picture;
         pic1 || pic2;
         pic1 = pic1->next, pic2 = pic2->next)
        if (et_picture_detect_difference (pic1, pic2))
            return TRUE;

    return FALSE; /* No changes */
}

std::string File_Tag::track_and_total() const
{	if (et_str_empty(track))
		return std::string();
	if (et_str_empty(track_total))
		return track;
	return std::string(track) + '/' + track_total;
}

std::string File_Tag::disc_and_total() const
{	if (et_str_empty(disc_number))
		return std::string();
	if (et_str_empty(disc_total))
		return disc_number;
	return std::string(disc_number) + '/' + disc_total;
}
