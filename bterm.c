/*
 * Copyright (C) 2015 Bastian Winkler <buz@netbuz.org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */


#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <vte/vte.h>


static void read_config (VteTerminal *term, const gchar *path);
static void setup_colors (VteTerminal *term, GKeyFile *config);
static void setup_font (VteTerminal *term, GKeyFile *config);

static void
set_color (GdkRGBA *color, GKeyFile *config, gint i)
{
  gchar *name, *value;

  name = g_strdup_printf ("color%d", i);
  value = g_key_file_get_string (config, "colors", name, NULL);

  if (i < 16)
    {
        color->blue = (i & 4) ? 0xc000 : 0;
        color->green = (i & 2) ? 0xc000 : 0;
        color->red = (i & 1) ? 0xc000 : 0;
        if (i > 7) {
            color->blue += 0x3fff;
            color->green += 0x3fff;
            color->red += 0x3fff;
        }
    }
  else if (i < 232)
    {
        gint j = i - 16;
        gint r = j / 36, g = (j / 6) % 6, b = j % 6;
        gint red =   (r == 0) ? 0 : r * 40 + 55;
        gint green = (g == 0) ? 0 : g * 40 + 55;
        gint blue =  (b == 0) ? 0 : b * 40 + 55;
        color->red   = (red | red << 8) / 65535.0;
        color->green = (green | green << 8) / 65535.0;
        color->blue  = (blue | blue << 8) / 65535.0;
    }
  else if (i < 256)
    {
        int shade = 8 + (i - 232) * 10;
        color->red = color->green = color->blue = (shade | shade << 8) / 65535.0;
    }

  if (value)
    {
      if (!gdk_rgba_parse (color, value))
        g_print ("Failed to parse color %s: %s\n", name, value);
    }

  g_free (name);
  g_free (value);
}

static void
setup_colors (VteTerminal *term, GKeyFile *config)
{
  GdkRGBA bg, fg, *palette;
  gchar *color;

  palette = g_new0 (GdkRGBA, 256);

  for (int i = 0; i < 256; ++i)
    set_color (&palette[i], config, i);

  color = g_key_file_get_string (config, "colors", "background", NULL);
  if (color)
    {
      gdk_rgba_parse (&bg, color);
      g_free (color);
    }

  color = g_key_file_get_string (config, "colors", "foreground", NULL);
  if (color)
    {
      gdk_rgba_parse (&fg, color);
      g_free (color);
    }

  vte_terminal_set_colors (term, &fg, &bg, palette, 256);

  g_free (palette);
}

static void
set_property (GObject *object,
              const gchar *property_name,
              GKeyFile *config,
              const gchar *section,
              const gchar *config_name)
{
  GObjectClass *klass;
  GParamSpec *pspec;
  GValue value = G_VALUE_INIT;
  GError *error = NULL;

  if (!g_key_file_has_key (config, section, config_name, NULL))
    {
      g_warning ("config is missing key %s:%s", section, config_name);
      return;
    }

  klass = G_OBJECT_GET_CLASS (object);
  pspec = g_object_class_find_property (klass, property_name);
  if (!pspec) {
      g_warning ("Unkown property %s", property_name);
      return;
  }

  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

  switch (G_TYPE_FUNDAMENTAL (G_PARAM_SPEC_VALUE_TYPE (pspec)))
    {
    case G_TYPE_STRING:
      g_value_take_string (&value, g_key_file_get_string (config, section, config_name, &error));
      break;

    case G_TYPE_BOOLEAN:
      g_value_set_boolean (&value, g_key_file_get_boolean (config, section, config_name, &error));
      break;

    case G_TYPE_INT:
      g_value_set_int (&value, g_key_file_get_integer (config, section, config_name, &error));
      break;

    case G_TYPE_UINT:
      g_value_set_uint (&value, (guint) g_key_file_get_integer (config, section, config_name, &error));
      break;

    case G_TYPE_INT64:
      g_value_set_int64 (&value, (gint64) g_key_file_get_integer (config, section, config_name, &error));
      break;

    case G_TYPE_DOUBLE:
      g_value_set_double (&value, g_key_file_get_double (config, section, config_name, &error));
      break;

    case G_TYPE_ENUM:
        {
          GEnumClass *eclass;
          GEnumValue *ev;
          gchar *v;

          v = g_key_file_get_string (config, section, config_name, &error);
          if (!v)
            return;

          eclass = g_type_class_ref (G_PARAM_SPEC_VALUE_TYPE (pspec));
          ev = g_enum_get_value_by_nick (eclass, v);
          g_value_set_enum (&value, ev->value);
          g_type_class_unref (eclass);
          g_free (v);
          break;
        }

    default:
      break;
    }

  if (error)
    {
      g_warning ("FAILED: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_object_set_property (object, pspec->name, &value);
}

static void
setup_font (VteTerminal *term, GKeyFile *config)
{
  PangoFontDescription *desc;
  gchar *s;

  s = g_key_file_get_string (config, "fonts", "font", NULL);
  desc = pango_font_description_from_string (s);
  /* desc = pango_font_description_from_string ("Monoid"); */
  /* pango_font_description_set_absolute_size (desc, PANGO_SCALE * 11); */
  /* pango_font_description_set_size (desc, PANGO_SCALE * 11); */
  vte_terminal_set_font (term, desc);

  g_free (s);

  set_property (G_OBJECT (term), "allow-bold", config, "general", "allow_bold");
  set_property (G_OBJECT (term), "audible-bell", config, "general", "audible_bell");
  set_property (G_OBJECT (term), "scrollback-lines", config, "general", "scrollback_lines");
  set_property (G_OBJECT (term), "cursor-shape", config, "cursor", "shape");
  set_property (G_OBJECT (term), "cursor-blink-mode", config, "cursor", "blink_mode");
  set_property (G_OBJECT (term), "scroll-on-keystroke", config, "general", "scroll_on_keystroke");
  set_property (G_OBJECT (term), "scroll-on-output", config, "general", "scroll_on_output");
  set_property (G_OBJECT (term), "pointer-autohide", config, "general", "pointer_autohide");
  set_property (G_OBJECT (term), "font-scale", config, "fonts", "scale");
}

static void
read_config (VteTerminal *term, const gchar *path)
{
  GError *error = NULL;
  GKeyFile *keyfile;

  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, &error))
    {
      g_print ("Failed to load config %s: %s\n", path, error->message);
      g_error_free (error);
    }

  setup_colors (term, keyfile);
  setup_font (term, keyfile);

  g_key_file_free (keyfile);
}

