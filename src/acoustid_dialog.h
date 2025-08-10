/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2025  Marcel Mueller <github@maazl.de>
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

#ifndef ET_ACOUSTID_DIALOG_H_
#define ET_ACOUSTID_DIALOG_H_

#include "config.h"
#ifdef ENABLE_ACOUSTID

#include <gtk/gtk.h>

class ET_File;

#define ET_TYPE_ACOUSTID_DIALOG (et_acoustid_dialog_get_type())
#define ET_ACOUSTID_DIALOG(object) (G_TYPE_CHECK_INSTANCE_CAST((object), ET_TYPE_ACOUSTID_DIALOG, EtAcoustIDDialog))

typedef struct _EtAcoustIDDialog EtAcoustIDDialog;
typedef struct _EtAcoustIDDialogClass EtAcoustIDDialogClass;

struct _EtAcoustIDDialog
{
	/*< private >*/
	GtkDialog parent_instance;

	/// Get the currently visible file.
	ET_File* current_file();
	/// Attach the dialog to a (new) file or update the current one.
	/// @param file Target file or \c nullptr to clear the dialog.
	void current_file(ET_File* file);
	/// Update the number of pending files
	void set_remaining_files(unsigned count);

	void update_button_sensitivity();
	/// Reset the entire dialog and stop any background worker.
	void reset();
};

struct _EtAcoustIDDialogClass
{
	/*< private >*/
	GtkDialogClass parent_class;
};

GType et_acoustid_dialog_get_type(void);
EtAcoustIDDialog *et_acoustid_dialog_new(void);

#endif
#endif // ET_ACOUSTID_DIALOG_H_
