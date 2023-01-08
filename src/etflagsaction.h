/* EasyTAG - Tag editor for audio files
 * Copyright (C) 2023  Marcel Müller
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

#ifndef ETFLAGSACTION_H_
#define ETFLAGSACTION_H_

#include <gtk/gtk.h>

/** Create boolean action for a single flag within a flags setting action.
 * The state binds to the existence of the flag value within the flags.
 * The name of the action is the name of the master + "." + flag.
 * @param master Action for the flags setting.
 * Typically created by g_settings_create_action.
 * @param flag Nick name of the flag to bind.
 * @return New action instance.
 * @remarks Typically you want to create a new action for each value of the flags.
 */
GAction* et_flags_action_new(GAction* master, const gchar *flag);

/** Create an individual flags action for each flags value
 * and add it to an action map.
 * @param map Action map where to add the actions.
 * @param master Action for the flags setting.
 * Typically created by g_settings_create_action.
 */
void et_flags_action_add_all_values(GActionMap* map, GAction* master);

#endif // ETFLAGSACTION_H_