static void
on_config_changed (GFileMonitor      *monitor,
                   GFile             *file,
                   GFile             *other_file,
                   GFileMonitorEvent  event_type,
                   VteTerminal       *term)
{
  gchar *path;

  if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    return;

  path = g_file_get_path (file);
  read_config (term, path);

  g_free (path);
}

static void
config_reader (VteTerminal *term)
{
  gchar *path;
  GFile *file;
  GFileMonitor *monitor;

  path = g_build_filename (g_get_user_config_dir (), "bterm", "btermrc", NULL);
  read_config (term, path);

  file = g_file_new_for_path (path);
  monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);

  g_signal_connect (monitor, "changed", G_CALLBACK (on_config_changed), term);


  g_object_unref (file);
  g_free (path);
}

static void
update_visuals (GtkWidget *widget,
                GdkScreen *old_screen,
                gpointer   user_data)
{
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);
  gtk_widget_set_visual (widget, visual);
}


static void
spawn_shell (VteTerminal *term)
{
  gchar *shell;
  gchar **env;

  shell = vte_get_user_shell ();

  gchar *argv[] = { shell, NULL };
  /* gchar *env[] = { "NVIM_TUI_ENABLE_TRUE_COLOR", NULL }; */
  env = g_get_environ ();

  vte_terminal_spawn_async (term,
                            VTE_PTY_DEFAULT,
                            g_get_home_dir (),
                            argv,
                            env,
                            G_SPAWN_DO_NOT_REAP_CHILD,
                            NULL,
                            NULL,
                            NULL,
                            -1,
                            NULL,
                            NULL,
                            NULL);

  g_free (shell);
  g_strfreev (env);
}

static void
on_bell (VteTerminal *term,
         GtkWidget   *win)
{
  gtk_window_set_urgency_hint (GTK_WINDOW (win), TRUE);
}

static void
on_active (GtkWidget *win)
{
  gboolean is_active, urgency_hint;

  g_object_get (win,
                "is-active", &is_active,
                "urgency-hint", &urgency_hint,
                NULL);

  if (is_active && urgency_hint) {
      gtk_window_set_urgency_hint (GTK_WINDOW (win), FALSE);
  }
}

static void
on_window_title (GtkWidget  *term,
                 GParamSpec *pspec,
                 GtkWidget  *win)
{
  const gchar *title;

  title = vte_terminal_get_window_title (VTE_TERMINAL (term));
  gtk_window_set_title (GTK_WINDOW (win), title);
}

static void
change_font_size (GtkWidget *term,
                  gdouble    val)
{
  gdouble scale;

  scale = vte_terminal_get_font_scale (VTE_TERMINAL (term)) + val;
  vte_terminal_set_font_scale (VTE_TERMINAL (term), CLAMP (scale, 0.1, 2.0));
}

static gboolean
on_key_press (GtkWidget   *win,
              GdkEventKey *event,
              GtkWidget   *term)
{
  if ((event->state & GDK_CONTROL_MASK) == 0)
    return FALSE;

  switch (event->keyval) {
    case GDK_KEY_KP_Add:
      change_font_size (term, 0.05);
      return TRUE;

    case GDK_KEY_KP_Subtract:
      change_font_size (term, -0.05);
      return TRUE;

    default:
      return FALSE;
  }
}


int
main (int argc, char *argv[])
{
  GtkWidget *win, *term;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (win), "BTerm");

  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (win, "notify::is-active", G_CALLBACK (on_active), NULL);

  // make sure we're using a rgba visual if available
  g_signal_connect (win, "screen-changed", G_CALLBACK (update_visuals), NULL);
  update_visuals (win, NULL, NULL);


  term = vte_terminal_new ();
  g_signal_connect (term, "child-exited", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (term, "bell", G_CALLBACK (on_bell), win);
  g_signal_connect (term, "notify::window-title",
                    G_CALLBACK (on_window_title), win);
  /* setup_term (VTE_TERMINAL (term)); */
  config_reader (VTE_TERMINAL (term));
  spawn_shell (VTE_TERMINAL (term));


  gtk_container_add (GTK_CONTAINER (win), term);

  g_signal_connect (win, "key-press-event", G_CALLBACK (on_key_press), term);

  gtk_widget_show_all (win);
  gtk_main ();

  return 0;
}
