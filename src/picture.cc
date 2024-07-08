/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2024 Marcel Müller
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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

#include "picture.h"

#include <glib/gi18n.h>

#include "easytag.h"
#include "file.h"
#include "log.h"
#include "misc.h"
#include "setting.h"
#include "charset.h"

#include "win32/win32dep.h"
#include <unordered_set>
#include <mutex>
using namespace std;


guint EtPicture::Data::CalcHash(const void* bytes, unsigned size)
{	GBytes* gb = g_bytes_new_static(bytes, size);
	guint hash = g_bytes_hash(gb);
	g_bytes_unref(gb);
	return hash;
}

struct EtPictureDataHash
{	constexpr size_t operator()(const EtPicture::Data* d) const noexcept
	{	return d->Hash; }
};

struct EtPictureDataEquals
{	static const void* GetBytes(const EtPicture::Data* d) noexcept
	{	return d->RefCount ? &d->Bytes : d->DataRef; }
	bool operator()(EtPicture::Data* l, EtPicture::Data* r) const noexcept
	{	return l->Size == r->Size && memcmp(GetBytes(l), GetBytes(r), l->Size) == 0; }
};

// Repository with all instances.

// If the ref count of an instance is 1 it could be removed.
static unordered_set<EtPicture::Data*, EtPictureDataHash, EtPictureDataEquals> Instances;
// Protect the dictionary above
static mutex InstancesMutex;

EtPicture::Data* EtPicture::GetOrAllocate(const void* data, unsigned size)
{	lock_guard<mutex> lock(InstancesMutex);
	Data tmp(data, size);
	auto it = Instances.find(&tmp);
	if (it != Instances.end())
	{	// cache hit => return reference
		++(*it)->RefCount;
		return *it;
	}
	// cache miss => allocate storage
	Data* storage = (Data*)g_malloc(offsetof(Data, Bytes) + size);
	memcpy(storage->Bytes, data, size);
	new(storage) Data(size, tmp.Hash);
	Instances.insert(storage);
	return storage;
}

void EtPicture::Deduplicate()
{	lock_guard<mutex> lock(InstancesMutex);
	auto it = Instances.find(storage);
	if (it != Instances.end())
	{	// cache hit
		g_free(storage);
		storage = *it;
	} else
		Instances.insert(storage);
	++storage->RefCount;
}

void EtPicture::GarbageCollector()
{	lock_guard<mutex> lock(InstancesMutex);
	for (auto it = Instances.begin(); it != Instances.end(); )
	{	if ((*it)->RefCount != 1)
			++it;
		else
		{	auto ptr = *it;
			it = Instances.erase(it);
			g_free(ptr);
		}
	}
}


static EtPicture* et_picture_copy_single(const EtPicture* pic) { return new EtPicture(*pic); }
static void et_picture_free(EtPicture* pic) { delete pic; }

G_DEFINE_BOXED_TYPE (EtPicture, et_picture, et_picture_copy_single, et_picture_free)

EtPicture::EtPicture(EtPictureType type, const gchar *description, guint width, guint height, const void* data, unsigned size)
:	storage(GetOrAllocate(data, size))
,	description(description)
,	type(type)
{	if (!storage->Width)
		storage->Width = width;
	if (!storage->Height)
		storage->Height = height;
}

EtPicture::EtPicture(const EtPicture& r) noexcept
:	storage(r.storage)
,	description(r.description)
,	type(r.type)
{	if (storage)
		++storage->RefCount;
}

EtPicture::~EtPicture()
{	if (storage && !--storage->RefCount)
	{	storage->RefCount.~atomic();
		g_free(storage);
	}
}

bool operator==(const EtPicture& l, const EtPicture& r)
{	if (l.description != r.description || l.type != r.type)
		return false;
	if (l.storage == r.storage)
		return true;
	if (!l.storage || !r.storage)
		return false;
	return l.storage->Size == r.storage->Size
		&& memcmp(l.storage->Bytes, r.storage->Bytes, l.storage->Size) == 0;
}

