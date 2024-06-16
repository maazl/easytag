/*
 *  EasyTAG - Tag editor for MP3 and Ogg Vorbis files
 *  Copyright (C) 2000-2003  Jerome Couderc <easytag@gmail.com>
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

#ifndef ET_GENRES_H_
#define ET_GENRES_H_

#include <glib.h>

G_BEGIN_DECLS

/* GENRE_MAX is the last genre number that can be used */
#define GENRE_MAX ( sizeof(id3_genres)/sizeof(id3_genres[0]) - 1 )

/**
    \def genre_no(IndeX) 
    \param IndeX number of genre using in id3v1 
    \return pointer to genre as string
*/
#define genre_no(IndeX) ( IndeX < (sizeof(id3_genres)/sizeof(*id3_genres) ) ? id3_genres[IndeX] : "Unknown" )
/**
    \def genre_no(IndeX)
    \param IndeX number of genre using in id3v1
    \return pointer to genre as string
*/
#define genre_no_or_null(IndeX) ( IndeX < (sizeof(id3_genres)/sizeof(*id3_genres) ) ? id3_genres[IndeX] : NULL )

extern const char *id3_genres[192];

G_END_DECLS

#endif /* ET_GENRES_H_ */
