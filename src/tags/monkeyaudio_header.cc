/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
 * Copyright (C) 2001-2003  Jerome Couderc <easytag@gmail.com>
 * Copyright (C) 2002-2003  Artur Polaczyñski <artii@o2.pl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "et_core.h"
#include "misc.h"
#include "monkeyaudio_header.h"
#include "libapetag/info_mac.h"

gboolean
et_mac_header_read_file_info (GFile *file,
                              ET_File_Info *ETFileInfo,
                              GError **error)
{
    StreamInfoMac Info;

    g_return_val_if_fail (file != NULL && ETFileInfo != NULL, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    if (!info_mac_read (file, &Info, error))
    {
        return FALSE;
    }

    ETFileInfo->mpc_profile   = g_strdup(Info.CompresionName);
    ETFileInfo->version       = Info.Version;
    ETFileInfo->bitrate       = Info.Bitrate/1000.0;
    ETFileInfo->samplerate    = Info.SampleFreq;
    ETFileInfo->mode          = Info.Channels;
    ETFileInfo->size          = Info.FileSize;
    ETFileInfo->duration      = Info.Duration/1000;

    return TRUE;
}

void
et_mac_header_display_file_info_to_ui (EtFileHeaderFields *fields, const ET_File *ETFile)
{
    const ET_File_Info *info = &ETFile->ETFileInfo;

    fields->description = _("Monkey's Audio File");

    /* Mode changed to profile name  */
    fields->mode_label = _("Profile:");
    fields->mode = info->mpc_profile;

    /* Version changed to encoder version */
    fields->version_label = _("Encoder:");
    fields->version = strprintf("%i.%i", info->version / 1000, info->version % 1000);
}
