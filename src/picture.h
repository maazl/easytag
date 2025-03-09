/* EasyTAG - tag editor for audio files
 * Copyright (C) 2024 Marcel Müller
 * Copyright (C) 2014-2015  David King <amigadave@amigadave.com>
 * Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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

#ifndef ET_PICTURE_H_
#define ET_PICTURE_H_

#include <gio/gio.h>

#define ET_TYPE_PICTURE (et_picture_get_type ())
GType et_picture_get_type (void);

#include "misc.h"
#include "xstring.h"
#include <atomic>

struct ET_File;

typedef enum // Picture types
{
    // Same values for FLAC, see: https://xiph.org/flac/api/group__flac__format.html#gaf6d3e836cee023e0b8d897f1fdc9825d
    ET_PICTURE_TYPE_OTHER = 0,
    ET_PICTURE_TYPE_FILE_ICON,
    ET_PICTURE_TYPE_OTHER_FILE_ICON,
    ET_PICTURE_TYPE_FRONT_COVER,
    ET_PICTURE_TYPE_BACK_COVER,
    ET_PICTURE_TYPE_LEAFLET_PAGE,
    ET_PICTURE_TYPE_MEDIA,
    ET_PICTURE_TYPE_LEAD_ARTIST_LEAD_PERFORMER_SOLOIST,
    ET_PICTURE_TYPE_ARTIST_PERFORMER,
    ET_PICTURE_TYPE_CONDUCTOR,
    ET_PICTURE_TYPE_BAND_ORCHESTRA,
    ET_PICTURE_TYPE_COMPOSER,
    ET_PICTURE_TYPE_LYRICIST_TEXT_WRITER,
    ET_PICTURE_TYPE_RECORDING_LOCATION,
    ET_PICTURE_TYPE_DURING_RECORDING,
    ET_PICTURE_TYPE_DURING_PERFORMANCE,
    ET_PICTURE_TYPE_MOVIE_VIDEO_SCREEN_CAPTURE,
    ET_PICTURE_TYPE_A_BRIGHT_COLOURED_FISH,
    ET_PICTURE_TYPE_ILLUSTRATION,
    ET_PICTURE_TYPE_BAND_ARTIST_LOGOTYPE,
    ET_PICTURE_TYPE_PUBLISHER_STUDIO_LOGOTYPE,
    
    ET_PICTURE_TYPE_UNDEFINED
} EtPictureType;

typedef enum
{
    PICTURE_FORMAT_JPEG,
    PICTURE_FORMAT_PNG,
    PICTURE_FORMAT_GIF,
    PICTURE_FORMAT_UNKNOWN
} Picture_Format;

/*
 * EtPicture:
 * @type: type of cover art
 * @description: string to describe the image, often a suitable filename
 * @width: original width, or 0 if unknown
 * @height: original height, or 0 if unknown
 * @bytes: image data
 */
typedef struct EtPicture
{	/// Backing store in EtPicture.
	struct Data
	{	std::atomic<unsigned> RefCount; ///< Reference counter. 0 = tag for use of bytes_ptr.
		const unsigned        Size;     ///< size of \ref bytes or \ref bytes_ref\.
		const guint           Hash;     ///< hash value of \ref bytes or \ref bytes_ref\.
		gint                  Width;    ///< image width (pixels) or 0 if unknown.
		gint                  Height;   ///< image height (pixels) or 0 if unknown.
		union
		{	char                Bytes[1]; ///< inline image data.
			const void*         DataRef;  ///< referenced image data - <b>internal use only</b>
		};
	private:
		static guint CalcHash(const void* bytes, unsigned size);
	public:
		/// Constructor <b>for placement new only!</b>
		explicit Data(unsigned size, unsigned hash) noexcept
		:	RefCount(2), Size(size), Hash(hash), Width(0), Height(0) {}
		/// Constructor <b>for placement new only!</b> Assumes that \ref bytes are already in place.
		explicit Data(unsigned size) noexcept
		:	RefCount(1), Size(size), Hash(CalcHash(&Bytes, size)), Width(0), Height(0) {}
		/// Constructor for unowned data storage. <b>No reference counting, internal use only!</b>
		Data(const void* data, unsigned size) noexcept
		:	RefCount(0), Size(size), Hash(CalcHash(data, size)), Width(0), Height(0), DataRef(data) {}
	};

	Data* storage;
	xStringD0 description;
	EtPictureType type;

	static Data* GetOrAllocate(const void* data, unsigned size);
	void Deduplicate();
public:
	EtPicture(const EtPicture& r) noexcept;
	constexpr EtPicture(EtPicture&& r) noexcept : storage(r.storage), description(std::move(r.description)), type(r.type) { r.storage = nullptr; }
	EtPicture(EtPictureType type, const xStringD0& description, guint width, guint height, const void* data, unsigned size);
	/// Load an image from the supplied \a file.
	/// @param File the GFile from which to load an image
	/// @param Error a GError to provide information on errors, or \c NULL to ignore
	/// On error \ref storage is \c nullptr.
	/// @remarks This function does not validate the loaded image data in any way.
	/// Call \ref get_pix_buf to do so.
	EtPicture(GFile* file, GError** error);
	~EtPicture();
	EtPicture& operator=(const EtPicture& r) = default;
	EtPicture& operator=(EtPicture&& r) = default;
	friend bool operator==(const EtPicture& l, const EtPicture& r);
	friend bool operator!=(const EtPicture& l, const EtPicture& r) { return !(l == r); }
	gBytes bytes() const { return storage ? gBytes(g_bytes_new_static(storage->Bytes, storage->Size)) : gBytes(); }

	/// Use some heuristics to provide an estimate of the type of the picture,
	/// based on the filename.
	/// @param filename UTF-8 representation of a filename
	/// @return The picture type, or \ref ET_PICTURE_TYPE_FRONT_COVER if the type could not be estimated.
	/// @note MP4_TAG:
	/// Just has one picture (ET_PICTURE_TYPE_FRONT_COVER).
	/// The format's don't matter to the MP4 side.
	static EtPictureType type_from_filename(const gchar *filename_utf8);
	Picture_Format Format() const;
	static const char* Mime_Type_String(Picture_Format format);
	static const char* Format_String(Picture_Format format);
	static const char* Type_String(EtPictureType type);
	std::string format_info(const ET_File& etfil) const;

	/// Parse the current image data into a GdkPixbuf.
	/// @param Error a GError to provide information on errors, or \c NULL to ignore
	/// @return image data on success, \c NULL otherwise
	/// @details Calling this function successfully has the side effect
	/// to fill width and height of the image data.
	gObject<GdkPixbuf> get_pix_buf(GError** error) const;
	/// Saves the image to the supplied \a file.
	/// @param file The GFile for which to save an image.
	/// @param Error a GError to provide information on errors, or \c NULL to ignore
	/// @return \c TRUE on success
	bool save_file_data(GFile* file, GError** error) const;

	/// Clean up the internal picture store from orphaned references.
	static void GarbageCollector();
} EtPicture;

#endif /* ET_PICTURE_H_ */
