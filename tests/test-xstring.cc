/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024 Marcel Müller
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

#include "xstring.h"

#include <glib/gstdio.h>
using namespace std;


static void constructor()
{
	xString s1;
	g_assert_null(s1);
	g_assert_cmpuint(s1.use_count(), ==, 0);

	xString s1a(nullptr);
	g_assert_null(s1a);
	g_assert_cmpuint(s1a.use_count(), ==, 0);

	xString s2("");
	g_assert_cmpuint(s2.use_count(), >, 1);
	g_assert_cmpstr(s2, ==, "");

	const char* c2 = "";
	xString s2a(c2);
	g_assert_cmpuint(s2a.use_count(), >, 1);
	g_assert_cmpstr(s2a, ==, "");

	xString s3("xxx");
	g_assert_cmpuint(s3.use_count(), ==, 1);
	g_assert_cmpstr(s3, ==, "xxx");

	const char* c3 = "xxx";
	xString s3a("xxx");
	g_assert_cmpuint(s3a.use_count(), ==, 1);
	g_assert_cmpstr(s3a, ==, "xxx");

	{	xString s4(s3);
		g_assert_cmpuint(s3.use_count(), ==, 2);
		g_assert(s4 == s3);
	}

	g_assert_cmpuint(s3.use_count(), ==, 1);
}

static void assignment()
{
	xString s1, s2;
	s1 = "xxx";
	g_assert_cmpuint(s1.use_count(), ==, 1);
	g_assert_cmpstr(s1, ==, "xxx");

	s2 = s1;
	g_assert_cmpuint(s2.use_count(), ==, 2);
	g_assert(s2 == s1);

	s1 = s2;
	g_assert_cmpuint(s2.use_count(), ==, 2);
	g_assert_cmpstr(s2, ==, "xxx");

	s2.alloc(7)[0] = 'x';
	g_assert_cmpuint(s1.use_count(), ==, 1);
	g_assert_cmpuint(s2.get()[7], ==, 0);
	g_assert_cmpuint(s2.get()[0], ==, 'x');

	s2.reset();
	g_assert_null(s2);

	s2 = xString("xxx", 2);
	g_assert_cmpuint(s2.use_count(), ==, 1);
	g_assert_cmpstr(s2, ==, "xx");

	const char* c1 = "xxx";
	s2 = c1;
	g_assert_cmpuint(s2.use_count(), ==, 1);
	g_assert_cmpstr(s2, ==, "xxx");

	s2.deduplicate(s1);
	g_assert_cmpuint(s2.use_count(), ==, 2);
	g_assert_cmpstr(s2, ==, "xxx");
}

static void literal()
{
	const std::array<char, 4> a = to_array<4>("abc");
	g_assert_cmpstr(&a[0], ==, "abc");

	const auto c1 = xStringL("xxx");
	xString s1 = c1;
	g_assert_cmpuint(s1.use_count(), ==, 2);
	g_assert_cmpstr(s1, ==, "xxx");
}

static void equality()
{
	xString s1;
	g_assert_false(s1.equals("abc"));
	g_assert_false(s1.equals("abcd", 3));
	g_assert_false(s1.equals(nullptr, 0));
	g_assert_false(s1.equals(""));
	g_assert_true(s1.equals(nullptr));
	g_assert_false(s1.equals(""s));
	g_assert_false(s1.equals("abc"s));

	s1 = "abc";
	g_assert_true(s1.equals("abc"));
	g_assert_false(s1.equals("abcd"));
	g_assert_true(s1.equals("abcd", 3));
	g_assert_false(s1.equals("abc", 2));
	g_assert_false(s1.equals(""));
	g_assert_false(s1.equals(nullptr));
	g_assert_false(s1.equals(""s));
	g_assert_true(s1.equals("abc"s));
	g_assert_false(s1.equals("abc\0"s));

	s1 = "";
	g_assert_false(s1.equals("abc"));
	g_assert_false(s1.equals("abcd", 3));
	g_assert_true(s1.equals(nullptr, 0));
	g_assert_true(s1.equals(""));
	g_assert_false(s1.equals(nullptr));
	g_assert_true(s1.equals(""s));
	g_assert_false(s1.equals("abc"s));
}

static void equality0()
{
	xString0 s1;
	g_assert_false(s1.equals("abc"));
	g_assert_false(s1.equals("abcd", 3));
	g_assert_true(s1.equals(nullptr, 0));
	g_assert_true(s1.equals(""));
	g_assert_true(s1.equals(nullptr));
	g_assert_true(s1.equals(""s));
	g_assert_false(s1.equals("abc"s));

	s1 = "abc";
	g_assert_true(s1.equals("abc"));
	g_assert_false(s1.equals("abcd"));
	g_assert_true(s1.equals("abcd", 3));
	g_assert_false(s1.equals("abc", 2));
	g_assert_false(s1.equals(""));
	g_assert_false(s1.equals(nullptr));
	g_assert_false(s1.equals(""s));
	g_assert_true(s1.equals("abc"s));
	g_assert_false(s1.equals("abc\0"s));

	s1 = "";
	g_assert_false(s1.equals("abc"));
	g_assert_false(s1.equals("abcd", 3));
	g_assert_true(s1.equals(nullptr, 0));
	g_assert_true(s1.equals(""));
	g_assert_true(s1.equals(nullptr));
	g_assert_true(s1.equals(""s));
	g_assert_false(s1.equals("abc"s));
}

int main(int argc, char** argv)
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/xstring/constructor", constructor);
	g_test_add_func ("/xstring/assignment", assignment);
	g_test_add_func ("/xstring/literal", literal);
	g_test_add_func ("/xstring/equality", equality);
	g_test_add_func ("/xstring/equality0", equality0);

	return g_test_run ();
}
