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
setup_term (VteTerminal *term)
{
  PangoFontDescription *desc;
  GdkRGBA bg_color, fg_color, cursor_color, *palette;
  gint i;

  gdk_rgba_parse (&bg_color, "rgba(0, 0, 0, 0.8)");
  gdk_rgba_parse (&fg_color, "rgb(178, 178, 178)");
  gdk_rgba_parse (&cursor_color, "rgb(255, 175, 0)");

  palette = g_new0 (GdkRGBA, 16);
  gchar *colors[16] = {
      "rgb(0, 0, 0)",
      "rgb(205, 0, 0)",
      "rgb(175, 223, 135)",
      "rgb(205, 205, 0)",
      "rgb(9, 29, 70)",
      "rgb(205, 0, 205)",
      "rgb(0, 205, 205)",
      "rgb(229, 229, 229)",
      "rgb(127, 127, 127)",
      "rgb(255, 0, 0)",
      "rgb(0, 255, 0)",
      "rgb(255, 255, 0)",
      "rgb(70, 130, 180)",
      "rgb(255, 0, 255)",
      "rgb(0, 255, 255)",
      "rgb(255, 255, 255)",
  };

  for (i = 0; i < 16; i++)
    gdk_rgba_parse (&palette[i], colors[i]);
  vte_terminal_set_colors (term, &fg_color, &bg_color, palette, 16);

  desc = pango_font_description_from_string ("Menlo Regular");
  pango_font_description_set_absolute_size (desc, PANGO_SCALE * 11);
  vte_terminal_set_font (term, desc);

  vte_terminal_set_cursor_blink_mode (term, VTE_CURSOR_BLINK_OFF);
  vte_terminal_set_allow_bold (term, FALSE);
}

static gboolean
spawn_shell (VteTerminal *term)
{
  gchar *shell;
  gboolean result;

  shell = vte_get_user_shell ();

  gchar *argv[] = { shell, NULL };
  gchar *env[] = { "NVIM_TUI_ENABLE_TRUE_COLOR", NULL };

  result = vte_terminal_spawn_sync (VTE_TERMINAL (term), VTE_PTY_DEFAULT,
                                    g_get_home_dir (), argv, env,
                                    G_SPAWN_DO_NOT_REAP_CHILD,
                                    NULL, NULL, NULL, NULL, NULL);
  g_free (shell);

  return result;
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
                    G_CALLBACK(on_window_title), win);
  setup_term (VTE_TERMINAL (term));
  spawn_shell (VTE_TERMINAL (term));

  gtk_container_add (GTK_CONTAINER (win), term);

  g_signal_connect (win, "key-press-event", G_CALLBACK (on_key_press), term);

  gtk_widget_show_all (win);
  gtk_main ();

  return 0;
}
