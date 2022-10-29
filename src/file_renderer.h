/* EasyTAG - tag editor for audio files
 * Copyright (C) 2022  Marcel Müller <gitlab@maazl.de>
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

#ifndef ET_FILERENDERER_H_
#define ET_FILERENDERER_H_

#include "file.h"

#include <gtk/gtk.h>

#include <string>

/**
 * Renderes for file grids.
 */
class FileColumnRenderer
{
public:
	enum Highlight : char
	{	NORMAL,
		HIGHLIGHT,
		STRONGHIGHLIGHT
	};
	const EtSortMode Column;
protected:
	FileColumnRenderer(EtSortMode col);
	static const gchar* EmptfIfNull(const gchar* value) { return value ? value : ""; }
public:
	/**
	 * Retrieve the column text.
	 * @param file
	 * @param original Fetch from last saved version
	 * @return Text to display.
	 */
	virtual std::string RenderText(const ET_File* file, bool original = false) const = 0;
	/**
	 * Render value
	 * @param renderer Target
	 * @param file File to render
	 * @param highlight Highlight the value
	 */
	static void SetText(GtkCellRendererText* renderer, const gchar* text, bool zebra, Highlight highlight);
	/**
	 * Get renderer for column
	 * @param column_id ID of the column.
	 * Must match the name of \c EtSortMode w/o "ascending" and with optional postfix "_column".
	 * @return Matching renderer (thread-safe singleton) or null if \a column_id is not valid.
	 */
	static const FileColumnRenderer* Get_Renderer(const gchar* column_id);
};

#endif // ET_FILERENDERER_H_