EtPictureType EtPicture::type_from_filename(const gchar *filename_utf8)
{
    EtPictureType picture_type = ET_PICTURE_TYPE_FRONT_COVER;

    /* TODO: Use g_str_tokenize_and_fold(). */
    static const struct
    {
        const gchar *type_str;
        const EtPictureType pic_type;
    } type_mappings[] =
    {
        { "front", ET_PICTURE_TYPE_FRONT_COVER },
        { "back", ET_PICTURE_TYPE_BACK_COVER },
        { "inlay", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "inside", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "leaflet", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "page", ET_PICTURE_TYPE_LEAFLET_PAGE },
        { "CD", ET_PICTURE_TYPE_MEDIA },
        { "media", ET_PICTURE_TYPE_MEDIA },
        { "artist", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "performer", ET_PICTURE_TYPE_ARTIST_PERFORMER },
        { "conductor", ET_PICTURE_TYPE_CONDUCTOR },
        { "band", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "orchestra", ET_PICTURE_TYPE_BAND_ORCHESTRA },
        { "composer", ET_PICTURE_TYPE_COMPOSER },
        { "lyricist", ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER },
        { "illustration", ET_PICTURE_TYPE_ILLUSTRATION },
        { "publisher", ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE }
    };
    gsize i;
    gchar *folded_filename;

    g_return_val_if_fail (filename_utf8 != NULL, ET_PICTURE_TYPE_FRONT_COVER);

    folded_filename = g_utf8_casefold (filename_utf8, -1);

    for (i = 0; i < G_N_ELEMENTS (type_mappings); i++)
    {
        gchar *folded_type = g_utf8_casefold (type_mappings[i].type_str, -1);
        if (strstr (folded_filename, folded_type) != NULL)
        {
            picture_type = type_mappings[i].pic_type;
            g_free (folded_type);
            break;
        }
        else
        {
            g_free (folded_type);
        }
    }

    g_free (folded_filename);

    return picture_type;
}

/* FIXME: Possibly use gnome_vfs_get_mime_type_for_buffer.
 * and cache the result in EtPictureData */
Picture_Format EtPicture::Format() const
{
    g_return_val_if_fail(storage != NULL, PICTURE_FORMAT_UNKNOWN);

    size_t size = storage->Size;
    const void* raw = storage->Bytes;

    /* JPEG : "\xff\xd8\xff". */
    if (size > 3 && (memcmp(raw, "\xff\xd8\xff", 3) == 0))
        return PICTURE_FORMAT_JPEG;

    /* PNG : "\x89PNG\x0d\x0a\x1a\x0a". */
    if (size > 8 && (memcmp(raw, "\x89PNG\x0d\x0a\x1a\x0a", 8) == 0))
        return PICTURE_FORMAT_PNG;
    
    /* GIF: "GIF87a" */
    if (size > 6 && (memcmp(raw, "GIF87a", 6) == 0))
        return PICTURE_FORMAT_GIF;

    /* GIF: "GIF89a" */
    if (size > 6 && (memcmp(raw, "GIF89a", 6) == 0))
        return PICTURE_FORMAT_GIF;
    
    return PICTURE_FORMAT_UNKNOWN;
}

const char* EtPicture::Mime_Type_String(Picture_Format format)
{
    switch (format)
    {
        case PICTURE_FORMAT_JPEG:
            return "image/jpeg";
        case PICTURE_FORMAT_PNG:
            return "image/png";
        case PICTURE_FORMAT_GIF:
            return "image/gif";
        case PICTURE_FORMAT_UNKNOWN:
        default:
            g_debug ("%s", "Unrecognised image MIME type");
            return "application/octet-stream";
    }
}


