/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014-2015 David King <amigadave@amigadave.com>
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
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "picture.h"

#include <gtk/gtk.h>
#include <string.h>
#include <memory>
using namespace std;

GtkWidget *MainWindow;
GSettings *MainSettings;

const auto foobar(xStringL("foobar.png"));
const auto baz(xStringL("baz.gif"));

static void
picture_copy (void)
{
    unique_ptr<EtPicture> pic1(new EtPicture(ET_PICTURE_TYPE_LEAFLET_PAGE, foobar, 640, 480, "foobar", 6));
    unique_ptr<EtPicture> pic2(new EtPicture(*pic1));
    g_assert(*pic1 == *pic2);
    g_assert(pic1->storage == pic2->storage);
}

static void
picture_difference (void)
{
    unique_ptr<EtPicture> pic1;
    unique_ptr<EtPicture> pic2;

    pic1.reset(new EtPicture(ET_PICTURE_TYPE_LEAFLET_PAGE, foobar, 640, 480, "foobar", 6));

    pic2.reset(new EtPicture(ET_PICTURE_TYPE_ILLUSTRATION, foobar, 640, 480, pic1->storage->Bytes, pic1->storage->Size));
    g_assert(*pic1 != *pic2);
    g_assert(pic1->storage == pic2->storage);

    pic2.reset(new EtPicture(ET_PICTURE_TYPE_LEAFLET_PAGE, baz, 640, 480, pic1->storage->Bytes, pic1->storage->Size));
    g_assert(*pic1 != *pic2);

    pic2.reset(new EtPicture(ET_PICTURE_TYPE_LEAFLET_PAGE, foobar, 0, 0, pic1->storage->Bytes, pic1->storage->Size));
    g_assert(*pic1 == *pic2); // deduplication reconstructs width/height

    pic2.reset(new EtPicture(ET_PICTURE_TYPE_LEAFLET_PAGE, foobar, 640, 480, "baz", 3));
    g_assert(*pic1 != *pic2);
}

static void
picture_type_from_filename (void)
{
    static const struct
    {
        const gchar *filename;
        EtPictureType type;
    } pictures[] = 
    {
        { "no clues here", ET_PICTURE_TYPE_FRONT_COVER },
        { "cover.jpg", ET_PICTURE_TYPE_FRONT_COVER },
        { "inside cover.png", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "acdc", ET_PICTURE_TYPE_MEDIA },
        { "ACDC", ET_PICTURE_TYPE_MEDIA },
        { "aCdC", ET_PICTURE_TYPE_MEDIA },
        { "aC dC", ET_PICTURE_TYPE_FRONT_COVER },
        { "back in black", ET_PICTURE_TYPE_BACK_COVER },
        { "illustrations of grandeur", ET_PICTURE_TYPE_ILLUSTRATION },
        { "inside outside", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "front to back", ET_PICTURE_TYPE_FRONT_COVER },
        { "back to front", ET_PICTURE_TYPE_FRONT_COVER },
        { "inlay", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "leaflet", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "page", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "multimedia", ET_PICTURE_TYPE_MEDIA },
        { "artist band", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "band", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "orchestra", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "performer", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "composer", ET_PICTURE_TYPE_COMPOSER },
        { "lyricist", ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER },
        { "writer", ET_PICTURE_TYPE_FRONT_COVER },
        { "publisher", ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE },
        { "studio", ET_PICTURE_TYPE_FRONT_COVER }
    };

    for (gsize i = 0; i < G_N_ELEMENTS (pictures); i++)
        g_assert_cmpint(pictures[i].type, ==, EtPicture::type_from_filename(pictures[i].filename));
}

static void
picture_format_from_data (void)
{
    static const struct
    {
        const gchar *data;
        Picture_Format format;
    } pictures[] =
    {
        { "\xff\xd8", PICTURE_FORMAT_UNKNOWN },
        { "\xff\xd8\xff", PICTURE_FORMAT_JPEG },
        { "\x89PNG\x0d\x0a\x1a\x0a", PICTURE_FORMAT_PNG },
        { "GIF87a", PICTURE_FORMAT_GIF },
        { "GIF89a", PICTURE_FORMAT_GIF },
        { "GIF900", PICTURE_FORMAT_UNKNOWN }
    };

    unique_ptr<EtPicture> pic;
    for (gsize i = 0; i < G_N_ELEMENTS (pictures); i++)
    {
        pic.reset(new EtPicture(ET_PICTURE_TYPE_FRONT_COVER, xString::empty_str, 0, 0,
            pictures[i].data, strlen(pictures[i].data) + 1));
        g_assert_cmpint(pictures[i].format, ==, pic->Format());
    }
}

int
main (int argc, char** argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/picture/copy", picture_copy);
    g_test_add_func ("/picture/difference", picture_difference);
    g_test_add_func ("/picture/format-from-data", picture_format_from_data);
    g_test_add_func ("/picture/type-from-filename",
                     picture_type_from_filename);

    return g_test_run ();
}
