/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2014  David King <amigadave@amigadave.com>
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "application.h"
#include "xstring.h"
#include "picture.h"

#if defined(ENABLE_ACOUSTID) || defined(ENABLE_REPLAYGAIN)
extern "C" {
#include <libavutil/log.h>
}
#endif

#include <string>

int
main (int argc, char *argv[])
{
    EtApplication *application;
    gint status;

#if ENABLE_NLS
    textdomain (GETTEXT_PACKAGE);
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (PACKAGE_TARNAME, "UTF-8");
#endif /* ENABLE_NLS */

#ifndef NDEBUG
    // Hack to allow locally modified schema files in debug mode w/o having to install it system wide.
    const char* oldpath = getenv("XDG_DATA_DIRS");
    if (oldpath)
    {   std::string path = argv[0];
        std::string::size_type p = path.rfind(G_DIR_SEPARATOR);
        if (p != std::string::npos)
            path.erase(p);
        path += G_DIR_SEPARATOR_S "share" G_SEARCHPATH_SEPARATOR_S;
        path += oldpath;
        setenv("XDG_DATA_DIRS", path.c_str(), 1);
    }
#endif

    // register EtxString for use in glade files
    g_type_ensure(et_xstring_get_type());

#if defined(ENABLE_ACOUSTID) || defined(ENABLE_REPLAYGAIN)
    av_log_set_level(AV_LOG_ERROR);

#endif

    application = et_application_new ();
    status = g_application_run (G_APPLICATION (application), argc, argv);
    g_object_unref (application);

    EtPicture::GarbageCollector();
    xStringD::garbage_collector();

    return status;
}
