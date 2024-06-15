/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024  Marcel Müller
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
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

#include "scan.h"
#include "misc.h"

#include <string.h>

#include <algorithm>
using namespace std;

void Scan_Convert_Underscore_Into_Space(string& str)
{	for (char& c : str)
		if (c == '_')
			c = ' ';
}

void Scan_Convert_P20_Into_Space(string& str)
{	size_t p = string::npos;
	while ((p = str.rfind("%20", p)) != string::npos)
		str.replace(p, 3, 1, ' ');
}

void Scan_Convert_Space_Into_Underscore(string& str)
{	for (char& c : str)
		if (c == ' ')
			c = '_';
}

void Scan_Process_Fields_Remove_Space(string& str)
{	size_t p = string::npos;
	while ((p = str.rfind(' ', p)) != string::npos)
		str.erase(p, 1);
}

void Scan_Process_Fields_Insert_Space(string& str)
{	if (str.length() < 2)
		return;

	gunichar last = 0;
	for (const gchar* iter = str.c_str() + 1; *iter; iter = g_utf8_next_char(iter))
	{	gunichar c = g_utf8_get_char(iter);
		if (g_unichar_isupper(c) && !g_unichar_isspace(last))
		{	size_t p = iter - str.c_str();
			str.insert(p, 1, ' ');
			iter = str.c_str() + p + 1;
		}
		last = c;
	}
}

/*
 * The function removes the duplicated spaces. No need to reallocate.
 */
void Scan_Process_Fields_Keep_One_Space(string& str)
{	size_t p = string::npos;
	while ((p = str.find_last_of(" _", p, 2)) != string::npos && p)
	{	size_t q = p - 1;
		p = str.find_last_not_of(" _", q, 2); // turns npos into 0
		str.erase(p + 2, q - p);
	}
}

void Scan_Process_Fields_All_Uppercase(string& str)
{	str = gString(g_utf8_strup(str.c_str(), str.size()));
}

void Scan_Process_Fields_All_Downcase(string& str)
{	str = gString(g_utf8_strdown(str.c_str(), str.size()));
}

void Scan_Process_Fields_Letter_Uppercase(string& str)
{
	string string1;
	string1.reserve(str.length());

	for (const gchar* temp = str.c_str(); *temp; temp = g_utf8_next_char (temp))
	{
		gunichar c = g_utf8_get_char(temp);
		gchar temp2[6];

		// After the first time, all will be lower case.
		if (temp == str.c_str())
		{	if (g_unichar_islower(c))
			{	string1.append(temp2, g_unichar_to_utf8(g_unichar_toupper(c), temp2));
				continue;
			}
		}
		// Uppercase the word 'I' in english
		else if (c == 'I'
			&& (temp[-1] == ' ' || temp[-1] == '_')
			&& (temp[1] == ' ' || temp[1] == '_'))
		{	string1 += 'I';
			continue;
		}
		else if (g_unichar_isupper(c))
		{	string1.append(temp2, g_unichar_to_utf8(g_unichar_tolower(c), temp2));
			continue;
		}
		string1.append(temp, (const char*)g_utf8_next_char(temp));
	}

	str.swap(string1);
}

static gint
Scan_Word_Is_Roman_Numeral (const gchar *text)
{
    /* No need for caseless strchr. */
    static const gchar romans[] = "MmDdCcLlXxVvIi";

    gsize next_allowed = 0;
    gsize prev = 0;
    gsize count = 0;
    const gchar *i;

    for (i = text; *i; i++)
    {
        const char *s = strchr (romans, *i);

        if (s)
        {
            gsize c = (s - romans) / 2;

            if (c < next_allowed)
            {
                return 0;
            }

            if (c < prev)
            {
                /* After subtraction, no more subtracted chars allowed. */
                next_allowed = prev + 1;
            }
            else if (c == prev)
            {
                /* Allow indefinite repetition for m; three for c, x and i; and
                 * none for d, l and v. */
                if ((c && ++count > 3) || (c & 1))
                {
                    return 0;
                }

                /* No more subtraction. */
                next_allowed = c;
            }
            else if (c && !(c & 1))
            {
                /* For first occurrence of c, x and i, allow "subtraction" from
                 * 10 and 5 times self, reset counting. */
                next_allowed = c - 2;
                count = 1;
            }

            prev = c;
        }
        else
        {
            if (g_unichar_isalnum (g_utf8_get_char (i)))
            {
                return 0;
            }

            break;
        }
    }

    /* Return length of found Roman numeral. */
    return i - text;
}

