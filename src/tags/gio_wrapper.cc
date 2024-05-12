/*
 *  EasyTAG - Tag editor for audio files
 *  Copyright (C) 2024  Marcel Müller
 *  Copyright (C) 2014  Santtu Lakkala <inz@inz.fi>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if defined(ENABLE_MP4) || defined(ENAMBLE_ASF)

#include "gio_wrapper.h"

GIO_Stream::GIO_Stream (GFile * file_) :
    file ((GFile *)g_object_ref (gpointer (file_))),
    filename (g_file_get_uri (file)),
    seekable (NULL),
    error (NULL)
{ }

GIO_Stream::~GIO_Stream ()
{
    g_clear_error (&error);
    g_clear_object (&seekable);
    g_free (filename);
    g_object_unref (file);
}

TagLib::FileName
GIO_Stream::name () const
{
    return TagLib::FileName (filename);
}

bool
GIO_Stream::isOpen () const
{
    return !!seekable;
}

void
GIO_Stream::clear ()
{
    TagLib::IOStream::clear();
    g_clear_error (&error);
}

void
GIO_Stream::seek (long int offset, TagLib::IOStream::Position p)
{
    if (error)
        return;

    GSeekType type;

    switch (p)
    {
        case TagLib::IOStream::Beginning:
            type = G_SEEK_SET;
            break;
        case TagLib::IOStream::Current:
            type = G_SEEK_CUR;
            break;
        case TagLib::IOStream::End:
            type = G_SEEK_END;
            break;
        default:
            // set invalid value to force g_seekable_seek to fail.
            type = (GSeekType)(G_SEEK_SET|G_SEEK_CUR|G_SEEK_END);
    }

    g_seekable_seek (seekable, offset, type, NULL, &error);
}

long int
GIO_Stream::tell () const
{
    return g_seekable_tell (seekable);
}

const GError *
GIO_Stream::getError () const
{
    return error;
}


GIO_InputStream::GIO_InputStream (GFile * file_)
:   GIO_Stream (file_)
{
    stream = g_file_read (file, NULL, &error);
    seekable = G_SEEKABLE(stream);
}

TagLib::ByteVector
GIO_InputStream::readBlock (ulong len)
{
    if (error || !len)
    {
        return TagLib::ByteVector();
    }

    TagLib::ByteVector rv (len, 0);
    gsize bytes = 0;
    g_input_stream_read_all (G_INPUT_STREAM (stream), (void *)rv.data (),
                             len, &bytes, NULL, &error);

    return rv.resize (bytes);
}

void
GIO_InputStream::writeBlock (TagLib::ByteVector const &data)
{
    g_warning ("%s", "Trying to write to read-only file!");
}

void
GIO_InputStream::insert (TagLib::ByteVector const &data,
                         TagLib::offset_t start,
                         size_t replace)
{
    g_warning ("%s", "Trying to write to read-only file!");
}

void
GIO_InputStream::removeBlock (TagLib::offset_t start, size_t len)
{
    g_warning ("%s", "Trying to write to read-only file!");
}

bool
GIO_InputStream::readOnly () const
{
    return true;
}

long int
GIO_InputStream::length ()
{
    if (error)
    {
        return -1;
    }

    long int rv = -1;
    GFileInfo *info = g_file_input_stream_query_info (stream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);
    if (info)
    {
        rv = g_file_info_get_size (info);
        g_object_unref (info);
    }

    return rv;
}

void
GIO_InputStream::truncate (long int len)
{
    g_warning ("%s", "Trying to truncate read-only file");
}


GIO_IOStream::GIO_IOStream (GFile *file_)
:   GIO_Stream(file_)
{
    stream = g_file_open_readwrite (file, NULL, &error);
    seekable = G_SEEKABLE(stream);
}

TagLib::ByteVector
GIO_IOStream::readBlock (ulong len)
{
    if (error || !len)
    {
        return TagLib::ByteVector();
    }

    gsize bytes = 0;
    TagLib::ByteVector rv (len, 0);
    GInputStream *istream = g_io_stream_get_input_stream (G_IO_STREAM (stream));
    g_input_stream_read_all(istream, (void *)rv.data(), len, &bytes, NULL, &error);

    return rv.resize(bytes);
}

void
GIO_IOStream::writeBlock (TagLib::ByteVector const &data)
{
    if (error)
    {
        return;
    }

    gsize bytes_written;
    GOutputStream *ostream = g_io_stream_get_output_stream (G_IO_STREAM (stream));

    if (!g_output_stream_write_all (ostream, data.data (), data.size (),
                                    &bytes_written, NULL, &error))
    {
        g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %u bytes of data were "
                 "written", bytes_written, data.size ());
    }
}

void
GIO_IOStream::insert (TagLib::ByteVector const &data,
                      TagLib::offset_t start,
                      size_t replace)
{
    if (error)
    {
        return;
    }

    if (data.size () == replace)
    {
        seek (start, TagLib::IOStream::Beginning);
        writeBlock (data);
        return;
    }
    else if (data.size () < replace)
    {
        removeBlock (start, replace - data.size ());
        seek (start);
        writeBlock (data);
        return;
    }

    GFileIOStream *tstr;
    GFile *tmp = g_file_new_tmp ("easytag-XXXXXX", &tstr, &error);

    if (tmp == NULL)
    {
        return;
    }

    char buffer[4096];
    gsize r;

    GOutputStream *ostream = g_io_stream_get_output_stream (G_IO_STREAM (tstr));
    GInputStream *istream = g_io_stream_get_input_stream (G_IO_STREAM (stream));

    seek (0);

    while (g_input_stream_read_all (istream, buffer,
                                    MIN (G_N_ELEMENTS (buffer), start),
                                    &r, NULL, &error) && r > 0)
    {
        gsize w;
        g_output_stream_write_all (ostream, buffer, r, &w, NULL, &error);

        if (w != r)
        {
            g_warning ("%s", "Unable to write all bytes");
        }

	if (error)
	{
            g_object_unref (tstr);
            g_object_unref (tmp);
            return;
	}

        start -= r;
    }

    if (error)
    {
        g_object_unref (tstr);
        g_object_unref (tmp);
        return;
    }

    g_output_stream_write_all (ostream, data.data (), data.size (), NULL,
                               NULL, &error);
    seek (replace, TagLib::IOStream::Current);

    if (error)
    {
        g_object_unref (tstr);
        g_object_unref (tmp);
        return;
    }

    while (g_input_stream_read_all (istream, buffer, sizeof (buffer), &r,
                                    NULL, &error) && r > 0)
    {
        gsize bytes_written;

        if (!g_output_stream_write_all (ostream, buffer, r, &bytes_written,
                                        NULL, &error))
        {
            g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %" G_GSIZE_FORMAT
                     " bytes of data were written", bytes_written, r);
            g_object_unref (tstr);
            g_object_unref (tmp);
            return;
        }
    }

    g_object_unref (tstr);
    g_object_unref (stream);
    stream = NULL;
    seekable = NULL;

    g_file_move (tmp, file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);

    if (error)
    {
        g_object_unref (tmp);
	      return;
    }

    stream = g_file_open_readwrite (file, NULL, &error);
    seekable = G_SEEKABLE(stream);

    g_object_unref (tmp);
}

void
GIO_IOStream::removeBlock (TagLib::offset_t start, size_t len)
{
    if (start + len >= (ulong)length ())
    {
        truncate (start);
        return;
    }

    char *buffer[4096];
    gsize r;
    GInputStream *istream = g_io_stream_get_input_stream (G_IO_STREAM (stream));
    GOutputStream *ostream = g_io_stream_get_output_stream (G_IO_STREAM (stream));
    seek (start + len);

    while (g_input_stream_read_all (istream, buffer, sizeof (buffer), &r, NULL,
                                    NULL) && r > 0)
    {
        gsize bytes_written;

        seek (start);

        if (!g_output_stream_write_all (ostream, buffer, r, &bytes_written,
                                        NULL, &error))
        {
            g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %" G_GSIZE_FORMAT
                     " bytes of data were written", bytes_written, r);
            return;
        }

        start += r;
        seek (start + len);
    }

    truncate (start);
}

bool
GIO_IOStream::readOnly () const
{
    return !stream;
}

long int
GIO_IOStream::length ()
{
    long rv = -1;

    if (error)
    {
        return rv;
    }

    GFileInfo *info = g_file_io_stream_query_info (stream, G_FILE_ATTRIBUTE_STANDARD_SIZE, NULL, &error);

    if (info)
    {
        rv = g_file_info_get_size (info);
        g_object_unref (info);
    }

    return rv;
}

void
GIO_IOStream::truncate (long int len)
{
    if (error)
    {
        return;
    }

    g_seekable_truncate (seekable, len, NULL, &error);
}

#endif /* ENABLE_MP4/ASF */
