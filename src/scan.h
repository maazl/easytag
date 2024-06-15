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

#ifndef ET_SCAN_H_
#define ET_SCAN_H_

#include <glib.h>
#include <string>

/// Function to replace underscore '_' by a space.
void Scan_Convert_Underscore_Into_Space(std::string& str);
/// Function to replace %20 by a space. No need to reallocate.
void Scan_Convert_P20_Into_Space(std::string& str);
/// Function to replace space by '_'. No need to reallocate.
void Scan_Convert_Space_Into_Underscore(std::string& str);
/// Remove all spaces from string.
void Scan_Process_Fields_Remove_Space(std::string& str);
/// This function will insert space before every uppercase character.
void Scan_Process_Fields_Insert_Space(std::string& str);
/// The function removes the duplicated spaces and uderscores.
void Scan_Process_Fields_Keep_One_Space(std::string& str);
/// Convert to upper
void Scan_Process_Fields_All_Uppercase(std::string& str);
/// Convert to lower
void Scan_Process_Fields_All_Downcase(std::string& str);
/// Convert first letter to uppercase, anything else to lowercase
void Scan_Process_Fields_Letter_Uppercase(std::string& str);
/// Function to set the first letter of each word to uppercase,
/// according the "Chicago Manual of Style" (http://www.docstyles.com/cmscrib.htm#Note2)
/// @param str
/// @param uppercase_preps
/// @param handle_roman
void Scan_Process_Fields_First_Letters_Uppercase(std::string& str, gboolean uppercase_preps, gboolean handle_roman);

#endif /* !ET_SCAN_H_ */