const char* EtPicture::Format_String(Picture_Format format)
{
    switch (format)
    {
        case PICTURE_FORMAT_JPEG:
            return _("JPEG image");
        case PICTURE_FORMAT_PNG:
            return _("PNG image");
        case PICTURE_FORMAT_GIF:
            return _("GIF image");
        case PICTURE_FORMAT_UNKNOWN:
        default:
            return _("Unknown image");
    }
}

const char* EtPicture::Type_String(EtPictureType type)
{
    switch (type)
    {
        case ET_PICTURE_TYPE_OTHER:
            return _("Other");
        case ET_PICTURE_TYPE_FILE_ICON:
            return _("32×32 pixel PNG file icon");
        case ET_PICTURE_TYPE_OTHER_FILE_ICON:
            return _("Other file icon");
        case ET_PICTURE_TYPE_FRONT_COVER:
            return _("Cover (front)");
        case ET_PICTURE_TYPE_BACK_COVER:
            return _("Cover (back)");
        case ET_PICTURE_TYPE_LEAFLET_PAGE:
            return _("Leaflet page");
        case ET_PICTURE_TYPE_MEDIA:
            return _("Media (such as label side of CD)");
        case ET_PICTURE_TYPE_LEAD_ARTIST_LEAD_PERFORMER_SOLOIST:
            return _("Lead artist/lead performer/soloist");
        case ET_PICTURE_TYPE_ARTIST_PERFORMER:
            return _("Artist/performer");
        case ET_PICTURE_TYPE_CONDUCTOR:
            return _("Conductor");
        case ET_PICTURE_TYPE_BAND_ORCHESTRA:
            return _("Band/Orchestra");
        case ET_PICTURE_TYPE_COMPOSER:
            return _("Composer");
        case ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER:
            return _("Lyricist/text writer");
        case ET_PICTURE_TYPE_RECORDING_LOCATION:
            return _("Recording location");
        case ET_PICTURE_TYPE_DURING_RECORDING:
            return _("During recording");
        case ET_PICTURE_TYPE_DURING_PERFORMANCE:
            return _("During performance");
        case ET_PICTURE_TYPE_MOVIE_VIDEO_SCREEN_CAPTURE:
            return _("Movie/video screen capture");
        case ET_PICTURE_TYPE_A_BRIGHT_COLOURED_FISH:
            return _("A bright colored fish");
        case ET_PICTURE_TYPE_ILLUSTRATION:
            return _("Illustration");
        case ET_PICTURE_TYPE_BAND_ARTIST_LOGOTYPE:
            return _("Band/Artist logotype");
        case ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE:
            return _("Publisher/studio logotype");
        
        case ET_PICTURE_TYPE_UNDEFINED:
        default:
            return _("Unknown image type");
    }
}

string EtPicture::format_info(const ET_File& ETFile) const
{
    g_return_val_if_fail(storage != NULL, string());

    const char* format = Format_String(Format());
    const char* desc = description;
    const char* type = Type_String(this->type);
    gString size_str(g_format_size(storage->Size));

    /* Behaviour following the tag type. */
    if (!ETFile.ETFileDescription->support_multiple_pictures(&ETFile))
        return strprintf("%s (%s - %d×%d %s)\n%s: %s", format,
            size_str.get(), storage->Width, storage->Height,
            _("pixels"), _("Type"), type);
    else
        return strprintf("%s (%s - %d×%d %s)\n%s: %s\n%s: %s", format,
            size_str.get(), storage->Width, storage->Height,
            _("pixels"), _("Type"), type, _("Description"), desc);
}

