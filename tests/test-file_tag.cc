/* EasyTAG - tag editor for audio files
 * Copyright (C) 2015-2016 David King <amigadave@amigadave.com>
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

#include "file_tag.h"

#include "misc.h"
#include "picture.h"

GtkWidget *MainWindow;
GSettings *MainSettings;

static void
file_tag_copy (void)
{
    File_Tag *tag1;
    File_Tag *tag2;

    tag1 = new File_Tag();

    g_assert (tag1);

    tag1->title = "foo";
    tag1->artist = "bar";
    tag1->album_artist = "baz";

    g_assert_cmpstr (tag1->title, ==, "foo");
    g_assert_cmpstr (tag1->artist, ==, "bar");
    g_assert_cmpstr (tag1->album_artist, ==, "baz");

    tag2 = new File_Tag(*tag1);

    g_assert (tag2);

    g_assert_cmpstr (tag2->title, ==, "foo");
    g_assert_cmpstr (tag2->artist, ==, "bar");
    g_assert_cmpstr (tag2->album_artist, ==, "baz");

    delete tag2;
    delete tag1;
}

static void
file_tag_difference (void)
{
    File_Tag *tag1;
    File_Tag *tag2;

    tag1 = new File_Tag();

    tag1->title = "foo：";

    /* Contains a full-width colon, which should compare differently to a
     * colon. */
    g_assert_cmpstr (tag1->title, ==, "foo：");

    tag2 = new File_Tag();

    tag2->title = "foo:";

    g_test_bug ("744897");
    g_assert (*tag1 != *tag2);

    delete tag2;
    delete tag1;

    tag1 = new File_Tag();

    tag1->artist = "bar";

    tag2 = new File_Tag();

    tag2->artist = "baz";

    g_assert (*tag1 != *tag2);

    delete tag2;
    delete tag1;

    tag1 = new File_Tag();
    tag1->pictures.emplace_back(ET_PICTURE_TYPE_FRONT_COVER, xStringD0(""), 0, 0, "foo", 3);

    tag2 = new File_Tag();

    g_assert (*tag1 != *tag2);

    delete tag2;
    delete tag1;
}

int
main (int argc, char** argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

    g_test_add_func ("/file_tag/copy", file_tag_copy);
    g_test_add_func ("/file_tag/difference", file_tag_difference);

    return g_test_run ();
}
