/* EasyTAG - tag editor for audio files
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
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef ET_FILE_DESCRIPTION_H_
#define ET_FILE_DESCRIPTION_H_

#include <glib.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#include "setting.h"

struct ET_File;
struct File_Tag;

#include <string>

/*
 * EtFileHeaderFields:
 * @description: a description of the file type, such as MP3 File
 * @version_label: the label for the encoder version, such as MPEG
 * @version: the encoder version (such as 2, Layer III)
 * @bitrate: the bitrate of the file (not the bit depth of the samples)
 * @samplerate: the sample rate of the primary audio track, generally in Hz
 * @mode_label: the label for the audio mode, for example Mode
 * @mode: the audio mode (stereo, mono, and so on)
 * @size: the size of the audio file
 * @duration: the length of the primary audio track
 *
 * UI-visible strings, populated by the tagging support code to be displayed in
 * the EtFileArea.
 */
struct EtFileHeaderFields
{
    /*< public >*/
    std::string description;
    std::string version_label;
    std::string version;
    std::string bitrate;
    std::string samplerate;
    std::string mode_label;
    std::string mode;
    std::string size;
    std::string duration;
};

/*
 * Structure to register supported file types
 */
struct ET_File_Description
{
    /// Extension, e.g. ".mp3"
    /// @remarks This is currently like the primary key of an instance
    const char* Extension;
    const char* FileType; ///< Human readable, translated file type
    const char* TagType;  ///< Human readable, translated type of tag, e.g. "ID3 Tag"

    /// Implicit registration of this file type
    /// @remarks <b>You must not create non-static instances of this class.</b>
    ET_File_Description();

    /// Supported file type or dummy entry?
    bool IsSupported() const { return *Extension != 0; }

    // temporary vtable for ET_File...
    /// read tag and file information from file
    File_Tag* (*read_file)(GFile* gfile, ET_File* FileTag, GError** error);
    /// write tag to file
    gboolean (*write_file_tag)(const ET_File* file, GError** error);
    /// extract file header information for UI
    void (*display_file_info_to_ui)(EtFileHeaderFields *fields, const ET_File *ETFile);
    /// restrict the visible fields in the UI, leave empty if all fields are suppoerted
    unsigned (*unsupported_fields)(const ET_File* file);
    /// Check whether the tag supports multiple pictures with description. true by default.
    bool (*support_multiple_pictures)(const ET_File* file);

    /// Determines description of file.
    /// @returns If \p filename is NULL or no registered instance of ET_File_Description matches the file extension,
    /// it returns a default instance that represents unsupported files.
    static const ET_File_Description* Get(const gchar *filename);

private:
    static const ET_File_Description* Root;
    const ET_File_Description* Link;
};

/// Returns the extension of the file
inline const gchar* ET_Get_File_Extension(const gchar *filename)
{	return filename ? strrchr(filename, '.') : nullptr;
}

/// Removes the extension from the file name.
std::string ET_Remove_File_Extension(const gchar *filename);

#endif /* !ET_FILE_DESCRIPTION_H_ */
