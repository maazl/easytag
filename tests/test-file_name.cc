/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022 Marcel Müller
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

#include "file_name.h"

#include <glib/gstdio.h>

using namespace std;

static void
file_name_prepare_func(void)
{
    gsize i;

    static const struct
    {
        const gchar *filename;
        const gchar *result_replace;
        const gchar *result;
    } filenames[] =
    {
        { "foo:bar", "foo-bar", "foo:bar" },
        { "foo" G_DIR_SEPARATOR_S "bar", "foo-bar", "foo-bar" },
        { "foo*bar", "foo+bar", "foo*bar" },
        { "foo?bar", "foo_bar", "foo?bar" },
        { "foo\"bar", "foo\'bar", "foo\"bar" },
        { "foo<bar", "foo(bar", "foo<bar" },
        { "foo>bar", "foo)bar", "foo>bar" },
        { "foo|bar", "foo-bar", "foo|bar" },
        { "foo|bar*baz", "foo-bar+baz", "foo|bar*baz" },
        { "foo.", "foo_", "foo." },
        { "foo ", "foo_", "foo " }
        /* TODO: Add more tests. */
    };

    auto replace_func = File_Name::prepare_func(ET_FILENAME_REPLACE_ASCII, ET_CONVERT_SPACES_NO_CHANGE);
    auto no_replace_func = File_Name::prepare_func(ET_FILENAME_REPLACE_NONE, ET_CONVERT_SPACES_NO_CHANGE);

    for (i = 0; i < G_N_ELEMENTS (filenames); i++)
    {
        string filename = filenames[i].filename;

        /* Replace illegal characters. */
        replace_func(filename, 0);
        g_assert_cmpstr (filename.c_str(), ==, filenames[i].result_replace);

        filename = filenames[i].filename;

        /* Leave illegal characters unchanged. */
        no_replace_func(filename, 0);
        g_assert_cmpstr (filename.c_str(), ==, filenames[i].result);
    }
}

int
main (int argc, char** argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/file-name/prepare-func", file_name_prepare_func);

    return g_test_run ();
}