void Scan_Process_Fields_First_Letters_Uppercase
(string& str2, gboolean uppercase_preps, gboolean handle_roman)
{
    Scan_Process_Fields_All_Downcase(str2);
    // code not ported to std::string because of inscrutable in place manipulation.
    gchar* str = g_strdup(str2.c_str());
/**** DANIEL TEST *****
    gchar *iter;
    gchar utf8_character[6];
    gboolean set_to_upper_case = TRUE;
    gunichar c;

    for (iter = text; *iter; iter = g_utf8_next_char(iter))
    {
        c = g_utf8_get_char(iter);
        if (set_to_upper_case && g_unichar_islower(c))
            strncpy(iter, utf8_character, g_unichar_to_utf8(g_unichar_toupper(c), utf8_character));
        else if (!set_to_upper_case && g_unichar_isupper(c))
            strncpy(iter, utf8_character, g_unichar_to_utf8(g_unichar_tolower(c), utf8_character));

        set_to_upper_case = (g_unichar_isalpha(c)
                            || c == (gunichar)'.'
                            || c == (gunichar)'\''
                            || c == (gunichar)'`') ? FALSE : TRUE;
    }
****/
/**** Barış Çiçek version ****/

    gchar *string = str;
    gchar *word, *word1, *word2, *temp;
    gint i, len;
    gchar utf8_character[6];
    gunichar c;
    gboolean set_to_upper_case, set_to_upper_case_tmp;
    // There have to be space at the end of words to seperate them from prefix
    // Chicago Manual of Style "Heading caps" Capitalization Rules (CMS 1993, 282) (http://www.docstyles.com/cmscrib.htm#Note2)
    const gchar * exempt[] =
    {
        "a ",       "a_",
        "against ", "against_",
        "an ",      "an_",
        "and ",     "and_",
        "at ",      "at_",
        "between ", "between_",
        "but ",     "but_",
        "feat. ",   "feat._",
        "for ",     "for_",
        "in ",      "in_",
        "nor ",     "nor_",
        "of ",      "of_",
        //"off ",     "off_",   // Removed by Slash Bunny
        "on ",      "on_",
        "or ",      "or_",
        //"over ",    "over_",  // Removed by Slash Bunny
        "so ",      "so_",
        "the ",     "the_",
        "to ",      "to_",
        "with ",    "with_",
        "yet ",     "yet_",
        NULL
    };

    if (!g_utf8_validate(string,-1,NULL))
    {
        /* FIXME: Translatable string. */
        g_warning ("%s",
                   "Scan_Process_Fields_First_Letters_Uppercase: Not valid UTF-8!");
        return;
    }
    /* Removes trailing whitespace. */
    string = g_strchomp(string);

    temp = string;

    /* If the word is a roman numeral, capitalize all of it. */
    if (handle_roman && (len = Scan_Word_Is_Roman_Numeral (temp)))
    {
        gchar *tmp = g_utf8_strup (temp, len);
        strncpy (string, tmp, len);
        g_free (tmp);
    }
    else
    {
        // Set first character to uppercase
        c = g_utf8_get_char(temp);
        strncpy(string, utf8_character, g_unichar_to_utf8(g_unichar_toupper(c), utf8_character));
    }

    // Uppercase first character of each word, except for 'exempt[]' words lists
    while ( temp )
    {
        word = temp; // Needed if there is only one word
        word1 = strchr (temp, ' ');
        word2 = strchr (temp, '_');

        // Take the first string found (near beginning of string)
        if (word1 && word2)
            word = MIN(word1,word2);
        else if (word1)
            word = word1;
        else if (word2)
            word = word2;
        else
        {
            // Last word of the string: the first letter is always uppercase,
            // even if it's in the exempt list. This is a Chicago Manual of Style rule.
            // Last Word In String - Should Capitalize Regardless of Word (Chicago Manual of Style)
            c = g_utf8_get_char(word);
            strncpy(word, utf8_character, g_unichar_to_utf8(g_unichar_toupper(c), utf8_character));
            break;
        }

        // Go to first character of the word (char. after ' ' or '_')
        word = word+1;

        // If the word is a roman numeral, capitalize all of it
        if (handle_roman && (len = Scan_Word_Is_Roman_Numeral (word)))
        {
            gchar *tmp = g_utf8_strup (word, len);
            strncpy (word, tmp, len);
            g_free (tmp);
        }
        else
        {
            // Set uppercase the first character of this word
            c = g_utf8_get_char(word);
            strncpy(word, utf8_character, g_unichar_to_utf8(g_unichar_toupper(c), utf8_character));

            if (uppercase_preps)
            {
                goto increment;
            }

            /* Lowercase the first character of this word if found in the
             * exempt words list. */
            for (i=0; exempt[i]!=NULL; i++)
            {
                if (g_ascii_strncasecmp(exempt[i], word, strlen(exempt[i])) == 0)
                {
                    c = g_utf8_get_char(word);
                    strncpy(word, utf8_character, g_unichar_to_utf8(g_unichar_tolower(c), utf8_character));
                    break;
                }
            }
        }

increment:
        temp = word;
    }

    // Uppercase letter placed after some characters like '(', '[', '{'
    set_to_upper_case = FALSE;
    for (temp = string; *temp; temp = g_utf8_next_char(temp))
    {
        c = g_utf8_get_char(temp);
        set_to_upper_case_tmp = (  c == (gunichar)'('
                                || c == (gunichar)'['
                                || c == (gunichar)'{'
                                || c == (gunichar)'"'
                                || c == (gunichar)':'
                                || c == (gunichar)'.'
                                || c == (gunichar)'`'
                                || c == (gunichar)'-'
                                ) ? TRUE : FALSE;

        if (set_to_upper_case && g_unichar_islower(c))
            strncpy(temp, utf8_character, g_unichar_to_utf8(g_unichar_toupper(c), utf8_character));

        set_to_upper_case = set_to_upper_case_tmp;
    }

    str2 = str;
    g_free(str);
}
