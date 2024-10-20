/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022 Marcel Müller <gitlab@maazl.de>
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

#ifndef ET_MASK_H_
#define ET_MASK_H_

#include <gtk/gtk.h>

#include "file.h"
#include "file_tag.h"

/// Get tag field by mask character
xStringD0 File_Tag::* et_mask_field(char code);

/**
 * Check mask string for syntactical correctness.
 * @param mask
 * @return empty: valid, error message otherwise.
 */
std::string et_check_mask(const gchar* mask);

/**
 * Apply the mask to a file and calculate new file name.
 * @param file
 * @param mask
 * @param no_dir_check_or_conversion Skip replacement of invalid file characters.
 * @return New file name or empty if the mask did not evaluate because off missing tag field value(s).
 */
std::string et_evaluate_mask(const ET_File *file, const gchar *mask, gboolean no_dir_check_or_conversion);

/**
 * Display an icon in the entry if the current text contains an invalid mask.
 * @param entry The entry for which to check the mask
 * @param user_data User data set when the signal was connected
 */
void entry_check_mask(GtkEntry *entry, gpointer user_data);

#endif
