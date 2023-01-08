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

/* Expensive work around for missing flags support in gtkmenutrackeritem */

#include "etflagsaction.h"
#include "misc.h"

typedef struct
{
  GObject parent_instance;

  GAction*     master;
  gchar*       name;
  gchar*       flag;
  bool         state;
} EtFlagsAction;

typedef GObjectClass EtFlagsActionClass;

static GType et_flags_action_get_type (void);
static void et_flags_action_iface_init (GActionInterface *iface);
G_DEFINE_TYPE_WITH_CODE (EtFlagsAction, et_flags_action, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(G_TYPE_ACTION, et_flags_action_iface_init))

enum
{
  ACTION_PROP_0,
  ACTION_PROP_NAME,
  ACTION_PROP_PARAMETER_TYPE,
  ACTION_PROP_ENABLED,
  ACTION_PROP_STATE_TYPE,
  ACTION_PROP_STATE
};

static const gchar* et_flags_action_get_name(GAction *action)
{	EtFlagsAction *gsa = (EtFlagsAction*)action;
	return gsa->name;
}

static const GVariantType* et_flags_action_get_parameter_type(GAction *action)
{	return NULL;
}

static gboolean et_flags_action_get_enabled (GAction *action)
{	EtFlagsAction *gsa = (EtFlagsAction*)action;
	return g_action_get_enabled(gsa->master);
}

static const GVariantType* et_flags_action_get_state_type(GAction *action)
{	return G_VARIANT_TYPE_BOOLEAN;
}

static GVariant* et_flags_action_get_state(GAction *action)
{	EtFlagsAction *gsa = (EtFlagsAction*)action;
	return g_variant_ref(g_variant_new_boolean(gsa->state));
}

static GVariant* et_flags_action_get_state_hint(GAction *action)
{	return nullptr; // not implemented
}

static void et_flags_action_change_state(GAction *action, GVariant *value)
{	EtFlagsAction *gsa = (EtFlagsAction*)action;
	// toggle value in master->state
	GVariant* oldvalue = g_action_get_state(gsa->master);
	GVariant* newvalue = et_variant_string_array_set(oldvalue, gsa->flag, g_variant_get_boolean(value));
	g_action_activate(gsa->master, newvalue);
	if (oldvalue == newvalue)
		g_variant_unref(newvalue);
	g_variant_unref(oldvalue);
}

static void et_flags_action_activate(GAction *action, GVariant *parameter)
{	EtFlagsAction *gsa = (EtFlagsAction*)action;
	// toggle value in master->state
	GVariant* oldvalue = g_action_get_state(gsa->master);
	GVariant* newvalue = et_variant_string_array_toggle(oldvalue, gsa->flag);
	g_action_activate(gsa->master, newvalue);
	g_variant_unref(oldvalue);
}

static void et_flags_action_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{	GAction *action = G_ACTION (object);

	switch (prop_id)
	{
	case ACTION_PROP_NAME:
		g_value_set_string (value, et_flags_action_get_name (action));
		break;

	case ACTION_PROP_PARAMETER_TYPE:
		g_value_set_boxed (value, et_flags_action_get_parameter_type (action));
		break;

	case ACTION_PROP_ENABLED:
		g_value_set_boolean (value, et_flags_action_get_enabled (action));
		break;

	case ACTION_PROP_STATE_TYPE:
		g_value_set_boxed (value, et_flags_action_get_state_type (action));
		break;

	case ACTION_PROP_STATE:
		g_value_take_variant (value, et_flags_action_get_state (action));
		break;

	default:
		g_assert_not_reached ();
	}
}

static void
et_flags_action_finalize (GObject *object)
{	EtFlagsAction *gsa = (EtFlagsAction*)object;
	g_signal_handlers_disconnect_by_data(gsa->master, gsa);
	g_object_unref(gsa->master);
	g_free(gsa->flag);
	g_free(gsa->name);
	G_OBJECT_CLASS (et_flags_action_parent_class)->finalize(object);
}

static void et_flags_action_init(EtFlagsAction *gsa)
{
}

static void et_flags_action_iface_init(GActionInterface *iface)
{
	iface->get_name = et_flags_action_get_name;
	iface->get_parameter_type = et_flags_action_get_parameter_type;
	iface->get_enabled = et_flags_action_get_enabled;
	iface->get_state_type = et_flags_action_get_state_type;
	iface->get_state = et_flags_action_get_state;
	iface->get_state_hint = et_flags_action_get_state_hint;
	iface->change_state = et_flags_action_change_state;
	iface->activate = et_flags_action_activate;
}

static void et_flags_action_class_init(EtFlagsActionClass *cls)
{
	cls->get_property = et_flags_action_get_property;
	cls->finalize = et_flags_action_finalize;

	g_object_class_override_property(cls, ACTION_PROP_NAME, "name");
	g_object_class_override_property(cls, ACTION_PROP_PARAMETER_TYPE, "parameter-type");
	g_object_class_override_property(cls, ACTION_PROP_ENABLED, "enabled");
	g_object_class_override_property(cls, ACTION_PROP_STATE_TYPE, "state-type");
	g_object_class_override_property(cls, ACTION_PROP_STATE, "state");
}

static void et_flags_action_changed(GObject* self, GParamSpec* pspec, gpointer user_data)
{	EtFlagsAction *gsa = (EtFlagsAction*)user_data;
	GVariant* value = g_action_get_state(gsa->master);
	gboolean ret = value && et_variant_string_array_contains(value, gsa->flag);
	g_variant_unref(value);
	if (gsa->state == !!ret)
		return;
	gsa->state = !!ret;
	g_object_notify(G_OBJECT(user_data), "state");
}

static void et_flags_action_enabled_changed(GObject* self, GParamSpec* pspec, gpointer user_data)
{	g_object_notify(G_OBJECT(user_data), "enabled");
}

GAction* et_flags_action_new(GAction* master, const gchar *flag)
{
	EtFlagsAction *gsa = (EtFlagsAction*)g_object_new(et_flags_action_get_type(), NULL);
	g_object_ref(master);
	gsa->master = master;
	gsa->flag = g_strdup(flag);
	gsa->name = g_strconcat(g_action_get_name(master), ".", flag, NULL);

	g_signal_connect(master, "notify::state", G_CALLBACK(et_flags_action_changed), gsa);
	g_signal_connect(master, "notify::enabled", G_CALLBACK(et_flags_action_enabled_changed), gsa);
	et_flags_action_changed(NULL, NULL, gsa); // init state

	return G_ACTION(gsa);
}

void et_flags_action_add_all_values(GActionMap* map, GAction* master)
{	GVariant* hint = g_action_get_state_hint(master);
	GVariant* value = g_variant_get_child_value(hint, 1);
	GVariant* values = g_variant_get_variant(value);
	GVariantIter iter;
	const gchar* flag;
	g_variant_iter_init(&iter, values);
	while (g_variant_iter_next(&iter, "&s", &flag))
	{	GAction* action = et_flags_action_new(master, flag);
		g_action_map_add_action(map, action);
	}
	g_variant_unref(values);
	g_variant_unref(value);
	g_variant_unref(hint);
}
