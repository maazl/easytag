/* EasyTAG - tag editor for audio files
 * Copyright (C) 2014 David King <amigadave@amigadave.com>
 * Copyright (C) 2002 Artur Polaczynski (Ar't) <artii@o2.pl>
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

/*
    Some portions of code or/and ideas come from 
    winamp plugins, xmms plugins, mppdec decoder
    thanks:
    -Frank Klemm <Frank.Klemm@uni-jena.de> 
    -Andree Buschmann <Andree.Buschmann@web.de> 
    -Thomas Juerges <thomas.juerges@astro.ruhr-uni-bochum.de> 
*/

#include <errno.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "info_mpc.h"
#include "is_tag.h"

#define MPC_HEADER_LENGTH 16

/*
*.MPC,*.MP+,*.MPP
*/
/* Profile is 0...15, where 7...13 is used. */
static const char *
profile_stringify (unsigned int profile)
{
    static const char na[] = "n.a.";
    static const char *Names[] = {
        na, "Experimental", na, na,
        na, na, na, "Telephone",
        "Thumb", "Radio", "Standard", "Xtreme",
        "Insane", "BrainDead", "BrainDead+", "BrainDead++"
    };

    return profile >=
        sizeof (Names) / sizeof (*Names) ? na : Names[profile];
}