EtPicture::EtPicture(GFile *file, GError **error)
:	storage(nullptr)
,	type(ET_PICTURE_TYPE_UNDEFINED)
{
    gsize size;
    GFileInfo *info;
    GFileInputStream *file_istream;
    gObject<GOutputStream> ostream;

    info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                              G_FILE_QUERY_INFO_NONE, NULL, error);
    if (!info)
    {
        g_assert (error == NULL || *error != NULL);
        return;
    }

    file_istream = g_file_read (file, NULL, error);
    if (!file_istream)
    {
        g_assert (error == NULL || *error != NULL);
        return;
    }

    size = g_file_info_get_size (info);
    g_object_unref (info);

    /* HTTP servers may not report a size, or the file could be empty. */
    if (size == 0)
        size = 60000;
    {
        gchar *buffer = (gchar*)g_malloc (size);
        ostream = gObject<GOutputStream>(g_memory_output_stream_new(buffer, size, g_realloc, g_free));
        g_seekable_seek(G_SEEKABLE(ostream.get()), offsetof(Data, Bytes), G_SEEK_SET, NULL, NULL);
    }

    if (g_output_stream_splice(ostream.get(), G_INPUT_STREAM (file_istream),
        G_OUTPUT_STREAM_SPLICE_NONE, NULL, error) == -1)
    {
        g_object_unref(ostream.get());
        g_object_unref (file_istream);
        g_assert (error == NULL || *error != NULL);
        return;
    }

    /* Image loaded. */
    g_object_unref (file_istream);

    if (!g_output_stream_close(ostream.get(), NULL, error))
    {
        g_assert (error == NULL || *error != NULL);
        return;
    }

    g_assert (error == NULL || *error == NULL);

    // update to real size
    size = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(ostream.get()));
    if (size == 0)
    {
        /* FIXME: Mark up the string for translation. */
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "%s",
                     _("Input truncated or empty"));
        return;
    }

    storage = (Data*)g_memory_output_stream_steal_data(G_MEMORY_OUTPUT_STREAM(ostream.get()));
    new(storage) Data(size);

    Deduplicate();

    g_assert (error == NULL || *error == NULL);
}

gObject<GdkPixbuf> EtPicture::get_pix_buf(GError **error) const
{
    gObject<GdkPixbuf> pixbuf;
    // analyze file data
    gObject<GdkPixbufLoader> loader(gdk_pixbuf_loader_new());
    if (!gdk_pixbuf_loader_write_bytes(loader.get(), bytes().get(), error))
        return pixbuf;

    if (!gdk_pixbuf_loader_close(loader.get(), error))
        return pixbuf;

    pixbuf = gObject<GdkPixbuf>(gdk_pixbuf_loader_get_pixbuf(loader.get()));
    if (!pixbuf)
    {   g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "%s",
            _("Cannot display the image because not enough data has been read to determine how to create the image buffer"));
        return pixbuf;
    } else
    {   g_object_ref(pixbuf.get());

        storage->Width  = gdk_pixbuf_get_width(pixbuf.get());
        storage->Height = gdk_pixbuf_get_height(pixbuf.get());
    }
    return pixbuf;
}

bool EtPicture::save_file_data(GFile *file, GError **error) const
{
    GFileOutputStream *file_ostream;
    gsize bytes_written;

    g_return_val_if_fail (storage != NULL && (error == NULL || *error == NULL), false);

    file_ostream = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_NONE, NULL,
                                   error);

    if (!file_ostream)
    {
        g_assert (error == NULL || *error != NULL);
        return false;
    }

    if (!g_output_stream_write_all (G_OUTPUT_STREAM (file_ostream),
        storage->Bytes, storage->Size, &bytes_written, NULL, error))
    {
        g_debug ("Only %" G_GSIZE_FORMAT " bytes out of %u"
                 " bytes of picture data were written", bytes_written, storage->Size);
        g_object_unref (file_ostream);
        g_assert (error == NULL || *error != NULL);
        return false;
    }

    if (!g_output_stream_close (G_OUTPUT_STREAM (file_ostream), NULL, error))
    {
        g_object_unref (file_ostream);
        g_assert (error == NULL || *error != NULL);
        return false;
    }

    g_assert (error == NULL || *error == NULL);
    g_object_unref (file_ostream);
    return true;
}
