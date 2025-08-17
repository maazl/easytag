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

#ifndef ET_GIO_WRAPPER_H_
#define ET_GIO_WRAPPER_H_

#include "config.h"

#if defined(ENABLE_MP4) || defined(ENAMBLE_ASF)

#include "../misc.h"
#include <taglib/tiostream.h>
#include <gio/gio.h>

class GIO_Stream : public TagLib::IOStream
{
protected:
#if TAGLIB_MAJOR_VERSION >= 2
	typedef TagLib::offset_t offset_t;
#else
	typedef TagLib::ulong offset_t;
#endif

public:
    virtual ~GIO_Stream ();
    virtual TagLib::FileName name () const;
    virtual bool isOpen () const;
    virtual void clear ();
    virtual void seek (long int offset, TagLib::IOStream::Position p = TagLib::IOStream::Beginning);
    virtual long int tell () const;

    virtual const GError *getError() const;

protected:
    GIO_Stream (GFile *file_);
    GIO_Stream (const GIO_Stream &other) = delete;
    void operator= (const GIO_Stream &other) = delete;

    GFile *file;
    char *filename;
    GSeekable *seekable; // type depends on implementing class
    GError *error;
};

class GIO_InputStream : public GIO_Stream
{
public:
    GIO_InputStream (GFile *file_);
    virtual TagLib::ByteVector readBlock (ulong length);
    virtual void writeBlock (TagLib::ByteVector const &data);
    virtual void insert (TagLib::ByteVector const &data, offset_t start = 0, size_t replace = 0);
    virtual void removeBlock (offset_t start = 0, size_t length = 0);
    virtual bool readOnly () const;
    virtual long int length ();
    virtual void truncate (long int length);

private:
    GFileInputStream *stream; // owned by base class (seekable)
};

class GIO_IOStream : public GIO_Stream
{
public:
    GIO_IOStream (GFile *file_);
    virtual TagLib::ByteVector readBlock (ulong length);
    virtual void writeBlock (TagLib::ByteVector const &data);
    virtual void insert (TagLib::ByteVector const &data, offset_t start = 0, size_t replace = 0);
    virtual void removeBlock (offset_t start = 0, size_t length = 0);
    virtual bool readOnly () const;
    virtual long int length ();
    virtual void truncate (long int length);

private:
    GFileIOStream *stream; // owned by base class (seekable)
};

#endif /* ENABLE_MP4/ASF */

#endif /* ET_GIO_WRAPPER_H_ */