/*
* info_mpc_read:
* @file: file from which to read a header
* @stream_info: stream information to fill
* @error: a #Gerror, or %NULL
*
* Read header from the given MusePack @file.
*
* Returns: %TRUE on success, %FALSE and with @error set on failure
*/
gboolean
info_mpc_read (GFile *file,
               ET_File *ETFile,
               GError **error)
{
    GFileInputStream *istream;
    guint32 header_buffer[MPC_HEADER_LENGTH];
    gsize bytes_read;
    gsize id3_size;
    guint64 byteLength;

    {
        gchar *path;
        FILE *fp;

        path = g_file_get_path (file);
        fp = g_fopen (path, "rb");
        g_free (path);

        if (!fp)
        {
            /* TODO: Add specific error domain and message. */
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                         g_strerror (EINVAL));
            return FALSE;
        }

        /* Skip id3v2. */
        /* FIXME: is_id3v2 (and is_id3v1 and is_ape) should accept an istream
         * or GFile. */
        id3_size = is_id3v2 (fp);

        /* Stream size. */
        byteLength = ETFile->FileSize - is_id3v1(fp) - is_ape(fp) - id3_size;

        fclose (fp);
    }
        
    istream = g_file_read (file, NULL, error);

    if (!istream)
    {
        return FALSE;
    }

    if (!g_seekable_seek (G_SEEKABLE (istream), id3_size, G_SEEK_SET, NULL,
                          error))
    {
        return FALSE;
    }

    /* Read 16 guint32. */
    if (!g_input_stream_read_all (G_INPUT_STREAM (istream), header_buffer,
                                  MPC_HEADER_LENGTH * 4, &bytes_read, NULL,
                                  error))
    {
        g_debug ("Only %" G_GSIZE_FORMAT "bytes out of 16 bytes of data were "
                 "read", bytes_read);
        return FALSE;
    }

    /* FIXME: Read 4 bytes, take as a uint32, then byteswap if necessary. (The
     * official Musepack decoder expects the user(!) to request the
     * byteswap.) */
    if (memcmp (header_buffer, "MP+", 3) != 0)
    {
        /* TODO: Add specific error domain and message. */
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s",
                     g_strerror (EINVAL));
        return FALSE;
    }
    
    ET_File_Info* ETFileInfo = &ETFile->ETFileInfo;

    ETFileInfo->version = header_buffer[0] >> 24;
    unsigned profile;
    unsigned long frames;

    if (ETFileInfo->version >= 7)
    {
        const long samplefreqs[4] = { 44100, 48000, 37800, 32000 };
        
        // read the file-header (SV7 and above)
        frames = header_buffer[1];
        ETFileInfo->samplerate = samplefreqs[(header_buffer[2] >> 16) & 0x0003];
        //stream_info->MaxBand = (header_buffer[2] >> 24) & 0x003F;
        //stream_info->MS = (header_buffer[2] >> 30) & 0x0001;
        profile = (header_buffer[2] << 8) >> 28;
        //stream_info->IS = (header_buffer[2] >> 31) & 0x0001;
        //stream_info->BlockSize = 1;
        
        unsigned encoderVersion = (header_buffer[6] >> 24) & 0x00FF;
        ETFileInfo->mode = 2; // channels
        /* currently ignored by EasyTag...
        // gain
        stream_info->EstPeakTitle = header_buffer[2] & 0xFFFF;    // read the ReplayGain data
        stream_info->GainTitle = (header_buffer[3] >> 16) & 0xFFFF;
        stream_info->PeakTitle = header_buffer[3] & 0xFFFF;
        stream_info->GainAlbum = (header_buffer[4] >> 16) & 0xFFFF;
        stream_info->PeakAlbum = header_buffer[4] & 0xFFFF;
        // gaples
        stream_info->IsTrueGapless = (header_buffer[5] >> 31) & 0x0001;    // true gapless: used?
        stream_info->LastFrameSamples = (header_buffer[5] >> 20) & 0x07FF;    // true gapless: valid samples for last frame*/
        
        if (encoderVersion == 0)
        {
            ETFileInfo->mpc_version = g_strdup("<= 1.05"); // Buschmann 1.7.x, Klemm <= 1.05
        }
        else
        {
            switch (encoderVersion % 10)
            {
            case 0:
                ETFileInfo->mpc_version = g_strdup_printf("%u.%u",
                    encoderVersion / 100, encoderVersion / 10 % 10);
                break;
            case 2:
            case 4:
            case 6:
            case 8:
                ETFileInfo->mpc_version = g_strdup_printf("%u.%02u Beta",
                    encoderVersion / 100, encoderVersion % 100);
                break;
            default:
                ETFileInfo->mpc_version = g_strdup_printf("%u.%02u Alpha",
                    encoderVersion / 100, encoderVersion % 100);
                break;
            }
        }
        // estimation, exact value needs too much time
        ETFileInfo->bitrate = byteLength * ETFileInfo->samplerate / (1152 * frames - 576) / 125;
    }
    else
    {
        // read the file-header (SV6 and below)
        ETFileInfo->bitrate = (header_buffer[0] >> 23) & 0x01FF;
        //stream_info->MS = (header_buffer[0] >> 21) & 0x0001;
        //stream_info->IS = (header_buffer[0] >> 22) & 0x0001;
        ETFileInfo->version = (header_buffer[0] >> 11) & 0x03FF;
        //stream_info->MaxBand = (header_buffer[0] >> 6) & 0x001F;
        //stream_info->BlockSize = (header_buffer[0]) & 0x003F;
        
        profile = 0;
        
        if (ETFileInfo->version >= 5)
            frames = header_buffer[1];    // 32 bit
        else
            frames = header_buffer[1] >> 16;    // 16 bit

#if 0
        if (Info->StreamVersion == 7)
            return ERROR_CODE_SV7BETA;    // are there any unsupported parameters used?
        if (Info->Bitrate != 0)
            return ERROR_CODE_CBR;
        if (Info->IS != 0)
            return ERROR_CODE_IS;
        if (Info->BlockSize != 1)
            return ERROR_CODE_BLOCKSIZE;
#endif
        if (ETFileInfo->version < 6)    // Bugfix: last frame was invalid for up to SV5
            frames -= 1;
        
        ETFileInfo->samplerate = 44100;    // AB: used by all files up to SV7
        ETFileInfo->mode = 2; // channels
    }
    
    ETFileInfo->mpc_profile = g_strdup(profile_stringify(profile));
    
    ETFileInfo->duration = (int)(frames * 1152 / ETFileInfo->samplerate);

    return TRUE;
}
